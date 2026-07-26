# Audit: `modules/core/include/System/Byte.hpp`

## Metadata

- Audit status: AUDITED (434 lines, header-only implementation, full read).
- Validation: the focused 8/16-bit numeric filter passed 312/312 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-small-integer-audit-probe.cpp`, compiled
  against `build/libsharp_runtime_core.a`.

## Assessment

Parsing differentiates malformed, overflow, and negative unsigned input;
bounded bit operations, `B` formatting, and divide-by-zero handling are all
implemented explicitly. The remaining shared discrepancy is diagnostic: an
unknown numeric format silently selects decimal output.

## Finding reference

- **SR-AUD-021:** `Byte::ToString(byte{5}, "Q")` returns `"5"` rather than
  throwing `System::FormatException`. The probe reproduced the same fallback
  in every newly reviewed 8/16-bit wrapper.
- **SR-AUD-022:** `Clamp(5, 10, 0)` forwards an inverted interval to
  `std::clamp` and observed output is `0`, rather than .NET's argument error.

## Required post-audit verification

Add an unknown-format `FormatException` assertion and an inverted-bound
`ArgumentException` assertion. Keep the present binary width vectors, which
are the only reviewed 8/16-bit coverage of the implemented `B` path.

## Final assessment

Solid small-unsigned parsing and bit coverage, with two shared public-input
diagnostic gaps. No implementation was changed during the audit.
