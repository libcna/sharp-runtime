<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `DateTimeOffset` after `DateTimeKind` — post-#1941 ripple audit

`DateTime` began storing and converting by `DateTimeKind` in #1941. `DateTimeOffset` still
described the kind as absent and retained its earlier kind-blind behaviour, so the two types
disagreed at every DateTime-taking boundary.

## Repaired contract

- `DateTimeOffset(DateTime)` chooses offset zero for `Utc`; `Local` and `Unspecified` choose the
  supported process-local offset.
- `DateTimeOffset(DateTime, TimeSpan)` rejects a nonzero offset for `Utc` and an offset unequal to
  the supported process-local offset for `Local`. `Unspecified` accepts every otherwise-valid
  offset. Kind consistency is checked before offset precision/range, matching .NET's observable
  exception order.
- `DateTime` is always returned with `DateTimeKind::Unspecified`, `UtcDateTime` with `Utc`, and
  `LocalDateTime` with `Local`. The stored clock is normalized to `Unspecified`, preserving the
  existing const-reference getter and object layout.
- `UtcNow` and `Now` read `system_clock` directly. This is load-bearing after `DateTime::Now`
  becomes a Local wall clock: treating those ticks as UTC makes `UtcNow` local-with-zero and makes
  `Now` add the offset twice.

Internal `ToOffset` and `ToLocalTime` paths construct their shifted clock as `Unspecified`
explicitly. They therefore remain valid even when `DateTime` arithmetic preserves Kind; a public
`Utc` DateTime plus a nonzero offset is correctly rejected. `ToOffset` retains DateTime's checked
addition for a full-range `TimeSpan`, avoiding signed-overflow UB before offset validation, and
`ToLocalTime` clamps a shifted clock at DateTime's two bounds as .NET's non-throwing path does.

## Accepted practical-subset boundary

.NET resolves a Local DateTime against the local timezone rules for that particular date. Doing
that here would require the date-sensitive implementation owned by the `TimeZone` component,
which already depends on `Core.Base`; calling it from `DateTimeOffset` would create a component
cycle. Moving or duplicating that resolver is deliberately outside this narrow ripple repair.

The supported model remains the process's **current** UTC offset, read privately in
`DateTimeOffset.cpp`. The one-argument constructor, Local-kind validation, `Now`, `ToLocalTime`,
and `LocalDateTime` all use that same model. Historical/future DST-by-date parity is therefore a
named limitation, not an implied claim. On Windows the current reader includes the active
`StandardBias` or `DaylightBias`; using `Bias` alone would be wrong for part of every DST year.

## Deterministic evidence

The Core.Base tests select the fixed POSIX zone `UTC-02` (UTC+02:00) with an RAII guard that
restores `TZ`. They pin all three constructor Kind branches, mismatch exception order and text,
the three DateTime property Kinds, internal clock normalization, and agreement between `Now` and
`UtcNow` on the represented instant. Fixed UTC+14 and UTC-14 cases additionally pin both local
conversion clamps, and full-range TimeSpan cases pin `ToOffset`'s checked arithmetic. The fixed
zones need no installed tzdata and deliberately test the supported current-offset contract rather
than pretending to test date-sensitive DST.
