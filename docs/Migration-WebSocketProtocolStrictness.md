<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::Net::WebSockets` protocol strictness (#2090, #2089, #2091)

`docs/SystemNetWebSocketsNamespaceReviewPlan.md` §12 requires this note. Three tickets narrow
what `System::Net::WebSockets` accepts. **No public signature, object layout, vtable, base class
or exception specification changed in any of them** — every change is to *what input is
accepted*, and every rejection replaces behaviour that was silently wrong.

The rules below are **RFC 6455 and RFC 7230 cited as protocol facts, not as .NET behaviour**.
`/rv/tmp/runtime/src/libraries/` is absent, so the *exception identity* each door raises is
recorded as **this port's choice** rather than as a match to .NET.

---

## 1. What now throws that did not before

### #2090 — the frame header (a **server** drives this)

A `WebSocketException` is thrown by `ReceiveAsync`/`CloseAsync` when the server sends a frame
with any reserved bit (RSV1/2/3) set, a reserved opcode (`0x3`–`0x7`, `0xB`–`0xF`), a mask bit
set, a fragmented control frame (FIN=0 on `0x8`/`0x9`/`0xA`), or a control payload above 125
bytes.

Previously a reserved opcode was **delivered to the caller as application data**, and a 256 MiB
"Ping" was read into memory and **echoed back** as a Pong.

### #2089 — the handshake's caller-text doors (**you** drive this)

| Door | Now throws | When |
|---|---|---|
| `ClientWebSocketOptions::SetRequestHeader(name, value)` | `System::ArgumentException` | either argument contains CR, LF or NUL |
| `ClientWebSocketOptions::AddSubProtocol(s)` | `System::ArgumentException` | `s` is not a single RFC 7230 `token` |
| `ClientWebSocket::ConnectAsync(uri)` | `System::ArgumentException` (`paramName` `uri`) | the URI's host, path or query contains CR, LF or NUL |

### #2091 — the close handshake and message payloads (a **server** drives this)

See §4 below.

---

## 2. #2089 in detail — what changed and why

**`SetRequestHeader`.** `performHandshake` concatenates `name + ": " + value + "\r\n"` straight
into the upgrade request. A value carrying CRLF therefore injected an extra header field, and a
*name* carrying CRLF injected a whole request line. Measured before the repair
(`build-probe/2089_probe1_before.log`), a six-field request became an **eight-field** one
carrying a smuggled `GET /admin HTTP/1.1`.

**The request URI is a third door, which SR-AUD-248 does not name.** The request line and the
`Host:` field are built from the URI, not from the options bag, and `System::Uri` preserves CR,
LF and NUL in both components (`build-probe/2089_probe2_uri_door.log`). Validating only the
options doors would have left request smuggling open. This mirrors the scope correction #2063
made for `System::Net::Http`, where the request URI likewise turned out to be a door the review's
paraphrase had dropped. **`System::Uri` itself is unchanged** — the Uri-side defect is the
separate, still-blocked ticket #2003.

**`AddSubProtocol`.** The old check rejected `c <= 0x20 || c == 0x7F` and **not one** RFC 7230
separator, so `chat,evil` was stored as a *single* subprotocol and joined into
`Sec-WebSocket-Protocol` with `", "` — the caller advertised two protocols where the API reported
one. The rejected set is now the separator set `( ) < > @ , ; : \ " / [ ] ? = { }` plus the
existing control/space/DEL range.

### Still accepted, deliberately

- a **space** and a **horizontal tab** inside a header value;
- every other C0 control (`\x07`, `\x0B`, …) — they do not terminate a field;
- **percent-encoded** control characters (`a%0D%0Ab`) — they are ordinary text on the wire;
- an **empty** header value;
- header values containing `;`, `=`, `,`, `/`, `"` — a header value is not a token;
- arbitrary **Unicode** text in a header value;
- every RFC 7230 `tchar` in a subprotocol: `-`, `.`, `_`, `+`, `!`, `#`, `$`, `%`, `&`, `'`, `*`,
  `^`, `` ` ``, `|`, `~`, digits and letters.

### The accepted path is byte-identical

For input that is accepted, the emitted handshake request is unchanged — pinned by
`ClientWebSocketHandshakeValidationTests.TheEmittedRequestIsByteIdenticalForAcceptedInput` and
`.ALegalUriStillConnectsAndIsUnchangedOnTheWire`.

### Rejection happens before any bytes are sent

All three doors reject before a socket exists. `SetRequestHeader`/`AddSubProtocol` reject at
configuration time; the URI check runs **before** `socket_` is constructed, so a rejected URI
opens no connection, sends no byte and leaks no descriptor — measured with `/proc/self/fd` over
20 attempts.

### The exception type, and why it is `ArgumentException`

`System::Net::Http`'s equivalent protocol-field doors throw `System::FormatException` (#2063).
This module throws `System::ArgumentException` instead, because within `ClientWebSocketOptions`
the sibling character check (`AddSubProtocol`) **already** reported invalid characters as an
argument error. Reporting one door's bad characters as an argument error and its neighbour's as a
format error would be arbitrary — #2063 resolved the same tension the same way for its multipart
doors. Recorded as this port's choice.

### The rejected text is never echoed

No rejection message contains the offending text. It is attacker-controlled by construction and
exception messages are very likely to be logged; echoing a CR/LF-bearing value into a log
re-creates exactly the injection the rejection exists to prevent.

---

## 3. One shared predicate, not a second policy

#2063 established the CR/LF/NUL rule for ten `System::Net::Http` doors. #2089 needed the same
rule on a second protocol — and the RFC 6455 upgrade **is** an HTTP/1.1 request, framed by the
same three characters.

The predicate body therefore lives in exactly one place,
`System::Net::detail::ContainsProtocolFieldTerminator`
(`modules/net/include/System/Net/detail/ProtocolFieldValidation.hpp`), in the `Net` component
that both protocol modules already depend on.
`System::Net::Http::detail::ContainsProtocolControlCharacter` is kept as a **forwarder** under its
original name, so **no `System::Net::Http` call site, exception type or message changed**.

`Net.WebSockets`' declared dependency on `Net` moved from `PRIVATE` to `PUBLIC` because a public
header now uses it. **No component edge was added** (the edge already existed) and no consumer
gained a new closure — `Net` already reached `Net.WebSockets` consumers transitively through
`Net.Sockets`. The graph is unchanged at 41 modules and 91 edges.

**This is not the CCF-021 promotion.** That family is minted from the `Net.Http.Headers` review,
which still holds two of its four findings
(`docs/SystemNetWebSocketsNamespaceReviewPlan.md` §8.2). Sharing the predicate is only the
mechanism that stops a second, quietly diverging copy of the rule from being written meanwhile.

---

## 4. #2091 in detail

Recorded when #2091 lands; see §5 of this note.

---

## 5. If a rejection breaks you

Every rejection replaces behaviour that was silently wrong: a smuggled request line, a
misadvertised protocol list, a reserved opcode delivered as data, or a frame the RFC required
the client to reject. There is no opt-out, and none is planned.

- **Header text carrying CR/LF** — the header was never being sent as one field. Split it into
  separate `SetRequestHeader` calls.
- **`AddSubProtocol("a,b")`** — call `AddSubProtocol` once per protocol; the joined
  `Sec-WebSocket-Protocol` value is built for you.
- **A URI carrying CR/LF/NUL** — percent-encode it (`%0D`, `%0A`, `%00`); percent-encoded forms
  are still accepted.
- **A server whose frames are now rejected** — the server is not conforming to RFC 6455. That is
  the finding, not the repair.
