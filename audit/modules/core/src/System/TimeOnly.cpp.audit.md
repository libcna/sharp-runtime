# Audit: `modules/core/src/System/TimeOnly.cpp`

## Metadata

- Audit status: AUDITED (123 lines, full read).
- Implementation: millisecond TimeOnly conversion, arithmetic, fixed numeric
  parsing, and custom formatting.
- Validation: the focused TimeOnly/TimeSpan filter passed 134 tests.  An
  isolated linked probe reproduced the malformed input acceptance below.

## Assessment

Modulo-day arithmetic and conversion follow the documented millisecond model.
The parser verifies numeric field ranges but not that its input is fully
consumed or that a decimal point is followed by fractional digits.

## Findings

### SR-AUD-009 — medium — TimeOnly `TryParse` reports success for malformed fixed-format input

`TryParse` uses `%d:%d:%d` to parse a prefix (line 38), then scans at most
three fractional digits but never rejects residual characters (lines 46–54).
It also accepts a decimal point with zero digits.

**Reproduction (observed in a linked probe):**

```cpp
TimeOnly result;
TimeOnly::TryParse("10:20:30.abc", result);
// observed: true; result is 10:20:30.000
```

`"10:20:30junk"`, `"10:20:30."`, and
`"10:20:30.1234"` similarly succeed, although the public header says the
supported grammar is exactly `HH:MM:SS` or `HH:MM:SS.fff`.

**Impact:** malformed user input is silently accepted and can lose a stated
fractional component.  This is another instance of date/time parser
false-success, but it has an independent implementation.

**Required post-audit verification:** add false/throw pairs for every input
above, assert exact three-digit handling for the deliberately limited grammar,
and assert output preservation after failed `TryParse`.

## Positive findings

Invalid component constructors, day wrap, and `Add(TimeSpan)` near TimeSpan's
extreme values are directly covered.

## Final assessment

Normal clock arithmetic is sound; parser full-consumption validation needs
remediation (SR-AUD-009).
