# Audit: `modules/io-compression/include/System/IO/Compression/DeflateDecoder.hpp`

## Metadata

- AUDITED: streamless raw-Deflate decoder API.
- Evidence: implementation accepts signed raw pointer lengths without guards.

## Assessment

The ownership/dispose structure is coherent, but public raw pointer plus
`intcs` length is a native adaptation requiring validation. Negative lengths
are cast to zlib `uInt`; DeflateDecoder and its GZip/ZLib wrappers participate
in SR-AUD-256.

## Other missing assertions and diagnostics

- Cover null pointers, negative lengths, zero destination, partial frames,
  trailing frames, disposal, and output-too-small progress.

## Final assessment

No separate finding is added beyond SR-AUD-256. No source or test changed.
