# Audit: `modules/core/tests/System/DecimalTests.cpp`

## Metadata

- Audit status: AUDITED (348 lines, 51 tests, full read).
- Validation: together with `DecimalNewTests.cpp` and `DecimalTests2.cpp`,
  `DecimalTests.*:DecimalTests2.*` passed 143/143 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This suite exercises ordinary parse, exact arithmetic, comparison, conversion,
rounding, and some large values well.  Its parse negatives are limited to
clearly malformed strings, so the public default grammar, range exception,
and precision boundary remain unobserved.  The assertion that `-Decimal::Zero`
equals zero is numerically correct but cannot test Decimal's separately
observable sign bit.

## Finding references

- **SR-AUD-035:** only simple valid literals, malformed input, and `"bad"`'s
  generic `FormatException` are covered.  Valid default whitespace/grouping,
  range overflow requiring `OverflowException`, and nearest rounding of an
  over-28-scale input are absent.
- **SR-AUD-038:** `NegativeZeroEqualsZero` and `UnaryMinus` check value
  equality only.  They cannot detect that the implementation clears the raw
  sign which `Decimal::GetBits` exposes.

## Required post-audit verification

Add exact parser vectors for `" 1,234.5 "`, one-above-MaxValue with an
`OverflowException` assertion, and a 29th fractional digit that rounds up.
Pair every negative-zero numeric-equality test with an explicit `GetBits`
flags assertion.

## Final assessment

The 51 tests provide useful arithmetic regression coverage, but normal-path
success obscures the parser and representation contracts confirmed in the
owning implementation report.
