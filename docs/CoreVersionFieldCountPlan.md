<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `Version::ToString(fieldCount)` singleton — plan

Ticket #2257. One frozen audit finding in
`modules/core/include/System/Version.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-011 | medium | `Version::ToString(fieldCount)` serialises unspecified components as `-1` |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **singleton on one member function**, not a
`System::Version` review and not a `modules/core` review.

---

## 1. Exact scope, and what is deliberately left out

In scope: the `ToString(intcs fieldCount)` overload of `System::Version`, and
nothing else in the header.

Out of scope, by decision rather than omission:

- **The no-argument `ToString()`.** It already omits an unspecified component
  (`if (Build >= 0)`, `if (Revision >= 0)`) and is *correct*. It is the control
  for this repair: every one of its outputs must stay byte-identical, and the
  probe measures that explicitly (§3).
- **The `-1` sentinel representation itself.** The two- and three-argument
  constructors leave `Build`/`Revision` at `-1`, exactly as .NET's `Version`
  leaves `_Build`/`_Revision` at `-1`. The sentinel is the .NET design, not the
  defect; the defect is *emitting* it. The review question the ticket asked —
  "is the two-argument constructor's sentinel representation separable from the
  repair?" — is answered **yes, and it must stay**: changing the sentinel would
  change `Build`/`Revision`, which are public readable fields, and would break
  the no-argument `ToString()` and `cmp()` at the same time.
- **`getMajorRevisionProperty()` / `getMinorRevisionProperty()` on an
  unspecified `Revision`.** Both return `-1` when `Revision == -1`. Checked
  against .NET: `MajorRevision => (short)(_Revision >> 16)` and
  `MinorRevision => (short)(_Revision & 0xFFFF)` produce `-1` and `-1` for
  `_Revision == -1` as well. **Same output, not a divergence** — deliberately
  not touched, and not a second occurrence of this root cause.
- **`GetHashCode()` on an unspecified component.** It folds `Build & 0xFF` and
  `Revision & 0xFFF` unconditionally; .NET's `GetHashCode` does the identical
  thing with the identical masks. **Same output, not a divergence.**
- **The message text of the pre-existing out-of-interval rejection** — see §5.
- **The public mutability of `Major`/`Minor`/`Build`/`Revision`** — see §6.

## 2. Is this one defect, or several? — one root cause, one member

`ToString(intcs fieldCount)` validates only the numeric interval `0..4` and then
appends `Build` and `Revision` *unconditionally*. There is exactly one place
where the emitted component count is decided and it consults `fieldCount` alone,
never the instance. Every observed bad cell in §3 is that one omission. It is a
singleton: one ticket reviews it (#2257) and one ticket repairs it (#2258).

The three other places in the header that read a possibly-unspecified component
— the no-argument `ToString()`, the revision properties and `GetHashCode()` —
were each checked against .NET above. The first is already correct; the other
two already match .NET's output. **There is no second output shape governed by
this root cause**, which is the question §12 of the review brief required to be
answered rather than assumed.

## 3. Before evidence, measured 2026-08-11

`build-probe/2257_probe1_before.cpp`, compiled against the shipped header and
the shipped `build/libsharp_runtime_core.a`, walks every
(defined-component-count, `fieldCount`) pair: eight subjects — the default
constructor, all three counted constructors, all three parsed spellings, and an
all-zero four-component version — against `fieldCount` ∈ `{-1, 0, 1, 2, 3, 4, 5}`.
Fifty-six pairs. Full output in `build-probe/2257_probe1_before.log`.

**Result: 48 OK / 8 BAD.** The eight bad cells are exactly, and only, the cells
where `fieldCount` exceeds the instance's defined component count:

| Subject | Call | Before | .NET |
|---|---|---|---|
| `Version()` | `.ToString(3)` | `"0.0.-1"` | throws |
| `Version()` | `.ToString(4)` | `"0.0.-1.-1"` | throws |
| `Version(1, 2)` | `.ToString(3)` | `"1.2.-1"` | throws |
| `Version(1, 2)` | `.ToString(4)` | `"1.2.-1.-1"` | throws |
| `Version(1, 2, 3)` | `.ToString(4)` | `"1.2.3.-1"` | throws |
| `Version("1.2")` | `.ToString(3)` | `"1.2.-1"` | throws |
| `Version("1.2")` | `.ToString(4)` | `"1.2.-1.-1"` | throws |
| `Version("1.2.3")` | `.ToString(4)` | `"1.2.3.-1"` | throws |

The finding's own reproduction, `Version(1, 2).ToString(3) == "1.2.-1"`,
reproduces **exactly as filed**. The probe additionally establishes three facts
the finding does not state:

1. **The parsed spellings reach the same cells**, so the repair must live in
   `ToString`, not in a constructor — `Version("1.2")` arrives at `Build == -1`
   through `parse()`, not through a defaulted member initialiser.
2. **`Version()` is a two-component version.** The default constructor is
   `0.0`, and `Version().ToString(3)` is one of the eight bad cells. No test or
   report named it.
3. **Zero is not a sentinel.** `Version(0, 0, 0, 0).ToString(4) == "0.0.0.0"` is
   an OK cell before and must stay one after. The row exists so the repair
   cannot be mis-written as "skip a falsy component".

**No premise correction was required for the finding itself.** Its severity, its
mechanism and its reproduction are all accurate.

## 4. Did any existing test pin the defect? — measured, no

`modules/core/tests/System/VersionTests.cpp` has 54 tests; seven of them cover
`ToString(fieldCount)` (lines 264–293). Every one uses a **fully specified
four-component** subject — `Version(1, 2, 3, 4)` or `Version(5, 6, 7, 8)` — so
none of them enters a bad cell. The two rejection tests assert the exception
**type only** (`EXPECT_THROW(..., System::ArgumentException)`), not the message.

No other test file calls the `fieldCount` overload at all: the seven other
translation units that include `Version.hpp`
(`ApplicationIdTests`, `EnvironmentTests`, `CultureInvariantFormattingTests`,
`OperatingSystemTests`, `NetTests`, `Task40Tests`, `Task42Tests`) use only the
no-argument `ToString()`.

**Nothing is retired by this repair.** That was the inherited premise and it is
confirmed by measurement, not assumed.

## 5. The repair

Insert two guards into `ToString(intcs fieldCount)`, after the existing interval
check and after the `fieldCount == 0` early return, before anything is written:

```cpp
if (fieldCount >= 3 && Build < 0)
    throw System::ArgumentException("fieldCount must be 0-2", "fieldCount");
