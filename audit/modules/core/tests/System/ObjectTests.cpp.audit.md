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

### Status: REMEDIATED — #2284, 2026-08-11

Test-only, as the review concluded: **no file under any `modules/*/include` or
`modules/*/src` tree was modified.** The design record is
`docs/HashAssertionContractRule.md` — six rules (R1–R6), the complete site
inventory, and a per-site A/B/C disposition.

**Inventory, re-measured before editing.** #2283 recorded 45 hash-inequality
assertions in 31 files plus 7 nonnegative/nonzero, and stored no per-site list.
A reproducible scan (`build-tmp/2284/scan_hash_assertions.py`, which parses whole
assertion statements so a multi-line site counts once) measured **51
hash-inequality assertions in 28 files plus 14 nonzero/nonnegative in 12 files**,
35 files in union. That is a superset in assertion count. It is not drift:
`git diff 274d284..HEAD -- modules tests test` touched no inventory file, so the
two numbers differ only in where the counting boundary was drawn — the review's
own module list omits `io-hashing`, which a shape-only search picks up. Nothing
#2283 named was omitted.

**Two production facts, read and not altered, decided most of the dispositions.**

1. `System::HashCode` mixes a `std::random_device` seed into its initial state
   once per process, deliberately, mirroring .NET (`HashCode.hpp:34–39`). Nine
   assertions in this inventory were therefore not merely *permitted* to fail —
   they were **not reproducible across runs**. `Matrix4x4`, `LingerOption`,
   `IPAddress` and `IPNetwork` all reach it. No exception rescues such an
   assertion, so each became a statement over a family of inputs.
2. Six types end their hash with `& 0x7fffffff` — `Object` (`Object.cpp:43`),
   `Memory`, `ReadOnlyMemory`, `ArraySegment`, `Decimal`, `BinaryData`. This
   answers the question #2284 left open about `BinaryData`: **the masking is
   intended**, it is not unique to `BinaryData`, and it is the identical one-line
   idiom in six independent types. Those assertions are kept, renamed
   `GetHashCode_MasksTheSignBit`, and each names the line it pins.

**This file's two sites.** `GetHashCode_IsNonNegativeValue` (line 166) — the
premise correction the review added — is **retained** under the second fact
above and renamed `GetHashCode_MasksTheSignBit`.
`GetHashCode_Override_DifferentValues_MayDiffer` (line 186) becomes
`GetHashCode_Override_DifferentValues_AreUnequalObjects`: its own name already
said "MayDiffer" while the body asserted they must, so the body now asserts what
the override actually guarantees. A **third site in this file, unrecorded
anywhere in the audit**, was found while editing and is folded into the masking
pin: `GetHashCode_WithinIntRange` (line 171) asserted `int <= INT_MAX` and could
not fail. Recorded as a newly found occurrence of the same family; **no new
`SR-AUD-*` identifier was minted.**

**Both properties the review warned must survive, do.**
`HashCode::Combine`'s designed order sensitivity survives as non-commutativity
over 28 pairs — "a commutative combiner would give 0 of 28" — and mutation M1
(drop the FNV multiply) fails it. `OrdinalComparer`'s case sensitivity survives
on `Equals`/`Compare`, where it already lived and was already pinned twice, and
its hash side now checks agreement with `Equals` over every pair of a vocabulary
in **both** modes; mutation M3 (ignore `ignoreCase_`) fails it. A third property
the review did not name was at greater risk than either: `IPAddress`'s IPv6 hash
regression — the pre-repair code combined the first 64 bits only, so an entire
`/64` hashed alike — had **no observable except the hash**, so deleting the
assertion would have deleted the regression test. It is reformulated as "eight
addresses in one `/64` do not all share a hash": 1 distinct hash before the
repair, 8 after. Mutation M6, which restores the exact defect, fails it.

**Retained deliberately, and why they are not this finding:** all five
`TotalOrderIeee754Comparer` inequalities (the production header states "-0 and +0
hash differently, and so do distinct NaN payloads" as behaviour, and for `float`
and `Half` the hash *is* the bit pattern widened, so it is injective by
construction), and two `std::hash<double>` inequalities in the collections
comparison-contract suite, relabelled as what they are — precondition guards that
keep mutation M8 detectable, not claims about a sharp-runtime contract.
**Excluded with reason:** the 13 same-shaped assertions in `io-hashing` are
known-answer checks on standardised checksums (`Crc32`, `Adler32`, `XxHash*`),
not equals/hash-contract sites.

**Three sites now pin a reachable collision** instead of forbidding one, which is
the finding's own claim turned into evidence: `Version(1,2,3,4)` and
`Version(1,2,3,4100)` are unequal and hash identically because `Revision` is
packed into 12 bits; `DateTime(1)` and `DateTime(0x100000000)`; the same for
`TimeSpan`. `UInt64::GetHashCode(0x0000000100000001ULL)` is exactly **0** — a
nonzero input with a zero hash, in the very function that used to assert
otherwise.

**After: zero class-A sites and zero nonzero claims remain** in the repository;
the 14 residual inequalities are the 7 excluded `io-hashing` ones, the 5 retained
comparer ones and the 2 relabelled guards.

Complete 38-executable gate: 16,941 run, 16,933 passed, 6 failed, 2 skipped
(16,933 + 6 + 2 = 16,941), −5 from 16,946. The six failures are inherited and
untouched: five `PingTests` (#1962) and
`SocketTests.Connect_ByHostname_NoMatchingAddressFamily_Throws`, which needs
usable IPv6 this environment does not provide. The −5 is five deletions, each
because a strictly stronger statement already stood beside it; three further
deletions are offset by three additions. Seven valid mutations, seven caught; one
labelled equivalent; two discarded as non-compiling. Sanitizers deliberately not
run — no site involves memory, lifetime, threading or overflow, and no production
instruction changed.
