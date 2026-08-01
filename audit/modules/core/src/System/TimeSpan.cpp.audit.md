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

---

**REMEDIATED 2026-07-30 (ticket #1836, CCF-004).** The text above is retained exactly as the
audit wrote it. Four corrections belong beside it, all by measurement
(`build-probe/1836_prefix.log`, `build-probe/1836_postfix.log`, one process per case against an
instrumented `build-asan` tree; recorded in full as
`docs/DefinedArithmeticBoundaryPlan.md` section 17):

1. **The parse half is four undefined columns, not one.** As well as `days * TicksPerDay`
   (`:454:53`), the same statement overflowed at `:454:16` and `:457:22` (two of the five-term
   accumulations) and at `:459:29` (`ticks = -ticks`, negating the int64 minimum). A repair
   aimed only at the day product would have left three live.
2. **A fifth undefined operation, from the C library.** `std::sscanf`'s `%d` conversion of a
   value the target cannot represent is undefined (C17 7.21.6.2p10) and was measured wrapping:
   `"2147483648.00:00:00"` reached the tick arithmetic as `-2147483648` and
   `"99999999999999999999.00:00:00"` as `-1`, the latter parsing *successfully* as minus one
   day. Components are now read as `long long` behind an eighteen-digit run limit.
3. **Two further silent wrong answers that UBSan reports nothing for.** `"--5.00:00:00"` was
   accepted and returned the **positive** five-day duration (the two sign inversions cancel;
   nothing overflows), and `"-10675199.02:48:05.4775809"` returned the **positive** `MaxValue`.
   A diagnostic list is not a defect list.
4. **`Parse` did not throw either.** The audit's suggested outcome
   ("`FormatException`/overflow from `Parse`") describes what `Parse` *should* have done; it
   was measured **returning** the wrapped duration. It now raises `OverflowException` with
   .NET's `SR.Overflow_TimeSpanElementTooLarge` text for an out-of-range component, and keeps
   `FormatException` for a malformed string.

`Subtract` was exactly as described and is class A: the sign-bit guard already produced the
correct `OverflowException`, so only the undefined subtraction was replaced (unsigned, converted
back), and every value and message is byte-identical. `Add`, `Negate()` and `operator-()` were
inventoried at the same time and validate before computing — they were never defective.

Public doors, all pinned by tests: `Subtract`, `operator-(TimeSpan)`, `TryParse`, `Parse`, and
`System::Xml::XmlConvert::ToTimeSpan` in another module. `TimeSpan.hpp` is unchanged; the shared
parse core has internal linkage.

## Positive findings

Trailing-garbage parsing and NaN handling have direct regressions, and the
six-component constructor has an explicit extreme-day overflow test.

## Final assessment

The routine implementation is mature, but SR-AUD-008 is a high-severity input
and arithmetic correctness defect.

---

## Post-audit subset remediation — #1929 row 5 (2026-08-01)

The SR-AUD-008 repair and its format/overflow taxonomy remain unchanged.
Exact approval under `docs/TextSubsetCompatibilityDecision.md` §6.5 item (3)
makes the shared parse core trim surrounding invariant whitespace before its
existing grammar and range checks. The ordinary no-whitespace path allocates
nothing; only actually trimmed input needs a NUL-terminated temporary for the
existing scanner. Seven fraction digits, MinValue/MaxValue, malformed input and
overflow taxonomy retain their prior values/status. No declaration, layout,
symbol or exception specification changed; SR-AUD-008's status is unchanged.