if (fieldCount >= 4 && Revision < 0)
    throw System::ArgumentException("fieldCount must be 0-3", "fieldCount");
```

Three decisions in that shape, each deliberate:

**Independent field tests, not a derived component count.** .NET checks
`_Build == -1` first and `_Revision == -1` second, in that order, with upper
bounds `2` and `3`. Independent tests are also the only well-defined choice for
*this* port, because `Build` and `Revision` are public mutable fields: a caller
can leave `Build` at `-1` and set `Revision` to `5`, which no .NET constructor
can produce. Under independent tests that instance rejects `fieldCount` 3 and 4
on the `Build` guard, which is what .NET's ordering gives. A derived "count of
leading defined components" would have to invent an answer for a state .NET
cannot reach.

**`< 0`, not `== -1`.** The public fields admit any negative value; the
no-argument `ToString()` already uses `>= 0` as its "is this component
specified" predicate. Using the same predicate keeps the two overloads agreeing
about which components exist, which is the property that actually matters. With
`== -1`, `v.Build = -5` would be omitted by `ToString()` and emitted by
`ToString(3)`.

**The pre-existing interval message is not touched.** The existing branch
(`fieldCount < 0 || fieldCount > 4`) keeps its exact text, `"fieldCount must be
0-4"`, and keeps running **first**. Two consequences, both recorded rather than
hidden:

- The new branches follow the same local message style (`"fieldCount must be
  0-N"` where `N` is the largest field count this instance can serve), so the
  header stays internally consistent.
- .NET's real text is the `ArgumentOutOfRange_Bounds_Lower_Upper` resource,
  believed to be `"Argument must be between {0} and {1}."`, and in .NET the
  *instance-dependent* bound is used for an out-of-interval `fieldCount` too —
  .NET's `Version(1,2).ToString(5)` is believed to report bound `2`, where this
  port reports `4`. **With `/rv` absent this cannot be verified**, and changing
  an already-correct rejection's text is not what SR-AUD-011 asks for. Both the
  exact resource text and the out-of-interval bound are therefore left alone and
  recorded as a P3 deferred verification, #2260, in the same shape as the
  existing #2252, #2234 and #2242.

