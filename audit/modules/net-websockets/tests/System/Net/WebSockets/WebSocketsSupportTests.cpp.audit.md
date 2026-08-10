# Audit: `modules/net-websockets/tests/System/Net/WebSockets/WebSocketsSupportTests.cpp`

## Metadata

- AUDITED: eighteen enum, result, option-value, exception, and buffer-helper
  cases.
- Validation: all eighteen support cases pass within the 22/24 executable
  result.

## Assessment

The fixture establishes sampled enum parity, value-result validation, basic
classic result storage, default creation options, two exception messages, and
buffer helpers.  It does not test ClientWebSocketOptions request headers,
subprotocol token grammar, keep-alive settings, or inner exception retention.

## Other missing assertions and diagnostics

- Add exhaustive enum values; default/copy/invalid result cases; all option
  durations/buffers/read-only transitions; and WebSocketException causal
  identity for SR-AUD-250.
- Add direct no-I/O coverage for SR-AUD-248/249/252 so these public inputs are
  not dependent on a loopback server.

## Final assessment

The support fixture passes but leaves SR-AUD-248 through SR-AUD-252 mostly
unasserted. No source or test was changed.
