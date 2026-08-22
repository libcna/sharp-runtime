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

## Final reconciliation (2026-08-22): REMEDIATED

The assessment above is the original audit snapshot, not current behavior. Ticket #2134 added a
shared in-flight boundary and an interruptible polling path for `AcceptAsync`. The final #2417
review then closed four residual edges: move construction and the source side of move-assignment
drain before reading source fields, a moved-to destination reopens its guard, caller-side RAII
registration rolls back when Task construction/launch throws, and public `Close()` drains before
retiring `fd_`.

The destructive and preserving paths are intentionally distinct. Destruction, `Close()` and the
old destination side of move-assignment may call `shutdown()` because they immediately discard the
descriptor. A source-side move cannot: shutdown is irreversible, so pending
Connect/Send/Receive work completes naturally before transfer; pending Accept observes the stop
flag in bounded poll slices. A connected-socket regression holds `ReceiveAsync` across move,
supplies its byte from the peer, and then proves the moved-to socket still sends and receives in
both directions. Separate deterministic cases cover Accept, Close, move construction, move
assignment, failed task startup, and registration count. The test-only seam is ODR-checked and
unreachable from ordinary consumers. SR-AUD-263 and its CCF-019 family are fully closed.
