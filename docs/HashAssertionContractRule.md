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
implementation scope is therefore re-measured here with a reproducible
definition, `build-tmp/2284/scan_hash_assertions.py`, which parses whole
assertion statements (so a site spanning several physical lines counts once).

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