What **is** verified without `/rv`, and is what the finding turns on: the
exception **type** is `ArgumentException` and the parameter name is
`fieldCount`. Both already hold for the existing branch and both are carried by
the new branches, so the repair adds no new exception identity.

## 6. CCF relationships — none minted, none extended

- **No CCF applies.** CCF-011 (empty callables at a public boundary) is closed
  and unrelated. CCF-019 (ownership/lifetime) is open and unrelated: nothing
  here returns a reference or stores a borrowed pointer.
- **SR-AUD-063 is an adjacency of *shape*, not a shared root cause.**
  `Version`'s `Major`/`Minor`/`Build`/`Revision` are public mutable fields, the
  same shape SR-AUD-063 files against `TupleN::ItemN`. That similarity is
  **not** what SR-AUD-011 is about, and this repair does not touch field
  visibility — it reads the fields exactly as they are. Recording the adjacency
  is not membership, and SR-AUD-063 stays `confirmed` and unclaimed.
- **SR-AUD-053 is unrelated.** It is `Array::MaxLengthProperty` reporting
  `INT32_MAX` instead of `0x7FFFFFC7` — a different header, a different type, a
  single constant, and no formatting involved. Sharing the word "Version" with
  nothing at all, it was separated in the #2257 scope check and stays separated.

## 7. Compatibility, ABI, layout and `noexcept`

| Property | Before | After |
|---|---|---|
| Signature of `ToString(intcs) const` | `[[nodiscard]] std::string` | unchanged |
| `noexcept` | absent (already throws) | unchanged |
| `sizeof(Version)` / `alignof(Version)` | 16 / 4 | unchanged |
| Data members, order, access | 4 public `intcs` | unchanged |
| Virtual functions / vtable | none | unchanged |
| Exported symbols | inline member, header-only | unchanged |
| Included headers | `ArgumentException.hpp` already included | unchanged |

**This is an observable behaviour change and it is deliberate.** Eight
(instance, `fieldCount`) shapes that returned text now throw
`System::ArgumentException`. That is the finding's required repair, and it is
the same kind of change this repository has already accepted twice as ordinary
compatible remediation — #2251 (SR-AUD-104, `ApplyPolicy` now rejects an empty
assembly name) and #2236 (SR-AUD-064) — namely, *narrowing an argument domain to
the one .NET documents*.

Blast radius, measured rather than estimated: `Version.hpp` has **14 dependent
translation units** in `build/` (`ninja -t deps`), of which two are library
sources (`AppDomain.cpp`, `Environment.cpp`) and twelve are tests. **No
first-party call site uses the `fieldCount` overload at all** — every production
use (`ApplicationId::ToString`, `ApplicationId::GetHashCode`,
`OperatingSystem::getVersionStringProperty`) calls the no-argument `ToString()`,
which is unchanged.

One consequence worth stating for downstream readers: `Environment::Version` is
`Version(1, 0, 0)`, a three-component instance, so
`Environment::getVersionProperty().ToString(4)` changes from `"1.0.0.-1"` to a
throw. No first-party code does this; it is pinned as a test so the change is
visible rather than latent. Downstream migration is out of this session's reach
and remains blocked under #1773.

## 8. Test matrix (#2258)

All in `modules/core/tests/System/VersionTests.cpp`, alongside the seven
existing `ToString(fieldCount)` tests, which are all kept unchanged.

