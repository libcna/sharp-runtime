<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Date/time validation boundary — CCF-002 plan

*Authored 2026-07-30 by the autonomous remediation batch on branch
`feature/remediation-batch-ccf002-datetime-validation`, immediately after the
CCF-014 and CCF-016 families closed (#1871–#1874). This is the durable,
evidence-based plan for **CCF-002 — "date/time input validation is weakened
across the DateTime family"** (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`
§CCF-002). Three findings, all `confirmed`: SR-AUD-006 (`DateTime` /
`DateTimeOffset` component constructors), SR-AUD-007 (`DateTime` and
`DateTimeOffset` parsers) and SR-AUD-009 (`TimeOnly` parser). One adjacent
finding outside the family record, SR-AUD-061 (`DateOnly` parser), is shown in
§6 to share SR-AUD-007's and SR-AUD-009's root cause and is planned with them.*

*Every current-behaviour statement in this document was **measured**, not
recalled, by `build-probe/1876_datetime_validation_probe.cpp` compiled against
the shipped production bodies on 2026-07-30 — 84 non-sanitised cases in
`build-probe/1876_current_behaviour.log` plus five one-shape-per-process UBSan
runs in `build-probe/1876_ubsan_prefix.log`. Every reference statement was read
from the current local .NET sources
(`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/DateTime.cs`,
`DateTimeOffset.cs`, `TimeOnly.cs`, `DateOnly.cs`,
`Globalization/DateTimeParse.cs`, `Globalization/ISOWeek.cs`,
`ThrowHelper.cs`, `Resources/Strings.resx`), not from memory. **All three
findings still reproduce; none is remediated as of this document.***

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364) and **marks no finding remediated**.

---

## 1. Exact family scope

**In scope.** The public entry points of `System::DateTime`,
`System::DateTimeOffset`, `System::TimeOnly` and `System::DateOnly` that accept
a **calendar/clock component value or a textual date/time** and either

- (a) perform arithmetic on that value **before** checking its range, or
- (b) accept an out-of-range component and silently **normalise** it into a
  different instant, or
- (c) publish a `DateTime` whose tick value is outside the documented
  `[0, MaxTicks]` invariant.

Measured: **13 public entries** (§3), reached through **6 further wrapper
entries** that perform no validation of their own.

**In scope but already correct, and therefore only *pinned* by tests, never
edited:**

- `System::TimeOnly`'s four component constructors and its tick constructor.
  They already implement exactly the rule `DateTime` is missing, with .NET's
  own message text and `paramName` (§5.4). They are the strongest evidence
  that this is a localised omission in one helper, not a deliberate
  house style.
- `System::DateOnly`'s component constructor, which already delegates to
  `DateTime(year, month, day)`.
- `System::Globalization::ISOWeek::ToDateTime` / `GetWeeksInYear`, whose
  `year` / `week` / `dayOfWeek` guards were compared line-by-line against
  `ISOWeek.cs` and match, including the deliberate acceptance of `dayOfWeek == 7`
  as Sunday.
- `System::TimeSpan`'s component constructors. .NET's `TimeSpan(int, int, int)`
  deliberately accepts `TimeSpan(25, 0, 0)`; there is no range to enforce, and
  its overflow arithmetic was already repaired under CCF-004 (SR-AUD-008).

**Not in scope.** See §17 for the full exclusion list with reasons — most
importantly the `TimeZoneInfo` family (SR-AUD-224…SR-AUD-230), the abstract-
`Calendar` shape finding (SR-AUD-281), `XmlConvert`'s duration grammar
(SR-AUD-354) and `Stopwatch`/`TimeProvider` elapsed-time arithmetic
(SR-AUD-131). None of them shares this family's cause.

---

## 2. Findings and affected symbols

| Finding | Sev | Status | Owner | Entries | One-line defect |
|---|---|---|---|---|---|
| **SR-AUD-006** | high | confirmed | `modules/core/src/System/DateTime.cpp`, `DateTimeOffset.cpp` | 5 ctors | `dateToTicks` validates year/month/day, then multiplies `hour`/`minute`/`second`/`millisecond` into ticks with **no range check at all**. |
| **SR-AUD-007** | medium | confirmed | `modules/core/src/System/DateTime.cpp`, `DateTimeOffset.cpp` | 4 parse entries | Both parsers accept trailing text, fabricate midnight from unparseable time text, and (in `DateTimeOffset`) normalise an impossible offset-minute field. |
| **SR-AUD-009** | medium | confirmed | `modules/core/src/System/TimeOnly.cpp` | 2 parse entries | The fixed-format parser accepts residual characters after the fraction and a bare `.`. |
| **SR-AUD-061** *(adjacent, not listed under CCF-002)* | medium | confirmed | `modules/core/src/System/DateOnly.cpp` | 2 parse entries | `TryParse` accepts arbitrary trailing text after a valid ISO prefix. |

Test targets: `SharpRuntimeTests_Core_Base`
(`modules/core/tests/System/DateTimeTests.cpp`, `DateTimeOffsetTests.cpp`,
`TimeOnlyTests.cpp`, `DateOnlyTimeOnlyTests.cpp`,
`modules/core/tests/DateTimePropertiesTests.cpp`) and
`SharpRuntimeTests_Globalization` (`CalendarTests.cpp`) for the wrapper path.

---

## 3. Complete public-entry inventory

### 3.1 Component-construction entries (SR-AUD-006)

| # | Exact signature | Owner | Validates y/m/d | Validates h/m/s | Validates ms |
|---|---|---|---|---|---|
| C1 | `DateTime::DateTime(intcs year, intcs month, intcs day)` | `DateTime.cpp:84` | yes | n/a | n/a |
| C2 | `DateTime::DateTime(intcs year, intcs month, intcs day, intcs hour, intcs minute, intcs second)` | `DateTime.cpp:87` | yes | **no** | n/a |
| C3 | `DateTime::DateTime(intcs year, intcs month, intcs day, intcs hour, intcs minute, intcs second, intcs millisecond)` | `DateTime.cpp:90` | yes | **no** | **no** |
| C4 | `DateTimeOffset::DateTimeOffset(intcs y, intcs mo, intcs d, intcs h, intcs mi, intcs s, const TimeSpan& offset)` | `DateTimeOffset.cpp:84` | via C2 | **no** | n/a |
| C5 | `DateTimeOffset::DateTimeOffset(intcs y, intcs mo, intcs d, intcs h, intcs mi, intcs s, intcs ms, const TimeSpan& offset)` | `DateTimeOffset.cpp:89` | via C3 | **no** | **no** |

Private helper carrying the whole defect: `DateTime::dateToTicks(int, int, int,
int = 0, int = 0, int = 0, int = 0)` (`DateTime.cpp:43`), the *only* place in
the repository that converts calendar components to ticks.

### 3.2 Wrapper entries that reach C1–C5 and add no validation of their own

| # | Exact signature | Reaches |
|---|---|---|
| W1 | `Globalization::Calendar::ToDateTime(intcs y, intcs mo, intcs d, intcs h, intcs mi, intcs s, intcs ms, intcs era = CurrentEra)` (`virtual`, `Calendar.hpp:593`) | C3 |
| W2 | `Globalization::GregorianCalendar` — inherits W1 unchanged | C3 |
| W3 | `DateOnly::ToDateTime(const TimeOnly&) const` (`DateOnly.cpp:162`) | C3 (components always valid — pinned, not repaired) |
| W4 | `DateTime::AddMonths(intcs)` → `dateToTicks(y, m, d)` | C1 path (date-only, already validated) |
| W5 | `DateTimeOffset::getDateProperty()` → `DateTime(y, mo, d)` | C1 path |
| W6 | `DateTime::TryParse` / `DateTime::Parse` | C3 — see §3.3 |

