# Audit: `modules/core/src/System/DateTime.cpp`

## Metadata

- Audit status: AUDITED (405 lines, full read).
- Implementation: calendar/tick conversion, arithmetic, formatting, and
  invariant numeric parsing for `System::DateTime`.
- Validation: `./build/SharpRuntimeTests_Core_Base --gtest_filter='DateTimeTests.*:DateTimeOffsetTests2.*' --gtest_color=no`
  passed 127 tests; source-level negative cases below are not represented by
  that filter.

## Assessment

Gregorian day calculation, leap-year logic, and overflow-conscious `AddTicks`
and `Subtract(TimeSpan)` are carefully implemented.  Two publicly reachable
validation paths are incomplete, however.

## Findings

### SR-AUD-006 — high — calendar constructors accept invalid time components and can violate the tick invariant

`dateToTicks` validates year, month, and day (lines 43–61), then directly adds
`hour`, `minute`, `second`, and `millisecond` (lines 66–69).  It performs no
range validation for any of those four inputs, despite the public constructor
documentation requiring all four ranges and an exception on violation.

**Reproduction:** each of the following constructor calls should throw
`System::ArgumentOutOfRangeException` but currently produces a normalized (or
out-of-range) tick value:

```cpp
DateTime(2024, 1, 1, 24, 0, 0);       // normalizes to next day
DateTime(2024, 1, 1, 0, 60, 0);       // normalizes to next hour
DateTime(2024, 1, 1, 0, 0, 60);       // normalizes to next minute
DateTime(2024, 1, 1, 0, 0, 0, 1000);  // normalizes to next second
DateTime(9999, 12, 31, 24, 0, 0);     // stores MaxTicks + 1, bypassing
                                       // DateTime(longcs) range validation
```

Negative components can likewise form negative ticks.  The component
constructors initialize `ticks_` directly, so the usual `DateTime(longcs)`
invariant check is never reached.

**Impact:** invalid user input is accepted, calendar semantics diverge from
.NET and the header, and a `DateTime` object can exist outside its documented
`[0, MaxTicks]` invariant.  DateTimeOffset component constructors delegate to
this path (see its implementation report).

**Required post-audit verification:** add four invalid-component assertions
for both signs/boundaries, an upper-bound invariant assertion for
9999-12-31 24:00, and equivalent DateTimeOffset constructor coverage.  Repair
must validate before computing ticks.

### SR-AUD-007 — medium — `TryParse` accepts malformed input and silently replaces invalid time text with midnight

The parser only checks date separator positions and uses permissive `sscanf`
conversions.  If a time separator is present but `%d:%d:%d` does not yield all
three fields, lines 305–306 replace every time field with zero and continue.
It also does not require date or time parsing to consume the full string.

**Reproduction:** these calls should return `false` (and `Parse` should throw
`FormatException`) but the implementation accepts them:

```cpp
DateTime value;
DateTime::TryParse("2024-06-15junk", value);       // accepted as date-only
DateTime::TryParse("2024-06-15 10:xx:00", value); // accepted as midnight
DateTime::TryParse("2024-06-15 trailing", value); // accepted as midnight
```

This is separate from the documented limited grammar: even that grammar does
not describe accepting trailing garbage or fabricating a valid time after a
failed conversion.  `DateTimeOffset::TryParse` inherits this behavior and adds
its own lax offset validation (see its report).

**Required post-audit verification:** add false/throw pairs for malformed
date suffixes and partially malformed time fields, with an assertion that a
failed `TryParse` does not overwrite the output argument.

## Positive findings

`TryParse` catches constructor failures, and the fractional-second digit scan
does not count a trailing timezone marker as a fractional digit.  These are
covered by existing regression tests.

## Final assessment

The arithmetic core is robust, but public construction and parsing validation
contain a high-severity invariant breach (SR-AUD-006) and a medium-severity
false-success parsing defect (SR-AUD-007).
