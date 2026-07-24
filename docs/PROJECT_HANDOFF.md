# Project Handoff: Windows PC, Strength Testing, and the v1 Roadmap

This document is the durable handoff for continuing the Chess Engine project in
a new Codex chat or on a different computer. It records the repository state,
the user's decisions, what is already implemented, how to reproduce the current
results, the known limitations, and the recommended order of future work.

The intended destination machine is:

- Windows 11
- AMD Ryzen 9 5900X
- NVIDIA RTX 3070 with 8 GB VRAM
- 32 GB DDR4-3200
- approximately 150–175 GB available if Linux is added later

The user is staying on native Windows for development, testing, Elo estimation,
and NNUE training for now. CachyOS remains an optional future desktop project;
it is not a prerequisite for this engine.

## Copy this into the first message of the new Codex chat

```text
Open this repository and read docs/PROJECT_HANDOFF.md completely before making
changes. Treat it as the project handoff and follow its "New Codex chat startup
checklist" in order.

The active development line is codex/v1-engine-rework, not main. First inspect
git status, fetch the remote, confirm the branch is current, build the headless
engine, run the available Windows tests, launch the local web GUI, and report
exactly what passed or failed. Preserve existing work and do not reset or
rewrite history.

After the PC environment is verified, continue the roadmap in the handoff. The
first product task should be the Windows-native testing and calibrated engine
rating workflow. Do not begin serious NNUE training until the benchmark
baseline, data provenance, dataset format, validation split, and incremental
NNUE evaluation plan are sound.

Commit coherent changes to the codex/v1-engine-rework branch with descriptive
messages and push them. Never claim an absolute Elo from a single game-analysis
website score or from a small match.
```

## Authoritative repository state

- GitHub repository:
  `https://github.com/tiraaamisuuu/Chess-Engine.git`
- Development branch: `codex/v1-engine-rework`
- Stable/default branch: `main`
- Current public legacy release/tag: `v0.4.0`
- The v1 branch started from `main` commit `518a3c1`.
- At the time this handoff was written, the code tip immediately before the
  handoff commit was `9214b17`.
- The development branch was 21 code commits ahead of `main` and zero behind
  after fetching all remotes and tags.
- The worktree was clean and matched `origin/codex/v1-engine-rework`.

Always verify rather than assuming those facts remain current:

```powershell
git fetch --all --prune --tags
git status --short --branch
git branch -vv
git rev-list --left-right --count origin/main...origin/codex/v1-engine-rework
git log --oneline --decorate -15
```

Do not develop from `main` by accident. Do not reset the development branch to
`main`; `main` intentionally does not contain the v1 work yet.

## What the user wants

The user's long-term objective is not merely a playable school-project engine.
They want to turn the existing repository into a clean, measurable, extensible
engine project and push it as far as the available hardware and time allow.

The agreed direction is:

1. Keep the existing repository and history.
2. Build v1 as a clean release on a development branch rather than throwing the
   entire engine away.
3. Make correctness and repeatable testing non-negotiable.
4. Measure every search/evaluation improvement with controlled paired games.
5. Use the web GUI as the primary interface.
6. Retain the SFML application only as a legacy fallback during the transition.
7. Support current, legacy, external UCI, Stockfish, and future NNUE profiles.
8. Train an NNUE later on the Ryzen 9/RTX 3070 PC.
9. Do not use the old account/repository handle as the engine's product name.
   The product is currently simply **Chess Engine** and deliberately has no
   invented brand name.
10. Commit and push coherent progress frequently.

The GitHub owner name remains present in the repository URL because it is the
account name. It must not be reintroduced into UI copy, engine branding, game
metadata defaults, or documentation as a product name.

## Terminology

- **Engine**: the C++ executable that accepts positions, searches them, and
  chooses moves.
- **Revision/build**: a compiled version of the engine from a Git commit or tag.
- **Classical evaluation**: the hand-written material, piece-square, pawn,
  mobility, rook-file, passed-pawn, and king-safety scoring.
- **NNUE network/model**: a trained weight file loaded by the engine to replace
  classical evaluation. The model is not the whole engine.
- **Stockfish**: an external UCI engine used as an opponent, teacher, and rough
  calibration reference.
- **Relative Elo**: the estimated strength difference between two engines in a
  controlled match.
- **Absolute/anchored rating**: an approximate rating inferred by playing a
  calibrated pool. It is environment- and pool-dependent and should never be
  presented as universal human Elo.
- **Game performance rating**: a website's estimate for one played game. This
  is not an engine Elo rating.

## What is already implemented

### Chess core and correctness

- Full legal move generation, including castling, en passant, and promotion.
- Make/unmake with deterministic state restoration.
- FEN loading, validation, and round-tripping.
- UCI and SAN move notation.
- Zobrist hashing with repetition-correct en-passant treatment.
- Threefold repetition, fifty-move, insufficient-material, stalemate, and
  checkmate adjudication.
