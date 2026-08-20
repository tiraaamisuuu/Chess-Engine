#!/usr/bin/env python3
"""Generate provenance-tracked HalfKP-v1 teacher shards from whole PGN games."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import subprocess
import time
from typing import Protocol

import chess
import chess.engine
import chess.pgn
import chess.polyglot

from nnue_dataset import BinaryShardWriter, sha256_file, write_json_atomic


SCHEMA_VERSION = 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, type=Path,
                        help="Path to a strong UCI teacher, normally Stockfish")
    parser.add_argument("--pgn", required=True, type=Path, nargs="+", help="Source PGN file(s)")
    parser.add_argument("--output", required=True, type=Path, help="Training shard output")
    parser.add_argument("--validation-output", type=Path,
                        help="Optional game-disjoint validation shard")
    parser.add_argument("--validation-fraction", type=float, default=0.0,
                        help="Fraction of whole games assigned to --validation-output")
    parser.add_argument("--format", choices=("auto", "jsonl", "compact"), default="auto",
                        help="Compact is selected automatically for .nnuebin outputs")
    parser.add_argument("--manifest", type=Path,
                        help="Defaults to <output>.manifest.json")
    parser.add_argument("--source-name", required=True,
                        help="Human-readable dataset/source collection name")
    parser.add_argument("--source-license", required=True,
                        help="Licence or permission basis for the source games")
    parser.add_argument("--source-url", default="", help="Optional source/provenance URL")
    parser.add_argument("--nodes", type=int, default=20_000, help="Teacher nodes per sampled position")
    parser.add_argument("--comparison-nodes", type=int, default=0,
                        help="Optional second teacher budget for same-position label diagnostics")
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--hash", type=int, default=512, dest="hash_mb")
    parser.add_argument("--sample-rate", type=float, default=0.25)
    parser.add_argument("--min-ply", type=int, default=8)
    parser.add_argument("--max-ply", type=int, default=180)
    parser.add_argument("--max-games", type=int, default=0, help="Zero means unlimited")
    parser.add_argument("--max-positions", type=int, default=0, help="Zero means unlimited")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--shard-index", type=int, default=0,
                        help="Zero-based deterministic game partition")
    parser.add_argument("--shard-count", type=int, default=1,
                        help="Number of deterministic game partitions")
    parser.add_argument("--deduplicate", action=argparse.BooleanOptionalAction, default=True)
    return parser.parse_args()


def result_for_side_to_move(result: str, turn: chess.Color) -> float:
    white_result = {"1-0": 1.0, "0-1": -1.0, "1/2-1/2": 0.0}.get(result, 0.0)
    return white_result if turn == chess.WHITE else -white_result


def game_is_validation(input_sha256: str, game_index: int, seed: int,
                       validation_fraction: float) -> bool:
    if validation_fraction <= 0.0:
        return False
    material = f"{input_sha256}:{game_index}:{seed}".encode("ascii")
    value = int.from_bytes(hashlib.blake2b(material, digest_size=8).digest(), "little")
    return value / float(1 << 64) < validation_fraction


def game_shard(input_sha256: str, game_index: int, seed: int, shard_count: int) -> int:
    material = f"shard:{input_sha256}:{game_index}:{seed}".encode("ascii")
    value = int.from_bytes(hashlib.blake2b(material, digest_size=8).digest(), "little")
    return value % shard_count


def position_is_sampled(input_sha256: str, game_index: int, ply: int,
                        seed: int, sample_rate: float) -> bool:
    if sample_rate >= 1.0:
        return True
    material = f"sample:{input_sha256}:{game_index}:{ply}:{seed}".encode("ascii")
    value = int.from_bytes(hashlib.blake2b(material, digest_size=8).digest(), "little")
    return value / float(1 << 64) < sample_rate


@contextmanager
def open_pgn_text(path: Path):
    if path.suffix.casefold() != ".zst":
        with path.open("r", encoding="utf-8", errors="replace") as source:
            yield source
        return
    try:
        import zstandard
    except ImportError as error:
        raise RuntimeError(
            "reading .zst PGNs requires the zstandard package from requirements.txt"
        ) from error
    with path.open("rb") as compressed:
        with zstandard.ZstdDecompressor().stream_reader(compressed) as reader:
            with io.TextIOWrapper(reader, encoding="utf-8", errors="replace") as source:
                yield source


def current_commit(root: Path) -> str | None:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else None


def option_manifest(options: dict[str, chess.engine.Option]) -> list[dict[str, object]]:
    result: list[dict[str, object]] = []
    for name in sorted(options):
        option = options[name]
        result.append({
            "name": name,
            "type": option.type,
            "default": option.default,
            "min": option.min,
            "max": option.max,
            "var": list(option.var) if option.var else [],
        })
    return result


class DatasetWriter(Protocol):
    count: int

    def write(self, board: chess.Board, score_cp: int, result: float,
              game_id: int, ply: int, split: str) -> None: ...
    def close(self) -> None: ...


class JsonlWriter:
    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.path = path
        self.output = path.open("w", encoding="utf-8", newline="\n")
        self.count = 0

    def write(self, board: chess.Board, score_cp: int, result: float,
              game_id: int, ply: int, split: str) -> None:
        record = {
            "fen": board.fen(en_passant="fen"),
            "score_cp": max(-32_000, min(32_000, int(score_cp))),
            "result": result,
            "game_id": game_id,
            "ply": ply,
            "split": split,
        }
        self.output.write(json.dumps(record, separators=(",", ":")) + "\n")
        self.count += 1

    def close(self) -> None:
        if not self.output.closed:
            self.output.close()


class CompactWriter:
    def __init__(self, path: Path):
        self.path = path
        self.writer = BinaryShardWriter(path)

    @property
    def count(self) -> int:
        return self.writer.count

    def write(self, board: chess.Board, score_cp: int, result: float,
              game_id: int, ply: int, split: str) -> None:
        del split
        self.writer.write(board, score_cp, result, game_id, ply)

    def close(self) -> None:
        self.writer.close()


def output_format(path: Path, requested: str) -> str:
    if requested != "auto":
        return requested
    return "compact" if path.suffix.casefold() == ".nnuebin" else "jsonl"


def make_writer(path: Path, requested_format: str) -> DatasetWriter:
    selected = output_format(path, requested_format)
    return CompactWriter(path) if selected == "compact" else JsonlWriter(path)


class TeacherComparison:
    def __init__(self) -> None:
        self.samples = 0
        self.signed_error = 0.0
        self.absolute_error = 0.0
        self.squared_error = 0.0
        self.maximum_absolute_error = 0
        self.sign_agreements = 0

    def add(self, primary_cp: int, comparison_cp: int) -> None:
        difference = comparison_cp - primary_cp
        absolute = abs(difference)
        self.samples += 1
        self.signed_error += difference
        self.absolute_error += absolute
        self.squared_error += difference * difference
        self.maximum_absolute_error = max(self.maximum_absolute_error, absolute)
        self.sign_agreements += int((primary_cp >= 0) == (comparison_cp >= 0))

    def report(self, primary_nodes: int, comparison_nodes: int) -> dict[str, float | int]:
        denominator = max(1, self.samples)
        return {
            "samples": self.samples,
            "primaryNodes": primary_nodes,
            "comparisonNodes": comparison_nodes,
            "meanDifferenceCp": self.signed_error / denominator,
            "maeCp": self.absolute_error / denominator,
            "rmseCp": (self.squared_error / denominator) ** 0.5,
            "maxAbsDifferenceCp": self.maximum_absolute_error,
            "signAgreement": self.sign_agreements / denominator,
        }


def validate_args(args: argparse.Namespace) -> None:
    if not 0.0 < args.sample_rate <= 1.0:
        raise SystemExit("--sample-rate must be in (0, 1]")
    if not 0.0 <= args.validation_fraction < 1.0:
        raise SystemExit("--validation-fraction must be in [0, 1)")
    if args.validation_fraction > 0.0 and not args.validation_output:
        raise SystemExit("--validation-output is required when --validation-fraction is positive")
    if args.validation_output and args.validation_fraction <= 0.0:
        raise SystemExit("--validation-fraction must be positive when --validation-output is used")
    if args.nodes < 1 or args.threads < 1 or args.hash_mb < 1:
        raise SystemExit("--nodes, --threads, and --hash must be positive")
    if args.comparison_nodes < 0:
        raise SystemExit("--comparison-nodes cannot be negative")
    if args.min_ply < 0 or args.max_ply < args.min_ply:
        raise SystemExit("invalid ply range")
    if args.max_games < 0 or args.max_positions < 0:
        raise SystemExit("maximum counts cannot be negative")
    if args.shard_count < 1 or not 0 <= args.shard_index < args.shard_count:
        raise SystemExit("--shard-index must be in [0, --shard-count)")
    if not args.engine.is_file():
        raise SystemExit(f"teacher engine does not exist: {args.engine}")
    missing = [str(path) for path in args.pgn if not path.is_file()]
    if missing:
        raise SystemExit("source PGN does not exist: " + ", ".join(missing))
    outputs = [args.output.resolve()]
    if args.validation_output:
        outputs.append(args.validation_output.resolve())
    if len(set(outputs)) != len(outputs):
        raise SystemExit("training and validation outputs must be different files")


def main() -> int:
    args = parse_args()
    validate_args(args)
    root = Path(__file__).resolve().parents[2]
    started_at = datetime.now(timezone.utc)
    progress_started = time.monotonic()
    manifest_path = args.manifest or args.output.with_suffix(args.output.suffix + ".manifest.json")

    inputs: list[dict[str, object]] = []
    for path in args.pgn:
        resolved = path.resolve()
        inputs.append({
            "path": str(resolved),
            "sizeBytes": resolved.stat().st_size,
            "sha256": sha256_file(resolved),
        })

    engine_path = args.engine.resolve()
    engine_sha256 = sha256_file(engine_path)
    configured_options: dict[str, int] = {}
    stats: dict[str, object] = {
        "gamesRead": 0,
        "gamesWithErrors": 0,
        "gamesAssigned": 0,
        "gamesSkippedByShard": 0,
        "trainingGames": 0,
        "validationGames": 0,
        "positionsConsidered": 0,
        "positionsSampled": 0,
        "duplicatesSkipped": 0,
        "trainingPositions": 0,
        "validationPositions": 0,
        "scoreMinCp": None,
        "scoreMaxCp": None,
        "resultCounts": {"win": 0, "draw": 0, "loss": 0},
    }
    seen_positions: set[int] = set()
    teacher_comparison = TeacherComparison()
    stop = False

    engine = chess.engine.SimpleEngine.popen_uci(str(engine_path))
    teacher_id = dict(engine.id)
    teacher_options = option_manifest(engine.options)
    try:
        if "Threads" in engine.options:
            configured_options["Threads"] = args.threads
        if "Hash" in engine.options:
            configured_options["Hash"] = args.hash_mb
        if configured_options:
            engine.configure(configured_options)

        writers: list[DatasetWriter] = []
        training_writer = make_writer(args.output, args.format)
        writers.append(training_writer)
        validation_writer = None
        if args.validation_output:
            validation_writer = make_writer(args.validation_output, args.format)
            writers.append(validation_writer)

        try:
            global_game_id = 0
            for input_index, pgn_path in enumerate(args.pgn):
                input_sha256 = str(inputs[input_index]["sha256"])
                local_game_index = 0
                with open_pgn_text(pgn_path) as source:
                    while not stop:
                        game = chess.pgn.read_game(source)
                        if game is None:
                            break
                        local_game_index += 1
                        global_game_id += 1
                        stats["gamesRead"] = global_game_id
                        if game.errors:
                            stats["gamesWithErrors"] = int(stats["gamesWithErrors"]) + 1

                        if game_shard(input_sha256, local_game_index, args.seed,
                                      args.shard_count) != args.shard_index:
                            stats["gamesSkippedByShard"] = int(stats["gamesSkippedByShard"]) + 1
                            if args.max_games and global_game_id >= args.max_games:
                                stop = True
                            continue
                        stats["gamesAssigned"] = int(stats["gamesAssigned"]) + 1

                        validation_game = game_is_validation(
                            input_sha256, local_game_index, args.seed, args.validation_fraction,
                        )
                        split = "validation" if validation_game else "training"
                        writer = validation_writer if validation_game else training_writer
                        if writer is None:
                            raise RuntimeError("validation game selected without a validation writer")
                        game_key = "validationGames" if validation_game else "trainingGames"
                        stats[game_key] = int(stats[game_key]) + 1

                        result = game.headers.get("Result", "*")
                        board = game.board()
                        for ply, move in enumerate(game.mainline_moves(), start=1):
                            board.push(move)
                            if ply < args.min_ply or ply > args.max_ply:
                                continue
                            if board.is_game_over(claim_draw=True):
                                continue
                            stats["positionsConsidered"] = int(stats["positionsConsidered"]) + 1
                            if not position_is_sampled(
                                input_sha256, local_game_index, ply, args.seed, args.sample_rate,
                            ):
                                continue
                            stats["positionsSampled"] = int(stats["positionsSampled"]) + 1

                            position_key = chess.polyglot.zobrist_hash(board)
                            if args.deduplicate and position_key in seen_positions:
                                stats["duplicatesSkipped"] = int(stats["duplicatesSkipped"]) + 1
                                continue
                            seen_positions.add(position_key)

                            analysis = engine.analyse(board, chess.engine.Limit(nodes=args.nodes))
                            score = analysis["score"].pov(board.turn).score(mate_score=32_000)
                            if score is None:
                                continue
                            bounded_score = max(-32_000, min(32_000, int(score)))
                            if args.comparison_nodes:
                                comparison_analysis = engine.analyse(
                                    board, chess.engine.Limit(nodes=args.comparison_nodes),
                                )
                                comparison_score = comparison_analysis["score"].pov(board.turn).score(
                                    mate_score=32_000,
                                )
                                if comparison_score is not None:
                                    teacher_comparison.add(bounded_score, max(
                                        -32_000, min(32_000, int(comparison_score)),
                                    ))
                            result_target = result_for_side_to_move(result, board.turn)
                            writer.write(board, bounded_score, result_target, global_game_id, ply, split)
                            position_key_name = "validationPositions" if validation_game else "trainingPositions"
                            stats[position_key_name] = int(stats[position_key_name]) + 1
                            stats["scoreMinCp"] = (bounded_score if stats["scoreMinCp"] is None else
                                                   min(int(stats["scoreMinCp"]), bounded_score))
                            stats["scoreMaxCp"] = (bounded_score if stats["scoreMaxCp"] is None else
                                                   max(int(stats["scoreMaxCp"]), bounded_score))
                            result_name = "win" if result_target > 0 else ("loss" if result_target < 0 else "draw")
                            result_counts = stats["resultCounts"]
                            assert isinstance(result_counts, dict)
                            result_counts[result_name] = int(result_counts[result_name]) + 1

                            total_positions = int(stats["trainingPositions"]) + int(stats["validationPositions"])
                            if total_positions % 1000 == 0:
                                elapsed = max(1e-9, time.monotonic() - progress_started)
                                rate = total_positions / elapsed
                                eta = ((args.max_positions - total_positions) / rate
                                       if args.max_positions and rate > 0 else None)
                                target = f"/{args.max_positions}" if args.max_positions else ""
                                eta_text = f" eta_seconds={eta:.0f}" if eta is not None else ""
                                print(f"positions={total_positions}{target} games={global_game_id} "
                                      f"duplicates={stats['duplicatesSkipped']} rate={rate:.2f}/s "
                                      f"elapsed_seconds={elapsed:.0f}{eta_text}", flush=True)
                            if args.max_positions and total_positions >= args.max_positions:
                                stop = True
                                break

                        if args.max_games and global_game_id >= args.max_games:
                            stop = True

        finally:
            for writer in writers:
                writer.close()
    finally:
        engine.quit()

    outputs: list[dict[str, object]] = []
    if args.shard_count == 1 and int(stats["trainingPositions"]) == 0:
        raise RuntimeError("generation produced an empty training split")
    if (args.shard_count == 1 and args.validation_output and
            int(stats["validationPositions"]) == 0):
        raise RuntimeError(
            "generation produced an empty validation split; increase the game count or change the seed"
        )
    for split, path in (("training", args.output), ("validation", args.validation_output)):
        if path is None:
            continue
        resolved = path.resolve()
        outputs.append({
            "split": split,
            "path": str(resolved),
            "format": output_format(path, args.format),
            "sizeBytes": resolved.stat().st_size,
            "sha256": sha256_file(resolved),
            "positions": int(stats[f"{split}Positions"]),
        })

    completed_at = datetime.now(timezone.utc)
    generator_script = Path(__file__).resolve()
    manifest = {
        "schemaVersion": SCHEMA_VERSION,
        "datasetFormat": "HalfKP-v1",
        "startedAt": started_at.isoformat(),
        "completedAt": completed_at.isoformat(),
        "durationSeconds": round((completed_at - started_at).total_seconds(), 3),
        "generator": {
            "commit": current_commit(root),
            "script": str(generator_script),
            "scriptSha256": sha256_file(generator_script),
            "pythonChess": chess.__version__,
        },
        "provenance": {
            "name": args.source_name,
            "license": args.source_license,
            "url": args.source_url or None,
            "inputs": inputs,
        },
        "teacher": {
            "path": str(engine_path),
            "sha256": engine_sha256,
            "id": teacher_id,
            "configuredOptions": configured_options,
            "availableOptions": teacher_options,
            "limit": {"nodes": args.nodes},
            "comparisonLimit": ({"nodes": args.comparison_nodes}
                                if args.comparison_nodes else None),
        },
        "sampling": {
            "seed": args.seed,
            "sampleRate": args.sample_rate,
            "minPly": args.min_ply,
            "maxPly": args.max_ply,
            "maxGames": args.max_games,
            "maxPositions": args.max_positions,
            "deduplicate": args.deduplicate,
            "validationFractionByGame": args.validation_fraction,
            "shardIndex": args.shard_index,
            "shardCount": args.shard_count,
        },
        "statistics": stats,
        "teacherComparison": teacher_comparison.report(
            args.nodes, args.comparison_nodes,
        ) if args.comparison_nodes else None,
        "outputs": outputs,
    }
    write_json_atomic(manifest_path, manifest)
    total = int(stats["trainingPositions"]) + int(stats["validationPositions"])
    elapsed = max(1e-9, time.monotonic() - progress_started)
    print(f"completed positions={total} games={stats['gamesRead']} rate={total / elapsed:.2f}/s "
          f"elapsed_seconds={elapsed:.1f} manifest={manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
