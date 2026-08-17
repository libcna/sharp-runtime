<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ClientWebSocket` is concurrency-safe, and `sizeof` grows 360 → 408 (ticket #2096)

*2026-08-17.* `System::Net::WebSockets::ClientWebSocket` had a data race on four public
properties and destroyed its socket underneath a thread that was still using it. Both are fixed.

Landed under `docs/StandingApprovals.md` **SA-3** (private data members; the `sizeof` change is
pinned by the layout test and there is no vtable, base-class, signature or `noexcept` change).

**Downstream consumers must be recompiled**, because the object grew. No source change is
needed.

---

## 1. What was wrong

**Defect 1 — a data race on public properties.** `state_` was written from task threads
(`ConnectAsync`, `ReceiveAsync`, `CloseAsync`) and read by `getStateProperty()` from whatever
thread the caller was on, with no synchronisation. That is undefined behaviour, not a stale read.
Measured, **four** members had that shape where the finding named one: `closeStatus_`,
`closeStatusDescription_` and `subProtocol_` too. .NET makes its own state atomic for exactly
this reason — `Interlocked.CompareExchange(ref _state, …)`, `ClientWebSocket.cs:104,132`.

**Defect 2 — the socket was destroyed under a working thread.** `Dispose()` and `Abort()` called
`socket_->Close()` and reset the owning `unique_ptr` while a task thread could be inside
`socket_->Send` or `readExact(*socket_, …)`. The `ThrowOnInvalidState` check in
`SendAsync`/`ReceiveAsync` is a TOCTOU against it, not a guard.

**Measured, defect 2 was worse than the ticket described.** Running the new tests against the
pre-#2096 code under AddressSanitizer, all five **hang** rather than crash. `Close()` does not
reliably wake a thread already parked in `recv()` on Linux, so the old code both freed the
`Socket` object under the worker *and* left the worker parked forever — a use-after-free waiting
to fire the moment the peer sent anything, with the caller's task never completing either way.

## 2. What changed

| | Was | Is |
|---|---|---|
| `socket_` | `std::unique_ptr<Socket>` | `std::shared_ptr<Socket>` |
| ownership during I/O | the member, dereferenced repeatedly | a strong reference taken once, held for the whole operation |
| `state_`, `closeStatus_`, `closeStatusDescription_`, `subProtocol_` | unsynchronised | guarded by a new `stateMutex_` |
| `Dispose()` | `Close()` then `reset()` | take the member away under the lock, `Shutdown()` to wake any worker, then drop this function's reference |
| liveness boundary (#2088) | `SendAsync`, `ReceiveAsync` | **all five** `*Async` members |
| `sizeof(ClientWebSocket)` | **360** | **408** (`alignof` 8, unchanged) |

`Dispose()` deliberately no longer calls `Close()`. Closing a descriptor another thread is
blocked on is the hazard this repair exists to remove, not a way to implement it. If a worker
still holds a reference the descriptor stays open until that worker returns — microseconds — and
is never handed to something else underneath it.

`stateMutex_` is **never** held across socket I/O. It is taken to read or publish a field, or to
take a strong reference, and released before anything can block; otherwise `Dispose()` would
deadlock behind a `ReceiveAsync` waiting for a frame the peer may never send.

## 3. Observable behaviour

For a correctly-used, single-threaded caller: **nothing changes.** Every existing test passes
unmodified.

For a caller that disposes or aborts while an operation is in flight, three things change, all
from "undefined" to "defined":

1. the in-flight task now **completes** with a `WebSocketException` instead of hanging or
   corrupting memory;
2. `getStateProperty()` and the three other properties are safe to read from any thread;
3. destroying the object while `ConnectAsync`, `CloseAsync` or `CloseOutputAsync` is in flight is
   now covered by #2088's boundary — it was not, which this ticket also fixes.

## 4. One deliberate divergence, recorded rather than smuggled

When an operation loses the race and finds the socket already gone, it raises
`WebSocketException(WebSocketError::InvalidState)` — the **same** exception the state check would
have raised a microsecond earlier. A caller racing `Dispose()` against `SendAsync` cannot tell
which side it landed on and should not have to.

.NET raises `ObjectDisposedException` there (`ClientWebSocket.cs:166`) — but it raises it for the
*non-racy* path too, where this port has always raised `WebSocketException(InvalidState)`.
Matching .NET on one side of the race and not the other would be worse than either. The
exception-type question for the whole family is ticket **#2357**.

## 5. Evidence

* The new suite `ClientWebSocketConcurrencyTests` reproduces both defects against a real
  loopback socket — a listener that accepts and then says nothing, which parks the client in the
  handshake's blocking read.
* **ThreadSanitizer** reports `data race … Read of size 4 by thread T3 / Previous write by main
  thread` when the state accessors are reverted to their unsynchronised form, and is clean over
  three runs with them.
* **AddressSanitizer** reports `heap-use-after-free` when `ConnectAsync` is taken back out of the
  liveness boundary, and is clean with it.
* Against the pre-#2096 production code, all five new tests hang (§1).

## 6. Downstream, measured

Per SA-2 condition 5 and SA-3's migration requirement: neither `cna` nor `mobile-eggbert`
references `ClientWebSocket` or `System::Net::WebSockets` at all — **zero sites in both**. Neither
repository was modified. The full-rebuild requirement is recorded here for any future consumer.
