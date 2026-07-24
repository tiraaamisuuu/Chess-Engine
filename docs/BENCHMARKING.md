# Engine and NNUE Match Testing

`scripts/compare_engines.py` builds two committed Git revisions in isolated
directories and runs a paired Cute Chess match. The repository stays on the
current branch throughout the process.

## First-time setup

On macOS or Linux, build the pinned Cute Chess CLI dependency:

```sh
scripts/install_cutechess.sh
```

The source installer needs Qt 6.8 or newer. On Windows PowerShell, the following
downloads and verifies the pinned official binary package instead:

```powershell
scripts\install_cutechess.ps1
py -3 scripts\compare_engines.py --quick
```

The comparison command automatically downloads and verifies the CC0
`UHO_4060_v4` opening suite from the official Stockfish books repository. The
bundled 24-position suite is used only by `--quick` installation checks.

## Compare the latest release with the rework

```sh
scripts/compare_engines.py \
  --baseline v0.4.0 \
  --candidate codex/v1-engine-rework \
  --games 400 \
  --tc 10+0.1 \
  --threads 1 \
  --hash 256
```

Every opening is played with reversed colours. PGN games and the complete
match log are written under `artifacts/elo/`.

Each match directory also contains:

- `manifest.json` with engine paths, Git commits or external selectors,
  executable SHA-256 values, UCI identity/options, configured option values,
  opening and Cute Chess checksums, and match settings
- `result.json` with score, relative Elo and uncertainty when finite,
  termination reasons, crashes, illegal moves, disconnects, time forfeits, and
  process completion status

The reported Elo is a **relative difference**. For example, `+100` means the
candidate performed about 100 Elo above the selected baseline under those test
conditions; it does not assign either engine an absolute human-style rating.

Both refs must be committed because the builder uses `git archive`. Old refs
are handled automatically: before v1, UCI mode lived in the SFML `gui` binary;
new refs use the independent `chess-engine-uci` executable. Set `SFML_PREFIX` or
pass `--sfml-prefix` if SFML 2.6 is not in the normal CMake search path.

After the two binaries are built, they are also registered as local web GUI
profiles. Open **New game → CvC** to watch those revisions play each other, or
select either one as the opponent in PvC. This is useful for visual inspection;
use the paired Cute Chess workflow for meaningful strength measurements.

## Check the installation

```sh
scripts/compare_engines.py --quick
```

This runs four paired games at a short time control. It proves the binaries,
protocol, openings, and tournament runner work; four games say effectively
nothing about Elo.

## Compare external UCI engines

Either side can be an existing executable instead of a Git revision. Repeated
`--candidate-option` and `--baseline-option` values use `NAME=VALUE` syntax and
are validated against the options advertised by the engine's live UCI
handshake. Executable arguments can likewise be repeated with
`--candidate-arg` or `--baseline-arg`; external engines receive no implicit
arguments.

For example, calibrate the current revision against a pinned Stockfish binary:

```powershell
python scripts\compare_engines.py `
  --candidate HEAD `
  --baseline-exe C:\Engines\stockfish-windows-x86-64.exe `
  --baseline-name "Stockfish 18 @ 1800" `
  --baseline-version 18 `
  --baseline-option UCI_LimitStrength=true `
  --baseline-option UCI_Elo=1800 `
  --games 100 --tc 10+0.1 --threads 1 --hash 256
```

The runner discovers and records the engine's supported `UCI_Elo` bounds at
runtime. A limited-strength value is a calibration anchor in this local test
pool, not a universal human rating.

## Serious acceptance testing

For a candidate expected to be at least five Elo stronger:

```sh
scripts/compare_engines.py \
  --baseline v0.4.0 \
  --candidate codex/v1-engine-rework \
  --games 10000 \
  --tc 10+0.1 \
  --sprt --elo0 0 --elo1 5
```

Keep the machine on AC power, close heavy background work, disable sleep, and
use identical thread/hash values. Concurrency changes throughput, but each
engine must still receive the same resources. Do not interpret a few dozen
games as a strength conclusion.

## Compare NNUE networks

An NNUE file is a model; the executable loading it is still the engine. To
isolate the effect of two networks, use the same engine ref on both sides:

```sh
scripts/compare_engines.py \
  --baseline codex/v1-engine-rework \
  --candidate codex/v1-engine-rework \
  --baseline-eval-file networks/network-a.nnue \
  --candidate-eval-file networks/network-b.nnue \
  --games 1000 --tc 10+0.1
```

To compare NNUE against classical evaluation, omit the eval-file option on the
classical side. A network should not become the default until it passes loader
tests and a properly powered paired match.
