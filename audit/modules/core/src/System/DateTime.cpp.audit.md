# Audit: `modules/core/src/System/DateTime.cpp`

## Metadata

- Audit status: AUDITED (405 lines, full read).
- Implementation: calendar/tick conversion, arithmetic, formatting, and
  invariant numeric parsing for `System::DateTime`.
- Validation: `./build/SharpRuntimeTests_Core_Base --gtest_filter='DateTimeTests.*:DateTimeOffsetTests2.*' --gtest_color=no`
  passed 127 tests; source-level negative cases below are not represented by
  that filter.

## Assessment

Gregorian day calculation, leap-year logic, and overflow-conscious `AddTicks`
and `Subtract(TimeSpan)` are carefully implemented.  Two publicly reachable
validation paths are incomplete, however.

## Findings

### SR-AUD-006 — high — calendar constructors accept invalid time components and can violate the tick invariant

`dateToTicks` validates year, month, and day (lines 43–61), then directly adds
`hour`, `minute`, `second`, and `millisecond` (lines 66–69).  It performs no
range validation for any of those four inputs, despite the public constructor
documentation requiring all four ranges and an exception on violation.

**Reproduction:** each of the following constructor calls should throw
`System::ArgumentOutOfRangeException` but currently produces a normalized (or
out-of-range) tick value:

```cpp
DateTime(2024, 1, 1, 24, 0, 0);       // normalizes to next day
DateTime(2024, 1, 1, 0, 60, 0);       // normalizes to next hour
DateTime(2024, 1, 1, 0, 0, 60);       // normalizes to next minute
DateTime(2024, 1, 1, 0, 0, 0, 1000);  // normalizes to next second
DateTime(9999, 12, 31, 24, 0, 0);     // stores MaxTicks + 1, bypassing
                                       // DateTime(longcs) range validation
```

Negative components can likewise form negative ticks.  The component
constructors initialize `ticks_` directly, so the usual `DateTime(longcs)`
invariant check is never reached.

**Impact:** invalid user input is accepted, calendar semantics diverge from
.NET and the header, and a `DateTime` object can exist outside its documented
`[0, MaxTicks]` invariant.  DateTimeOffset component constructors delegate to
this path (see its implementation report).

**Required post-audit verification:** add four invalid-component assertions
for both signs/boundaries, an upper-bound invariant assertion for
9999-12-31 24:00, and equivalent DateTimeOffset constructor coverage.  Repair
must validate before computing ticks.

### SR-AUD-007 — medium — `TryParse` accepts malformed input and silently replaces invalid time text with midnight

The parser only checks date separator positions and uses permissive `sscanf`
conversions.  If a time separator is present but `%d:%d:%d` does not yield all
three fields, lines 305–306 replace every time field with zero and continue.
It also does not require date or time parsing to consume the full string.

**Reproduction:** these calls should return `false` (and `Parse` should throw
`FormatException`) but the implementation accepts them:

```cpp
DateTime value;
DateTime::TryParse("2024-06-15junk", value);       // accepted as date-only
DateTime::TryParse("2024-06-15 10:xx:00", value); // accepted as midnight
DateTime::TryParse("2024-06-15 trailing", value); // accepted as midnight
```

This is separate from the documented limited grammar: even that grammar does
not describe accepting trailing garbage or fabricating a valid time after a
failed conversion.  `DateTimeOffset::TryParse` inherits this behavior and adds
its own lax offset validation (see its report).

**Required post-audit verification:** add false/throw pairs for malformed
date suffixes and partially malformed time fields, with an assertion that a
failed `TryParse` does not overwrite the output argument.

## Positive findings

`TryParse` catches constructor failures, and the fractional-second digit scan
does not count a trailing timezone marker as a fractional digit.  These are
covered by existing regression tests.

## Final assessment

The arithmetic core is robust, but public construction and parsing validation
contain a high-severity invariant breach (SR-AUD-006) and a medium-severity
false-success parsing defect (SR-AUD-007).

---

## SR-AUD-006 — REMEDIATED (ticket #1877, 2026-07-30, CCF-002)

The original evidence above is retained unchanged. Every one of the five
reproduction calls it lists reproduced exactly as written before the repair
(`build-probe/1876_current_behaviour.log` cases 001–004 and 009) and is rejected
now (`build-probe/1877_postfix_behaviour.log`).

`DateTime::dateToTicks` now validates `hour`/`minute`/`second` with one unsigned
triple-compare and `millisecond` with one unsigned compare, **before** any
arithmetic, in .NET's order (year/month/day → hour/minute/second → millisecond).
The rule, the idiom, the messages and the `paramName`s are copied from
`DateTime.cs:1111-1133`, `ThrowHelper.cs:234-236` and `DateTime.cs:207`, and are
byte-identical to `TimeOnly::validateHms`, which already carried this contract.

