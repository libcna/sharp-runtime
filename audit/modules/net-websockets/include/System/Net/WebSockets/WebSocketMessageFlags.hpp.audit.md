# Audit: `modules/net-websockets/include/System/Net/WebSockets/WebSocketMessageFlags.hpp`

## Metadata

- AUDITED: flags vocabulary and OR/AND helpers.
- Validation: support fixture passed the implemented combination check and
  values were compared with local .NET declarations.

## Assessment

The named flag values and basic composition match current .NET.  The concrete
C++ send API exposes `endOfMessage` rather than the managed flags overload, so
`DisableCompression` remains metadata while permessage-deflate is out of
scope.

## Other missing assertions and diagnostics

- Assert every value, unknown-bit preservation, and no accidental claim that
  `DisableCompression` changes a transport without compression support.

## Final assessment

No flags mismatch was demonstrated. No source or test was changed.