- Transposition-table collision handling.
- Fixed-capacity move lists in recursive search/perft paths.
- Standard six-position perft suite through depth 4.
- Randomized make/unmake and incremental-hash tests.

### Search

- Iterative deepening.
- Alpha-beta/negamax with principal-variation search.
- Quiescence search.
- Transposition table.
- Aspiration windows.
- Null-move pruning with verification behavior.
- Late-move reductions.
- Razoring and shallow futility pruning.
- Mate-distance pruning.
- Killer, history, capture-history, and countermove ordering.
- Static exchange evaluation for captures.
- Soft and hard time limits.
- Configurable UCI move overhead.
- Experimental root-parallel search.

One thread remains the default. Multi-thread search no longer has the original
catastrophic scaling behavior, but it has not yet proved a positive equal-time
Elo gain and must not be made the default without match evidence.

### Classical evaluation

The current evaluator includes:

- material
- tapered piece-square terms
- bishop pair
- pawn structure
- passed pawns
- rook-file terms
- allocation-free pseudo-mobility
- king centralization, shelter, and castling-related terms

The terms are still primarily hand-authored. They have not undergone a complete
Texel/SPSA-style automated tuning campaign.

### UCI executable

The independent `chess-engine-uci` target supports:

- `uci`, `isready`, `ucinewgame`, `position`, `go`, `stop`, and `quit`
- FEN and start-position setup
- `go depth`, `movetime`, clocks/increments, `movestogo`, `infinite`, and
  `searchmoves`
- `Hash`
- `Threads`
- `Move Overhead`
- `Clear Hash`
- `EvalFile`
- `Use NNUE`

Current UCI identity:

```text
id name Chess Engine v1.0-dev
id author Alfie Corthine
```

### Web GUI

The web UI is the primary interface. It is a local Python HTTP service using
`python-chess`, a vanilla HTML/CSS/JavaScript frontend, and real UCI engine
processes. It binds to `127.0.0.1` by default.

Implemented behavior includes:

- the New Game setup dialog opens on first load
- PvP, PvC, and CvC
- human side selection
- independent white and black engines in CvC
- board flipping
- click-to-move
- pointer-following drag-and-drop without leaving a duplicate source piece
- legal-move indicators
- promotion selection
- undo
- move list
- game state and termination display
- evaluation bar
- on-demand analysis, depth, score, nodes, NPS, time, and PV
- autoplay/pause for CvC
- responsive desktop/mobile layout
- sharp minimalist visual design
- engine-role badges for development, candidate, baseline/legacy, external,
  Stockfish, and NNUE profiles
- magnetic thinking-time slider from 50 ms to 10 seconds
- presets at 250 ms, 650 ms, 1.5 seconds, 5 seconds, and 10 seconds
- PGN, FEN, and JSON export
- editable PGN Event/White/Black metadata
- copy and file-download export actions
- persistent external UCI engine library
- verified one-click official Stockfish 18 installation for supported platforms
- importing and removing other trusted UCI executables

The built-in Stockfish installer downloads a pinned platform archive, verifies
its SHA-256, extracts the expected executable and GPL files, performs a UCI
handshake, and stores it under the ignored `.tools/user-engines/` directory.
External engines run with the user's local account permissions and must be
treated as trusted executables.

### Engine comparison workflow

`scripts/compare_engines.py`:

- accepts two committed Git refs
- accepts existing external UCI executables on either side
- discovers UCI identity, supported options, and option bounds at runtime
- validates repeated arbitrary UCI options per engine
- archives and builds each ref in isolated ignored directories
- installs/discovers Cute Chess
- downloads and verifies the Stockfish UHO 4060 opening suite
- plays paired openings with reversed colours
- applies equal thread/hash/time settings
- supports ordinary fixed-game matches and SPRT
- writes complete logs and PGNs under `artifacts/elo/`
- writes checksummed machine-readable match manifests and result summaries
- registers the resulting local binaries as web GUI engine profiles
- can compare two NNUE files with the same engine revision

Git-based selections must resolve to committed refs because the builder
intentionally uses `git archive`; external selections use an existing
checksummed executable.

`scripts/calibrate_rating.py` runs a deterministic multi-rung Stockfish
limited-strength ladder on top of the same paired runner. It validates
`UCI_Elo` rungs against the live engine bounds, pins Git candidates to an exact
commit, skips completed rungs when resumed, preserves incomplete attempts, and
writes aggregate JSON plus a deliberately qualified local-pool report.

### NNUE foundation

The repository contains:

- a versioned `HalfKP-v1` feature contract
- a binary loader with magic/version/shape validation
- quantized C++ inference
- a deterministic loader/inference test fixture
- a PGN-to-labelled-JSONL generator using a UCI teacher
- a PyTorch `EmbeddingBag` training implementation
- validation loss reporting
- direct quantized `.nnue` export
- `.pt` checkpoint output
- UCI and web profile loading

