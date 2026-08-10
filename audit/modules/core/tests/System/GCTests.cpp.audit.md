# Audit: `modules/core/tests/System/GCTests.cpp`

## Metadata

- Audit status: AUDITED (320 lines, 61 tests, fully read).
- Validation: `GCTests.*` passed 61/61 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.
- Reference basis: the port's explicit RAII/no-tracing-GC design and local .NET
  GC public values/status meanings.

## Assessment

The source covers all declared GC overload families, enum values, core
zero-information metrics, and the important `NotApplicable` notification
status.  It accurately tests the present adapter rather than pretending a
native vector/pointer runtime can observe .NET-managed collections.

## Other missing assertions and diagnostics

- Most calls use only ordinary positive generation, threshold, allocation, and
  timeout inputs; invalid generations/modes, negative pressure/size values,
  null pointers, empty callbacks, and every GCMemoryInfo accessor are absent.
- `RegisterNoGCRegionCallback_DoesNotThrow` verifies the no-op but not the
  public empty-`std::function` policy or a future callback lifetime contract.
- Every metric test expects literal zero.  A future optional allocator/GC
  telemetry implementation needs feature-gated tests rather than rewriting
  these expectations without documenting its capability boundary.

## Final assessment

The complete direct fixture validates the deliberately unavailable GC surface
well enough for the stated adapter.  No test defect was confirmed and no test
was modified during this audit.
