# Audit: `modules/net-websockets/include/System/Net/WebSockets/WebSocketError.hpp`

## Metadata

- AUDITED: WebSocket error enum values.
- Validation: support fixture samples and local .NET source comparison.

## Assessment

The consecutive public values through `InvalidState` agree with the managed
enum.  The meaningful mapping is in `WebSocketException`; its discarded inner
cause is tracked separately as SR-AUD-250.

## Other missing assertions and diagnostics

- Assert all values and their association with each canned exception message.
- Test errors arising from real handshake/frame failures in a permitted local
  network environment.

## Final assessment

No enum mismatch was demonstrated. No source or test was changed.
