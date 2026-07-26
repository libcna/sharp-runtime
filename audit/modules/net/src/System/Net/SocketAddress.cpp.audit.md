# Audit: `modules/net/src/System/Net/SocketAddress.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [SocketAddress.cs](/rv/tmp/runtime/src/libraries/Common/src/System/Net/SocketAddress.cs).
- Evidence: ASan/UBSan probe `/tmp/sharp-runtime-net-audit/net_bounds.cpp`.

## Assessment

The raw-buffer constructor deliberately permits a two-byte `SocketAddress`, as
does .NET.  The C++-only public `GetIPEndPoint()` then assumes an IPv4 or IPv6
layout without checking the stored size or family.  `IPEndPoint::Create` does
perform those checks, so this is a direct-helper boundary that bypasses its
own safe path.

### SR-AUD-300 — high — `GetIPEndPoint` decodes undersized and non-IP raw buffers without validating their layout

`SocketAddress(AddressFamily::InterNetwork, 2).GetIPEndPoint()` reads
`buffer_[2]`, `buffer_[3]`, and the address bytes past the two-byte allocation.
The ASan probe reports a heap-buffer-overflow at `SocketAddress.cpp:106`.
An `AddressFamily::Unix` buffer is also treated as IPv4.  The public
`SocketAddress(const IPAddress&, intcs)` accepts an invalid port and truncates
it to 16 bits before serialization.  .NET exposes the raw buffer and lets an
endpoint's `Create` validate family and size; it does not expose this unsafe
decoder as a public shortcut.

Required remediation: validate family, minimum size, and port at every public
decode/encode boundary; reject unsupported family values with the established
`ArgumentException` contract.  Keep raw-buffer construction permissive.

## Missing assertions and diagnostics

`NetTests.cpp` covers normal `IPEndPoint::Create` layouts but has no direct
undersized `GetIPEndPoint`, unknown-family, or invalid-port assertion.  Add
those cases and run them under ASan/UBSan.

## Final assessment

Unsafe public raw-buffer decoding is confirmed.
