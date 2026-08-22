<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::TimeZoneInfo` and `System::TimeZone` behaviour changes (2026-08-10)

Tickets **#2177–#2184**, closing findings **SR-AUD-223, 224, 225, 226, 227 and 229** plus two
post-audit defects. **No public signature, virtual function, vtable slot, object layout, mangled
symbol or `noexcept` specification changed** — every change below is behavioural. Source compiles
unchanged; a rebuild is enough.

> **Post-#1941 addendum, 2026-08-22 (#2418).** The statement above describes tickets #2177–#2184,
> not the later Kind ripple. #2418 adds a default UTC-instant query to `ILocalTimeZone`, so the
> public interface vtable changes and consumers must rebuild. Existing fixed-offset derived
> classes remain source-compatible through the default implementation.

`sizeof(TimeZoneInfo)` is **160** bytes before and after, pinned by a `static_assert` in the test
suite; `sizeof(TimeZoneInfo::TransitionTime)` (40) is likewise unchanged.

---

## 1. `BaseUtcOffset`, `StandardName` and `DaylightName` are now invariant

**This is the change most likely to move a number in your code.**

`FindSystemTimeZoneById` used to read `tm_gmtoff` and `tm_zone` at `time(nullptr)` and store
whatever the zone happened to be doing at that instant. The result depended on the month the
process ran in:

| | Before (queried in July) | Before (queried in January) | After (always) |
|---|---|---|---|
| `America/New_York` `BaseUtcOffset` | −4:00 | −5:00 | **−5:00** |
| `America/New_York` `StandardName` | `EDT` | `EST` | **`EST`** |
| `America/New_York` `DaylightName` | `EDT` | `EST` | **`EDT`** |
| `Europe/Dublin` `BaseUtcOffset` | +1:00 | 0:00 | **0:00** |
| `Australia/Sydney` `BaseUtcOffset` | +10:00 | +11:00 | **+10:00** |
| `Africa/Casablanca` `BaseUtcOffset` | +1:00 | +1:00 | **0:00** |

Measured across the whole installed database: **499 TZif zones, 158 of which observe daylight
time**; on 2026-08-10 **141 of them reported the wrong base offset**, and the other 17 — the
southern-hemisphere zones — would have been wrong in January instead.

The header always documented this property as *"always the standard offset"*. The original
repair made the POSIX branch honour that contract. A later #2418 review found a separate Windows
edge: the standard offset is `Bias + StandardBias`, not `Bias` alone. Both the local-zone and
named-zone Windows paths now use that complete value; `DaylightBias` remains excluded.

### What moves

`ConvertTime`, `ConvertTimeFromUtc`, `ConvertTimeToUtc` and `ConvertTimeBySystemTimeZoneId` all
apply `BaseUtcOffset`. For a daylight-observing zone during its daylight period they now shift by
the **standard** offset — one hour less than before for most zones, thirty minutes for
`Australia/Lord_Howe`. That is the consistent reading of the type's documented "DST transitions are
not modelled" contract; the previous answer was correct for roughly half the year and silently
wrong for the other half, with which half depending on when your process started.

### If you need daylight-aware conversion

`TimeZoneInfo` does not provide it — see `docs/SystemTimeZoneNamespaceReviewPlan.md` §18. The
legacy `System::TimeZone::CurrentTimeZone()` adapter does, for the process-local zone (§3 below).

---

## 2. Four inputs that used to be accepted are now rejected

| Call | Before | After |
|---|---|---|
| `CreateCustomTimeZone("", offset, …)` | accepted, id `""` | `ArgumentException` |
| `CreateCustomTimeZone(id, ±15h, …)` | accepted | `ArgumentOutOfRangeException` |
| `CreateCustomTimeZone(id, TimeSpan::FromSeconds(90), …)` | accepted | `ArgumentException` |
| `AdjustmentRule::CreateAdjustmentRule(later, earlier, …)` (both overloads) | accepted | `ArgumentException` |

`±14:00` exactly is still accepted, in both directions. Equal `dateStart` and `dateEnd` are still
accepted — a single-day rule is legal.

The offset bound also closes a door that used to reach `TimeSpan`'s negation guard: a zone built
with `TimeSpan::MinValue` made `ConvertTimeToUtc` report *"Negating the minimum value of a twos
complement number is invalid."* The argument is now refused where it is supplied.

**Migration:** if you built a custom zone with a sub-minute offset or an unnamed id, give it a name
and round the offset to whole minutes.

---

## 3. `TimeZone::CurrentTimeZone()` answers per date

`GetUtcOffset(DateTime)` used to return one offset captured at static-construction time, and
`IsDaylightSavingTime(DateTime)` used to return `false` for every date. Under
`TZ=America/New_York`:

| Date | Before | After |
|---|---|---|
| 2025-01-15 | −4:00, DST false | **−5:00, DST false** |
| 2025-07-15 | −4:00, DST false | **−4:00, DST true** |

Two boundary behaviours are worth knowing:

- A local time that occurs **twice** (01:30 on 2025-11-02 in New York) resolves to the **standard**
  reading. Raw `mktime` is not deterministic here — measured, it inherits the daylight state of the
  preceding conversion in the same process, so the identical argument answered −4:00, −5:00 or
  −4:00 depending on unrelated earlier calls. The adapter resolves the ambiguity explicitly, so the
  answer is a function of the argument alone.
- A local time that **does not occur** (02:30 on 2025-03-09) normalises forward and reports the
  post-transition offset. The legacy `TimeZone` surface has no member with which to report
  invalidity.

`TimeZoneInfo` is deliberately **not** changed to match: its fixed-offset model is documented, and
making it date-sensitive is a different, larger change.

After `DateTimeKind` became observable, two further distinctions became load-bearing:

- public `GetUtcOffset` returns zero, and public `IsDaylightSavingTime` returns false, for a Utc
  DateTime; Local and Unspecified inputs remain local wall-clock questions;
- `DateTime::ToLocalTime` resolves a separate UTC-instant query. For example, New York
  `2025-03-09T06:30Z` maps with -05:00 to 01:30, while `07:30Z` maps with -04:00 to 03:30.

The second route cannot be implemented by feeding UTC calendar fields to `mktime`: that function
interprets them as a local wall clock and selects the wrong transition side.

---

## 4. Case-variant zone ids now compare equal

`Equals` compared identifiers byte-for-byte while `GetHashCode` folded case, so `"Zone"` and
`"zone"` compared **unequal** yet hashed **equal** — a broken equality/hash pair as well as a
divergence from .NET's `OrdinalIgnoreCase` comparison. Both sides now use one ordinal ASCII fold.

This also removes a `std::tolower` call, so the answer no longer depends on the process
`LC_CTYPE`.

**Migration:** if you relied on case-sensitive zone-id identity — for instance keying a map on a
`TimeZoneInfo` and expecting `"UTC"` and `"utc"` to be distinct entries — they are now one key.

---

## 5. A failed `TryFindSystemTimeZoneById` clears its out parameter

```cpp
std::shared_ptr<TimeZoneInfo> zone = TimeZoneInfo::FindSystemTimeZoneById("UTC");
bool found = TimeZoneInfo::TryFindSystemTimeZoneById("Mars/Olympus", zone);
// before: found == false, zone still points at UTC
// after:  found == false, zone == nullptr    (matching .NET's `out` = null)
```

**Migration:** code that ignored the `bool` and used the out parameter anyway now dereferences a
null `shared_ptr` instead of silently using an unrelated zone. Honour the return value.

---

## 6. Identifiers that are not zones are rejected

`FindSystemTimeZoneById` previously accepted any regular file under `/usr/share/zoneinfo`, and then
handed the identifier to `setenv("TZ", …)`, which glibc parses as a POSIX rule string when it is
not a path. Every plain-text data file the tz database ships therefore resolved to a zone:

```
zone.tab  zone1970.tab  iso3166.tab  tzdata.zi  leapseconds  leap-seconds.list
        -> accepted, BaseUtcOffset 0, StandardName taken from the filename
