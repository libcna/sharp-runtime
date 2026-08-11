<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# The hash-assertion rule, and the SR-AUD-018 site inventory

Ticket **#2284** (implementation) — review **#2283** — finding **SR-AUD-018**.
Written 2026-08-11. This document is the design record the finding's acceptance
criteria require: *"the resulting rule written down so the pattern is not
reintroduced"*.

---

## 1. What the finding actually says

SR-AUD-018 records that tests across this repository require distinct values to
have distinct hash codes, or require a hash code to be nonzero or nonnegative,
although **valid hash collisions and negative or zero hash codes are allowed**.

Review #2283 measured the real surface and disproved the inherited assumption
that this was one assertion in one file. It is a repository-wide pattern. It
also established that **not every such assertion is defective** — some pin a
property the type deliberately implements and documents.

## 2. The rule

These six rules are binding on every test in this repository, present and
future. They are stated in terms of what a hash contract does and does not
promise, not in terms of which macro is written.

**R1 — the contract direction is assertable.**
A test may assert that values which are equal under the relevant equality hash
equally, and that hashing the same value twice yields the same result. That is
the whole of the equals/hash contract, and it is the direction a hashed
container actually depends on.

**R2 — the converse is not.**
A test must **not** assert that two *unequal* values hash differently. No hash
contract forbids collisions; a hash function that collides is slower, not
wrong. Where such an assertion's real intent is "these two values are
distinct", assert that on `Equals` / `operator==` / `Compare` / container
lookup, which is where the distinction actually lives.

**R3 — exception: behaviour the type documents or the suite pins exactly.**
R2 does not apply where the production type's own documentation states the
distinction as behaviour, or where the same suite already pins the hash's exact
value. Two measured examples:

* `TotalOrderIeee754Comparer<T>` documents, in the header, that "-0 and +0 hash
  differently, and so do distinct NaN payloads" and that the binary64 hash
  "folds the wide pattern rather than truncating it". For `float` and `Half`
  the hash *is* the bit pattern widened, so the property is not merely
  documented, it is injective by construction.
* `TupleTests.cpp` pins `Tuple::Create(1, 2, 3, 4).GetHashCode() == 1252` and
  its siblings exactly, because `detail::tupleHashCombine` is a defined-arithmetic
  contract (SR-AUD-062). A separate inequality between two of those pinned
  values adds nothing.

An assertion admitted by R3 must say in a comment *which* documented property it
pins. It is not a licence to reintroduce R2 by asserting that some pair happens
not to collide.

**R4 — no exception at all for `System::HashCode`.**
`System::HashCode` mixes a `std::random_device` seed into its initial state once
per process, deliberately, mirroring .NET (`HashCode.hpp` lines 34–39). Any two
values derived from it are therefore **not reproducible across runs**, and an
inequality between them is a coin flip the test cannot win by design — a real
flake, not a theoretical one. R3 never rescues such an assertion. Where the
property under test is a genuine sensitivity of the combiner (order sensitivity,
dependence on a given argument), state it over a *family* of inputs — "a
commutative combiner would make all of these agree" — never over one pair.

Production types measured to route their hash through `System::HashCode`:
`Matrix4x4`, `LingerOption`, `IPAddress`, `IPNetwork`, `Colors`,
`RunePosition`, and six `Net.Http.Headers` value types.

**R5 — `EXPECT_GE(hash, 0)` is valid only where the implementation masks.**
Six types in this repository deliberately end their hash with
`& 0x7fffffff`: `System::Object`, `Memory<T>`, `ReadOnlyMemory<T>`,
`ArraySegment<T>`, `Decimal` and `BinaryData` (and the `NamedObj` test fixture
in `ObjectTests.cpp` copies the idiom). For those, a nonnegative assertion pins
a real, intended implementation choice and should say so. Anywhere else it is
invalid — a signed hash may legally be negative.

**R6 — `EXPECT_NE(hash, 0)` is never valid.**
Zero is a legal hash code, and several types in this repository *document* zero
for a distinguished state: an empty `Nullable<T>`, an empty `Delegate`, a
default `Type`, and `RuntimeHelpers::GetHashCode(nullptr)`. Pin those documented
zeros directly. For the non-distinguished case, assert determinism instead. Two
measured reachable zeros that show the claim is not academic:
`Version(0, 0, 0, 0).GetHashCode() == 0` (already pinned in `VersionTests.cpp`)
and `UInt64::GetHashCode(0x0000000100000001ULL) == 0`.

