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

---

## 11.1 #2096 LANDED 2026-08-17 — and the defect was worse than §7.11 recorded

#2096 landed under `docs/StandingApprovals.md` SA-3 (the `sizeof` change) with the reference
tree available. Three things are worth carrying forward.

**The race was four members wide, not one.** §7.11 names `state_`. Measured, `closeStatus_`,
`closeStatusDescription_` and `subProtocol_` have the identical shape — written by a task thread,
read by a public getter, unsynchronised. All four are now guarded by one `stateMutex_`. .NET
makes its own state atomic for the same reason (`ClientWebSocket.cs:104,132`).

**`Dispose()` did not merely risk a null dereference — it hung.** Running the new
`ClientWebSocketConcurrencyTests` against the pre-#2096 production code under ASan, **all five
hang**. `Close()` does not reliably wake a thread already parked in `recv()` on Linux, so the old
code freed the `Socket` object under the worker *and* left the worker parked forever. The repair
is shared ownership plus `Shutdown()`: the member is taken away under the lock so no new
operation can start, the socket is shut down to wake any worker, and only then is `Dispose()`'s
own reference dropped. It deliberately no longer calls `Close()` — closing a descriptor another
thread is blocked on is the hazard, not the fix.

**#2088's boundary covered two of five.** `ConnectAsync`, `CloseAsync` and `CloseOutputAsync`
never called `beginAsyncOperation()`, so they captured raw `this` with exactly the lifetime
defect #2088 was written to remove. All five now join it; ASan reports `heap-use-after-free` the
moment `ConnectAsync` is taken back out.

`sizeof(ClientWebSocket)` 360 → 408, `alignof` 8 unchanged, pinned by the layout probe in
`ClientWebSocketFrameValidationTests.cpp`. Record:
`docs/Migration-ClientWebSocketConcurrencySafety.md`. The exception-type question the racy-loss
path raised is ticket **#2357**; #2093 (cancellation) and #2094 (keep-alive) remain open, and
this ticket deliberately does not pre-empt their design.


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

### 20.2 #2089 landed with a THIRD door the finding does not name

§4.2 named `SetRequestHeader`, and the ticket named it too. Measured against `a4698e6`, the
**request URI is a third vector**, and closing only the options doors would have left request
smuggling open:

    "GET " + uri.getPathAndQueryProperty() + " HTTP/1.1\r\n"
    "Host: " + uri.getHostProperty() + ":" + port + "\r\n"

`System::Uri` preserves CR, LF and NUL in both components
(`build-probe/2089_probe2_uri_door.log`), so `ws://127.0.0.1:P/a\r\nX-Injected:+yes` put
`GET /a` on the request line and `X-Injected: yes HTTP/1.1` into a header field. **This is the
same scope correction #2063 made for `System::Net::Http`**, where the request URI likewise turned
out to be a door the review's paraphrase had dropped — the second time in two namespaces that a
control-character finding named the header door and missed the URI. `System::Uri` itself is
**not** modified; the Uri-side defect stays the separate, blocked ticket #2003.

Measured before the repair (`build-probe/2089_probe1_before.log`): a six-field request became an
**eight-field** one carrying a smuggled `GET /admin HTTP/1.1`, and **not one** of the seventeen
RFC 7230 separators was rejected in a subprotocol. §6.2's correction held exactly — a validator
did exist and already rejected `<= 0x20` and `0x7F`, so this **widened** it.

**The predicate has exactly one body in the repository, and §8.2's constraint is met without
minting CCF-021.** Rather than either duplicating #2063's predicate or making `Net.WebSockets`
depend on the whole `Net.Http` component for a three-character check, the body moved down to
`System::Net::detail::ContainsProtocolFieldTerminator` in the `Net` component **both** protocol
modules already depend on, and `System::Net::Http::detail::ContainsProtocolControlCharacter` is
kept as a forwarder under its original name. **No `System::Net::Http` call site, exception type or
message changed** (181/181 `SharpRuntimeTests_Net_Http` green), and the module graph is unchanged
at **41 modules and 91 edges** — `Net.WebSockets`' existing `Net` edge only moved `PRIVATE` →
`PUBLIC`, which is the accurate declaration now that a public header uses it, and costs consumers
nothing because `Net` already reached them transitively through `Net.Sockets`.

