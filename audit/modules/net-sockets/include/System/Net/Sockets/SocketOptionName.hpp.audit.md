# Audit: `modules/net-sockets/include/System/Net/Sockets/SocketOptionName.hpp`

## Metadata

- AUDITED: option-name constants and mapping consumers.
- Evidence: enum fixture and `mapOption` source review.

## Assessment

The constants are available, but only a documented scalar subset maps to
native options. Unsupported combinations report OperationNotSupported rather
than silently issuing a mismatched system option.

## Other missing assertions and diagnostics

- Test valid/invalid option pairs, value ranges, post-close calls, and native
  readback on every platform.

## Final assessment

No separate finding. No source or test changed.
