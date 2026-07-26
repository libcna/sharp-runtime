# Audit: `modules/time-zone/include/System/TimeZoneInfo.hpp`

## Metadata

- AUDITED: 718-line `TimeZoneInfo` public surface, including transition and
  adjustment value types, lookup, conversion, equality, and factory APIs.
- Validation: `TimeZoneInfoTests.*` plus `AdjustmentRuleTests.*` passed 99/99
  on 2026-07-27; the complete TimeZone fixture set previously passed 114/114.
  Direct C++20/current-.NET 10 probes exercised failed TryFind, factories,
  equality, HasSameRules, and an IANA system zone.
- Reference basis: current .NET 10 `TimeZoneInfo` public behavior and installed
  API reference documentation.  Explicitly documented native DST/serialization
  limitations are retained as adaptations, not duplicate findings.

## SR-AUD-224 — medium — failed TryFind leaves a caller's stale zone in its out parameter

`TryFindSystemTimeZoneById` catches lookup failure but never resets `result`.
Pass an existing UTC result and query `Mars/Olympus`: C++ prints
`tryFind_found=0 result=UTC`; current .NET prints
`tryFind_found=False result=null`.  Callers that honor the false return can
still accidentally reuse a stale, unrelated zone.

## SR-AUD-225 — medium — CreateCustomTimeZone accepts empty IDs and offsets outside the managed range

The factory copies every input without validation.  C++ accepts both an empty
ID and a +15-hour offset; current .NET rejects them with `ArgumentException`
and `ArgumentOutOfRangeException`, respectively.  The supported managed range
is ±14 hours and an ID is required, so the native API creates values its
counterpart cannot represent.

## SR-AUD-226 — medium — AdjustmentRule accepts an effective end date before its start date

Both adjustment factories blindly retain their date range.  A 2025-01-02
start with 2025-01-01 end returns normally in C++; the identical current-.NET
call throws `ArgumentException`.  The 19 green AdjustmentRule tests cover only
ascending valid ranges.

## SR-AUD-227 — medium — zone equality is case-sensitive while its hash is case-insensitive and .NET treats custom IDs case-insensitively

`Equals` compares `id_` byte-for-byte whereas `GetHashCode` lowercases it.
Custom `"Zone"` and `"zone"` compare false in C++ but true in current .NET.
This creates incompatible equality semantics and a confusing local
equality/hash boundary that the same-ID-only fixture cannot expose.

## SR-AUD-228 — medium — HasSameRules reduces distinct adjustment histories to current offset plus a DST flag

The method compares only `baseUtcOffset_` and `supportsDst_`, discarding every
actual rule.  C++ reports `America/New_York` and `America/Havana` as same-rule
zones; current .NET reports false.  The public native type advertises
HasSameRules even though its abbreviated state cannot represent the required
identity.

## SR-AUD-229 — medium — IANA lookup records the current daylight offset as BaseUtcOffset rather than the zone's standard offset

POSIX lookup stores `tm_gmtoff` for `time(nullptr)` directly as
`baseUtcOffset_`.  During the current New York daylight period C++ reports
`baseOffset_newYork=-4`, while current .NET reports the invariant standard
`-5`.  This also makes custom/system conversions depend on the month in which
the zone object was created, contrary to the property contract.

## Assessment

UTC and ordinary fixed-offset custom conversions work under the documented
subset.  The six findings arise from input/result handling and metadata that
the native surface does claim to provide; they are independent of the explicit
no-DST transition adaptation.

## Other missing assertions and diagnostics

- Add failed TryFind result-clearing, empty/overlong/offset-minute factory,
  reversed/range-invalid adjustment-rule, and case-variant equality/hash tests.
- Add fixed calendar-date New York/Havana metadata/rule comparisons in a
  process-isolated timezone database environment.
- Test null-equivalent/native empty inputs, DateTime/DateTimeOffset kind and
  range boundaries, conversion overflow, ClearCachedData observable behavior,
  and IANA/Windows mapping collision/territory choices.

## Final assessment

SR-AUD-224 through SR-AUD-229 are confirmed by direct C++/current-.NET
comparison.  No production or test source was changed.
