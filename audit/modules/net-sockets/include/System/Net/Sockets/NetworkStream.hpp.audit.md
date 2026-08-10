# Audit: `modules/net-sockets/include/System/Net/Sockets/NetworkStream.hpp`

## Metadata

- AUDITED: raw descriptor ownership, stream capabilities, and I/O declarations.
- Evidence: direct native probe and current .NET `NetworkStream.cs`.

## Assessment

The public raw-fd constructor has no connected-stream/type/ownership validation.
`NetworkStream(-1)` then presents a closed stream whose Read returns EOF and
Write succeeds silently, which is SR-AUD-265.

## Other missing assertions and diagnostics

- Test invalid, non-socket, unconnected, nonblocking, and disposed handles;
  test complete writes and offset/capacity bounds under sanitizers.

## Final assessment

SR-AUD-265 applies. No source or test changed.
