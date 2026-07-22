#!/usr/bin/env python3
"""Build two Git revisions and run a reproducible paired Cute Chess match."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
from datetime import datetime
from urllib.request import urlopen
import zipfile


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / ".tools"
OPENINGS_URL = (
    "https://raw.githubusercontent.com/official-stockfish/books/master/"
    "UHO_4060_v4.epd.zip"
)
OPENINGS_SHA256 = "a97424c5b98b42f8c27ff450f0681ad11696148548c975752350e98417ead11d"


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare two chess engine revisions with paired opening games."
    )
    parser.add_argument("--baseline", default="v0.4.0", help="Git ref for engine A")
    parser.add_argument("--candidate", default="HEAD", help="Git ref for engine B")
    parser.add_argument("--games", type=int, default=200, help="Even number of games")
    parser.add_argument("--tc", default="10+0.1", help="Cute Chess time control")
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--hash", type=int, default=256, dest="hash_mb")
    parser.add_argument("--concurrency", type=int, default=2)
    parser.add_argument("--build-jobs", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--openings", type=Path)
    parser.add_argument("--cutechess", type=Path)
    parser.add_argument("--sfml-prefix", type=Path)
    parser.add_argument("--candidate-eval-file", type=Path)
    parser.add_argument("--baseline-eval-file", type=Path)
    parser.add_argument("--sprt", action="store_true")
    parser.add_argument("--elo0", type=float, default=0.0)
    parser.add_argument("--elo1", type=float, default=5.0)
    parser.add_argument("--quick", action="store_true", help="Four-game installation check")
    parser.add_argument("--build-only", action="store_true")
    return parser.parse_args()


def run(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
    printable = " ".join(command)
    print(f"+ {printable}", flush=True)
    return subprocess.run(command, check=True, text=True, **kwargs)


def git_output(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def safe_name(value: str) -> str:
    return "".join(character if character.isalnum() else "-" for character in value).strip("-")


def extract_ref(ref: str, destination: Path) -> str:
    commit = git_output("rev-parse", "--verify", f"{ref}^{{commit}}")
    marker = destination / ".source-commit"
    if marker.exists() and marker.read_text(encoding="utf-8").strip() == commit:
        return commit
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    archive = subprocess.check_output(["git", "archive", commit], cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        destination_root = destination.resolve()
        for member in tar.getmembers():
            target = (destination / member.name).resolve()
            if destination_root not in target.parents and target != destination_root:
                raise RuntimeError("Git archive contains an unsafe path")
        tar.extractall(destination)
    marker.write_text(commit + "\n", encoding="utf-8")
    return commit


def default_sfml_prefix() -> Path | None:
    configured = os.environ.get("SFML_PREFIX")
    if configured:
        return Path(configured).expanduser()
    mac_default = Path.home() / ".local" / "sfml-2.6.2"
    return mac_default if mac_default.exists() else None


def find_engine(build: Path) -> Path:
    names = {"chess-engine-uci", "chess-engine-uci.exe", "gui", "gui.exe"}
    candidates = [path for path in build.rglob("*") if path.is_file() and path.name in names]
    candidates.sort(key=lambda path: (path.name.startswith("gui"), len(path.parts)))
    if not candidates:
        raise RuntimeError(f"No UCI-capable engine binary found under {build}")
    return candidates[0].resolve()


def build_ref(ref: str, label: str, jobs: int, sfml_prefix: Path | None) -> tuple[Path, str]:
    ref_root = TOOLS / "engine-match" / f"{label}-{safe_name(ref)}"
    source = ref_root / "source"
    build = ref_root / "build"
    commit = extract_ref(ref, source)

    configure = [
        "cmake", "-S", str(source), "-B", str(build),
        "-DCMAKE_BUILD_TYPE=Release", "-DCHESS_BUILD_GUI=OFF", "-DBUILD_TESTING=OFF",
    ]
    # Before v1 the UCI loop lived in the SFML executable, so those refs still
    # need SFML even though the match itself is headless.
    if not (source / "src" / "uci_main.cpp").exists() and sfml_prefix:
        configure.append(f"-DCMAKE_PREFIX_PATH={sfml_prefix.resolve()}")
    run(configure)
    run(["cmake", "--build", str(build), "--config", "Release", "--parallel", str(jobs)])
    engine = find_engine(build)
    print(f"{label}: {ref} ({commit[:10]}) -> {engine}")
    return engine, commit


def ensure_openings(requested: Path | None, quick: bool) -> Path:
    if requested:
        path = requested.expanduser().resolve()
        if not path.is_file():
            raise FileNotFoundError(f"Opening suite not found: {path}")
        return path
    if quick:
        return ROOT / "tests" / "openings.epd"

    destination = TOOLS / "openings"
    book = destination / "UHO_4060_v4.epd"
    if book.exists():
        return book
    print("Downloading the pinned Stockfish UHO opening suite...", flush=True)
    payload = urlopen(OPENINGS_URL, timeout=120).read()
    if hashlib.sha256(payload).hexdigest() != OPENINGS_SHA256:
        raise RuntimeError("Opening archive checksum mismatch")
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        archive.extract("UHO_4060_v4.epd", destination)
    return book


def find_cutechess(requested: Path | None) -> Path:
    windows_install = TOOLS / "cutechess-windows"
    windows_candidates = list(windows_install.rglob("cutechess-cli.exe")) if windows_install.exists() else []
    candidates = [
        requested,
        Path(os.environ["CUTECHESS_BIN"]) if os.environ.get("CUTECHESS_BIN") else None,
        Path(shutil.which("cutechess-cli")) if shutil.which("cutechess-cli") else None,
        TOOLS / "build" / "cutechess" / ("cutechess-cli.exe" if os.name == "nt" else "cutechess-cli"),
        *windows_candidates,
    ]
    for candidate in candidates:
        if candidate and candidate.expanduser().is_file():
            return candidate.expanduser().resolve()
    raise FileNotFoundError(
        "cutechess-cli is unavailable. Run scripts/install_cutechess.sh on macOS/Linux "
        "or scripts/install_cutechess.ps1 on Windows."
    )


def engine_definition(name: str, binary: Path, threads: int, hash_mb: int,
                      eval_file: Path | None) -> list[str]:
    definition = [
        f"name={name}", f"cmd={binary}", "arg=--uci", "proto=uci",
        f"option.Hash={hash_mb}", f"option.Threads={threads}",
    ]
    if eval_file:
        network = eval_file.expanduser().resolve()
        if not network.is_file():
            raise FileNotFoundError(f"NNUE network not found: {network}")
        definition.extend([f"option.EvalFile={network}", "option.Use NNUE=true"])
    return definition


def write_web_profiles(baseline_ref: str, baseline: Path, baseline_commit: str,
                       candidate_ref: str, candidate: Path, candidate_commit: str) -> Path:
    """Expose locally built revisions to the web GUI without committing machine paths."""
    destination = TOOLS / "engine-match" / "profiles.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    legacy_baseline = baseline_ref.lower().startswith(("v0.", "v0-"))
    payload = {
        "version": 1,
        "profiles": [
            {
                "id": f"candidate-{safe_name(candidate_ref)}",
                "name": f"Candidate · {candidate_ref}",
                "detail": f"Committed comparison build · revision {candidate_commit[:10]}",
                "role": "candidate",
                "badge": "CANDIDATE · COMMITTED",
                "path": str(candidate.resolve()),
                "args": ["--uci"],
            },
            {
                "id": f"baseline-{safe_name(baseline_ref)}",
                "name": f"{'Legacy' if legacy_baseline else 'Baseline'} · {baseline_ref}",
                "detail": (
                    f"Older release build · revision {baseline_commit[:10]}" if legacy_baseline
                    else f"Reference comparison build · revision {baseline_commit[:10]}"
                ),
                "role": "legacy" if legacy_baseline else "baseline",
                "badge": "LEGACY · BASELINE" if legacy_baseline else "REFERENCE · BASELINE",
                "path": str(baseline.resolve()),
                "args": ["--uci"],
            },
        ],
    }
    destination.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Web GUI profiles -> {destination}")
    return destination


def main() -> int:
    args = arguments()
    if args.games < 2 or args.games % 2:
        raise SystemExit("--games must be a positive even number so colours remain paired")
    if min(args.threads, args.hash_mb, args.concurrency, args.build_jobs) < 1:
        raise SystemExit("threads, hash, concurrency, and build-jobs must be positive")
    if args.quick:
        args.games = 4
        # Keep the smoke test short without pushing process/protocol overhead
        # into bullet-time forfeits on slower CI or development machines.
        args.tc = "2+0.02"
        args.concurrency = 1

    sfml = args.sfml_prefix.expanduser() if args.sfml_prefix else default_sfml_prefix()
    baseline, baseline_commit = build_ref(args.baseline, "baseline", args.build_jobs, sfml)
    candidate, candidate_commit = build_ref(args.candidate, "candidate", args.build_jobs, sfml)
    write_web_profiles(
        args.baseline, baseline, baseline_commit,
        args.candidate, candidate, candidate_commit,
    )
    if args.build_only:
        return 0

    cutechess = find_cutechess(args.cutechess)
    openings = ensure_openings(args.openings, args.quick)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    output = ROOT / "artifacts" / "elo" / f"{safe_name(args.candidate)}-vs-{safe_name(args.baseline)}-{stamp}"
    output.mkdir(parents=True, exist_ok=True)
    pgn = output / "games.pgn"
    log = output / "match.log"

    command = [str(cutechess)]
    command += ["-engine", *engine_definition("Candidate", candidate, args.threads, args.hash_mb,
                                               args.candidate_eval_file)]
    command += ["-engine", *engine_definition("Baseline", baseline, args.threads, args.hash_mb,
                                               args.baseline_eval_file)]
    command += [
        "-each", f"tc={args.tc}",
        "-openings", f"file={openings}", "format=epd", "order=random",
        "-games", str(args.games), "-repeat", "-recover",
        "-concurrency", str(args.concurrency),
        "-resign", "movecount=6", "score=700",
        "-draw", "movenumber=40", "movecount=8", "score=10",
        "-pgnout", str(pgn),
    ]
    if args.sprt:
        command += [
            "-sprt", f"elo0={args.elo0}", f"elo1={args.elo1}",
            "alpha=0.05", "beta=0.05",
        ]

    print("\nPaired engine match")
    print(f"  Candidate: {args.candidate} ({candidate_commit[:10]})")
    print(f"  Baseline : {args.baseline} ({baseline_commit[:10]})")
    print(f"  Games    : {args.games} at {args.tc}")
    print(f"  Openings : {openings}")
    print(f"  Output   : {output}\n")

    with log.open("w", encoding="utf-8") as log_file:
        process = subprocess.Popen(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="")
            log_file.write(line)
        return process.wait()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
