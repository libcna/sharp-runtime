# Audit: `modules/net-websockets/include/System/Net/WebSockets/ClientWebSocketOptions.hpp`

## Metadata

- AUDITED: request-header/subprotocol validation, immutable-after-connect
  transition, buffers, and keep-alive configuration.
- Validation: direct C++/current-.NET probes covered CR/LF header input and
  `chat,evil` subprotocol input.

## SR-AUD-248 — high — SetRequestHeader accepts CR/LF and serializes caller text directly into the HTTP upgrade request

The C++ probe accepts value `ok\r\nInjected: yes`; current .NET throws
`ArgumentException`.  `performHandshake` subsequently writes map key/value
text as `name + ": " + value + "\r\n"`, permitting the unvalidated newline
to create a second handshake header on the wire.  This is a request-header
injection boundary, not merely a cosmetic validation difference.

## SR-AUD-249 — medium — AddSubProtocol accepts HTTP-token separators that .NET rejects

The C++ probe accepts `chat,evil`, while current .NET rejects it with
`ArgumentException` through its token validation.  The implementation checks
only control/space bytes and DEL, then joins requests with commas, so a single
invalid caller string can alter the advertised protocol list.

## SR-AUD-252 — medium — configured keep-alive interval and timeout are stored but never affect a ClientWebSocket connection

Repository-wide `keepAlive` use ends in these getters/setters; no connection
or frame path reads either stored option.  Current .NET uses its values to
choose ping/pong keep-alive behavior and response timeout.  The public
configuration therefore reports successful setup but has no transport effect.

## Assessment

Read-only transition, positive buffer validation, and ASCII duplicate checks
are present.  Header token validation, header-value sanitization, and actual
keep-alive consumption are absent.

## Other missing assertions and diagnostics

- Add CR/LF/NUL/name-token rejection, all RFC token separators, Unicode/ASCII
  duplicate pairs, and immutability-after-connect cases.
- In a network-permitted fixture, assert outbound headers contain no injected
  line and configured keep-alives produce the documented ping/pong behavior.

## Final assessment

SR-AUD-248, SR-AUD-249, and SR-AUD-252 are confirmed. No source or test was
changed.