There is no bundled trained network yet. Classical evaluation remains the
default.

The current C++ NNUE inference rebuilds both accumulators from the entire board
at each evaluation. It is a correct reference path, not a production-speed
incremental NNUE implementation.

## Important development commits

These commits explain how the present branch evolved:

- `d42322a` — v1 headless core and correctness baseline
- `f7c2d31` — NNUE pipeline and stronger search infrastructure
- `1c197e1` — runner-portable CI gates
- `9de940e` — paired engine testing and minimalist web GUI
- `3e01127` — configurable matches and true pointer drag behavior
- `1bd6c7e` — clear engine generations and recorded match results
- `4f1bdc8` — game export and repaired web analysis
- `f88a255` — magnetic engine-time controls
- `40026f2` — persistent external UCI engine library
- `814f95f` — allocation-free classical mobility
- `6d22b62` — fixed recursive move lists
- `dea7450` — reduced search timing overhead
- `3ab4451`, `dcdaaa0`, `2d5c805` — low-clock safety and UCI move overhead
- `48f6352` — low-clock tournament verification
- `e177ae6` — deterministic low-budget testing
- `018b547` — setup-first onboarding and Stockfish import
- `89119ae` — verified one-click Stockfish installer
- `9214b17` — preserve the actively selected setup opponent

Use `git show <commit>` when a future change regresses one of these behaviors.

## Repository structure

```text
.github/workflows/
  ci.yml                  Linux/macOS/Windows headless CI, sanitizers, web smoke
  release.yml             tag-triggered cross-platform ZIP release workflow

assets/
  pieces/                 SVG pieces used by the web GUI
  pieces_png/             PNG pieces used by the SFML GUI

docs/
  BASELINE.md             pre-v1 correctness and performance baseline
  BENCHMARKING.md         paired games, SPRT, and network comparisons
  NNUE.md                 current training/export contract
  PROJECT_HANDOFF.md      this handoff and full continuation plan
  V1_RESULTS.md           measured development results
  V1_ROADMAP.md           short release-level roadmap

scripts/
  compare_engines.py      committed-ref paired match builder/runner
  install_cutechess.*     pinned Cute Chess installers
  run_web_gui.*           build/venv/server launchers
  run_quality_gate.sh     regression plus UCI smoke on POSIX
  run_regression.sh       perft plus fixed-position benchmark
  run_uci_smoke.sh        UCI lifecycle/options/searchmoves checks
  run_sanitizers.sh       ASan/UBSan gate
  nnue/
    generate_dataset.py   PGN sampling and Stockfish teacher labelling
    train.py              PyTorch training and quantized export
    requirements.txt      NNUE Python dependencies

src/
  chess_types.hpp         pieces, moves, fixed lists, hashes, TT
  board.hpp               board state, rules, move generation, FEN, SAN
  game_status.hpp         terminal state and draw adjudication
  evaluation.hpp          classical evaluation and evaluator selection
  nnue.hpp                HalfKP-v1 loader and quantized inference
  see.hpp                 static exchange evaluation
  search.hpp              search, perft, and benchmark implementation
  time_management.hpp     GUI and UCI time allocation
  uci.cpp/.hpp            standalone UCI protocol service
  uci_main.cpp            UCI executable entry point
  tools_main.cpp          perft/divide/benchmark executable
  main.cpp, ui.cpp/.hpp   legacy SFML application

tests/
  core_tests.cpp          deterministic C++ correctness tests
  openings.epd            small quick-check paired opening suite
  web_api_smoke.py        real-engine HTTP/API smoke test

web/
  server.py               local HTTP/UCI bridge and engine library
  index.html              application structure and dialogs
  styles.css              visual system and responsive layout
  app.js                  board interaction and API client
```

The chess core is currently header-heavy. This makes small experiments easy but
increases compile coupling and is not the ideal final module boundary.

## What has been measured

### Correctness

- Headless and GUI configurations have built successfully on the original
  Apple Silicon development host.
- C++ core and UCI CTest targets passed.
- Six-position perft passed through depth 4.
- The web API smoke test passed against a real UCI engine.
- NNUE Python files passed syntax compilation.
- CI was configured for Linux, macOS, Windows, sanitizers, and the real-engine
  web smoke test.

Always check the live GitHub CI state again before release.

### Throughput

On the Apple M3 development host:

- replacing full pseudo-move-vector generation in the mobility evaluator with
  an allocation-free counter improved the identical depth-12 search tree by
  approximately 11.8%
- fixed-capacity recursive move lists improved an identical depth-11 tree by a
  further median 11.5%
- reduced clock sampling and faster material checks improved the same tree by a
  further median 4.4%

NPS improvements are useful telemetry, not direct Elo proof.

### Relative matches

