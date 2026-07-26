# Audit: `modules/core/tests/System/DateTimeTests.cpp`

## Metadata

- Audit status: AUDITED (639 lines, 93 tests, full read).
- Validation: the focused DateTime/DateTimeOffset run passed all 127 selected
  tests.

## Assessment

The suite has strong coverage of tick bounds, arithmetic overflow guards,
leap-year/month behavior, parsing and formatting happy paths, and prior
fractional-second regressions.  It verifies only bad ticks and calendar-date
components; no test supplies an invalid hour, minute, second, or millisecond.

Its negative parsing coverage checks only a wholly unrelated string and bad
month.  It does not reject trailing text or a syntactically partial time, so
the green suite cannot detect either source finding.

## Finding references

- **SR-AUD-006:** missing assertions for time-component validation permit an
  out-of-range `DateTime` invariant breach.
- **SR-AUD-007:** missing negative parser assertions permit false success for
  malformed suffixes and failed time fields.

## Required post-audit assertions

Use `EXPECT_THROW` for each invalid time component at both a regular date and
the 9999 upper boundary.  Add `EXPECT_FALSE`/`EXPECT_THROW` pairs for
`"2024-06-15junk"` and `"2024-06-15 10:xx:00"`; verify that a failed
`TryParse` preserves a sentinel output value.

## Final assessment

The existing 93 passing tests give useful regression coverage but miss the
two confirmed validation contracts in the implementation.
