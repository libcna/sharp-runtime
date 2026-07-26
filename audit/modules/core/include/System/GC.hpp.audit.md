# Audit: `modules/core/include/System/GC.hpp`

## Metadata

- Audit status: AUDITED (482-line inline API, fully read with all included GC
  value types and the dedicated fixture).
- Validation: `GCTests.*` passed 61/61 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.
- Reference basis: local .NET `GC.cs`, `GCMemoryInfo.cs`, and
  `GCNotificationStatus` contracts.

## Assessment

This header deliberately substitutes deterministic C++ RAII for the managed
tracing collector.  Its no-op Collect/finalizer/pressure paths, zero metrics,
and `NotApplicable` full-notification result are documented rather than hidden.
The latter is particularly important: it does not claim that a full collection
was observed when registration itself is unavailable.  Enum values and the
zero-state GCMemoryInfo representation agree with the declared adaptation.

No additional source-level defect is classified because a real GC generation,
managed allocation count, finalization queue, or no-GC region cannot be
implemented through the published native-pointer/vector model.  These are
intentional capability omissions, not a failing partially implemented state.

## Other missing assertions and diagnostics

- The fixture does not call invalid generation/mode/threshold/timeout values,
  null object pointers, empty callbacks, negative memory-pressure arguments,
  or all `GCMemoryInfo` property accessors.
- No caller-visible capability query distinguishes a no-op Collect/pressure
  operation from a real managed collection; callers infer it only from the
  documentation and zero metrics.
- `KeepAlive` only accepts a C++ reference and cannot express managed-object
  liveness/early-finalization behavior.  The test verifies no throw but cannot
  validate the .NET lifetime guarantee.

## Final assessment

All 61 declared-stub tests pass and the RAII boundary is explicit.  No source
or test was modified during this audit.