Preliminary candidate versus `v0.4.0`, 100 paired games, UHO openings,
`2+0.02`, one thread, 256 MB hash:

| Candidate wins | Legacy wins | Draws | Score | Relative Elo |
|---:|---:|---:|---:|---:|
| 70 | 13 | 17 | 78.5% | +225.0 ± 73.7 |

Optimized search revision versus its pre-optimization revision, 100 paired games
under the same fast conditions:

| Optimized wins | Earlier wins | Draws | Score | Relative Elo |
|---:|---:|---:|---:|---:|
| 48 | 25 | 27 | 61.5% | +81.4 ± 59.8 |

These samples are promising but too small and too fast for a release-grade Elo
claim.

### Interpreting the Chessigma screenshots

The user observed approximately:

- 1500 versus 1350 game-performance scores when the engines had a short budget
- 2250 versus 1850 when the engines had roughly 10 seconds per move
- a previous game-performance score near 2500 when the old engine had around
  five seconds per move against a human

These results correctly demonstrate that additional search time can materially
improve move quality and expose differences between revisions. They do not
establish an absolute engine rating. Game length, position type, opponent
errors, opening familiarity, and the analysis site's estimator all affect a
single-game result.

## What the user must do on the Windows PC

### 1. Install the base tools

Use Windows 11, VS Code, and the native MSVC toolchain. WSL and Ubuntu are not
required.

In PowerShell:

```powershell
winget install -e --id Microsoft.VisualStudioCode
winget install -e --id Git.Git
winget install -e --id Kitware.CMake
winget install -e --id Ninja-build.Ninja
winget install -e --id Python.Python.3.12
winget install -e --id Microsoft.PowerShell
```

Install **Visual Studio Build Tools 2026** and select:

- Desktop development with C++
- current MSVC x64/x86 build tools
- Windows 11 SDK
- C++ CMake tools for Windows

Recommended VS Code extensions:

- C/C++ Extension Pack by Microsoft
- Python by Microsoft

Install a current NVIDIA driver and confirm:

```powershell
nvidia-smi
```

Do not install an arbitrary full CUDA Toolkit yet. Select the CUDA-enabled
PyTorch build compatible with the installed driver when the NNUE environment is
created.

Optional visual customization can use GlazeWM/Zebar later. Do not introduce it
while diagnosing the initial build.

### 2. Clone the correct branch

Suggested layout:

```text
D:\Dev\Chess-Engine
D:\ChessData
D:\EngineMatches
D:\NNUE
```

Clone:

```powershell
New-Item -ItemType Directory -Force D:\Dev
Set-Location D:\Dev
git clone --branch codex/v1-engine-rework `
  https://github.com/tiraaamisuuu/Chess-Engine.git
Set-Location D:\Dev\Chess-Engine
git status --short --branch
git log -1 --oneline --decorate
code .
```

If the repository is already cloned:

```powershell
Set-Location D:\Dev\Chess-Engine
git status --short --branch
git fetch --all --prune --tags
git switch codex/v1-engine-rework
git pull --ff-only
```

Do not run `git reset --hard` to make an existing checkout match the remote.
Inspect and preserve unexpected local files or changes.

### 3. Give the new Codex chat the repository

Create/open the `D:\Dev\Chess-Engine` project in Codex, paste the prompt from
the top of this document, and allow the new chat to run the startup checklist.

The new chat should do the technical verification. The user should not be
expected to diagnose compiler, CMake, Python, UCI, or test failures manually.

## New Codex chat startup checklist

The new Codex agent should complete these steps before feature development.

### A. Inspect before modifying

```powershell
git status --short --branch
git remote -v
git fetch --all --prune --tags
git branch -vv
git log --oneline --decorate -15
```

Read completely:

- `docs/PROJECT_HANDOFF.md`
- `README.md`
- `docs/V1_RESULTS.md`
- `docs/BENCHMARKING.md`
- `docs/NNUE.md`
- `.github/workflows/ci.yml`

Check for unexpected user changes and preserve them.

### B. Build the current headless engine

This path does not require SFML:

```powershell
cmake -S . -B build-pc -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build-pc --config Release --parallel
ctest --test-dir build-pc -C Release --output-on-failure
```

Expected Windows executables:

```text
build-pc\Release\chess-engine-uci.exe
build-pc\Release\chess-engine-tools.exe
build-pc\Release\chess-core-tests.exe
```

Run direct sanity checks:

```powershell
.\build-pc\Release\chess-core-tests.exe
.\build-pc\Release\chess-engine-tools.exe --perft-tests --max-depth 4
.\build-pc\Release\chess-engine-tools.exe --bench --bench-depth 8 `
  --bench-time 4000 --bench-tt 256 --threads 1
```

Record the PC compiler, CPU, executable commit, benchmark output, and test
results in a new dated document under `docs/results/` or another deliberate
tracked location. Do not commit generated binaries or raw large artifacts.

### C. Validate the web interface

