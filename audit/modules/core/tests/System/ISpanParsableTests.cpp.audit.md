# Audit: `modules/core/tests/System/ISpanParsableTests.cpp`

## Metadata

- Audit status: AUDITED (181 lines, 16 tests, fully read).
- Validation: `ISpanParsableTests.*` passed 16/16 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The fixture is a focused structural and behavioral test of the local
`ISpanParsable<IntParser>` adapter.  It checks inheritance, string and span
parsing, invalid input, and virtual dispatch through the derived interface.
The test's span adapter constructs a `std::string` from the pointer and signed
length converted to `size_t`; that is test-fixture code, not an implementation
of the interface.  It demonstrates why future concrete consumers must validate
malformed span metadata before conversion, as already recorded in
`SR-AUD-043`.

## Positive findings

- All inherited and added overloads are exercised, including both interface
  pointer calls that would reveal the original C++ overload-hiding issue.
- Invalid string and span Parse calls are required to expose the local
  `System::FormatException` rather than merely any standard exception.

## Other missing assertions and diagnostics

- Both invalid `TryParse` cases use a default output object and assert only the
  false return.  The helper preserves any pre-existing result on failure, so
  these tests do not state whether the local adapter requires defaulting or
  preservation of a previously populated result.
- Every provider is `nullptr`; no observable provider behavior or deliberate
  provider-ignorance is asserted.
- No empty span, overflow numeric text, embedded-NUL span, or malformed
  negative-length span case is covered.  Negative metadata must be tested only
  with the existing Span safety finding in scope, not through an unbounded
  string construction.

## Final assessment

The test provides useful overload and dispatch coverage but leaves failure
output state and several parser boundaries undocumented.  No test was modified
during this audit.
