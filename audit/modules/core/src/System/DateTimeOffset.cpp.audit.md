# Audit: `modules/core/src/System/DateTimeOffset.cpp`

## Metadata

- Audit status: AUDITED (432 lines, full read).
- Implementation: offset validation, UTC conversion, arithmetic, Unix time,
  parsing, formatting, and equality for `System::DateTimeOffset`.
- Validation: focused Core.Base test filter passed its 34 dedicated tests and
  93 DateTime tests.

## Assessment

Offset construction correctly checks whole minutes, ±14-hour bounds, and UTC
range; arithmetic delegates to checked DateTime methods.  The documented
current-offset approximation for `ToLocalTime` is explicit.  Its parser is
not strict enough for the contract it exposes.

## Findings

### SR-AUD-006 — high — DateTimeOffset component constructors propagate invalid DateTime values

Both component constructors directly delegate to `DateTime(year, month, day,
hour, minute, second[, millisecond])` (lines 76–85).  Since that callee does
not validate time fields, DateTimeOffset shares the invalid-normalization and
out-of-range-state path in SR-AUD-006.  Offset range checking cannot repair a
clock time that was already accepted incorrectly.

**Required post-audit verification:** assert that invalid hour/minute/second/
millisecond inputs throw from both DateTimeOffset component constructors,
including a `9999-12-31 24:00` boundary case.

### SR-AUD-007 — medium — DateTimeOffset parser accepts impossible offset minutes and malformed DateTime text

For a textual offset, lines 268–270 use `%d:%d` but do not constrain the
minute field to 0–59 or require full input consumption.  The constructed
`TimeSpan` therefore normalizes impossible minute values before the
whole-minute/±14-hour constructor validation.  It also delegates its date
portion to DateTime's permissive parser.

**Reproduction:**

```cpp
DateTimeOffset value;
DateTimeOffset::TryParse("2024-06-15T10:30:00+02:75", value);
// currently true, with a +03:15 offset; must be false

DateTimeOffset::TryParse("2024-06-15T10:xx:00+02:00", value);
// currently true, because DateTime turns the failed time parse into midnight
```

`+02:75` is neither a valid ISO-8601 offset nor a valid .NET DateTimeOffset
parse input.  The existing check for offsets beyond ±14 hours does not cover
this normalization path.

**Required post-audit verification:** add false/throw pairs for minute 60/75,
trailing offset garbage, and malformed date-time fields; verify a failed
`TryParse` preserves a sentinel output value.

## Positive findings

The `TryParse` constructor call is caught so an out-of-range but syntactically
valid `+15:00` returns false rather than leaking an exception.  RFC1123 and
universal formats correctly convert to UTC before emitting their UTC marker.

## Final assessment

Construction and conversion are mostly well defended, but parser strictness
and inherited DateTime validation leave confirmed contract failures
(SR-AUD-006 and SR-AUD-007).

---

## SR-AUD-006 — REMEDIATED (ticket #1877, 2026-07-30, CCF-002)

The original evidence above is retained unchanged. Both component constructors
reproduced the inherited defect exactly as described
(`build-probe/1876_current_behaviour.log` cases 021–024, 026) and are fixed by
`DateTime::dateToTicks`'s new component validation with no change to their own
argument handling — see this file's sibling report for the repair itself.

The boundary case the finding asks for is now correct at the right layer:
`DateTimeOffset(9999,12,31,24,0,0,+00:00)` previously threw
`ArgumentOutOfRangeException("offset", "The UTC time represented when the offset
is applied must be between year 0 and 10,000.")` — the right *type* for the wrong
*reason*, caught only accidentally by the UTC-range guard and blaming a parameter
the caller had got right. It now throws the hour/minute/second exception.

**Second, independent defect fixed by the same ticket: validation ORDER
(CCF-002 class B).** Real .NET builds a `DateTimeOffset` from components as

```csharp
_offsetMinutes = ValidateOffset(offset);
_dateTime = ValidateDate(new DateTime(year, month, day, hour, minute, second), offset);
```

and from ticks as `this(ValidateOffset(offset), ValidateDate(new DateTime(ticks),
offset))`. C#'s left-to-right evaluation puts the **offset** check first in both.
This port validated it last — not by choice, but because the clock `DateTime` was
produced in a mem-initialiser, which cannot sequence a free-standing check before
a member's construction. The former combined `validateOffsetAndRange` is now split
into `validateOffset` (whole minutes, ±14 h) and `validateUtcRange`, and the three
constructors obtain their clock value from a `clockOf` factory that runs
`validateOffset` first.

