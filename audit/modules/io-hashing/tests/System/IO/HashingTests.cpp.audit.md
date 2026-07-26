# Audit: `modules/io-hashing/tests/System/IO/HashingTests.cpp`

## Metadata

- AUDITED: 96 focused unit tests, vectors, lifecycle, and randomized XXH3/128
  chunk coverage.
- Validation: `SharpRuntimeTests_IO_Hashing` passed 96/96.

## Assessment

The suite gives strong native-host coverage for normal vectors and the recent
XXH negative-length repairs. It has no tests for the unchanged Adler/CRC
negative-length acceptance, raw positive null pointers, null destinations, or
forced big-endian XXH lanes, so it cannot detect SR-AUD-260 through
SR-AUD-262.

## Other missing assertions and diagnostics

- Add exact exception/type/state assertions for every invalid raw buffer case;
  add a portable byte-swapped helper seam rather than host-only vectors.

## Final assessment

Missing coverage documents SR-AUD-260 through SR-AUD-262. No source or test
changed.
