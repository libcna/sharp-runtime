# Audit: `modules/core/src/System/TimeOnly.cpp`

## Metadata

- Audit status: AUDITED (123 lines, full read).
- Implementation: millisecond TimeOnly conversion, arithmetic, fixed numeric
  parsing, and custom formatting.
- Validation: the focused TimeOnly/TimeSpan filter passed 134 tests.  An
  isolated linked probe reproduced the malformed input acceptance below.

## Assessment

Modulo-day arithmetic and conversion follow the documented millisecond model.
The parser verifies numeric field ranges but not that its input is fully
consumed or that a decimal point is followed by fractional digits.

## Findings

### SR-AUD-009 — medium — TimeOnly `TryParse` reports success for malformed fixed-format input

`TryParse` uses `%d:%d:%d` to parse a prefix (line 38), then scans at most
three fractional digits but never rejects residual characters (lines 46–54).
It also accepts a decimal point with zero digits.

**Reproduction (observed in a linked probe):**

```cpp
TimeOnly result;
TimeOnly::TryParse("10:20:30.abc", result);
// observed: true; result is 10:20:30.000
```

`"10:20:30junk"`, `"10:20:30."`, and
`"10:20:30.1234"` similarly succeed, although the public header says the
supported grammar is exactly `HH:MM:SS` or `HH:MM:SS.fff`.

**Impact:** malformed user input is silently accepted and can lose a stated
fractional component.  This is another instance of date/time parser
false-success, but it has an independent implementation.

**Required post-audit verification:** add false/throw pairs for every input
above, assert exact three-digit handling for the deliberately limited grammar,
and assert output preservation after failed `TryParse`.

**Remediated (#1879, 2026-07-31, CCF-002 class D).** Approved in the exact words
of `docs/RemainingApprovalDecisions.md` §C.8 item (1). `std::sscanf` was replaced
by the full-consumption `System::detail::DateTimeTextScanner`, so the parser
consumes the whole string or fails, and the `%d` leniencies that came with
`sscanf` (leading whitespace, an explicit sign, an out-of-`int` numeral) are gone
with it. Every well-formed input keeps its exact previous value. Measured over 82
cases in `build-probe/1879_{prefix,postfix}_plain.log`; +29 permanent tests
across the four parsers. No signature, `noexcept`, layout, vtable or symbol
change. Two of the fifteen approved rejections rest on an incorrect .NET premise
and are recorded, not silently reversed, in
`docs/DateTimeValidationBoundaryPlan.md` §20.3.1 (inactive ticket #1929).

## Positive findings

Invalid component constructors, day wrap, and `Add(TimeSpan)` near TimeSpan's
extreme values are directly covered.

## Final assessment

Normal clock arithmetic is sound; parser full-consumption validation needs
remediation (SR-AUD-009).

---

## Correction and post-audit remediation — #1929 rows 5–6 (2026-08-01)

The original report's `millisecond TimeOnly` / `documented millisecond model`
premise is retained above but is incomplete. The public `TimeOnly(long ticks)`
and `Ticks` property promise 100-nanosecond units; measured before this repair,
`TimeOnly(36'002'000'345).Ticks` returned `36'002'000'000`. The existing fourth
`int` truncated rather than represented the input. Exact §6.5 item (3) approval
for seven-digit fractions made fixing that structural path inseparable: the
field now stores ticks within the second, and conversions, comparisons, hash,
subtraction, IsBetween and `f` formatting preserve them. This is completed
inactive post-audit ticket #1931, with no new audit identifier.

TimeOnly Parse/TryParse now also trim surrounding invariant whitespace and
accept one through seven fraction digits. Bare dots, non-digits, an eighth
digit, inner whitespace and trailing garbage remain rejected; the already-valid
one/two-digit fields remain unchanged. `sizeof=16`, `alignof=4`, declarations,
exception specifications, vtables and affected exported symbol names are
unchanged. SR-AUD-009 remains remediated by #1879; #1929 remains partial.
