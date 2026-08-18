<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ClientWebSocket`'s outer gate raises .NET's exceptions (ticket #2357)

*2026-08-18.* Every `ClientWebSocket` operation raised `WebSocketException(InvalidState)` when
the instance was disposed or had never connected. .NET raises `ObjectDisposedException` and
`InvalidOperationException` there.

Landed under SA-5 on the user's decision of the same date. **The exception type changes** at five
doors. No signature, layout, vtable or `noexcept` change — and, notably, **no new data member**:
`sizeof(ClientWebSocket)` is unchanged at 424, so this is not an SA-3 change.

---

## 1. The ticket's framing was too simple, and the reference corrected it

The ticket reported that .NET raises `ObjectDisposedException` or `InvalidOperationException` and
that *"neither type is ever `WebSocketException` there"*. .NET actually has **two layers**, and
only the outer one avoids `WebSocketException`:

| Layer | Where | Raises |
|---|---|---|
| **outer** | `ClientWebSocket.ConnectedWebSocket` (`ClientWebSocket.cs:163-177`) | `ObjectDisposedException` if disposed; `InvalidOperationException("The WebSocket is not connected.")` if never connected or still connecting |
| **inner** | `WebSocketStateHelper.ThrowIfInvalidState` (`WebSocketStateHelper.cs:21-41`), per operation | **`WebSocketException(InvalidState)`** when the current state forbids the operation |

This port had only the inner layer — `WebSocket::ThrowOnInvalidState`, which is a faithful
counterpart of .NET's own `protected static WebSocket.ThrowOnInvalidState` and **was already
correct**. Rewriting it would have replaced a correct exception with a wrong one. The outer layer
did not exist here at all, and is what this ticket adds.

## 2. What changed

| Situation | Was | Is |
|---|---|---|
| `SendAsync` / `ReceiveAsync` / `CloseAsync` / `CloseOutputAsync` after `Dispose()` or `Abort()` | `WebSocketException(InvalidState)` | **`ObjectDisposedException`** |
| the same before `ConnectAsync` completes | `WebSocketException(InvalidState)` | **`InvalidOperationException`**, *"The WebSocket is not connected."* |
| `CloseAsync` after `Dispose()` | **succeeded as a no-op** | `ObjectDisposedException` |
| a state that forbids the operation on a **live** socket | `WebSocketException(InvalidState)` | **unchanged** |
| the `Dispose()` race in `socketForIo()` | `WebSocketException(InvalidState)` | `ObjectDisposedException` — still matching the non-racy path, which is what #2096 required |

## 3. No new data member was needed

.NET's `InternalState` maps exactly onto state this class already holds, because `Abort()` calls
`Dispose()` (`ClientWebSocket.cs:179-193`) — so *Aborted is Disposed* there — and because
`socket_` is assigned only on a successful connect and cleared only by `Dispose()`:

| `InternalState` | condition here |
|---|---|
| `Created` | `!connectStarted_` |
| `Connecting` | `connectStarted_ && !socket_ && state_ == Connecting` |
| `Disposed` | `connectStarted_ && !socket_ && state_ != Connecting` |
| `Connected` | `socket_ != nullptr` |

`CloseAsync` deliberately does **not** clear `socket_`, matching .NET, where a closed socket is
still `InternalState.Connected` and the *inner* layer reports the state.

## 4. To migrate

```cpp
try { ws.SendAsync(...).Wait(); }
catch (const WebSocketException&) { /* was reached for a disposed socket */ }

// now:
catch (const System::ObjectDisposedException&) { /* disposed */ }
catch (const System::InvalidOperationException&) { /* never connected */ }
catch (const WebSocketException&) { /* live socket, wrong state -- unchanged */ }
```

If you called `CloseAsync` on a disposed instance and relied on it succeeding, guard it or drop
it: `Dispose()` has already closed the connection.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `System::Net::WebSockets` — **zero sites in both**.
Neither repository was modified.
