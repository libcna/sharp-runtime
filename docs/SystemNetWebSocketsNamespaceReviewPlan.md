<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Net::WebSockets` namespace review — ticket #2087

Owning ticket **#2087**. This document is the durable record; it **remediates nothing by
itself**. Every claim is measured against the tree at `59ef5b9`.

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-04. Every statement about
.NET below comes from repository-contained evidence only: the per-file audit reports, the
doc-comments transcribed from .NET when the module was written, and this module's own tests.
Where a repair would need .NET's exact behaviour and no repository evidence pins it, a
**deferred-verification ticket** is created instead of a guess. **RFC 6455 requirements are
cited as protocol facts, not as .NET behaviour** — they are what the wire format says, and the
module's own comments already reason in those terms.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit defects
found by this review carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket #1773 stays blocked.

---

## 1. Why this unit was selected — measured, not inherited

Re-derived from `audit/AUDIT_FINDINGS_INDEX.md` at `59ef5b9`, **after** `System::Xml` closed.
Every unit with at least six open findings:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `modules/core` | 72 | 9 | 59 | 4 | 12% | 1 | 47 | family plans only |
| `modules/threading` | 17 | 6 | 11 | 0 | 35% | 0 | 21 | **yes** |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7% | 12 | 8 | **yes** |
| `modules/text` | 11 | 1 | 10 | 0 | 9% | 11 | 3 | **yes** |
| `modules/io` | 11 | 0 | 11 | 0 | 0% | 0 | 2 | none |
| `modules/uri` | 10 | 0 | 10 | 0 | 0% | 10 | 4 | **yes** |
| `modules/time-zone` | 7 | 0 | 7 | 0 | 0% | 0 | 0 | none |
| `modules/text-json` | 7 | 1 | 6 | 0 | 14% | 1 | 0 | none |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none |
| **`modules/net-websockets`** | **6** | **2** | **4** | **0** | **33%** | **0** | **0** | **none** |
| `modules/net-http` | 6 | 1 | 5 | 0 | 16% | 2 | 3 | **yes** (closed) |

`modules/xml` is now at **1** open (SR-AUD-354, deferred) with **7** remediated, so it drops out.

### Applying the stated priorities, in order

1. **High-severity memory or lifetime risk.** `net-websockets` wins outright: SR-AUD-247 is an
   **ASan-confirmed use-after-free**, and it has the highest high-severity ratio (33%) of any
   unreviewed unit with ≥6 open findings. `modules/threading` is 35% but **already has a
   review**. `io` and `time-zone` have **zero** highs.
2. **Public-input attackability.** This module parses **frames arriving from a remote server**:
   header bits, 16- and 64-bit payload lengths, masking, opcodes, close payloads, and an HTTP
   upgrade response. Apart from `net-http` (already reviewed and closed for compatible work),
   it is the most remotely-attackable surface in the repository. `text-json` is comparable;
   `io`, `time-zone` and `globalization` are weaker.
3. **Useful compatible queue.** Measured in §14: **three** compatible tickets, **five** blocked
   or design, **one** deferred. The blocked ones are genuinely blocked, not merely large.
4. **Coherent module boundary.** One CMake component (`Net.WebSockets`, `TYPE STATIC`): **12
   public headers, one 504-line body, two test files**, ~1,650 lines total. The smallest
   coherent unit in the candidate set, and the whole protocol lives in one file.
5. **No existing complete review.** Correct — none.

### The objection this review answers head-on

The `System::Xml` review (§1) declined `net-websockets` because *"reviewing it today produces a
review whose highest-value finding is blocked on arrival"*. That reasoning is **explicitly
overruled here**, and the overruling is the point: a review whose top finding is blocked can
still complete the design, identify the compatible work, pin current behaviour, establish CCF
promotion evidence, and stop a competing local policy from being invented. This review does all
five — and, critically, **the compatible work it found is not the leftovers.** §7 records
**eleven post-audit protocol-validation defects** on a remotely-driven parser, every one of them
reachable before the blocked lifetime question is ever touched.

**Selected: `modules/net-websockets`.** `modules/io` is the recommended next unit (§17).

---

## 2. Scope and file inventory

Component `Net.WebSockets` (`modules/net-websockets/CMakeLists.txt`): `TYPE STATIC`,
`PUBLIC_DEPENDENCIES ComponentModel Core.Base Net.Sockets Threading Threading.Tasks Uri`,
`PRIVATE_DEPENDENCIES Net`.

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 12 | 738 |
| implementation | 1 (`ClientWebSocket.cpp`) | 504 |
| tests | 2 | 402 |

**In scope:** everything under `modules/net-websockets/`.

**Out of scope, and why:**

- `System.Net.WebSockets` **server** support (`HttpListenerWebSocketContext`,
  `WebSocket::CreateFromStream`) — absent by design; no finding names it.
- **TLS / `wss://`** — `performHandshake` throws `PlatformNotSupportedException` for `wss`, and
  symmetric/asymmetric cryptography and TLS are an explicit permanent deviation in `CLAUDE.md`.
  Not a finding, not fixable here.
