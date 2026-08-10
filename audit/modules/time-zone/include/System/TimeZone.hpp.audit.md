# Audit: `modules/time-zone/include/System/TimeZone.hpp`

## Metadata

- AUDITED: 61-line legacy abstract `TimeZone` compatibility surface and its
  `CurrentTimeZone` entry point.
- Validation: `TimeZoneTest.*` passed 8/8 on 2026-07-27.  A direct native
  comparison ran under `TZ=America/New_York` for January and July 2024.
- Reference basis: current .NET 10 obsolete-but-supported `System.TimeZone`
  CurrentTimeZone/GetUtcOffset/IsDaylightSavingTime behavior.

## SR-AUD-223 — medium — legacy CurrentTimeZone freezes one current offset and reports no daylight saving time for every date

The local adapter snapshots `TimeZoneInfo::Local().BaseUtcOffset` during
static construction; `GetUtcOffset` always returns that snapshot and
`IsDaylightSavingTime` always returns false.  With `TZ=America/New_York`, the
native probe prints winter/summer offsets `-4,-4` and DST `0,0`, whereas the
matching .NET 10 probe prints `-5,-4` and `False,True`.  The behavior corrupts
historical and future legacy-zone conversions whenever local DST rules apply.

## Assessment

The abstract custom-subclass surface is coherent.  Its default local adapter
is not an adequate compatibility path for a date-sensitive time-zone API.

## Other missing assertions and diagnostics

- Current tests inspect only the current name/reference, never winter versus
  summer dates under a deterministic DST `TZ` setting; add that process-isolated
  regression for SR-AUD-223.
- Test fixed-offset zones, local timezone reload/cache behavior, invalid and
  ambiguous local times, and exception propagation from the underlying
  TimeZoneInfo provider.

## Final assessment

SR-AUD-223 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed.

---

## Remediation record — ticket #2182 (2026-08-10)

**SR-AUD-223 → remediated.** Original evidence above is retained unchanged; this section appends
what measurement added.

**Premise correction: this is two independent defects, not one.** The adapter froze the offset at
static construction *and* hard-coded `IsDaylightSavingTime` to `false`. Repairing either alone still
answers a July date in New York wrongly, so both are repaired and both are pinned separately.

`SystemTimeZoneAdapter` (file-local, anonymous namespace in `TimeZone.cpp`) now resolves both
members per date through `mktime` against the process-local zone, keeping the cached offset only as
the fallback for a date the platform cannot resolve and as the whole answer on Windows and
Emscripten. Measured under `TZ=America/New_York` (`build-probe/2176_probe1_surface_after.log`):

```
GetUtcOffset(Jan 15 2025) = -300 min   IsDaylightSavingTime = 0
GetUtcOffset(Jul 15 2025) = -240 min   IsDaylightSavingTime = 1
std = EST   dst = EDT
```

matching the −5/−4 and False/True this report recorded for current .NET.

**A third defect found while writing the pins.** Raw `mktime(tm_isdst = -1)` is not deterministic
for a *repeated* local hour: measured in `build-probe/2176_probe6_ambiguous.log`, 2025-11-02 01:30
answers −240 as a process's first call, −300 immediately after a standard-time conversion and −240
immediately after a daylight one. The same argument giving different answers depending on unrelated
earlier calls is a defect on its own terms. The adapter now re-asks the standard reading and takes
it only when it reproduces the requested wall clock, so the answer is a function of the argument
alone. Which of the two readings to prefer rests on a recollection of .NET and is carried by
ticket #2186; the determinism does not.

**Missing assertions this report asked for, now present** (`TimeZoneTests.cpp`, +14 tests): winter
versus summer under a deterministic `TZ`, both 2025 transitions from four sides each, the
non-existent and repeated local hours, southern-hemisphere and non-daylight zones, the `DateTime`
range extremes, and `TZ` restoration across successful and failing lookups. The one-time static
cache is exercised by resolving per call rather than by asserting the cached value.

Three mutations proven: reverting `GetUtcOffset` fails 9 pins, reverting `IsDaylightSavingTime`
fails 5, and dropping the ambiguity re-ask fails exactly the determinism pin.

**No public signature, virtual function, vtable slot, object layout, mangled symbol or `noexcept`
specification changed** — the adapter is not a public type.