### 3.3 Parse entries (SR-AUD-007, SR-AUD-009, SR-AUD-061)

| # | Exact signature | Owner |
|---|---|---|
| P1 | `static bool DateTime::TryParse(const std::string& s, DateTime& result)` | `DateTime.cpp:350` |
| P2 | `static DateTime DateTime::Parse(const std::string& s)` | `DateTime.cpp:384` — delegates to P1 |
| P3 | `static bool DateTimeOffset::TryParse(const std::string& s, DateTimeOffset& result)` | `DateTimeOffset.cpp:279` — delegates to P1 for the date/time half |
| P4 | `static DateTimeOffset DateTimeOffset::Parse(const std::string& s)` | `DateTimeOffset.cpp:325` — delegates to P3 |
| P5 | `static bool TimeOnly::TryParse(const std::string& s, TimeOnly& result)` | `TimeOnly.cpp:43` |
| P6 | `static TimeOnly TimeOnly::Parse(const std::string& s)` | `TimeOnly.cpp:66` — delegates to P5 |
| P7 | `static bool DateOnly::TryParse(const std::string& s, DateOnly& result)` | `DateOnly.cpp:168` |
| P8 | `static DateOnly DateOnly::Parse(const std::string& s)` | `DateOnly.cpp:183` — delegates to P7 |

---

## 4. Current behaviour matrix (measured 2026-07-30)

Case numbers are `build-probe/1876_current_behaviour.log` line numbers.

### 4.1 SR-AUD-006 — no time-component validation exists at all

| Case | Call | Current result |
|---|---|---|
| 001 | `DateTime(2024,1,1,24,0,0)` | **OK** — 2024-01-02 00:00:00 (normalised into the next day) |
| 002 | `DateTime(2024,1,1,0,60,0)` | **OK** — 2024-01-01 01:00:00 |
| 003 | `DateTime(2024,1,1,0,0,60)` | **OK** — 2024-01-01 00:01:00 |
| 004 | `DateTime(2024,1,1,0,0,0,1000)` | **OK** — 2024-01-01 00:00:01 |
| 005 | `DateTime(2024,1,1,-1,0,0)` | **OK** — 2023-12-31 23:00:00 (**previous year**) |
| 006 | `DateTime(2024,1,1,0,-1,0)` | **OK** — 2023-12-31 23:59:00 |
| 007 | `DateTime(2024,1,1,0,0,-1)` | **OK** — 2023-12-31 23:59:59 |
| 008 | `DateTime(2024,1,1,0,0,0,-1)` | **OK** — 2023-12-31 23:59:59.999 |
| 020 | `DateTime(2024,1,1,24,60,60,1000)` | **OK** — all four wrong at once, 2024-01-02 01:01:01 |

### 4.2 SR-AUD-006 — the tick invariant is breached from a public constructor

`DateTime.hpp` documents `[0, MaxTicks]` and `DateTime(longcs)` enforces it. The
component constructors bypass that check entirely.

| Case | Call | `ticks_` produced | Invariant |
|---|---|---|---|
| 009 | `DateTime(9999,12,31,24,0,0)` | `3155378976000000000` | **> MaxTicks** (`3155378975999999999`) |
| 010 | `DateTime(9999,12,31,23,59,59,1000)` | `3155378976000000000` | **> MaxTicks** |
| 011 | `DateTime(1,1,1,-1,0,0)` | `-36000000000` | **< 0** |
| 012 | `DateTime(1,1,1,0,0,0,-1)` | `-10000` | **< 0** |

Consequence beyond the value itself: every decomposition property
(`getYearProperty` … `getDayOfWeekProperty`) routes through
`DateTime::toTm()` → `gmtime_r`, which is specified only for representable
`time_t` values; case 009 prints "10000-01-01", a year `DateTime` claims cannot
exist.

### 4.3 SR-AUD-006 — **undefined arithmetic** reachable from a public entry

`DateTime.cpp:65` computes `hour * TicksPerHour`, i.e.
`(long long)hour * 36000000000`, *before* any check of `hour`. That overflows
`int64` for `|hour| > 256'204'778` — an ordinary `int` argument, and an
ordinary parsed string, can supply it. One shape per process, `-fsanitize=undefined`
(`build-probe/1876_ubsan_prefix.log`):

| Shape | Entry | UBSan | Result returned |
|---|---|---|---|
| 1 | `DateTime(2024,1,1,2000000000,0,0)` | `DateTime.cpp:65:26: runtime error: signed integer overflow: 2000000000 * 36000000000 cannot be represented in type 'long int'` | **OK**, `ticks = -1148579654838206464` (**negative**) |
| 2 | `DateTime(2024,1,1,-2000000000,0,0)` | same site, `-2000000000 * 36000000000` | **OK**, `ticks = 2425372934838206464` (a plausible-looking year 7686) |
| 3 | `DateTime::TryParse("2024-06-15 2000000000:00:00", out)` | same site | **returns `true`**, `ticks = -1148436230838206464` |

Shape 3 is the family's most severe measured fact: **a public string parse
reaches undefined signed-integer arithmetic and then reports success with a
negative tick count.** `TryParse`'s `try`/`catch(...)` cannot help — undefined
behaviour is not an exception.

`minute`, `second` and `millisecond` cannot overflow their own multiplication
(`INTCS_MAX * TicksPerMinute` = 1.29e18 < `INT64_MAX`), and the measured sum
`days * TicksPerDay + …` stays representable for them, so **`hour` is the only
UB operand**. Shapes 4 and 5 (`"99999999999999:00:00"`,
`"99999999999999-06-15"`) produced **no** UBSan report: glibc's `%d` conversion
saturates rather than misbehaving on this platform, and both inputs are rejected
before any arithmetic. Recorded as measured-clean rather than assumed.

### 4.4 SR-AUD-006 — validation order actually in force

| Case | Call | Current | Note |
|---|---|---|---|
| 018 | `DateTime(2024,13,1,24,0,0)` | throws AOORE `param='year'` | date checked before time — matches .NET |
| 019 | `DateTime(0,1,1,24,0,0)` | throws AOORE `param='year'` | ditto |
| 016 | `DateTime(2023,2,29)` | throws AOORE `param='day'` msg `DateTime: day out of range for given month` | leap rule correct |
| 017 | `DateTime(2024,4,31)` | throws AOORE `param='day'` | month-length rule correct |
| 027 | `DateTimeOffset(2024,1,1,24,0,0, +15h)` | throws AOORE `param='offset'` "Offset must be within plus or minus 14 hours." | **offset checked *after* the DateTime** — only reaches the offset check because hour 24 is currently accepted |
| 028 | `DateTimeOffset(2024,1,1,24,0,0, 90 ticks)` | throws `ArgumentException` `param='offset'` "Offset must be specified in whole minutes." | ditto |
| 029 | `DateTimeOffset(2024,13,1,0,0,0, +15h)` | throws AOORE `param='year'` | **date wins over offset — .NET is the other way round** |

