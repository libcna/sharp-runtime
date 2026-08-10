# Audit: `modules/time-zone/tests/System/TimeZoneInfoTests.cpp`

## Metadata

- AUDITED: 608-line TimeZoneInfo fixture, 80 tests, fully read.
- Validation: `TimeZoneInfoTests.*` passed 80/80 on 2026-07-27.

## Coverage observed

The fixture provides useful UTC, normal IANA lookup, ordinary fixed-offset
conversion, mapping, transition validation, and basic adjustment/equality
coverage.  It runs only happy paths or weak range checks for the audited
metadata contracts.

## Missing assertions and diagnostics

- Failed TryFind result clearing, empty/invalid custom IDs, ±14-hour and
  whole-minute offsets, and case-insensitive equality are absent, permitting
  SR-AUD-224/225/227.
- It never compares two same-current-offset but different-rule zones or the
  invariant standard BaseUtcOffset across a DST season, permitting
  SR-AUD-228/229.
- All DST transition/ambiguity APIs are asserted only for UTC, so the explicit
  native subset is not distinguished from accidental failures on real zones.

## Final assessment

The 80 green tests do not detect SR-AUD-224, SR-AUD-225, or SR-AUD-227 through
SR-AUD-229.  No source or test was changed.

---

## Remediation record — tickets #2177–#2186 (2026-08-10)

The suite grew from **114 to 187 tests** across the component. `TimeZoneInfoTests.cpp` gained the
assertions the reports above asked for (failed-`TryFind` clearing, factory validation, adjustment-rule
ranges on both overloads, case-variant equality and hash agreement, the standard-offset matrix over
sixteen real zones chosen for distinct properties, the identifier and non-TZif-file matrices, and the
`DateTime` range boundaries), plus six `PIN_` tests holding the behaviours tickets #2185 and #2186
defer. `AdjustmentRuleTests.cpp` gained 8 and `TimeZoneTests.cpp` gained 14.
