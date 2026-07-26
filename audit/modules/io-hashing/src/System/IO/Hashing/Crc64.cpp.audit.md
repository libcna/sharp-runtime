# Audit: `modules/io-hashing/src/System/IO/Hashing/Crc64.cpp`

## Metadata

- AUDITED: CRC-64 construction, custom parameters, one-shot and streaming API.
- Evidence: source review, UBSan probe, and current .NET `Crc64.cs`.

## Assessment

ECMA-182 and NVMe normal vectors pass, but raw input reaches `Update` without
validation. A positive null input reaches a UBSan-confirmed load and negative
length produces a successful no-op, extending SR-AUD-260 and SR-AUD-261.

## Other missing assertions and diagnostics

- Cover null and negative input, custom parameter overloads, null/short
  destinations, both byte orders, clone, and reset transitions.

## Final assessment

SR-AUD-260 and SR-AUD-261 apply. No source or test changed.
