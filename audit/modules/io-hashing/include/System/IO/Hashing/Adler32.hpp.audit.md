# Audit: `modules/io-hashing/include/System/IO/Hashing/Adler32.hpp`

## Metadata

- AUDITED: Adler-32 public construction, streaming, clone, and one-shot API.
- Evidence: declaration review and direct ASan/UBSan probes.

## Assessment

The header exposes raw pointer plus signed-length operations as the managed
span replacement. Their contract needs explicit validation: a positive null
input reaches a native dereference (SR-AUD-260), while a negative length is
silently accepted (SR-AUD-261).

## Other missing assertions and diagnostics

- Test null positive input, null zero-length input, negative length, every
  destination boundary, clone independence, and stream failures.

## Final assessment

SR-AUD-260 and SR-AUD-261 apply. No source or test changed.
