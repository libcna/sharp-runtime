# Audit: `modules/time-zone/tests/System/TimeZoneTests.cpp`

## Metadata

- AUDITED: 71-line legacy TimeZone fixture, eight tests, fully read.
- Validation: `TimeZoneTest.*` passed 8/8 on 2026-07-27.

## Coverage observed

The fixture covers custom virtual dispatch, a fixed two-hour custom offset,
current-zone non-null/name/reference stability, and polymorphic access.  It
does not make any date-sensitive assertion against the production local
adapter.

## Missing assertions and diagnostics

- Add deterministic New York (or another DST-zone) January/July offset and
  IsDaylightSavingTime assertions; their absence permits SR-AUD-223.
- Add CurrentTimeZone cache/reset/configuration boundaries, local invalid and
  ambiguous time behavior, and actual TimeZoneInfo delegation diagnostics.

## Final assessment

The green happy-path suite cannot detect SR-AUD-223.  No source or test was
changed during this audit.

---

## Remediation record — ticket #2182 (2026-08-10)

Both gaps this report names are closed. `TimeZoneTests.cpp` now controls `TZ` through an RAII guard
that restores it exactly (distinguishing unset from empty-but-set), and exercises the adapter's
summer and winter branches, both 2025 transitions from four sides each, the repeated and
non-existent local hours, a southern-hemisphere zone, a zone with no daylight time, and the
`DateTime` range extremes. The one-time static cache is addressed by resolving per call rather than
by asserting a cached value, so no test depends on which zone was in force at first use.
