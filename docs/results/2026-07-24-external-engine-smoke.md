# External Engine Workflow Smoke — 2026-07-24

This result validates Phase 1 tournament plumbing on the Windows baseline
machine. It is not a strength estimate.

## Configuration

- Candidate: Git revision `52bf420e28`
- Baseline: verified official Stockfish 18 Windows x64 executable
- Baseline executable SHA-256:
  `9bde420202717ce083412027fbfb8c5c935b537591d712be8a8a8bae92f6e8d6`
- Stockfish options discovered by live UCI handshake: 20
- Discovered `UCI_Elo` range: 1320–3190
- Applied baseline options:
  - `Hash=256`
  - `Threads=1`
  - `UCI_LimitStrength=true`
  - `UCI_Elo=1320`
- Games: four, paired openings with reversed colours
- Time control: `2+0.02`
- Concurrency: one

## Outcome

The candidate scored 4–0. There were no reported crashes, illegal moves,
disconnects, or time forfeits. This result only proves that the external
executable, option discovery/validation, paired runner, and artifact pipeline
work together. Four games provide no meaningful Elo evidence.

The ignored match directory contained:

- complete PGN
- complete Cute Chess log
- `manifest.json` with engine identities, exact configured options, executable
  and opening checksums, commits, and match settings
- `result.json` with strict JSON score, termination, failure, and Elo fields

The infinite point estimate and undefined uncertainty from a 4–0 result were
stored as JSON `null` values while retaining the original `inf +/- nan` display
text. This keeps the summary machine-readable without pretending the tiny
sample has finite precision.

The original Git-revision-versus-Git-revision four-game workflow was rerun after
the change and also completed successfully with the new artifacts.
