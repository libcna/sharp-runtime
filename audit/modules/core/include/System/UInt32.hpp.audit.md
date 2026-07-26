# Audit: `modules/core/include/System/UInt32.hpp`

## Metadata

- Audit status: AUDITED (297 lines, header-only implementation, full read).
- Validation: the focused Core.Base numeric/style filter passed 167/167 on
  2026-07-25.
- Cross-width probe output includes `uint32_clamp=0`, `uint32(Q)=5`, and
  `uint32(B)=5` for the audited boundary inputs.

## Assessment

Parsing correctly protects the unsigned negative-sign cases (including `-0`),
uses a shared NumberStyles grammar, and validates native divide-by-zero.  The
recently added two-argument formatter covers X/D/G but duplicates the known
fallback behavior of unsigned siblings.  `Clamp` also uses `std::clamp`
without honoring the .NET invalid-bound contract.

## Finding references

- **SR-AUD-021:** `ToString(5, "Q")` returns `"5"` instead of throwing
  `System::FormatException`.
- **SR-AUD-022:** `Clamp(5, 10, 0)` returns `0` after an invalid
  `std::clamp` call rather than throwing `System::ArgumentException`.
- **SR-AUD-023:** `ToString(5, "B")` returns decimal `"5"`, whereas the
  supported .NET integral binary format must produce `"101"`.

## Other missing assertions and diagnostics

- The new two-argument-format tests do not cover B/b, unknown types, or a
  precision overflow; preserve the existing malformed-width checks while
  adding those cases.
- No Clamp test supplies `min > max`.
- `UInt32::Parse` depends on the width of `unsigned long` before its final
  range check.  Existing negative and maximum vectors give useful coverage, but
  LLP64 must remain part of a supported-platform test matrix.

## Final assessment

Good common-path and NumberStyles behavior, with three already-indexed
cross-width public-contract gaps in formatting and Clamp validation.
