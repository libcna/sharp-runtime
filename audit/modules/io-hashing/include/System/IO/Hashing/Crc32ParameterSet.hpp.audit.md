# Audit: `modules/io-hashing/include/System/IO/Hashing/Crc32ParameterSet.hpp`

## Metadata

- AUDITED: CRC-32 parameter construction, singleton variants, update, and
  output serialization declarations.
- Evidence: source/table review and native negative-length probe.

## Assessment

The type constructs the documented reflected and forward tables. Its public
`Update` continues to accept raw input and a signed length without a boundary
check, making SR-AUD-260 and SR-AUD-261 directly reachable.

## Other missing assertions and diagnostics

- Exercise reflected/forward custom parameter vectors, both byte orders, null
  input, negative length, and invalid destination pointers.

## Final assessment

SR-AUD-260 and SR-AUD-261 apply. No source or test changed.
