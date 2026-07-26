# Audit: `modules/runtime/tests/System/Runtime/ExceptionServices/ExceptionDispatchInfoTests.cpp`

## Metadata

- AUDITED: 64-line dedicated fixture, fully read.
- Validation: `ExceptionDispatchInfoTests.*` passed 4/4 on 2026-07-27.

## Findings

The fixture verifies ordinary `runtime_error`/`logic_error` capture, rethrow,
source pointer equality, and one cross-thread path. It omits null input, leaving
SR-AUD-155 unguarded.

## Missing assertions and diagnostics

- Missing Capture/Throw null rejection, nested and non-std exception behavior,
  repeated dispatch, and cause/state diagnostics.
- Missing any representable assertion for current .NET's stack-trace helper
  APIs or unthrown Exception instance behavior.

## Final assessment

Useful normal dispatch smoke coverage, but it does not validate the mandatory
source boundary. No source or test was modified.
