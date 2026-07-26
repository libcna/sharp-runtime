# Audit: `modules/io-compression/src/System/IO/Compression/DeflateEncoder.cpp`

## Metadata

- AUDITED: raw Deflate zlib setup, compression, flushing, bounds, and options.
- Validation: ASan/UBSan probe plus strategy comparison against current .NET
  source.

## Assessment

Normal compression works, but public `intcs` source/destination lengths are
blindly cast to `uInt`; the exact `sourceLength=-1` probe gives zlib a huge
input and ASan reports a heap-buffer-overflow. Options construction also drops
its stored strategy, hard-coding `Z_DEFAULT_STRATEGY`.

## SR-AUD-256 — high — raw buffer compression APIs cast negative public lengths into unbounded native zlib input

`DeflateEncoder::Compress` accepts a one-byte source and `sourceLength=-1`,
casts it to `uInt`, and zlib reads 65,536 bytes past the allocation under ASan.
DeflateDecoder and the GZip/ZLib encoder/decoder wrappers expose the same
unguarded signed pointer-length boundary. The managed span APIs cannot express
negative lengths and validate their managed boundaries first.

## SR-AUD-259 — medium — CompressionStrategy is stored but never reaches any encoder and stream option constructors are absent

`DeflateEncoder(options)`, `GZipEncoder(options)`, and `ZLibEncoder(options)`
discard `options.getCompressionStrategyProperty()` and initialise
`Z_DEFAULT_STRATEGY`. Native RLE and default option probes produce identical
60-byte output. Current .NET passes `options.CompressionStrategy` to its native
encoder; its three stream types also provide options constructors that this
module omits.

## Other missing assertions and diagnostics

- Validate null/negative/oversized metadata before zlib; test destination
  boundaries, all strategies, flush/final states, and error diagnostics.

## Final assessment

SR-AUD-256 and SR-AUD-259 are confirmed. No source or test changed.
