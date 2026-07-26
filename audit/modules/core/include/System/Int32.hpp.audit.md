# Audit: `modules/core/include/System/Int32.hpp`

## Metadata

- Audit status: AUDITED (552 lines, header-only implementation, full read).
- Validation: the focused Core.Base numeric/style filter passed 167/167 on
  2026-07-25.
- Cross-width probe: `/tmp/sharp_runtimervc_int64_audit_probe.cpp` links
  `build/libsharp_runtime_core.a` and was run with UBSan enabled.

## Assessment

The implementation handles the important signed hazards: range-safe parsing,
`DivRem` special cases, `Abs`, `CopySign`, magnitude helpers, bit operations,
and binary formatting all have meaningful tests.  The style parser explicitly
documents its invariant-culture limitation and has unusually extensive
regressions.  The remaining issue is shared `std::clamp` misuse on an inverted
range, plus unknown formats silently becoming decimal output.

## Finding references

- **SR-AUD-021:** the probe printed `int32(Q)=5`; `ToString(5, "Q")` should
  reject an unknown standard format instead of silently returning decimal.
- **SR-AUD-022:** `Clamp(5, 10, 0)` forwards an inverted range to
  `std::clamp`, which returned `0` in the probe.  .NET validates the interval
  and throws `ArgumentException` before selecting a bound.

## Other missing assertions and diagnostics

- Tests check valid Clamp cases only; add the inverted-bound exception case
  after the shared repair and remove `noexcept` as needed.
- `B` formatting is tested for small positive values only.  Add `MinValue`
  and width-32 raw-bit assertions.
- The formats implemented by this lightweight port are intentionally narrower
  than full culture/custom numeric formatting.  Unsupported formats must fail
  diagnostically rather than masquerade as `G` output (SR-AUD-021).

## Final assessment

Well-hardened core arithmetic and parsing; the confirmed defects are the
cross-cutting invalid Clamp precondition and unknown-format fallback, not a
new Int32-specific calculation error.
