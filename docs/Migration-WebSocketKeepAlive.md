<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ClientWebSocket` keep-alive is real, and `sizeof` grows 408 → 424 (ticket #2094)

*2026-08-17.* `KeepAliveInterval` and `KeepAliveTimeout` were validated, stored and returned, and
nothing read them. They now drive a background heartbeat.

Landed under `docs/StandingApprovals.md` **SA-3** (one private data member; `sizeof` pinned by the
layout test, no vtable, base-class, signature or `noexcept` change) and **SA-5** (the behaviour is
transcribed from the reference).

**Downstream consumers must be recompiled.** No source change is needed.

---

## 1. What changed

.NET picks between **two** strategies by whether `KeepAliveTimeout` is positive
(`ManagedWebSocket.cs:169-198`), and this port now does the same:

| `KeepAliveInterval` | `KeepAliveTimeout` | Behaviour |
|---|---|---|
| `<= 0` or `InfiniteTimeSpan` | any | **no heartbeat at all** — no thread, no frames |
| `> 0` | `<= 0` or `InfiniteTimeSpan` (**the default**) | **unsolicited Pong** every interval; nothing is expected back, so it cannot fault |
| `> 0` | `> 0` | **Ping/Pong**: a Ping carrying an 8-byte big-endian counter, and the connection is aborted if no Pong echoes it within the timeout |

**The default is unchanged in effect and in defaults**: 30 seconds and `InfiniteTimeSpan`, which
this port already matched (`WebSocketDefaults.cs:15-17`). So an existing caller who never touched
these properties gets an unsolicited Pong every 30 seconds where it previously got nothing — and
nothing else moves, because that strategy has no failure mode.

## 2. The one thing to know before setting `KeepAliveTimeout`

**A Pong is only observed while a `ReceiveAsync` is running.** This port has no independent
receive pump — and neither does .NET's `ManagedWebSocket`, which also processes pongs inside
`ReceiveAsyncPrivate`. So a caller who enables Ping/Pong and then *never receives* will lose the
connection to a keep-alive timeout even against a perfectly healthy server that answers every
Ping.

That is exactly why the default is unsolicited Pong. It is pinned by a test —
`Fix2094_PingPongNeedsAReceiverAndThatLimitIsPinnedNotHidden` — rather than left as a comment, so
closing the gap later cannot happen silently.

If you set `KeepAliveTimeout`, keep a `ReceiveAsync` outstanding.

## 3. Layout

`sizeof(ClientWebSocket)`: **408 → 424** (`alignof` 8, unchanged). #2096 took it 360 → 408 in the
same session; the combined move for a consumer upgrading across both is **360 → 424**.

## 4. It needed a defect in another module fixed first

`Socket::Send` called `::send()` without `MSG_NOSIGNAL`, so writing to a socket whose peer had
closed raised **SIGPIPE and terminated the process**. The heartbeat is the first thing in this
repository that writes on a background thread without the peer necessarily reading, so it was the
first thing to hit it. That is ticket **#2358**, fixed separately and first — see
`build-probe/2094_probe1_sendafterclose.cpp`, a thirty-line reproduction with no WebSocket in it.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `ClientWebSocket` or `System::Net::WebSockets` —
**zero sites in both**. Neither repository was modified. `Socket` itself is likewise unreferenced
in both.