### 4.5 SR-AUD-006 — `DateTimeOffset` component constructors

| Case | Call | Current |
|---|---|---|
| 021 | `DateTimeOffset(2024,1,1,24,0,0,+00)` | **OK** — 2024-01-02 |
| 022–024 | minute 60 / second 60 / millisecond 1000 | **OK**, all normalised |
| 025 | `DateTimeOffset(9999,12,31,24,0,0,+00)` | throws AOORE `param='offset'` "The UTC time represented when the offset is applied…" — caught only *accidentally*, by the UTC-range guard, and reported against the wrong parameter |
| 026 | `DateTimeOffset(2024,1,1,25,0,0,+02)` | **OK** — 2024-01-02 01:00 +02:00 |

### 4.6 SR-AUD-007 — `DateTime` parser

| Case | Input | Current | Class |
|---|---|---|---|
| 035 | `"2024-06-15junk"` | `true`, 2024-06-15 | grammar (trailing text) |
| 036 | `"2024-06-15 10:xx:00"` | `true`, **midnight** | grammar (fabricated time) |
| 037 | `"2024-06-15 trailing"` | `true`, midnight | grammar |
| 038 | `"2024-06-15T10:20:30zzzz"` | `true`, 10:20:30 | grammar |
| 043 | `"2024-06-15T10:20:30."` | `true`, 10:20:30.000 | grammar (bare `.`) |
| 044 | `"2024-06-15T10:20:30.abc"` | `true`, 10:20:30.000 | grammar |
| 039 | `"2024-06-15 25:00:00"` | `true`, **2024-06-16 01:00** | **component range** |
| 040 | `"2024-06-15 10:99:00"` | `true`, 11:39 | **component range** |
| 041 | `"2024-06-15 10:20:99"` | `true`, 10:21:39 | **component range** |
| 042 | `"2024-06-15 24:00:00"` | `true`, 2024-06-16 00:00 | **component range** |
| 045–047 | `"2024-13-15"`, `"2024-02-30"`, `""` | `false`, **caller's output left untouched** | Try-output contract, §16.2 |
| 048/049 | `Parse("2024-06-15junk")`, `Parse("2024-06-15 25:00:00")` | both **succeed** | inherited from P1 |

### 4.7 SR-AUD-007 — `DateTimeOffset` offset field

| Case | Input | Current offset | Class |
|---|---|---|---|
| 053 | `"…+02:75"` | **+195 min (+03:15)** | **component range** |
| 054 | `"…+02:60"` | +180 min (+03:00) | **component range** |
| 055 | `"…+02:99"` | +219 min | **component range** |
| 056 | `"…-02:75"` | −195 min | **component range** |
| 057 | `"…+02:-30"` | **+90 min (+01:30)** | **component range** (a negative minute field) |
| 058 | `"2024-06-15T10:xx:00+02:00"` | `true`, **midnight** +02:00 | grammar, inherited from P1 |
| 060 | `"…+02:00junk"` | `true`, +120 | grammar |
| 061 | `"…+2:5"` | `true`, **+125 min** | grammar (unpadded fields) |
| 062 | `"2024-06-15T25:00:00+02:00"` | `true`, next day 01:00 | **component range**, inherited from P1 |
| 059 | `"…+15:00"` | `false`, output preserved | already correct |

### 4.8 SR-AUD-009 / SR-AUD-061 — `TimeOnly` and `DateOnly` parsers

| Case | Input | Current | Class |
|---|---|---|---|
| 065 | `TimeOnly "10:20:30.abc"` | `true`, 10:20:30.000 | grammar |
| 066 | `TimeOnly "10:20:30junk"` | `true`, 10:20:30.000 | grammar |
| 067 | `TimeOnly "10:20:30."` | `true`, 10:20:30.000 | grammar |
| 068 | `TimeOnly "10:20:30.1234"` | `true`, 10:20:30.123 | grammar (4th digit dropped) |
| 069 | `TimeOnly "10:20:30.1"` | `true`, 10:20:30.100 | correct scaling — pin, do not change |
| 070–072 | `"24:00:00"`, `"10:60:00"`, `"-1:00:00"` | `false` | **already correct** — `TimeOnly`'s ctor rejects them |
| 076 | `DateOnly "2024-06-15junk"` | `true`, 2024-06-15 | grammar |
| 077 | `DateOnly "2024-06-15 10:20:30"` | `true`, 2024-06-15 | grammar |

### 4.9 Entries measured and found already correct (pin only)

| Case | Call | Current |
|---|---|---|
| 013 | `DateTime(1,1,1,0,0,0,0)` | ticks 0 |
| 014 | `DateTime(9999,12,31,23,59,59,999)` | ticks 3155378975999990000 |
| 015 | `DateTime(2024,2,29,23,59,59,999)` | leap day accepted |
| 030 | `DateTimeOffset(2024,6,15,10,30,0,999,+02)` | correct |
| 080 | `DateOnly(2024,1,1).ToDateTime(TimeOnly(10,20,30,400))` | correct |
| 081/082/084 | `TimeOnly(24,0,0)` / `(0,60,0)` / `(-1,0,0)` | AOORE `param=''` msg **"Hour, Minute, and Second parameters describe an un-representable DateTime."** |
| 083 | `TimeOnly(0,0,0,1000)` | AOORE `param='millisecond'` msg **"Valid values are between 0 and 999, inclusive."** |

Cases 081–084 are the template for the repair: **the exact .NET strings and
`paramName`s already exist in this repository**, in `TimeOnly.hpp`'s
`validateHms`.

---

## 5. Reference behaviour matrix (.NET, read 2026-07-30)

### 5.1 `DateTime` component constructors

`DateTime.cs:314-325` — `DateTime(int y, int mo, int d, int h, int mi, int s)`:

```csharp
ulong ticks = DateToTicks(year, month, day);          // 1. date first
_dateData = ticks + TimeToTicks(hour, minute, second); // 2. then time
```

`DateTime.cs:1111-1120` — `TimeToTicks(int hour, int minute, int second)`:

```csharp
if ((uint)hour >= 24 || (uint)minute >= 60 || (uint)second >= 60)
    ThrowHelper.ThrowArgumentOutOfRange_BadHourMinuteSecond();
int totalSeconds = hour * 3600 + minute * 60 + second;
return (uint)totalSeconds * (ulong)TimeSpan.TicksPerSecond;
```

`DateTime.cs:422-427` — the millisecond constructor delegates to the six-argument
one and *then* checks:

```csharp
if ((uint)millisecond >= TimeSpan.MillisecondsPerSecond) ThrowMillisecondOutOfRange();
_dateData += (uint)millisecond * (uint)TimeSpan.TicksPerMillisecond;
```

Binding consequences:

1. **Validation order is date → time → millisecond**, and the check is a single
   **unsigned** compare, so a negative component is rejected by the same test.
2. The multiplication happens **after** the check, on a value already known to be
   in `[0, 23]` / `[0, 59]` / `[0, 999]`, so it can never overflow. .NET states
   this explicitly: `Debug.Assert(ticks <= MaxTicks, "Input parameters validated already")`
   (`DateTime.cs:1129`).
3. Therefore **no separate `MaxTicks` check is needed after component
   validation** — it is implied. This is the structural repair, not a bolt-on
   range test.

### 5.2 Exact exception identity

