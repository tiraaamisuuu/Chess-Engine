# Staged move picker

Date: 2026-08-30

## Hypothesis

A staged move picker should improve search quality by trying the most useful
move classes in a deliberate order while avoiding the cost of fully sorting
moves that will never be searched after a cutoff.

## Immutable revisions

- Validated baseline: `bcf262d041e723771e89fd833d1ed78b08743fea`
- Candidate: `d8a456f7f4e4983dfe6393177b7b450aa8691704`

The candidate changes `src/search.hpp` and focused invariants in
`tests/core_tests.cpp`. Recursive alpha-beta search now selects moves in these
stages: transposition-table move, good tactical moves, killer/counter moves,
remaining quiet moves, then bad tactical moves. Root search and quiescence
search remain unchanged.

## Correctness invariants

- The transposition-table move is returned first when legal.
- Winning captures precede quiet moves.
- Killer and counter moves precede ordinary quiet moves.
- Losing captures are deferred until after quiet moves.
- Every legal move is returned exactly once.
- Release build, core tests and UCI smoke test: passed.

## Fixed-depth diagnostics

The picker is not presented as a raw node-count optimisation. On the four-position
diagnostic set it changed the explored tree and principal variation:

- Depth 10: 186,618 baseline nodes versus 272,988 candidate nodes (`+46.3%`).
- Depth 12: 626,297 baseline nodes versus 703,416 candidate nodes (`+12.3%`).
- Nodes per second remained broadly similar.

This makes direct match testing essential: a larger tree can still be stronger
if the new ordering spends effort on tactically useful branches, but no speed
or efficiency gain is claimed from these measurements.

## Short promotion screen

Contract: 200 paired games, `2+0.02`, one thread and 256 MiB hash per engine,
six concurrent games, opening seed 5901.

- Candidate W-D-L: `83-49-68`
- Candidate score: 53.7%
- Relative Elo: `+26.1 +/- 42.1`
- LOS: 88.9%
- Technical failures: zero

The screen is too small for a strength claim, but the positive point estimate
and clean stability record qualify the candidate for confirmation.

## Powered confirmation

A preregistered maximum 5,000-game confirmation is running under
`E:\Dev\Forklift-Research\matches\staged-move-picker-confirmation-20260830`.
Its contract is `10+0.1`, one thread, 256 MiB hash, six concurrent games,
opening seed 5902 and SPRT Elo0 = 0 / Elo1 = 5. The candidate is retained only
if it satisfies the registered decision rule.
