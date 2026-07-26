# Audit: `modules/io-hashing/src/System/IO/Hashing/XxHash128.cpp`

## Metadata

- AUDITED: XXH128 short/medium/long dispatch, pair ordering, and public forms.
- Evidence: official vectors, randomized streaming checks, and shared-source
  review.

## Assessment

The formerly unsafe negative-length one-shot path is repaired. Positive null
raw input still reaches shared reads/copies (SR-AUD-260). Exact output also
inherits host-endian `ReadUInt*LE`/write helpers from the shared implementation
(SR-AUD-262).

## Other missing assertions and diagnostics

- Add null/zero-length behavior, forced byte-swapped helper tests, long clone
  boundaries, and all raw output error paths.

## Final assessment

SR-AUD-260 and SR-AUD-262 apply. No source or test changed.