**Exception identity, recorded as this port's choice** (§16): `System::ArgumentException`, not the
`System::FormatException` `System::Net::Http` uses, because within `ClientWebSocketOptions` the
sibling character check (`AddSubProtocol`) **already** reported invalid characters as an argument
error. #2063 resolved the same tension the same way for its multipart doors. The rejected text is
never echoed into the message.

**+16 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Net_WebSockets` 39 → 55. The wire-level cases run over a **real loopback
socket** against a mock server thread, synchronised only by blocking accept/read — **no sleeps**.

**Three mutations**, each reverted from an exact backup with `git diff --stat` identical on both
sides and no marker surviving: M1 (`validateHeaderField` neutered) → exactly the 5 header-door
tests, 50 green; M2 (`isSeparator` returns false) → exactly 2; M3 (URI check removed) → exactly 3,
including the descriptor-count test. No unrelated test moved in any of the three.

**Rejection precedes any byte on the wire**, and the instrument is a **descriptor count**, not
LSan: LSan tracks memory, not descriptors, and would have said nothing. Over 20 rejected
connections the `/proc/self/fd` delta is **0** and all 20 threw — the "all 20 threw" half is what
stops a silently-skipped loop from reading as a pass. SKIPPED where `/proc/self/fd` is absent,
because a missing instrument is not a passing measurement.

**Sanitizers**: ASan/UBSan/LSan clean over **52 rejections and 19 acceptances** with
`ClientWebSocket.cpp` compiled **from source** into the probe — `Net.WebSockets` is a STATIC
archive, so linking it would have measured an uninstrumented `performHandshake` — plus a control
heap-buffer-overflow proving the instrumentation was live (`build-probe/2089_probe3_asan.log`).

No signature, member, base-class, virtual, vtable, object-layout or exception-specification
change. Migration note: `docs/Migration-WebSocketProtocolStrictness.md` (which §12 required when
the first of #2089–#2091 landed, and which #2090 had not yet created).

### 20.3 #2091 landed, and its sharpest result is a mutation that FAILED to fail

All four defects reproduced against `0527b35` before the repair
(`build-probe/2091_probe1_before.log`) and are closed after
(`build-probe/2091_probe1_after.log`): a 1-byte close payload accepted and reported as a **clean
close**; **every one** of 0, 999, 1004, 1005, 1006, 1015 and 5000 reaching the public
`getCloseStatusProperty()`; invalid UTF-8 accepted in both a close reason and a Text payload; and
a `Sec-WebSocket-Protocol` response accepted both when it did not match the request and when
**nothing had been requested at all**.

**Two close parsers existed, not one.** §7.6 describes the `payload.size() >= 2` guard as if it
were one site. Measured, `ReceiveAsync`'s `case 0x8` and `CloseAsync`'s close-handshake loop each
had their **own copy** of the same unvalidated block. Repairing only the receive path — which is
the one §7.6 and §7.7 describe — would have left a malformed close frame accepted during the
close handshake. Both now call one `parseClosePayload`, and mutation N1b proves both are
exercised: it fails `AOneBytePayloadIsRejected` **and**
`TheCloseHandshakeLoopValidatesTheSamePayload`.

**A mutation that failed to fail, found and corrected rather than reported as a pass.** The first
form of N1 simply deleted the `payload.size() == 1` guard. Every test still passed — but for the
wrong reason: with the guard gone, `payload[1]` is an **out-of-bounds read** on a one-element
vector, and the garbage code it produced happened to be invalid, so the *code-domain* check threw
and the test saw its expected exception. A mutation that introduces undefined behaviour cannot
discriminate the guard it was aimed at. N1b instead restores the **original** defect —
`if (payload.size() < 2) return;`, the pre-#2091 `>= 2` guard — and fails exactly the two tests
above. This is the fifth such correction in the programme (`SystemXmlNamespaceReviewPlan.md`
§20.4, §20.6, §20.7 and this plan's §20.1) and the first where the false pass came from **UB in
the mutant** rather than from a truncated fixture.

**The close-code domain, and what is deliberately NOT repaired.** Accepted: 1000–1003, 1007–1014,
3000–4999. Rejected: 0–999, 1004, 1005, 1006, 1015, 1016–2999, 5000+. §7.7's framing —
"`getCloseStatusProperty()` can return an enumerator that does not exist" — is **half right and
worth correcting**: codes in 3000–4999 are perfectly legal on the wire and have **no enumerator**
in `WebSocketCloseStatus`, in this port *or* in .NET, whose enum names the same subset. Returning
an unnamed enumerator for those is inherent to the enum's design, not a defect. What #2091 closes
is codes that can **never** be valid reaching the getter. Accepting 1012–1014 is **this port's
choice**, taken because they are registered under the procedure RFC 6455 §7.4.2 delegates and
rejecting them would reject a conforming server; 1005 is rejected on the wire even though it is
this port's `Empty` enumerator, because a local enumerator does not make a value legal in a frame.

**The UTF-8 limit is stated, not hidden.** A close reason is validated completely (control frames
cannot be fragmented, which #2090 now enforces). A Text message is validated when it arrives as
**one complete frame**. A **fragmented** Text message is deliberately not validated: a scalar may
legally straddle a fragment boundary, so per-frame validation would reject conforming input, and
doing it correctly needs incremental decoder state on the object — an **object-layout change**
this compatible ticket does not make, and one the layout `static_assert` from #2090 would reject.
The gap is **pinned by a test** so it cannot change silently. The validator is transcribed from
this repository's own already-correct `SslApplicationProtocol::isValidUtf8` rather than invented;
it is copied rather than shared because that one is a *private member* of an unrelated class in a
different INTERFACE component, and the repository already holds several independent UTF-8
decoders (`UTF8Encoding`, `Utf8JsonWriter`, `BinaryReader`, `IdnMapping`) — consolidating them is
a real but separate concern, recorded here and not done under cover of this ticket.

**Exception identity, recorded as this port's choice** (§16): `WebSocketError::Faulted` for a
**payload** violation, deliberately distinct from #2090's `HeaderError` for a **header**
violation so a caller can tell them apart, and `UnsupportedProtocol` for the subprotocol
response. Rejecting an unrequested subprotocol rather than ignoring it is likewise this port's
choice, taken from RFC 6455 §4.1; the server-supplied value is not echoed into the message.

**+23 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Net_WebSockets` 55 → **78**. Every wire case runs over a real loopback socket
with blocking accept/read as the only synchronisation — no sleeps.

