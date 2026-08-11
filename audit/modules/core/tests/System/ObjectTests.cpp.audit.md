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

### Status: STILL CONFIRMED — inventory measured, implementation deferred (#2283 review, #2284, 2026-08-11)

The finding is **test-only, confirmed**: no production code is involved at any
site. But it is **not the small singleton an inherited ranking called it**, and
it is not the same class as SR-AUD-112 — that was one file, one mechanism, seven
fabricated objects and one mechanical replacement.

**Measured across every test tree**, not inherited from the Source column:
**45 hash-inequality assertions in 31 files**, plus **7 nonnegative/nonzero
assertions**, spanning at least `core`, `buffers`, `io`, `numerics`, `uri`,
`net`, `net-sockets`, `runtime`, `collections`, `globalization`, `time-zone` and
`tests/integration`. The finding's Source column names **8** files.

**Two premise corrections.**

1. **This owning file has two over-constrained assertions, not one.**
   `GetHashCode_Override_DifferentValues_MayDiffer` (line 186) is recorded above;
   `GetHashCode_IsNonNegativeValue` (line 166) is not, and it forbids precisely
   the negative signed hash this finding says is legal — the same defect the
   report attributes to `MemoryTests` and `BinaryDataTests` elsewhere.
2. **At least two further sites are unrecorded anywhere in the audit** —
   `DecimalNewTests.cpp:109` `GetHashCode_NonNegative` and `:105`
   `GetHashCode_DifferentValues_DifferentHash` — while
   `VectorMatrixTests.cpp:166` `Matrix4x4Tests.GetHashCode_DifferentMatrices_DifferentHash`
   appears only as an "extends" note in a report owned by SR-AUD-276 and is absent
   from this finding's Source column.

**The repair is not mechanical**, which is why it was deferred rather than
attempted. Blanket deletion would remove real coverage: `HashCodeTests.Combine2_OrderMatters`
pins .NET `HashCode.Combine`'s *designed* order sensitivity, and
`OrdinalComparerTests.GetHashCode_CaseSensitive_DifferentCaseDifferentHash` pins
the case sensitivity of the comparer itself — behavioural properties that must
survive in some form, expressed without claiming hash uniqueness. A third class,
the nonnegative assertions, interacts with an implementation choice:
`BinaryData` masks its hash positive, so removing the assertion also removes the
only pin on that choice.

Ticket **#2284** carries the per-site A/B/C taxonomy and the repository-wide rule
that has to be written down so the pattern is not reintroduced. SR-AUD-018 stays
**confirmed** until it lands. Nothing was modified by this review.

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
