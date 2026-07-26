# Audit: `modules/io-hashing/include/System/IO/Hashing/Crc64.hpp`

## Metadata

- AUDITED: CRC-64 public overloads, parameter-set ownership, and output API.
- Evidence: declaration/source review and current .NET parity source.

## Assessment

The ECMA and NVMe paths are present, but all raw input overloads expose a
positive-null dereference boundary and route negative signed lengths to a
silent no-op through `Update` (SR-AUD-260 and SR-AUD-261).

## Other missing assertions and diagnostics

- Add null/negative metadata and custom reflected/forward vector coverage to
  every one-shot and streaming overload.

## Final assessment

SR-AUD-260 and SR-AUD-261 apply. No source or test changed.
