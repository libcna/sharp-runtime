# Audit: `modules/net-websockets/include/System/Net/WebSockets/ValueWebSocketReceiveResult.hpp`

## Metadata

- AUDITED: readonly result storage and count/message-type validation.
- Validation: support fixture passed its three direct cases; local .NET source
  was compared.

## Assessment

The value result rejects negative counts and message-type values outside the
managed packed range through `Close`.  Its value-only design is coherent with
the local C++ task adapter.

## Other missing assertions and diagnostics

- Assert all permitted message values, default construction, copy/move, and
  maximum count behavior.
- Add an ABI/value-semantics check if this type is passed across component
  boundaries.

## Final assessment

No value-result mismatch was demonstrated. No source or test was changed.
