# Audit: `modules/net-sockets/README.md`

## Metadata

- AUDITED: component scope and platform/dependency claims.
- Evidence: implementation and focused test review.

## Assessment

The README identifies the module but does not disclose that TcpClient/UdpClient
paths are IPv4-only, nor the raw-lifetime requirement placed on asynchronous
Socket callers. Those omissions obscure SR-AUD-263, SR-AUD-266, and
SR-AUD-267.

## Other missing assertions and diagnostics

- Document the supported address families, async ownership semantics, and the
  local-network validation requirement.

## Final assessment

No separate finding. No source or test changed.