### 2.1 What replaces a removed assertion

In order of preference: the value inequality the test actually meant
(`Equals`/`operator==`/`Compare`); a container-lookup outcome; the contract
direction (equal values, equal hashes) over a *family* of inputs rather than one
pair; a determinism assertion; a documented exact value. Never a retry, never a
sampling loop that tolerates failure, never a "likely" in a test name.

---

## 3. Inventory: recorded versus measured

Review #2283 recorded **45 hash-inequality assertions in 31 files, plus 7
nonnegative/nonzero assertions**, and stored no per-site list. #2284's
implementation scope is therefore re-measured here with an explicit definition,
stated in full so it can be reproduced without the throwaway probe that ran it
(`build-tmp/2284/scan_hash_assertions.py`, deleted with the batch's other probe
artefacts per `CLAUDE.md` build-resource rule 11):

> Over every `.cpp` under `modules/*/tests`, `tests/` and `test/`: find each
> `EXPECT_*`/`ASSERT_*` macro and take the **whole** parenthesised statement,
> balanced across newlines and string literals, so a site spanning several
> physical lines counts once. A statement is a **hash-inequality** site when the
> macro is `_NE` and both operands are hash-producing expressions
> (`GetHashCode(`, `ToHashCode(`, `HashCode::Combine(`, `std::hash<...>{}(`,
> `GetCurrentHashAs*(`, or a local assigned from one). It is a **nonzero /
> nonnegative** site when the macro is `_NE` against a literal zero, or `_GE`/`_GT`
> against zero, with one hash-producing operand.

| Measure | #2283 recorded | Re-measured 2026-08-11 |
|---|---|---|
| Hash-inequality assertions | 45 | **51** |
| Files carrying them | 31 | **28** |
| Nonzero / nonnegative assertions | 7 | **14** (6 nonnegative + 8 nonzero) |
| Files carrying those | not recorded | **12** |
| Union of files | 31 | **35** |

