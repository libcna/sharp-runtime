# Audit: `modules/core/include/System/IUtf8SpanParsable.hpp`

## Metadata

- Audit status: AUDITED (101-line public template interface, fully read).
- Supporting validation: `IUtf8SpanParsableTests.*` and
  `IUtf8SpanParsableTests2.*` passed 9/9 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The interface explicitly models the static-abstract .NET surface as a const
virtual CRTP adapter.  Its distinct `ParseUtf8` / `TryParseUtf8` spelling
prevents unrelated C++ base-class overload sets from hiding each other.  The
provider overloads safely forward to the required providerless operations and
the `TryParseUtf8` contract remains `noexcept`.  No interface method reads the
span or converts its signed length; concrete parsers own validation.

The documentation says failed `TryParseUtf8` calls produce a default result,
which is stronger than an unspecified C++ out-parameter convention.  That
contract is suitable for the adapter but needs an explicit pre-populated-output
test in every representative implementation.

## Positive findings

- Direct tests exercise valid, invalid, and zero byte-text parsing plus both
  provider-forwarding overloads through an interface reference.
- The representative `Utf8Int` fixture resets its result to `Utf8Int(0)` on
  parsing failure, matching the header's stated failure contract.

## Other missing assertions and diagnostics

- The invalid-input tests contain only ASCII non-numeric text.  They do not
  cover malformed multi-byte UTF-8, empty input, integer overflow, or a
  pre-populated output value on a false result.
- The representative `ParseUtf8` lets `std::stoi` exception types escape and
  its test accepts any `std::exception`; it therefore does not verify the
  documented `System::FormatException` / `System::OverflowException` taxonomy
  for a concrete implementation.
- No diagnostic establishes how malformed public `ReadOnlySpan<uint8_t>`
  metadata must be rejected before a parser constructs a string; consumers
  that make an unchecked signed-to-unsigned conversion remain covered by
  `SR-AUD-043`.

## Final assessment

The interface forwarding and const dispatch are exercised and no declaration
defect is confirmed.  The remaining gaps are concrete-parser assertion and
diagnostic coverage; no source or test was modified during this audit.
