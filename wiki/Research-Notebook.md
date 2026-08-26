# Research Notebook

Forklift is an engineering project first, but its experiments are now preserved
so they can support a serious academic study later. A paper is not a current
roadmap priority and no method is being claimed as novel yet.

## Current signal

![NNUE v1 offline and playing metrics](https://raw.githubusercontent.com/tiraaamisuuu/Forklift/main/docs/assets/nnue-research-baseline.svg)

The five-million-position corpus reached 92.50% HalfKP feature coverage, but
its original centipawn candidate scored only 32.5% against classical evaluation
over 400 paired games. A later WDL candidate reached 84.12% held-out score-sign
accuracy but only 43.8% in a noisy 40-game screen. Together these results create
a concrete question: which offline measurements actually predict engine
playing strength?

## Possible study

The leading direction is a fixed-compute comparison of NNUE data selection,
teacher-search depth, target choice and feature representation. Playing Elo
against a frozen champion would be the primary outcome; validation error,
coverage, inference speed and energy use would explain why candidates behaved
differently.

One provisional idea is **RIFT sampling**—rare-feature and
instability-focused teaching. A cheap pass would rank positions using feature
rarity, score instability, evaluator disagreement, model error and search
importance. Deeper teacher work would then be spent only on the most informative
positions. Random, rarity-only and RIFT sampling would be compared at the same
total teacher-node budget. The name and novelty are explicitly provisional
until a literature review is completed.

## Evidence discipline

- Preserve hypotheses, commits, seeds, hardware, checksums and failed runs.
- Keep screening and independent confirmation data separate.
- Use permanent holdouts and paired matches with uncertainty.
- Never substitute validation accuracy, NPS or one reviewed game for Elo.
- Keep large raw artifacts outside Git and compact registries inside it.

The versioned [research notebook](https://github.com/tiraaamisuuu/Forklift/tree/main/research)
contains the full questions, data policy, CSV baselines and figure generator.