| # | Pin | Why |
|---|---|---|
| 1 | `Version(1,2).ToString(3)` throws `ArgumentException` | the finding's own required assertion |
| 2 | `Version(1,2).ToString(4)` throws `ArgumentException` | the finding's own required assertion |
| 3 | `Version(1,2,3).ToString(4)` throws `ArgumentException` | the finding's own required assertion |
| 4 | `Version()` .ToString(3)/(4) throw | the default constructor is a 2-component version |
| 5 | `Version("1.2").ToString(3)` and `Version("1.2.3").ToString(4)` throw | the parsed route reaches the same cells |
| 6 | paramName is `fieldCount` on the `Build` and the `Revision` branch | exception identity, not just type |
| 7 | message is `"fieldCount must be 0-2 (Parameter 'fieldCount')"` and `"...0-3..."` | distinguishes the two new branches from each other **and** from the pre-existing interval branch — without it, one guard could serve both and pass |
| 8 | `Version(1,2).ToString(0/1/2)` still return `""`/`"1"`/`"1.2"` | the valid 2-field cases the finding requires |
| 9 | `Version(1,2,3).ToString(3)` still returns `"1.2.3"` | the valid 3-field case |
| 10 | `Version(1,2,3,4).ToString(0..4)` unchanged | the fully specified subject never enters a guard |
| 11 | `Version(0,0,0,0).ToString(4) == "0.0.0.0"` | zero is a defined component, not a sentinel |
| 12 | `Version(1,2).ToString(-1)` and `.ToString(5)` still carry the **`0-4`** message | proves the pre-existing branch is untouched **and** still runs first |
| 13 | a `Build`-undefined, `Revision`-defined instance rejects 3 and 4 on the `Build` branch | the state only this port's public fields can reach |
| 14 | `v.Build = -5` is rejected too | pins `< 0`, not `== -1` |
| 15 | every no-argument `ToString()` in the probe's control set is byte-identical | the control |
| 16 | `Environment::getVersionProperty().ToString(4)` throws | the one reachable first-party partial version |

Row 14's first draft also asserted that `Version(1,2,3,4)` with `Build = -5`
renders as `"1.2"` from the **no-argument** `ToString()`. It rendered as
`"1.2.4"`, and that failure is a real discovery rather than a typo — see §13.
The row now pins only what #2258 owns.

## 9. Sanitizers — not applicable, deliberately

This is a missing-validation defect on a well-defined code path: the before-state
produces a *wrong string*, not undefined behaviour. `-1` is formatted by
`std::ostringstream` entirely legally. ASan/UBSan/TSan cannot discriminate
before from after and would only lengthen the evidence list, so they are not
run — the same judgement, for the same reason, that #2254 recorded.

## 10. Mutation checks planned

| # | Mutation | Expected |
|---|---|---|
| M1 | delete both guards | the new throw tests fail |
| M2 | keep only the `Build` guard | the `Revision`-branch tests fail |
| M3 | `Build < 0` → `Build == -1` | the `Build = -5` test fails |
| M4 | move the new guards *before* the interval check | the `0-4` message tests fail |
| M5 | over-reach: reject `fieldCount >= 3` regardless of `Build` | the valid 3- and 4-field tests fail |

## 11. Completion criteria

1. `cmake --build build --parallel 2` — zero errors, zero warnings.
2. An after-probe over the identical 56-pair matrix reads 56 OK / 0 BAD, with
   the no-argument control byte-identical.
3. The full 38-executable gate shows the six inherited failures and no others,
   with a test delta accounted for exactly.
4. SR-AUD-011 moves to `remediated` in `audit/AUDIT_FINDINGS_INDEX.md` and in
   its per-file report.
5. #2257 and #2258 close; #2259 opens as a P3 deferred verification.

## 12. Outcome, measured 2026-08-11

Implemented as #2258, exactly as §5 specifies: two guards, six lines, inside
`ToString(intcs fieldCount)`. Nothing else in the header changed.

**After-probe: 56 OK / 0 BAD** over the identical 56-pair matrix
(`build-probe/2257_probe2_after.log`, same source as the before-probe so the
verdict logic cannot drift). The eight formerly bad cells now throw
`ArgumentException` with `paramName == "fieldCount"`; a line-by-line diff of the
`OK` cells shows **only additions** — none of the 48 already-correct cells
changed — and the no-argument control block is **byte-identical** between the
two logs.

**Tests: +15.** `VersionTests` 54 → 68 (the seven pre-existing
`ToString(fieldCount)` tests kept unchanged), plus one in `EnvironmentTests` for
row 16. All 71 pass.

**Mutations: 5 planned, 5 run, 5 caught** (`build-probe/2258_mutations.log`).
Each mutation edited the shipped header, rebuilt `SharpRuntimeTests_Core_Base`
at two jobs and ran the suite; a mutation that is not rebuilt and not run proves
nothing.

