#!/usr/bin/env python3
"""Safely merge portable HalfKP dataset bundles from independent machines."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path, PurePosixPath, PureWindowsPath

from generate_shards import merge_shards
from nnue_dataset import sha256_file, write_json_atomic


SCHEMA_VERSION = 1
SUPPORTED_FORMATS = {"HalfKP-v1", "HalfKP-v1-sharded"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path, nargs="+",
                        help="Part or dataset manifests copied with their output files")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--target-training-positions", type=int, default=0,
                        help="Zero retains every globally unique training record")
    return parser.parse_args()


def portable_name(value: str) -> str:
    """Return the final component of either a Windows or POSIX path."""
    return PureWindowsPath(value).name if "\\" in value else PurePosixPath(value).name


def output_map(manifest: dict[str, object]) -> dict[str, dict[str, object]]:
    outputs = manifest.get("outputs")
    if not isinstance(outputs, list):
        raise ValueError("manifest outputs must be a list")
    result: dict[str, dict[str, object]] = {}
    for output in outputs:
        if not isinstance(output, dict) or not isinstance(output.get("split"), str):
            raise ValueError("manifest contains an invalid output entry")
        result[str(output["split"])] = output
    return result


def locate_output(manifest_path: Path, output: dict[str, object]) -> Path:
    recorded = output.get("path")
    if not isinstance(recorded, str) or not recorded:
        raise ValueError(f"manifest output has no path: {manifest_path}")
    original = Path(recorded)
    candidates = (original, manifest_path.parent / portable_name(recorded))
    for candidate in candidates:
        if candidate.is_file():
            expected = output.get("sha256")
            if not isinstance(expected, str) or sha256_file(candidate) != expected:
                raise ValueError(f"output checksum mismatch: {candidate}")
            return candidate.resolve()
    raise ValueError(
        f"manifest output is missing; expected {portable_name(recorded)} beside {manifest_path}",
    )


def teacher_contract(manifest: dict[str, object]) -> dict[str, object]:
    teacher = manifest.get("teacher")
    if not isinstance(teacher, dict):
        teacher = manifest.get("teacherContract")
    if not isinstance(teacher, dict):
        raise ValueError("manifest has no teacher configuration")
    limit = teacher.get("limit")
    nodes = limit.get("nodes") if isinstance(limit, dict) else teacher.get("nodes")
    comparison = teacher.get("comparisonLimit")
    comparison_nodes = (comparison.get("nodes") if isinstance(comparison, dict)
                        else teacher.get("comparisonNodes"))
    identity = teacher.get("id") if isinstance(teacher.get("id"), dict) else None
    identities = teacher.get("identities")
    if identity is None and isinstance(identities, list) and len(identities) == 1:
        identity = identities[0] if isinstance(identities[0], dict) else None
    binary_checksums = teacher.get("binarySha256")
    binary_sha = teacher.get("sha256")
    if isinstance(binary_checksums, list):
        normalized_checksums = [str(value) for value in binary_checksums]
    elif binary_sha:
        normalized_checksums = [str(binary_sha)]
    else:
        normalized_checksums = []
    return {
        "nodes": nodes,
        "comparisonNodes": comparison_nodes or None,
        "identity": identity,
        "binarySha256": normalized_checksums,
    }


def split_contract(manifest: dict[str, object]) -> dict[str, object]:
    sampling = manifest.get("sampling")
    if not isinstance(sampling, dict):
        sampling = manifest.get("splitContract")
    if not isinstance(sampling, dict):
        raise ValueError("manifest has no sampling configuration")
    return {
        "seed": sampling.get("seed"),
        "validationFractionByGame": sampling.get("validationFractionByGame"),
    }


def load_bundle(manifest_path: Path) -> dict[str, object]:
    resolved = manifest_path.resolve()
    manifest = json.loads(resolved.read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != SCHEMA_VERSION:
        raise ValueError(f"unsupported manifest schema: {manifest_path}")
    if manifest.get("datasetFormat") not in SUPPORTED_FORMATS:
        raise ValueError(f"unsupported dataset format: {manifest_path}")
    outputs = output_map(manifest)
    training = outputs.get("training")
    if training is None:
        raise ValueError(f"manifest has no training split: {manifest_path}")
    validation = outputs.get("validation")
    return {
        "manifestPath": resolved,
        "manifestSha256": sha256_file(resolved),
        "format": manifest["datasetFormat"],
        "teacher": teacher_contract(manifest),
        "splitContract": split_contract(manifest),
        "provenance": manifest.get("provenance"),
        "training": locate_output(resolved, training),
        "validation": locate_output(resolved, validation) if validation else None,
    }


def validate_contracts(bundles: list[dict[str, object]]) -> dict[str, object]:
    first_teacher = bundles[0]["teacher"]
    first_split = bundles[0]["splitContract"]
    assert isinstance(first_teacher, dict) and isinstance(first_split, dict)
    if not isinstance(first_teacher["nodes"], int) or first_teacher["nodes"] < 1:
        raise ValueError("teacher node budget is missing or invalid")
    for bundle in bundles[1:]:
        teacher = bundle["teacher"]
        split = bundle["splitContract"]
        assert isinstance(teacher, dict) and isinstance(split, dict)
        if teacher["nodes"] != first_teacher["nodes"]:
            raise ValueError("teacher node budgets differ across manifests")
        if teacher["comparisonNodes"] != first_teacher["comparisonNodes"]:
            raise ValueError("teacher comparison budgets differ across manifests")
        if (teacher["identity"] is not None and first_teacher["identity"] is not None and
                teacher["identity"] != first_teacher["identity"]):
            raise ValueError("teacher UCI identities differ across manifests")
        if split != first_split:
            raise ValueError("game split seed/fraction differ across manifests")
    return {
        "nodes": first_teacher["nodes"],
        "comparisonNodes": first_teacher["comparisonNodes"],
        "identities": [json.loads(value) for value in sorted(
            {json.dumps(bundle["teacher"]["identity"], sort_keys=True)
             for bundle in bundles if bundle["teacher"]["identity"] is not None},
        )],
        "binarySha256": sorted(
            {checksum for bundle in bundles for checksum in bundle["teacher"]["binarySha256"]},
        ),
    }


def main() -> int:
    args = parse_args()
    if args.target_training_positions < 0:
        raise SystemExit("--target-training-positions cannot be negative")
    missing = [str(path) for path in args.manifest if not path.is_file()]
    if missing:
        raise SystemExit("manifest does not exist: " + ", ".join(missing))

    bundles = [load_bundle(path) for path in args.manifest]
    teacher = validate_contracts(bundles)
    validation_presence = {bundle["validation"] is not None for bundle in bundles}
    if len(validation_presence) != 1:
        raise SystemExit("all manifests must either contain validation data or omit it")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    seen: set[int] = set()
    training_path = (args.output_dir / "train.nnuebin").resolve()
    training_merge = merge_shards(
        [bundle["training"] for bundle in bundles], training_path, seen,
        args.target_training_positions,
    )
    if training_merge["written"] == 0:
        raise RuntimeError("merged training split is empty")

    validation_path = None
    validation_merge = None
    if True in validation_presence:
        validation_path = (args.output_dir / "validation.nnuebin").resolve()
        validation_merge = merge_shards(
            [bundle["validation"] for bundle in bundles], validation_path, seen,
        )
        if validation_merge["written"] == 0:
            raise RuntimeError("merged validation split is empty")

    outputs: list[dict[str, object]] = [{
        "split": "training", "path": str(training_path),
        "sizeBytes": training_path.stat().st_size,
        "sha256": sha256_file(training_path), "merge": training_merge,
    }]
    if validation_path is not None and validation_merge is not None:
        outputs.append({
            "split": "validation", "path": str(validation_path),
            "sizeBytes": validation_path.stat().st_size,
            "sha256": sha256_file(validation_path), "merge": validation_merge,
        })

    source_entries = [{
        "manifestPath": str(bundle["manifestPath"]),
        "manifestSha256": bundle["manifestSha256"],
        "datasetFormat": bundle["format"],
        "teacher": bundle["teacher"],
        "splitContract": bundle["splitContract"],
        "provenance": bundle["provenance"],
    } for bundle in bundles]
    manifest = {
        "schemaVersion": SCHEMA_VERSION,
        "datasetFormat": "HalfKP-v1-sharded",
        "completedAt": datetime.now(timezone.utc).isoformat(),
        "generator": {"script": str(Path(__file__).resolve()),
                      "scriptSha256": sha256_file(Path(__file__).resolve())},
        "teacherContract": teacher,
        "splitContract": bundles[0]["splitContract"],
        "targetTrainingPositions": args.target_training_positions,
        "sources": source_entries,
        "outputs": outputs,
    }
    manifest_path = args.output_dir / "dataset.manifest.json"
    write_json_atomic(manifest_path, manifest)
    total = training_merge["written"] + (validation_merge["written"] if validation_merge else 0)
    print(f"completed positions={total} manifests={len(bundles)} manifest={manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
