# Audit: `modules/net-sockets/tests/System/Net/Sockets/SocketsTests.cpp`

## Metadata

- AUDITED: TcpClient/TcpListener/UdpClient/NetworkStream fixture.
- Validation: 38 tests ran; 21 passed and 17 failed through sandbox native
  socket/send permission, including loopback paths.

## Assessment

The tests cover prior copy, raw-count, and stream negative-argument repairs.
They assert only that `TcpClient(IPEndPoint)` does not throw, so they miss its
inert bind/IPv6 behavior (SR-AUD-266), UDP port/family behavior
(SR-AUD-267), and the invalid-fd NetworkStream contract (SR-AUD-265).

## Other missing assertions and diagnostics

- Add local endpoint/IPv6/port range tests, invalid descriptor creation,
  post-close errors, full-write behavior, and retained native exception cause.

## Final assessment

Environment-limited; SR-AUD-265 through SR-AUD-267 lack regression coverage.
No source or test changed.
