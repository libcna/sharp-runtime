# Audit: `modules/net-sockets/src/System/Net/Sockets/TcpClient.cpp`

## Metadata

- AUDITED: TcpClient/TcpListener creation, binding, connect, cached stream,
  and lifecycle.
- Evidence: source/current .NET `TCPClient.cs` review; local integration blocked.

## SR-AUD-266 — medium — TcpClient local-endpoint construction is inert and all connection paths force IPv4

`TcpClient::TcpClient(const IPEndPoint&)` ignores its only argument and opens
no/binds no client socket. Both hostname resolution and endpoint Connect use
AF_INET/sockaddr_in, so an IPv6 local or remote endpoint cannot be represented.
Current .NET sets the local address family, creates the client socket, and
binds the supplied endpoint before connecting.

## Other missing assertions and diagnostics

- Test local source address/port, IPv6, port range, DNS multi-address fallback,
  failed reconnect state, and cached stream close behavior.

## Final assessment

SR-AUD-266 is confirmed. No source or test changed.