**No inventory file changed between #2283's commit (`274d284`) and this one** —
`git diff --stat 274d284..HEAD -- modules tests test` touches only
`FuncTests.cpp`, `InterfaceTests2.cpp`, `ObsoleteAttributeTests.cpp`,
`ThreadStaticAttributeTests.cpp` and `VoidTests.cpp`, none of which is a site.
The difference is therefore a **counting-boundary artefact, not repository
drift**: #2283 recorded a total and a file count that cannot both be reproduced
from any single definition, and its own module list ("core, buffers, io,
numerics, uri, net, net-sockets, runtime, collections, globalization, time-zone
and the integration fixture") excludes `io-hashing`, which the shape-only search
picks up. The re-measured set is a **superset** in assertion count and is what
#2284 dispositions; nothing #2283 named is omitted.

### 3.1 Deliberately excluded, with reason

* **`modules/io-hashing/tests/System/IO/HashingTests.cpp`** — 13 assertions of
  the same *shape* (`EXPECT_NE(crc32Hash(a), crc32Hash(b))`,
  `EXPECT_NE(afterHello, 0u)`, clone-independence checks). These are **not**
  equals/hash-contract sites: `Crc32`, `Adler32`, `XxHash32/64/3/128` are
  standardised checksums whose output for a given input is fully specified by
  the algorithm, so each assertion is a known-answer check, not a claim that
  collisions cannot happen. `io-hashing` is absent from #2283's own module list.
  Left untouched.
* **`modules/security-cryptography/.../CryptographySupportTests.cpp:83`** —
  `EXPECT_NE(a, HashAlgorithmName::SHA1)` compares two *names*, not two hash
  codes. Not a site.

---

## 4. Taxonomy and per-site disposition

#2284's taxonomy is used as written. One sub-label is added where measurement
required it: **B-guard**, for an inequality that guards a *test's own
discriminating power* against a third-party hasher rather than asserting a
sharp-runtime contract.

* **A — accidental collision assertion.** Two unequal values asserted to hash
  differently with no documented basis. *Rewrite to the semantic claim, or
  delete where the file already carries it.*
* **B — designed property expressed through a hash inequality.** *Keep the
  property; restate it so it is not a claim of injectivity.*
* **C — nonnegative / nonzero.** *C1 = the implementation masks: retain and say
  so. C2 = zero or a negative value is legal: remove or convert.*

### 4.1 Class A — 33 sites in 22 files

| Site | Test | Disposition |
|---|---|---|
| `buffers/…/ReadOnlyMemoryTests.cpp:138` | `GetHashCode_DifferentRegion_LikelyDifferent` | rewrite → identity inequality (`Equals` false for equal-content distinct buffers) |
| `core/…/ArraySegmentNewTests.cpp:335` | `GetHashCode_DifferentOffset_DifferentHash` | rewrite → segments at different offsets are unequal |
| `core/…/ObjectTests.cpp:186` | `GetHashCode_Override_DifferentValues_MayDiffer` | rewrite → the override is value-based; unequal names are unequal objects |
| `core/…/DecimalNewTests.cpp:105` | `GetHashCode_DifferentValues_DifferentHash` | rewrite → value inequality + hash determinism |
| `core/…/DateTimeTests.cpp:535` | `GetHashCode_DifferentTicksDiffer` | rewrite → value inequality |
| `core/…/DateOnlyTimeOnlyTests.cpp:234` | `GetHashCode_DifferentDatesDiffer` | **delete** — `GetHashCode_MatchesDayNumber` in the same suite pins `hash == dayNumber` exactly, which is strictly stronger |
| `core/…/TimeOnlyTests.cpp:240` | `GetHashCode_DifferentTime_DifferentHash` | rewrite → value inequality |
| `core/…/TimeSpanTests.cpp:477` | `GetHashCode_DifferentTicks_DifferentHash` | rewrite → value inequality |
| `core/…/TupleNewTests.cpp:52` | `Tuple1_GetHashCode_Differs` | rewrite → `Tuple1` value inequality |
| `core/…/TupleNewTests.cpp:76` | `Tuple2_GetHashCode_DiffValues_Differ` | rewrite → tuple **equality** is order-sensitive (the real contract behind the pair `(1,2)`/`(2,1)`) |
| `core/…/TupleTests.cpp:316` | `EqualTuplesStillHashEquallyAcrossOverflowingInputs` | mixed test: drop the one NE line, keep the rest; substitute value inequality |
| `core/…/VersionTests.cpp:256` | `GetHashCode_DifferentVersions_DifferentHash` | rewrite → **pin a real collision**: `Version(1,2,3,4)` and `Version(1,2,3,4100)` are unequal and hash identically, because `Revision` is masked to 12 bits |
| `core/…/StringTests.cpp:865` | `GetHashCode_DifferentStrings` | rewrite → equal strings built differently hash equally |
| `collections/…/EqualityComparerTests.cpp:37` | `DefaultHashCodeDifferent` | rewrite → hash agrees with `Equals` |
| `collections/…/SpecialComparersTests.cpp:38` | `DifferentStringsLikelyDifferentHash` | **delete** — the suite already carries `UnequalStrings`, `HashCodeIsStable`, `EqualStringsHaveSameHash` and `IgnoreCaseHashSameForDifferentCase` |
| `globalization/…/IdnMappingTests.cpp:126` | `GetHashCode_MatchesForEqualInstances` | mixed test: drop the NE line, keep the EQ; the adjacent `Equality` test already pins the inequality |
| `net/…/IPNetworkScopeTests.cpp:146` | `NetworksOnDifferentLinksAreNoLongerEqual` | mixed test: drop the NE line (R4 — `IPNetwork` hashes through `System::HashCode`); `EXPECT_NE(onLink7, onLink9)` on the line above is the regression pin |
| `net-sockets/…/SocketsSupportTests.cpp:91` | `IPPacketInformationTests.GetHashCode_DifferingInterfaceDiffers` | **delete** — `Equality` above already asserts `EXPECT_NE(info, IPPacketInformation(Loopback, 4))` |
| `net-sockets/…/SocketsSupportTests.cpp:130` | `UdpReceiveResultTests.GetHashCode_DifferingBufferDiffers` | rewrite → value inequality |
| `net-sockets/…/SocketsSupportTests.cpp:155` | `LingerOptionTests.GetHashCode_DifferingTimeDiffers` | rewrite → value inequality (R4: `LingerOption` hashes through `System::HashCode`) |
| `net-sockets/…/SocketsSupportTests.cpp:254` | `UnixDomainSocketEndPointTests.GetHashCode_DifferingPathDiffers` | **delete** — `Equality` above already asserts `EXPECT_NE(a, c)` |
| `numerics/…/VectorMatrixTests.cpp:166` | `Matrix4x4Tests.GetHashCode_DifferentMatrices_DifferentHash` | rewrite → matrix value inequality (R4: `Matrix4x4` hashes through `System::HashCode`) |
| `runtime/…/RuntimeInformationTests.cpp:44` | `OSPlatformTests.GetHashCode_DiffersForDistinctValues` | rewrite → `OSPlatform` value inequality |
| `time-zone/…/AdjustmentRuleTests.cpp:199` | `GetHashCode_DiffStart_LikelyDiffHash` | rewrite → rule value inequality |
| `uri/…/UriBuilderTests.cpp:364,365,366` | `DeliberatelyUnequalPairsStayUnequal_…` | drop 3 NE lines; the three `EXPECT_FALSE(lower.Equals(...))` above them are the gate pin |
| `uri/…/UriBuilderTests.cpp:380,382` | `UriIdentityItselfIsUnchangedByThisTicket` | drop 2 NE lines; `EXPECT_FALSE(a == …)` already present |
| `uri/…/UriTests.cpp:833` | `DocumentedContract_CaseDifferingUrisAreNotEqualYet` | drop the NE line; `EXPECT_FALSE(a == b)` is the divergence pin |
| `uri/…/UriTests.cpp:1415` | `EmbeddedNul_DoesNotProduceAPrefixOnlyParse` | drop the NE line; `EXPECT_FALSE(withNul == prefix)` is the safety pin |
| `collections/…/CollectionsComparisonContractTests.cpp:1123` | `SortedSetHoldsEveryNaNPayloadExactlyOnce` | rewrite → the exact bit-pattern inequality the assertion's own message already claims. `SortedSet` is comparison-based and never hashes, so `std::hash` is irrelevant here |

### 4.2 Class B — 13 sites in 5 files

| Site | Test | Property that must survive | Disposition |
|---|---|---|---|
| `numerics/…/TotalOrderIeee754ComparerTests.cpp:144,158,196,206,225` | signed zeros, NaN payloads, fold-not-truncate | documented in the production header, and injective by construction for `float`/`Half` | **retained unchanged**, with an added comment naming R3 |
| `core/…/HashCodeTests.cpp:44` | `Combine2_OrderMatters` | `HashCode::Combine` is order-sensitive, not commutative | restate over a family of pairs (R4) |
| `core/…/HashCodeTests.cpp:32` | `Add_DifferentValues_DifferentResult` | the accumulator depends on the value added | restate over a family (R4) |
| `core/…/SystemTypesRemainingTests.cpp:377,383,392,406,411,416,421` | `Combine1/2/4/5/6/7/8` argument sensitivity | the result depends on every argument | drop the seven per-arity NE lines, keep every determinism EQ, and add **one** test covering *every* argument position of *every* arity — strictly stronger and seed-robust (R4) |
| `core/…/SystemTypesRemainingTests.cpp:445` | `AddBytes_DifferentInput_DifferentHash` | `AddBytes` depends on the bytes | restate over a family (R4) |
| `core/…/StringComparerTests.cpp:111` | `OrdinalComparer` case sensitivity | the two comparer modes differ | restate as an agreement check over a vector of strings, for both modes: `Equals` ⇒ equal hashes. The case sensitivity itself is already pinned by `Equals_DifferentCase_False` and `Compare_CaseSensitive_DifferentCase` |
| `net/…/NetTests.cpp:323` | `GetHashCode_DiffersWhenOnlyLower64BitsDiffer` | the IPv6 hash reads all 128 bits — a defect repaired **in the hash function itself**, with no other observable | restate as "not all of eight addresses sharing a `/64` hash alike", which is exactly what the pre-repair code did and is robust to the seed (R4) |
| `collections/…/CollectionsComparisonContractTests.cpp:1037,1256` | NaN-key lookup in hashed containers | **B-guard**: the guard makes mutation M8 (restore `std::hash` as the backing hasher) detectable | retained as an explicitly labelled *test precondition* on the standard library's hasher, with the exact bit-pattern inequality added alongside |

### 4.3 Class C — 14 sites in 12 files

**C1 — retained, the implementation deliberately masks (`& 0x7fffffff`):**

| Site | Test | Masking implementation |
|---|---|---|
| `core/…/ObjectTests.cpp:166` | `GetHashCode_IsNonNegativeValue` | `Object.cpp:43` |
| `buffers/…/MemoryTests.cpp:321` | `GetHashCode_NonNegative` | `Memory.hpp:280` |
| `core/…/ArraySegmentNewTests.cpp:328` | `GetHashCode_NonNegative` | `ArraySegment.hpp:414` |
| `core/…/DecimalNewTests.cpp:109` | `GetHashCode_NonNegative` | `Decimal.hpp:246` |
| `io/…/BinaryDataTests.cpp:42` | `GetHashCode_NonNegative` | `BinaryData.hpp:391` |
| `tests/integration/Task42Tests.cpp:254` | `ObjectTests.GetHashCode_NonNegative` | `Object.cpp:43` |

Each keeps its assertion and gains a comment naming the masking line, so the
assertion reads as the deliberate pin #2284 asks for rather than as an
accidental universal claim. **This is the measured answer to #2284's open
question about `BinaryData`: the masking is intended, it is not unique to
`BinaryData`, and it is the same one-line idiom in six independent types.**

**C2 — removed or converted, zero is legal:**

| Site | Test | Disposition |
|---|---|---|
| `core/…/HashCodeTests.cpp:18` | `ToHashCode_DefaultIsNonZero` | convert → a default `HashCode` is *stable*; zero is legal and, being seeded, is reachable (R4/R6) |
| `core/…/NullableTests.cpp:70` | `GetHashCode_WithValue` | convert → the documented contract: a present value hashes as the wrapped value's default hash |
| `core/…/SystemTypesRemainingTests.cpp:224` | `GetHashCode_HasValue_NonZero` | convert → same documented contract |
| `core/…/DelegateTests.cpp:297` | `GetHashCode_Multicast_DiffersFromEmpty` | convert → stability, plus the documented empty-delegate zero pinned directly |
| `core/…/DelegateInvocationListEqualityTests.cpp:149` | `LambdaEntryList_HashIsStableAndNonZero` | drop the NE(0); stability is the ticket's actual claim |
| `runtime/…/RuntimeTests.cpp:297` | `IdentityHashAndObjectValue_…` | drop the NE(0); the documented null-is-zero assertion on the next line stays, and identity agreement is added |
| `tests/integration/Task42Tests.cpp:317` | `TypeTests.GetHashCode_NonZero` | convert → a default-constructed `Type` hashes to the documented 0 |
| `tests/integration/Task42Tests.cpp:485` | `UInt64Tests.GetHashCode_NonZero` | convert → **pin a real zero**: `UInt64::GetHashCode(0x0000000100000001ULL) == 0`, equal to `GetHashCode(0)`, from `value ^ (value >> 32)` |

### 4.4 Newly found occurrence, same family

`core/…/ObjectTests.cpp:171` `GetHashCode_WithinIntRange` asserts
`EXPECT_LE(o.GetHashCode(), INT_MAX)` on a value of type `int`. It is
**vacuous** — it cannot fail — and it is the upper half of the same masking
claim as line 166. Recorded here as a newly found occurrence of the SR-AUD-018
family (it is not in the finding's Source column and not in #2283's two premise
corrections) and folded into the single C1 masking pin. No `SR-AUD-*` identifier
was minted.

---

## 5. Production impact

**None.** No file under any `modules/*/include` or `modules/*/src` tree is
modified by #2284. Every hash implementation the inventory touches was measured
and found correct for its documented contract; nothing was changed to make a
test pass, and no independent production defect was found that would need an
ordinary ticket.

The two facts that most shape the dispositions are properties of production code
that were **read, not altered**: the per-process seed in `HashCode.hpp` (R4) and
the `& 0x7fffffff` idiom in six types (R5).

## 6. Sanitizers

Not run. SR-AUD-018 is a test-contract correctness problem: no site involves
memory access, lifetime, threading or integer overflow, and no disposition
changes any production instruction. A sanitizer run would add no discriminating
evidence.

---

## 7. Mutation evidence

Eight mutations were applied to production code, each rebuilt at two jobs and
run against the specific test it targets, then reverted. Two further attempts
were **discarded as invalid**: the first forms of M2 and M5 failed to compile
under `-Werror` (`unused-parameter`, `misleading-indentation`), so their test
runs came from a stale binary and are not counted; both were rewritten into the
compiling forms below and re-run.

| # | Mutation | Target of the mutation | Result |
|---|---|---|---|
| M1 | `HashCode::mix` drops the FNV multiply, becoming commutative | `HashCode.hpp` | **caught** — `Combine2_IsOrderSensitiveNotCommutative` fails. `CombineN_ResultDependsOnEveryArgument` and `Add_ResultDependsOnTheValueAdded` correctly survive: a commutative combiner is still sensitive to each argument, and the two tests claim different things. |
| M2 | `Combine(v1..v4)` silently drops `v3` | `HashCode.hpp` | **caught** — `CombineN_ResultDependsOnEveryArgument` fails. `Combine4_Works`, the determinism half that survived the removal of the old `EXPECT_NE`, **passes** — evidence that the new test, not the old one, carries this coverage. |
| M3 | `OrdinalComparer::GetHashCode` ignores `ignoreCase_` | `StringComparer.hpp` | **caught** — `GetHashCode_AgreesWithEqualsInBothModes` fails, alongside the pre-existing `GetHashCode_IgnoreCase_SameHashForDifferentCase`. `Equals_IgnoreCase_DifferentCase_True` passes, confirming the mutation is hash-side only. |
| M4a | `Version::GetHashCode` drops the 12-bit `Revision` mask (`hash \|= Revision`) | `Version.hpp` | **equivalent, discarded** — `\|=` cannot clear the `0x3000` that `Build` already set, so 4 and 4100 still collide and `GetHashCode_UnequalVersionsMayCollide` still passes. Labelled equivalent rather than counted as a survivor. |
| M4b | `Version::GetHashCode` folds `Revision` with `^=` and no mask | `Version.hpp` | **caught** — `GetHashCode_UnequalVersionsMayCollide` fails; `GetHashCode_SameVersion_SameHash` and `GetHashCode_ZeroVersion_Zero` pass. |
| M5 | `HashCode::AddBytes` reads each byte but always mixes `0` | `HashCode.hpp` | **caught** — `AddBytes_ResultDependsOnTheBytes` fails; `AddBytes_SameData_SameHash` and `AddBytes_SameInput_SameHash` pass. |
| M6 | `IPAddress::GetHashCode` combines `numbers_[0..3]` only — the exact pre-repair defect | `IPAddress.cpp` | **caught** — `GetHashCode_ReadsTheLower64BitsToo` fails. This is the load-bearing result of the batch: it proves the reformulated test preserves the regression coverage the removed pairwise inequality carried. |
| M7 | `Decimal::GetHashCode` stops masking the sign bit | `Decimal.hpp` | **caught** — `GetHashCode_MasksTheSignBit` fails. The second value the test gained (`"-3.14"`) is what makes the detection deterministic rather than a coin flip on one datum. |

Seven valid non-equivalent mutations, seven caught; one equivalent mutation
labelled; two invalid mutations discarded. Every counted result involved a real
source edit, a real rebuild and a real run.

## 8. Closing state

Re-measured after the three implementation commits, with the same scanner:

| | Before | After |
|---|---|---|
| Hash-inequality assertions | 51 | **14** |
| — of which `io-hashing`, out of scope (§3.1) | 7 | 7 |
| — of which retained class B (`TotalOrderIeee754Comparer`) | 5 | 5 |
| — of which retained class B-guard (`std::hash` preconditions) | 2 | 2 |
| — **class A, the defect itself** | **37** | **0** |
| Nonzero / nonnegative assertions | 14 | **8** |
| — retained C1, inside the six `GetHashCode_MasksTheSignBit` tests | 6 | 8 |
| — **C2 nonzero (`!= 0`) claims** | **8** | **0** |

(The C1 count rises from 6 to 8 because `Decimal`'s and `BinaryData`'s pins each
gained a second value — a negative decimal and the empty payload — which is what
makes mutation M7 deterministic.)

Every one of the 65 measured sites is dispositioned. No class-A site and no
nonzero claim remains anywhere in the repository.

**Test count.** Complete 38-executable gate: 16,941 run, 16,933 passed, 6
failed, 2 skipped — `16,933 + 6 + 2 = 16,941`, a delta of **−5** from the
inherited 16,946. The six failures are the inherited ones (five `PingTests`,
ticket #1962; one `SocketTests.Connect_ByHostname_NoMatchingAddressFamily_Throws`,
no usable IPv6 in this environment) and are untouched by this batch. Counting
`TEST`/`TEST_F`/`TEST_P` declarations across the 35 changed files gives
2,321 → 2,316, the same −5, from exactly five deletions:

| File | Test deleted | Why nothing was lost |
|---|---|---|
| `ObjectTests.cpp` | `GetHashCode_WithinIntRange` | asserted `int <= INT_MAX`; could not fail |
| `DateOnlyTimeOnlyTests.cpp` | `GetHashCode_DifferentDatesDiffer` | `GetHashCode_MatchesDayNumber` pins the hash exactly |
| `SpecialComparersTests.cpp` | `DifferentStringsLikelyDifferentHash` | three neighbouring tests already carry Equals, stability and the contract direction |
| `SocketsSupportTests.cpp` | `IPPacketInformationTests.GetHashCode_DifferingInterfaceDiffers` | the `Equality` test above asserts the same pair |
| `SocketsSupportTests.cpp` | `UnixDomainSocketEndPointTests.GetHashCode_DifferingPathDiffers` | the `Equality` test above asserts the same pair |

Three tests were added — `CombineN_ResultDependsOnEveryArgument`,
`UInt64Tests.GetHashCode_FoldsTheHalves` and
`UInt64Tests.GetHashCode_ZeroIsReachable` — and three deleted alongside them
(`Combine1_DifferentValueDifferentHash`, `TypeTests.GetHashCode_NonZero`,
`UInt64Tests.GetHashCode_NonZero`), which is why those two files net to zero.
**Correction to commit `05bef4f`'s message**, which states the integration
fixture moved by "+1 net": it did not — one deletion plus one
replaced-by-two nets to zero, and the arithmetic above is the accurate record.

Forty-four hash-inequality assertions and eight nonzero assertions were removed
or replaced; six nonnegative assertions were retained and restated; twenty-eight
tests were rewritten in place without a count change.

**Validation.** Build 0 errors / 0 warnings; module graph 41 modules / 92 edges;
seams 3 / 20; negative fixtures 16 files / 128 sites / 144 compiler invocations
at peak 2 jobs; generated catalogue current; validator self-tests OK; DB
consistency OK; `git diff --check` clean; tracked `scripts/__pycache__` 3 files,
unmodified. Maximum aggregate compiler parallelism throughout: **2**.

`scripts/run_component_tests.sh` and `scripts/local_ci_check.sh` both stop at the
first failing executable and therefore cannot produce a complete total while the
six inherited failures stand; the 38-executable total above comes from
`build-tmp/full_gate.sh`, which runs each executable independently and continues
past failures.

**Selective components was not rerun.** #2284 changed no component boundary, no
`PUBLIC_DEPENDENCIES`, `PRIVATE_DEPENDENCIES` or `TEST_DEPENDENCIES`, no module
graph entry and no catalogue entry. The only include lines added are standard
headers (`<array>`, `<cstddef>`, `<cstdint>`, `<functional>`, `<set>`,
`<string>`) plus `SharpRuntime/SharpRuntimeHelper.hpp`, which is inside the
`core` module's own include root and creates no new edge. The boundary validator
and the catalogue check both confirm the graph is unchanged at 41 / 92.
