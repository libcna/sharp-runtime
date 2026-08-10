# Audit: `modules/net-sockets/include/System/Net/Sockets/SocketShutdown.hpp`

## Metadata

- AUDITED: shutdown constants and native mapping.
- Evidence: enum fixture and `Socket::Shutdown` review.

## Assessment

The three standard values map correctly. Invalid casts and native shutdown
errors are currently ignored by the implementation and need focused network
validation before severity classification.

## Other missing assertions and diagnostics

- Test each direction, invalid enums, repeated calls, close ordering, and error
  translation on both supported OS families.

## Final assessment

No separate finding. No source or test changed.
