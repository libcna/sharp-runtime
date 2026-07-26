# Audit: `modules/net-sockets/src/System/Net/Sockets/UdpClient.cpp`

## Metadata

- AUDITED: UDP creation/bind, connect, send/receive, and close paths.
- Evidence: source/current .NET `UDPClient.cs` review and blocked fixture.

## SR-AUD-267 — medium — UdpClient silently narrows invalid ports and implements every endpoint path as IPv4

`UdpClient(int port)` immediately casts any signed port to `uint16_t`; host
and endpoint connect/bind paths all create AF_INET sockets and `sockaddr_in`.
Thus -1/65536 do not receive the managed range diagnostic and IPv6 endpoints
are misrepresented. Current .NET validates ports before socket creation and
selects the endpoint address family.

## Other missing assertions and diagnostics

- Add 0/65535/-1/65536 and IPv6 bind/connect/send/receive cases plus failure
  state assertions in a network-permitted environment.

## Final assessment

SR-AUD-267 is confirmed. No source or test changed.
