# Champion Promotion Gate Foundation

Forklift now has a versioned champion registry and an automated
candidate-versus-champion SPRT gate. This report records the end-to-end workflow
smoke; it is not a playing-strength experiment.

## Frozen champion

- Commit: `071b4b624629ead2326b8ff03cace89b0520aa1f`
- Configuration: classical evaluation, one thread, 256 MiB hash
- Calibration evidence: 4,200 slower-control Stockfish games with a local-pool
  crossing near 2503
- Registry: `research/champion.json`

## Gate behaviour

`scripts/champion_gate.py`:

- resolves champion and candidate to immutable commits;
- refuses a serious self-match or a dirty tracked worktree;
- uses the versioned equal-resource match contract;
- preserves manifests, binaries' checksums, PGN, logs, JSON, and a report;
- distinguishes SPRT `promote`, `reject`, and `inconclusive` outcomes;
- blocks promotion after a technical termination or interrupted match;
- refuses to overwrite interrupted evidence; and
- updates the registry only through an explicit, stale-baseline-checked apply
  step after H1 is accepted.

The underlying match parser was also corrected so a legitimate early SPRT
boundary counts as a completed match even when the maximum game cap was not
reached.

## End-to-end smoke

- Champion: `071b4b624629ead2326b8ff03cace89b0520aa1f`
- Candidate/workflow revision: `80677f3ca7c7869863bb9abf034a8cd472526ddf`
- Contract: four games at `2+0.02`, one thread, 256 MiB hash, concurrency one
- Result: `1-1-2` from the candidate perspective
- Technical failures: zero
- Decision: `smoke_pass`

Four games cannot measure strength. This result proves only that committed refs
were built, the paired tournament completed, strict results were parsed, and
the gate correctly refused to interpret a smoke as promotion evidence.

The external evidence is retained at
`E:\Dev\Forklift-Research\matches\champion-gates\workflow-smoke-20260829`.

## Evidence checksums

- `gate-manifest.json`: `8dc71174841d2bbbce24e967299cc9c4b79f1877bd08a9b48894012f5ab1e9e0`
- `gate-result.json`: `f2c63be5ad664d90e3df5e7eec376eb07960237b673e9a4eb443f66423045055`
- match `manifest.json`: `d0cc44d92fff59dc3586da34a4643d69db272fcd009abda018dbc308a1f66880`
- match `result.json`: `e29a7380c17971fdd02dda8d0388e0ab3acbec4b52bda69db48d39679338cbf2`
- match PGN: `399e0697a8813cc13d45b75c40c6b7c35a34ff6cc73382562a92d02e88d3ca6b`
