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
  **Partially remediated (#1857, 2026-07-30):** `TryParse` now (a) skips
  leading/trailing whitespace (`std::isspace`), a pure widening, and (b) rounds
  excess fractional precision beyond scale 28 half-to-even instead of
  discarding it, so `0.0…06` → `1e-28` (with a defensive scale-drop on the rare
  rounding carry past the 96-bit mantissa). **Fully remediated (#1858, 2026-07-31):** approved in the exact words of
  `docs/RemainingApprovalDecisions.md` §B.8 items (2) and (3) and delivered as
  the two separate commits §B.5 required. The scanner became a private
  three-state `ParseStatus { Ok, Malformed, Overflow }`, so `Parse` throws
  `OverflowException("Value was either too large or too small for a Decimal.")`
  (`SR.Overflow_Decimal`, verbatim) for a well-formed oversized magnitude while
  malformed text stays a `FormatException`; `TryParse` keeps its `bool` and its
  no-partial-write guarantee. Then `','` became the invariant-culture **group
  separator** per `NumberStyles.Number`, so `" 1,234.5 "` — the finding's own
  example — parses to `1234.5m`. **Premise correction:** the finding, and the
  decision packet §B.3, each name one value change (`Parse("1,5")` 1.5→15);
  measured, there are **two** — `Parse(",5")` also changes, from `0.5m` to
  `FormatException`, because a group separator requires a preceding digit, which
  is what .NET does. Both are tabulated in
  `docs/Migration-DecimalCommaGroupSeparator.md`. Adding a private static member
  function changed no layout, no vtable and no existing mangled name. +15 tests,
  including the inversion of both `*_PendingApproval` tests. `SR-AUD-035 →
  remediated`, and with it the CCF-005 Decimal slice is complete.
- **SR-AUD-036:** `Round(d, decimals, static_cast<MidpointRounding>(99))`
  reaches the switch default and returns `1` for `1.9` rather than throwing
  `ArgumentException`.  .NET requires that exception for an invalid
  `MidpointRounding` value:
  <https://learn.microsoft.com/en-us/dotnet/api/system.math.round?view=net-10.0>.
  **Remediated (#1855, 2026-07-30):** `Decimal::Round(d, decimals, mode)` now
  rejects `(uint)mode > (uint)MidpointRounding::ToPositiveInfinity` with
  `System::ArgumentException(paramName "mode")` before the scale-vs-decimals
  early-out (so even a no-op round validates), matching
  `Decimal.Round(ref, int, MidpointRounding)`. **Closes CCF-008.**
- **SR-AUD-038:** both `Decimal(lo, mid, hi, isNegative, scale)` and
  `TryParse` condition the sign on a nonzero mantissa.  The probe constructs
  raw negative zero and obtains `negative_zero_flags=0`; .NET exposes a
  distinct sign bit for negative zero through `GetBits`:
  <https://learn.microsoft.com/en-us/dotnet/api/system.decimal.getbits?view=net-10.0>.
  **Remediated (#1856, 2026-07-30):** the raw ctor now sets `negative_ =
  isNegative` unconditionally and `TryParse` builds `Decimal(mantissa, scale,
  neg)` (so `Parse("-0")` → −0), matching .NET's raw ctor and its
  `NumberBufferKind.Decimal` parser path. Equality/hash stay sign-agnostic for
  zero, so `−0m == 0m` and both hash equal. `normalize()`/unary-`−` production
  of negative zeros is deliberately out of scope (deferred broader decision).

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
