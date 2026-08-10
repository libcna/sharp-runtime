# Audit: `modules/net-sockets/src/System/Net/Sockets/Socket.cpp`

## Metadata

- AUDITED: native endpoint conversion, sync I/O, options, polling, and tasks.
- Validation: source/task review; live socket fixture is sandbox-blocked.

## SR-AUD-263 — high — Socket asynchronous operations start immediately with raw this captures that can outlive the Socket

`ConnectAsync`, `AcceptAsync`, `SendAsync`, and `ReceiveAsync` each create a
`TaskT` lambda capturing `[this]`. `TaskT` launches `std::async` immediately,
so destroying or moving the Socket while the operation waits causes the worker
to dereference freed object state. The header explicitly warns callers of that
lifetime hazard; current .NET tasks keep their Socket object alive.

## Assessment

The reviewed POSIX flag translation and offset/count validation repairs are
present. Local networking is prohibited here, so readiness/options/endpoint
paths require a final-gate native run.

## Other missing assertions and diagnostics

- Add ASan lifetime tests for all four async paths, null receive buffer,
  cancellation/disposal races, invalid enum diagnostics, and Unix endpoint
  maximum-path integration.

## Final assessment

SR-AUD-263 is confirmed from the immediate task/lifetime construction. No
source or test changed.