- `permessage-deflate` (`WebSocketDeflateOptions`) — documented out of scope in
  `ClientWebSocketOptions`' own header comment.
- `modules/net-sockets` — the transport. Its five open findings are its own; this review
  measures its behaviour and never edits it.

---

## 3. Complete public-surface inventory

| Area | Types / members | Notes |
|---|---|---|
| Abstract base | `WebSocket` — `getCloseStatus/CloseStatusDescription/SubProtocol/StateProperty`, `Abort`, `CloseAsync`, `CloseOutputAsync`, `Dispose`, `SendAsync`/`ReceiveAsync` (full and whole-buffer overloads), `ThrowOnInvalidState` | pure-virtual surface plus non-virtual convenience overloads |
| Client | `ClientWebSocket` — `getOptionsProperty`, `ConnectAsync`, the six overridden members | **one concrete implementation**; all protocol logic is here |
| Options | `ClientWebSocketOptions` — `SetRequestHeader`, `AddSubProtocol`, `KeepAliveInterval`, `KeepAliveTimeout`, `SetBuffer`, `CollectHttpResponseDetails`, `setToReadOnly` (private, `friend ClientWebSocket`) | a property bag; two of its properties are **inert** (SR-AUD-252) |
| Creation | `WebSocketCreationOptions` | storage only; nothing consumes it |
| Results | `WebSocketReceiveResult`, `ValueWebSocketReceiveResult` | |
| Enums | `WebSocketState`, `WebSocketMessageType`, `WebSocketCloseStatus`, `WebSocketError`, `WebSocketMessageFlags` | |
| Exception | `WebSocketException : System::ComponentModel::Win32Exception` — `getWebSocketErrorCodeProperty`, `getErrorCodeProperty` | one overload **discards** its `exception_ptr` (SR-AUD-250) |

**Internal (file-local) surface that carries the protocol**, and therefore this review's real
subject: `sha1`, `randomBytes16`, `randomMaskingKey`, `readExact`, `validateWebSocketBuffer`,
`ClientWebSocket::performHandshake`, `sendFrame`, `readFrame`, `sendCloseFrame`.

---

## 4. Every open finding, with its measured disposition

| Finding | Severity | Reproduced? | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-247 | high | by the audit, ASan-confirmed | **blocked — CCF-019, no selected repair** | **#2088** |
| SR-AUD-248 | high | yes, by reading the emitted request | compatible | **#2089** |
| SR-AUD-249 | medium | yes, and **narrower than filed** | compatible | **#2089** |
| SR-AUD-250 | medium | yes, explicit `(void)innerException` | **blocked — needs a base-class change** | **#2092** |
| SR-AUD-251 | medium | yes, all four tokens unused | **design, blocked** | **#2093** |
| SR-AUD-252 | medium | yes, storage only | **design, blocked** | **#2094** |

### 4.1 SR-AUD-247 — every async member captures a raw `this` (high) → **#2088, BLOCKED**

`ConnectAsync`, `SendAsync`, `ReceiveAsync`, `CloseAsync` and `CloseOutputAsync` all return
`Task([this, …]{ … })`. The task body dereferences `this` when it runs; nothing keeps the
`ClientWebSocket` alive until then. The audit confirmed the use-after-free with ASan.

