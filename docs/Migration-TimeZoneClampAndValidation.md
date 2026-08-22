<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — time-zone conversions clamp, and adjustment rules validate (ticket #2186)

*2026-08-18.* #2186 asked five `System::TimeZone` parity questions, each *"requiring the .NET
reference or a managed runtime that this container does not have"*. `/rv` answers all five.
**Three were repairs, two were already correct — and the reference corrects the ticket's own
statement of two of the repairs.**

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. The five answers

| # | Question | .NET | Outcome |
|---|---|---|---|
| 1 | Do the conversions clamp at `DateTime::Min/MaxValue`? | **yes** (`TimeZoneInfo.Cache.cs:340-342`) | **repaired** |
| 2 | Does `CreateAdjustmentRule` reject three further shapes? | **yes** (`AdjustmentRule.cs:206-223`) | **repaired**, with two corrections |
| 3 | What are the exact resource strings? | four of them | **landed** |
| 4 | Does `TryFind…` return false for every failure? Which exception for non-zone data? | false always; **`InvalidTimeZoneException`** | half already correct, half **repaired** |
| 5 | For an ambiguous local time, is the standard reading preferred? | **yes** | **already correct** |

## 2. Question 1 — the conversions clamp

```csharp
private static DateTime SafeCreateDateTimeFromTicks(long ticks, DateTimeKind kind = …)
    => (ulong)ticks <= DateTime.MaxTicks ? new DateTime(ticks, kind)
                                         : (ticks < 0 ? DateTime.MinValue : DateTime.MaxValue);
```

| Call | Was | Is |
|---|---|---|
| `ConvertTimeFromUtc(DateTime::MaxValue, +14)` | `ArgumentOutOfRangeException` | `DateTime::MaxValue` |
| `ConvertTimeToUtc(DateTime::MinValue, +14)` | `ArgumentOutOfRangeException` | `DateTime::MinValue` |
| any in-range conversion | — | **unchanged** |

The cast to `ulong` is the whole trick and is reproduced deliberately: a negative tick count wraps
to something enormous, so **one** unsigned comparison rejects both ends at once.

`ConvertTime(dt, source, destination)` clamps **once, at the end**. .NET computes the result *"from
raw ticks to avoid precision loss from double-clamping"* (`TimeZoneInfo.cs:683-685`), so an
intermediate UTC value that leaves the range must not drag a representable final answer to a bound.
A test pins that, and a mutation that clamps twice is caught.

**.NET does not clamp everywhere**, and the exception is documented rather than smoothed over: its
invalid-time compatibility path builds a raw `new DateTime(...)` and lets it throw, with a comment
saying so (`TimeZoneInfo.cs:661-667`). That path needs `TimeZoneInfoOptions` and adjustment rules
this port's `TimeZoneInfo` does not model, so it is unreachable here.

## 3. Question 2 — and two of the ticket's three statements were wrong

#2179 measured all three as accepted and declined to repair them, because *"inventing three more
rejections on a recollection of the .NET source is exactly what this review declines to do."* That
was the right call, and the reference shows why:

* **the `daylightDelta` range is not ±14 hours.** It is `-23.0 .. 14.0`, and .NET explains why in
  a comment of its own: *Samoa moved across the International Date Line*, so describing its delta
  needs −23. **The message still says "plus or minus 14.0 hours"**, because it is shared with
  `UtcOffsetOutOfRange`. That inconsistency is .NET's and is transcribed rather than tidied;
* **the seconds check is not "sub-minute".** It is *not a whole number of minutes*, so
  `1h30m30s` fails as surely as `30s` does;
* the time-of-day check **exempts** `MinValue` for `dateStart` and `MaxValue` for `dateEnd`.

At the time #2186 landed, this port had no stored `DateTimeKind`, so the reference's Kind guards
could not be expressed. That statement became stale when #1941 added Kind. The post-#1941
`TimeZoneInfo` ripple audit now makes the **public** factories require Unspecified
`dateStart`/`dateEnd`; both Local and UTC are rejected before the time-of-day and range checks.
The runtime's broader internal rule factories are not exposed here. See
`docs/Migration-TimeZoneInfoDateTimeKind.md`.

## 4. Question 4 — two questions, two different answers

`TryFindSystemTimeZoneById` returns **false for every failure**: .NET's discards the exception
outright (`TimeZoneInfo.cs:526-527`), and this port's `catch (...)` already did the same.

The throwing form is where the divergence was. #2183 folded three failures into one boolean and
kept `TimeZoneNotFoundException` for all of them *"rather than guessing InvalidTimeZoneException"*.
.NET raises `InvalidTimeZoneException` when the file **exists but is not zone data**
(`TimeZoneInfo.Unix.cs:697`) and reserves `TimeZoneNotFoundException` for an id that names nothing.
Those are different answers to different questions — *"there is no such zone"* versus *"that is not
a zone"* — and a caller catching only the first used to swallow the second.

## 5. Question 5 — already correct, and the reference contains a trap

.NET's legacy `TimeZone.CalculateUtcOffset` first decides `isDst` from the DST window, which puts
an ambiguous 01:30 **inside daylight** — and then overrides it:

```csharp
if (isDst && time >= ambiguousStart && time < ambiguousEnd)
    isDst = time.IsAmbiguousDaylightSavingTime();      // TimeZone.cs:237-240
```

That flag is set only by a prior UTC→local conversion, so a `DateTime` built from its fields
carries `false` and the answer is the **standard** offset. **Reading only the window test gives the
opposite answer**, and that is the trap: the override decides it, not the window. #2182 chose
standard, and it chose right.

## 6. To migrate

Code that caught `ArgumentOutOfRangeException` around a conversion at the extremes will no longer
see it; the result is clamped instead. Code that builds adjustment rules with a time-of-day, a
sub-minute delta or a delta beyond `-23..14` hours will now be rejected.

## 7. Evidence

| Mutation | Caught |
|---|---|
| Clamp both ends to `MaxValue` (the sign is ignored) | ✅ |
| `ConvertTime` clamps the intermediate UTC value too | ✅ |
| The `daylightDelta` range becomes a symmetric ±14 | ✅ |
| The whole-minutes check becomes a sub-minute check | ✅ |
| Non-zone-data goes back to `TimeZoneNotFoundException` | ✅ (3 tests) |

## 8. Downstream

Neither `cna` nor `mobile-eggbert` references `TimeZoneInfo` or `TimeZone` — zero sites in both.
