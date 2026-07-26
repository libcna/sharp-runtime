# Audit: `modules/io-hashing/src/System/IO/Hashing/XxHash32.cpp`

## Metadata

- AUDITED: XXH32 buffering, lanes, finalization, and one-shot output.
- Evidence: official vectors, source review, and current .NET state code.

## Assessment

The independent negative-length repair is present. `processBlock` and
finalization load native integers via `memcpy`, so byte order changes exact
hashes on a big-endian target (SR-AUD-262); positive null input has no
diagnostic (SR-AUD-260).

## Other missing assertions and diagnostics

- Add portable little-endian helper tests, null/empty raw input tests, and
  every 16-byte boundary for clone, reset, and destinations.

## Final assessment

SR-AUD-260 and SR-AUD-262 apply. No source or test changed.
