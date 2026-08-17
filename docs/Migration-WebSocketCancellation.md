<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a cancelled `ClientWebSocket` operation aborts the WebSocket (ticket #2093)

*2026-08-17.* All five `ClientWebSocket` `*Async` members took a `CancellationToken` and none
consulted it. They all do now, and cancelling any of them **aborts the whole WebSocket**.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout, vtable or `noexcept` change
— the parameters were already there.

---

## 1. What changed

| Situation | Was | Is |
|---|---|---|
| an already-cancelled token | ignored; the operation ran normally | `TaskCanceledException`, before any I/O |
| a token that fires mid-operation | ignored; the operation blocked until the peer answered | the WebSocket is aborted and the task faults with `TaskCanceledException` |
| `getStateProperty()` after a cancellation | unchanged | `WebSocketState::Aborted` |
| a live but unfired token | ignored | **unchanged** — nothing is aborted |
| `CancellationToken::None()` (the default) | ignored | **unchanged** |

`TaskCanceledException` derives from `System::OperationCanceledException`, so `catch (const
System::OperationCanceledException&)` catches it.

## 2. Why this is nine lines and not a transport redesign

The ticket, and `CloseAsync`'s own comment, both recorded the blocker correctly: the tasks run a
**blocking** `Socket::Receive`, so cancellation needs a socket timeout, a poll-based
non-blocking read loop, or a shutdown-based interrupt — "a transport-level design decision
touching `modules/net-sockets` behaviour, and doing it for one method is worse than not doing
it".

The first half is true. The second turned out to cost nine lines, because .NET's answer is none
of those three:

```csharp
registration = cancellationToken.Register(static s => ((ManagedWebSocket)s!).Abort(), this);
```

`ManagedWebSocket.cs:608,789`. **Cancelling any WebSocket operation aborts the whole WebSocket**
— that is .NET's documented contract, not an implementation shortcut, and it is why no per-read
polling is needed anywhere. This port's `Abort()` already shuts the socket down safely under
#2096's shared ownership, which is exactly what unblocks a worker parked in `recv()`.

So the repair is a small RAII scope that registers `Abort()` for the duration of one operation,
throws on entry if the token has already fired, and translates the resulting failure into
`TaskCanceledException`. It is applied to **all five** members at once, which is what the old
comment asked for.

## 3. The consequence worth stating plainly

**Cancellation is not a "stop waiting" operation — it destroys the connection.** After a
cancelled `ReceiveAsync`, the `ClientWebSocket` is `Aborted` and cannot be used again. That is
what .NET does, and a caller that wants a bounded wait without losing the connection needs a
different mechanism (WebSocket-level keep-alive, ticket #2094) rather than a cancellation token.

## 4. To migrate

If you passed a token and relied on it being ignored — which was the only possible behaviour
before — the operation will now fault. Pass `CancellationToken::None()` (the default) where you
did not mean to cancel.

If you already wrote `try { … } catch (const System::OperationCanceledException&) { … }` around
these calls in anticipation, that code starts working.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `ClientWebSocket` or `System::Net::WebSockets` at
all — **zero sites in both**. Neither repository was modified.

## 6. A test-harness defect this exposed

`RunAgainstServer` in `WebSocketsGatedBehaviourPins.cpp` joined its server thread on the
success path only. Once a client body could legitimately throw, a failing assertion took the
**whole suite** down with `terminate called without an active exception` and no failing test
name. The join is RAII now. Worth recording because the symptom names no test and is easy to
misread as a defect in the code under test.
