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

---

## SR-AUD-085 — REMEDIATED (ticket #1872, 2026-07-30, CCF-014)

The original evidence above is retained unchanged.

All **nine** public `TryParse` overloads — `bool` plus the eight integer widths —
now route every non-throwing failure through one shared private helper,
`template<typename T> static bool fail(T& value, intcs& bytesConsumed)`, which
writes `value = T{}` *and* `bytesConsumed = 0` and returns `false`. Ten failure
exits were replaced. .NET writes both outputs at one labelled `FalseExit:` per
parser (`Utf8Parser.Boolean.cs:52-54`,
`Utf8Parser.Integer.Signed.D.cs:79-83` and its width siblings,
`Utf8Parser.Integer.Signed.N.cs:89-92/181-184/276-279/371-374`,
`Utf8Parser.Integer.Unsigned.X.cs:12-13/26-27/77-78/94-95/108-109`).

A single boundary was chosen deliberately: the two halves of this contract
diverged precisely because they were normalised independently at ten sites, so
making them inseparable is the structural repair, not just the arithmetic one.

**Correction / extension of the finding's premise (measured 2026-07-30).** The
finding is correct and its probe reproduces exactly; three things are now
established beyond it.

1. **The defect is grammar-independent.** The finding cites the default decimal
   path. Measured, the `X` grammar (`utf8parser.int32.hex.invalid`) and the `N`
   grammar (`utf8parser.int32.grouped.fraction`) leave the same stale value, and
   both are now covered by permanent tests.
2. **Failure *after* a successful core parse behaves identically.** When
   `tryParseIntegerCore` succeeds and only the target-width range check rejects
   the result — `"256"` into `uint8_t`, `"9999999999"` into `int32_t` — the value
   was equally stale. A repair that reset the output only when the core failed
   would have missed every one of those cases; permanent tests cover all eight
   widths in both directions.
3. **`bytesConsumed` was never the problem, and no partial value was ever
   published.** Every non-throwing failure already set the cursor to 0, and the
   core accumulates into locals (`sv`/`uv`/`n`), copying to `value` only after the
   range check. So of the recurring Try-output failure classes, only "output left
   unchanged where the reference writes a default" applies here.

**The `FormatException` path is parity, not a defect, and was left alone.**
Measured before *and* after: `utf8parser.int32.badformat.throws` reports
`threw=1 value=42 counter=7`, i.e. both outputs still hold the caller's
sentinels. .NET does the same on purpose —
`ParserHelpers.TryParseThrowFormatException<T>` calls `Unsafe.SkipInit(out value)`
and `Unsafe.SkipInit(out bytesConsumed)` specifically to bypass C#'s
definite-assignment rule before throwing (`ParserHelpers.cs:59-70`). Normalising
these would be a divergence. A permanent test pins it, and the shared `fail()`
helper's doc-comment records why it is deliberately not used there.

The class doc-comment previously stated only the `bytesConsumed = 0` half of the
contract and was silent about `value`; it now states both halves and the throwing
exception.

Closure evidence: 7 new permanent regressions in `Utf8ParserTests.cpp` (failed
`bool` parse including a valid-token prefix; not-a-digit across all eight integer
widths; empty source; overflow-after-successful-core across every width in both
directions plus a negative literal into an unsigned type; all four format
grammars; the throwing path asserting **both** sentinels survive; and a success
regression asserting both outputs are still written and only the token consumed).
`Utf8ParserTest` + `SequenceReaderTests` 52/52, `SharpRuntimeTests_Buffers`
536/536, whole-repository build clean with zero errors and zero warnings.
**Mutation-checked:** making `fail()` skip its `value` write — the pre-#1872
behaviour — fails five permanent tests. The direct probe compiled **with**
`-fsanitize=address,undefined` exits 0 with zero AddressSanitizer,
UndefinedBehaviorSanitizer and LeakSanitizer reports, including the 20- and
23-digit overflow inputs that drive the accumulator and the raw `const uint8_t*`
walk to their limits.

Source, ABI and layout consequences: none. `Utf8Parser` has no data members
(`Utf8Parser() = delete`); every entry is a `static` member of a non-template
class and every signature is byte-identical; no `noexcept` specification changed.

The plan for this family is `docs/TryOutputFailureContractPlan.md` (ticket #1871).
