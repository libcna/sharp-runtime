# Audit: `modules/io-compression/src/System/IO/Compression/DeflateStream.cpp`

## Metadata

- AUDITED: raw Deflate stream lifecycle, zlib I/O, and inner-stream ownership.
- Validation: ASan/UBSan native probe and current-.NET constructor/lifecycle
  probe.

## Assessment

Nominal round trips and pre-zlib buffer validation pass. Constructor and closed
state guards are absent, so invalid public states are stored and later ignored
or dereferenced.

## SR-AUD-257 — high — compression streams accept a null inner stream and later dereference it

`DeflateStream(nullptr, Compress, true)` constructs successfully; a sufficiently
large incompressible Write reaches `inner_->Write` and UBSan reports member
access through null (the isolated native process exits 139). Current .NET
prints `nullStream=ArgumentNullException`. GZipStream and ZLibStream share the
same unguarded constructor/Write shape.

## SR-AUD-258 — medium — compression streams silently accept invalid modes and post-close operations

A native `(CompressionMode)42` constructor succeeds, creates a deflater, and
reports both CanRead and CanWrite false; current .NET prints
`invalidMode=ArgumentException`. After `Close`, native Write returns silently,
where .NET prints `afterClose=ObjectDisposedException`. The three stream
implementations duplicate this mode/closed-state pattern.

## Other missing assertions and diagnostics

- Reject null/invalid mode at construction; enforce disposed state on all
  properties/I/O/Flush; cover leave-open, mode misuse, corrupted frames, and
  throwing inner close/write paths.

## Final assessment

SR-AUD-257 and SR-AUD-258 are confirmed. No source or test changed.