| Call | Was | Now | Reference |
|---|---|---|---|
| `(2024,1,1,24,0,0, +15h)` | AOORE `offset` (only because hour 24 was accepted) | AOORE `offset` — **unchanged** | `ValidateOffset` first |
| `(2024,1,1,24,0,0, 90 ticks)` | `ArgumentException` `offset` | `ArgumentException` `offset` — **unchanged** | `ValidateOffset` first |
| `(2024,13,1,0,0,0, +15h)` | AOORE **`year`** | AOORE **`offset`** | `ValidateOffset` first |
| `(2024,13,1,24,0,0, +00:00)` | AOORE `year` | AOORE `year` — unchanged | date before time |
| `(9999,12,31,23,59,59,999, −14h)` | AOORE `offset` UTC message | unchanged | `ValidateDate` last |
| `(−1 ticks, +15h)` | AOORE `ticks` | AOORE `offset` | `ValidateOffset` first |

Row three and row six are the only observable moves, and both are
`ArgumentOutOfRangeException` before and after. Doing the reorder in the **same**
change as the component validation is what keeps rows one and two unchanged: with
the hour check added and the order left alone, they would have started reporting
the hour instead of the offset.

**Extension of the finding's premise (measured).** The report says the offset
range check "cannot repair a clock time that was already accepted incorrectly".
Correct, and the converse also held and is not recorded: with the order inverted,
an invalid *date* masked an invalid *offset*, so a caller who got both wrong was
told about the one .NET does not report.

