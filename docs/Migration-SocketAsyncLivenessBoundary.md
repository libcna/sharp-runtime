<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Socket`'s async liveness boundary (ticket #2134)

*2026-08-17; final-audit follow-up 2026-08-22.* `System::Net::Sockets::Socket`'s destructor,
public `Close()`, move constructor and both sides of move-assignment now **wait** for any in-flight
`ConnectAsync`/`AcceptAsync`/`SendAsync`/`ReceiveAsync` body before releasing, replacing or
transferring the descriptor.

**No public signature changed**, and every one of #2139's four pins survives: `Socket` does not
become `enable_shared_from_this`, `~Socket` stays `noexcept`, `Socket` stays move-assignable and
non-copyable, and the async return types are untouched. Two things did change — an object layout
and a destructor that can now block — and this note is about those.

---

## 1. What was wrong

Each `*Async` member builds a `TaskT` from a lambda capturing raw `this`, and `TaskT` dispatches
with `std::async(std::launch::async)` immediately. `Socket` was move-assignable and destructible
with no join, no `shared_from_this` and no retention of any kind, so destroying or move-assigning
one while a body was running read freed storage (SR-AUD-263, CCF-019).

.NET needs no boundary: the GC keeps the object alive for as long as the captured delegate can
reach it. C++ has no such mechanism, so this port takes the RAII answer — the object outlives the
work because its destructor waits for it, the same shape `FileSystemWatcher` took in #2347.

## 2. The layout change, and who must rebuild

| Type | `sizeof` before | `sizeof` after | `alignof` |
|---|---:|---:|---|
| `Socket` | 24 | **40** | 8, unchanged |

One `std::shared_ptr` was added, and the scalar block gained alignment padding once an 8-byte
member joined it — which is why the growth is 16 rather than 8. **Every consumer must be fully
recompiled**: a `sizeof` change across a stale-header boundary is an ODR violation with no
diagnostic. Landed under `docs/StandingApprovals.md` SA-3, pinned by
`SocketsGatedBehaviourPins.Fix2134_LayoutPin_TheCostOfTheBoundary` against shadow structs rather
than literals.

## 3. The behaviour change: a destructor that can block

```cpp
{
    Socket listener(/* … */);
    listener.Listen(1);
    auto pending = listener.AcceptAsync();
}   // was: returned immediately, and `pending` then read freed storage
    // now: waits here until the accept body has finished
```

Teardown is crossable: `AcceptAsync` observes a stop flag in bounded poll slices, while
`ConnectAsync`/`SendAsync`/`ReceiveAsync` are interrupted with `shutdown()` before a descriptor
that will be discarded is closed. A **source-side move is intentionally different**: shutdown is
irreversible, so it stops a pending accept but lets blocking connect/send/receive work finish
naturally before transferring the still-usable descriptor. Such a move can therefore wait for its
peer; preserving the resource and promising a bounded cancellation cannot both be true with the
portable socket primitives this runtime exposes.

**To migrate:** nothing, if you already awaited your tasks before dropping the socket — that is
the case the boundary is a no-op for. If you deliberately abandoned a socket with work in flight,
the destructor now pauses where it previously returned into undefined behaviour.

### 3.1 A pending `AcceptAsync` reports the abort

A body interrupted by the boundary throws
`SocketException(SocketError::OperationAborted, "AcceptAsync: the socket was destroyed while the
accept was pending.")` rather than returning a `Socket` built from a dead descriptor. The same
diagnostic is used when `Close()` or a move stops the accept; the descriptor itself remains usable
after a source move.

## 4. Why `AcceptAsync` is implemented differently from its three siblings

`shutdown()` reliably unblocks `recv()` and `send()`, which is how the boundary reaches the other
three bodies. On Linux it does **not** unblock `accept()` on a listening socket — it returns
`ENOTCONN` and the accept stays blocked — and `close()` is unsafe while another thread is inside
the syscall. A boundary that could never be crossed would have turned a use-after-free into a
**hang**, which is not a repair.

`AcceptAsync` therefore waits on this type's own portable `Poll()` in 50 ms slices and re-checks a
stop flag between them, calling `Accept()` only once the descriptor is readable. From the outside
this is still one blocking accept, and the synchronous `Accept()` is untouched.

## 5. Downstream, measured

Per `docs/StandingApprovals.md` SA-2 condition 5, both consumer checkouts were searched: neither
`cna` nor `mobile-eggbert` names `Socket`, `ConnectAsync`, `AcceptAsync`, `SendAsync` or
`ReceiveAsync` — **zero sites in both**. Neither repository was modified. Both must still be
rebuilt, as every consumer must.

## 6. Final-audit closure

The paragraph that originally followed here said CCF-019 remained open. That was accurate when
#2134 landed, but is historical now: `HttpClient` was repaired by #2066,
`ClientWebSocket` by #2088, and the ThreadPool/SynchronizationContext lifetime family by #1959.
The final reconciliation rechecked those implementations and the current audit index supersedes
that checkpoint.

The same recheck found four residual `Socket` boundary holes and closed them under #2417:

- move construction and the source side of move-assignment now drain raw-`this` work before any
  source field is read or cleared;
- a drained move-assignment destination reopens its guard for new async operations;
- the caller-side registration is RAII-owned before Task construction, so allocation or launch
  failure cannot leak an in-flight count;
- public `Close()` uses the same teardown boundary as destruction.

The source-move path was then separated from destructive teardown after a connected-socket
regression proved that shutdown-and-transfer merely replaced a race with a disabled resource.
A pending `ReceiveAsync` now delays the move until its peer supplies data; the moved-to socket is
then exercised bidirectionally. SR-AUD-263 and CCF-019 are therefore fully remediated, not deferred.
