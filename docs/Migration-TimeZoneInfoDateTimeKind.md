<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `TimeZoneInfo` consumes and produces `DateTimeKind`

*2026-08-22.* Ticket #1941 made `DateTimeKind` real. `TimeZoneInfo` still contained the earlier
world's assumptions: two validators said Kind did not exist, conversion helpers constructed every
result as Unspecified, and several overloads used `DateTime::Add` as an accidental Kind-erasing
step. Once DateTime arithmetic correctly preserves Kind, that last dependency also makes a
`DateTimeOffset` conversion to a non-zero offset fail its valid Utc/offset consistency check.

This ripple repair changes no `TimeZoneInfo` object layout and keeps its documented fixed-offset,
no-transition model. It makes the existing model Kind-correct.

## Validation

- `TransitionTime` requires `timeOfDay.Kind == Unspecified`. Utc and Local are rejected with
  `ArgumentException` naming `timeOfDay`.
- Public `AdjustmentRule` factories require Unspecified `dateStart`/`dateEnd`; both Utc and Local
  are rejected with the offending parameter name. Internal .NET runtime factories have a broader
  UTC-boundary shape, but it is not part of this public surface.
- Public `AdjustmentRule` factories reject identical start/end transitions after the two Kind
  checks and before effective-date ordering, as current .NET does. The former acceptance test was
  not a supported no-transition spelling: that state is available only to .NET's internal
  `noDaylightTransitions` factory.

## Conversion matrix

Every `DateTime`-returning conversion now chooses the result Kind from the destination object:

| Destination | Result Kind |
|---|---|
| the canonical `TimeZoneInfo::Utc()` singleton | `Utc` |
| the canonical `TimeZoneInfo::Local()` singleton | `Local` |
| any other system or custom zone | `Unspecified` |

The test is deliberately reference identity, not id/offset equality. A custom zero-offset zone
called `"UTC"` remains an arbitrary zone and produces Unspecified.

Lookup preserves .NET's deliberately asymmetric identities. UTC lookup is ASCII
OrdinalIgnoreCase, so `FindSystemTimeZoneById("UTC")` and `FindSystemTimeZoneById("uTc")`, as well
as the UTC entry in `GetSystemTimeZones()`, refer to the canonical `Utc()` singleton, while a Local
lookup is a copy distinct from `Local()`. Consequently a by-id `"UTC"` destination produces Utc,
but a by-id `"Local"` destination produces Unspecified; only explicitly passing `Local()` produces
Local. .NET's cache contains comments requiring both halves: deduplicate UTC because conversions
use reference equality, and clone Local specifically to break reference equality with its
equivalent database zone. A non-owning `shared_ptr` view preserves the static UTC object's identity
without changing this port's lookup signature or `TimeZoneInfo` layout.

`ConvertTime(DateTime, destination)` now reads Utc input from Utc and Local/Unspecified input from
Local, matching .NET. The explicit-source overloads reject a non-Unspecified input whose Kind does
not correspond to the source zone. Consequently `ConvertTimeFromUtc` rejects a Local input, and
all `ConvertTimeToUtc` spellings return Utc.

Overflow still clamps once, after the raw source-to-UTC-to-destination tick calculation. The
low-level `safeFromTicks(ticks, kind)` matches .NET's subtle boundary contract: it applies `kind`
to an in-range construction, while an overflow returns the Unspecified `MinValue`/`MaxValue`
constant. The public conversion path then stamps the destination Kind onto those clamped ticks,
so a clamped `ConvertTimeToUtc` result is still Utc.

The `DateTimeOffset` overload now computes its destination wall-clock ticks directly rather than
passing a Utc `DateTime` through Kind-preserving arithmetic. Values beyond either DateTime boundary
saturate to the zero-offset `DateTimeOffset::MinValue`/`MaxValue`, while an exact boundary retains
the destination offset. This is both Kind-safe (the tick constructor creates an Unspecified clock)
and matches the separate clamping contract of .NET's DateTimeOffset conversion overload.

## Compatibility impact

Code that supplied Local `DateTime` values with a non-Local explicit source now receives
`ArgumentException`, as does Utc with a non-Utc explicit source. Code that inspected conversion
results sees meaningful Utc/Local Kinds instead of Unspecified. Tick arithmetic is unchanged
inside the supported fixed-offset model, except that the one-destination overload now interprets
Local and Unspecified input through `TimeZoneInfo::Local` rather than assuming UTC.
