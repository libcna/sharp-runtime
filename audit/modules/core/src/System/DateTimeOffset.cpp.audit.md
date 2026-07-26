# Audit: `modules/core/src/System/DateTimeOffset.cpp`

## Metadata

- Audit status: AUDITED (432 lines, full read).
- Implementation: offset validation, UTC conversion, arithmetic, Unix time,
  parsing, formatting, and equality for `System::DateTimeOffset`.
- Validation: focused Core.Base test filter passed its 34 dedicated tests and
  93 DateTime tests.

## Assessment

Offset construction correctly checks whole minutes, ±14-hour bounds, and UTC
range; arithmetic delegates to checked DateTime methods.  The documented
current-offset approximation for `ToLocalTime` is explicit.  Its parser is
not strict enough for the contract it exposes.

## Findings

### SR-AUD-006 — high — DateTimeOffset component constructors propagate invalid DateTime values

Both component constructors directly delegate to `DateTime(year, month, day,
hour, minute, second[, millisecond])` (lines 76–85).  Since that callee does
not validate time fields, DateTimeOffset shares the invalid-normalization and
out-of-range-state path in SR-AUD-006.  Offset range checking cannot repair a
clock time that was already accepted incorrectly.

**Required post-audit verification:** assert that invalid hour/minute/second/
millisecond inputs throw from both DateTimeOffset component constructors,
including a `9999-12-31 24:00` boundary case.

### SR-AUD-007 — medium — DateTimeOffset parser accepts impossible offset minutes and malformed DateTime text

For a textual offset, lines 268–270 use `%d:%d` but do not constrain the
minute field to 0–59 or require full input consumption.  The constructed
`TimeSpan` therefore normalizes impossible minute values before the
whole-minute/±14-hour constructor validation.  It also delegates its date
portion to DateTime's permissive parser.

**Reproduction:**

```cpp
DateTimeOffset value;
DateTimeOffset::TryParse("2024-06-15T10:30:00+02:75", value);
// currently true, with a +03:15 offset; must be false

DateTimeOffset::TryParse("2024-06-15T10:xx:00+02:00", value);
// currently true, because DateTime turns the failed time parse into midnight
```

`+02:75` is neither a valid ISO-8601 offset nor a valid .NET DateTimeOffset
parse input.  The existing check for offsets beyond ±14 hours does not cover
this normalization path.

**Required post-audit verification:** add false/throw pairs for minute 60/75,
trailing offset garbage, and malformed date-time fields; verify a failed
`TryParse` preserves a sentinel output value.

## Positive findings

The `TryParse` constructor call is caught so an out-of-range but syntactically
valid `+15:00` returns false rather than leaking an exception.  RFC1123 and
universal formats correctly convert to UTC before emitting their UTC marker.

## Final assessment

Construction and conversion are mostly well defended, but parser strictness
and inherited DateTime validation leave confirmed contract failures
(SR-AUD-006 and SR-AUD-007).
