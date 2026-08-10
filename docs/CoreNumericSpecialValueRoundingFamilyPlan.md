<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` numeric special-value and rounding-contract family — plan

**Review ticket #2229** (`P1`, `review`, area `core`). Written 2026-08-10.
This document is the review deliverable; no production source changed under #2229 itself.

It issues **no `SR-AUD-*` identifier**. Audit numbering stays frozen at **364**, and all four
members keep status `confirmed` until their own implementation tickets land.

**This is one bounded family of `modules/core`, not a `modules/core` namespace review.** After
this family lands, `modules/core` still has **56** open findings. Nothing here closes the module.

---

## 1. Exact scope

| Finding | Severity | Status at the open of #2229 | Owning report |
|---|---|---|---|
| **SR-AUD-034** | medium | `confirmed` | `audit/modules/core/include/System/Single.hpp.audit.md` |
| **SR-AUD-037** | medium | `confirmed` | `audit/modules/core/include/System/Decimal.hpp.audit.md` |
| **SR-AUD-039** | medium | `confirmed` | `audit/modules/core/src/System/Math.cpp.audit.md` |
| **SR-AUD-040** | medium | `confirmed` | `audit/modules/core/include/System/MathF.hpp.audit.md` |

Verified at the open of this ticket rather than inherited: the index was re-parsed **by finding
identifier** (rows SR-AUD-029/033/249/286/307 carry extra table columns, so a fixed-column parser
reports a false 363), giving **185 remediated / 124 confirmed / 55 confirmed (design-complete) /
364 total**, with **60** `modules/core` findings open. All four members are `medium` and
`confirmed`; none is remediated by later work; `plan.sqlite3` contains no other ticket naming any
of them, so #2229 is not stale.

---

## 2. Why these four are one family

Every member is a **public numeric operation whose .NET contract names a specific special-value or
rounding outcome, implemented by delegating to a C++ primitive or comparison that does not provide
it**. That is a single root-cause shape, and three of the four have an in-repository *sibling that
already implements the contract correctly* — which is what makes the divergence measurable rather
than arguable:

| Finding | The wrong door | The already-correct sibling |
|---|---|---|
| SR-AUD-034 | `Single::IsPositive` adds `&& !std::isnan(value)` to the sign-bit test | `Double::IsPositive` is `!std::signbit(d)` |
| SR-AUD-039 | `Math::Log(double, newBase)` is a bare `std::log(a) / std::log(newBase)` | `MathF::Log(float, float)` implements .NET's four guards |
| SR-AUD-040 | `MathF` ties-to-even is `std::nearbyint`, which follows `fesetround()` | `Math::roundToEvenImpl` is a mode-independent `floor`/`fmod` funnel |
| SR-AUD-037 | `Decimal::ToOACurrency` scales then **truncates** | *(none — see §4.2)* |

SR-AUD-037 has no sibling, so it is the family's one **cause-only** member: same shape (a coarser
C++/port primitive standing in for a specified rounding rule), no paired API to compare against.

This grouping is a **review** unit. The four share no code and are repaired independently, so they
split into four bounded implementation tickets, exactly as #2223 did.

---

## 3. Before evidence, measured 2026-08-10

One probe, `build-probe/2229_probe1_before.cpp`, links the shipped `libsharp_runtime_core.a` and
prints an OK/BAD verdict per case, so the same binary re-run after each repair is a direct
comparison. Log: `build-probe/2229_probe1_before.log`.

**106 cases, 27 wrong.**

| Group | Cases | Wrong before |
|---|---|---|
| SR-AUD-034 | 12 | 1 |
| SR-AUD-037 | 15 | 5 |
| SR-AUD-039 | 15 | 3 |
| SR-AUD-040 | 64 | 18 |

### 3.1 A premise correction the probe itself produced

The probe's **first draft expected `Math::Log(1, 0) == +0`** and flagged both the `Math` case and
the `MathF` control as wrong. The control disagreeing is the discriminator: `MathF::Log` is a
faithful transcription of .NET's algorithm, so a control it fails is an error in the expectation,
not in the port. .NET's guard is

```csharp
if ((a != 1) && ((newBase == 0) || double.IsPositiveInfinity(newBase))) return double.NaN;
```

so with `a == 1` it does **not** fire and the result falls through to `Log(1)/Log(0)`, i.e.
`0 / -Inf == -0`. The correct expectation is a **signed** zero: `Log(1, 0) == -0` and
`Log(1, +Inf) == +0`. Both already hold in the port. Corrected in the probe before the baseline was
taken; the 27 above is the corrected count (the uncorrected first run said 29).

---

## 4. The four members, individually

### 4.1 SR-AUD-034 — `Single::IsPositive` excludes a positive-sign NaN

`Single.hpp:101` is `!std::signbit(value) && !std::isnan(value)`. .NET defines the generic-math
predicate on the raw representation (`SingleToInt32Bits(value) >= 0`), so a positive-sign NaN is
positive and a negative-sign NaN is negative. Measured: `Single::IsPositive(+NaN)` is `false`.

**One shape only**, confirmed by measurement: `IsNegative` is already the bare `std::signbit`, both
`Double` predicates are already correct, and `+0`/`-0`/finite/infinite results are unchanged. There
is **no production consumer** of `Single::IsPositive` in `modules/`, and **no test pins the wrong
answer** — both greps are recorded in §9.

### 4.2 SR-AUD-037 — `Decimal::ToOACurrency` truncates

`Decimal.hpp:743` computes `Truncate(*this * Decimal(10000))`. .NET's `Decimal.ToOACurrency`
multiplies by 10,000 and rounds to the **nearest** integer. Measured against the two values the
.NET documentation tabulates: `0.123456789` gives `1234` (documented `1235`) and
`-79.228162514264337593543950335` gives `-792281` (documented `-792282`).

**The tie rule is the one element the documented examples cannot decide.** Neither tabulated value
is a midpoint. .NET performs the scale reduction through
`DecCalc.VarCyFromDec` → `InternalRound(ref, scale, MidpointRounding.ToEven)`, i.e. banker's
rounding, which is also what every other rounding funnel in this port defaults to
(`Decimal::Round`, `Math::Round`, `MathF::Round`). `/rv` is absent here to confirm the exact call,
so:

- **ToEven is implemented**, because it is both the .NET-internal rule and this port's own default;
- the choice is **pinned by a test** so a later change is visible rather than silent;
- the residual question becomes a **deferred-verification ticket (#2234)**, following the #2060 /
  #2070 / #2130 convention for a parity question with no measurable answer in this container.

Overflow behaviour was measured, not assumed: both `Decimal::MaxValue` and `1000000000000000`
already throw `OverflowException` today, but with the messages `"Decimal overflow."` and
`"Value was either too large or too small for an Int64."`. .NET raises `SR.Overflow_Currency`.
The exception **type** is already right; only the message moves.

### 4.3 SR-AUD-039 — `Math::Log(double, newBase)` has no special cases

`Math.cpp:157` is `std::log(a) / std::log(newBase)`. Measured: `Log(5, 1)` gives `+Inf`,
`Log(5, 0)` gives `-0`, `Log(5, +Inf)` gives `+0`; all three must be `NaN`.

**Three shapes**, matching the finding's own text (base one, base zero, base positive infinity).
The NaN-argument cases and the `a == 1` fall-through cases already behave correctly, and are
included as controls so the repair cannot over-reject.

### 4.4 SR-AUD-040 — ambient-mode-dependent ties-to-even, at **four** doors, not two

**PREMISE EXTENSION, measured.** The finding names `MathF::Round(float)` and
`MathF::Round(float, MidpointRounding::ToEven)`, and says *"the sibling double Math API has …
a dedicated mode-independent implementation"*. That is true of **`Math`** and false of **`Double`**:
`Double.hpp:292` is `std::nearbyint(x)` and carries the identical defect. A repository-wide grep for
`nearbyint`/`rint` outside tests returns exactly four production sites:

| Site | Reached from |
|---|---|
| `MathF.hpp:50` — `Round(float)` | direct |
| `MathF.hpp:236` — `Round(float, MidpointRounding)` `ToEven` arm | direct; also `MathF::Round(float, intcs)` and `MathF::Round(float, intcs, ToEven)` |
| `Single.hpp:247` — `Round(float)` | direct; `Single::Round(float, intcs)` funnels into `MathF` and inherits the arm above |
| `Double.hpp:292` — `Round(double)` | direct |

`Math::Round` in every form is already correct and is the family's control.

**The defect is not FE_UPWARD-only.** Measured across all four IEEE modes: `FE_UPWARD` turns
`Round(2.5f)` into `3`, and `FE_DOWNWARD`/`FE_TOWARDZERO` turn `Round(3.5f)` into `3` and
`Round(-2.5f)` into `-3`. The *digits* overloads inherit it — `Round(2.25f, 1)` returns
`2.3000002` under `FE_UPWARD` and `2.1999998` under `FE_DOWNWARD`.

Repairing `MathF` fixes the `Single` digits path for free, because #1927 already routed
`Single::Round(float, intcs)` through `MathF::Round(x, digits, ToEven)`.

#### 4.4.1 A residual the repair does **not** own, measured rather than argued

After #2233 the original probe still reported **4 of 106** wrong, all the same shape: under
`FE_DOWNWARD` and `FE_TOWARDZERO`, `MathF::Round(2.25f, 1)` and `Single::Round(2.25f, 1)` return
`2.1999998` rather than `2.2000000`. A second probe, `build-probe/2229_probe2_digits.cpp`, was
written to attribute it, and the attribution is unambiguous:

| Mode | `MathF::Round(2.25f,1)` | `Math::Round(2.25,1)` | bare `22.0 / 10.0` |
|---|---|---|---|
| `FE_TONEAREST` | 2.2000000476837158 | 2.2000000000000002 | 2.2000000000000002 |
| `FE_DOWNWARD` | 2.1999998092651367 | **2.1999999999999997** | **2.1999999999999997** |
| `FE_TOWARDZERO` | 2.1999998092651367 | **2.1999999999999997** | **2.1999999999999997** |

`Math::Round(double, intcs)` — the sibling this family treats as the *correct reference*, and which
this batch does not touch — deviates **identically**, and matches a bare division to the bit. The
digits overloads scale by a power of ten, round, and divide back; the final **division** observes
the ambient mode exactly as every other C++ floating-point operation does, including `printf`'s own
decimal conversion (visible in the table's last digit under `FE_UPWARD`).

So the residual is **ordinary ambient-mode arithmetic, not a rounding-rule defect**, and it is not
attributable to #2233. The contract the repair actually establishes is stated precisely:

> The rounding **rule** is ties-to-even at every door regardless of the ambient mode. The
> arithmetic that scales to and from a digit position is not mode-independent, and cannot be
> without either saving and restoring a process-global register — which is not thread-safe — or
> replacing the scale round trip with a decimal algorithm.

**No ticket is opened for it**, deliberately. A ticket scoped to `Round` would misdescribe the
condition: the deviation belongs to every `float`/`double` expression in the library under a
non-default mode, not to these functions. Recorded here so it is neither hidden nor inflated.

`build-probe/2229_probe3_contract.cpp` is the original probe with the two digits rows held to an
exact value under `FE_TONEAREST` only, plus an unconditional row asserting the digits doors agree
with the reference sibling to 1e-6 in every mode — the part the repair *does* own. It reports
**104 cases, 0 wrong**. The original probe file is left unamended so that
`2229_probe1_before.log` and the source that produced it continue to match.

---

## 5. CCF relationships — stated, and neither cause reopened

- **CCF-007** (*"the binary float wrappers delegate public edge semantics to unsuitable native
  primitives"*) is the closest existing cause and its subject genuinely overlaps SR-AUD-034 and the
  `Single`/`Double` half of SR-AUD-040. It is **not** extended to them. Its membership is explicit
  and closed — SR-AUD-029 through SR-AUD-033, all five `remediated` — and its listed subjects are
  decimal rounding precision, `IsPow2`, `ilogb`, Pi-scaled trigonometry and text conversion. None
  of them is a sign-bit predicate or an ambient-FP-mode dependency. These four findings are
  **adjacent**, not members. Broadening a cause because the same repair technique is useful is
  exactly what CCF-015's own SR-AUD-294 note forbids.
- **CCF-008** (`MidpointRounding` validation across `Decimal`/`Math`/`MathF`) is **closed** and its
  sole member SR-AUD-036 is `remediated`. SR-AUD-040 shares CCF-008's *files* and none of its
  subject: CCF-008 is about rejecting an out-of-range `mode`, SR-AUD-040 about the value a valid
  `ToEven` produces. Not a member, not an occurrence.
- **CCF-003** and **CCF-006** are closed; neither is touched.
- **No new CCF is minted.** The four-way shape described in §2 is a real commonality, but this
  family repairs every occurrence it names inside its own tickets, so a cause identifier would
  carry no work. Recorded here as a *named shape without an identifier*, following the #2148
  precedent for the signed-length-into-unsigned-count idiom.
- **CCF-019, CCF-021/#2131 and CCF-022/#2109 are untouched** by this family.

---

## 6. Compatibility, ABI, layout and `noexcept`

| Ticket | Signature | `noexcept` | Layout / vtable | Exported symbols | Observable value change |
|---|---|---|---|---|---|
| #2230 SR-AUD-034 | unchanged | unchanged (`noexcept`, no throw added) | none | none | `Single::IsPositive(+NaN)` `false` → `true` |
| #2231 SR-AUD-037 | unchanged | unchanged (already throwing) | none | none | non-representable-at-4dp values round instead of truncate; two overflow **messages** change |
| #2232 SR-AUD-039 | unchanged | unchanged (not `noexcept`) | none | none | three special bases return `NaN` |
| #2233 SR-AUD-040 | unchanged | unchanged (`noexcept` preserved; the replacement cannot throw) | none | one **added** inline static, `MathF::roundToEvenImpl(float)` | ties-to-even becomes mode-independent |

Every repair is inside a function body except the one **additive** static in `MathF`, which mirrors
the public `Math::roundToEvenImpl(double)` that already exists — deliberately symmetric, since this
is the sibling-parity family. No member is added or removed from any class with storage, no virtual
function is touched, and no existing mangled name changes.

**No approval boundary is crossed.** No public representation, layout, signature or exception
specification changes anywhere in this family.

---

## 7. Behaviour that deliberately does not move

- `Char`/integer/`Half` predicates: untouched. `Single::IsNegative` was already correct.
- `MidpointRounding` validation (CCF-008): untouched; the `switch default` still throws.
- The `>= 1e8` / `>= 1e16` unchanged-value guards in the digits overloads: untouched.
- `Math::Round` in every form: untouched — it is the control.
- `Decimal::FromOACurrency`: untouched. Only the `To` direction is in scope.
- `Math::Log(a)`, `Log2`, `Log10`: untouched; only the two-argument overload has special cases.

---

## 8. Ticket split

| Ticket | Finding | Scope |
|---|---|---|
| **#2229** | — | this review |
| **#2230** | SR-AUD-034 | `Single::IsPositive` becomes the bare sign-bit test |
| **#2231** | SR-AUD-037 | `Decimal::ToOACurrency` rounds to the nearest currency unit and reports overflow as .NET does |
| **#2232** | SR-AUD-039 | `Math::Log(double, newBase)` gains .NET's four special cases |
| **#2233** | SR-AUD-040 | ties-to-even becomes independent of `fesetround()` at all four doors |
| **#2234** | — | deferred verification: does .NET's `ToOACurrency` break a tie to even or away from zero? |

All four implementation tickets are **independent**; any may land alone.

---

## 9. Greps run for this review, with their results

- `nearbyint`/`std::rint` in `modules/` outside `tests/`: **4 production sites**, tabulated in §4.4.
- `IsPositive` in `modules/` outside `tests/`: **no production consumer** of the float/double forms.
- `IsPositive` with a NaN argument in `modules/core/tests/`: **no match** — no test pins the wrong
  answer, so #2230 corrects no existing assertion.
- `fesetround`/`FE_UPWARD` in `modules/`: one existing regression guard,
  `MathTests.cpp:687-710`, covering the `Math` sibling only. #2233 mirrors its `RoundingModeGuard`
  rather than inventing a second pattern.

---

## 10. Test matrix

| Ticket | Suite | Cases |
|---|---|---|
| #2230 | `NumericSpecialValueTests.cpp` | both NaN signs through `IsPositive`/`IsNegative` on `Single` and `Double`; `±0`, finite, infinite controls |
| #2231 | `NumericSpecialValueTests.cpp` | both documented .NET values; above/below midpoint, both signs; the three pinned tie cases; exactly-4-dp controls; both overflow doors |
| #2232 | `NumericSpecialValueTests.cpp` | base 1 / 0 / +Inf; NaN in each position; the two signed-zero fall-throughs; ordinary controls; `MathF` parity |
| #2233 | `NumericSpecialValueTests.cpp` | every door × `FE_TONEAREST`/`FE_UPWARD`/`FE_DOWNWARD`/`FE_TOWARDZERO`, under a restoring guard; signed zero, non-finite and above-limit controls |

## 11. Sanitizer matrix

**No sanitizer run is planned, and that is a measured decision, not an omission.** Every defect in
this family produces a *wrong answer* from fully defined operations: a comparison that adds a
predicate, a truncation where rounding was specified, a missing special case, and a correctly
defined library call that reads a mutable global mode. There is no out-of-bounds access, no
uninitialised read, no signed overflow and no invalid conversion anywhere in the four repairs, so
ASan/UBSan/`float-cast-overflow` cannot discriminate the failure class. This follows the #2223
precedent explicitly: sanitizers are used where they decide something, not to say they were run.

## 12. Family completion criteria

1. All four implementation tickets `done`, or explicitly blocked with a stated reason.
2. The probes re-run against the repaired library: `2229_probe3_contract` **0 wrong of 104**, and
   `2229_probe1_before` **4 wrong of 106**, those four being the over-strong digits rows attributed
   to shared ambient-mode arithmetic in §4.4.1.
3. Permanent regressions land in a tracked suite, not only in the probe.
4. `audit/AUDIT_FINDINGS_INDEX.md` and the four owning per-file reports record each transition,
   with the §3.1, §4.4 and §4.4.1 premise corrections appended rather than rewritten.
5. No `SR-AUD-*` identifier created; numbering stays frozen at 364.
6. `modules/core` open count falls 60 → 56.

---

## 13. Outcome, measured 2026-08-10

All four implementation tickets landed. **None blocked, none partial.**

| Probe | Before | After |
|---|---|---|
| `2229_probe1_before` (original) | 106 cases, **27 wrong** | 106 cases, **4 wrong** (§4.4.1) |
| `2229_probe3_contract` (precise) | — | 104 cases, **0 wrong** |

Per group, on the original probe: SR-AUD-034 1 → 0, SR-AUD-037 5 → 0, SR-AUD-039 3 → 0,
SR-AUD-040 18 → 4 (all four the §4.4.1 residual).

**+16 permanent regressions** in `modules/core/tests/System/NumericSpecialValueTests.cpp`.

**No sanitizer was run**, per §11: every defect here was a wrong answer from fully defined
operations, so ASan/UBSan cannot discriminate the failure class. **No mutation testing was run**
either: each repair is a two-to-six-line body whose every branch is directly asserted by a probe row
and a test case, and the probes' before/after delta already demonstrates the tests fail against the
unrepaired code — which is what a mutation would be constructed to show.