Launch:

```powershell
.\scripts\run_web_gui.ps1
```

The script creates `.venv-web`, installs `web/requirements.txt`, builds the
headless engine if needed, starts the local service, and opens the browser.

Manually verify:

1. New Game appears immediately.
2. PvP starts and permits both sides.
3. PvC exposes side, engine, orientation, and time controls.
4. CvC exposes separate white and black engine choices.
5. Dragging makes the selected piece follow the pointer without duplicating it.
6. Flip works.
7. Presets move the slider and custom values snap gently near presets.
8. Analysis returns score/depth/PV after a position exists.
9. PGN, FEN, and JSON export can be copied and downloaded.
10. PGN imports successfully into Chessigma or another PGN parser.
11. Install Stockfish downloads and selects the verified official engine.
12. Stockfish appears clearly labelled in the engine library and selectors.

Run the automated web smoke test against the built engine:

```powershell
py -3 tests\web_api_smoke.py `
  --engine .\build-pc\Release\chess-engine-uci.exe
```

### D. Install and verify Cute Chess

```powershell
.\scripts\install_cutechess.ps1
```

The default `--quick` comparison uses `v0.4.0` as the baseline, and `v0.4.0`
still requires SFML 2.6 to compile. For a first headless workflow check, compare
against the first v1 commit instead:

```powershell
py -3 scripts\compare_engines.py `
  --baseline 4f1bdc8 `
  --candidate HEAD `
  --quick
```

Four quick games only verify builds, UCI communication, openings, and the
tournament runner. They say nothing meaningful about strength. Revision
`4f1bdc8` is used for this smoke test because it is a headless v1 revision with
the current executable names; the earliest v1 headless commits used obsolete
target filenames that the comparison discoverer intentionally ignores.

### E. Report before continuing

The new chat should report:

- active branch and commit
- tool versions
- build result
- core/perft/CTest result
- web smoke result
- manual GUI findings
- Cute Chess quick-check result
- any Windows-only issues
- exact files changed, if fixes were necessary

Only then should it start the next roadmap phase.

## Legacy `v0.4.0` comparison warning

Current v1 builds without SFML. The `v0.4.0` source does not: its UCI protocol
lives inside the SFML `gui.exe`.

To build `v0.4.0` through `compare_engines.py` on Windows:

1. Install SFML 2.6.x, not SFML 3.
2. Ensure the SFML package matches the MSVC architecture/toolchain.
3. Pass its prefix to the comparison script:

```powershell
py -3 scripts\compare_engines.py `
  --baseline v0.4.0 `
  --candidate HEAD `
  --games 100 `
  --tc 10+0.1 `
  --threads 1 `
  --hash 256 `
  --concurrency 4 `
  --sfml-prefix C:\Libraries\SFML-2.6.2
```

The preferred future fix is to make the benchmark tooling accept an existing
legacy UCI binary as a baseline or provide a reproducible verified legacy
package. Do not force all modern development to depend on SFML merely to retain
one historical opponent.

## Benchmarking rules

These rules must be followed for strength claims:

1. Compare committed revisions.
2. Use a balanced opening suite.
3. Play each opening with reversed colours.
4. Give both engines identical clocks, increments, threads, hash, and tablebase
   access.
5. Record engine commit hashes and network checksums.
6. Keep the PC on AC power and disable sleep for long runs.
7. Close games, browsers, launchers, indexing, and other heavy workloads.
8. Keep CPU power mode, cooling, and concurrency stable between tests.
9. Do not combine Mac and PC games in the same statistical result.
10. Treat crashes and time forfeits as bugs, not merely match noise.
11. Preserve PGN and complete logs until the result is reviewed.
12. Report confidence/error intervals, not only the point estimate.
13. Do not promote a patch because it searches more nodes or looks clever.
14. Reject or retest changes with inconclusive evidence.

Suggested PC progression:

### Installation check

```powershell
py -3 scripts\compare_engines.py `
  --baseline 4f1bdc8 --candidate HEAD --quick
```

### Initial legacy sanity match

After solving the SFML baseline dependency:

```powershell
py -3 scripts\compare_engines.py `
  --baseline v0.4.0 `
  --candidate HEAD `
  --games 100 `
  --tc 10+0.1 `
  --threads 1 `
  --hash 256 `
  --concurrency 4 `
  --sfml-prefix C:\Libraries\SFML-2.6.2
```

### Better preliminary estimate

```powershell
py -3 scripts\compare_engines.py `
  --baseline v0.4.0 `
  --candidate HEAD `
  --games 400 `
  --tc 10+0.1 `
  --threads 1 `
  --hash 256 `
  --concurrency 6 `
  --sfml-prefix C:\Libraries\SFML-2.6.2
