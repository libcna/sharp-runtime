# Audit: `modules/net-sockets/include/System/Net/Sockets/UdpClient.hpp`

## Metadata

- AUDITED: UDP construction, connect/send/receive, and ownership declarations.
- Evidence: source/current .NET `UDPClient.cs` comparison and fixture review.

## Assessment

The public integer-port and endpoint operations neither validate the port range
nor preserve IPv6 address families: all native paths create/use AF_INET. This
is SR-AUD-267.

## Other missing assertions and diagnostics

- Test ports -1/0/65535/65536, IPv6 bind/connect/receive, post-close behavior,
  datagram truncation, and disconnected send diagnostics.

## Final assessment

SR-AUD-267 applies. No source or test changed.
