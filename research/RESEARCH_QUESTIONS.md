# Candidate Research Questions

These are hypotheses to investigate later, not current roadmap commitments or
claims of novelty.

## Primary direction: fixed-budget NNUE training

**Question:** under a fixed Stockfish teacher-node and GPU budget, which mixture
of position selection, label depth, feature representation and objective gives
the strongest compact NNUE?

Suggested controlled factors:

- random versus rare-feature-aware sampling;
- shallow labels for every position versus deeper labels for selected positions;
- dataset size while holding total teacher nodes constant;
- centipawn, WDL and blended targets;
- HalfKP-v1 versus a stronger bucketed or HalfKAv2-like representation; and
- network capacity while holding measured inference cost within a fixed budget.

Primary outcome should be paired-match Elo against the frozen classical
champion. Validation Brier score, MAE, sign accuracy, feature coverage,
inference NPS and energy cost are explanatory outcomes rather than substitutes.

## Provisional method: RIFT sampling

**RIFT** is a working name for *rare-feature and instability-focused teaching*.
Its novelty has not been established.

1. Search a large candidate pool cheaply.
2. Score each position using a combination of rare-feature coverage, evaluation
   instability across depths, disagreement between evaluators, model residual,
   tactical volatility and search importance.
3. Spend deeper teacher searches on the highest-information positions.
4. Compare RIFT with random and rarity-only sampling at the same total teacher
   node budget.
5. Measure both held-out prediction quality and downstream Elo.

The interesting outcome is not merely whether RIFT wins. The experiment can
show which selection signals correlate with actual strength and whether deeper
labels are best spent uniformly or selectively.

## Search-instability dataset

Future opt-in instrumentation could record positions where:

- the principal variation or best move changes between completed depths;
- aspiration windows fail high or low;
- scores swing substantially between iterations;
- a reduced move requires a full-depth re-search;
- null-move assumptions fail verification; or
- classical and NNUE evaluation disagree strongly.

This dataset could support learned time allocation, better sampling, or a small
model that predicts which moves should not be reduced. Any learned pruning or
reduction policy would need strict tactical safety gates and equal-resource
matches before entering the champion.

## Secondary studies

- When does root-parallel NPS translate into equal-wall-time Elo?
- Can search-instability signals improve automatic move-time allocation?
- Which validation slices best predict NNUE match performance?
- How quickly does a teacher become stale as the student search improves?
- Is a policy model valuable for alpha-beta move ordering before full MCTS is
  economically justified?
