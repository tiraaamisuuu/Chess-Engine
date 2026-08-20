# Development Workflow

This project treats correctness, reproducibility, and coherent history as part of engine performance work.

## Repository layout

```text
src/                 C++ engine, UCI, search, evaluation, and NNUE runtime
tests/               C++ and protocol tests
scripts/             benchmarks, matches, and NNUE tooling
web/                 local analysis interface and HTTP bridge
docs/                design notes, procedures, status, and raw results
wiki/                source of the public GitHub Wiki
data/                 local source archives and generated datasets (ignored)
artifacts/            generated match and benchmark output (ignored)
```

## Change cycle

For each meaningful change:

1. start from a clean, synced branch;
2. make one coherent implementation change;
3. run the smallest relevant correctness checks;
4. run the full test suite before committing;
5. benchmark fixed work where performance can change;
6. run a paired match where move choice can change;
7. preserve the result and update the roadmap; and
8. commit and push the coherent milestone.

Do not combine unrelated search, UI, and training changes in one commit. A negative experiment is still valuable when its configuration and result are recorded clearly.

## Building

The supported local workflow uses CMake and a Release build:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The CMake configuration automatically enables supported compiler optimizations. Keep generated build trees, downloaded corpora, datasets, and match artifacts out of source control.

## Adding search features

Search changes should include a tactical or regression test when possible. Compare node counts on the fixed benchmark before interpreting wall-clock results, and use a paired match for any change that may alter strength.

## Adding an NNUE network

A network is not accepted by copying it into the repository. It must have a recorded dataset and training manifest, pass export verification, meet the runtime budget, and pass the playing-strength promotion gate. The current runtime keeps the classical evaluator available as a safe fallback.

## Distributed labelling

Each worker writes independent shards with provenance and checksums. The merger validates compatible schemas and configurations, prevents duplicate shard identities, and writes a consolidated manifest. Source archives and generated shards are operational data, not repository content.

## Documentation policy

- `README.md` is the concise public landing page.
- This Wiki explains the project by topic.
- `docs/DEVELOPMENT.md` records the current developer state.
- `docs/results/` preserves detailed experimental evidence.

This separation keeps the landing page readable without losing the information required to reproduce a result.
