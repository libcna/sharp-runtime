# Audit: `modules/core/include/System/detail/IntegerNumberStylesParser.hpp`

## Metadata

- AUDITED: 376-line shared integer NumberStyles parsing implementation, fully
  read.
- Validation: `NumberStylesExtendedTests.*` passed 43/43 on 2026-07-27.
- Reference/probe: local current-.NET `Number.Parsing.cs`,
  `NumberFormatInfo.ValidateParseStyleInteger`, and Int32 tests; C++ probe
  prints `exponent_tryparse=0`, `unknown_tryparse=1 result=42`, and
  `hexfloat_tryparse=1 result=42`, while a managed probe prints
  `exponent_parse=100` and `unknown=argument`.

## SR-AUD-177 — medium — shared integer parser ignores valid AllowExponent input that current .NET parses

The parser's own scope comment says `AllowExponent` does not apply to integer
types, and neither signed nor unsigned grammar consumes an exponent.  Current
.NET integer Parse/TryParse accepts the flag: its Int32 tests include `"1E2"`,
`"1E+2"`, and `"1e2"` with result 100, as well as signed/parenthesized and
overflow cases.

The C++ probe's `Int32::TryParse("1E2", AllowExponent, ...)` returns false
and zero, whereas the managed probe parses it as 100.  Because the header is
the shared core for Byte/SByte/Int16/UInt16/Int32/UInt32/Int64/UInt64, the
unsupported valid style is a cross-wrapper functional gap, not a local
documentation choice.

## SR-AUD-178 — medium — parser never validates undefined or incompatible NumberStyles flags before silently choosing a grammar

Current .NET calls `NumberFormatInfo.ValidateParseStyleInteger` at each
integer style overload.  It throws `ArgumentException` for undefined bits and
for `AllowHexSpecifier` or `AllowBinarySpecifier` combined with anything other
than their allowed leading/trailing whitespace flags.  The C++ wrappers select
hex/binary whenever that one bit is present and otherwise ignore all unknown
bits.

The C++ probe accepts `static_cast<NumberStyles>(0x8000)` and parses `"42"`
normally.  It also accepts `NumberStyles::HexFloat` for integer `"2A"` and
returns hexadecimal 42, even though `HexFloat` combines `AllowHexSpecifier`
with sign/decimal/exponent bits that the managed integer validator rejects.
The managed probe throws `ArgumentException` for the unknown bit.  This turns
invalid caller configuration into unrelated successful values or normal parse
errors rather than the required immediate argument failure.

## Assessment

The implemented invariant decimal, group, currency, parentheses, trailing
sign, hex, binary, leading-zero, range, and error-precedence paths are
substantially exercised by the green 43-case fixture.  The explicitly
documented invariant-provider and unsigned-negative exception adaptations are
visible choices.  They do not justify omission of a valid public exponent
style or absence of style validation.

## Other missing assertions and diagnostics

- Add signed/unsigned exponent success, negative exponent overflow, malformed
  exponent, and exponent combined with sign/parentheses/whitespace cases
  across at least one narrow and one wide wrapper.
- Assert `ArgumentException` for every undefined bit and invalid hex/binary
  combination, including HexFloat at integer entry points.
- Provider arguments remain ignored and parsing works on UTF-8 byte strings;
  retain explicit invariant/non-ASCII delimiter cases rather than treating
  host locale classification as managed culture support.

## Final assessment

The shared parser has two confirmed cross-wrapper defects (SR-AUD-177 and
SR-AUD-178).  No source or test was modified.
