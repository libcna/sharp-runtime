# Audit: `modules/core/tests/System/DecimalNewTests.cpp`

## Metadata

- Audit status: AUDITED (254 lines, 49 tests, full read).
- Validation: these tests register in the `DecimalTests` suite; the combined
  `DecimalTests.*:DecimalTests2.*` filter passed 143/143 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This supplemental suite supplies useful scale, comparison, static arithmetic,
and ordinary Clamp coverage.  Its Clamp cases all use a valid ordered interval,
and its parse-derived expectations exercise no whitespace/grouping, range, or
representation boundary.  Two hash-code expectations also assume more than
the equality/hash contract, but no additional finding is recorded here because
the tested concrete values are not independently shown incompatible with the
.NET Decimal implementation.

## Finding references

- **SR-AUD-022:** `Clamp_WithinRange`, `Clamp_BelowMin`, `Clamp_AboveMax`, and
  `Clamp_Fractions` never exercise `min > max`, so the public decimal
  `ArgumentException` contract remains untested.
- **SR-AUD-035:** scale tests use simple parser literals only; they do not
  cover default valid whitespace/grouping, overflow taxonomy, or precision
  rounding.
- **SR-AUD-038:** no test constructs or observes a raw negative zero through
  `GetBits`.

## Required post-audit verification

Add the invalid-range, parser-boundary, and raw negative-zero assertions
listed in the owning Decimal reports.  Keep hash-code tests limited to the
required implication that equal values have equal hashes unless a stable
.NET-compatible hash value is intentionally part of the public contract.

## Final assessment

These tests broaden basic Decimal coverage, but they leave all newly confirmed
boundary defects unobservable.  No implementation was modified during this
audit.
