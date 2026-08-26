#!/usr/bin/env python3
"""Run a resumable limited-strength Stockfish calibration ladder."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import subprocess
import sys

import compare_engines


ROOT = Path(__file__).resolve().parents[1]
LAUNCHER_CONTROL_FILES = {
    "calibration.pid",
    "calibration.stdout.log",
    "calibration.stderr.log",
    "dashboard.pid",
    "dashboard.stdout.log",
    "dashboard.stderr.log",
}


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run the same candidate against several Stockfish UCI_Elo rungs. "
            "The result is a local-pool calibration, not a universal human rating."
        )
    )
    parser.add_argument("--stockfish-exe", type=Path, required=True)
    parser.add_argument("--stockfish-name", default="Stockfish")
    parser.add_argument("--stockfish-version")
    parser.add_argument("--stockfish-arg", action="append", default=[])
    parser.add_argument("--stockfish-option", action="append", default=[], metavar="NAME=VALUE")
    candidate = parser.add_mutually_exclusive_group()
    candidate.add_argument("--engine-ref", default="HEAD")
    candidate.add_argument("--engine-exe", type=Path)
    parser.add_argument("--engine-name", default="Candidate")
    parser.add_argument("--engine-version")
    parser.add_argument("--engine-arg", action="append", default=[])
    parser.add_argument("--engine-option", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--rungs", default="1320,1600,1900,2200,2500")
    parser.add_argument("--games", type=int, default=100)
    parser.add_argument("--tc", default="10+0.1")
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--hash", type=int, default=256, dest="hash_mb")
    parser.add_argument("--concurrency", type=int, default=2)
    parser.add_argument("--build-jobs", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--openings", type=Path)
    parser.add_argument("--cutechess", type=Path)
    parser.add_argument("--sfml-prefix", type=Path)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--run-dir", type=Path, help="Create or resume this ladder directory")
    parser.add_argument(
        "--quick", action="store_true", help="Use four games per rung at 2+0.02"
    )
    return parser.parse_args()


def parse_rungs(value: str) -> list[int]:
    try:
        rungs = [int(item.strip()) for item in value.split(",") if item.strip()]
    except ValueError as error:
        raise RuntimeError("--rungs must be a comma-separated list of integers") from error
    if not rungs:
        raise RuntimeError("--rungs cannot be empty")
    if len(rungs) != len(set(rungs)):
        raise RuntimeError("--rungs cannot contain duplicates")
    return sorted(rungs)


def estimate_local_rating(rungs: list[dict[str, object]]) -> dict[str, object]:
    usable = sorted(
        (
            rung for rung in rungs
            if isinstance(rung.get("candidateScore"), (int, float))
        ),
        key=lambda rung: int(rung["uciElo"]),
    )
    if not usable:
        return {"status": "no_results", "estimate": None}

    exact = [rung for rung in usable if float(rung["candidateScore"]) == 0.5]
    if exact:
        closest = min(exact, key=lambda rung: int(rung["uciElo"]))
        rating = int(closest["uciElo"])
        return {
            "status": "exact_anchor",
            "estimate": float(rating),
            "lowerAnchor": rating,
            "upperAnchor": rating,
        }

    brackets: list[tuple[dict[str, object], dict[str, object]]] = []
    for lower, upper in zip(usable, usable[1:]):
        lower_score = float(lower["candidateScore"])
        upper_score = float(upper["candidateScore"])
        if (lower_score - 0.5) * (upper_score - 0.5) < 0:
            brackets.append((lower, upper))

    if brackets:
        lower, upper = min(
            brackets,
            key=lambda pair: (
                abs(float(pair[0]["candidateScore"]) - 0.5)
                + abs(float(pair[1]["candidateScore"]) - 0.5)
            ),
        )
        lower_rating = int(lower["uciElo"])
        upper_rating = int(upper["uciElo"])
        lower_score = float(lower["candidateScore"])
        upper_score = float(upper["candidateScore"])
        estimate = lower_rating + (
            (0.5 - lower_score)
            * (upper_rating - lower_rating)
            / (upper_score - lower_score)
        )
        return {
            "status": "bracketed",
            "estimate": round(estimate, 1),
            "lowerAnchor": lower_rating,
            "upperAnchor": upper_rating,
        }

    scores = [float(rung["candidateScore"]) for rung in usable]
    if all(score > 0.5 for score in scores):
        return {
            "status": "above_range",
            "estimate": None,
            "lowerAnchor": int(usable[-1]["uciElo"]),
            "upperAnchor": None,
        }
    if all(score < 0.5 for score in scores):
        return {
            "status": "below_range",
            "estimate": None,
            "lowerAnchor": None,
            "upperAnchor": int(usable[0]["uciElo"]),
        }
    return {"status": "noisy_unbracketed", "estimate": None}


def completed_attempt(rung_dir: Path) -> tuple[Path, dict[str, object]] | None:
    attempts = sorted(rung_dir.glob("attempt-*"))
    for attempt in reversed(attempts):
        result_path = attempt / "match" / "result.json"
        if not result_path.is_file():
            continue
        try:
            result = json.loads(result_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            continue
        if result.get("completed") is True:
            return attempt, result
    return None


def next_attempt(rung_dir: Path) -> Path:
    numbers = []
    for attempt in rung_dir.glob("attempt-*"):
        try:
            numbers.append(int(attempt.name.removeprefix("attempt-")))
        except ValueError:
            continue
    return rung_dir / f"attempt-{max(numbers, default=0) + 1:03d}"


def unexpected_new_run_entries(run_dir: Path) -> list[Path]:
    return sorted(
        (path for path in run_dir.iterdir() if path.name not in LAUNCHER_CONTROL_FILES),
        key=lambda path: path.name,
    )


def run_and_tee(command: list[str], log_path: Path) -> int:
    print("+ " + " ".join(command), flush=True)
    with log_path.open("w", encoding="utf-8") as log_file:
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=dict(os.environ),
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            log_file.write(line)
        return process.wait()


def rung_summary(
    uci_elo: int, attempt: Path, result: dict[str, object]
) -> dict[str, object]:
    score = result.get("score")
    elo = result.get("elo")
    failures = result.get("failures")
    if not isinstance(score, dict):
        score = {}
    if not isinstance(elo, dict):
        elo = {}
    if not isinstance(failures, dict):
        failures = {}
    relative_elo = elo.get("difference")
    return {
        "uciElo": uci_elo,
        "attempt": str(attempt.resolve()),
        "completed": result.get("completed") is True,
        "games": score.get("games"),
        "candidateWins": score.get("candidateWins"),
        "stockfishWins": score.get("baselineWins"),
        "draws": score.get("draws"),
        "candidateScore": score.get("candidateScore"),
        "relativeElo": relative_elo,
        "relativeEloUncertainty": elo.get("uncertainty"),
        "anchoredPointEstimate": (
            round(uci_elo + float(relative_elo), 1)
            if isinstance(relative_elo, (int, float))
            else None
        ),
        "failures": failures,
    }


def write_report(
    report_path: Path,
    summary: dict[str, object],
    stockfish_name: str,
) -> None:
    estimate = summary["localPoolEstimate"]
    assert isinstance(estimate, dict)
    lines = [
        "# Stockfish Limited-Strength Calibration",
        "",
        (
            "This is a rating anchor in this exact local engine pool, hardware, "
            "opening suite, and time control. It is not FIDE Elo, human Elo, or "
            "a universal engine rating."
        ),
        "",
        "| Stockfish rung | W | L | D | Candidate score | Relative Elo | Anchored point |",
        "|---:|---:|---:|---:|---:|---:|---:|",
    ]
    rungs = summary["rungs"]
    assert isinstance(rungs, list)
    for rung in rungs:
        assert isinstance(rung, dict)
        relative = rung.get("relativeElo")
        uncertainty = rung.get("relativeEloUncertainty")
        relative_text = (
            f"{float(relative):+.1f} ± {float(uncertainty):.1f}"
            if isinstance(relative, (int, float))
            and isinstance(uncertainty, (int, float))
            else "unresolved"
        )
        anchored = rung.get("anchoredPointEstimate")
        anchored_text = f"{float(anchored):.1f}" if isinstance(anchored, (int, float)) else "—"
        score = rung.get("candidateScore")
        score_text = f"{100 * float(score):.1f}%" if isinstance(score, (int, float)) else "—"
        lines.append(
            f"| {rung['uciElo']} | {rung.get('candidateWins', '—')} | "
            f"{rung.get('stockfishWins', '—')} | {rung.get('draws', '—')} | "
            f"{score_text} | {relative_text} | {anchored_text} |"
        )

    lines.extend(["", "## Interpretation", ""])
    status = estimate.get("status")
    if status in {"bracketed", "exact_anchor"}:
        lines.append(
            f"The observed 50% crossing is approximately **{estimate['estimate']:.1f}** "
            f"within the {stockfish_name} limited-strength pool."
        )
        lines.append(
            f"It is bracketed by configured rungs {estimate.get('lowerAnchor')} and "
            f"{estimate.get('upperAnchor')} and should retain a wide uncertainty "
            "until substantially more games and a second time control agree."
        )
    elif status == "above_range":
        lines.append(
            f"The candidate scored above 50% at every completed rung. Add rungs above "
            f"{estimate.get('lowerAnchor')} before reporting a point estimate."
        )
    elif status == "below_range":
        lines.append(
            f"The candidate scored below 50% at every completed rung. Add rungs below "
            f"{estimate.get('upperAnchor')} before reporting a point estimate."
        )
    else:
        lines.append(
            "The completed rungs do not provide a stable 50% bracket. More games or "
            "additional rungs are required before reporting a point estimate."
        )
    lines.extend(
        [
            "",
            "Keep the per-rung manifests, PGNs, logs, result summaries, executable "
            "checksums, and option bounds with any published interpretation.",
            "",
        ]
    )
    report_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = arguments()
    rungs = parse_rungs(args.rungs)
    if args.quick:
        args.games = 4
        args.concurrency = 1
        args.tc = "2+0.02"
    if args.games < 2 or args.games % 2:
        raise RuntimeError("--games must be a positive even number")
    if min(args.threads, args.hash_mb, args.concurrency, args.build_jobs) < 1:
        raise RuntimeError("threads, hash, concurrency, and build-jobs must be positive")
    if args.seed < 0:
        raise RuntimeError("--seed must be non-negative")

    stockfish = args.stockfish_exe.expanduser().resolve()
    if not stockfish.is_file():
        raise FileNotFoundError(f"Stockfish executable not found: {stockfish}")
    inspection = compare_engines.inspect_uci_engine(stockfish, args.stockfish_arg)
    option_lookup = {
        str(option["name"]).casefold(): option
        for option in inspection["options"]
        if isinstance(option, dict) and "name" in option
    }
    if "uci_limitstrength" not in option_lookup or "uci_elo" not in option_lookup:
        raise RuntimeError(
            "The selected Stockfish executable must advertise UCI_LimitStrength and UCI_Elo"
        )
    for rung in rungs:
        compare_engines.configured_options(
            inspection,
            "Stockfish",
            args.threads,
            args.hash_mb,
            [
                *args.stockfish_option,
                "UCI_LimitStrength=true",
                f"UCI_Elo={rung}",
            ],
            None,
        )

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = (
        args.run_dir.expanduser().resolve()
        if args.run_dir
        else ROOT / "artifacts" / "rating" / f"stockfish-ladder-{stamp}"
    )
    run_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = run_dir / "ladder-manifest.json"
    summary_path = run_dir / "summary.json"
    report_path = run_dir / "report.md"
    uci_elo = option_lookup["uci_elo"]
    configuration = {
        "stockfish": {
            "path": str(stockfish),
            "sha256": compare_engines.file_sha256(stockfish),
            "name": inspection.get("name"),
            "author": inspection.get("author"),
            "version": args.stockfish_version,
            "args": args.stockfish_arg,
            "options": args.stockfish_option,
            "uciEloMin": uci_elo.get("min"),
            "uciEloMax": uci_elo.get("max"),
        },
        "candidate": {},
        "rungs": rungs,
        "gamesPerRung": args.games,
        "timeControl": args.tc,
        "threads": args.threads,
        "hashMb": args.hash_mb,
        "concurrency": args.concurrency,
        "seed": args.seed,
        "openings": str(args.openings.expanduser().resolve()) if args.openings else None,
        "cutechess": str(args.cutechess.expanduser().resolve()) if args.cutechess else None,
        "sfmlPrefix": str(args.sfml_prefix.expanduser().resolve()) if args.sfml_prefix else None,
    }
    candidate_configuration = configuration["candidate"]
    assert isinstance(candidate_configuration, dict)
    if args.engine_exe:
        candidate_path = args.engine_exe.expanduser().resolve()
        if not candidate_path.is_file():
            raise FileNotFoundError(f"Candidate executable not found: {candidate_path}")
        candidate_selector = str(candidate_path)
        candidate_configuration.update(
            {
                "source": "external",
                "selector": candidate_selector,
                "sha256": compare_engines.file_sha256(candidate_path),
            }
        )
    else:
        candidate_commit = compare_engines.git_output(
            "rev-parse", "--verify", f"{args.engine_ref}^{{commit}}"
        )
        candidate_selector = candidate_commit
        candidate_configuration.update(
            {
                "source": "git",
                "requestedRef": args.engine_ref,
                "selector": candidate_commit,
                "commit": candidate_commit,
            }
        )
    candidate_configuration.update(
        {
            "name": args.engine_name,
            "version": args.engine_version,
            "args": args.engine_arg,
            "options": args.engine_option,
        }
    )
    if manifest_path.exists():
        existing = json.loads(manifest_path.read_text(encoding="utf-8"))
        if existing.get("configuration") != configuration:
            raise RuntimeError(
                f"Resume configuration does not match {manifest_path}. "
                "Use a new --run-dir or restore the original options."
            )
        print(f"Resuming ladder: {run_dir}")
    else:
        unexpected = unexpected_new_run_entries(run_dir)
        if unexpected:
            names = ", ".join(path.name for path in unexpected)
            raise RuntimeError(
                f"New ladder directory contains unexpected entries: {names}"
            )
        manifest = {
            "schemaVersion": 1,
            "createdAt": datetime.now().astimezone().isoformat(timespec="seconds"),
            "runnerCommit": compare_engines.git_output("rev-parse", "HEAD"),
            "runnerDirty": bool(
                compare_engines.git_output("status", "--short", "--untracked-files=no")
            ),
            "configuration": configuration,
        }
        manifest_path.write_text(
            json.dumps(manifest, indent=2, allow_nan=False) + "\n",
            encoding="utf-8",
        )
        print(f"Created ladder: {run_dir}")

    rung_results: list[dict[str, object]] = []
    compare_script = ROOT / "scripts" / "compare_engines.py"
    for rung in rungs:
        rung_dir = run_dir / f"rung-{rung}"
        rung_dir.mkdir(exist_ok=True)
        existing_attempt = completed_attempt(rung_dir)
        if existing_attempt:
            attempt, result = existing_attempt
            print(f"Rung {rung}: already complete ({attempt.name}); skipping")
            rung_results.append(rung_summary(rung, attempt, result))
            continue

        attempt = next_attempt(rung_dir)
        attempt.mkdir()
        match_dir = attempt / "match"
        stockfish_label = " ".join(
            item for item in (args.stockfish_name, args.stockfish_version) if item
        )
        command = [
            sys.executable,
            str(compare_script),
            "--baseline-exe", str(stockfish),
            "--baseline-name", f"{stockfish_label} @ {rung}",
            "--candidate-name", args.engine_name,
            "--games", str(args.games),
            "--tc", args.tc,
            "--threads", str(args.threads),
            "--hash", str(args.hash_mb),
            "--concurrency", str(args.concurrency),
            "--build-jobs", str(args.build_jobs),
            "--seed", str(args.seed),
            "--output-dir", str(match_dir),
        ]
        if args.engine_exe:
            command += ["--candidate-exe", candidate_selector]
        else:
            command += ["--candidate", candidate_selector]
        if args.stockfish_version:
            command += ["--baseline-version", args.stockfish_version]
        if args.engine_version:
            command += ["--candidate-version", args.engine_version]
        if args.openings:
            command += ["--openings", str(args.openings.expanduser().resolve())]
        if args.cutechess:
            command += ["--cutechess", str(args.cutechess.expanduser().resolve())]
        if args.sfml_prefix:
            command += ["--sfml-prefix", str(args.sfml_prefix.expanduser().resolve())]
        for argument in args.stockfish_arg:
            command += ["--baseline-arg", argument]
        for argument in args.engine_arg:
            command += ["--candidate-arg", argument]
        for option in args.stockfish_option:
            command += ["--baseline-option", option]
        command += [
            "--baseline-option", "UCI_LimitStrength=true",
            "--baseline-option", f"UCI_Elo={rung}",
        ]
        for option in args.engine_option:
            command += ["--candidate-option", option]

        exit_code = run_and_tee(command, attempt / "driver.log")
        result_path = match_dir / "result.json"
        if exit_code or not result_path.is_file():
            print(
                f"Rung {rung} did not complete. Preserved {attempt}; "
                f"rerun with --run-dir {run_dir} to retry.",
                file=sys.stderr,
            )
            return exit_code or 1
        result = json.loads(result_path.read_text(encoding="utf-8"))
        if result.get("completed") is not True:
            print(
                f"Rung {rung} is incomplete. Preserved {attempt}; "
                f"rerun with --run-dir {run_dir} to retry.",
                file=sys.stderr,
            )
            return 1
        rung_results.append(rung_summary(rung, attempt, result))

    estimate = estimate_local_rating(rung_results)
    summary: dict[str, object] = {
        "schemaVersion": 1,
        "completedAt": datetime.now().astimezone().isoformat(timespec="seconds"),
        "manifest": str(manifest_path.resolve()),
        "qualification": (
            "Rating anchor for this exact local Stockfish limited-strength pool, "
            "hardware, openings, and time control; not FIDE, human, or universal Elo."
        ),
        "rungs": rung_results,
        "localPoolEstimate": estimate,
    }
    summary_path.write_text(
        json.dumps(summary, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    write_report(report_path, summary, args.stockfish_name)
    print(f"Ladder summary -> {summary_path}")
    print(f"Qualified report -> {report_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
