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

### Review of the SR-AUD-177 / SR-AUD-178 pairing (#2267, 2026-08-11)

An inherited ranking paired these two findings and recommended verifying the
shared cause before treating them as a family. **The shared-cause hypothesis is
false. They are adjacent, not a family, and must not share an implementation
ticket.**

Both reproducers were verified against the current parser rather than taken from
this report. `TryParse("1E2"/"1E+2"/"1e2", AllowExponent)` all return `false`
with `0`; `Parse("42", static_cast<NumberStyles>(0x8000))` returns `42`; and
`Parse("2A", NumberStyles::HexFloat)` returns hexadecimal `42`.

| | SR-AUD-177 | SR-AUD-178 |
|---|---|---|
| Kind of defect | a grammar production was never written | a precondition check does not exist |
| Immediate cause | an incorrect premise recorded in the file's own scope comment | no counterpart to .NET's `ValidateParseStyleInteger` at any entry point |
| Repair site | the digit/grammar state machine | a new validation call before parsing, at each public overload |
| Direction | **widens** — currently rejected input becomes accepted | **tightens** — currently accepted configuration becomes an exception |
| Authority needed | none; a widening cannot break a caller | an explicit compatibility decision |
| Owner | #2268 (deferred on evidence) | #2269 (`needs_user`) |

They share a file and a `style` parameter. Under this repository's cross-cutting
rules a common file and a common implementation technique do not establish a
common cause, so **no CCF is minted**. They do not interact either: .NET's
`ValidateParseStyleInteger` does not reject `AllowExponent` — the flag is inside
the valid mask — so neither repair implies or blocks the other.

**Premise correction, now fixed in the header.** The parser's scope comment
called `AllowExponent` a "non-gap" because only `NumberStyles.Float`/`.HexFloat`
carry the flag and those "don't apply to integer types". That is refuted by this
port's own header: `NumberStyles::Any` is `0x1FF` and **includes**
`AllowExponent` `0x80` (`NumberStyles.hpp:34`), it is a public style accepted by
every integer overload, and the flag can be passed alone besides. Measured,
`Int32::Parse("1E2", NumberStyles::Any)` throws `FormatException` where .NET
returns `100`. SR-AUD-177 is a real functional gap across all eight wrappers.

Neither finding is remediated by this review; both remain `confirmed`.
SR-AUD-177 is **not** implemented here despite being compatible, because this
file's own standard of evidence cannot currently be met: an exponent grammar has
to agree with .NET on how the exponent folds into `number.scale` and how that
meets `TryNumberBufferToBinaryInteger`'s "significant digits exceed scale"
overflow rule — the rule this file documents as *confirmed against the source,
not guessed* — which is what decides `"1.5E1"`, `"100E-2"` and `"1E-2"`. The
reference source is unavailable, and a partial grammar that handled `"1E2"`
while guessing those would replace a documented, uniform absence with an
undocumented partial divergence. That is #2268, on the #2260/#2252
deferred-verification precedent.

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