| Mutation | Result |
|---|---|
| M1 delete both guards (the pre-repair behaviour) | CAUGHT — 10 tests fail |
| M2 drop the `Revision` guard | CAUGHT — 5 tests fail |
| M3 `== -1` instead of `< 0` | CAUGHT — 1 test fails |
| M4 run the new guards before the interval guard | CAUGHT — 1 test fails |
| M5 over-reach: reject `fieldCount >= 3` unconditionally | CAUGHT — 7 tests fail |

M3 and M4 each fail exactly one test, and that is the point: they are the two
mutations the obvious test matrix misses, which is why rows 12 and 14 exist.

**What did not change:** the signature, `noexcept`, `sizeof`/`alignof` (16/4,
measured), the four public fields and their order, the absent vtable, the
exported symbols, the included headers, and every one of the 48 already-correct
matrix cells.

## 13. A defect found while testing this one — #2259, no `SR-AUD-*` identifier

Test row 14's first draft asserted that a version whose `Build` was overwritten
with `-5` renders as `"1.2"` from the no-argument `ToString()`. It renders as
**`"1.2.4"`**: `ToString()` tests `Build >= 0` and `Revision >= 0` in two
*independent* `if`s, so it omits an undefined leading component while still
emitting a defined trailing one — printing `Revision` in `Build`'s position.

.NET does not do this. `Version.ToString()` delegates to `ToString(n)` with a
**short-circuiting** field count — `_Build == -1 ? 2 : _Revision == -1 ? 3 : 4`
— so an undefined `Build` truncates the output at two fields regardless of
`Revision`.

Three things are true about it at once, and all three are recorded rather than
collapsed:

1. **It is a real divergence**, not a test typo.
2. **It is not SR-AUD-011.** SR-AUD-011 is about the `fieldCount` overload
   emitting a sentinel it was asked for; this is the no-argument overload
   choosing a field count. Audit numbering is frozen, so this carries **no
   `SR-AUD-*` identifier** and takes an ordinary ticket, #2259.
3. **It is only reachable because `Build`/`Revision` are public mutable
   fields** — the SR-AUD-063 *shape*. That is an adjacency, not membership, and
   #2259 does not touch field visibility: it changes only how `ToString()`
   derives its field count. No approval boundary is crossed.

It was deliberately **not absorbed** into #2258. #2258 landed first, pinning
only what SR-AUD-011 owns; #2259 is the separate bounded repair.

### 13.1 #2259, implemented 2026-08-11

`ToString()` now derives its field count the way .NET does and delegates:

```cpp
[[nodiscard]] std::string ToString() const { return ToString(defaultFieldCount()); }

// private, mirroring .NET's Version.DefaultFormatFieldCount
[[nodiscard]] intcs defaultFieldCount() const {
    if (Build    < 0) return 2;
    if (Revision < 0) return 3;
    return 4;
}
```

Delegation is the point, not an economy: it makes the two overloads agree **by
construction** rather than by two hand-maintained copies of the same predicate.
The derived count is always 2, 3 or 4 and can only name components that are
defined, so the guards #2258 added can never reject it — `ToString()` still
never throws for any component state, which is pinned rather than asserted.

**Compatibility.** The only state whose text changes is an undefined `Build`
beside a defined `Revision`, which **no constructor and no `parse()` produces**.
Every constructor- and parser-reachable version keeps its exact text: the
control test `ToString_NoArgument_ByteIdenticalAcrossTheMatrix`, added by
#2258 over the probe's eight subjects, passes unchanged and — measured — did
**not** fail under any of the three mutations below, which is the strongest
statement available that reachable output is untouched. No signature, layout,
vtable, `noexcept` or symbol change; `defaultFieldCount()` is a private inline
member of a header-only class.

**Tests: +3** (`VersionTests` 68 → 71). Mutations: **3 planned, 3 run, 3
caught** (`build-probe/2259_mutations.log`).

| Mutation | Result |
|---|---|
| N1 restore the pre-repair independent `if`s | CAUGHT — 2 tests fail |
| N2 short-circuit on the trailing component instead of the leading one | CAUGHT — 3 tests fail |
| N3 `== -1` instead of `< 0` in `defaultFieldCount` | CAUGHT — 4 tests fail |

N2 is the interesting one: deriving the count from `Revision` first returns 4
for the very state this ticket is about, and `ToString(4)` then **throws** out
of a no-argument formatter — which is why
`ToString_NoArgument_NeverRejectsItsOwnFieldCount` exists.