| Call | Was | Now |
|---|---|---|
| `DateTime(2024,1,1,24,0,0)` | 2024-01-02 00:00:00 | `ArgumentOutOfRangeException` `paramName=""`, `Hour, Minute, and Second parameters describe an un-representable DateTime.` |
| `DateTime(2024,1,1,0,60,0)` | 2024-01-01 01:00:00 | same |
| `DateTime(2024,1,1,0,0,60)` | 2024-01-01 00:01:00 | same |
| `DateTime(2024,1,1,0,0,0,1000)` | 2024-01-01 00:00:01 | `ArgumentOutOfRangeException` `paramName="millisecond"`, `Valid values are between 0 and 999, inclusive.` |
| `DateTime(9999,12,31,24,0,0)` | ticks `3155378976000000000` = **MaxTicks + 1** | rejected |
| `DateTime(1,1,1,-1,0,0)` | ticks `-36000000000` (**negative**) | rejected |

**Extensions of the finding's premise (measured 2026-07-30).**

1. **The report omits the most severe consequence: undefined behaviour.**
   `hour * TicksPerHour` at this file's tick sum is `(long long)hour * 36000000000`,
   which overflows `int64` for `|hour| > 256204778`. UBSan reported
   `signed integer overflow: 2000000000 * 36000000000 cannot be represented in
   type 'long int'` for `DateTime(2024,1,1,2000000000,0,0)` — which then
   **returned** a `DateTime` with `ticks = -1148579654838206464` — and for the
   negative-hour mirror (`build-probe/1876_ubsan_prefix.log` shapes 1 and 2).
   `hour` is the only operand that can overflow; `INTCS_MAX` times
   `TicksPerMinute`/`TicksPerSecond`/`TicksPerMillisecond` all stay inside
   `int64`. After the repair, all three shapes report zero UBSan errors and throw
   (`build-probe/1877_ubsan_postfix.log`).

2. **The UB is reachable from a public *string* parse, not only from a
   constructor.** `DateTime::TryParse("2024-06-15 2000000000:00:00", out)`
   returned **`true`** with `ticks = -1148436230838206464` after the same
   overflow (shape 3). `TryParse`'s `catch (...)` cannot help — undefined
   behaviour is not an exception. It now returns `false`.

3. **Negative components move the value into a different year.**
   `DateTime(2024,1,1,-1,0,0)` returned 2023-12-31 23:00:00. The report mentions
   negative components can "form negative ticks" but does not record that a
   *representable* negative component silently changes the calendar year.

4. **`DateTimeOffset`'s two component constructors validated the offset LAST,
   not by choice.** They produced the clock `DateTime` in a mem-initialiser,
   which cannot sequence a free-standing check before a member's construction,
   so .NET's `ValidateOffset(offset)`-first order (`DateTimeOffset.cs`) was
   inverted. See that file's report.

5. **The port's year/month/day exception identity already deviates from .NET and
   is NOT part of this finding.** .NET throws `paramName = null` with
   `Year, Month, and Day parameters describe an un-representable DateTime.`; this
   port throws `paramName = "year"` / `"day"` with its own messages. Pre-existing,
   observable by callers, and deliberately left unchanged — recorded as an
   explicit exclusion in `docs/DateTimeValidationBoundaryPlan.md` §16.1.

