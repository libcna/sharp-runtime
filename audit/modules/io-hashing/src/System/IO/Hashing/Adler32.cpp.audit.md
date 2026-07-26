# Audit: `modules/io-hashing/src/System/IO/Hashing/Adler32.cpp`

## Metadata

- AUDITED: Adler update loop, output order, lifecycle, and raw argument paths.
- Validation: an ASan/UBSan direct probe and current .NET `Adler32.cs` review.

## SR-AUD-260 — high — hashing raw-pointer overloads dereference positive null buffers instead of reporting an argument error

`Adler32::HashToUInt32(nullptr, 1)` reaches `Update` line 24; UBSan reports a
null `bytecs` load. The corresponding CRC and XXH raw source paths, and base
raw destination writers, have the same unchecked pointer boundary. Current
.NET's byte-array forms call `ArgumentNullException.ThrowIfNull`; spans cannot
represent a positive-length null buffer.

## SR-AUD-261 — medium — Adler and CRC raw signed lengths accept negative input as a successful empty operation

The direct `source=-1` probe reports `returned` for Adler `HashToUInt32` and
`Append`, both CRC one-shot/streaming paths, and both parameter-set `Update`
methods. `Adler32::Update` and the CRC loops simply skip negative counts,
whereas XXH has explicit `ArgumentOutOfRangeException` guards.

## Other missing assertions and diagnostics

- Test null positive versus null zero-length input, negative lengths, null
  destination, short destination, and no-state-change-on-rejected append.

## Final assessment

SR-AUD-260 and SR-AUD-261 are confirmed. No source or test changed.
