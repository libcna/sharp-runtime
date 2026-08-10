# Audit: `modules/core/include/System/Numerics/BFloat16.hpp`

## Metadata

- AUDITED: 112-line inline binary-floating implementation, fully read.
- Validation: the existing `BitConverterTests.*` filter passed 67/67 on
  2026-07-27; a standalone C++20 warnings-as-errors probe exercised exact
  float-to-BFloat16 tie and above-tie bit patterns.
- Reference basis: local current-.NET 2,152-line `System.Numerics.BFloat16`
  implementation and its BFloat16 BitConverter tests.

## SR-AUD-175 — medium — float conversion truncates BFloat16 payloads instead of performing current .NET round-to-nearest-even

`fromFloat` simply shifts the IEEE float bits right by 16.  Current .NET
passes non-NaN float bits through `RoundMidpointToEven(bits, 16)` before taking
the BFloat16 payload, so it rounds both ordinary discarded bits and exact
ties.

The C++ bit probe prints `midpoint_odd=0x3F81 above_midpoint=0x3F80`.  Input
float bits `0x3F818000` are the exact midpoint above odd BFloat16 payload
`0x3F81`, so current .NET rounds it to even `0x3F82`; input `0x3F808001` is
strictly above the midpoint from `0x3F80` and rounds to `0x3F81`.  C++ drops
the bottom bits in both cases, returning the lower payload.  Arithmetic also
constructs through this path, so results can be biased downward after every
operation.

## SR-AUD-176 — medium — public BFloat16 is a narrow bit-wrapper rather than the current .NET numeric value-type contract

The header exposes raw construction/property access, float arithmetic and a
single `ToString`, but omits public Parse/TryParse, standard format overloads,
comparison/hash APIs, finite/negative/normal/subnormal/zero classification,
CopySign, BitIncrement/Decrement, conversions to/from the ordinary numeric
domain, and the current generic-math interface contract.  Current .NET makes
these part of the public `readonly struct BFloat16` surface, not internal
helpers.

The local BitConverter fixture only round-trips raw BFloat16 bits; it never
constructs from float, performs arithmetic, formats/parses, compares, or
attempts any absent public operation.  No permanent-deviation documentation
limits this header to bit conversion, while it is publicly re-exported by
`BitConverter` and recorded as a Core Base public type.

## Assessment

Raw bit storage, float expansion, special-value bit predicates, sign flip,
and BFloat16 byte reinterpretation are coherent for their narrow paths.  They
do not make a correct managed BFloat16 numeric conversion or public API.

## Other missing assertions and diagnostics

- Direct BFloat16 tests cover only four raw bit conversions and no float
  construction.  Add exact lower/upper midpoint, just-below/above midpoint,
  carry-to-infinity, subnormal, signed zero, and NaN payload cases.
- They omit arithmetic result rounding, all comparisons including NaN/zero,
  ToString special/round-trip text, and every absent parse/classification/
  conversion operation.
- A complete C++ BFloat16 boundary requires explicit tests of endian
  byte layout plus BitConverter's already-recorded short-buffer bounds path
  (SR-AUD-041).

## Final assessment

The public BFloat16 header has two confirmed numeric/API defects (SR-AUD-175
and SR-AUD-176).  No source or test was modified.