```

The Ryzen 9 5900X has enough cores for approximately 4–6 one-thread concurrent
games, but the new chat should measure throughput, temperature, and time-loss
behavior rather than blindly maximizing concurrency.

### Release-grade search change

For a candidate expected to gain at least five Elo:

```powershell
py -3 scripts\compare_engines.py `
  --baseline <known-good-commit> `
  --candidate <candidate-commit> `
  --games 10000 `
  --tc 10+0.1 `
  --threads 1 `
  --hash 256 `
  --concurrency 6 `
  --sprt --elo0 0 --elo1 5
```

SPRT may stop before the maximum game count.

## Absolute Elo estimation plan

The repository now measures relative Elo between Git revisions or external
executables and can run a resumable limited-strength Stockfish ladder. It does
not yet have the larger, slower, multi-time-control game sample needed for a
defensible anchored rating. Producing that evidence remains the first major PC
product task.

### Required implementation

Extend the match tooling, preferably without duplicating the paired-game logic,
to support:

- external UCI executable paths as either side
- arbitrary repeated UCI options per engine
- engine names and version metadata
- Stockfish `UCI_LimitStrength`/`UCI_Elo` when exposed
- runtime discovery of supported option bounds instead of hard-coded assumptions
- multiple opponents/rating rungs
- resuming interrupted matches
- a machine-readable result summary
- PGN and complete logs per rung
- a final report with score, relative Elo, uncertainty, crashes, and time losses
- optional round-robin output suitable for Ordo/BayesElo-style analysis

### Calibration procedure

1. Pin one Stockfish binary by version and SHA-256.
2. Query its UCI options and discover the supported limited-strength range.
3. Pick several rungs around the engine's apparent strength.
4. Use the same balanced openings and reversed colours at every rung.
5. Use the same time control and hardware allocation at every rung.
6. Run enough games to identify where the engine scores near 50%.
7. Fit/report the rating estimate with a wide honest interval.
8. Repeat at a second time control to show how sensitive the estimate is.
9. Label the result as a rating in this specific test pool, not universal Elo.
10. Cross-check against at least one independently implemented UCI engine if a
    stronger public claim is desired.

Stockfish's `Skill Level` is useful for gameplay but is not itself a standardized
Elo scale. Prefer its explicit limited-strength controls when available and
still treat them as calibration anchors rather than ground truth.

Do not automate games against public chess accounts as an Elo shortcut. Use
local UCI matches and respect platform rules.

## NNUE plan

The hardware is sufficient for a sensible 256-wide HalfKP network. The RTX 3070
can train the network; the Ryzen 9 is useful for teacher labelling. Teacher
generation is likely to dominate wall-clock time.

### Do not start with a giant training run

Use staged gates:

1. **Pipeline smoke**: a few thousand positions, one epoch, prove CUDA,
   checkpointing, export, C++ load, and inference agreement.
2. **Small experiment**: hundreds of thousands of positions, test loss design,
   validation isolation, throughput, and engine integration.
3. **Medium candidate**: one to five million positions, tune training and run
   classical-vs-NNUE matches.
4. **Serious candidate**: five to twenty million diverse positions only after
   the earlier experiments justify the compute and storage.

### Environment

Create an isolated environment:

```powershell
py -3.12 -m venv .venv-nnue
.\.venv-nnue\Scripts\python.exe -m pip install --upgrade pip
```

Install the CUDA-enabled PyTorch command selected from the official PyTorch
installer for the installed NVIDIA driver, then:

```powershell
.\.venv-nnue\Scripts\python.exe -m pip install `
  -r scripts\nnue\requirements.txt
.\.venv-nnue\Scripts\python.exe -c `
  "import torch; print(torch.__version__); print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'CPU')"
```

Do not commit `.venv-nnue`, datasets, checkpoints, generated networks, or
downloaded engines.

### Dataset requirements

- Only use legally obtained PGNs/positions with recorded provenance and licence.
- Split train and validation by whole games, not random positions from the same
  games.
- Deduplicate positions or at minimum measure duplicates.
- Retain tactical positions, quiet positions, endgames, imbalances, and diverse
  king locations.
- Avoid flooding the dataset with near-identical opening positions.
- Store teacher version, teacher options, nodes/depth, generator commit, seed,
  input checksum, and generation date.
- Generate multiple independent shards.
- Keep a deterministic small fixture for CI.

Current JSONL is readable and convenient but inefficient for tens of millions
of positions. Before serious generation, design a compact indexed/binary shard
format or another streaming representation that avoids repeated FEN parsing and
large text overhead.

### Teacher labelling

Current example:

```powershell
.\.venv-nnue\Scripts\python.exe scripts\nnue\generate_dataset.py `
  --engine C:\Path\To\stockfish.exe `
  --pgn D:\ChessData\games-1.pgn `
  --output D:\ChessData\shard-1.jsonl `
  --nodes 20000 `
  --threads 3 `
  --hash 512 `
  --sample-rate 0.25
```

