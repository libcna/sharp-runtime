# Audit: `modules/net-websockets/tests/System/Net/WebSockets/ClientWebSocketTests.cpp`

## Metadata

- AUDITED: six handshake/frame/range/subprotocol fixture cases.
- Validation: 4/6 non-network cases pass; both loopback cases fail immediately
  in this sandbox with `Socket::Socket: socket() failed` before test setup.

## Assessment

The fixture has valuable full-handshake, echo/close, huge-length, range, wrong
scheme, and ASCII duplicate-subprotocol coverage.  In this environment only
the range/scheme/duplicate cases execute.  It lacks raw-owner lifetime,
cancellation, request-header sanitization, invalid token, keep-alive, and
malformed handshake/frame regressions.

## Other missing assertions and diagnostics

- Add ASan destruction-during-operation tests for SR-AUD-247 and exact
  pre-canceled task status for SR-AUD-251.
- Add no-CR/LF header-name/value and RFC token-negative cases for
  SR-AUD-248/249, plus asserted outbound request contents.
- Re-run full loopback cases in a local-network-permitted final gate; retain
  them enabled rather than treating the sandbox failure as a product result.

## Final assessment

Transport coverage is environment-blocked and does not protect the four
ClientWebSocket findings. No source or test was changed.