**Four mutations**, each reverted from an exact backup with `git diff --stat` identical on both
sides and no marker surviving: N1b (original close-length guard) → 2; N2 (close-code domain
always valid) → 3; N3 (UTF-8 validator always true) → 4; N4 (subprotocol response accepted
unconditionally) → 2. No unrelated test moved in any of the four.

**Sanitizers**: ASan/UBSan/LSan clean over the whole defect matrix with `ClientWebSocket.cpp`
compiled **from source** into the probe — verified by `nm`, which shows `performHandshake` and
`readFrame` as text symbols in the probe binary rather than resolved from the STATIC archive
(`build-probe/2091_probe1_asan.log`). Instrumentation liveness was demonstrated by the control
heap-buffer-overflow built with the identical script and flags in §20.2.

**Interaction with #2089 and #2090 is asserted, not assumed.** #2091 edits `performHandshake`, so
#2089's URI door is re-asserted; it edits `readFrame`'s caller, so #2090's oversized-control-frame
rejection is re-asserted. Both are permanent tests in the #2091 suite.

No signature, member, base-class, virtual, vtable, object-layout or exception-specification
change. Migration note: `docs/Migration-WebSocketProtocolStrictness.md` §4.

**Recorded, not changed:** `sendCloseFrame` does **not** validate the code or reason a *caller*
supplies, so an application can still send a close frame this client would reject on receipt.
That is a caller-side door outside this ticket's acceptance criteria and outside §7's remote-input
subject; it is recorded here rather than repaired under cover of a received-payload ticket.

---

## 21. Namespace reconciliation — measured after #2089, #2090 and #2091

Every finding and post-audit defect in this namespace, with exactly one disposition. Nothing is
unaccounted for.

### 21.1 The six `SR-AUD-*` findings