| Condition | Type | `paramName` | Message (`Strings.resx`) |
|---|---|---|---|
| year/month/day invalid | `ArgumentOutOfRangeException` | **`null`** (`ThrowHelper.cs:228-230`) | `Year, Month, and Day parameters describe an un-representable DateTime.` |
| hour/minute/second invalid | `ArgumentOutOfRangeException` | **`null`** (`ThrowHelper.cs:234-236`) | `Hour, Minute, and Second parameters describe an un-representable DateTime.` |
| millisecond invalid | `ArgumentOutOfRangeException` | **`"millisecond"`** (`DateTime.cs:207`) | `Valid values are between 0 and 999, inclusive.` |
| ticks invalid | `ArgumentOutOfRangeException` | `"ticks"` (`DateTime.cs:205`) | `Ticks must be between DateTime.MinValue.Ticks and DateTime.MaxValue.Ticks.` |
| offset not whole minutes | `ArgumentException` | `"offset"` | `Offset must be specified in whole minutes.` |
| offset beyond ±14h | `ArgumentOutOfRangeException` | `"offset"` | `Offset must be within plus or minus 14 hours.` |
| UTC instant unrepresentable | `ArgumentOutOfRangeException` | `"offset"` (`DateTimeOffset.cs` `ValidateDate`) | `The UTC time represented when the offset is applied must be between year 0 and 10,000.` |

The port already matches rows 4–7 byte-for-byte (case 025/027/028 and
`DateTime.cpp:81`). It matches row 3 in `TimeOnly` (case 083) and row 2 in
`TimeOnly` (cases 081/082/084). It **deviates on row 1**: the port throws
`paramName = "year"` with `"DateTime: date component out of range"` and a
separate `paramName = "day"` / `"DateTime: day out of range for given month"`.
See §7.3 — that deviation is **pre-existing, out of scope, and deliberately not
touched here.**

### 5.3 `DateTimeOffset` component constructors

`DateTimeOffset.cs`:

```csharp
public DateTimeOffset(int year, int month, int day, int hour, int minute, int second, TimeSpan offset)
{
    _offsetMinutes = ValidateOffset(offset);                                    // 1. offset FIRST
    _dateTime = ValidateDate(new DateTime(year, month, day, hour, minute, second), offset); // 2. then y/m/d, then h/m/s, then UTC range
}
public DateTimeOffset(…, int millisecond, TimeSpan offset) : this(…, second, offset)
{
    if ((uint)millisecond >= TimeSpan.MillisecondsPerSecond) DateTime.ThrowMillisecondOutOfRange();
    _dateTime = DateTime.CreateUnchecked(UtcTicks + (uint)millisecond * (uint)TimeSpan.TicksPerMillisecond);
}
```

`ValidateOffset` checks whole minutes and ±14 hours; `ValidateDate` checks the
resulting UTC instant. So .NET's order is
**offset-shape → offset-range → date → time → millisecond → UTC-range**, whereas
the port's is **date → time → millisecond → offset-shape → offset-range →
UTC-range** (measured, case 029). §8 fixes this.

### 5.4 `TimeOnly` (already correct in the port)

`TimeOnly.cs`: `TimeOnly(int, int, int, int) : this(DateTime.TimeToTicks(hour, minute, second, millisecond))`
— i.e. .NET routes `TimeOnly` through the *same* helper `DateTime` uses. The
port's `TimeOnly::validateHms` reproduces its rule, message and `paramName`
exactly. This is the shape `DateTime` must be brought to.

### 5.5 `TryParse` output on failure

`DateTimeParse.cs:2459-2472`:

```csharp
if (TryParse(s, dtfi, styles, ref resultData)) { result = resultData.parsedDate; return true; }
result = DateTime.MinValue;
return false;
```

**.NET assigns `DateTime.MinValue` to the output when `TryParse` fails.** The
port leaves the caller's previous value in place (measured cases 045–047,
059, 070–074, 078–079). See §16.2 — this is a real divergence, it is **not**
what SR-AUD-006's and SR-AUD-007's "Required post-audit verification" text
assumed, and it is deliberately **not** repaired in this family.

---

## 6. Common root causes

Three distinct causes, deliberately separated because they need different
repairs and different approval status.

**Cause A — one helper omits half its contract.**
`DateTime::dateToTicks` validates the three date components and then multiplies
the four time components straight into the tick sum. Every SR-AUD-006 symptom
— normalisation (§4.1), the invariant breach (§4.2), the UB (§4.3), the
`DateTimeOffset` inheritance (§4.5) and the parser's out-of-range acceptance
(§4.6 rows 039–042) — is that single omission, observed from a different entry
point. **One helper is the whole repair.** The 45-line `TimeOnly::validateHms`
next door already contains the missing rule.

**Cause B — validation order was chosen by C++ construction order, not by
contract.** `DateTimeOffset`'s component constructors delegate to
`DateTime(...)` in a *mem-initialiser*, so the date/time check necessarily runs
before the constructor body's offset check. .NET validates the offset first.
Nobody chose this; it fell out of the delegation shape.

**Cause C — the parsers verify *positions*, never *consumption*.** P1, P3, P5
and P7 all check a fixed separator position, run one `std::sscanf` prefix
conversion, and never assert that the conversion consumed the whole string.
`sscanf` is a *prefix* matcher, so every "trailing garbage" symptom in §4.6,
§4.7 and §4.8 is that one omission. P1 adds a second variant: when the time
`sscanf` yields fewer than three fields it **substitutes zeros** rather than
failing (`DateTime.cpp:357-358`), which is how `"10:xx:00"` becomes midnight.

Cause C is why **SR-AUD-061 belongs with SR-AUD-007/009 even though the
cross-cutting record does not list it**: `DateOnly.cpp:171` is the same
`std::sscanf` prefix pattern, in the same module, with the same symptom.

---

## 7. Corrected audit premises

Historical audit text is preserved; these are appended corrections, measured
2026-07-30, following the SR-AUD-081 / SR-AUD-362 convention.

**7.1 — SR-AUD-006 is a five-constructor, one-helper defect, and it includes
undefined behaviour the report does not mention.** The report describes
normalisation and an out-of-range tick value. Both are real (§4.1, §4.2), but
the report stops short of the most severe consequence: `hour * TicksPerHour` at
`DateTime.cpp:65` is **signed-integer-overflow UB**, reproduced under UBSan from
a plain constructor call *and from `DateTime::TryParse`* (§4.3). The finding's
severity rating (`high`) is if anything understated.

**7.2 — the SR-AUD-006 reproduction list is incomplete in one direction and
over-broad in none.** All five listed calls reproduce exactly as written. The
report does not list the negative-component cases (005–008, 011, 012), the
combined case (020), or the `DateTimeOffset` cases 021–024/026, all of which
reproduce.

**7.3 — the port's year/month/day exception identity already deviates from .NET,
and SR-AUD-006 does not cover it.** .NET throws `paramName = null` with
`Year, Month, and Day parameters describe an un-representable DateTime.`; the
port throws `paramName = "year"` / `"day"` with its own messages (cases 016–019).
This is **pre-existing**, is not what SR-AUD-006 reports, and changing it would
alter an exception identity callers can already observe. **Explicitly excluded**
(§17.1). The new time-component checks must therefore *not* be modelled on the
port's date-component throw; they are modelled on `TimeOnly::validateHms`, which
already matches .NET.

