# Audit: `modules/io-hashing/src/System/IO/Hashing/XxHash3Shared.cpp`

## Metadata

- AUDITED: XXH3/128 secret derivation, stripe accumulation, buffering, and LE
  load/store helpers.
- Validation: deterministic randomized chunk tests and direct code comparison
  with current .NET `XxHashShared.cs`.

## SR-AUD-262 — medium — XXH implementations label native object copies as little-endian loads and produce platform-dependent hashes

`ReadUInt32LE`, `ReadUInt64LE`, and `WriteUInt64LE` merely `memcpy` native
integers. XxHash32/64 do the same for every lane. On a big-endian target these
are byte-swapped relative to the xxHash specification and current .NET, whose
helpers explicitly swap when necessary and whose XXH32/64 state uses
`BinaryPrimitives.ReadUInt*LittleEndian`.

## Assessment

The negative-length guard added at shared `Append` is correct and randomized
streaming coverage passes on this little-endian host. Positive null input
still reaches `memcpy`/dereferences (SR-AUD-260), while the named LE helpers
are not portable (SR-AUD-262).

## Other missing assertions and diagnostics

- Make byte order an explicit helper contract and test it under forced swapped
  loads; add null/zero-length raw input tests at this shared boundary.

## Final assessment

SR-AUD-260 and SR-AUD-262 apply; SR-AUD-262 is confirmed here. No source or
test changed.
