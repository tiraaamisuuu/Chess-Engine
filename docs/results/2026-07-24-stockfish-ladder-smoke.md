# Stockfish Ladder Workflow Smoke — 2026-07-24

This Windows run validates the Phase 1 multi-rung calibration workflow. It is
not a rating estimate.

## Workflow exercised

- Candidate ref `HEAD` was resolved once to immutable commit `399b95076c`.
- The verified official Stockfish 18 Windows x64 executable was preflighted.
- Its live `UCI_Elo` range was discovered as 1320–3190.
- Rungs 1320 and 1600 used the same deterministic opening seed, UHO suite,
  `10+0.1` time control, one thread, 256 MiB hash, and one-game concurrency.
- Each rung wrote a separate attempt containing PGN, full match log, driver
  log, manifest, and machine-readable result.
- The aggregate `summary.json` and qualified Markdown report were generated.
- Re-running with the same ladder directory skipped both completed rungs.

The four-game-per-rung scores were:

| Stockfish `UCI_Elo` | Candidate wins | Stockfish wins | Draws | Candidate score |
|---:|---:|---:|---:|---:|
| 1320 | 4 | 0 | 0 | 100% |
| 1600 | 3 | 1 | 0 | 75% |

There were no recorded crashes, illegal moves, disconnects, or time forfeits.
Because both observed scores were above 50%, the aggregator correctly returned
`above_range` with no point estimate and recommended higher rungs. Eight games
are far too few for strength interpretation.

A separate fresh `--quick` run verified that the smoke preset uses four games
at `2+0.02`, completes a rung, and produces the same artifact structure.

Incomplete rung attempts are preserved and retried in a new numbered directory.
The current resume boundary is a complete rung; games from a partial attempt
are not merged into its retry.
