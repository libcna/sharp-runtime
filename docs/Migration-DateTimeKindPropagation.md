<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Final post-#1941 `DateTimeKind` propagation audit — #2418

*2026-08-22.* The final audit reconciliation correctly closed all 364 findings that existed in its
index, but that did not prove that a later feature could not introduce a new ripple. Ticket #1941
made `DateTimeKind` observable in two phases. Several older consumers still implemented or
documented the earlier kindless world, and the phase-1 test deliberately pinned arithmetic to
`Unspecified` even after kind-aware conversion had landed.

Ticket #2418 is the bounded follow-up audit of that feature. It is tracked as a new planning item,
not retroactively hidden under one of the frozen 364 audit ids.

## Corrected contracts

### `DateTime`

- `Now` now contains local wall-clock ticks and reports `Local`; `Today` is that local date at
  midnight and also reports `Local`.
- `UnixEpoch` reports `Utc`.
- every DateTime-returning arithmetic spelling preserves the complete internal kind encoding:
  `Add`, all unit `Add*` members, `AddMonths`, `AddYears`, `Subtract(TimeSpan)`, and `+`/`-`.
  Equality, ordering, hashing, formatting, component access, and DateTime-to-DateTime subtraction
  continue to compare or expose ticks only.

### `DateTimeOffset`

- the DateTime-taking constructors choose and validate offsets from Kind, then normalize the
  exposed clock to `Unspecified`;
- `DateTime`, `UtcDateTime`, and `LocalDateTime` report `Unspecified`, `Utc`, and `Local`;
- `Now`/`UtcNow`, `ToOffset`, and `ToLocalTime` no longer depend on DateTime arithmetic erasing
  Kind.

The detailed constructor matrix and its current-offset limitation are recorded in
`Migration-DateTimeOffsetKindRipple.md`.

### `TimeZoneInfo`, `TransitionTime`, and `AdjustmentRule`

- conversion results use `Utc` for the canonical UTC destination, `Local` for the canonical Local
  destination, and `Unspecified` for every other zone;
- an explicit source zone must agree with any non-Unspecified input Kind;
- `ConvertTimeFromUtc` rejects Local input, and all `ConvertTimeToUtc` spellings return Utc even at
  a clamped boundary;
- transition clock values require `Unspecified`;
- public adjustment-rule factories require **Unspecified** boundaries. Utc and Local are both
  rejected before the other rule validations; the broader Utc boundary shape belongs only to
  internal runtime factories, not `CreateAdjustmentRule`.

The complete destination matrix, canonical-object identity rule, and clamp behavior are recorded
in `Migration-TimeZoneInfoDateTimeKind.md`.

## Second-order consumers checked

| Consumer | Disposition |
|---|---|
| `System::Globalization::Calendar` | Its `Add*` contract is intentionally the opposite of `DateTime`: results are always Unspecified. Delegating base members now strip Kind explicitly; calendar-specific month/year implementations already construct Unspecified values. |
| `System::IO::FileSystemInfo` | Raw `*TimeUtc` producers report Utc, local getters report Local, and the local/UTC setters apply the three input-Kind branches rather than treating every input as Local. |
| `System::Xml::XmlConvert` | The implementation already writes and reads XSD Kind markers and uses the date-sensitive `TimeZone` provider available to the XML component. Stale comments and migration text claiming RoundtripKind was still indistinguishable were corrected. |
| `System::TimeZone` | Its public `GetUtcOffset(Utc)`/`IsDaylightSavingTime(Utc)` now return zero/false, while `DateTime::ToLocalTime` uses a distinct UTC-instant offset query. The POSIX and Windows implementations resolve both sides of DST transitions correctly instead of interpreting UTC fields as a local wall clock; POSIX core current-time readers and temporary `TZ` selection share one mutex. |
| `TimeProvider` / timers | `GetUtcNow` delegates to the repaired `DateTimeOffset::UtcNow`; `GetLocalNow` already constructs an offset clock as Unspecified. Timer event times now inherit the corrected Local `DateTime::Now`. |
| cookies / HTTP date consumers | Cookie expiry is a UTC-timeline comparison in .NET. `Expires` is converted by Kind before comparison with UtcNow; HTTP `Expires` and `Max-Age` values are stored as Utc. This requires one new private `Net -> TimeZone` edge, moving the graph to 41 / 96. |
| exact parsing / formatting | The style-taking exact parsers own kind-aware `z`/`K` behavior through an explicit zone parameter. `XmlConvert` now routes both format-taking doors through them with its required outer-whitespace styles, so explicit DateTimeOffset offsets are captured instead of overwritten with the local offset. The general parser and local DateTime `K` limitation below remain explicit. |

The canonical `TimeZoneInfo::Utc()` function-static and zero-offset platform fallbacks construct
their `TimeSpan` in place. They do not copy the cross-translation-unit `TimeSpan::Zero` global:
`Utc()` is legal from a consumer's pre-main initializer, where such a copy would otherwise have an
undefined static-initialization order. A pre-main regression exercises that entry under UBSan.

`XmlConvert` also keeps the XSD lexical boundary narrower than the general date parser. Only an
upper-case `Z` or an exact `+/-hh:mm` suffix is a timezone marker; lower-case `z` and compact or
variable-width spellings such as `+8`, `+2:5`, `+800`, and `+0800` now fail at both DateTime and
DateTimeOffset doors instead of being accepted by the broader parser with their offset discarded.

## Remaining intentional subset boundaries

These are declared limitations, not forgotten follow-ups:

- Core.Base cannot call upward into `TimeZone` or `TimeZoneInfo`. The no-argument .NET
  `DateTime.ToLocalTime()` / `ToUniversalTime()` forms remain absent; callers pass an
  `ILocalTimeZone`.
- General invariant `DateTime::Parse` consumes a trailing zone designator but returns
  `Unspecified`; kind-aware conversion is available through the explicit-zone `ParseExact`
  surface, while `XmlConvert` owns XSD round trips.
- `DateTimeOffset` operations that need the process-local zone use its **current** offset rather
  than resolving historical/future DST rules for the value's date. This was already the class's
  documented practical-subset model and is now applied consistently.
- Core's custom `K` formatter cannot emit a numeric marker for Local because its signature carries
  no zone; Utc emits `Z` and Unspecified/Local emit no marker. XML rendering, which can reach a
  zone, writes the local numeric offset.
- `TimeZoneInfo` remains the documented fixed-offset/no-adjustment-rule conversion model. The
  date-sensitive process-local implementation continues to live in `System::TimeZone`.
- `LocalAmbiguousDst` is still not produced. UTC-to-local conversion chooses the correct offset on
  both occurrences of a repeated hour, but the two results have identical visible local ticks and
  Kind. Converting the daylight occurrence back from fields therefore selects the standard
  occurrence; exact ambiguous-hour round trips require the hidden fourth DateTime encoding.

## Audit and planning identity

The authoritative audit index remains **343 remediated / 19 accepted-deviation / 2 false-positive
= 364**. Those counts answer the status of the indexed audit; #2418 records the newly discovered
post-feature ripple. The three externally blocked items (#1773, #2381, #1962) are unchanged and
no external repository was modified.
