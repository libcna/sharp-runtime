# Audit: `modules/core/tests/System/InterfaceTests3.cpp`

## Metadata

- Audit status: AUDITED (125 lines, 6 tests, fully read).
- Validation: `ISpanParsableTests2.*`, `IUtf8SpanFormattableTests2.*`, and
  `IUtf8SpanParsableTests2.*` passed 6/6 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

This small cross-interface fixture supplies one concrete type for each span
adapter.  It proves that a C++ type can derive from the interfaces, invoke
their required operations, and reach the UTF-8 formatting provider default
through a base reference.  The source uses test-only `std::stoi` and raw
`std::memcpy` implementations; it is not a production parser or formatter.

## Positive findings

- The UTF-8 formatting test asserts the bytes and output count, and its
  provider test proves virtual forwarding through `IUtf8SpanFormattable`.
- String and character-span `ISpanParsable` calls compile together, which is
  direct coverage of the inherited-overload visibility mechanism.

## Other missing assertions and diagnostics

- `Utf8ParsableInt::TryParseUtf8` returns false without resetting a
  pre-populated result, contrary to the local interface comment that failure
  produces a default result.  The sole failure test starts with the default
  zero value and asserts neither output state nor this contract boundary.
- `SpanParsableInt::TryParse` has the same stale-output ambiguity on failure;
  no test passes a non-default result to establish the intended local rule.
- There are no short-destination, empty-destination, UTF-8 malformed-sequence,
  overflow, exception-type, or non-null-provider cases.  The formatter's raw
  `memcpy` is safe only because its test checks `s.size() <= dest` length;
  broader implementations need an explicit buffer contract.

## Final assessment

The fixture is useful structural smoke coverage, but its false-result output
assertions are too weak to document the stated UTF-8 parser contract.  This is
a test-coverage gap, not a separately confirmed production defect; no test was
modified during this audit.