SR-AUD-007 remains **confirmed** and is split by
`docs/DateTimeValidationBoundaryPlan.md` §8: its impossible-offset-minute half is
compatible (ticket #1878) and its grammar half is approval-gated (ticket #1879).

Compatibility: **none** beyond the observable exception identity in the two rows
above. No public signature, `noexcept` specification, virtual function, vtable
slot, data member, `sizeof`, `alignof` or member offset changed.

Closure evidence: 8 new permanent regressions in `DateTimeOffsetTests2`
(`Ccf002_*`), covering component rejection in both overloads, extreme integer
hours, offset-before-clock ordering for all three constructors, date-before-time
ordering, the UTC-range guard still running last, and every valid endpoint.
Full component gate **14,508 tests across 37 executables**.
**Mutation-checked:** restoring the pre-repair ordering fails
`Ccf002_OffsetIsValidatedBeforeTheClockDateTime` and
`Ccf002_TickConstructorValidatesTheOffsetFirst`.

---

## SR-AUD-007 — SPLIT: 007a REMEDIATED (ticket #1878, 2026-07-30, CCF-002), 007b OPEN

The original evidence above is retained unchanged. Following the
SR-AUD-035 → #1857/#1858 and SR-AUD-033 → #1864/#1865 convention, this finding
is split by cause, because its two halves have different approval status:

- **007a — impossible offset minutes (compatible, REMEDIATED here).** A range
  check on numeric component values. The accepted textual grammar is unchanged:
  `%d:%d` still matches the same character sequences.
- **007b — parser grammar (approval-gated, OPEN).** Trailing text after a
  complete offset, unpadded fields, and the `DateTime` half's fabricated
  midnight. Ticket **#1879**, `needs_user`, with its exact before/after string
  table in `docs/DateTimeValidationBoundaryPlan.md` §8.2.

**The finding's reproduction is correct and its class is three defects wide, not
one.** `TryParse` read both offset fields with `std::sscanf` and used them
unchecked:

| Input | Was | Now |
|---|---|---|
| `"…+02:75"` | `true`, offset **+03:15** | `false` |
| `"…+02:60"` | `true`, **+03:00** | `false` |
| `"…+02:99"` | `true`, **+03:39** | `false` |
| `"…-02:75"` | `true`, **−03:15** | `false` |
| `"…+02:-30"` | `true`, **+01:30** | `false` |
| `"…+-05:00"` | `true`, **−05:00** — a `+` sign yielding a negative offset | `false` |
| `"…--05:00"` | `true`, **+05:00** — a `-` sign yielding a positive offset | `false` |
| `"…+2147483647:00"` | **`TryParse` THREW `OverflowException`** | `false` |
| `"…+999999999999:00"` | **`TryParse` THREW `OverflowException`** | `false` |
| `"…+15:00"` | `false` (via the ±14 h guard, after the arithmetic) | `false` (before it) |
| `"…+00:00"`, `"…+00:59"`, `"…+02:00"`, `"…-05:30"`, `"…+14:00"`, `"…-14:00"`, `"…Z"` | correct | **identical** |

**Two defects the finding does not record (measured 2026-07-30,
`build-probe/1876_current_behaviour.log` cases 063–064 and
`build-probe/1878_prefix.log`).**

1. **Sign inversion.** The sign character is consumed separately into `neg`, so a
   second one reaches `sscanf` as part of the number. `"+-05:00"` produced a
   *negative* five-hour offset and `"--05:00"` a *positive* one — the parser
   returned the opposite of what the text says.
2. **`TryParse` could throw.** `TimeSpan::FromSeconds` → `IntervalFromDoubleTicks`
   rejects an out-of-`int64` tick count with `OverflowException`, and that call
   sat **outside** `TryParse`'s own `try`/`catch` — the very block whose comment
   states that "TryParse's entire contract is to never throw, only report failure
   via its bool return". `"…+2147483647:00"` escaped as an exception from a
   Try-style method, and `Parse` surfaced `OverflowException` where its
   documentation promises `FormatException`. **No new `SR-AUD-*` identifier is
   issued**: the defect is inseparable from 007a's repair — one
   bounds-before-arithmetic guard closes all three — and the numbering stays
   frozen at 364.

**The repair.** `if (hh < 0 || hh > 14 || mm < 0 || mm > 59) return false;`
before the `TimeSpan` is built. The bounds are the ones .NET already enforces on
the finished offset (whole hours in `[0, 14]`, the sign living in `neg`; minutes
in `[0, 59]`), moved ahead of the arithmetic instead of behind it — the same
bounds-before-operation strategy ticket #1877 applied to `dateToTicks`, recorded
in `docs/DateTimeValidationBoundaryPlan.md` §10.

Compatibility: **none** beyond the observable rejection. No public signature,
`noexcept` specification, virtual function, vtable slot, data member, `sizeof`,
`alignof` or member offset changed; the whole repair is one statement in one
`.cpp` body.

Closure evidence: 6 new permanent regressions in `DateTimeOffsetTests2`
(impossible minutes, sign inversion, never-throws, every valid offset unchanged,
out-of-range hours, and the currently-accepted grammar pinned unchanged).
Full component gate **14,514 tests across 37 executables**, up from 14,508.
**Mutation-checked:** deleting the guard fails 3 permanent tests.
Sanitizers **not applicable** and recorded as such: one integer comparison, no
allocation, pointer arithmetic, lifetime change or shared state — and the probe
`build-probe/1878_offset_field_probe.cpp` shows the previously escaping
`OverflowException` is gone (`build-probe/1878_postfix.log`).

## Post-audit subset remediation — #1929 rows 5–6 (2026-08-01)

The historical SR-AUD-007a repair above is unchanged. Under the exact §6.5
item (3) approval, DateTimeOffset now trims surrounding invariant whitespace
and inherits DateTime's approved one/two-digit clock fields and one-through-seven
digit tick-precision fraction. Offset text is still exactly `Z`/`z` or
`±HH:MM`; `+2:5`, `+2`, `+0205`, inner whitespace, garbage and eight fraction
digits still fail. Both Parse and TryParse are pinned. No declaration, layout,
vtable, exception specification or affected archive symbol name changed; no
audit status or identifier changed.

## Correction and post-audit remediation — #1880 (2026-08-01)

The historical output-preservation verification text remains above, but its
reference premise was wrong. DateTimeOffset::TryParse now assigns
DateTimeOffset::MinValue on every false result, including malformed/large
offsets, inherited DateTime failure and UTC-range constructor failure. Current
.NET constructs the same default clock-plus-zero-offset result on failure, and
the repository's CCF-014 record makes this compatible convention explicit.
Parse exception identity and every successful offset value remain unchanged;
no public/ABI/layout/vtable/`noexcept` consequence. #1880 is complete without a
new audit identifier.
