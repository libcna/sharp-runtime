# Audit: `modules/core/src/System/ArgumentOutOfRangeException.cpp`

## Metadata

- Audit status: AUDITED (52-line implementation, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference: local .NET `ArgumentOutOfRangeException.cs` message override and
  HResult assignment were reviewed.

## Assessment

The implementation assigns the distinct `COR_E_ARGUMENTOUTOFRANGE` HResult
on every constructor and correctly uses the base already-composed path so the
actual-value suffix follows the parameter marker.  The compact source leaves
template comparison/formatting behavior in the header; no additional native
memory-safety defect was established here.

## Finding references

- **SR-AUD-091 (context):** the public static guard templates in the paired
  header cannot accept their documented comparison-only types because they
  require `std::to_string`; this source receives only the resulting strings.

## Other missing assertions and diagnostics

- Tests do not compare exact message separators/newlines, empty parameter and
  empty actual-value cases, embedded-NUL text, or multiple exception copies.
- No test validates every constructor's HResult, the `exception_ptr` identity,
  derived catch ordering, or a parameter name containing non-ASCII text.
- Actual values are represented only as strings, unlike the source object's
  typed value.  The adaptation and its loss of typed inspection need explicit
  documentation for a later API-compatibility decision.

## Final assessment

Constructor and suffix ordering behavior are supported by the focused tests;
the generic helper restriction resides in the declaration.  No source or test
was modified during this audit.