6. **The report's "Required post-audit verification" text is wrong about .NET in
   one respect.** It asks (in SR-AUD-007's paragraph) for "an assertion that a
   failed `TryParse` does not overwrite the output argument". .NET does the
   opposite: `DateTimeParse.cs:2470` assigns `DateTime.MinValue` before returning
   `false`. The port's preservation is a divergence, not the contract. It is
   recorded, pinned as *current* behaviour, and carried to inactive ticket #1880;
   see `docs/DateTimeValidationBoundaryPlan.md` §7.4.

**Consequence for the parser, which is not a grammar change.** `TryParse` already
funnelled every parsed component through the seven-argument constructor inside
`try`/`catch(...)`, so an out-of-range component *value* now reports failure:
`"2024-06-15 25:00:00"`, `"…24:00:00"`, `"…10:99:00"`, `"…10:20:99"` and
`"…2000000000:00:00"` move from `true` to `false`, and `Parse` throws
`FormatException`. The accepted textual **grammar** is untouched — every
trailing-text and fabricated-midnight case (035–038, 043, 044) behaves exactly as
before and is pinned that way, pending ticket #1879's approval.

Compatibility: **none** beyond the observable rejection. No public signature,
`noexcept` specification, virtual function, vtable slot, data member, `sizeof`,
`alignof` or member offset changed; the whole repair is a private static
function's body plus a mem-initialiser rewrite in a sibling `.cpp`.

Closure evidence: 30 new permanent regressions across `DateTimeTests`,
`DateTimeOffsetTests2` and `TimeOnlyTests`, plus 2 in `CalendarTests`
(the `Calendar::ToDateTime` wrapper path, which inherits the repair with no edit
in `modules/globalization`). Full component gate **14,508 tests across 37
executables**, up from 14,476, zero errors, zero warnings.
**Mutation-checked:** deleting the hour/minute/second check fails 15 permanent
tests, deleting the millisecond check fails 7, and restoring the pre-repair
`DateTimeOffset` ordering fails 2.

Sanitizers: **UBSan required and run** — the family's core defect is
signed-integer overflow. Zero reports after the repair for all three previously
overflowing shapes, one shape per process, `volatile` operands, compiled with
`-fsanitize=undefined` directly against the changed bodies (so no stale
`build-asan` archive is involved; that tree was not touched). ASan/LSan/TSan
**not applicable** and recorded as such: no allocation, ownership transfer,
pointer arithmetic, lifetime change or shared state.

The plan for this family is `docs/DateTimeValidationBoundaryPlan.md` (ticket
#1876). SR-AUD-007 and SR-AUD-009 remain **confirmed**; SR-AUD-007's
offset-minute half is ticket #1878 and its grammar half is the `needs_user`
ticket #1879.

### SR-AUD-007b remediated — ticket #1879 (2026-07-31)

Approved by the batch instruction in the exact words of
`docs/RemainingApprovalDecisions.md` §C.8 item (1). `std::sscanf` was removed
from `DateTime::TryParse`, `DateTimeOffset::TryParse`, `TimeOnly::TryParse` and
`DateOnly::TryParse` and replaced by `System::detail::DateTimeTextScanner` plus a
whole-string consumption requirement. Two defects went with it: the **prefix
acceptance** that made `"2024-06-15junk"` a valid date, and the **zero
substitution** that turned `"2024-06-15 10:xx:00"` and `"2024-06-15 trailing"`
into **midnight** — a wrong answer with no diagnostic, which is the finding's own
headline. Every one of `docs/DateTimeValidationBoundaryPlan.md` §20.1's fifteen
rows now returns `false` and throws `FormatException` from `Parse`; every
documented shape keeps its exact previous value, including the trailing `Z`/`z`
and the `±HH:MM` offset that three `DateTimeTests` cases pin.

**Three premises corrected, all preserved additively in the plan §20.3.** (1)
§20.1's claim that ".NET rejects **every** input in the table" is wrong for two
rows: `ParseFraction` (`Globalization/DateTimeParse.cs:479-492`) accepts
`".1234567"`, and `ParseTimeZone` (`:530-548`) accepts `"+2:5"` and reads it as
2h05m — **125 minutes, exactly what the port already produced**, so §C.4's
"wrong answer that survives round-tripping" was not wrong at all. Both
rejections are deliberate narrowings of this port's fixed-width,
millisecond-resolution subset — the same subset that has always rejected
`"2024-6-15"`, which .NET accepts — and were implemented as approved, with the
widening question filed as inactive ticket **#1929**. (2) Replacing `sscanf`
necessarily also removes `%d`'s leading-whitespace and explicit-sign leniencies,
which §20.1 does not list; structurally the same defect, repaired under the same
approval. (3) §20.1's test matrix names "the four `Ccf002_*GrammarIsPinnedUnchanged`
tests"; **there are two**, plus one unanticipated `DateTimeTests` fraction test
that also had to be inverted.

`TimeOnly`'s unpadded `"1:2:3"` was **deliberately kept**, because .NET accepts it
too — the narrowing is applied exactly where §20.1 asked for it and nowhere else.
+29 permanent tests; ASan and UBSan clean before and after over 82 probe cases
with all four `.cpp` files compiled into the probe. No signature, `noexcept`,
layout, vtable or symbol change. **SR-AUD-007 is now fully `remediated`** (007a
#1878, 007b #1879); SR-AUD-009 and SR-AUD-061 are `remediated` by the same
ticket. CCF-002's remaining member is #1880 (`TryParse` failure output),
inactive. **No new `SR-AUD-*` identifier; numbering stays frozen at 364.**

### Post-audit subset remediation — #1929 rows 5–6 (2026-08-01)

The #1879 record above is preserved. Exact approval under
`docs/TextSubsetCompatibilityDecision.md` §6.5 item (3) now makes the parser
trim only surrounding invariant whitespace, accept one- or two-digit clock
fields, and retain one through seven fractional digits in DateTime's existing
100-nanosecond tick representation. The corrected-premise note in the #1879
section was right that .NET accepts `.1234567`, but wrong to call this type
millisecond-resolution: only the old parser intermediate was. Inner whitespace,
garbage, an eighth digit, unpadded dates and short/compact offsets remain
rejected. SR-AUD-007 remains remediated; #1929 is a partial post-audit ticket,
not a new finding. Evidence and ABI/sanitizer results are in the date-time plan
§22; numbering remains 364.

### Correction and post-audit remediation — #1880 (2026-08-01)

The original "failed `TryParse` does not overwrite" verification requirement
and its earlier correction are preserved above. The corrected premise is now
implemented: DateTime::TryParse assigns `DateTime::MinValue` on every false
return, matching current .NET and the repository's closed CCF-014 convention.
Empty, malformed, range, constructor, suffix, offset and precision failures are
pinned with caller sentinels; Parse retains FormatException `0x80131537` and its
exact message. No declaration, `noexcept`, layout, vtable or symbol name changed.
This post-audit contract repair closes #1880/CCF2-E, not a new SR-AUD finding.
