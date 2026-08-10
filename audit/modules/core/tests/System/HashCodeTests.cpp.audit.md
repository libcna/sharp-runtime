# Audit: `modules/core/tests/System/HashCodeTests.cpp`

## Metadata

- Audit status: AUDITED (76 lines, 11 tests, fully read).
- Validation: `HashCodeTests.*` passed 25/25 in `SharpRuntimeTests_Core_Base`
  on 2026-07-25; 14 of those registrations are in the separately pending
  `SystemTypesRemainingTests.cpp` and are not treated as this file's review.

## Assessment

The dedicated suite covers normal accumulation, order sensitivity, all eight
`Combine` arities at least once, vector/span equality, and within-process seed
consistency.  It exercises only valid byte views, so it cannot reveal the
negative-span length conversion that produces an ASan-confirmed over-read.
Several assertions also overstate the hash contract by requiring distinct
inputs and an empty accumulator to produce nonzero/distinct values.

## Finding references

- **SR-AUD-043:** no test constructs `ReadOnlySpan<uint8_t>(data, -1)` or
  otherwise verifies validation before `AddBytes` converts a signed span length
  to `size_t`; the independently compiled ASan probe reports a stack-buffer-
  overflow.
- **SR-AUD-018 (extended):** `ToHashCode_DefaultIsNonZero`,
  `Add_DifferentValues_DifferentResult`, and `Combine2_OrderMatters` require
  nonzero/hash-unique results for particular unequal inputs.  Hashing permits
  collisions and does not promise a nonzero value; these tests can reject a
  valid algorithm or an allowed seed result.

## Required post-audit verification

Add exception tests for negative span construction and for `HashCode::AddBytes`
before and after the associated `Span` repair.  Run that case under ASan.  Keep
same-input determinism and equality-comparer tests, but replace hash-uniqueness
assertions with contract-valid equality/non-equality assertions or optional,
non-normative distribution tests.

## Other missing assertions and diagnostics

- The test named `Seed_DiffersAcrossProcessesButConsistentWithinOne` only
  demonstrates the second half; it cannot and does not launch a second process.
- No direct test documents the raw-pointer overload's null/length contract.
- The suite provides no custom type with a collision-prone `std::hash`, so it
  cannot guard against reintroducing forbidden uniqueness assumptions.

## Final assessment

Normal hashing tests are useful but malformed-span coverage is absent and some
assertions confuse a desirable distribution property with the hash contract.
No test was modified during this audit.