**7.4 — SR-AUD-007's "Required post-audit verification" asks for an assertion
that is factually wrong about .NET.** It requires "an assertion that a failed
`TryParse` does not overwrite the output argument". .NET does the opposite: it
assigns `DateTime.MinValue` (§5.5). The port currently preserves the output. The
audit's requested assertion would therefore have **pinned a divergence as if it
were the contract**. Recorded here, pinned as *current* behaviour only, and
carried to a separate inactive ticket (§16.2). The same wording appears in
SR-AUD-006's, SR-AUD-009's and the `DateTimeOffset` report's verification
sections and is wrong in all four places.

**7.5 — SR-AUD-007's `+02:75` reproduction is correct but understates the
class.** `+02:75` → +03:15 reproduces exactly (case 053). Also reproducing, and
not listed: `+02:60`, `+02:99`, `-02:75`, and `+02:-30` → **+01:30** (case 057),
where a *negative* minute field silently subtracts from the hour field.

**7.6 — SR-AUD-009's four listed inputs all reproduce, and its "loses a stated
fractional component" wording is precise.** `"10:20:30.1234"` yields
`.123` (case 068). But the report's implication that the fractional scan is
generally wrong is not supported: `"10:20:30.1"` correctly yields `.100`
(case 069). Only the *residual-character* and *bare-dot* halves are defects.

**7.7 — `TimeOnly`'s constructors are not part of SR-AUD-009 and are already
correct.** Cases 070–072 show `TryParse` already rejects `"24:00:00"`,
`"10:60:00"` and `"-1:00:00"` — because `TimeOnly::TryParse` explicitly range-
checks before constructing. `TimeOnly` is a *counter-example* inside the family,
not a member of its constructor half.

**7.8 — `ISOWeek`, `TimeSpan` and `Calendar`'s own guards are not defective.**
Compared line-by-line against `ISOWeek.cs` (including the deliberate
`dayOfWeek == 7` acceptance) and `TimeSpan.cs`. `Calendar::ToDateTime` has no
validation of its own **by design in .NET too** — it forwards to `DateTime`.
Fixing Cause A therefore fixes W1/W2 with no edit in `modules/globalization`.

---

## 8. Compatible versus approval-gated classification

The line is drawn exactly where #1857→#1858 and #1864→#1865 drew it: a change to
the **range of accepted component values** is compatible; a change to the
**textual grammar** — which character sequences are a well-formed date — is
approval-gated.

### 8.1 Compatible — implement now

| Class | Members | Why compatible |
|---|---|---|
| **CCF2-A** — missing component range validation | C1–C5, and W1/W2/W6 as consequences | Removes reachable UB (§4.3); removes objects that violate their own documented invariant (§4.2); no signature, layout or `noexcept` change; every currently-*valid* input keeps its exact value (§4.9). The rejected inputs never had a meaningful value. |
| **CCF2-B** — validation order in `DateTimeOffset` | C4, C5 | .NET validates the offset first (§5.3). Doing the same **keeps cases 027 and 028 exactly as they are today**, which CCF2-A would otherwise change; only case 029 moves, from one `ArgumentOutOfRangeException` to another. |
| **CCF2-C** — impossible offset-minute field in `DateTimeOffset::TryParse` | P3, P4 | A range check on a numeric component, identical in kind to `hour ≤ 23`. Today `"+02:75"` is accepted and silently *means something else* (+03:15). The accepted **grammar** is untouched: `%d:%d` still matches the same character sequences; only out-of-range values are rejected. |

**Declared consequence of CCF2-A on the parsers, which is *not* a grammar
change and is pinned by tests:** P1/P3 already funnel every parsed component
through C3 inside `try`/`catch(...)`. Once C3 validates, these inputs move from
`true` to `false` (and `Parse` from success to `FormatException`):

| Input | Before | After |
|---|---|---|
| `"2024-06-15 25:00:00"` | `true` → 2024-06-16 01:00 | `false` |
| `"2024-06-15 24:00:00"` | `true` → 2024-06-16 00:00 | `false` |
| `"2024-06-15 10:99:00"` | `true` → 11:39 | `false` |
| `"2024-06-15 10:20:99"` | `true` → 10:21:39 | `false` |
| `"2024-06-15 2000000000:00:00"` | `true` → negative ticks, **UB** | `false` |
| `"2024-06-15T25:00:00+02:00"` | `true` → next day 01:00 | `false` |

Every other measured parse case in §4.6/§4.7/§4.8 is **unchanged** by CCF2-A.

### 8.2 Approval-gated — plan only, do not implement

| Class | Members | Approval trigger |
|---|---|---|
| **CCF2-D** — parser full-consumption and fabricated-midnight grammar | P1, P2, P3, P4, P5, P6, P7, P8 | Changes the **accepted textual grammar**: `"2024-06-15junk"`, `"…10:xx:00"`, `"…30zzzz"`, `"…30."`, `"…30.abc"`, `"…30.1234"`, `"+02:00junk"`, `"+2:5"`, `"2024-06-15 10:20:30"` (as `DateOnly`) all move from accepted to rejected. Exactly the trigger #1858 and #1865 were opened for. |
| **CCF2-E** — `TryParse` failure-output normalisation | P1, P3, P5, P7 | .NET assigns `MinValue`; the port preserves the caller's value (§5.5). Changing it is an observable output-contract change for **every** failing parse, including the ones that already fail today. Same class as the just-closed CCF-014, and it must not be smuggled in under a validation ticket. |

---

## 9. Validation-order rules (binding for the implementation)

For `DateTime`'s component constructors, in this exact order:

1. `year`, `month`, `day` — existing checks, **unchanged text and `paramName`**.
2. `hour`, `minute`, `second` — one unsigned triple-compare; on failure
   `ArgumentOutOfRangeException("", "Hour, Minute, and Second parameters describe an un-representable DateTime.")`,
   byte-identical to `TimeOnly::validateHms`.
3. `millisecond` — unsigned compare; on failure
   `ArgumentOutOfRangeException("millisecond", "Valid values are between 0 and 999, inclusive.")`,
   byte-identical to `TimeOnly`'s millisecond throw.
4. **No arithmetic before step 3 completes.**

For `DateTimeOffset`'s component constructors:

1. offset shape (whole minutes) — `ArgumentException("…", "offset")`, unchanged.
2. offset range (±14 h) — `ArgumentOutOfRangeException("offset", …)`, unchanged.
3. the `DateTime` above, in its own order.
4. UTC-instant range — `ArgumentOutOfRangeException("offset", …)`, unchanged.

For `DateTimeOffset::TryParse`'s offset field: reject `mm ∉ [0, 59]` **before**
building the `TimeSpan`; return `false` (never throw).

---

## 10. Arithmetic safety strategy

The chosen strategy is **bounds-before-operation**, matching .NET exactly, and
*not* unsigned/modulo arithmetic:

- After step 2 of §9, `hour ∈ [0,23]`, `minute ∈ [0,59]`, `second ∈ [0,59]`, so
  `hour * TicksPerHour + minute * TicksPerMinute + second * TicksPerSecond`
  is at most `863 999 990 000` — eleven orders of magnitude below `INT64_MAX`.
- After step 1, `days ∈ [0, 3652058]`, so `days * TicksPerDay ≤ 3.155e18`.
- The sum is therefore `≤ MaxTicks` by construction, which is why .NET needs no
  post-hoc `MaxTicks` check and neither will this port. A defensive check is
  *not* added: it would be dead code that hides the real invariant.

