# Audit: `modules/core/tests/System/ArraySegmentNewTests.cpp`

## Metadata

- Audit status: AUDITED (348 lines, 45 tests, fully read).
- Validation: `ArraySegmentTests.*` passed 45/45 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The suite has excellent coverage for ordinary construction, slicing, copying,
indexing, equality, and the prior integer-overflow range fix.  It exercises
only backed segments for behavior and only separate `int` containers for
copying.  Its destination-expansion and hash uniqueness assertions also encode
two behaviors that differ from the reference contract.

## Finding references

- **SR-AUD-018:** `GetHashCode_NonNegative` and
  `GetHashCode_DifferentOffset_DifferentHash` require properties that a valid
  hash function does not guarantee.
- **SR-AUD-043:** no test addresses vector sizes that cannot be represented by
  the signed `intcs` metadata held by ArraySegment.
- **SR-AUD-044:** all copy tests use distinct `int` ranges; no left/right
  same-vector `std::string` overlap checks exist for either CopyTo overload.
- **SR-AUD-054:** default-constructor tests inspect only properties.  They do
  not call Slice, CopyTo, ToArray, Contains, IndexOf, or range-for, so a null
  dereference/silent no-op instead of `InvalidOperationException` remains
  invisible.
- **SR-AUD-055:** `CopyTo_VectorWithOffset_ExpandsDest` asserts the local
  resize behavior rather than the .NET fixed-destination capacity contract.

## Required post-audit verification

After a default-state guard is introduced, add deterministic exception checks
for every operation that requires an underlying array and run their focused
filter under ASan/UBSan.  Add overlapping nontrivial copies in both directions,
plus ordinary short-vector capacity checks that verify no resize/write before
the exception.  Replace hash uniqueness/nonnegative assertions with only the
required equality-implies-equal-hash contract.

## Final assessment

The 45-test suite protects the prior range work well, but it omits the
default-state, aliasing, capacity, and valid-hash boundaries that expose the
confirmed findings.  No test was modified during this audit.
