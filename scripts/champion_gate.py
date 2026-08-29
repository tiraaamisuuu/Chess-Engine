#!/usr/bin/env python3
"""Run and adjudicate a reproducible candidate-versus-champion SPRT gate."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CHAMPION = ROOT / "research" / "champion.json"
COMPARE_SCRIPT = ROOT / "scripts" / "compare_engines.py"


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--champion-file", type=Path, default=DEFAULT_CHAMPION)
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("show", help="show the frozen champion and gate defaults")

    run_parser = subparsers.add_parser("run", help="run one immutable promotion gate")
    run_parser.add_argument("--candidate", default="HEAD", help="committed candidate ref")
    run_parser.add_argument("--run-dir", type=Path, required=True)
    run_parser.add_argument("--games", type=int)
    run_parser.add_argument("--tc", dest="time_control")
    run_parser.add_argument("--threads", type=int)
    run_parser.add_argument("--hash", type=int, dest="hash_mb")
    run_parser.add_argument("--concurrency", type=int)
    run_parser.add_argument("--seed", type=int)
    run_parser.add_argument("--elo0", type=float)
    run_parser.add_argument("--elo1", type=float)
    run_parser.add_argument("--build-jobs", type=int, default=12)
    run_parser.add_argument("--quick", action="store_true")

    apply_parser = subparsers.add_parser(
        "apply", help="update the champion registry after a passing gate"
    )
    apply_parser.add_argument("--run-dir", type=Path, required=True)
    return parser.parse_args()


def read_json(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"unable to read JSON: {path}") from error
    if not isinstance(value, dict):
        raise RuntimeError(f"expected a JSON object: {path}")
    return value


def write_json(path: Path, value: dict[str, object]) -> None:
    path.write_text(
        json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_output(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True
    ).strip()


def resolve_commit(ref: str) -> str:
    try:
        return git_output("rev-parse", "--verify", f"{ref}^{{commit}}")
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"unable to resolve committed candidate ref: {ref}") from error


def load_champion(path: Path) -> dict[str, object]:
    champion = read_json(path)
    commit = champion.get("commit")
    if not isinstance(commit, str) or len(commit) != 40:
        raise RuntimeError("champion registry must contain a full 40-character commit")
    if resolve_commit(commit) != commit:
        raise RuntimeError(f"champion commit is unavailable: {commit}")
    defaults = champion.get("gateDefaults")
    if not isinstance(defaults, dict):
        raise RuntimeError("champion registry is missing gateDefaults")
    return champion


def positive_even(value: int, name: str) -> int:
    if value < 2 or value % 2:
        raise RuntimeError(f"{name} must be a positive even number")
    return value


def positive(value: int, name: str) -> int:
    if value < 1:
        raise RuntimeError(f"{name} must be positive")
    return value


def contract_from_args(
    args: argparse.Namespace, champion: dict[str, object]
) -> dict[str, object]:
    defaults = champion["gateDefaults"]
    assert isinstance(defaults, dict)

    def selected(argument_name: str, default_name: str | None = None) -> object:
        value = getattr(args, argument_name)
        key = default_name or argument_name
        return defaults.get(key) if value is None else value

    contract = {
        "games": positive_even(int(selected("games")), "games"),
        "timeControl": str(selected("time_control", "timeControl")),
        "threads": positive(int(selected("threads")), "threads"),
        "hashMb": positive(int(selected("hash_mb", "hashMb")), "hash"),
        "concurrency": positive(int(selected("concurrency")), "concurrency"),
        "seed": int(selected("seed")),
        "elo0": float(selected("elo0")),
        "elo1": float(selected("elo1")),
        "alpha": float(defaults.get("alpha", 0.05)),
        "beta": float(defaults.get("beta", 0.05)),
        "sprt": not args.quick,
    }
    if not contract["timeControl"]:
        raise RuntimeError("time control cannot be empty")
    if int(contract["seed"]) < 0:
        raise RuntimeError("seed must be non-negative")
    if float(contract["elo1"]) <= float(contract["elo0"]):
        raise RuntimeError("elo1 must be greater than elo0")
    if args.quick:
        contract.update(
            {
                "games": 4,
                "timeControl": "2+0.02",
                "concurrency": 1,
                "sprt": False,
            }
        )
    return contract


def technical_failure_count(match_result: dict[str, object]) -> int:
    failures = match_result.get("failures")
    if not isinstance(failures, dict):
        return 0
    return sum(int(value or 0) for value in failures.values())


def classify_gate_result(
    match_result: dict[str, object], *, quick: bool = False
) -> tuple[str, str]:
    if technical_failure_count(match_result):
        return "technical_failure", "match contained a technical termination"
    if match_result.get("completed") is not True:
        return "interrupted", "match did not reach a valid terminal condition"
    if quick:
        return "smoke_pass", "workflow completed; four games are not strength evidence"
    sprt = match_result.get("sprt")
    sprt = sprt if isinstance(sprt, dict) else {}
    decision = sprt.get("decision")
    if decision == "accepted_h1":
        return "promote", "SPRT accepted H1 for the candidate"
    if decision == "accepted_h0":
        return "reject", "SPRT accepted H0 against the candidate"
    return "inconclusive", "maximum games reached without an SPRT boundary"


def gate_report(result: dict[str, object]) -> str:
    match = result.get("match")
    match = match if isinstance(match, dict) else {}
    score = match.get("score")
    score = score if isinstance(score, dict) else {}
    elo = match.get("elo")
    elo = elo if isinstance(elo, dict) else {}
    sprt = match.get("sprt")
    sprt = sprt if isinstance(sprt, dict) else {}
    contract = result.get("contract")
    contract = contract if isinstance(contract, dict) else {}
    return f"""# Forklift Champion Gate

