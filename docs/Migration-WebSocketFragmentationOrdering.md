<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — fragmentation ordering is enforced (ticket #2095)

*2026-08-17.* `ClientWebSocket::ReceiveAsync` never checked that a continuation frame belonged to
a message in progress. It does now, and two frame sequences that used to be accepted are
rejected.

Landed under `docs/StandingApprovals.md` SA-5. Two private `bool` members were added and they
fit in existing padding — `sizeof(ClientWebSocket)` is **unchanged at 424**.

---

## 1. What changed

| Frame sequence from the server | Was | Is |
|---|---|---|
| a continuation (`0x0`) with no message in progress | accepted, typed from a **C++ member initialiser** | `WebSocketException` |
| a Text/Binary frame while a fragmented message is in progress | accepted; message boundaries were server-controllable | `WebSocketException` |
| the tail of a **non-final** frame that overflowed the caller's buffer | reported `endOfMessage = true` | reports the **frame's FIN** |
| a well-formed fragmented message, with or without control frames interleaved | accepted | **unchanged** |

## 2. Why this stopped being a judgement call

The ticket deferred deliberately: *"RFC 6455 §5.4 does not by itself decide what a **client
library** should surface here — rejecting is defensible and so is tolerating, and .NET's exact
choice is not verifiable with `/rv/tmp/runtime` absent."* That was the right call at the time.

It is verifiable now, and .NET rejects both:

```csharp
case MessageOpcode.Continuation:
    if (_lastReceiveHeader.Fin)                     // ManagedWebSocket.cs:1385-1390
        return SR.net_Websockets_ContinuationFromFinalFrame;
case MessageOpcode.Binary:
case MessageOpcode.Text:
    if (!_lastReceiveHeader.Fin)                    // :1405-1410
        return SR.net_Websockets_NonContinuationAfterNonFinalFrame;
```

The port uses .NET's own message texts. `_lastReceiveHeader` starts `{ Text, Fin = true }`
(`:98`), which this port matches, and it is assigned only for **data** frames — a control frame
between two fragments does not disturb the tracking, and a test covers that.

## 3. The second defect, found while fixing the first

When a frame is larger than the caller's buffer the remainder is buffered. Draining the last of
that buffer reported `endOfMessage = true` from the **buffer's** exhaustion, so the tail of a
non-final frame claimed the message had ended.

This was not in the ticket. It is fixed here because it is the same state: tracking "was the last
frame final" is only correct if the buffered tail carries the frame's FIN too. .NET returns
`header.EndOfMessage` (`ManagedWebSocket.cs:995`).

## 4. What the old behaviour actually was, and why it mattered

The old pin recorded something worth keeping: a bare continuation was reported with
`fragmentType_`'s value, and `fragmentType_`'s *member initialiser* is `Binary`. So on a fresh
connection the message **type** came from a C++ default rather than from anything the server
sent. The previous behaviour was not merely lax; it was arbitrary.

## 5. To migrate

A conforming server never produces either sequence, so conforming traffic is unaffected. If you
were relying on the old tolerance, the server is violating RFC 6455 §5.4 and .NET would have
rejected it too.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `ClientWebSocket` — **zero sites in both**. Neither
repository was modified.
