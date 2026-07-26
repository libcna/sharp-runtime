# Audit: `modules/buffers/include/System/Buffers/Text/Utf8Parser.hpp`

## Metadata

- Audit status: AUDITED (375-line public header-only implementation, fully
  read).
- Validation: `Utf8ParserTest.*` passed 25/25 in `SharpRuntimeTests_Buffers`
  on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-utf8parser-audit-probe.cpp` was compiled
  both normally and with `-fsanitize=undefined` on 2026-07-26.
- Reference: local .NET `Utf8Parser.Boolean.cs`, integer signed/unsigned D/N/X
  sources, `ParserHelpers.cs`, and parser validation sources were reviewed.

## Assessment

The parser has an airtight pre-multiply unsigned overflow check, supports
prefix parsing, and retains the documented bool/integer subset.  The signed
minimum conversion, however, converts a magnitude of `INT64_MAX + 1` back to a
signed minimum then negates it.  This is undefined behavior in C++.  Public
failure paths also preserve caller output instead of implementing .NET `out`
default assignment, and default/D integer grammar omits the allowed leading
plus sign.

## SR-AUD-084 — high — Int64 minimum parsing negates signed minimum and executes undefined behavior

`tryParseInt` accepts a negative magnitude up to `INT64_MAX + 1`, then uses
`out = -static_cast<int64_t>(v)` at line 224.  The grouped `N` branch repeats
the same conversion at line 357.  On the intended minimum input, converting
the magnitude yields `INT64_MIN` on the local compiler and its unary negation
is undefined rather than a defined .NET two's-complement value.

The UBSan probe reports both reachable operations:

```text
Utf8Parser.hpp:224:19: runtime error: negation of -9223372036854775808 cannot be represented in type 'long int'
Utf8Parser.hpp:357:37: runtime error: negation of -9223372036854775808 cannot be represented in type 'long int'
```

The ordinary run returns apparent success for `-9223372036854775808` and for
`-9,223,372,036,854,775,808` with `N`, hiding the undefined behavior.  Current
.NET's integer parser calculates the signed result without a C++ signed
overflow operation.  No direct test covers either minimum input.

## SR-AUD-085 — medium — failed Utf8Parser calls retain stale caller output instead of assigning default

Every public integer overload sets only `bytesConsumed = 0` on a core/range
failure; it does not assign `value`.  The bool overload likewise sets
`bytesConsumed` before returning false but leaves `value` untouched (lines
45–66).  Current .NET uses `out` parameters and explicitly assigns
`value = default` on false in its bool and integer helpers.

The probe initializes an `int32_t` to 42 and a `bool` to true, then parses
`"abc"` and `"no"`.  It prints `0,42,0` and `0,1,0` respectively: false with
the documented zero cursor but stale values.  This invites use of a prior
success despite a checked false result.  `ParseInvalidNotDigit_BytesConsumedIsZero`
even initializes its value to 99 yet asserts only the cursor.

## SR-AUD-086 — medium — default and D integer parsing reject valid leading plus signs

`tryParseUInt` requires its first character to be a decimal digit (lines
198–200), and `tryParseInt` recognizes only `-` before delegating to it (lines
215–218).  Thus the default/G/D integer paths reject `+42` for signed and
unsigned values.  Current .NET's signed and unsigned decimal parser paths
accept an optional leading `+` (as do this header's own `N` parser).

The probe reports `0,0,0` for both `int32_t` and `uint32_t` parsing of `+42`:
false, zero cursor, and unchanged zero output.  The uneven grammar makes a
valid current-.NET decimal token depend on whether the caller selected `N`.

## Other missing assertions and diagnostics

- No test covers `int64_t` minimum/maximum, signed extrema at any width,
  positive sign, unsigned plus, malformed signs, leading zeros, prefix
  boundaries, or false value reset for bool and every integer type.
- Hex coverage omits width overflow, signed minima/bit patterns for 16/32/64,
  zero, uppercase input, empty input, and failure output/cursor state.
- N coverage omits `+`, fractional-without-leading-digit, arbitrary comma
  placement (the reference deliberately permits it), signed minimum, every
  grouping boundary, trailing text, and overflow for signed paths.
- The formatter/parser pair has no generated round-trip test for every type,
  specifier, 0–99 precision, prefix suffix, or failure-output contract.
- Guid, dates, times, floats, and decimal overloads are explicitly absent;
  retain this documented API-baseline gap separately from the demonstrated
  implemented-subset defects.

## Final assessment

The direct filter is green, but two `Int64.MinValue` parse paths have
UBSan-confirmed undefined behavior and the false/plus-sign contracts diverge
from current .NET.  No production or test source was modified during this
audit.