- Decision: **{result.get('decision')}**
- Reason: {result.get('reason')}
- Champion: `{result.get('championCommit')}`
- Candidate: `{result.get('candidateCommit')}`
- Contract: {contract.get('games')} games maximum at `{contract.get('timeControl')}`, {contract.get('threads')} thread(s), {contract.get('hashMb')} MiB hash, concurrency {contract.get('concurrency')}, seed {contract.get('seed')}
- Score: {score.get('candidateWins', '—')}-{score.get('draws', '—')}-{score.get('baselineWins', '—')} over {score.get('games', match.get('finishedGames', '—'))} games
- Relative Elo: {elo.get('display', 'unavailable')}
- SPRT: {sprt.get('decision', 'disabled')} (LLR {sprt.get('llr', '—')}, bounds {sprt.get('lowerBound', '—')} to {sprt.get('upperBound', '—')})
- Technical terminations: {technical_failure_count(match)}

This result compares committed revisions under the frozen champion contract.
It is relative match evidence, not an absolute Elo measurement.
"""


def run_gate(
    args: argparse.Namespace, champion_path: Path, champion: dict[str, object]
) -> int:
    tracked_changes = git_output("status", "--short", "--untracked-files=no")
    if tracked_changes:
        raise RuntimeError("refusing to run a promotion gate with tracked changes")

    champion_commit = str(champion["commit"])
    candidate_commit = resolve_commit(args.candidate)
    if candidate_commit == champion_commit and not args.quick:
        raise RuntimeError("candidate resolves to the current champion")

    run_dir = args.run_dir.expanduser().resolve()
    if run_dir.exists() and any(run_dir.iterdir()):
        existing = run_dir / "gate-result.json"
        if existing.is_file():
            result = read_json(existing)
            print(gate_report(result))
            return 0
        raise RuntimeError(
            "run directory contains an interrupted gate; preserve it and choose a new directory"
        )
    run_dir.mkdir(parents=True, exist_ok=True)
    match_dir = run_dir / "match"
    contract = contract_from_args(args, champion)
    manifest = {
        "schemaVersion": 1,
        "createdAt": datetime.now().astimezone().isoformat(timespec="seconds"),
        "runnerCommit": git_output("rev-parse", "HEAD"),
        "championRegistry": str(champion_path),
        "championRegistrySha256": file_sha256(champion_path),
        "championCommit": champion_commit,
        "candidateRequestedRef": args.candidate,
        "candidateCommit": candidate_commit,
        "contract": contract,
        "artifacts": {
            "match": str(match_dir),
            "driverLog": str(run_dir / "driver.log"),
            "result": str(run_dir / "gate-result.json"),
            "report": str(run_dir / "report.md"),
        },
    }
    write_json(run_dir / "gate-manifest.json", manifest)

    command = [
        sys.executable,
        str(COMPARE_SCRIPT),
        "--baseline",
        champion_commit,
        "--candidate",
        candidate_commit,
        "--baseline-name",
        "Champion",
        "--candidate-name",
        "Candidate",
        "--games",
        str(contract["games"]),
        "--tc",
        str(contract["timeControl"]),
        "--threads",
        str(contract["threads"]),
        "--hash",
        str(contract["hashMb"]),
        "--concurrency",
        str(contract["concurrency"]),
        "--seed",
        str(contract["seed"]),
        "--build-jobs",
        str(args.build_jobs),
        "--output-dir",
        str(match_dir),
    ]
    if contract["sprt"]:
        command.extend(
            [
                "--sprt",
                "--elo0",
                str(contract["elo0"]),
                "--elo1",
                str(contract["elo1"]),
            ]
        )
    if args.quick:
        command.append("--quick")

    print("Champion gate")
    print(f"  Champion : {champion_commit}")
    print(f"  Candidate: {candidate_commit}")
    print(f"  Output   : {run_dir}")
    print("+ " + " ".join(command), flush=True)
    with (run_dir / "driver.log").open("w", encoding="utf-8", buffering=1) as log:
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
        exit_code = process.wait()

    match_result_path = match_dir / "result.json"
    match_result = (
        read_json(match_result_path)
        if match_result_path.is_file()
        else {"completed": False, "processExitCode": exit_code}
    )
    decision, reason = classify_gate_result(match_result, quick=args.quick)
    result = {
        "schemaVersion": 1,
        "completedAt": datetime.now().astimezone().isoformat(timespec="seconds"),
        "decision": decision,
        "reason": reason,
        "championCommit": champion_commit,
        "candidateCommit": candidate_commit,
        "contract": contract,
        "matchResult": str(match_result_path),
        "matchResultSha256": (
            file_sha256(match_result_path) if match_result_path.is_file() else None
        ),
        "match": match_result,
    }
    write_json(run_dir / "gate-result.json", result)
    (run_dir / "report.md").write_text(gate_report(result), encoding="utf-8")
    print(f"\nDecision: {decision} — {reason}")
    print(f"Gate result -> {run_dir / 'gate-result.json'}")
    return 0 if decision not in {"technical_failure", "interrupted"} else 1


def apply_promotion(
    run_dir: Path, champion_path: Path, champion: dict[str, object]
) -> int:
    run_dir = run_dir.expanduser().resolve()
    result_path = run_dir / "gate-result.json"
    result = read_json(result_path)
    if result.get("decision") != "promote":
        raise RuntimeError("only a passing gate can update the champion registry")
    if result.get("championCommit") != champion.get("commit"):
        raise RuntimeError("gate baseline is stale; the registered champion has changed")
    candidate_commit = str(result.get("candidateCommit"))
    if resolve_commit(candidate_commit) != candidate_commit:
        raise RuntimeError("promoted candidate commit is unavailable")

    match = result.get("match")
    match = match if isinstance(match, dict) else {}
    manifest_path = Path(str(match.get("manifest", "")))
    match_manifest = read_json(manifest_path)
    engines = match_manifest.get("engines")
    engines = engines if isinstance(engines, list) else []
    candidate_engine = next(
        (
            engine
            for engine in engines
            if isinstance(engine, dict) and engine.get("side") == "Candidate"
        ),
        {},
    )

    updated = dict(champion)
    updated["commit"] = candidate_commit
    updated["establishedAt"] = datetime.now().astimezone().date().isoformat()
    updated["previousChampion"] = result.get("championCommit")
    engine = updated.get("engine")
    engine = dict(engine) if isinstance(engine, dict) else {}
    engine["binarySha256"] = candidate_engine.get("sha256")
    updated["engine"] = engine
    updated["promotionEvidence"] = {
        "gateResult": str(result_path),
        "gateResultSha256": file_sha256(result_path),
        "decision": result.get("decision"),
        "contract": result.get("contract"),
    }
    write_json(champion_path, updated)
    print(f"Champion registry updated -> {champion_path}")
    print("Review, document, commit, and push this registry change.")
    return 0


def main() -> int:
    args = arguments()
    champion_path = args.champion_file.expanduser().resolve()
    champion = load_champion(champion_path)
    if args.command == "show":
        print(json.dumps(champion, indent=2))
        return 0
    if args.command == "run":
        return run_gate(args, champion_path, champion)
    if args.command == "apply":
        return apply_promotion(args.run_dir, champion_path, champion)
    raise RuntimeError(f"unsupported command: {args.command}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
