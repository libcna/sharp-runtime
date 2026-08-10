# Audit: `modules/core/tests/System/ExceptionTests.cpp`

## Metadata

- Audit status: AUDITED (685 lines, 114 registered cases across nine suites,
  fully read).
- Validation: all suites declared in this file plus the companion
  `ExceptionNewTests.cpp` suites passed 124/124 in `SharpRuntimeTests_Core_Base`
  on 2026-07-26.
- Scope note: the file intentionally tests many derived exception types; its
  report assesses the test contract, not an assertion that every referenced
  implementation header is already audited.

## Assessment

This broad integration-style fixture supplies valuable inheritance, constructor,
HResult, and ordinary message smoke coverage across the exception hierarchy.
It is heterogeneous and often checks only a substring/nonthrowing call, so its
green result should not be interpreted as full exception-contract validation.

## Finding references

- **SR-AUD-048 (extended):** `ArgumentException_ThrowIfNullOrWhiteSpace` uses
  ASCII spaces only, omitting the demonstrated UTF-8 U+00A0 bypass.
- **SR-AUD-089 / SR-AUD-090:** ArgumentNull coverage calls `what()` without
  content assertions; it misses null C-string construction and the duplicated
  parameter suffix.
- **SR-AUD-091:** the file tests only numeric ArgumentOutOfRange construction,
  not comparison-only/equality-only template instantiations.
- **SR-AUD-092:** `DefaultCtorEmptyMessage` explicitly asserts the empty base
  message that contradicts the .NET fallback diagnostic.

## Other missing assertions and diagnostics

- Many named `InnerException` tests assert only outer message substrings or a
  non-null `exception_ptr`; none rethrows and identifies the exact inner
  exception, checks a nested chain, or asserts lifecycle/copy behavior.
- Source, HelpLink, Data, and HResult use only string/int happy paths.  There
  are no invalid/empty/Unicode/embedded-NUL values, map key/value type policy,
  concurrent mutation, or exception-pointer foreign-type diagnostics.
- Most derived exceptions lack an all-constructor HResult and exact default
  resource assertion.  Catch tests prove only a few inheritance paths and do
  not cover slicing, virtual destruction, or `std::exception` polymorphism for
  every type.
- ObjectDisposed tests merely call `what()` and do not assert object-name
  composition, null/empty object identifiers, ThrowIf argument diagnostics, or
  post-throw inner exception state.

## Final assessment

All 114 cases in this source pass within the 124-test companion filter, but
the fixture contains several weak assertions that preserve confirmed base and
argument-exception defects.  No source or test was modified during this audit.
