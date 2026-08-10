# Audit: `modules/core/include/System/MathF.hpp`

## Metadata

- Audit status: AUDITED (331 lines, header-only implementation, full read).
- Validation: `MathTests.*:MathFTest.*` passed 174/174 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-math-audit-probe.cpp`, compiled with
  `c++ -std=c++20 -I modules/core/include -I include` on 2026-07-25.

## Assessment

MathF correctly implements several subtle .NET special cases that Math lacks:
base-log guards, NaN-propagating Min/Max, finite `ILogB` mapping, precision
limits, and large-value rounding protection.  Three public boundaries remain
unchecked or depend on mutable C++ process state: inverted Clamp bounds,
invalid rounding enums, and `ToEven` rounding under a changed floating-point
rounding mode.

## Finding references

- **SR-AUD-022:** `Clamp(5.0f, 10.0f, 0.0f)` returns `10` rather than throwing
  `ArgumentException`.  The direct probe reports `mathf_clamp_inverted=10`;
  .NET validates `min > max` before selecting a float Clamp result:
  <https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Math.cs.html>.
- **SR-AUD-036:** `Round(1.9f, static_cast<MidpointRounding>(99))` returns `2`
  through the switch default instead of `ArgumentException`.  The .NET MathF
  implementation throws for that default case:
  <https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/MathF.cs.html>.
  **Remediated (#1855, 2026-07-30):** `Round(float, MidpointRounding)`'s
  `switch` default now throws `System::ArgumentException(paramName "mode")`
  instead of `std::nearbyintf`. The `Round(float, int, MidpointRounding)`
  overload validates the mode through the funnel for magnitudes `< 1e8`
  (matching .NET's own MathF.cs). **Closes CCF-008.**
- **SR-AUD-040:** `Round(float)` and `Round(float, MidpointRounding::ToEven)`
  call `std::nearbyintf`, which observes the ambient C++ `fesetround` mode.
  With `FE_UPWARD`, the probe returns `3` for `2.5f` in both overloads instead
  of the documented ties-to-even result `2`.  The sibling double Math API has
  an explicit ambient-mode regression test and a dedicated mode-independent
  implementation; the float API has neither.

## Required post-audit verification

Check Clamp bounds and `MidpointRounding` before calculation.  Replace
`nearbyintf` for `ToEven` with a mode-independent implementation equivalent to
the guarded double path, then test positive/negative ties and the no-mode
overload under both upward and downward C++ rounding modes.  Preserve the
existing precision and large-magnitude guards.

## Other missing assertions and diagnostics

- The MathF Clamp test covers only ordered intervals.
- All named rounding modes are present only implicitly or through normal
  values; no invalid enum cast has an exception assertion.
- Unlike `MathTests`, the float suite does not alter `fesetround`, so its
  current green `Round` assertions cannot detect process-global mode leakage.

## Final assessment

MathF has good normal and special-value coverage, but the uncovered public
validation and ambient-state behavior produce reproducible wrong results.  No
implementation was modified during this audit.

### SR-AUD-040 remediated — ticket #2233 (2026-08-10)

`MathF` gains `roundToEvenImpl(float)`, the single-precision mirror of the
`Math::roundToEvenImpl(double)` funnel this finding's own text points at: a
`std::floor`/`std::fmod` implementation, both of which are specified with fixed
rounding behaviour independent of `fesetround`, with the same signed-zero
preservation. `MathF::Round(float)` and the `ToEven` arm of
`MathF::Round(float, MidpointRounding)` now route through it, which also repairs
`MathF::Round(float, intcs)` and `MathF::Round(float, intcs, MidpointRounding)`.

**PREMISE EXTENSION — the defect reaches four production doors, not two.** The
finding names the two `MathF` doors and states that *"the sibling double Math API
has … a dedicated mode-independent implementation"*. That is true of `Math` and
**false of `Double`**: `Double.hpp:292` was `std::nearbyint(x)` and carried the
identical defect, as did `Single.hpp:247`. A repository-wide grep for
`nearbyint`/`rint` outside `tests/` returns exactly four production sites, all
four repaired by #2233:

| Site | Reached from |
|---|---|
| `MathF.hpp:50` — `Round(float)` | direct |
| `MathF.hpp:236` — `Round(float, MidpointRounding)` `ToEven` arm | direct, and both digits overloads |
| `Single.hpp:247` — `Round(float)` | direct; `Round(float, intcs)` inherits via the `MathF` funnel |
| `Double.hpp:292` — `Round(double)` | direct |

**The defect is also not `FE_UPWARD`-only**, which the finding's text implies.
Measured across all four IEEE modes: `FE_UPWARD` turned `Round(2.5f)` into `3`,
while `FE_DOWNWARD` and `FE_TOWARDZERO` turned `Round(3.5f)` into `3` and
`Round(-2.5f)` into `-3`.

Measured on the shipped library before the edit
(`build-probe/2229_probe1_before.log`, group `[040]`): **64 cases, 18 wrong**.
After: **4 wrong**, and those four are attributed, not waved away.

**The residual, measured rather than argued.** Under `FE_DOWNWARD` and
`FE_TOWARDZERO`, `MathF::Round(2.25f, 1)` returns `2.1999998` rather than
`2.2000000`. `build-probe/2229_probe2_digits.log` shows `Math::Round(2.25, 1)` —
the sibling this finding treats as the *correct reference*, untouched by this
ticket — deviating identically, and matching a bare `22.0 / 10.0` to the bit. The
digits overloads scale by a power of ten, round, and divide back; the final
**division** observes the ambient mode exactly as every other C++ floating-point
operation does, including `printf`'s own decimal conversion. The residual is
therefore ordinary ambient-mode arithmetic, not a rounding-rule defect, and is
not attributable to this repair. No ticket is opened for it because a ticket
scoped to `Round` would misdescribe it: the deviation belongs to every
`float`/`double` expression in the library under a non-default mode.
`build-probe/2229_probe3_contract.cpp` states the claim precisely — the rounding
**rule** is mode-independent at every door, and the digits doors agree with the
reference sibling to 1e-6 in every mode — and reports **104 cases, 0 wrong**.

`MidpointRounding` validation (CCF-008, closed) is untouched, as are the
`>= 1e8` / `>= 1e16` unchanged-value guards and every other rounding arm. All six
public doors keep their existing signatures and `noexcept`; the only surface
change is the **added** inline static `MathF::roundToEvenImpl(float)`, mirroring
the public `Math::roundToEvenImpl(double)` that already existed. No layout,
vtable or existing mangled name changed. +14 test cases in
`modules/core/tests/System/NumericSpecialValueTests.cpp`, which mirror
`MathTests.cpp`'s existing `RoundingModeGuard` rather than inventing a second
pattern and exercise every door under all four IEEE modes. Family plan:
`docs/CoreNumericSpecialValueRoundingFamilyPlan.md` §4.4 and §4.4.1.
