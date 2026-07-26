# Audit: `modules/net-sockets/src/System/Net/Sockets/NetworkStream.cpp`

## Metadata

- AUDITED: descriptor lifecycle, raw I/O validation, error wrapping, and close.
- Validation: direct native probe and current .NET `NetworkStream.cs` source.

## SR-AUD-265 — medium — NetworkStream accepts an invalid raw descriptor and turns reads/writes into silent EOF/no-op operations

The direct probe constructs `NetworkStream(-1)`: `Read(oneByte,0,1)` prints
`read=0` and `Write(oneByte,0,1)` returns normally. Current .NET rejects a
null, nonblocking, unconnected, or non-stream Socket at construction and
throws after disposal; the C++ public raw-fd replacement has no equivalent
state validation.

## Assessment

Negative offset/count are now guarded. Positive raw capacity cannot be known
from the pointer API and needs a documented contract; network I/O validation is
otherwise sandbox-blocked.

## Other missing assertions and diagnostics

- Test invalid/non-socket/unconnected descriptors, closed reads/writes, partial
  sends, capacity bounds under ASan, and native error-cause retention.

## Final assessment

SR-AUD-265 is confirmed. No source or test changed.
