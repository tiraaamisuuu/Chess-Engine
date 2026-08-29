# Continuation-aware late-move reductions

Date: 2026-08-30

## Hypothesis

Continuation history already improves quiet-move ordering. Reusing that same
context when choosing a late-move reduction should search historically strong
replies more fully and spend less work on persistently poor late replies.

## Immutable revisions

- Validated baseline: `7a7badeb6718a868158372bde13c89e48310524d`
- Candidate: `22a055a39c2a48b0c2c0ee688fe557b2ee8ef720`

The candidate changes only `src/search.hpp` and focused core-test invariants.
It preserves the existing reduction when continuation history is neutral,
removes one ply of reduction above `+12000`, and adds one ply below `-12000`
only for sufficiently deep and late quiet moves.

## Correctness and fixed-depth work

- Release build: passed
- Core tests: passed
- UCI smoke test: passed
- Depth-12 benchmark: all four best moves and scores unchanged
- Baseline nodes: 626,297
- Candidate nodes: 636,197 (`+1.58%`)

The additional work is expected: successful continuations are deliberately
searched more fully. Runtime was effectively level in the single sample and is
not treated as a speed claim.

## Short rejection screen

Contract: 200 paired games, `2+0.02`, one thread and 256 MiB hash per engine,
six concurrent games, opening seed 5801.

- Candidate W-D-L: `72-61-67`
- Candidate score: 51.2%
- Relative Elo: `+8.7 +/- 40.3`
- Technical failures: zero

The screen is far too small for a strength claim, but it found no rejection or
stability signal, so the candidate advanced to confirmation.

## Powered confirmation

The preregistered confirmation is running under
`E:\Dev\Forklift-Research\matches\lmr-continuation-confirmation-20260830`.
Its maximum is 5,000 games at `10+0.1`, with SPRT Elo0 = 0 and Elo1 = 5,
opening seed 5802, one thread, 256 MiB hash and six concurrent games. The
candidate is retained only if the registered decision rule supports it.

