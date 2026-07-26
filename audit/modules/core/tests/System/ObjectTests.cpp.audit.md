# Audit: `modules/core/tests/System/ObjectTests.cpp`

## Metadata

- Audit status: AUDITED (250 lines, 34 tests, full read).
- Validation: `./build/SharpRuntimeTests_Core_Base --gtest_filter='ObjectTests.*:TypeTest.*' --gtest_color=no`
  passed all 34 Object tests on 2026-07-25.

## Assessment

The suite exercises default and overridden string/equality behavior, all
static null/reference-equality combinations, address-hash range/stability, heap
destruction through the base type, and dynamic RTTI through an `Object*`.
Those are meaningful checks for this small base class.

## SR-AUD-018 — low — a hash test forbids a valid collision for different values

`GetHashCode_Override_DifferentValues_MayDiffer` creates `NamedObj("alpha")`
and `NamedObj("beta")` but uses `EXPECT_NE` on their hash codes.  The test name
says “may differ”, while the assertion requires that they differ.  Hashes of
objects that are not equal are allowed to collide; only equality requires equal
hash codes.  The .NET `Object.GetHashCode` contract explicitly says unequal
objects do not have to return different values:
<https://learn.microsoft.com/en-us/dotnet/api/system.object.gethashcode?view=net-10.0>.

The current standard-library hash values make the test pass, so this is not a
runtime behavior failure.  It nevertheless rejects a valid implementation and
can make the suite non-portably brittle if the hash algorithm changes.

The finding is extended by `MemoryTests::GetHashCode_NonNegative`, which
forbids a valid negative signed hash, and
`ReadOnlyMemoryTest::GetHashCode_DifferentRegion_LikelyDifferent`, which again
forbids a valid collision between unequal regions.  Those tests are recorded
in their respective reports and do not establish a runtime hash defect.
`BinaryDataTests::GetHashCode_NonNegative` also forbids a valid negative hash
in the separately built IO fixture.

### Required post-audit verification

Replace the inequality assertion with contract-valid checks: equal `NamedObj`
values must produce equal hash codes, and distinct values must remain unequal
according to `Equals` without asserting hash uniqueness.  If distribution is
important, test it statistically and non-normatively outside correctness
tests.

## Other missing assertions and diagnostics

- No test verifies static `Object::Equals` with an override that is asymmetric
  or violates the hash contract; a base helper cannot repair a malformed derived
  implementation, but a dedicated derived-type example could document the
  expected rule.
- `GetTypeName` lifetime is exercised only through static strings.  A derived
  implementation returning a dangling reference would be undefined behavior;
  keep the header's stable-reference requirement visible in code review.

## Final assessment

Strong focused coverage for base semantics, with one confirmed low-severity
test-contract error (SR-AUD-018).  No production code was changed.
