# Audit: `modules/core/include/System/Int64.hpp`

## Metadata

- Audit status: AUDITED (410 lines, header-only implementation, full read).
- Validation: `Int64Tests.*:Int64NewTests.*:UInt64Test.*` passed 85/85 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp_runtimervc_int64_audit_probe.cpp`, compiled with
  `-fsanitize=undefined` and `build/libsharp_runtime_core.a`.

## Assessment

This is a substantially hardened wrapper: parsing translates standard-library
errors, `DivRem` protects divide-by-zero and `MinValue / -1`, bit operations
use `<bit>`, and `CopySign`/magnitude logic explicitly accounts for the signed
minimum.  Binary formatting is implemented and tested.  Its remaining
weaknesses are shared generic numeric behavior: unknown formats fall through
to decimal, and `Clamp` delegates an invalid interval to `std::clamp`.

## SR-AUD-022 — medium — numeric `Clamp` does not reject inverted intervals and reaches invalid `std::clamp` use

`Int64::Clamp` is marked `noexcept` and calls `std::clamp(value, min, max)`
without checking `min > max`; `UInt64::Clamp` does the same.  `std::clamp`
requires an ordered range, while .NET checks `min > max` before it selects a
bound.  The direct probe observed both public APIs silently returning the
upper argument for an inverted interval:

```
int64_clamp=0
uint64_clamp=0
```

for `Clamp(5, 10, 0)`, where .NET throws `ArgumentException`.  The current
.NET implementation performs that validation before its comparisons:
<https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/Math.cs>.
`UInt128::Clamp` likewise omits the range validation, although it does not use
`std::clamp`; all three audited numeric APIs are covered by this finding.

### Required post-audit verification

Validate `min > max` first and throw `System::ArgumentException` in every
affected wrapper; remove `noexcept` where necessary.  Add an exact invalid-range
test for signed and unsigned 64/128-bit `Clamp`, then rerun it under UBSan for
the `std::clamp` callers.

## Finding reference

**SR-AUD-021:** `ToString(5, "Q")` returns `"5"` rather than rejecting an
unknown standard format.  The same probe confirmed the behavior for `UInt64`
and both audited 128-bit wrappers.

## Other missing assertions and diagnostics

- Existing tests cover `B` for positive values but not `MinValue`, whose .NET
  binary format must use the raw two's-complement bits.
- The number-style parser is thoroughly reused but its culture provider is
  intentionally ignored; callers need a documented invariant-only contract.
- No test asserts `Clamp`'s invalid-interval error, so the `noexcept` API
  mismatch remained invisible.

## Final assessment

Strongly hardened signed arithmetic with one shared invalid-range contract
defect and one shared format-diagnostic defect.  The latter belongs to
SR-AUD-021; no implementation was modified.
