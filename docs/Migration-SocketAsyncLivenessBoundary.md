<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Socket`'s async liveness boundary (ticket #2134)

*2026-08-17.* `System::Net::Sockets::Socket`'s destructor and move-assignment now **wait** for any
in-flight `ConnectAsync`/`AcceptAsync`/`SendAsync`/`ReceiveAsync` body before releasing the
descriptor.

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

The wait is **bounded**, not open-ended, and the same boundary applies to move-assignment, which
replaces the descriptor and every field while a body may still be reading them.

**To migrate:** nothing, if you already awaited your tasks before dropping the socket — that is
the case the boundary is a no-op for. If you deliberately abandoned a socket with work in flight,
the destructor now pauses where it previously returned into undefined behaviour.

### 3.1 A pending `AcceptAsync` reports the abort

A body interrupted by the boundary throws
`SocketException(SocketError::OperationAborted, "AcceptAsync: the socket was destroyed while the
accept was pending.")` rather than returning a `Socket` built from a dead descriptor. Retrieving
the task's result after the socket is gone therefore raises, instead of handing back something
unusable.

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

## 6. What this does not close

**CCF-019 remains open.** This ticket settles the boundary for `Socket` only. `HttpClient`
(#2066), `ClientWebSocket` (#2088) and `ThreadPool`/`SynchronizationContext` (#1959) have the same
shape and are not repaired here; the owned-tree members (SR-AUD-327, SR-AUD-333) are a different
problem again.
