# Audit: `modules/net-websockets/include/System/Net/WebSockets/ClientWebSocket.hpp`

## Metadata

- AUDITED: public client state, transport scope, asynchronous operations, and
  object/buffer lifetime contract.
- Validation: direct ASan probe constructed `wss://` work, destroyed the stack
  client, then waited; it reported stack-use-after-scope in `ConnectAsync`.

## SR-AUD-247 — high — asynchronous operations retain raw ClientWebSocket pointers after destruction

`ConnectAsync`, `SendAsync`, `ReceiveAsync`, `CloseOutputAsync`, and
`CloseAsync` create immediately-running `Task` lambdas capturing raw `this`.
The direct ASan probe destroys a stack client after `ConnectAsync` returns;
the background lambda then reads the dead object at `ClientWebSocket.cpp:222`.
The destructor only calls `Dispose`; it neither owns nor joins outstanding
tasks.  Current .NET retains managed lifetime/state through its async operation
and cannot expose this use-after-scope contract.

## Assessment

The header candidly warns about caller-owned vector lifetime, but it does not
state or enforce the equally necessary client-object lifetime.  The `ws://`
transport and missing TLS/compression are declared scope boundaries; they do
not explain the reachable raw-owner use-after-free.

## Other missing assertions and diagnostics

- Add ASan regressions for destruction during connect, send, receive, and both
  close operations, plus `Dispose` racing an active operation.
- Give each operation an ownership/lifecycle completion mechanism rather than
  documenting raw references as a substitute for safety.

## Final assessment

SR-AUD-247 is ASan-confirmed. No source or test was changed during this audit.
