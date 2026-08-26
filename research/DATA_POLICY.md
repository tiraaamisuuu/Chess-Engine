# Research Data Policy

## Storage tiers

### Git: compact and reviewable

Store experiment definitions, small CSV registries, plotting code, final SVGs,
checksums and written decisions in the repository. Every row must identify a
source document or external experiment directory.

### `E:\Dev\Forklift-Research`: large and durable

Store source archives, generated positions, labels, networks, checkpoints,
PGNs, raw logs and packaged experiment snapshots outside Git. Treat raw inputs
as immutable. Derived data must record the inputs and script version that
created it.

### Scratch space: disposable

Build trees, caches and incomplete downloads may remain in the existing ignored
repository directories. They are not evidence until registered and checksummed.

## Minimum experiment record

Every result intended for research use should preserve:

- experiment ID, hypothesis and predeclared primary metric;
- UTC start/end time and completion state;
- Git commit, dirty-state flag and executable/model hashes;
- CPU, GPU, memory, operating system, compiler and relevant power settings;
- dataset, opening-suite and teacher provenance plus checksums;
- seeds, time control, threads, hash, concurrency and adjudication;
- raw result, uncertainty, failures and the retain/reject decision; and
- whether the result was exploratory, confirmatory or anecdotal.

## Rules against accidental bad science

- Do not tune on a permanent holdout set.
- Do not report the screening match used to select a candidate as independent
  confirmation.
- Do not silently discard failed runs or inconvenient seeds.
- Do not compare runs whose compute budgets differ without saying so.
- Do not convert Chess.com accuracy or a single-game review into engine Elo.
- Do not claim a method is novel until relevant literature has been searched.