Rejected alternatives, recorded so the decision is not re-litigated:

| Alternative | Why rejected |
|---|---|
| unsigned accumulate + single `> MaxTicks` compare (the `AddTicks`/`Subtract` shape) | Correct against overflow, but would still *accept* `hour = 24` when the date leaves room, i.e. it fixes the UB and leaves the .NET divergence. |
| keep the arithmetic, add a `MaxTicks` check afterwards | Does not remove the UB — the overflow happens before the check. |
| widen to `__int128` | Would make the multiplication defined but keep every wrong-value symptom, and drags in the MSVC limitation recorded in `CLAUDE.md`. |

---

## 11. Parse/grammar exclusions

Deliberately **not** changed by any compatible ticket in this family:

- whether trailing text after a complete date/time is accepted;
- whether an unparseable time field becomes midnight;
- whether a bare `.` or non-numeric fraction is accepted;
- how many fractional digits are honoured;
- whether unpadded offset fields (`+2:5`) are accepted;
- the `std::sscanf`-based conversion itself, and its formally undefined
  behaviour on an out-of-`int` numeral (measured as saturating, not misbehaving,
  on this platform — §4.3 shapes 4/5);
- the `false`-path output value (§8.2 CCF2-E).

---

## 12. Source / ABI / layout / `noexcept` matrix

| Property | Before | After CCF2-A/B/C | Consumer impact |
|---|---|---|---|
| Public signatures | 5 ctors + 4 parse entries | identical | none |
| Mangled symbols | — | identical (bodies only; `dateToTicks` is a private static already emitted in `DateTime.cpp`) | none |
| `sizeof`/`alignof`/offsets | `DateTime` 16 B (vptr + `longcs`), `DateTimeOffset` 40 B | identical — no member added or reordered | none |
| `noexcept` | none of the affected entries is `noexcept` | unchanged | none |
| `virtual` / vtable | `DateTime`/`DateTimeOffset` derive from `Object`; no virtual added or changed | unchanged | none |
| `constexpr` | none affected | unchanged | none |
| Header changes | doc-comments only (`@throws` wording) | recompilation of dependents, no relink requirement | rebuild only |
| Exception **types** thrown | AOORE / `ArgumentException` | same types only | none |
| Exception **identity** | new `paramName ""` / `"millisecond"` on paths that previously threw nothing | new information, no existing identity changed except case 029 (§8.1) | documented |

**No approval trigger is crossed by CCF2-A/B/C.**

---

## 13. Implementation dependency order

1. **CCF2-A first** — `DateTime::dateToTicks`. Everything else depends on it;
   W1/W2/W6 and §8.1's parse table are fixed by it with no further edit.
2. **CCF2-B second** — `DateTimeOffset` ordering. Must land *with or after* A,
   because before A the reorder is unobservable for cases 027/028 and after A it
   is what keeps them unchanged.
3. **CCF2-C third** — independent of both; touches only
   `DateTimeOffset::TryParse`.
4. **CCF2-D / CCF2-E** — not implemented; `needs_user` tickets.

A and B are one commit (they are one contract, and splitting them would land a
visible intermediate regression on cases 027/028). C is its own commit.

---

## 14. Permanent test matrix

Every row is an add-only GoogleTest case; no existing assertion is deleted or
weakened.

### 14.1 `DateTimeTests.cpp` (CCF2-A)

| Group | Cases |
|---|---|
| minimum valid | `(1,1,1,0,0,0,0)` → ticks 0 |
| maximum valid | `(9999,12,31,23,59,59,999)` → ticks 3155378975999990000 |
| one above max, per component | hour 24, minute 60, second 60, millisecond 1000 → AOORE |
| one below min, per component | hour −1, minute −1, second −1, millisecond −1 → AOORE |
| extreme ints | hour = `INTCS_MAX`, `INTCS_MIN`, ±2e9 → AOORE (was UB) |
| invariant boundary | `(9999,12,31,24,0,0)` and `(9999,12,31,23,59,59,1000)` → AOORE, **not** a tick above `MaxTicks` |
| lower invariant boundary | `(1,1,1,-1,0,0)`, `(1,1,1,0,0,0,-1)` → AOORE, not a negative tick |
| leap year | `(2024,2,29,23,59,59,999)` valid; `(2023,2,29)` AOORE `param='day'` |
| month lengths | `(2024,4,31)` AOORE; `(2024,1,31,23,59,59,999)` valid |
| validation order | `(2024,13,1,24,0,0)` → `param='year'`, **not** the hour message |
| validation order | `(2024,1,1,24,60,60,1000)` → the *hour* message, not the millisecond one |
| exception identity | exact type, `paramName`, HResult `0x80131502`, and verbatim message for each of the four new throws |
| six-arg vs seven-arg | both overloads reject identically |
| no partial state | a caller-owned `DateTime` sentinel is unchanged after a throwing construction |

### 14.2 `DateTimeOffsetTests.cpp` (CCF2-A + CCF2-B + CCF2-C)

| Group | Cases |
|---|---|
| component rejection | hour 24 / minute 60 / second 60 / ms 1000, both overloads |
| order: offset first | `(2024,1,1,24,0,0,+15h)` → `param='offset'`; `(2024,13,1,0,0,0,+15h)` → `param='offset'` |
| order: whole minutes first | `(2024,1,1,24,0,0, 90 ticks)` → `ArgumentException` `param='offset'` |
| order: date before time | `(2024,13,1,24,0,0,+00)` → `param='year'` |
| UTC-range guard still last | `(9999,12,31,23,59,59,999,-14h)` → `param='offset'` UTC message |
| offset minutes | `TryParse("…+02:75")`, `+02:60`, `+02:99`, `-02:75`, `+02:-30` → all `false` |
| offset minutes valid | `+02:59`, `+02:00`, `-05:30`, `+14:00`, `-14:00` → `true`, exact minutes |
| parse consequence | `"2024-06-15T25:00:00+02:00"` → `false` |
| unchanged grammar | `"…+02:00junk"` and `"…+2:5"` still `true` (pinned as *current*, with a comment naming CCF2-D) |

### 14.3 `TimeOnlyTests.cpp` / `DateOnlyTimeOnlyTests.cpp`

Pin the already-correct behaviour so a future grammar ticket cannot silently
change it: cases 069–074 and 078–079, plus `TimeOnly`'s four constructor throws
with exact identity.

### 14.4 `CalendarTests.cpp` (globalization)

`Calendar().ToDateTime(2024,1,1,24,0,0,0)` → AOORE, proving W1/W2 inherit the
repair with no edit in `modules/globalization`.

### 14.5 Mutation checks (required before the ticket may close)

- delete the hour check → the hour, extreme-int and invariant-boundary tests must fail;
- delete the millisecond check → the millisecond tests must fail;
- move the offset check back after the `DateTime` → the CCF2-B order tests must fail;
- delete the offset-minute check → the CCF2-C tests must fail.

---

## 15. Sanitizer matrix

