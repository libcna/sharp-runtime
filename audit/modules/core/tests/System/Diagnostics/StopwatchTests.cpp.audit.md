# Audit: `modules/core/tests/System/Diagnostics/StopwatchTests.cpp`

## Metadata

- AUDITED: 172-line dedicated fixture, fully read.
- Validation: `StopwatchTests.*` passed 20/20 in `SharpRuntimeTests_Core_Base`
  on 2026-07-26.

## Findings

The fixture gives useful state-transition coverage: stopped/running, idempotent
Start/Stop, reset/restart, elapsed accumulation, TimeSpan correspondence, and
ordinary two-timestamp elapsed calculation. It also asserts `Frequency ==
10,000,000`, locking the public unit mismatch in SR-AUD-130. Its timestamp
case uses only a small positive delta, leaving the UBSan-confirmed SR-AUD-131
overflow path untested.

## Missing assertions and diagnostics

- Missing native-frequency/timestamp-unit comparison, platform condition, and
  external timestamp consumer vectors.
- Missing `INT64_MIN`/`INT64_MAX`, reversed, negative, and near-overflow
  timestamp pairs.
- Missing repeated/cumulative long-run overflow, concurrent access, move/copy,
  and wall-clock-adjustment tests.
- Several assertions use `sleep_for` with only a positive lower bound; there is
  no test diagnostic for scheduling stalls or timer resolution.

## Final assessment

Good ordinary lifecycle coverage, but it preserves both confirmed public
timestamp defects. No source or test was modified during this audit.
