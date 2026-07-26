# Audit: `modules/io-hashing/include/System/IO/Hashing/XxHash64.hpp`

## Metadata

- AUDITED: XXH64 public state, seed, clone, and output forms.
- Evidence: official vectors, source review, and current .NET state source.

## Assessment

The independent negative-length guard is present. Positive null raw input is
not diagnosed (SR-AUD-260), while native lane `memcpy` loads make results
dependent on host endianness (SR-AUD-262).

## Other missing assertions and diagnostics

- Cover null/empty raw input, cross-endian vectors, 32-byte buffering edges,
  destination bounds, and clone/reset after a partial block.

## Final assessment

SR-AUD-260 and SR-AUD-262 apply. No source or test changed.
