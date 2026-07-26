# Audit: `modules/net-sockets/include/System/Net/Sockets/TcpClient.hpp`

## Metadata

- AUDITED: TCP client/listener ownership, connection, and stream declarations.
- Evidence: source/current .NET `TCPClient.cs` review and blocked fixture.

## Assessment

The copy-deletion and cached-stream repairs are present. The local-endpoint
constructor is declared as a normal binding constructor despite being inert in
the implementation, while connection forms cannot represent IPv6; see
SR-AUD-266.

## Other missing assertions and diagnostics

- Test local binding, IPv4/IPv6 selection, port validation, reconnect/disposal,
  and GetStream ownership in a network-permitted job.

## Final assessment

SR-AUD-266 applies. No source or test changed.