| Target | Applicable | Why |
|---|---|---|
| **UBSan** | **required** | The family's core defect is signed-integer-overflow UB (§4.3). The probe must be rebuilt with `-fsanitize=undefined` against the **repaired** bodies and re-run for shapes 1–3, expecting **zero** reports and an `ArgumentOutOfRangeException` instead. One shape per process, `volatile` operands. |
| ASan | not applicable | No allocation, no pointer arithmetic, no lifetime change, no new member. Recorded with reason rather than skipped silently. |
| LSan | not applicable | No ownership transfer. |
| TSan | not applicable | No shared state; all touched functions are pure value computations on caller-owned storage. |

`build-asan` is **not** rebuilt for this family: the changed code is compiled
directly into the probe, so no stale archive can be involved.

---

## 16. Explicit exclusions

**16.1 — the year/month/day exception identity** (§7.3). Pre-existing deviation,
observable, not part of any CCF-002 finding.

**16.2 — `TryParse` failure-output normalisation** (§5.5, §7.4, CCF2-E).
Separate inactive ticket; **no `SR-AUD-*` identifier issued**. It is a
CCF-014-class contract question for four date/time parsers, and the audit's
own verification text asserts the opposite of .NET, so it needs its own
before/after record.

**16.3 — the `std::sscanf` conversions themselves.** Formally undefined for a
numeral outside `int`; measured as saturating on glibc (§4.3 shapes 4/5) and
unreachable past the existing guards for every field except `hour`, which
CCF2-A validates. Replacing `sscanf` is part of CCF2-D's grammar rewrite, not of
a range-validation ticket.

**16.4 — missing .NET surface.** `DateTimeKind`, `DateTime.Kind`,
`SpecifyKind`, the `Calendar`-taking constructors, the microsecond
constructors, `ParseExact`/`TryParseExact`, and `IFormatProvider` overloads are
all absent from this port by prior documented decision (`DateTime.hpp:24-29`,
`TimeOnly.hpp:27-35`). Adding them is new API surface, not a validation repair.
Note in passing that .NET's `(uint)kind > (uint)DateTimeKind.Local` guard —
an "invalid enum accepted" check — **has no counterpart to audit here**,
because no port entry point takes a `DateTimeKind`.

**16.5 — leap seconds.** .NET's `second == 60` path is guarded by
`SystemSupportsLeapSeconds`, which is a Windows-only OS capability. The port has
no equivalent and rejects `second == 60`; .NET on Linux does the same. Not a
divergence.

**16.6 — the `TimeZoneInfo`, `Calendar`-shape, `XmlConvert` and
`Stopwatch`/`TimeProvider` findings.** Different causes; §1.

**16.7 — surfaces inspected for this family and found already correct.** Listed
so a later batch does not re-derive them, and so "complete inventory" is a
statement of work done rather than a claim:

| Surface | Checked | Verdict |
|---|---|---|
| `DateTimeOffset::FromUnixTimeSeconds` / `FromUnixTimeMilliseconds` | range-checked against the exact `min`/`max` before the multiplication, with .NET's `Valid values are between {0} and {1}, inclusive.` message | correct |
| `DateTimeOffset::ToUnixTimeSeconds` / `ToUnixTimeMilliseconds` | pure division of an already-valid tick count | correct |
| `DateTime::AddDays` / `AddHours` / `AddMinutes` / `AddSeconds` / `AddMilliseconds` / `AddTicks` / `Add` / `Subtract` | bounded-before-multiply or unsigned-wrap-then-compare, with the reasoning recorded in-place | correct — repaired under CCF-004 |
| `DateTime::AddMonths` / `AddYears`, `DateOnly::AddMonths` / `AddYears` / `AddDays` / `FromDayNumber` | ±120000 / ±10000 bounds before any arithmetic | correct — repaired under CCF-004 (SR-AUD-060) |
| `DateTime::IsLeapYear` / `DaysInMonth` | explicit year and month guards | correct |
| `Globalization::ISOWeek::ToDateTime` / `ToDateOnly` / `GetWeeksInYear` / `GetYearStart` | line-by-line against `ISOWeek.cs`, including its deliberate `dayOfWeek == 7` acceptance | correct |
| `Globalization::Calendar::AddMonths` / `AddYears` / `AddWeeks` / `ToFourDigitYear` | bounds before multiply; `ToFourDigitYear` rejects a negative year | correct |
| `TimeSpan`'s component constructors | .NET accepts `TimeSpan(25,0,0)` by design; the overflow arithmetic is CCF-004 work already done | correct — no range exists to enforce |
| `TimeOnly`'s four component constructors and its tick constructor | already carry the exact rule, message and `paramName` `DateTime` was missing | correct — the family's counter-example |
| `DateOnly`'s component constructor | delegates to `DateTime(year, month, day)`, matching `DateOnly.cs` | correct |
| `XmlConvert`'s date/time conversions | SR-AUD-354 is a *duration-grammar* finding on `TimeSpan`, with no shared cause | out of family |
| `Stopwatch` / `TimeProvider::GetElapsedTime` | SR-AUD-131, a CCF-004-class subtraction | out of family |
| `TimeZoneInfo` / `TimeZone` | SR-AUD-224…230, timezone *semantics* | out of family |

**No `DateTimeKind`-taking entry point exists in this port**, so .NET's
`(uint)kind > (uint)DateTimeKind.Local` guard — the "invalid enum accepted"
class the family template asks about — has nothing here to audit.

---

## 17. Performance considerations

Three integer compares per constructor call, on values already in registers,
against branch targets that never execute in valid code. The removed work is
larger than the added work in one case: `DateTimeOffset` currently constructs a
whole `DateTime` before discovering an invalid offset; after CCF2-B it rejects
first. No allocation, no branch in a loop, no change to any hot path. No
benchmark is required, and none is claimed.

---

## 18. Completion criteria

CCF-002 may be marked **CLOSED** only when all of the following hold:

1. CCF2-A, CCF2-B and CCF2-C are implemented and committed.
2. SR-AUD-006 is `remediated`.
3. SR-AUD-007 is recorded as **split**: `007a remediated` (offset-minute range,
   CCF2-C) and `007b open` (grammar, CCF2-D) — the `SR-AUD-035`/`043` convention.
4. SR-AUD-009 and SR-AUD-061 remain `confirmed`, with the CCF2-D ticket named in
   their reports.
5. §14's test matrix is present and its §14.5 mutation checks were run.
6. §15's UBSan re-run is clean on the repaired bodies.
7. The premise corrections in §7 are appended to the four per-file audit reports.
8. A `needs_user` ticket exists for CCF2-D with the exact before/after string
   table from §8.2, and an inactive ticket exists for CCF2-E.

**Until item 8's tickets are answered, CCF-002 is `PARTIALLY REMEDIATED`, not
closed.** This document states that plainly so a later batch does not read
"three of five classes done" as closure.

---

## 19. Ticket breakdown and status

| Ticket | Class | Status | Scope |
|---|---|---|---|
| **#1876** | — | this document | CCF-002 family plan |
| **#1877** | CCF2-A + CCF2-B | implementation | `DateTime::dateToTicks` component validation; `DateTimeOffset` validation order |
| **#1878** | CCF2-C | implementation | `DateTimeOffset::TryParse` offset-minute range |
| **#1879** | CCF2-D | `needs_user` | parser full-consumption / fabricated-midnight grammar across P1–P8 |
| **#1880** | CCF2-E | `todo`, **inactive** | `TryParse` failure-output normalisation for the four date/time parsers |