Run independent shards rather than allocating all CPU threads to one Stockfish
process. Benchmark positions/hour on the 5900X before choosing nodes, threads,
and process count.

### Training and export

Current example:

```powershell
.\.venv-nnue\Scripts\python.exe scripts\nnue\train.py `
  --data D:\ChessData\shard-*.jsonl `
  --validation-data D:\ChessData\validation-*.jsonl `
  --output D:\NNUE\engine-v1.nnue `
  --hidden 256 `
  --batch-size 2048 `
  --epochs 8 `
  --workers 4
```

Improve the trainer before serious use with:

- resumable optimizer/scheduler checkpoints, not only model weights
- periodic checkpoints and best-validation checkpoint selection
- CSV/JSON metrics
- training throughput and GPU-memory reporting
- configurable loss mixture and learning-rate schedule
- early stopping
- checksum/provenance manifest beside every exported network
- deterministic export verification against PyTorch outputs
- explicit quantization-error measurement

### Engine-side NNUE performance work

Before NNUE becomes the default:

1. Add accumulators to board/search state.
2. Incrementally update piece add/remove/move features in make/unmake.
3. Rebuild the relevant perspective when its king moves.
4. Test accumulator output against a full rebuild after random legal playouts
   and complete unmake.
5. Preserve the on-disk HalfKP-v1 feature contract unless evidence requires a
   versioned replacement.
6. Benchmark nodes/second with classical, full-rebuild NNUE, and incremental
   NNUE.
7. Run classical-vs-NNUE paired games.
8. Run network-vs-network paired games with the exact same engine commit.
9. Require an SPRT pass before enabling or shipping a default network.

## Prioritized roadmap

### Phase 0 — Windows bring-up

- Complete the new-chat startup checklist.
- Fix any native Windows build/test issues.
- Add a Windows-native UCI/quality gate instead of depending on Bash.
- Ensure CTest runs equivalent UCI checks on Windows.
- Record a reproducible PC baseline document.
- Verify the web GUI and Stockfish installer end to end.

Exit condition: a fresh Windows clone builds, tests, launches, and runs a quick
paired match with documented commands.

### Phase 1 — Rating and tournament infrastructure

- Remove the SFML bottleneck for historical baseline comparisons.
- Extend the runner to accept external binaries and arbitrary UCI options.
- Implement the calibrated Stockfish ladder.
- Add resumable match manifests and machine-readable summaries.
- Record crashes, illegal moves, disconnects, and time forfeits explicitly.
- Add result parsing and confidence reporting.
- Keep a small smoke suite and a separate serious UHO suite.
- Establish current-vs-`v0.4.0` results on the Ryzen PC at useful time controls.

Windows progress on 2026-07-24: external executables, live UCI option discovery
and bounds validation, deterministic seeds, checksummed manifests, strict JSON
results, explicit failure categories, and a rung-resumable Stockfish ladder are
implemented and smoke-tested. Remaining work includes partial-match game-level
resume, optional round-robin/Ordo output, the historical binary/SFML solution,
and properly powered ladder/baseline evidence.

Exit condition: reproducible relative results plus an honestly qualified rough
rating estimate against a pinned local pool.

### Phase 2 — Classical engine engineering

- Profile the Ryzen release build before changing algorithms.
- Add benchmark result comparison tooling rather than eyeballing logs.
- Deepen correctness coverage where practical, including more perft depths in
  offline/nightly jobs.
- Add evaluation tracing by term for diagnosis and tuning.
- Create a parameterized evaluator suitable for Texel/SPSA-style tuning.
- Improve pawn evaluation and consider a pawn hash only if profiling supports it.
- Review endgame scaling, king safety, passed-pawn races, and drawish positions.
- Test singular extensions, probcut, internal iterative reduction, improved
  pruning, and move ordering one measured patch at a time.
- Revisit root parallelism only with equal-time scaling and Elo data.
- Consider board representation/bitboards only behind perft and match gates;
  do not rewrite everything for fashion.

Every accepted search/evaluation patch needs:

1. correctness tests
2. benchmark telemetry
3. committed candidate
4. paired match against the previous known-good commit
5. documentation of the result

### Phase 3 — NNUE data and training foundation

- Add dataset manifests, licensing/provenance, and checksums.
- Add game-level train/validation splitting.
- Add deduplication and distribution reports.
- Replace serious-scale JSONL with a compact efficient shard format.
- Add parallel/resumable teacher generation.
- Upgrade training checkpointing, metrics, and export validation.
- Run pipeline smoke and small experiments on the RTX 3070.

Exit condition: a repeatable small training run produces a C++-validated network
with documented data and metrics.

### Phase 4 — Incremental NNUE and strength testing

- Implement tested incremental accumulators.
- Measure inference/search throughput.
- Train medium and serious candidates.
- Compare NNUE against classical with paired openings.
- Compare network iterations using the same executable.
- Tune the feature set or architecture only from evidence.

Exit condition: NNUE wins a properly powered match and has acceptable runtime
cost, correctness, and provenance.

### Phase 5 — v1 release

- Resolve all release-blocking correctness issues.
- Pass Linux/macOS/Windows CI and sanitizers.
- Pass web smoke and Windows-native UCI tests.
- Complete a slower, larger baseline match or SPRT.
- Decide whether the first v1 release remains classical or includes a proven
  network.
- Update README, roadmap, results, changelog/release notes, and screenshots.
- Merge the development branch to `main` through a reviewed pull request.
- Tag `v1.0.0` only after the merge and gates.
- Let `.github/workflows/release.yml` build/test/package the tag and create the
  GitHub release.
- Verify every produced ZIP on its target platform.

## Known limitations and technical debt

- No defensible absolute Elo estimate exists yet.
- Current match samples are too small/fast for precise release claims.
- The `v0.4.0` source baseline requires SFML 2.6.
- The Windows CMake test matrix does not currently run the Bash UCI smoke test.
- Root multi-threading is experimental and unproven at equal time.
- Classical evaluation is not comprehensively tuned.
- NNUE rebuilds accumulators at every evaluation.
- NNUE datasets use verbose JSONL and sequential teacher generation.
- NNUE checkpointing does not preserve optimizer/scheduler state.
- There is no trained network bundled with the project.
- The board/search core is header-heavy and tightly compiled.
- Match profiles, external engines, artifacts, data, and networks are local and
  ignored by Git; a fresh clone will not contain them.
- Web state is in-memory; restarting the server does not resume an unfinished
  game.
- External engine import intentionally executes local binaries and therefore
  requires trust.
- The release packages are primarily the headless engine plus web interface;
  the SFML GUI is legacy and should not drive v1 architecture.
- A few hundred games cannot resolve small Elo changes.

## Git and collaboration rules

- Work on `codex/v1-engine-rework` until the v1 pull request is ready.
- Start every session with `git status`.
- Preserve unrelated user changes.
- Use `git pull --ff-only`; do not silently create merge commits from routine
  pulls.
- Make small coherent commits with verification noted in the commit message or
  accompanying results document.
- Push after meaningful tested milestones.
- Do not commit generated directories:

```text
build*/
.tools/
.venv*/
artifacts/
data/
networks/
*.pgn
*.log
*.pt
*.nnue
```

- If a network must be released, attach it as a versioned release asset with a
  checksum/provenance manifest rather than casually adding a large binary to
  normal Git history.
- Do not rewrite published branch history unless the user explicitly requests it.
- Do not merge to `main`, tag, or publish a release merely because a build passes.
  Strength and release evidence are separate gates.

## Useful commands

### Start the Windows web GUI

```powershell
.\scripts\run_web_gui.ps1
```

### Headless Windows build

```powershell
cmake -S . -B build-pc -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build-pc --config Release --parallel
ctest --test-dir build-pc -C Release --output-on-failure
```

### Current macOS/Linux headless build

```sh
cmake -S . -B build -DCHESS_BUILD_GUI=OFF \
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Perft

