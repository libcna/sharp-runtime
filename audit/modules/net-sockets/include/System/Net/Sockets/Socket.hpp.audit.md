# Audit: `modules/net-sockets/include/System/Net/Sockets/Socket.hpp`

## Metadata

- AUDITED: Socket ownership, sync/async public surface, and documented scope.
- Evidence: task implementation, source review, and blocked socket fixture.

## Assessment

The header candidly documents raw-`this` asynchronous captures, but such a
caller lifetime requirement is incompatible with a managed Socket reference.
Every async entry starts a `TaskT` immediately and retains the raw pointer,
forming SR-AUD-263.

## Other missing assertions and diagnostics

- Add ASan tests that destroy/move Socket during each async operation, null
  receive-buffer checks, cancellation/disposal races, and closed-socket errors.

## Final assessment

SR-AUD-263 applies. No source or test changed.
