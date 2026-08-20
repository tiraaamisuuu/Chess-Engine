#!/usr/bin/env python3
"""Run deterministic teacher workers and merge resumable compact NNUE shards."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time

import chess

from nnue_dataset import BinaryShardWriter, iter_compact_records, sha256_file, write_json_atomic


SCHEMA_VERSION = 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--pgn", required=True, type=Path, nargs="+")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--shards", type=int, default=8)
    parser.add_argument("--jobs", type=int, default=0,
                        help="Concurrent teachers; zero derives a CPU-safe value")
    parser.add_argument("--source-name", required=True)
    parser.add_argument("--source-license", required=True)
    parser.add_argument("--source-url", default="")
    parser.add_argument("--nodes", type=int, default=20_000)
    parser.add_argument("--comparison-nodes", type=int, default=0,
                        help="Optional same-position second teacher budget")
    parser.add_argument("--threads", type=int, default=3, help="Threads per teacher")
    parser.add_argument("--hash", type=int, default=256, dest="hash_mb",
                        help="Hash MiB per teacher")
    parser.add_argument("--sample-rate", type=float, default=0.25)
    parser.add_argument("--min-ply", type=int, default=8)
    parser.add_argument("--max-ply", type=int, default=180)
    parser.add_argument("--max-games", type=int, default=0)
    parser.add_argument("--max-positions-per-shard", type=int, default=0)
    parser.add_argument("--target-training-positions", type=int, default=0,
                        help="Approximate requested training positions before deterministic merge capping")
    parser.add_argument("--target-oversample", type=float, default=1.10,
                        help="Worker headroom for validation assignment and global deduplication")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--validation-fraction", type=float, default=0.1)
    parser.add_argument("--deduplicate", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--resume", action=argparse.BooleanOptionalAction, default=True)
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if not args.engine.is_file():
        raise SystemExit(f"teacher engine does not exist: {args.engine}")
    missing = [str(path) for path in args.pgn if not path.is_file()]
    if missing:
        raise SystemExit("source PGN does not exist: " + ", ".join(missing))
    if args.shards < 1 or args.jobs < 0 or args.threads < 1 or args.hash_mb < 1:
        raise SystemExit("shards, threads, and hash must be positive; jobs cannot be negative")
    if not 0.0 < args.sample_rate <= 1.0:
        raise SystemExit("--sample-rate must be in (0, 1]")
    if not 0.0 <= args.validation_fraction < 1.0:
        raise SystemExit("--validation-fraction must be in [0, 1)")
    if (args.nodes < 1 or args.comparison_nodes < 0 or
            args.min_ply < 0 or args.max_ply < args.min_ply):
        raise SystemExit("invalid teacher or ply configuration")
    if (args.max_games < 0 or args.max_positions_per_shard < 0 or
            args.target_training_positions < 0):
        raise SystemExit("maximum counts cannot be negative")
    if args.target_training_positions and args.max_positions_per_shard:
        raise SystemExit("--target-training-positions and --max-positions-per-shard are mutually exclusive")
    if args.target_oversample < 1.0:
        raise SystemExit("--target-oversample must be at least 1.0")


def positions_per_shard_for_target(target: int, shards: int,
                                   validation_fraction: float, oversample: float) -> int:
    if target <= 0:
        return 0
    training_fraction = 1.0 - validation_fraction
    return max(1, math.ceil(target * oversample / (shards * training_fraction)))


def current_commit(root: Path) -> str | None:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else None


def output_map(manifest: dict[str, object]) -> dict[str, dict[str, object]]:
    outputs = manifest.get("outputs", [])
    if not isinstance(outputs, list):
        return {}
    return {
        str(output["split"]): output
        for output in outputs
        if isinstance(output, dict) and "split" in output
    }


def completed_shard(manifest_path: Path, expected: dict[str, object]) -> bool:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        sampling = manifest["sampling"]
        teacher = manifest["teacher"]
        generator = manifest["generator"]
        if not isinstance(sampling, dict) or not isinstance(teacher, dict) or not isinstance(generator, dict):
            return False
        for key, value in expected["sampling"].items():
            if sampling.get(key) != value:
                return False
        if teacher.get("sha256") != expected["teacherSha256"]:
            return False
        if teacher.get("limit") != {"nodes": expected["nodes"]}:
            return False
        expected_comparison = ({"nodes": expected["comparisonNodes"]}
                               if expected["comparisonNodes"] else None)
        if teacher.get("comparisonLimit") != expected_comparison:
            return False
        if generator.get("commit") != expected["commit"]:
            return False
        if generator.get("scriptSha256") != expected["generatorSha256"]:
            return False
        if generator.get("pythonChess") != expected["pythonChess"]:
            return False
        provenance = manifest.get("provenance")
        if not isinstance(provenance, dict) or provenance != expected["provenance"]:
            return False
        configured = teacher.get("configuredOptions")
        if not isinstance(configured, dict):
            return False
        if "Threads" in configured and configured["Threads"] != expected["threads"]:
            return False
        if "Hash" in configured and configured["Hash"] != expected["hashMiB"]:
            return False
        outputs = output_map(manifest)
        for split, expected_path in expected["outputs"].items():
            output = outputs.get(split)
            if output is None:
                return False
            path = Path(str(output.get("path", "")))
            if path.resolve() != expected_path.resolve() or not path.is_file():
                return False
            if output.get("sha256") != sha256_file(path):
                return False
        return True
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        return False


def run_worker(index: int, command: list[str]) -> int:
    print(f"launching shard={index}", flush=True)
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"teacher shard {index} failed with exit code {result.returncode}")
    print(f"finished shard={index}", flush=True)
    return index


def record_key(board_bytes: bytes, turn: bool) -> int:
    digest = hashlib.blake2b(board_bytes + bytes((1 if turn else 0,)), digest_size=16).digest()
    return int.from_bytes(digest, "little")


def merge_shards(paths: list[Path], destination: Path, seen: set[int] | None,
                 maximum_records: int = 0) -> dict[str, int]:
    temporary = destination.with_name(destination.name + ".tmp")
    read = 0
    written = 0
    duplicates = 0
    with BinaryShardWriter(temporary) as writer:
        for record in iter_compact_records(paths):
            read += 1
            if seen is not None:
                key = record_key(record.board_bytes, record.turn)
                if key in seen:
                    duplicates += 1
                    continue
                seen.add(key)
            writer.write_record(record)
            written += 1
            if maximum_records and written >= maximum_records:
                break
    temporary.replace(destination)
    return {"read": read, "written": written, "duplicatesSkipped": duplicates}


def aggregate_teacher_comparisons(manifest_paths: list[Path]) -> dict[str, float | int] | None:
    reports = []
    for path in manifest_paths:
        manifest = json.loads(path.read_text(encoding="utf-8"))
        report = manifest.get("teacherComparison")
        if isinstance(report, dict) and int(report.get("samples", 0)) > 0:
            reports.append(report)
    if not reports:
        return None

    samples = sum(int(report["samples"]) for report in reports)
    return {
        "samples": samples,
        "primaryNodes": int(reports[0]["primaryNodes"]),
        "comparisonNodes": int(reports[0]["comparisonNodes"]),
        "meanDifferenceCp": sum(float(report["meanDifferenceCp"]) * int(report["samples"])
                                for report in reports) / samples,
        "maeCp": sum(float(report["maeCp"]) * int(report["samples"])
                     for report in reports) / samples,
        "rmseCp": math.sqrt(sum(float(report["rmseCp"]) ** 2 * int(report["samples"])
                                for report in reports) / samples),
        "maxAbsDifferenceCp": max(int(report["maxAbsDifferenceCp"]) for report in reports),
        "signAgreement": sum(float(report["signAgreement"]) * int(report["samples"])
                             for report in reports) / samples,
    }


def main() -> int:
    args = parse_args()
    validate_args(args)
    started_at = datetime.now(timezone.utc)
    root = Path(__file__).resolve().parents[2]
    generator = Path(__file__).with_name("generate_dataset.py")
    generator_sha256 = sha256_file(generator)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    parts_dir = args.output_dir / "parts"
    parts_dir.mkdir(parents=True, exist_ok=True)
    commit = current_commit(root)
    engine = args.engine.resolve()
    engine_sha256 = sha256_file(engine)
    inputs = [{
        "path": str(path.resolve()),
        "sizeBytes": path.resolve().stat().st_size,
        "sha256": sha256_file(path.resolve()),
    } for path in args.pgn]

    jobs = args.jobs or max(1, min(
        args.shards, max(1, (os.cpu_count() or 1) // args.threads),
    ))
    jobs = min(jobs, args.shards)
    maximum_positions_per_shard = args.max_positions_per_shard or positions_per_shard_for_target(
        args.target_training_positions, args.shards,
        args.validation_fraction, args.target_oversample,
    )
    print(
        f"teacher_plan shards={args.shards} jobs={jobs} threads_per_teacher={args.threads} "
        f"maximum_engine_threads={jobs * args.threads} maximum_hash_mb={jobs * args.hash_mb} "
        f"positions_per_shard={maximum_positions_per_shard or 'unlimited'} "
        f"target_training_positions={args.target_training_positions or 'unlimited'}",
        flush=True,
    )

    commands: list[tuple[int, list[str], Path]] = []
    part_manifests: list[Path] = []
    training_parts: list[Path] = []
    validation_parts: list[Path] = []
    for index in range(args.shards):
        stem = f"part-{index:04d}"
        training = (parts_dir / f"{stem}.train.nnuebin").resolve()
        validation = (parts_dir / f"{stem}.validation.nnuebin").resolve()
        manifest = (parts_dir / f"{stem}.manifest.json").resolve()
        training_parts.append(training)
        if args.validation_fraction > 0.0:
            validation_parts.append(validation)
        part_manifests.append(manifest)
        command = [
            sys.executable, str(generator),
            "--engine", str(engine), "--pgn", *[str(path.resolve()) for path in args.pgn],
            "--output", str(training), "--manifest", str(manifest),
            "--format", "compact", "--source-name", args.source_name,
            "--source-license", args.source_license, "--nodes", str(args.nodes),
            "--comparison-nodes", str(args.comparison_nodes),
            "--threads", str(args.threads), "--hash", str(args.hash_mb),
            "--sample-rate", str(args.sample_rate), "--min-ply", str(args.min_ply),
            "--max-ply", str(args.max_ply), "--max-games", str(args.max_games),
            "--max-positions", str(maximum_positions_per_shard), "--seed", str(args.seed),
            "--shard-index", str(index), "--shard-count", str(args.shards),
        ]
        if args.source_url:
            command.extend(("--source-url", args.source_url))
        if not args.deduplicate:
            command.append("--no-deduplicate")
        outputs = {"training": training}
        if args.validation_fraction > 0.0:
            command.extend((
                "--validation-output", str(validation),
                "--validation-fraction", str(args.validation_fraction),
            ))
            outputs["validation"] = validation
        expected = {
            "commit": commit,
            "generatorSha256": generator_sha256,
            "pythonChess": chess.__version__,
            "teacherSha256": engine_sha256,
            "nodes": args.nodes,
            "comparisonNodes": args.comparison_nodes,
            "provenance": {
                "name": args.source_name,
                "license": args.source_license,
                "url": args.source_url or None,
                "inputs": inputs,
            },
            "threads": args.threads,
            "hashMiB": args.hash_mb,
            "outputs": outputs,
            "sampling": {
                "seed": args.seed,
                "sampleRate": args.sample_rate,
                "minPly": args.min_ply,
                "maxPly": args.max_ply,
                "maxGames": args.max_games,
                "maxPositions": maximum_positions_per_shard,
                "deduplicate": args.deduplicate,
                "validationFractionByGame": args.validation_fraction,
                "shardIndex": index,
                "shardCount": args.shards,
            },
        }
        if args.resume and completed_shard(manifest, expected):
            print(f"resuming shard={index} status=complete", flush=True)
        else:
            commands.append((index, command, manifest))

    if commands:
        progress_started = time.monotonic()
        completed_workers = 0
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = {pool.submit(run_worker, index, command): index
                       for index, command, _manifest in commands}
            for future in as_completed(futures):
                future.result()
                completed_workers += 1
                elapsed = max(1e-9, time.monotonic() - progress_started)
                rate = completed_workers / elapsed
                eta = (len(commands) - completed_workers) / rate if rate > 0 else 0.0
                print(f"teacher_progress shards={completed_workers}/{len(commands)} "
                      f"rate={rate * 3600:.2f}/hour elapsed_seconds={elapsed:.0f} "
                      f"eta_seconds={eta:.0f}", flush=True)

    missing_manifests = [str(path) for path in part_manifests if not path.is_file()]
    if missing_manifests:
        raise RuntimeError("teacher generation did not produce manifests: " + ", ".join(missing_manifests))

    seen: set[int] | None = set() if args.deduplicate else None
    training_output = (args.output_dir / "train.nnuebin").resolve()
    training_merge = merge_shards(
        training_parts, training_output, seen, args.target_training_positions,
    )
    if training_merge["written"] == 0:
        raise RuntimeError("parallel generation produced an empty training split")
    if (args.target_training_positions and
            training_merge["written"] < args.target_training_positions):
        print(f"warning target_training_positions={args.target_training_positions} "
              f"achieved={training_merge['written']} target_not_reached=true "
              f"hint=increase_target_oversample_or_source_games", flush=True)
    validation_output = None
    validation_merge = None
    if validation_parts:
        validation_output = (args.output_dir / "validation.nnuebin").resolve()
        validation_merge = merge_shards(validation_parts, validation_output, seen)
        if validation_merge["written"] == 0:
            raise RuntimeError("parallel generation produced an empty validation split")

    part_entries = []
    for manifest_path in part_manifests:
        part_entries.append({
            "path": str(manifest_path),
            "sha256": sha256_file(manifest_path),
        })
    outputs: list[dict[str, object]] = [{
        "split": "training", "path": str(training_output),
        "sizeBytes": training_output.stat().st_size,
        "sha256": sha256_file(training_output), "merge": training_merge,
    }]
    if validation_output and validation_merge:
        outputs.append({
            "split": "validation", "path": str(validation_output),
            "sizeBytes": validation_output.stat().st_size,
            "sha256": sha256_file(validation_output), "merge": validation_merge,
        })
    completed_at = datetime.now(timezone.utc)
    manifest = {
        "schemaVersion": SCHEMA_VERSION,
        "datasetFormat": "HalfKP-v1-sharded",
        "startedAt": started_at.isoformat(),
        "completedAt": completed_at.isoformat(),
        "durationSeconds": round((completed_at - started_at).total_seconds(), 3),
        "generator": {"commit": commit, "script": str(Path(__file__).resolve()),
                      "scriptSha256": sha256_file(Path(__file__).resolve())},
        "teacher": {"path": str(engine), "sha256": engine_sha256, "nodes": args.nodes,
                    "comparisonNodes": args.comparison_nodes or None,
                    "comparisonHashIsolation": ("clear-before-each-budget"
                                                if args.comparison_nodes else None),
                    "threadsPerProcess": args.threads, "hashMiBPerProcess": args.hash_mb},
        "provenance": {"name": args.source_name, "license": args.source_license,
                       "url": args.source_url or None, "inputs": inputs},
        "orchestration": {"shards": args.shards, "jobs": jobs, "resume": args.resume,
                          "deduplicate": args.deduplicate},
        "sampling": {"seed": args.seed, "sampleRate": args.sample_rate,
                     "minPly": args.min_ply, "maxPly": args.max_ply,
                     "maxGames": args.max_games,
                      "maxPositionsPerShard": maximum_positions_per_shard,
                      "targetTrainingPositions": args.target_training_positions,
                      "targetOversample": args.target_oversample,
                     "validationFractionByGame": args.validation_fraction},
        "partManifests": part_entries,
        "teacherComparison": aggregate_teacher_comparisons(part_manifests),
        "outputs": outputs,
    }
    manifest_path = args.output_dir / "dataset.manifest.json"
    write_json_atomic(manifest_path, manifest)
    total = training_merge["written"] + (validation_merge["written"] if validation_merge else 0)
    print(f"completed positions={total} manifest={manifest_path}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