```powershell
.\build-pc\Release\chess-engine-tools.exe --perft 5
.\build-pc\Release\chess-engine-tools.exe --divide 4
.\build-pc\Release\chess-engine-tools.exe --perft-tests --max-depth 4
```

### Search benchmark

```powershell
.\build-pc\Release\chess-engine-tools.exe --bench `
  --bench-depth 64 --bench-time 1000 --bench-tt 256 --threads 1
```

### Raw UCI check

```powershell
@"
uci
isready
position startpos moves e2e4 e7e5 g1f3
go movetime 1000
quit
"@ | .\build-pc\Release\chess-engine-uci.exe --uci
```

### Build comparison profiles without playing

```powershell
py -3 scripts\compare_engines.py `
  --baseline 4f1bdc8 --candidate HEAD --build-only
```

After that, launch the web GUI and select the generated candidate/baseline
profiles in PvC or CvC.

## Definition of success for the next session

The first Windows session is successful when:

- the correct branch is cloned and current
- MSVC builds the current engine in Release mode
- CTest/core/perft pass
- the web API smoke test passes
- the GUI opens with setup first
- PvP, PvC, CvC, drag, flip, analysis, export, and time controls work
- verified Stockfish installs and appears as a labelled opponent
- Cute Chess installs
- a four-game headless workflow check completes
- results and any Windows issues are recorded
- any necessary fixes are committed and pushed

The next feature milestone is successful when the PC can run resumable,
controlled matches against Git revisions and a pinned external-engine ladder,
producing PGN, logs, relative Elo, uncertainty, and an honestly qualified rough
rating estimate.

That benchmark foundation comes before serious NNUE training. Once it is sound,
the RTX 3070/5900X system is capable of taking the project through dataset
generation, training, quantized export, incremental inference, and controlled
classical-versus-NNUE testing.
