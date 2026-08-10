# Audit: `modules/core/tests/System/StackOverflowExceptionTests.cpp`

## Metadata

- AUDITED: 99-line direct exception fixture, fully read.
- Validation: the focused four-fixture command passed 41/41 on 2026-07-27.
  Ten selected cases originate here; four companion cases with the same suite
  name live in the previously audited `ExceptionRemainingTests.cpp`.
- Related implementation evidence: audited `StackOverflowException.hpp` and
  `.cpp`; local .NET `StackOverflowException.cs` specifies
  `COR_E_STACKOVERFLOW` (`0x800703E9`).

## Assessment

The fixture covers default/custom messages, default HResult, direct and base
catchability, and basic inner-construction behavior.  Those construction-path
observations agree with the reviewed sealed exception implementation.  No new
implementation defect is demonstrated.

## Other missing assertions and diagnostics

- `InnerExceptionCtor_ContainsInnerMessage` asserts only that the outer
  message is nonempty; it does not inspect the inner exception at all.  The
  neighboring inner test likewise omits identity/rethrow and HResult.
- HResult is covered only for the default constructor; C-string, string, and
  inner routes are unasserted.  Null C-string, empty/non-ASCII message, and
  exact default text cases are also absent.
- Constructing this exception is not evidence that a genuine native stack
  overflow is translated correctly.  Such overflow cannot safely be induced
  and recovered in-process as an ordinary C++ exception.

## Final assessment

The fixture is appropriate construction smoke coverage but must not be read
as real stack-overflow translation validation.  No new finding and no source
or test change.
