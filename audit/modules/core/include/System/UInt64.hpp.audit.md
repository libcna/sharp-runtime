# Audit: `modules/core/include/System/UInt64.hpp`

## Metadata

- Audit status: AUDITED (273 lines, header-only implementation, full read).
- Validation: Core.Base `UInt64Test.*` passed 17/17; integration
  `UInt64Tests.*` passed 36/36 on 2026-07-25.
- Direct probe: `/tmp/sharp_runtimervc_int64_audit_probe.cpp`, compiled against
  `build/libsharp_runtime_core.a` with UBSan enabled.

## Assessment

Decimal and style-based parsing correctly distinguishes invalid, overflow, and
negative unsigned input.  Normal `X`, `D`, and `G` formatting, division
checks, rotations, and bit operations are well covered.  The header still
falls back to decimal for unknown formats, has no binary format despite the
signed sibling implementing it, and leaves `std::clamp` with an invalid range.

## SR-AUD-023 — medium — unsigned integral binary formatting is missing and silently returns decimal

`UInt64::ToString` supports only `X/x`, `D/d`, and `G/g`.  Its final fallback
returns decimal, so the valid integral binary specifiers `"B"` and `"B8"`
both return `"5"` for the value 5.  The probe showed the contrast with Int64:

```
int64(B)=101
uint64(B)=5
int64(B8)=00000101
uint64(B8)=5
```

The .NET standard numeric format contract supports `B`/`b` for integral types
from .NET 8 and specifies zero-padding through precision:
<https://learn.microsoft.com/en-us/dotnet/standard/base-types/standard-numeric-format-strings>.
`UInt128` has the same missing capability and is included in this finding.

### Required post-audit verification

Implement `B/b` with a validated non-negative precision for UInt64 and UInt128,
using the raw unsigned bit pattern.  Add positive, zero, width, `MaxValue`, and
invalid-format tests.  Preserve the project’s selected scope for other numeric
formats separately rather than treating unknown input as general decimal.

## Finding references

- **SR-AUD-021:** `ToString(5, "Q")` silently returns `"5"`; malformed widths
  are translated, but unknown format types are not.
- **SR-AUD-022:** `Clamp(5, 10, 0)` passes an inverted range to `std::clamp`
  and returns `0` instead of raising `System::ArgumentException`.

## Other missing assertions and diagnostics

- No tests combine `B` with a value using bit 63, so the absent raw-bit
  conversion cannot be detected.
- The Core.Base suite lacks `Clamp(min > max)` and format-unknown assertions;
  the integration suite repeats only valid cases.
- `GetHashCode_NonZero` in integration is too specific: zero is a legal hash
  code.  Correctness tests should assert equality consistency, not a non-zero
  implementation detail.

## Final assessment

Good parsing and common operations, but public formatting and invalid-range
behavior diverge at observable API boundaries.  These are recorded as
SR-AUD-021 through SR-AUD-023.
