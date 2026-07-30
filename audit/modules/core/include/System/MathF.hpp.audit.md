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