**This is CCF-019 verbatim** — the same shape as SR-AUD-310 (`HttpClient`'s five async members)
and SR-AUD-327 (`JsonNode`) and SR-AUD-333 (`Xml.Linq`'s borrowed views). #2066 recorded **two
competing options with no selection** (`docs/SystemNetHttpNamespaceReviewPlan.md` §20.7), and
that is still true. **This review does not select one**, and §16 records why doing so locally
would be the wrong move.

**Corrected premise, and it makes the finding WIDER.** The audit says *"all client async
operations share the capture pattern"*. Measured, **`SendAsync` and `ReceiveAsync` capture a
second borrowed object**: `[this, &buffer, …]` takes the caller's `std::vector<bytecs>` **by
reference** into a task that may outlive the caller's expression. A caller who writes
`ws.SendAsync(makeBuffer(), 0, n, …)` — or who lets a local buffer leave scope before `Wait()`
— gets a dangling reference to *their own* storage, not to the socket. That is a **separate
borrowed-pointer edge** governed by the same policy question, and it is recorded here so the
eventual CCF-019 repair covers both. It is **not** a new CCF and **not** a new `SR-AUD`.

### 4.2 SR-AUD-248 — CR/LF in a request header reaches the upgrade request (high) → **#2089, compatible**

`SetRequestHeader(name, value)` stores anything; `performHandshake` concatenates
`name + ": " + value + "\r\n"` straight into the request. A value containing `\r\n` therefore
injects **additional handshake headers**, and a name containing `\r\n` injects a whole
**request line**. This is the `System::Net::Http` NH-B / CCF-021 shape exactly, on a different
protocol.

**The repair is already written in this repository.** `System::Net::Http` #2063 built one shared
predicate and routed **ten** doors through it (`docs/Migration-HttpControlCharacterRejection.md`).
This module needs the same rule at its own doors — it must not invent a second one, which is
precisely the "prevent duplicated policy" value §1 claims for reviewing a unit whose top finding
is blocked.

### 4.3 SR-AUD-249 — the subprotocol validator is narrower than the finding says → **#2089, compatible**

The finding says C++ accepts `chat,evil` as one subprotocol. **Measured, the premise needs
correcting in the port's favour and against it at once.** `validateSubprotocol` **does** exist
and **does** reject `c <= 0x20` and `c == 0x7F`. What it does **not** reject is the RFC 7230
`tchar` separator set — `,` `;` `(` `)` `<` `>` `@` `\` `"` `/` `[` `]` `?` `=` `{` `}` `:` —
so `chat,evil` and `a;b` still pass, exactly as filed, while a space or a control character is
already caught. So the finding is **correct in its example and wrong in implying no validation
exists**; the repair is to widen an existing validator, not to add one.

### 4.4 SR-AUD-250 — the inner exception is discarded (medium) → **#2092, BLOCKED**

`WebSocketException(error, message, std::exception_ptr)` contains a literal
`(void)innerException;` with the comment *"Win32Exception has no inner-exception-carrying
constructor to forward to."* The comment is **accurate**, which is what blocks the ticket: the
fix is a change to `System::ComponentModel::Win32Exception` (or to `System::Exception`) in
`modules/component-model` / `modules/core` — **another component**, and a public constructor
addition on a widely derived base. Not compatible, not local. Blocked on approval.

### 4.5 SR-AUD-251 — every `CancellationToken` is ignored (medium) → **#2093, design, blocked**

All five async members take a token; all five ignore it. `CloseAsync`'s own comment already
says so honestly and says *"wiring up real cancellation would need to interrupt the loop's
blocking socket read mid-flight, which only makes sense done consistently across all of this
file's async methods at once"*. **That analysis is correct and this review endorses it**: the
tasks run a **blocking** `Socket::Receive`, so cancellation requires either a socket timeout, a
non-blocking/`poll`-based read loop, or a shutdown-based interrupt — a transport-level design
decision that touches `modules/net-sockets` behaviour. Blocked.

### 4.6 SR-AUD-252 — `KeepAliveInterval`/`KeepAliveTimeout` are inert (medium) → **#2094, design, blocked**

Both are validated, stored and returned, and nothing reads them. Driving them requires a
**background timer thread** sending Ping frames and tracking Pong deadlines — new concurrency in
a class that today has exactly one mutex and races on `state_` (§7.11). Blocked on the same
concurrency design as #2093.

---

## 5. Structural root-cause families

- **W-A — a borrowed raw pointer crosses an asynchronous boundary.** SR-AUD-247 (`this`) and
  its measured second edge (the caller's `buffer`). **CCF-019.** Blocked.
- **W-B — caller text is concatenated into a protocol frame with no control-character check.**
  SR-AUD-248. **CCF-021 candidate** (see §8).
- **W-C — a validator exists but its rejection set is narrower than the grammar.**
  SR-AUD-249. Same shape as `System::Xml`'s X-C, but here the validator is present and
  incomplete rather than absent.
- **W-D — remote input is parsed without validating the fields the format constrains.**
  §7's eleven post-audit defects. **No `SR-AUD-*` finding names this family at all**, which is
  the single most important thing this review adds.
- **W-E — a public property is stored and never consulted.** SR-AUD-252, plus
  `WebSocketCreationOptions` and `CollectHttpResponseDetails`. Blocked.
- **W-F — a public state field is mutated from task threads with no synchronisation.**
  Post-audit §7.11. Governed by the same concurrency design as #2093/#2094.

---

## 6. Corrected premises

| # | The record said | Measured |
|---|---|---|
| 6.1 | SR-AUD-247: "all client async operations share the capture pattern" | Confirmed, **and wider**: `SendAsync`/`ReceiveAsync` also capture the **caller's buffer by reference** (`[this, &buffer, …]`), a second borrowed object with the same lifetime hazard. |
| 6.2 | SR-AUD-249: "C++ accepts `chat,evil` … despite HTTP-token separators" | The example is right, but a validator **does** exist and already rejects `c <= 0x20` and `0x7F`. The repair widens it to the RFC 7230 separator set; it does not create it. |
| 6.3 | SR-AUD-250 is a WebSockets defect | Its **cause** is in another component: `Win32Exception` has no inner-exception constructor. Fixing it here is impossible; fixing it at all is a public base-class change. |
| 6.4 | The `System::Xml` review: reviewing this unit "produces a review whose highest-value finding is blocked on arrival" | True of SR-AUD-247 — and **not true of the unit**. Eleven post-audit protocol-validation defects (§7) are compatible and independent of the blocked lifetime question. |
| 6.5 | — | The **frame parser is entirely unaudited.** All six findings concern the handshake, the options bag, the exception type and lifetime. **Not one** names `readFrame`, which is the code that touches remote bytes. |
| 6.6 | — | Two real positives already exist and must not regress: a **256 MiB frame-payload cap** and a **16 KiB handshake-response cap**, both added by earlier work with comments explaining why. `validateWebSocketBuffer` likewise already bounds `offset`/`count`. |

---

## 7. Post-audit protocol defects — the remote-input surface, measured by reading `readFrame`

Every item below is reachable by a **server the client connected to**, with no handshake
credential beyond completing the upgrade. None carries an `SR-AUD-*` identifier.

| # | Defect | Consequence | Ticket |
|---|---|---|---|
| 7.1 | **Reserved bits RSV1/2/3 (`header[0] & 0x70`) are never examined.** | A frame with an extension bit set is silently treated as an ordinary frame although no extension was negotiated. RFC 6455 §5.2 requires failing the connection. | **#2090** |
| 7.2 | **The opcode is never validated.** `readFrame` returns any value 0x0–0xF; `ReceiveAsync`'s `switch` routes 0x9/0xA/0x8 and sends **everything else** — including the reserved 0x3–0x7 and 0xB–0xF — to `default:`, where it is delivered as message data. | Reserved opcodes become application payload. RFC 6455 §5.2 requires failing. | **#2090** |
| 7.3 | **A masked frame from the server is accepted and unmasked.** | RFC 6455 §5.1 says a client **MUST** fail the connection on a masked frame. The port instead honours the mask bit. | **#2090** |
| 7.4 | **A fragmented control frame (FIN=0 on 0x8/0x9/0xA) is accepted.** | RFC 6455 §5.5: control frames MUST NOT be fragmented. A fragmented Ping is answered with a Pong. | **#2090** |
| 7.5 | **A control frame with a payload > 125 bytes is accepted**, up to the 256 MiB cap. | RFC 6455 §5.5: control payloads MUST be ≤ 125. A 256 MiB "Ping" is read into memory **and echoed back** as a Pong — an amplification the server controls. | **#2090** |
| 7.6 | **A Close payload of exactly 1 byte is silently ignored** (`if (payload.size() >= 2)`), leaving `closeStatus_` unset and reporting a clean close. | RFC 6455 §5.5.1: a 1-byte close payload is a protocol error. | **#2091** |
| 7.7 | **Any 16-bit close code is accepted**, including 0, 1005, 1006, 1015 and the unassigned ranges, and cast straight to `WebSocketCloseStatus` — a value outside the enum's domain. | The public `getCloseStatusProperty()` can return an enumerator that does not exist. | **#2091** |
| 7.8 | **Text payloads and close reasons are never UTF-8 validated.** | RFC 6455 §8.1 requires failing the connection on invalid UTF-8 in a Text message or a close reason. | **#2091** |
| 7.9 | **`ReceiveAsync` never checks fragmentation ordering**: a continuation frame (0x0) with no message in progress is accepted and reported with the *previous* message's `fragmentType_`; a new data frame arriving mid-fragment is accepted too. | Message boundaries are server-controllable. | deferred, **#2095** |
| 7.10 | **`Sec-WebSocket-Protocol` in the response is not checked against the requested list**, and is accepted even when **none** was requested. | The server chooses the client's `SubProtocol` freely. RFC 6455 §4.1 forbids it. | **#2091** |
| 7.11 | **`state_` is written from task threads and read from `getStateProperty()` with no synchronisation**, and `sendFrame`/`readFrame` dereference `socket_` with no null check — `Dispose()` may have reset it. | A data race on a public property, and a **null dereference** if `Dispose()`/`Abort()` races an in-flight send or receive. | **#2096**, blocked with #2093 |

**Two positives are recorded so they are not lost**: the 256 MiB frame cap and the 16 KiB
handshake cap both already exist, each with a comment explaining the `std::bad_alloc`-vs-
`WebSocketException` reasoning, and `validateWebSocketBuffer` already bounds `offset`/`count`
against `WebSocketValidate.cs`.

**Also measured and deliberately NOT ticketed:** `statusLine.find(" 101 ")` matches the substring
anywhere in the status line rather than parsing the status code. It is weak, but the response is
already bound by a correct `Sec-WebSocket-Accept` digest check three lines later, so it is not
independently exploitable; recorded here rather than ticketed.

---

## 8. CCF-019 mapping, and CCF-021/CCF-022 promotion status

### 8.1 CCF-019 — this is its **third** module, and it is still unselected

| Site | Finding | Ticket | State |
|---|---|---|---|
| `text-json` `JsonNode` | SR-AUD-327 | #1888/#1889/#1894 | blocked, design-complete |
| `xml-linq` borrowed views | SR-AUD-333 | #1899 | blocked, design-complete |
| `threading` (2 members) | SR-AUD-187/221 | #1959 | blocked, design |
| `threading-tasks` | SR-AUD-230 | #1970 | blocked, design |
| `net-http` `HttpClient` | SR-AUD-310 | #2066 | **blocked, two options, NO selection** |
| **`net-websockets`** | **SR-AUD-247** | **#2088** | **blocked, no selection — this review** |

**No local policy is invented here, and that is deliberate.** #2066 is the family's open design
question and it has two competing options with no decision; choosing one inside
`net-websockets` would create a *sixth* answer to a question the family exists to answer once.
#2088 therefore: reproduces nothing new, records the **second borrowed edge** §6.1 found so the
eventual repair covers it, and **pins the ownership model at compile time** the way #2066's pin
does — a `static_assert` that `ClientWebSocket` is **not** `enable_shared_from_this`, so the
day someone changes the ownership model the build says so.

**SR-AUD-247 is NOT marked remediated, and CCF-019 is NOT marked closed.**

### 8.2 CCF-021 — the evidence is now complete, and it is still **not minted here**

The `System::Net::Http` review §18 and the `System::Xml` review §17 both say to mint CCF-021 —
*a control character crossing a public door into a protocol frame* — when `Net.Http.Headers` or
`Net.WebSockets` is reviewed. Measured membership:

| Module | Finding | State |
|---|---|---|
| `net-http` | SR-AUD-313 (+ SR-AUD-316's reason half) | **remediated** by #2063, ten doors, one shared predicate |
| `net-http-headers` | SR-AUD-319, SR-AUD-322 | confirmed, **unreviewed module** |
| `net-websockets` | **SR-AUD-248** | confirmed, **this review**, compatible as #2089 |

That is **three modules and four findings with one structural root cause and one governing
policy** (reject CR/LF/NUL at the door, do not echo the rejected text). The repository's
promotion discipline — applied by the Buffers review §5.3, the `Net.Http` review §18 and the
`Xml` review §17 — is to mint a CCF **from a completed review of the module that supplies the
decisive evidence**, and `net-http-headers` is still unreviewed with **two** of the four
findings. **CCF-021 is therefore NOT minted by this review.** What this review does instead is
record the completed evidence above and require #2089 to **reuse `System::Net::Http`'s existing
predicate shape rather than invent a second one**, which captures the whole practical benefit
of the family without pre-empting the audit process. Mint CCF-021 when `net-http-headers` is
reviewed, citing all four.

### 8.3 CCF-022 — unchanged, still not minted

X-D (*a public lifecycle state recorded but not enforced*) now has two **remediated** members in
`System::Xml` (#2076, #2078) and three open in `modules/io`. `net-websockets` adds a
**candidate** — §7.11's disposed-socket race — but that is a *concurrency* defect, not a
recorded-but-unenforced state, so it is **not** counted as a member. The `Xml` review §17 rule
stands: mint CCF-022 when `modules/io` is reviewed, citing all five.

**No CCF is minted by this review.**

---

## 9. Dependency graph of the tickets

```
#2089  handshake header + subprotocol validation   (P1) ── independent; reuses Net.Http's predicate shape
#2090  frame header validation                      (P1) ── independent of #2089; same file
#2091  close/UTF-8/subprotocol-response validation   (P2) ── AFTER #2090: it validates what #2090 lets through
#2092  WebSocketException inner exception            (P2) ── BLOCKED: needs a Win32Exception/Exception base change
#2088  CCF-019 lifetime                              (P1) ── BLOCKED on #2066's unselected family design
#2093  cancellation                                  (P2) ── BLOCKED: needs a non-blocking transport design
#2094  keep-alive ping/pong                          (P2) ── BLOCKED: depends on #2093's concurrency design
#2096  state_ race + disposed-socket null deref      (P2) ── BLOCKED with #2093/#2094 (same concurrency design)
#2095  fragmentation ordering                             ── deferred verification
```

---

## 10. Compatible versus blocked

| Ticket | Compatible? | Why |
|---|---|---|
| #2089 | **yes, with a documented narrowing** | rejects CR/LF/NUL in header name/value and widens the existing subprotocol validator; no type or signature change |
| #2090 | **yes, with a documented narrowing** | rejects malformed frames the RFC already forbids; changes no signature |
| #2091 | **yes, with a documented narrowing** | rejects invalid close payloads/codes, invalid UTF-8 and an unrequested subprotocol |
| #2088 | **no** — CCF-019, unselected family policy |
| #2092 | **no** — public base-class change in another component |
| #2093, #2094, #2096 | **no** — new concurrency design; #2096 additionally changes when a disposed socket throws |
| #2095 | **no** — deferred verification |

**No compatible ticket in this namespace requires an object-layout, vtable, base-class or
public-type change.**

---

## 11. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` |
|---|---|---|---|---|
| #2089 | narrows two public option doors | none | none | none |
| #2090 | narrows accepted **remote** input | none | none | none |
| #2091 | narrows accepted **remote** input | none | none | none |

`ClientWebSocket`'s layout must be unchanged and is to be pinned by the first implementation
ticket, using the **probe-struct** technique (`docs/SystemXmlNamespaceReviewPlan.md` §10 and
`XmlContractPinTests`) — **not** literal byte counts.

---

## 12. Observable semantic consequences

- **#2089** — `SetRequestHeader` and `AddSubProtocol` throw for text that would corrupt the
  handshake. A caller who was smuggling a header stops.
- **#2090/#2091** — a **server** that sends a malformed frame now gets a `WebSocketException`
  instead of having its bytes delivered as application data. Any client that was tolerating a
  non-conforming server will start seeing exceptions; that is the point.
- **#2091** — `getCloseStatusProperty()` stops returning values outside the enum's domain.

A migration note (`docs/Migration-WebSocketProtocolStrictness.md`) covers #2089–#2091 together
when the first of them lands.

---

## 13. Test matrix

| Ticket | Required cases |
|---|---|
| **#2089** | `\r`, `\n`, `\r\n`, NUL in a header **name** and in a **value**; a header value containing a lone `\r`; every RFC 7230 separator in a subprotocol; a valid subprotocol with `-`, `.`, `_`, `+`, digits (accepted); the existing empty/space/DEL rejections unchanged; the emitted request is byte-identical for accepted input |
| **#2090** | each RSV bit alone and all three together; every reserved opcode (0x3–0x7, 0xB–0xF); a masked server frame; FIN=0 on each of 0x8/0x9/0xA; a control payload of 125 (accepted) and 126 (rejected); payload lengths 0, 125, 126, 127, 65535, 65536; truncated 16-bit and 64-bit length fields; **EOF at every byte offset of the header**; a valid frame still round-trips |
| **#2091** | close payload length 0 (accepted, no status), 1 (rejected), 2 (accepted), 2+reason; close codes 1000/1001/1011 (accepted) and 0/999/1004/1005/1006/1015/5000 (rejected); invalid UTF-8 in a Text payload and in a close reason; a valid multi-byte UTF-8 payload and a valid supplementary scalar (accepted); a lone surrogate encoding (rejected); a `Sec-WebSocket-Protocol` response that was not requested, and one that was |
| **pins** | `ClientWebSocket`'s layout probe struct; the 256 MiB frame cap; the 16 KiB handshake cap; `validateWebSocketBuffer`'s bounds; #2088's ownership `static_assert`; #2095's measured fragmentation behaviour |

All frame-level cases are driven by the **real loopback server harness** the existing
`ClientWebSocketTests` already builds (`Socket` listener + `std::thread`), so every assertion
goes over a real socket. **Deterministic synchronisation only — no sleeps.**

---

## 14. Ticket split

### 14.1 Compatible, ready

| # | P | Size | Scope | Findings | Family |
|---|---|---|---|---|---|
| **#2089** | P1 | S | reject CR/LF/NUL at the handshake header doors; widen the subprotocol validator to the RFC 7230 separator set | SR-AUD-248, SR-AUD-249 | W-B, W-C |
| **#2090** | P1 | M | validate the frame header: reserved bits, opcode domain, server masking, control-frame fragmentation and payload size | post-audit §7.1–7.5 | W-D |
| **#2091** | P2 | M | validate close payload length and code domain, UTF-8 in Text and close reasons, and the negotiated subprotocol response | post-audit §7.6–7.8, §7.10 | W-D |

### 14.2 Blocked on approval or on another design

| # | P | Why |
|---|---|---|
| **#2088** | P1 | CCF-019; #2066's family design has two options and no selection |
| **#2092** | P2 | needs an inner-exception constructor on `Win32Exception`/`Exception` — another component, public base-class change |
| **#2093** | P2 | cancellation needs a non-blocking or interruptible transport design |
| **#2094** | P2 | keep-alive needs a background timer thread; depends on #2093 |
| **#2096** | P2 | `state_` race and disposed-socket null dereference; same concurrency design |

### 14.3 Deferred verification

| # | P | Question |
|---|---|---|
| **#2095** | P3 | exactly which fragmentation-ordering violations does .NET reject, and with what error? |

---

## 15. Recommended implementation order

1. **#2090** — the frame header is the first thing remote bytes touch, the defects are
   RFC-decidable without the absent reference, and it is independent of everything else.
2. **#2089** — reuses `System::Net::Http`'s established predicate shape; small and bounded.
3. **#2091** — after #2090, because it validates payloads #2090 has already let through.
4. Stop. Everything else is blocked or deferred.

---

## 16. Deferred evidence

`/rv/tmp/runtime/src/libraries/` is absent. The following are **not** decided by this review:

- the exact exception type and message .NET raises for each protocol violation — this port uses
  `WebSocketException` with the closest `WebSocketError` and **records that as its choice**;
- .NET's exact fragmentation-ordering rejections (#2095);
- whether .NET's `ClientWebSocket` rejects an unrequested `Sec-WebSocket-Protocol` response or
  merely ignores it — RFC 6455 §4.1 forbids the server from sending it, so #2091 rejects and
  records the choice;
- **CCF-019's repair.** Selecting one here would be a guess against an open family design, not
  against an absent reference — a different and worse failure mode.

---

## 17. Recommended next unit

**`modules/io`** — 11 open findings, zero highs, and it is the module that unlocks **CCF-022**
(X-D's three remaining members, SR-AUD-337/343/344, alongside `System::Xml`'s two now-remediated
ones). `modules/net-http-headers` has only 5 open findings — below the ≥6 threshold — but is the
module that unlocks **CCF-021** with all four members present; §8.2 records that as the trigger.

---

## 18. Exclusions

- WebSocket **server** support, TLS/`wss://`, and `permessage-deflate` — absent by design.
- `modules/net-sockets` — measured, never edited.
- CNA and mobile-eggbert — not inspected; #1773 stays blocked.

---

## 19. Completion criteria

This review (#2087) is complete when this document exists, each of the six open findings has
exactly one disposition in §4, §7's post-audit defects each carry a ticket, and §14's tickets
are in `plan.sqlite3`. **It is complete on those terms and remediates nothing by itself.**

`System::Net::WebSockets` is closed for *compatible* work when #2089, #2090 and #2091 are
`done`; SR-AUD-248 and SR-AUD-249 are `remediated`; SR-AUD-247, 250, 251 and 252 each carry a
blocked ticket and a pin; and #2095's measured behaviour is pinned.

---

## 20. Implementation record

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.

### 20.1 #2090 landed as §7.1–§7.5 specified, and the sharpest detail is where the size check sits

All five checks run **before** any dependent bytes are read. The control-payload limit is
tested on the **7-bit length field**, so a `len` of 126 or 127 on a control frame is rejected
**without reading the extended-length bytes and without allocating anything** — which is what
turns §7.5 from "a 256 MiB Ping is read into memory and echoed back as a Pong" into a two-byte
rejection.

The mask-key read and the unmasking loop were **deleted**, not left as dead code: a masked
frame can no longer reach them, and leaving an unreachable unmasking path would invite exactly
the "it handles masking, so masking must be allowed" misreading the defect came from.

**+15 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Net_WebSockets` 24 → 39. Every case runs over a **real loopback socket**
against a mock server thread, with blocking accept/read as the only synchronisation — **no
sleeps**.

**Five mutations**, each reverted from an exact backup: W1 (reserved bits) → 2 tests; W2
(opcode domain) → 1; W3 (masked server frame) → 1; W4 (fragmented control frame) → 1; W5
(oversized control payload) → 1.

**One non-discriminating test was found and fixed rather than reported as a pass.** W5
originally failed to discriminate: the oversized-control case sent only the two header bytes,
so with the guard removed the parser read past the end, hit EOF, and threw
`ConnectionClosedPrematurely` — also a `WebSocketException`. **A truncated frame cannot
distinguish "rejected for being an oversized control frame" from "rejected for ending early."**
The test now sends **complete** 200-byte control frames in both the 16-bit and 64-bit length
forms. This is the fourth such correction in this batch (`SystemXmlNamespaceReviewPlan.md`
§20.4, §20.6, §20.7): a mutation that fails to fail is a statement about the test.

**Pins landed with it**, as §11 and #2088 require:

- `ClientWebSocket`'s layout, via a **probe struct** — never a literal byte count, because its
  members include `std::string`, `std::vector`, `std::optional` and `std::mutex` whose sizes
  differ between libstdc++ and libc++ and the MinGW/Emscripten/Apple-Clang builds must keep
  compiling;
- **#2088's CCF-019 ownership pin**: a `static_assert` that `ClientWebSocket` is **not**
  `enable_shared_from_this`, the same shape as #2066's, because a use-after-free is not a
  behaviour that can be pinned behaviourally;
- the **16 KiB handshake-response cap**, which §6.6 recorded as an existing positive and which
  nothing covered.

**Recorded as this port's choices** (§16): the exception identity — `WebSocketException` with
the closest `WebSocketError` — and the decision to reject rather than ignore. Every *rule* is
RFC 6455 cited as a **protocol** fact, not as .NET behaviour.