| Finding | Severity | Disposition | Ticket | Pin |
|---|---|---|---|---|
| SR-AUD-247 | high | **confirmed** — CCF-019, sixth site, family design unselected | #2088, blocked | `static_assert` on the ownership model |
| SR-AUD-248 | high | **REMEDIATED** (#2089) | done | 16 permanent tests |
| SR-AUD-249 | medium | **REMEDIATED** (#2089) | done | 16 permanent tests |
| SR-AUD-250 | medium | **confirmed** — cause is in another component | #2092, blocked | `Pin2092_…` |
| SR-AUD-251 | medium | **confirmed** — needs an interruptible transport design | #2093, blocked | `Pin2093_…` |
| SR-AUD-252 | medium | **confirmed** — needs a background timer thread | #2094, blocked | `Pin2094_…` |

### 21.2 The eleven post-audit protocol defects (§7)

| § | Defect | Disposition |
|---|---|---|
| 7.1 | reserved bits never examined | **fixed**, #2090 |
| 7.2 | opcode never validated | **fixed**, #2090 |
| 7.3 | masked server frame accepted | **fixed**, #2090 |
| 7.4 | fragmented control frame accepted | **fixed**, #2090 |
| 7.5 | oversized control payload accepted and echoed | **fixed**, #2090 |
| 7.6 | 1-byte close payload silently ignored | **fixed**, #2091 |
| 7.7 | any 16-bit close code accepted | **fixed**, #2091 (premise corrected — see §20.3) |
| 7.8 | Text and close reasons never UTF-8 validated | **fixed**, #2091, with a **stated and pinned limit** for fragmented Text |
| 7.9 | fragmentation ordering never checked | **deferred**, #2095, behaviour **pinned** in two tests |
| 7.10 | unrequested subprotocol response accepted | **fixed**, #2091 |
| 7.11 | `state_` race and disposed-socket null dereference | **blocked**, #2096, deliberately **not** behaviourally pinned (it is UB, not behaviour) |

### 21.3 Doors and sites found during implementation that the review did not name

| Found by | What the record missed |
|---|---|
| #2089 | the **request URI** is a third injection door — the request line and `Host:` are built from it, and `System::Uri` preserves CR/LF/NUL. The same door #2063 found missing from SR-AUD-313's paraphrase. |
| #2091 | there were **two** close parsers, not one — `ReceiveAsync`'s and `CloseAsync`'s, each with its own copy of the unvalidated block. |
| #2091 | `fragmentType_`'s member initialiser is **Binary**, so a bare continuation frame is reported as Binary. A pin written against the assumed default of Text failed, which is what pins are for. |
| #2091 | `WebSocketError` has **no** `InvalidPayloadData` member — that is a `WebSocketCloseStatus` value. Payload violations use `Faulted`, deliberately distinct from #2090's `HeaderError`. |

### 21.4 Recorded, not ticketed

- `sendCloseFrame` does not validate a **caller**-supplied close code or reason, so an
  application can still send a close frame this client would reject on receipt. A caller-side
  door, outside §7's remote-input subject.
- `statusLine.find(" 101 ")` matches a substring rather than parsing the status code (§7,
  already recorded) — bounded by the `Sec-WebSocket-Accept` digest check three lines later.
- The repository holds **several independent UTF-8 decoders** (`UTF8Encoding`, `Utf8JsonWriter`,
  `BinaryReader`, `IdnMapping`, `SslApplicationProtocol`, and now this module's). #2091's
  validator is **transcribed** from the existing correct one rather than invented. Consolidating
  them is a real architectural concern and is deliberately not done under cover of a
  payload-validation ticket.

### 21.5 Status

**`System::Net::WebSockets` is closed for compatible work.** Every §19 criterion is met: #2089,
#2090 and #2091 are `done`; SR-AUD-248 and SR-AUD-249 are `remediated`; SR-AUD-247, 250, 251 and
252 each carry a blocked ticket **and** a pin; and #2095's measured behaviour is pinned in two
tests.

**No compatible, ready ticket remains in this namespace.** What remains is #2088, #2092, #2093,
#2094 and #2096 — all blocked on approvals or on designs that belong to other components — and
#2095, deferred pending reference evidence that does not exist in this container.

**CCF-019 remains open. CCF-021 is not minted** (`net-http-headers` holds two of five findings
and is unreviewed). **CCF-022 is not minted** (its trigger is the `modules/io` review).
`SharpRuntimeTests_Net_WebSockets`: 24 → **83**.
