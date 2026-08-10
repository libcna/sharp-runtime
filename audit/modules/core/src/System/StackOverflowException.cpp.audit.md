# Audit: `modules/core/src/System/StackOverflowException.cpp`

## Metadata

- Audit status: AUDITED (27-line implementation, fully read with declaration).
- Validation: the focused five-type exception filter passed 29/29 on 2026-07-26.
- Reference basis: local .NET `StackOverflowException.cs` and `COR_E_STACKOVERFLOW` (`0x800703E9`).

## Assessment

Each implemented constructor delegates to `SystemException` then sets the
documented stack-overflow code. The direct fixture verifies the default value
and message/inheritance behavior. No standalone implementation defect was
confirmed.

## Other missing assertions and diagnostics

- No test asserts HResult on non-default constructors, null C-string handling, stored-inner identity/rethrow, or non-ASCII messages.
- The source relies on transitive headers for `std::move`; current builds succeed but no isolated include-self-sufficiency check protects that fact.
- Actual stack-exhaustion translation is not safely covered by this constructor-only suite.

## Final assessment

The implementation establishes the intended normal diagnostic code. No source or test was modified.