```

Now rejected, along with these identifier shapes, all of which used to resolve:

| Identifier | Why it is refused |
|---|---|
| `America//New_York`, `America///New_York` | empty path segment |
| `./America/New_York`, `America/./New_York` | `.` segment |
| `America/New_York/` | trailing separator |
| `/America/New_York`, `/etc/passwd` | absolute path |
| `"America/New_York\0junk"` | embedded NUL — the C string stopped at the NUL while all 21 bytes were stored as the zone's `Id` |

`..` was already refused and still is. Every real installed zone still resolves, including
single-segment ids (`UTC`, `EST5EDT`), three-segment ids
(`America/Argentina/Buenos_Aires`) and ids containing `+` (`Etc/GMT+5`).

---

## 7. `TZ` is restored on every path

`FindSystemTimeZoneById` temporarily points the process at the zone being queried. The restore was
straight-line code, so an exception in the window left the process-global zone changed for the rest
of its life; it is now a scope guard.

The restore also used to branch on whether the saved value was **empty** rather than whether it was
**set**, so a process running with `TZ=""` — which POSIX defines as UTC — came back with `TZ`
*deleted*, silently switching to whatever `/etc/localtime` says. An empty-but-set `TZ` is now
restored as empty.

#2418 moved the guard into Core.Base and made every production `localtime_r` reader share it,
including `DateTime::Now` and `DateTimeOffset`'s current-offset path. This closes the remaining
window in which a concurrent Core call could observe the temporary zone selected by a lookup.

---

## Evidence

Before/after measurements, the whole-database survey, the `mktime` ambiguity experiment and the
sanitizer runs are recorded in `docs/SystemTimeZoneNamespaceReviewPlan.md` and in
`build-probe/2176_probe*.log`.
