# Audit: `modules/core/include/System/UnhandledExceptionEventArgs.hpp`

## Metadata

- Audit status: AUDITED (53-line event-data declaration, fully read with both
  direct fixtures).
- Validation: plural and singular UnhandledExceptionEventArgs suites passed
  11/11 in the 33-test related event filter on 2026-07-26.
- Reference basis: local .NET `System/UnhandledExceptionEventArgs.cs:6-20`.

## Findings

The class consistently stores `std::exception_ptr` and the termination flag.
Replacing .NET's arbitrary `object ExceptionObject` with rethrowable C++
exception state is explicitly documented because C++ has no universal
throwable-object base.  AppDomain subscription/dispatch remains a separate
no-op issue already recorded by SR-AUD-103.

## Other missing assertions and diagnostics

- Direct tests cover null and `runtime_error`, but not non-`std::exception`
  throws, pointer identity after copying, exception lifetime, concurrent
  reads, or application termination integration.
- The public constructor accepts null although an actual unhandled event would
  normally have a throwable; no invalid-state diagnostic is defined.

## Final assessment

Payload storage is a clearly documented C++ adaptation with no additional
confirmed implementation defect.  No source or test was modified during this
audit.
