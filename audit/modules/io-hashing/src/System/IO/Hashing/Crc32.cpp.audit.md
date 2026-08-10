# Audit: `modules/io-hashing/src/System/IO/Hashing/Crc32.cpp`

## Metadata

- AUDITED: CRC-32 construction, parameter forwarding, hash forms, and reset.
- Evidence: source review, native probe, and current .NET `Crc32.cs`.

## Assessment

Default and custom parameter paths serialize correctly on the reviewed host.
They forward raw data unchecked into `Crc32ParameterSet::Update`: positive
null input crashes under UBSan (SR-AUD-260) and negative signed length returns
as a normal empty update (SR-AUD-261).

## Other missing assertions and diagnostics

- Test every default/custom overload for invalid input metadata, null output,
  short output, reset behavior, and reflected/forward byte order.

## Final assessment

SR-AUD-260 and SR-AUD-261 apply. No source or test changed.