**Status after this batch (2026-07-30).** #1876, #1877 and #1878 are `done`;
SR-AUD-006 is `remediated`, SR-AUD-007 is split `007a remediated / 007b open`,
and SR-AUD-009 and SR-AUD-061 remain `confirmed`. By §18's criteria CCF-002 is
**PARTIALLY REMEDIATED**, not closed: items 1–7 hold, item 8's two tickets
(#1879 `needs_user`, #1880 inactive) are open, and their decision records are
§20.1 and §20.2.

---

## 20. Approval decision records

Written 2026-07-30 after #1877 and #1878 landed, so every "current state" line
below describes the tree **as it is now**, not as it was before this batch.
Neither record authorises any change: both wait for an explicit per-action
decision, exactly as `docs/FloatingValueFidelityPlan.md` §19 does for
#1854/#1858/#1862/#1863/#1865.

### 20.1 CCF2-D / #1879 — parser accepted-grammar strictness (SR-AUD-007b, SR-AUD-009, SR-AUD-061)

**Current state (measured, post-#1877/#1878).** Four parsers verify separator
*positions*, run one `std::sscanf` prefix conversion, and never assert that the
conversion consumed the whole string. `DateTime::TryParse` additionally
substitutes zeros for all three time fields when its `%d:%d:%d` conversion yields
fewer than three (`DateTime.cpp:357-358`), which is how a malformed time becomes
midnight.

**.NET reference (exact).** `DateTimeParse.cs` is a full state-machine lexer:
it tokenises, tracks `DTFI` patterns, and fails on any unconsumed token
(`ParseFraction`, `Lex`, and the terminal `if (str.Index < str.Length)`
consumption test). There is no prefix-acceptance path in the reference at all.
`TimeOnly.cs` and `DateOnly.cs` delegate to the same machinery. So the reference
rejects **every** input in the table below.

**Exact before/after.** All of these return `true` today and would return
`false`; `Parse` succeeds today and would throw `FormatException`.

| Entry | Input | Currently accepted as | Under #1879 |
|---|---|---|---|
| `DateTime::TryParse` | `"2024-06-15junk"` | 2024-06-15 00:00:00 | `false` |
| `DateTime::TryParse` | `"2024-06-15 10:xx:00"` | 2024-06-15 **00:00:00** | `false` |
| `DateTime::TryParse` | `"2024-06-15 trailing"` | 2024-06-15 **00:00:00** | `false` |
| `DateTime::TryParse` | `"2024-06-15T10:20:30zzzz"` | 10:20:30 | `false` |
| `DateTime::TryParse` | `"2024-06-15T10:20:30."` | 10:20:30.000 | `false` |
| `DateTime::TryParse` | `"2024-06-15T10:20:30.abc"` | 10:20:30.000 | `false` |
| `DateTime::TryParse` | `"2024-06-15T10:20:30.1234"` | 10:20:30.**123** | `false` |
| `DateTimeOffset::TryParse` | `"…+02:00junk"` | +120 min | `false` |
| `DateTimeOffset::TryParse` | `"…+2:5"` | **+125 min** | `false` |
| `TimeOnly::TryParse` | `"10:20:30.abc"` | 10:20:30.000 | `false` |
| `TimeOnly::TryParse` | `"10:20:30junk"` | 10:20:30.000 | `false` |
| `TimeOnly::TryParse` | `"10:20:30."` | 10:20:30.000 | `false` |
| `TimeOnly::TryParse` | `"10:20:30.1234"` | 10:20:30.**123** | `false` |
| `DateOnly::TryParse` | `"2024-06-15junk"` | 2024-06-15 | `false` |
| `DateOnly::TryParse` | `"2024-06-15 10:20:30"` | 2024-06-15 | `false` |

**Unchanged in every option** — the documented grammars keep parsing to identical
values: `"yyyy-MM-dd"`, `"yyyy-MM-dd HH:mm:ss"`, `"yyyy-MM-ddTHH:mm:ss"`, an
optional 1–3-digit fraction, a trailing `Z`/`z`, and a `±HH:MM` offset.

**Options.**

- **(A) Full-consumption parity.** Replace `std::sscanf` in all four parsers with
  a hand-written strict scanner that consumes a fixed-width field, requires the
  exact separator, and fails on any residual character. Also removes the
  fabricated-midnight substitution, and removes `sscanf`'s formally undefined
  behaviour on a numeral outside `int` (measured as saturating on glibc, §4.3
  shapes 4/5, so this is a standards-conformance repair, not an observed bug).
- **(B) Full consumption only, keep the fabricated midnight.** Rejects trailing
  text but still turns `"10:xx:00"` into midnight — internally inconsistent, and
  recommended against.
- **(C) Do nothing.** Leaves three `confirmed` findings open indefinitely.

**Recommendation: (A).** It is the only option that makes the port's own
documented grammar (`DateTime.hpp:337-339`, `TimeOnly.hpp:32-34`) true.

**Approval trigger:** accepted textual grammar. A caller that today gets a usable
`DateTime` from a log line with a trailing token would start getting `false`.

**Source / ABI / layout / `noexcept`:** none under any option. All four bodies
are in `.cpp` files; no public signature, `noexcept` specification, virtual
function, vtable slot, data member, `sizeof`, `alignof` or member offset changes.
Consumers need no migration beyond correcting inputs that were never valid.

**Rollback:** revert the single commit; the parsers are self-contained and no
other subsystem reads their internals.

**Test matrix (add-only, when approved):** every row of the table above in both
directions — the listed input rejected, and the corresponding well-formed input
still parsed to its exact current value — plus `Parse`/`TryParse` asserted
separately, plus the four `Ccf002_*GrammarIsPinnedUnchanged` tests inverted in
the same commit.

**Performance:** a hand-written scanner is strictly faster than `sscanf`
(no format-string interpretation, no locale lookup). No benchmark is required.

### 20.2 CCF2-E / #1880 — `TryParse` failure-output normalisation

**Current state (measured).** All four parsers leave the caller's previous output
value in place when they return `false` (cases 045–047, 059, 070–074, 078–079).

**.NET reference (exact).** `DateTimeParse.cs:2459-2472` assigns
`result = DateTime.MinValue` before returning `false`. `DateTime.TryParse(null,
out)` assigns `default` explicitly (`DateTime.cs:1774-1780`).

**Why this is not folded into #1877 or #1878.** It changes the observable output
of **every** failing parse, including the ones that already failed before this
batch — it is not a consequence of validation, it is a separate contract. It is
the same class as the just-closed CCF-014, and CCF-014's own lesson was that a
`Try` output contract deserves its own measured record.

**Corrected premise it rests on.** The "Required post-audit verification" text of
all four CCF-002 reports asks for an assertion that a failed `TryParse` does
**not** overwrite the output. That is the opposite of the reference. Any batch
that implements those verification paragraphs literally would pin the divergence
as the contract. See §7.4.

**Options.** (A) adopt .NET's assign-minimum-on-failure for all four parsers;
(B) adopt it only where a `Try*` sibling in the same module already does;
(C) document the divergence as deliberate and close the ticket `wontfix`.
No recommendation is made here — it needs its own probe of how many
same-repository `Try*` methods follow each convention, which is why #1880 is
**inactive** rather than `needs_user`.

**Source / ABI / layout / `noexcept`:** none under any option.

**Ticket status:** `todo`, **inactive** — do not start without confirming it is
still wanted. No `SR-AUD-*` identifier issued.
