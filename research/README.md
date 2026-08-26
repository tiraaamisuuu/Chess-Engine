# Forklift Research Notebook

This directory preserves compact, reviewable research metadata. Large raw
datasets, models, PGNs and generated experiment directories belong in the
external archive at `E:\Dev\Forklift-Research`, not in Git.

The research programme is intentionally on the backburner. Normal engine work
should collect useful evidence without allowing a future paper to distort the
current engineering priorities.

## Current research signal

The five-million-position corpus reached 92.50% input coverage, but its original
centipawn candidate scored only 32.5% against classical evaluation over 400
games. A later WDL candidate reached 84.12% validation score-sign accuracy but
only 43.8% in a noisy 40-game screen. Together, these results suggest a useful
question: which offline dataset and model metrics actually predict downstream
playing strength?

The working paper direction is:

> How should a compact NNUE evaluator allocate a fixed data-generation and
> teacher-search budget to maximize playing strength on consumer hardware?

This is only a direction. Novelty must be established by a proper literature
review before any publication claim.

## Repository contents

- [`RESEARCH_QUESTIONS.md`](RESEARCH_QUESTIONS.md) records hypotheses and
  possible new methods without treating them as established inventions.
- [`DATA_POLICY.md`](DATA_POLICY.md) defines what is collected and where it
  lives.
- [`data/matches.csv`](data/matches.csv) is the compact match-result registry.
- [`data/nnue_runs.csv`](data/nnue_runs.csv) records comparable NNUE data,
  validation and inference measurements.
- `scripts/generate_research_figures.py` regenerates the checked-in SVG figures.

## Regenerate the figures

```powershell
py -3 scripts\generate_research_figures.py
```

The generated figures are derived summaries. The dated files under
`docs/results/` remain the human-readable evidence, while external manifests,
PGNs and binaries remain the source artifacts.
