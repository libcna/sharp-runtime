<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `DateTime` converts by its `Kind` (ticket #1941, phase 2, SA-15.1)

*2026-08-20.* Phase 1 (2026-08-19) made `DateTime` **store and report** a `DateTimeKind` and
converted nothing. Phase 2 adds the conversion. **Purely additive**: no existing member changed, no
existing result moved, `sizeof(DateTime)` is still 16.

> **Post-#1941 ripple audit, 2026-08-22.** “Purely additive” accurately describes this phase-2
> commit, but it was not a sufficient final-state claim. Existing arithmetic, `Now`, `Today`,
> `UnixEpoch`, `DateTimeOffset`, `TimeZoneInfo`, filesystem timestamps, and Calendar's opposite
> Kind contract still needed reconciliation once Kind became observable. Ticket #2418 completed
> that bounded consumer audit; see `Migration-DateTimeKindPropagation.md` and the two component
> migration documents it links.

**Downstream, measured: zero** `ToLocalTime`/`ToUniversalTime` sites in `cna` and in
`mobile-eggbert`. **This unblocks #1942, #1943 and #1944.**

---

## 1. The recorded blocker looked at the wrong type

#1941's record blocked the conversion phase on *"a date-sensitive timezone/DST model"*, and the
natural place to look is `TimeZoneInfo`. **Measured, that type cannot supply one**:
`GetUtcOffset(const DateTime&)` **ignores its argument** and returns the zone's standard offset, and
`IsDaylightSavingTime`, `IsAmbiguousTime` and `IsInvalidTime` **always return false** — all
documented as limitations on the type itself, with `GetAdjustmentRules()` a **permanent** deviation
since #2185.

**`System::TimeZone::CurrentTimeZone()` is the one that is per-date.** On POSIX it resolves both
local wall-clock questions and UTC-instant conversion questions from the process zone. Those are
not interchangeable around a DST transition: a `Utc` DateTime passed to the public
`TimeZone::GetUtcOffset` returns zero, while `DateTime::ToLocalTime` uses the zone's separate
UTC-instant resolver to select the offset that applies at that instant. Ticket #2418 split the two
after finding that phase 2 had passed UTC fields through the wall-clock resolver.

## 2. The shape, and the deviation that came with it

SA-15.1 chose **an abstraction in `Core.Base`** over moving `TimeZoneInfo` — the latter is not
header-only, carries two exception types and a 270-line private POSIX header, and would put tzdata
reading under **every** `Core.Base` consumer.

```cpp
// Core.Base — a narrow conversion boundary, deliberately not a zone in miniature
class ILocalTimeZone {
    virtual TimeSpan GetUtcOffset(const DateTime& time) const = 0;
    virtual TimeSpan GetUtcOffsetFromUniversalTime(const DateTime& time) const {
        return GetUtcOffset(time); // fixed-offset implementations remain source-compatible
    }
    virtual bool IsDaylightSavingTime(const DateTime& time) const = 0;
};
```

`System::TimeZone` implements it. Phase 2 originally cost no new member because its two public
members already matched. The #2418 boundary repair adds the UTC-instant member with a default so
fixed-offset implementations remain source-compatible; date-sensitive implementations override
it. This does change the interface vtable, so consumers must rebuild the static library and their
derived implementations.

**The deviation, stated plainly**: .NET's `ToLocalTime()` takes **no argument**, because it reaches
`TimeZoneInfo.Local` directly. `Core.Base` has nothing to ask, so the zone is a **parameter**:

```cpp
dt.ToLocalTime(System::TimeZone::CurrentTimeZone());
```

The alternative was a **registration hook** — hidden global state with a
static-initialisation-order dependency — and it was rejected for the reason #1940 gave when
`GetInstance(nullptr)` resolved to the invariant info: *a caller who wants the current one passes
it*. **The no-argument forms stay absent**, and phase 1's absence pin, which names exactly those,
still holds.

## 3. The rules, and the asymmetry that is .NET's

| Input `Kind` | `ToLocalTime` | `ToUniversalTime` |
|---|---|---|
| `Local` | returned unchanged | converted (subtract) |
| `Utc` | converted (add) | returned unchanged |
| `Unspecified` | **treated as UTC** | **treated as local** |

`DateTime.cs:1707` tests only the `Local` bit and `:1772` returns early only for `Utc`, so
`Unspecified` goes the opposite way in each direction. **A repair that "harmonised" them would pass
every ordinary case and fail only here**, which is why each direction has its own mutation.

On POSIX, `ToLocalTime` resolves the offset from the UTC instant (`timegm` followed by
`localtime_r`); `ToUniversalTime` resolves an Unspecified/Local wall clock with `mktime`. This
distinction is observable on all four sides of a DST transition. The two Core current-time readers
and the TimeZone component also share one mutex, because `FindSystemTimeZoneById` temporarily
changes the process-global `TZ` value and an otherwise reentrant `localtime_r` call can still see
that transient zone.

**Overflow clamps rather than throws** (`:1718-1721`), at both ends.

## 4. What remains absent after the later follow-ups

- **`LocalAmbiguousDst`** — phase 1 transcribed the reserved fourth encoding and phase 2 still does
  not produce it. Producing it needs an *ambiguity* answer, and this port's
  `TimeZoneInfo::IsAmbiguousTime` is documented as always `false`; inventing one would fabricate a
  distinction the runtime cannot make.
- **Implicit-zone conversion in general `Parse`** — the older invariant grammar consumes a zone
  suffix but returns Unspecified because Core.Base cannot obtain a current zone. #1942's
  `ParseExact` follow-up supports `z`/`K` plus the approved kind-affecting `DateTimeStyles`, taking
  `ILocalTimeZone` explicitly when conversion is required. `XmlConvert` owns the full XSD marker
  round trip in the XML component, which can reach `TimeZone::CurrentTimeZone()`.
- **The no-argument `ToLocalTime()`/`ToUniversalTime()`** — §2.

## 5. Testing

Seven mutations, **all caught**. The one worth recording is the last: a clamp written `< -1` instead
of `< 0` still clamps **every ordinary underflow** and differs on **exactly one input**, so a first
run reported it **NOT CAUGHT**. A zone with a **one-tick** offset makes that boundary reachable, and
the mutation is then caught. A boundary that is one tick wide needs an input one tick wide.
