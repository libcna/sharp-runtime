# Audit: `modules/core/tests/System/IUtf8SpanParsableTests.cpp`

## Metadata

- Audit status: AUDITED (85 lines, 7 tests, fully read).
- Validation: `IUtf8SpanParsableTests.*` passed 7/7 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The test fixture executes the representative UTF-8 parser's normal, invalid,
and provider-forwarding paths through both concrete and interface views.  It
also makes failed `TryParseUtf8` assign a default value, which agrees with the
interface documentation.  This is a lightweight adapter test rather than a
production UTF-8 parser: the fixture decodes byte text as a `std::string` and
uses `std::stoi`.

## Positive findings

- The provider default overloads are called through an
  `IUtf8SpanParsable<Utf8Int>` reference, correctly avoiding derived-class
  name hiding and proving the forwarding dispatch.
- The invalid `TryParseUtf8` case asserts both `false` and the zero/default
  result.

## Other missing assertions and diagnostics

- `ParseUtf8_InvalidInput_Throws` permits any `std::exception`, not the
  interface's documented `System::FormatException`; no overflow case verifies
  `System::OverflowException`.
- The fixture covers ASCII invalid input only.  It does not distinguish
  malformed UTF-8, empty byte text, embedded NUL, or a multi-byte code point.
- The result-defaulting test begins from a default object; a pre-populated
  result would prove that a failed parse actually overwrites stale state.

## Final assessment

The forwarding and baseline parser behavior are covered, while error taxonomy
and UTF-8 boundary diagnostics remain unasserted.  No test was modified during
this audit.
