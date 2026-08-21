# Search

## Iterative deepening and PVS

The engine searches depth 1, 2, 3 and onward until its depth or time limit. Each
completed iteration supplies the likely principal move, score and ordering for
the next one, while preserving a legal move if time expires.

The main recursion is negamax alpha-beta with principal-variation search (PVS):
the first ordered move receives a full window; later moves begin with a null
window and are re-searched only when they improve alpha. Mate-distance bounds,
draw detection and terminal legal-move checks are applied consistently.

## Aspiration windows

After an initial iteration, search opens a narrow alpha-beta window around the
previous score. Fail-low and fail-high results expand and repeat the iteration.
This reduces work when the evaluation is stable while retaining full-window
correctness after a tactical score shift.

## Quiescence

At the nominal leaf, quiescence evaluates the position and explores forcing
captures instead of returning in tactically unstable positions. In-check nodes
generate legal evasions rather than using stand pat.

Current selectivity includes:

- delta pruning when even a plausible captured value cannot raise alpha
- static exchange evaluation (SEE) pruning for sufficiently losing ordinary
  captures
- exemptions for promotions, en passant and in-check situations where the
  simplified material test is unsafe

The retained SEE change reduced the measured depth-10 tree from 270,343 to
244,274 nodes and scored 55% in its 100-game diagnostic.

## Transposition-table use

Probes can return exact values or prove alpha/beta bounds when the stored depth
is sufficient. The stored best move receives top ordering priority even when a
bound cannot end the node. The table also supplies the principal variation for
UCI and developer telemetry.

## Null-move pruning

Eligible non-PV positions with sufficient depth and non-pawn material may pass
the turn and run a reduced null-window search. A high result suggests the real
position is already above beta. Verification safeguards avoid accepting the
cutoff in sensitive deeper cases and zugzwang-prone material.

Null make/undo is incremental. The optimization preserved a 270,343-node tree
and reduced median depth-10 time by about 3.1% on the Windows host.

## Reductions and pruning

The current search includes:

- late-move reductions (LMR) based on depth and move index, with context for
  checks, tactical moves, PV status, improving positions and history quality
- reverse futility pruning at shallow non-PV nodes
- razoring near the horizon
- ordinary move futility pruning
- late-move pruning for unpromising quiets after enough alternatives
- internal iterative deepening/reduction support when a useful TT move is absent

Selective conditions deliberately exempt checks, mates, tactical moves and
other situations where a static margin is unreliable.

ProbCut and singular extensions are roadmap experiments, not current features.
They require targeted TT/tactical tests before adoption.

## Static exchange evaluation

SEE models the least-valuable-attacker capture sequence on a square and returns
the expected material result. It contributes to capture ordering and pruning.
The implementation handles x-ray slider attacks, promotion value, and en
passant's displaced captured pawn.

## Move ordering

The full move list is currently scored and sorted. Priority combines:

1. transposition-table move
2. promotions and captures using tactical value and capture history
3. killer moves
4. countermove response
5. butterfly history
6. continuation history keyed by side, previous destination and current
   destination

History entries use bounded gravity updates so repeated evidence matters without
unbounded growth. Quiet and tactical tried-move lists receive positive or
negative updates after a cutoff.

A simple selection-scan replacement for the full sort was about 17.5% slower
and was rejected. A future staged picker must avoid both full sorting and
repeated rescans before it is likely to help.

## Root parallelism

The optional root split searches the principal move first, shares alpha, and
uses persistent workers plus a shared concurrent TT for the remaining root
moves. It includes tie-handling and short-budget safety regressions discovered
during match testing.

Six workers reach roughly 1.5×–1.7× single-thread benchmark throughput on the
Ryzen 9 5900X, but paired matches have not established a strength gain. One
thread therefore remains the supported default. See
[Experiments and Roadmap](Experiments-and-Roadmap.md) for the measured history.

## Stopping and reporting

The search records nodes, quiescence nodes, depth, score, time, worker count,
best-move changes, aspiration re-searches and a TT-derived PV. UCI `stop` and
hard deadlines are atomic/safe; only fully completed information is promoted as
the stable result.
