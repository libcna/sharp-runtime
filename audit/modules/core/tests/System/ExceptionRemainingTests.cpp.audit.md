# Audit: `modules/core/tests/System/ExceptionRemainingTests.cpp`

## Metadata

- AUDITED: 392-line shared exception fixture, fully read.
- Validation: its selected AppDomainUnloaded, BadImageFormat, DllNotFound,
  DuplicateWaitObject, and EntryPointNotFound cases passed 16/16 within the
  selected 33-test exception-fixture filter on 2026-07-27.

## Findings

`EXCEPT_SIMPLE` gives every listed exception only three broad checks:
non-empty default `what()`, supplied-message containment, and catchability as
`System::Exception`. The selected five derived types additionally have only
an AppDomain outer-message test. None reads a type-specific HResult, so this
shared fixture leaves confirmed SR-AUD-094, SR-AUD-095, and SR-AUD-100 defects
green despite passing normal constructor smoke tests.

## Missing assertions and diagnostics

- The macro does not verify exact default diagnostics, the expected immediate
  base type, HResult, C-string null/UTF-8 behavior, or copy/move semantics.
- The AppDomain inner-exception case checks outer text only; it does not retain
  or rethrow the `std::exception_ptr` cause.
- For its many other exception types, the macro is intentionally a shallow
  common baseline rather than compatibility coverage; later type-specific
  audits must not treat these three tests as constructor-contract validation.

## Final assessment

The shared fixture is useful for gross constructor breakage but is not a
diagnostic compatibility regression suite. No source or test was modified.
