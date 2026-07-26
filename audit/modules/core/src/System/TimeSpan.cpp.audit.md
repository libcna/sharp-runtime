# Audit: `modules/core/src/System/TimeSpan.cpp`

## Metadata

- Audit status: AUDITED (529 lines, full read).
- Implementation: duration construction, checked arithmetic, formatting, and
  fixed invariant parsing.
- Validation: `./build/SharpRuntimeTests_Core_Base --gtest_filter='TimeOnlyTests.*:TimeSpan*.*' --gtest_color=no`
  passed 134 tests.  An isolated linked probe reproduced the malformed large
  parse below.

## Assessment

Several paths deliberately avoid C++ signed-overflow undefined behavior,
including six-component construction and `Add`.  The parse and subtraction
paths remain inconsistent with that care.

## Findings

### SR-AUD-008 — high — TimeSpan parse can return success with a wrapped, wrong duration

`TryParse` stores the day field in `int` and, after lexical validation, forms
ticks using direct signed multiplications/additions (lines 455–459).  There is
no range check before `days * TicksPerDay`, even though `int` accepts day
values far beyond TimeSpan's representable range.  This is signed-overflow
undefined behavior in C++ and currently becomes a wrapped value.

**Reproduction (observed in a linked probe):**

```cpp
TimeSpan result;
const bool parsed = TimeSpan::TryParse("2147483647.00:00:00", result);
// observed: parsed == true, result.getTicksProperty() == -7695280436664713216
```

The input is well beyond TimeSpan's maximum duration and must produce false
from `TryParse` (or `FormatException`/overflow from `Parse`), not a negative
duration unrelated to the input.

The nearby `Subtract` method has a related correctness hazard: it first
evaluates `ticks_internal - ts.ticks_internal` (line 261) and only then
inspects sign bits for overflow.  Boundary subtraction currently throws in the
probe, but the signed subtraction has already invoked undefined behavior, so
the guard is not portable or optimization-safe.

**Impact:** untrusted textual duration input can be accepted as a false,
opposite-sign value.  Boundary arithmetic also relies on undefined behavior.

**Required post-audit verification:** add false/throw tests for a day count
above the maximum and for an otherwise valid maximum boundary; add
`MinValue.Subtract(TimeSpan(1))` and `MaxValue.Subtract(TimeSpan(-1))`
overflow tests under UBSan.  Repair must check range before multiplication and
before subtraction, or use an explicitly defined unsigned strategy.

## Positive findings

Trailing-garbage parsing and NaN handling have direct regressions, and the
six-component constructor has an explicit extreme-day overflow test.

## Final assessment

The routine implementation is mature, but SR-AUD-008 is a high-severity input
and arithmetic correctness defect.
