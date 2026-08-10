<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Numerics` namespace review and remediation plan

*Ticket #2167. Opened 2026-08-10 on branch `claude/remediation-batch-1804-namespace-b1yjh5`, after
`modules/security-cryptography` and `modules/console` were both fully closed. Audit numbering is
**frozen at 364** — this review creates no `SR-AUD-*` identifier. Everything below was measured in
this container; the `/rv` reference tree is **absent**, and every place that limits a claim says so
rather than guessing past it.*

---

## 1. Selection, re-measured rather than inherited

`audit/AUDIT_FINDINGS_INDEX.md` was re-parsed from scratch (364 rows: **160 remediated**,
**154 confirmed**, **50 confirmed (design-complete)** — 204 open). Among units with **no** review
plan and **no** remediated finding:

| Module | Open | high | Remediated | `/rv`-bound | Verdict |
|---|---|---|---|---|---|
| `core` | 72 | 9 | 47 | mixed | **not a namespace**; already carved by seven `CCF-*` plans and partly remediated |
| `time-zone` | 7 | 0 | 0 | **heavily** | needs a real tz database; most findings would defer |
| `globalization` | 7 | 1 | 0 | **heavily** | needs `/rv` **and** ICU data |
| **`numerics`** | **4** | **0** | **0** | **partly** | **selected** |
| `xml-linq` | 4 | 1 | 0 | no | its only high **is** CCF-019 — blocked (#1899/#1894) |
| `net-network-information` | 3 | 0 | 0 | no | dominated by #1962, which is blocked |
| `scripts` / `tests` / `.github` | 3 / 2 / 1 | 0 | 0 | no | tooling, not namespaces |

`numerics` is selected because it is the **largest remaining unreviewed unit that is a real
namespace, has zero blocked findings, and can be decided without a reference tree**. The two
seven-finding alternatives both fail the last clause: `time-zone` and `globalization` are parity
questions against data this container does not have, so a review of either would produce mostly
deferred-verification tickets — exactly what the batch brief says to avoid.

**The inherited handoff's characterisation is confirmed in three of four parts and corrected in the
fourth.** Confirmed: 4 open findings, 0 high, no seam entanglement. **Corrected: "no blocked work
expected" is wrong.** Two of the four findings have a repair whose *complete* form is gated —
one on an object-layout change and one on a public return-type change — and one more cannot be
implemented without a reference measurement this container cannot make. §6 gives the split.

---

## 2. Scope and file inventory

**In scope:** `modules/numerics` — 16 public headers, 1 body, 8 test files, component `Numerics`
(public dependencies `Buffers`, `Collections.Core`, `Core.Base`).

| File | Lines | Owned findings |
|---|---:|---|
| `include/System/Numerics/BigInteger.hpp` | 204 | — |
| `include/System/Numerics/BitOperations.hpp` | 113 | — |
| `include/System/Numerics/Complex.hpp` | 115 | **SR-AUD-277** |
| `include/System/Numerics/DivisionRounding.hpp` | 30 | — |
| `include/System/Numerics/GenericMathInterfaces.hpp` | 206 | **SR-AUD-278** |
| `include/System/Numerics/Matrix3x2.hpp` | 189 | — |
| `include/System/Numerics/Matrix4x4.hpp` | 484 | SR-AUD-276 (dependent) |
| `include/System/Numerics/Plane.hpp` | 82 | SR-AUD-276 |
| `include/System/Numerics/Quaternion.hpp` | 185 | — (already .NET-shaped, §4.4) |
| `include/System/Numerics/TotalOrderIeee754Comparer.hpp` | 71 | **SR-AUD-042** |
| `include/System/Numerics/Vector2/3/4.hpp` | 153/140/146 | **SR-AUD-276** |
| `include/System/Numerics/Colors/{Colors,Argb,Rgba}.hpp` | 273/5/5 | — (project extension, not .NET) |
| `src/System/Numerics/BigInteger.cpp` | 623 | — |
| tests (8 files) | 1,490 | SR-AUD-018 (core-owned), SR-AUD-042/276/277 |

`SharpRuntimeTests_Numerics` runs **299 tests** at the start of this review, all passing.

**Not in scope:** `System::Math`/`MathF` and the `System::Double`/`Single`/`Decimal`/`Int128`
wrappers (module `core`, covered by `NumericWrapperBoundaryPlan.md`,
`FloatingValueFidelityPlan.md` and `DecimalBoundaryFamilyPlan.md`); `System::Half` itself (core).

**Public-surface inventory.** Arbitrary precision (`BigInteger`: construction from `intcs`/`longcs`
/byte vectors, decimal `Parse`/`ToString`, `+ - * / %`, `<< >>`, `& | ^ ~`, comparison, `Abs`,
`Pow`, `GreatestCommonDivisor`, `ToByteArray`); bit intrinsics (`BitOperations`); complex arithmetic
and transcendentals (`Complex`); 2/3/4-D vectors, `Matrix3x2`, `Matrix4x4`, `Quaternion`, `Plane`;
the total-order comparer; the generic-math interface stubs; and the project's own `Colors` extension.

---

## 3. Confirmed finding inventory — measured current behaviour

Probes `build-probe/2167_probe1_numerics.cpp` (log `..._before.log`),
`2167_probe2_layout.cpp`, `2167_probe3_ubsan.cpp`, `2167_probe4_shift.cpp`.

| ID | Sev | Audit claim | Measured here |
|---|---|---|---|
| SR-AUD-042 | med | the comparer implements ordering only; cannot bind to `IEqualityComparer<T>` | **Confirmed.** `is_base_of_v<IEqualityComparer<float>, TotalOrderIeee754Comparer<float>>` is `false`; no `Equals`/`GetHashCode` exists. Ordering itself is correct: `Compare(-0,+0) = -1`, `Compare(NaN₁,NaN₂) = -1` for distinct payloads |
| SR-AUD-276 | med | zero/degenerate vector and plane normalization returns finite zero rather than .NET NaNs | **Confirmed, and materially wider and less uniform than recorded — see §4.1–§4.3** |
| SR-AUD-277 | med | `Complex::Abs` returns `Complex` not `double`; default text is fixed-six-decimal angle-bracket output instead of .NET's parenthesized pair | **Half confirmed, half contested — see §4.5 and §4.6** |
| SR-AUD-278 | med | generic-math static members are declarations with no definitions → unresolved references at final link | **Confirmed.** 44 static members across 9 interface templates are declared and never defined |

---

## 4. Premise corrections and extensions — all measured

### 4.1 The vector guard is not a *zero* guard; it is an "is the length not greater than zero" guard, and that catches three separate classes

`Vector2/3/4::Normalize` is `float l = v.Length(); return l > 0 ? v/l : v;`. Because
`NaN > 0` is **false**, the branch that the doc-comment describes as "length is zero" is taken in
three distinct situations, measured:

| Input | Length | Guard fires? | Result today |
|---|---|---|---|
| `{0,0,0}` | `+0` | yes | `(+0,+0,+0)` — the audit's case |
| `{-0,-0,-0}` | `+0` | yes | **`(-0,-0,-0)`** — signed zero preserved, not "finite zero" |
| `{NaN,0,0}` | `NaN` | **yes** | **`(NaN,0,0)`** — the NaN is *not* propagated to Y and Z |
| `{inf,NaN,-inf}` | `NaN` | **yes** | **`(inf,NaN,-inf)`** — an all-infinite vector is treated as "degenerate" |
| `{1e-25,1e-25,1e-25}` | `+0` (**`LengthSquared` underflows**) | **yes** | **the tiny vector, unnormalized** — a perfectly normalizable input |

The third and fifth rows are the correction that matters. The finding is recorded as being about
the **zero** vector; measured, the guard also fires for **any NaN component** and for **any vector
whose squared length underflows to zero** (roughly, all components below ~1e-22). Neither is
"degenerate geometry"; both are inputs a caller can reach with ordinary data.

### 4.2 `Plane::Normalize` does **not** share `Vector3::Normalize`'s behaviour, although the finding groups them

`Plane::Normalize` uses a completely different guard — `if (len < 1e-10f) return plane;` — and
`NaN < 1e-10f` is **false**, so it falls through and divides. Measured:

| Input | `Vector3`-style guard | `Plane` today |
|---|---|---|
| zero normal | returns input | returns input |
| `{NaN,0,0}` | **returns input unchanged** | **propagates NaN to all four fields** |
| `{1e-11,0,0}`, D=1 | would normalize | **returns input unnormalized** |
| `{1e-9,0,0}`, D=1 | would normalize | normalizes to `(1,0,0)`, **D scaled to 1e+09** |

So the module currently holds **three different answers to one structural question**: `>0` on the
length (vectors), `< 1e-10f` on the length (plane), and `> 1.192092896e-7f` on the length *squared*
(`Quaternion::Inverse`). Only the third is documented as matching a .NET threshold.

The audit's sentence "the direct probe records finite zero components for `Vector3::Normalize({})`
**and** `Plane::Normalize({})`" is true for the zero vector and false for every other special input.

### 4.3 Two dependents, measured, plus one that the finding does not name

- `Plane::CreateFromVertices` with three identical or three collinear points yields
  `Normal = (0,0,0)`, `D = -0` — a plane with no orientation, silently.
- `Matrix4x4::CreateLookAt(eye == target)` yields `M11 = M22 = M33 = 0`, `M44 = 1` — a **singular**
  view matrix, silently. `CreateLookAt` with `up` parallel to the forward axis does the same.
- **Not named by the finding, and .NET-identical:** `Vector3::Normalize({FLT_MAX,FLT_MAX,FLT_MAX})`
  returns `(0,0,0)` because `LengthSquared` **overflows** to `+inf` and the division underflows.
  That is a wrong answer, but it is the same wrong answer .NET's `value / value.Length()` gives, so
  it is recorded as a shared limitation and **not** repaired here.

### 4.4 `Quaternion::Normalize` already implements the behaviour SR-AUD-276 asks for — in this repository, with a doc-comment citing .NET

`Quaternion.hpp:88-92`: *"Matches .NET exactly: an unconditional `q/Length()`, including for a
zero-length `q`, which produces a NaN-component quaternion (not a caught/guarded case in .NET's own
implementation either — see Quaternion.cs)."* Measured, `Quaternion::Normalize({0,0,0,0})` returns
all-NaN. This is the strongest in-repository evidence for what SR-AUD-276 wants, **and it is the
reason the finding is real**: one member of the family follows .NET and four do not. It is *not*
sufficient evidence to make the change here — see §6.2.

### 4.5 `Complex::ToString` is worse than "fixed six decimals", and its bracket form is **contested by the audit's own citation**

Measured (`std::to_string`, which is fixed-six-decimal and locale-independent):

| Value | Today |
|---|---|
| `(1, 2)` | `<1.000000; 2.000000>` |
| `(1e-9, 0)` | `<0.000000; 0.000000>` — **the value is destroyed** |
| `(1e300, -1e300)` | a **619-character** string: two 309-digit decimal expansions |
| `(inf, -inf)` | `<inf; -inf>` — the **C library** spellings |
| `(NaN, 1)` | `<nan; 1.000000>` — likewise |

Two of these are decidable **from this repository alone**, with no reference tree: `System::Double::
ToString(double)` (`Double.hpp:1136`) is this port's settled renderer for a .NET `double`, it emits
the shortest round-trippable form via `std::to_chars`, and it emits `Infinity`/`-Infinity`/`NaN`.
`Complex::ToString` is the only place in the port that renders a `double` with `std::to_string`.

The **bracket and separator** half is a different matter. The audit calls the port's `<a; b>` form a
divergence and cites .NET's *constructor documentation example* (`(26.1, 18.06)`) as the target. But
the audit also links the **current .NET `Complex` source**, and `<a; b>` is what a current-source
reading gives. The two citations in one audit report disagree, `/rv` is absent, and no managed probe
was recorded for this member. **The bracket form is therefore not decided here** — it is pinned and
deferred (§6.3).

### 4.6 `Complex::Abs`'s divergence is a public *return type*, which is a different class of change from everything else in this namespace

Measured: `decltype(Complex::Abs(z))` is `Complex`; `decltype(Complex::AbsD(z))` is `double`.
Changing `Abs` to return `double` is source-breaking for any caller writing
`Complex m = Complex::Abs(z);` — and there is **no implicit conversion** either way
(`Complex(double, double)` has no defaulted second parameter), so such a caller gets a hard compile
error, not a silent change. **Zero in-repository callers use `Complex::Abs`**; all three call sites
use `AbsD`.

### 4.7 SR-AUD-278 is 44 members across 9 templates, not the 3 the probe names

Enumerated from the header: `IMinMaxValue` (2), `IAdditiveIdentity` (1), `IMultiplicativeIdentity`
(1), `ITrigonometricFunctions` (12), `IHyperbolicFunctions` (6), `ILogarithmicFunctions` (4),
`IExponentialFunctions` (3), `IPowerFunctions` (1), `IRootFunctions` (4), `IFloatingPointConstants`
(3) — **37 function declarations plus 7 aggregated through `IBinaryFloatingPointIeee754`**. The
audit's probe demonstrated three; every one of the 37 has the identical defect.

**The decisive context the finding does not cite** is in this repository: `Half.hpp:43-45` states
*"Generic math interface conformance (`INumber<Half>`, `IFloatingPointIeee754<Half>`,
`IMinMaxValue<Half>`, etc.) is **out of scope**, consistent with this codebase's position on C#
generic-math machinery elsewhere"*, and `DivisionRounding.hpp` says the same
(*"no concrete type currently implements the rounding-aware overload — this enum exists for API name
compatibility"*). The header's own preamble agrees: *"C++ templates naturally cover the same use
cases; these stubs exist for API name compatibility."* So the namespace's settled position is that
generic-math **conformance** is a documented reduction. What is *not* settled, and what the finding
correctly identifies, is that a documented reduction was implemented as **44 callable declarations
with no definitions** — the failure mode is an unresolved symbol at final link, arbitrarily far from
the call, rather than a diagnostic at the call site.

### 4.8 Observations outside the four findings — recorded, not repaired

| Observation | Measured | Disposition |
|---|---|---|
| `BigInteger(1) << INTCS_MAX` does not return within **30 s** (unbounded work); `<< INTCS_MIN` returns `0` promptly | probe 4 | **#2174** — needs a reference answer for what .NET does before a bound can be chosen |
| `BigInteger::Parse(" 1")` and `Parse("1 ")` **throw**; .NET's default `NumberStyles.Integer` includes `AllowLeadingWhite`/`AllowTrailingWhite` | probe 3 | **#2174** — recollection, not measurable here |
| `Vector3::Min/Max` with a NaN operand return `(NaN, 1, 1)` for **both**, via `std::min`/`std::max` | probe 3 | recorded; the audit lists it under "missing assertions", not as a finding |
| `Vector3::Clamp(5, min=10, max=0)` returns `0` — inverted bounds silently accepted | probe 3 | recorded; .NET documents `min > max` as undefined |
| `Plane::Normalize` on an already-unit plane is **bit-identical** through the call | probe 1 | recorded; .NET's `\|lenSq−1\|<ε` fast path would agree here, so no divergence is observable |

---

## 5. Root causes

### NM-A — a public interface is advertised by declaration and delivered by nobody (SR-AUD-278)

44 static members are declared in a header with no definition anywhere. C++ diagnoses that only at
final link, so a consumer's build fails with an unresolved symbol naming a mangled template
specialisation, at a point that has no connection to the call. The *reduction* is intended and
documented in three places; the *shape* of the reduction is the defect.

### NM-B — one structural question, three thresholds, none of them documented as the contract (SR-AUD-276)

"What does normalization do when the length is not usefully positive?" is answered `>0` in three
vector types, `<1e-10f` in `Plane`, and `>1.192092896e-7f` (on the square) in `Quaternion::Inverse`,
while `Quaternion::Normalize` refuses to answer it at all and follows .NET. The doc-comments state
only the zero case, so the NaN and underflow behaviour of §4.1 is undocumented in every one of them.

### NM-C — the comparer implements one half of a two-half .NET contract (SR-AUD-042)

`TotalOrderIeee754Comparer` provides the ordering and omits the equality/hash counterpart, so the
total-order semantics that distinguish `-0`/`+0` and NaN payloads cannot reach any hash-based API.

### NM-D — one type renders a `double` with the C++ standard library instead of with this port's own .NET-compatible renderer (SR-AUD-277, text half)

`Complex::ToString` is the only `double`-rendering site in the port that uses `std::to_string`.

### NM-E — a member's public *return type* was chosen for symmetry with its siblings rather than from .NET (SR-AUD-277, signature half)

---

## 6. Compatible / gated / deferred matrix

| Cause | Finding | Ticket | Class |
|---|---|---|---|
| NM-A | **SR-AUD-278** | **#2168** | **compatible** — closes the finding outright |
| NM-C | SR-AUD-042 (equality *semantics*) | **#2169** | **compatible subpart** — layout-neutral |
| NM-C | SR-AUD-042 (polymorphic *binding*) | **#2170** | **APPROVAL** — object-layout change, §6.1 |
| NM-D | SR-AUD-277 (number rendering) | **#2171** | **compatible subpart** |
| NM-E | SR-AUD-277 (`Abs` return type) | **#2172** | **APPROVAL** — public signature change, §6.3 |
| NM-B | SR-AUD-276 (contract + pins) | **#2173** | **compatible subpart** |
| NM-B | SR-AUD-276 (NaN normalization semantics) | **#2175** | **DEFERRED VERIFICATION**, §6.2 |
| — | §4.8 observations | **#2174** | **DEFERRED VERIFICATION** |

### 6.1 Why #2170 is gated: measured, it is an object-layout change

`build-probe/2167_probe2_layout.cpp`, three shapes compiled side by side:

| Shape | `sizeof` | `alignof` |
|---|---:|---:|
| today | **8** | 8 |
| **#2169** — non-virtual `Equals`/`GetHashCode` with `IEqualityComparer`'s exact signatures | **8** | 8 |
| **#2170** — `IEqualityComparer<T>` added as a second base | **16** | 8 |

The full repair adds a second vptr; the `IEqualityComparer` subobject lands at offset 8. That is a
**public object-layout change on a public type**, the class of change this repository has required
explicit per-action user approval for (#1788 `LinkedList<T>` 40→48, #1789 `BitArray::Enumerator`
32→40) and has blocked without it (#1889). It is not taken here.

The **risk is measurably low** and that is recorded so the decision can be cheap: the type is
header-only, stateless, and has **zero in-repository users outside its own six tests**, so no
in-repository object file embeds its size. #2169 delivers the *semantics* the finding is about
without touching layout, and is written with exactly the signatures `IEqualityComparer<T>` declares,
so #2170 becomes a two-line change (add the base, add `override`) if it is ever approved.

**The one-sentence approval #2170 needs:** *"`TotalOrderIeee754Comparer<float>`, `<double>` and
`<Half>` may grow from 8 to 16 bytes so they can also implement `IEqualityComparer<T>`."*

### 6.2 Why #2175 is deferred rather than implemented: the target is recalled, not measured

Making the four `Normalize` members divide unconditionally would change a **previously accepted
input from a finite answer to NaN**, with no diagnostic, in geometry code. This repository has drawn
that line consistently:

- Changes where the audit itself carried a **managed probe result** for .NET's answer were
  implemented without approval (`SystemConsoleNamespaceReviewPlan.md` §1: *"the target behaviour is
  measured, not recalled, which matters because `/rv` is absent"*).
- Changes that altered a **numeric answer for previously-accepted input** were gated
  (`CLAUDE.md`'s four documented incompatibilities — `Decimal::Parse`, the date/time parsers,
  `String::Format`, `Single`/`Double` `ToString` — were all approved Groups A–D of
  `RemainingApprovalDecisions.md`).

SR-AUD-276 has **no managed probe**. Its .NET claim is the auditor's reading of the .NET source,
corroborated by this repository's `Quaternion.hpp` doc-comment (§4.4) — good corroboration, but
still a reading, and it does **not** cover `Plane::Normalize`, whose .NET counterpart is believed to
carry an already-normalized fast path that the vector types do not have. Implementing four members'
NaN semantics, and choosing whether `Plane` gets that fast path, from recollection is exactly what
this programme forbids. **#2175 owns the question; #2173 pins the current behaviour so it cannot
change silently.**

### 6.3 Why #2172 is gated and #2171 is not

`Abs`'s return type is a public signature change with no conversion path (§4.6) → approval, wording
in §12.

`Complex::ToString`'s **number rendering** is decidable here because the target is this repository's
own `System::Double::ToString`, not a recollection of .NET (§4.5), and because the change is an
improvement under *both* readings of the contested bracket question: under the angle-bracket reading
it becomes exactly right, and under the parenthesis reading it becomes strictly closer. The bracket
and separator characters are **left exactly as they are** and pinned.

---

## 7. The deliberate behavioural breaks

Recorded so they are not discovered as surprises.

- **#2168:** calling any of the 44 generic-math static members becomes a **compile-time** error
  instead of a link-time error. **No program that builds today can be affected**: a call that exists
  today already fails to link, so nothing that currently links calls one.
- **#2171:** `Complex::ToString()` text changes for every value — `<1.000000; 2.000000>` becomes
  `<1; 2>`, `<inf; -inf>` becomes `<Infinity; -Infinity>`, `<nan; 1.000000>` becomes `<NaN; 1>`.
  The bracket/semicolon/space skeleton is unchanged. One existing test (`ComplexTests.ToStringFormat`)
  asserts `s.find("1.") != npos` and therefore **pins the defect**; it is rewritten, as #1828
  rewrote #1841's placeholder.
- **#2169:** additive only — two new non-virtual members on three specialisations.
- **#2173:** documentation and tests only; no behaviour changes.

---

## 8. Test matrix

**#2168** — a `test/consumer/*_negative.cpp` fixture with one site per interface family proving each
deleted member is rejected at compile time, plus a runtime test that the interface hierarchy still
instantiates and `INumberBase<int>::Radix` is still 2 (the surface `Task42Tests.cpp` uses).

**#2169** — for `float`, `double` and `Half`: `Equals` agrees with `Compare == 0` on every vector
below; `-0`/`+0` **not** equal; identical NaN payloads equal; **distinct** NaN payloads not equal;
`GetHashCode` equal whenever `Equals` is true, and differing for `-0` vs `+0`; ordinary values;
`±inf`; min normal; subnormal; max finite. Signature compatibility with `IEqualityComparer<T>`
asserted by a compile-time check so #2170 stays a two-line change.

**#2171** — `(1,2)`, `(0,0)`, `(-0,-0)`, `(0.1,0.2)`, `(26.1,18.06)`, `1e-9`, `1e300`, `1/3`,
`±inf`, NaN; and an invariance pin that the skeleton is still `<`, `; `, `>`.

**#2173** — `PIN_`-prefixed tests for every row of §4.1 and §4.2 (zero, negative zero, NaN component,
all-infinite, underflowing tiny vector, `Plane` above and below 1e-10, `Plane` with a NaN normal,
`Plane` already-unit bit-identity) and both dependents of §4.3, plus `Quaternion::Normalize`'s
existing NaN behaviour as the contrasting control.

**Invariance across the whole batch:** every ordinary numeric result must be bit-identical.

## 9. Sanitizer matrix

| Tool | Used for | Discriminating? |
|---|---|---|
| **UBSan** | the whole arithmetic surface — `BigInteger` signed boundaries, division, shifts, `BitOperations` rotations at `INTCS_MIN`/`INTCS_MAX`, vector/matrix extremes | **yes**, and it returned a **clean** result: see §10 |
| ASan | `BigInteger`'s heap limb vectors and byte-array paths under the same sweep | yes |
| LSan | no ownership change in any ticket | weak — not claimed |
| TSan | **no**: nothing in this namespace holds shared mutable state or documents a thread-safety contract | not run, not claimed |

## 10. The sanitizer result is a clean one, and that is reported as a result

`build-probe/2167_probe3_ubsan.cpp` compiles the **production** `BigInteger.cpp` with
`-fsanitize=undefined,address`, drives every case through `volatile` operands so nothing is
constant-folded, and covers `0`, `±1`, `INTCS_MIN`, `INTCS_MIN+1`, `INTCS_MAX`, `INTCS_MAX-1`,
`LONGCS_MIN`, `LONGCS_MIN+1`, `LONGCS_MAX`, `LONGCS_MAX-1`, ordinary positives and negatives,
`LONGCS_MIN / -1`, `LONGCS_MIN % -1`, division by zero, the base-10⁹ limb boundaries, shift counts
`0/1/31/32/63/64/65/127/-1/-63`, 14 parse texts, `BitOperations` at every rotation extreme, and the
vector/matrix/quaternion extremes.

**Exit 0, zero runtime errors.** `BigInteger(LONGCS_MIN)`'s magnitude is already computed by
well-defined unsigned subtraction with a comment naming the UBSan report that motivated it, and
`LONGCS_MIN / -1` correctly yields `9223372036854775808` rather than overflowing. **No ticket in
this batch is an arithmetic-UB repair**, and none is claimed to be. Sanitizer cleanliness is
reported here as evidence that the *defined-arithmetic* family (CCF-004) has **no member in this
namespace** — not as evidence of numerical parity, which it cannot show.

---

## 11. Dependency order and implementation order

1. **#2168** (SR-AUD-278) — independent; closes a finding outright.
2. **#2169** (SR-AUD-042 subpart) — independent.
3. **#2171** (SR-AUD-277 subpart) — independent.
4. **#2173** (SR-AUD-276 subpart) — must land **after** the §4 measurements are transcribed, and
   before #2175 could ever be started.
5. **#2170**, **#2172** — blocked on approval; **#2174**, **#2175** — blocked on evidence.

## 12. The two approval sentences, stated exactly

- **#2170:** *"`TotalOrderIeee754Comparer<float>`, `<double>` and `<Half>` may grow from 8 to 16
  bytes (a second vptr) so that they also implement `System::Collections::Generic::IEqualityComparer<T>`."*
- **#2172:** *"`System::Numerics::Complex::Abs` may change its public return type from `Complex` to
  `double`, breaking source compatibility for any caller that assigns its result to a `Complex`."*
  (If approved, `AbsD` becomes a redundant alias and its disposition — keep as a deprecated forward,
  or remove — is part of the same decision.)

## 13. Exclusions

1. `BitOperations`' missing `Crc32C` overloads and the exact `TrailingZeroCount(longcs)` overload —
   the audit itself records these as needing "an explicit compatibility-baseline decision", not as
   confirmed defects, and no baseline exists.
2. `BigInteger`'s absent culture/`NumberStyles`/`TryWriteBytes`/generic-math surface — the same.
3. The `FLT_MAX` normalization overflow (§4.3) — shared with .NET, not a divergence.
4. `Vector3::Min`/`Max`/`Clamp` NaN and inverted-bound behaviour (§4.8) — listed by the audit under
   "missing assertions", not as findings, and the .NET answer is not measurable here.
5. `Colors` — a project extension with no .NET counterpart.
6. SR-AUD-018 — a **core**-owned test-hygiene finding whose source list is `ObjectTests.cpp` and
   seven other core suites; `VectorMatrixTests.cpp` cites it but does not own it.

## 14. Completion criteria

SR-AUD-278 `remediated`; SR-AUD-042, SR-AUD-276 and SR-AUD-277 each carrying a completed compatible
subpart plus exactly one recorded gate (approval or evidence); every §8 row a permanent test; every
ordinary numeric result bit-identical; zero warnings, zero errors, two jobs maximum; module graph,
version seams unchanged and the negative-fixture count increased by exactly the #2168 fixture.

---

## 15. Implementation record — #2168, #2169, #2171, #2173

**All four compatible tickets done 2026-08-10.** `SharpRuntimeTests_Numerics` **299 → 335**, all
passing. Zero warnings, zero errors throughout, `--parallel 2`.

### 15.1 What landed

| Ticket | Finding | Commit | Tests |
|---|---|---|---|
| **#2168** | SR-AUD-278 — **closed** | `074adc8` | +6 runtime, +12 negative-fixture sites |
| **#2169** | SR-AUD-042 subpart | `e763968` | +10 |
| **#2171** | SR-AUD-277 subpart | `e6a6ce7` | +4 −1 replaced |
| **#2173** | SR-AUD-276 subpart | `959aed1` | +16 `PIN_` |

### 15.2 Measured before → after

| Door | Before | After |
|---|---|---|
| any generic-math static | compiles, **fails at final link** with an unresolved mangled symbol | rejected at the **call site** |
| `TotalOrderIeee754Comparer<float>::Equals` | **does not exist** | bit-pattern equality, agrees with `Compare == 0` on all 15 vectors |
| `Compare(-0, +0)` / `Equals(-0, +0)` | `-1` / — | `-1` / **`false`**, and the hashes differ |
| `Complex(1, 2).ToString()` | `<1.000000; 2.000000>` | `<1; 2>` |
| `Complex(1e-9, 0).ToString()` | `<0.000000; 0.000000>` — **value destroyed** | `<1e-09; 0>` |
| `Complex(1e300, -1e300).ToString()` | **619 characters** | `<1e+300; -1e+300>` |
| `Complex(inf, -inf)` / `(NaN, 1)` | `<inf; -inf>` / `<nan; 1.000000>` | `<Infinity; -Infinity>` / `<NaN; 1>` |
| every `Normalize` door | undocumented and inconsistent | **unchanged**, documented and pinned by 16 tests |
| `sizeof` of all three comparers | 8 | **8** (`static_assert`ed) |

### 15.3 Mutations — five applied, five counted

| Mutation | Result |
|---|---|
| Undelete `IMinMaxValue::MinValue` (restore the bare declaration) | negative site 1 **COMPILED**; other 11 still rejected. **Counts** |
| `Equals` → ordinary floating `x == y` | **4 clean failures** / 12 pass — exactly the signed-zero, NaN-payload, agreement and hash tests. **Counts** |
| binary64 hash truncates instead of folding | **2 clean failures**, including the vector pair written for it. **Counts** |
| `Complex::ToString` → `std::to_string` | **5 clean failures** — the four repair tests plus the skeleton pin. **Counts** |
| Apply the **#2175 shape** (unconditional division in `Vector3` and `Plane`) | **10 of 16 pins fire**, the 6 invariance/control tests stable. **Counts** — this is the right mutation for a pins ticket: it proves the pins detect the change they exist to make visible |

All five reverted; `git diff` clean afterwards; 335/335.

### 15.4 Honest limitations

- **No arithmetic-UB repair happened, and none is claimed.** §10's sweep was clean before any
  change. This namespace has no CCF-004 member.
- **`Complex::ToString`'s bracket skeleton is unresolved, not resolved.** #2174 owns it, and the
  `PIN_` test is the record of what would change.
- **Culture sensitivity is untouched.** .NET formats each `Complex` component with the current
  culture; this port is invariant throughout, a pre-existing reduction, not part of #2171.
- **TSan was not run and is not claimed** (§9).
- The `-0` normalization pin asserts only the sign bit, so it is the one pin the #2175 mutation
  does **not** fire; the other nine cover the same ground.

## 16. Namespace reconciliation — `System::Numerics`

| Finding | Sev | Compatible ticket | Gated remainder | Disposition |
|---|---|---|---|---|
| **SR-AUD-278** | med | **#2168** done | — | **remediated** |
| **SR-AUD-042** | med | **#2169** done | **#2170** needs_user — object layout 8 → 16 | **confirmed (design-complete)** |
| **SR-AUD-277** | med | **#2171** done | **#2172** needs_user — public return type | **confirmed (design-complete)** |
| **SR-AUD-276** | med | **#2173** done | **#2175** blocked on evidence | **confirmed (design-complete)** |

**`modules/numerics` is NOT fully closed, and that is the measured outcome rather than a shortfall
of effort.** One of four findings is remediated outright; the other three each have their
compatible subpart complete and exactly one recorded gate. **Two gates are approvals** — a public
object-layout growth and a public return-type change, both classes this repository has always
required an explicit per-action decision for — and **one is missing evidence**, not permission:
SR-AUD-276 carries no managed probe and `/rv` is absent, so implementing four members' NaN
semantics would be recollection.

Every finding maps to exactly one disposition; none disappeared; no `SR-AUD-*` identifier was
created and **numbering stays frozen at 364**. Two post-audit tickets carry no identifier:
**#2174** (four parity questions) and **#2175** (SR-AUD-276's remainder).

**Audit totals after this batch: 161 remediated / 203 confirmed (150 plain + 53 design-complete)
of 364.**

**Totals:** `numerics` 4 findings, 1 remediated, 3 design-complete, 0 unaddressed. Tests
**299 → 335**. No layout, signature, vtable, seam or module-graph change; negative fixtures
**12 → 13 files, 104 → 116 sites**.
