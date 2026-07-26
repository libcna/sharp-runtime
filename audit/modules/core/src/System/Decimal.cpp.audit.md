# Audit: `modules/core/src/System/Decimal.cpp`

## Metadata

- Audit status: AUDITED (508 lines, full read).
- Validation: `DecimalTests.*:DecimalTests2.*` passed 143/143 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Direct probe: `/tmp/sharp-runtimervc-decimal-audit-probe.cpp`, compiled and
  run against `build/libsharp_runtime_core.a` on 2026-07-25.

## Assessment

The source implements a compact unsigned-128 decimal core and correctly
handles many ordinary operations.  Its text parser is a custom narrow grammar
rather than .NET's default `NumberStyles.Number` behaviour, collapses every
parse failure to `FormatException`, and discards fractional precision rather
than rounding it.  The rounding dispatcher also treats an invalid public enum
value as `ToZero`, and raw construction/parser paths erase the sign bit of
zero.

## Finding references

- **SR-AUD-035:** `TryParse` rejects valid default whitespace/grouped input
  (`" 1,234.5 "`) and `Parse` maps the out-of-range
  `79228162514264337593543950336` to `FormatException` instead of
  `OverflowException`.  It also returns zero for
  `0.00000000000000000000000000006`, silently discarding the 29th fractional
  digit; .NET rounds such excess precision to the nearest representable
  Decimal.  The default Decimal grammar permits whitespace, sign, grouping,
  and decimal point, distinguishes format from overflow, and specifies
  nearest rounding of excess fractional precision:
  <https://learn.microsoft.com/en-us/dotnet/api/system.decimal.parse?view=net-10.0>.
- **SR-AUD-036:** `Round(d, decimals, static_cast<MidpointRounding>(99))`
  reaches the switch default and returns `1` for `1.9` rather than throwing
  `ArgumentException`.  .NET requires that exception for an invalid
  `MidpointRounding` value:
  <https://learn.microsoft.com/en-us/dotnet/api/system.math.round?view=net-10.0>.
- **SR-AUD-038:** both `Decimal(lo, mid, hi, isNegative, scale)` and
  `TryParse` condition the sign on a nonzero mantissa.  The probe constructs
  raw negative zero and obtains `negative_zero_flags=0`; .NET exposes a
  distinct sign bit for negative zero through `GetBits`:
  <https://learn.microsoft.com/en-us/dotnet/api/system.decimal.getbits?view=net-10.0>.

## Required post-audit verification

Implement a single documented default parse policy with culture/feature scope
made explicit, distinguish malformed from out-of-range input, and round rather
than drop excess precision.  Reject every enum value outside the five
`MidpointRounding` values before calculation.  Preserve a valid zero sign in
raw construction, parsing, and `CopySign`, then test it through `GetBits`.

## Other missing assertions and diagnostics

- The tests have no valid default leading/trailing-whitespace or grouping
  inputs, no out-of-range `Parse` exception-type assertion, and no
  29th-fractional-digit rounding vector.
- `Round` covers all named modes but no cast invalid enum value.
- Existing negative-zero equality tests cannot reveal a cleared sign bit;
  `GetBits` must be part of the assertion.

## Final assessment

The arithmetic core passes its focused ordinary suite, but parser validation,
exception taxonomy, enum validation, and raw Decimal representation require
post-audit repair.  No implementation was modified during this audit.
