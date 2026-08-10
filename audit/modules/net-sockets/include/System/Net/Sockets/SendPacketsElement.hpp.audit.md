# Audit: `modules/net-sockets/include/System/Net/Sockets/SendPacketsElement.hpp`

## Metadata

- AUDITED: file/buffer packet element validation and reduced-scope contract.
- Validation: direct native probe and current .NET source comparison.

## SR-AUD-264 — medium — SendPacketsElement treats every negative buffer count as the whole buffer instead of rejecting invalid ranges

`SendPacketsElement({1,2,3}, 0, -2)` returns normally and stores count 3,
because C++ maps all negative counts to `buffer_.size()`. Current .NET uses
unsigned range validation and rejects -2; only the separate whole-buffer
constructor supplies `buffer.Length` itself.

## Other missing assertions and diagnostics

- Cover null/adapted empty buffer policy, -1/-2, offset/count boundary pairs,
  file offset/count behavior, and a real SendPackets consumer if implemented.

## Final assessment

SR-AUD-264 is confirmed. No source or test changed.
