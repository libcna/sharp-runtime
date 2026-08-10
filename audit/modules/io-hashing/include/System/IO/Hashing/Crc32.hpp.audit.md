# Audit: `modules/io-hashing/include/System/IO/Hashing/Crc32.hpp`

## Metadata

- AUDITED: CRC-32 public overloads, parameter-set ownership, and result API.
- Evidence: declaration/source review and current .NET source comparison.

## Assessment

Normal and custom-parameter operation is represented, including clone state.
All raw pointer overloads retain the unguarded null-pointer boundary
(SR-AUD-260) and expose signed lengths that the CRC update path treats as an
empty no-op (SR-AUD-261).

## Other missing assertions and diagnostics

- Cover all parameterized `Hash`/`TryHash` overloads for null input, negative
  length, null destination, short destination, and output byte order.

## Final assessment

SR-AUD-260 and SR-AUD-261 apply. No source or test changed.
