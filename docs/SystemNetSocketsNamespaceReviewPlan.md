<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Net::Sockets` (`modules/net-sockets`) namespace review — ticket #2133

Owning ticket **#2133**. This document is the durable record; it **remediates nothing by itself**.
Every claim is measured against the tree at `76ba79b`.

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-08. Every statement about .NET
comes from repository-contained evidence only: the per-file audit reports, doc-comments transcribed
from .NET when the module was written, this module's own tests, and POSIX/RFC grammar this
repository already encodes. Where a repair would need .NET's exact behaviour and no repository
evidence pins it, a **deferred-verification ticket** is created instead of a guess.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit defects
carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket **#1773 stays blocked**.

Primary evidence: `build-probe/2133_probe1_sockets.cpp`, log `build-probe/2133_probe1_before.log`.

---

## 1. Why this unit was selected — re-derived by measurement at this tip

Re-parsed from `audit/AUDIT_FINDINGS_INDEX.md` **after** #2124–#2127/#2129 moved SR-AUD-319/320/321/323
to `remediated`. Every unit with ≥4 open findings:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review | `/rv`-dependent? |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| `modules/core` | 72 | 9 | 59 | 4 | 12% | 1 | 47 | family plans only | mixed |
| `modules/threading` | 17 | 6 | 11 | 0 | 35% | 0 | 21 | **yes** | — |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7% | 12 | 8 | **yes** | — |
| `modules/text` | 11 | 1 | 10 | 0 | 9% | 11 | 3 | **yes** | — |
| `modules/uri` | 10 | 0 | 10 | 0 | 0% | 10 | 4 | **yes** | — |
| `modules/time-zone` | 7 | 0 | 7 | 0 | **0%** | 0 | 0 | none | **7 of 7** |
| `modules/io` | 7 | 0 | 7 | 0 | 0% | 0 | 6 | **yes** | — |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none | **5 of 7** |
| `modules/net-http` | 6 | 1 | 5 | 0 | 16% | 2 | 3 | **yes** (closed) | — |
| **`modules/net-sockets`** | **5** | **1** | **4** | **0** | **20%** | **0** | **0** | **none** | **0 of 5** |
| `modules/net` | 5 | 1 | 4 | 0 | 20% | 5 | 5 | **yes** | — |
| `modules/buffers` | 5 | 1 | 4 | 0 | 20% | 3 | 13 | **yes** | — |
| `modules/numerics` | 4 | 0 | 4 | 0 | 0% | 0 | 0 | none | 2 of 4 |
| `modules/xml-linq` | 4 | 1 | 3 | 0 | 25% | 1 | 0 | none | 1 of 4 |
| `modules/text-json` | 4 | 1 | 3 | 0 | 25% | 1 | 3 | **yes** (closed) | — |

**Unreviewed coherent units with ≥4 open:** `time-zone` (7), `globalization` (7), `net-sockets` (5),
`numerics` (4), `xml-linq` (4). (`core` at 72 is not a coherent unit — 47 files across a dozen
subsystems, already carrying family plans; excluded on that basis by every previous review and here
for the same reason.)

**Selected: `modules/net-sockets`.** The case, in the order it actually decided:

1. **Decidability with `/rv` absent — decisive, and it is the criterion the batch instruction says
   to weight.** **Zero of five** net-sockets findings ask "what exactly does .NET do". All five are
   argument-domain, address-family and object-lifetime defects settled by POSIX, by this module's
   own contracts, and by direct measurement. `time-zone` is the opposite extreme — **seven of
   seven** are custom-zone equality / `BaseUtcOffset` / `HasSameRules` questions that yield a queue
   of deferred-verification tickets and no compatible work. `globalization` is **five of seven**
   (grapheme clusters, collation, culture casing, IDN unassigned code points).
2. **Highest actionable high-severity ratio among unreviewed units — 20%.** `xml-linq` shows 25%,
   but its only `high` (SR-AUD-333) is already `confirmed (design-complete)` behind ticket #1899,
   whose layout approval was **declined**; its *actionable* high count is zero. `globalization` is
   14% and `time-zone` and `numerics` are **0%**.
3. **Defect class, and a dimension no other candidate has.** These are **descriptor-owning** types.
   This is the only remaining unreviewed unit where `/proc/self/fd` accounting is a real instrument
   rather than a formality, and this batch's own testing policy names direct descriptor accounting
   as a required tool that LSan must not be substituted for.
4. **Dependency readiness — it is the last unreviewed member of its family.** `Net` (#2035–#2047),
   `Net.Http` (#2062–#2072), `Net.WebSockets` (#2087–#2096) and `Net.Http.Headers` (#2122–#2132) are
   all reviewed and closed for compatible work. `Net.Sockets` is what remains.
5. **No existing review, and zero of five findings remediated** — the module has never been touched.
6. **Module cohesion.** One CMake component, one namespace, one directory: 19 headers (1,150 lines),
   5 sources (1,559 lines), 3 test files (942 lines).

**The honest cost, stated up front:** its one `high` (SR-AUD-263) is a **CCF-019** shape and will be
`blocked`, so this review's compatible queue is four mediums, not five findings. `numerics`'
SR-AUD-278 (public generic-math members declared and never defined) is a genuinely attractive
alternative — a link-time defect provable by a consumer fixture — but the unit has **no** `high` and
half its findings are reference-dependent.

---

## 2. Scope and file inventory

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 19 | 1,150 |
| implementation | 5 | 1,559 |
| tests | 3 | 942 |

One component: `Net.Sockets`, `TYPE STATIC`,
`PUBLIC_DEPENDENCIES Core.Base IO Net Threading.Tasks`.

**In scope:** everything under `modules/net-sockets/`.

**Out of scope, and why:**

- `modules/net`'s `IPAddress`/`IPEndPoint`/`Dns` — reviewed (#2035–#2047); consumed, never modified.
  §4.4 records where a `net-sockets` defect *ends* and a `net` one begins.
- `System::Net::NetworkInformation` — a different component; `Ping` is #1962, **blocked**.
- TLS (`SslStream`) — a **permanent deviation** by the 2026-07-07 decision in `CLAUDE.md`; there is
  no subject.
- `SocketAsyncEventArgs` and the `Begin*`/`End*` APM surface — **absent from this port**; recorded
  rather than invented.
- Windows and Emscripten runtime behaviour — this module is POSIX-and-Winsock at compile time but
  the verified baseline is Linux/GCC (`CLAUDE.md` platform policy). Every measurement here is Linux.

---

## 3. Complete public-surface inventory

| Area | Types |
|---|---|
| Core socket | `Socket` (243 header lines: 4 async members, `Close`/`Dispose`, move-assign, deleted copy) |
| Stream adapter | `NetworkStream` (owns a raw `int fd_`) |
| Clients | `TcpClient`, `TcpListener` (in `TcpClient.hpp`), `UdpClient` |
| Endpoints | `UnixDomainSocketEndPoint` |
| Options / results | `LingerOption`, `MulticastOption`, `SendPacketsElement`, `IPPacketInformation`, `SocketReceiveFromResult`, `SocketReceiveMessageFromResult`, `UdpReceiveResult` |
| Enums | `ProtocolType`, `SelectMode`, `SocketFlags`, `SocketOptionLevel`, `SocketOptionName`, `SocketShutdown`, `SocketType` |

**The structural fact that shapes four of the five findings:** every descriptor-owning type in this
module holds a **raw `int` file descriptor** and validates **nothing** about it or about the
arguments that produce it. `NetworkStream` takes any `int`; `UdpClient` takes any `int` port and
truncates it with `htons(static_cast<uint16_t>(port))`; `TcpClient`'s local-endpoint constructor
takes an `IPEndPoint` and has an **empty body with the parameter name commented out**; and every
connect/bind path in `TcpClient` and `UdpClient` is `AF_INET` + `sockaddr_in`, unconditionally.

---

## 4. Every open finding, with its measured disposition

All five reproduced against `76ba79b`.

| Finding | Sev | Measured | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-263 | **high** | **confirmed** — §4.1 | **blocked** (CCF-019, ownership) | **#2134** |
| SR-AUD-264 | med | **confirmed, and narrower than it reads** — §4.2 | compatible | **#2135** |
| SR-AUD-265 | med | **confirmed and wider** — §4.3 | compatible | **#2136** |
| SR-AUD-266 | med | **confirmed exactly as filed** — §4.4 | split: compatible + design | **#2137**, **#2138** |
| SR-AUD-267 | med | **confirmed** — §4.5 | compatible | **#2137** |

### 4.1 SR-AUD-263 — confirmed; all four async members capture a raw `this`

Measured by reading `Socket.cpp:808–836`. Every one of the four returns a `TaskT<T>` built from a
lambda capturing **`this`**, and the header's own comment at line 212 says `TaskT` dispatches with
`std::async(std::launch::async, …)` **immediately, not deferred**:

```cpp
TaskT<bool>                  ConnectAsync(...)  { return TaskT<bool>([this, copy]{ ... }); }
TaskT<std::shared_ptr<Socket>> AcceptAsync()    { return TaskT<...>([this]{ return Accept(); }); }
TaskT<intcs>                 SendAsync(...)     { return TaskT<intcs>([this, buffer = ..., flags]{ ... }); }
TaskT<intcs>                 ReceiveAsync(...)  { return TaskT<intcs>([this, buffer, flags]{ ... }); }
```

`Socket` is move-assignable and destructible while a worker runs, and there is no join, no
`shared_from_this`, and no liveness boundary of any kind. **This is CCF-019** — the identical shape
as `text-json` SR-AUD-327, `xml-linq` SR-AUD-333, `threading` SR-AUD-187/221, `threading-tasks`
SR-AUD-230, `net-http` SR-AUD-310 and `net-websockets` SR-AUD-247.

**No use-after-free was executed to confirm it**, deliberately. A racing ASan reproduction of a
lifetime bug is flaky by construction, and #2096 already recorded the reason a data race gets a
comment rather than a test. The shape is read off the source, which is sufficient and honest.

**Disposition: blocked.** Every repair changes public ownership (`shared_ptr<Socket>` /
`enable_shared_from_this`, or a joining destructor that changes `~Socket`'s exception behaviour).
CCF-019 is **not approved** and is **not marked closed**. Ticket **#2134**, `blocked`.

### 4.2 SR-AUD-264 — confirmed, and the defect is one overload, not the type

| Input | Measured |
|---|---|
| `SendPacketsElement({1,2,3}, 0, -1)` | **accepted**, `count == 3` |
| `SendPacketsElement({1,2,3}, 0, -2)` | **accepted**, `count == 3` — the finding's example |
| `SendPacketsElement({1,2,3}, 0, INT_MIN)` | **accepted**, `count == 3` |
| `SendPacketsElement({1,2,3}, 0, 4)` | rejected — **the control** |
| `SendPacketsElement({1,2,3}, -1, 1)` | **rejected** |
| `SendPacketsElement("/x", 5, -1)` (file overload) | **rejected** |

**Two corrections the finding does not contain:**

1. **A negative *offset* is already rejected**, and by accident of the same idiom that lets a
   negative *count* through: the offset check casts to `uintcs` first, so `-1` becomes `4294967295`
   and trips `ThrowIfGreaterThan`. The count check reads `count >= 0 ? count : buffer_.size()`
   **before** any cast, so the sign is consumed by the ternary and never reaches a check.
2. **The file-path overload is correct** — it calls `ThrowIfNegative(count)`. So this is **one
   overload's ternary**, not a type-wide gap, and the repair is correspondingly small.

**A diagnostic defect neither the finding nor this review's brief names:** the rejection message for
`offset = -1` reads *"'offset' must be less than or equal to 3. Actual value was 4294967295."* — it
reports the **unsigned reinterpretation** of the caller's argument. A caller who passed `-1` is told
about `4294967295`. Filed as part of **#2135**, because fixing the sign check and leaving the
message lying about the argument would be half a repair.

**Also measured:** an **empty file path** is accepted. Recorded in §6.1, not ticketed here.

### 4.3 SR-AUD-265 — confirmed, and there is a second defect underneath it

| Door | Measured |
|---|---|
| `NetworkStream(-1)` | **accepted**; `CanRead`/`CanWrite` **false**; `Read` returns **0** (EOF); `Write` **silently succeeds** |
| `NetworkStream(-2)` | same |
| `NetworkStream(999999)` | **accepted**; `CanRead`/`CanWrite` **true**; `Read` throws `IOException` *"Bad file descriptor"* |
| `NetworkStream(pipe read end)` | **accepted** — a descriptor that is **not a socket at all** |
| after `Close()`: `CanRead` | false |
| after `Close()`: `Read` | **returns 0** |
| after `Close()`: `Write` | **silently succeeds** |
| `Read(buf, -1, 4)` / `Read(buf, 0, -4)` | **rejected** — the controls |

**The finding's own claim is exactly right** and the two argument-domain controls hold, so this is
a *construction and state* defect rather than an argument-checking one.

**The second defect, and it is the more interesting one: a closed `NetworkStream` silently accepts
writes.** `Close()` records the closed state (`fd_ = -1`, and `CanRead`/`CanWrite` correctly report
`false`), and then `Read` returns EOF and `Write` **returns normally having written nothing**. A
caller that checks nothing — which is every caller of a `void Write` — is told the bytes went out.

**That is CCF-022's cause exactly**: *a public lifecycle state recorded but not enforced at every
public member that depends on the closed resource, not only at the data-transfer ones* — except
here it is not enforced at the data-transfer members either, which is the strongest form of it seen
so far. **CCF-022 is NOT minted** (see §8.2); this is recorded as a **candidate member** and
nothing more.

### 4.4 SR-AUD-266 — confirmed exactly as filed, and the parameter name says so

```cpp
TcpClient::TcpClient(const IPEndPoint& /*localEP*/) {}     // TcpClient.cpp:83
```

The local-endpoint constructor is an **empty body with the argument commented out**. Measured: it
constructs without error for both `127.0.0.1:0` and `[::1]:0`, and binds neither. Every other path
is `AF_INET` unconditionally — `Connect(hostname, port)` sets `hints.ai_family = AF_INET`
(TcpClient.cpp:96), `Connect(IPEndPoint)` builds a `sockaddr_in` (129), and `TcpListener::Start`
does the same (238).

**This is two different defects wearing one finding number, and they must not share a ticket:**

- **The ignored local endpoint** is a plain implementation gap: the constructor stores nothing, so
  nothing downstream can honour it. Compatible — **#2137**.
- **IPv4-only address-family selection** is not. Supporting `AF_INET6` means every connect, bind and
  accept path grows a `sockaddr_storage`, `TcpListener::AcceptTcpClient` starts returning endpoints
  it cannot represent today, and `UdpClient`'s receive path changes shape. That is a **design**
  question about how far this port carries IPv6, not a bug fix — **#2138**, `needs_user`.

### 4.5 SR-AUD-267 — confirmed; the port domain is never checked

| Input | Measured |
|---|---|
| `UdpClient(-1)` | **accepted** |
| `UdpClient(65536)` | **accepted** |
| `UdpClient(70000)` | **accepted** |
| `UdpClient(0)`, `UdpClient(65535)` | accepted — the controls |

`UdpClient.cpp:105` is `addr.sin_port = htons(static_cast<uint16_t>(port))`, so `70000` silently
becomes `4464` and `-1` becomes `65535`. The same truncation appears at lines 124, 157 and 173.
`System::Net::IPEndPoint` **already** validates its port (`modules/net`, reviewed), so the values
that arrive through an `IPEndPoint` overload are sound; it is the bare-`int` overloads that are
open. Compatible — **#2137**, with §4.4's ignored-endpoint half, because both are
"the constructor drops the caller's argument on the floor" in the same two files.

---

## 5. Structural root-cause families

- **NS-A — a descriptor-owning type accepts any `int` and validates nothing.** SR-AUD-265's
  construction half. The type's whole contract is "I own this handle", and it never asks whether it
  is one.
- **NS-B — a recorded closed state is not enforced at any member.** §4.3's second defect.
  **CCF-022 candidate**, not minted.
- **NS-C — the caller's argument is silently discarded or truncated at a constructor.**
  SR-AUD-266's local-endpoint half and SR-AUD-267. One ticket, **#2137**.
- **NS-D — a sign is consumed by a ternary before any range check can see it.** SR-AUD-264. Note
  this is the *inverse* of the idiom two lines below it, which converts to unsigned precisely so the
  sign trips the check — the two are in the same constructor.
- **NS-E — an async member captures a raw owner pointer with no liveness boundary.** SR-AUD-263.
  **CCF-019**; blocked.
- **NS-F — the address family is a compile-time constant.** SR-AUD-266's `AF_INET` half. Design;
  **#2138**.

---

## 6. Post-audit observations (no `SR-AUD-*` identifier)

### 6.1 Recorded, and deliberately not ticketed

- **`SendPacketsElement("")` is accepted.** An empty file path is not a file. It is recorded rather
  than ticketed because the only consumer of this type — `Socket::SendPacketsAsync` — is **absent
  from this port** (the header says so), so there is no door at which the empty path can do
  anything. If that consumer is ever ported, this becomes a real defect and this paragraph is where
  it is already written down.
- **`NetworkStream` accepts a non-socket descriptor** (a pipe read end was accepted). Folded into
  **#2136** rather than separately ticketed: it is the same missing construction check.

### 6.2 Measured positives, recorded so they are not re-investigated

- **`NetworkStream`'s `offset`/`count` argument checking is correct** — `Read(buf, -1, 4)` and
  `Read(buf, 0, -4)` both throw `ArgumentOutOfRangeException` with the right parameter name.
- **`SendPacketsElement`'s negative *offset* is rejected**, and its **file-path overload's negative
  count is rejected**. Only one overload's count is open.
- **`SendPacketsElement`'s upper bounds are correct** — `count` past the buffer end and `offset`
  past the buffer end are both rejected.
- **`NetworkStream::getLengthProperty` throws** *"does not support seeking"* rather than returning
  a fabricated length, before and after `Close()`.
- **`Socket` is non-copyable** and `NetworkStream` is non-copyable, each with a comment explaining
  the double-close it prevents. The descriptor-ownership hazard this module *does* have is
  lifetime, not aliasing.
- **No `std::` exception escaped any door probed**; every rejection was a `System::` exception.
- **No descriptor leak was observed** across the probe's construction/rejection paths.

---

## 7. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` | Component graph |
|---|---|---|---|---|---|
| **#2134** | **PUBLIC OWNERSHIP CHANGE** — `shared_ptr<Socket>` or a joining `~Socket` | **yes, expected** | possible | `~Socket` gains a throw path | none |
| **#2135** | narrows: a negative `count` stops meaning "whole buffer" | none | none | none | none |
| **#2136** | narrows: an invalid/non-socket fd stops constructing; a closed stream starts throwing | **none expected** — `fd_` already records the state | none | none | none |
| **#2137** | narrows: an out-of-range port throws; the local endpoint starts being honoured | none | none | none | none |
| **#2138** | **DESIGN** — IPv6 changes every connect/bind/accept path | likely | none | none | none |
| **#2139** | documentation and pins | none | none | none | none |

**No compatible ticket in this module needs an object-layout, vtable or component-graph change.**
The graph stays at **41 modules / 92 edges**; `Net.Sockets` already depends on `Net`, `IO`,
`Core.Base` and `Threading.Tasks`, which is everything the compatible repairs need.

---

## 8. CCF mapping

### 8.1 CCF-019 — one member here, and it is this module's only `high`

SR-AUD-263 joins `text-json` SR-AUD-327, `xml-linq` SR-AUD-333, `threading` SR-AUD-187/221,
`threading-tasks` SR-AUD-230, `net-http` SR-AUD-310 and `net-websockets` SR-AUD-247. **CCF-019 is
NOT marked closed** and gains no approval from this review. #2134 is `blocked`.

### 8.2 CCF-022 — a NEW candidate member, recorded and not minted

§4.3's closed-`NetworkStream`-still-writes defect is X-D's cause exactly: *a public lifecycle state
recorded but not enforced*. It would be the family's **seventh** site and its **third** module,
alongside `xml` (SR-AUD-349, SR-AUD-348 — both remediated), `io` (SR-AUD-344 remediated;
SR-AUD-337/343 blocked on Approval IO-1; SR-AUD-342's half open as #2099).

**CCF-022 is NOT minted, and the reason is the one #2109 recorded and this batch re-verified for
CCF-021: authority.** Every promotion sentence in the audit corpus is passive and names no agent,
and the single non-passive statement — `docs/SystemIONamespaceReviewPlan.md` §8.2 — reserves the act
to the maintainer. Adding a member to an unminted candidate changes nothing about that. Recorded on
#2109; **this review does not mint and does not recommend minting on its own authority.**

### 8.3 CCF-021 — no member here

`Net.Sockets` writes no protocol frame. It moves opaque bytes; the framing lives in `Net.Http` and
`Net.WebSockets`, both already remediated. **No member.** CCF-021 remains unminted (#2131).

### 8.4 CCF-012 — no member. **CCF-012 is not marked closed.**

---

## 9. Parsing, serialization, mutation, resource and thread-safety consequences

- **Parsing.** There is none to speak of: this module parses no text. `TcpClient::Connect(hostname)`
  delegates to `getaddrinfo`. That is unusual for a namespace review at this stage and is why the
  test matrix below is dominated by argument domains and descriptor state rather than grammars.
- **Resources — the dimension that selected this unit.** Every finding except SR-AUD-264 touches a
  descriptor. #2136's construction check must not *close* a descriptor it rejects (the caller still
  owns it until construction succeeds), and #2137's port rejection must not leak the socket it has
  already created. Both are `/proc/self/fd` questions, and **LSan cannot answer either** — a leaked
  descriptor is not a leaked allocation.
- **Mutation.** `Socket` is move-assignable while an async worker runs (§4.1). No other type in the
  module has a mutation-during-use hazard: the clients own their descriptor outright.
- **Thread safety.** Nothing here is documented thread-safe and nothing is, except that SR-AUD-263
  makes `Socket` actively unsafe to destroy or move-assign concurrently with its own async members.
  Recorded so a future reader does not assume it was checked and found safe — it was checked and
  found **absent**, which is different.
- **Partial state before failure.** Measured at each rejecting door in §4: no constructor left a
  half-built object, and no rejected `SendPacketsElement` stored a count.

---

## 10. Deferred evidence — what `/rv` would settle and this review will not guess

- Whether .NET's `SendPacketsElement` rejects a negative `count` with
  `ArgumentOutOfRangeException` **or** clamps — the per-file report states rejection via unsigned
  range validation, transcribed at audit time; **#2135** follows that and records it as this port's
  choice.
- Whether .NET's `NetworkStream` throws `ObjectDisposedException` or `IOException` from `Write`
  after `Close()`, and whether it validates the socket's `Connected`/`Blocking`/`SocketType` at
  construction — the per-file report names all four checks; **#2136** follows it for *what* is
  rejected and records the **exception type** as this port's choice.
- Whether .NET's `UdpClient(int port)` throws `ArgumentOutOfRangeException` for `65536` — RFC 793's
  16-bit port field settles that a port outside `[0, 65535]` is not a port, which is the evidence
  **#2137** rests on.
- How far this port should carry IPv6 — **#2138**, a design ticket for exactly this reason.

---

## 11. Test matrix

| Ticket | Required cases |
|---|---|
| **#2135** | `count` ∈ {−1, −2, `INT_MIN`} → throws with `paramName == "count"`; `count` ∈ {0, 3} still accepted; `count == 4` still rejected (**control**); a negative **offset** still rejected; the **message names the caller's value, not its unsigned reinterpretation**; the file-path overload unchanged (**pin**) |
| **#2136** | `NetworkStream(-1)`, `(-2)`, a closed fd, and a **non-socket** fd → rejected at construction; a valid socket fd still accepted (**control**); after `Close()`, `Read` **and** `Write` throw rather than returning EOF/succeeding; `CanRead`/`CanWrite` stay `false`; `getLengthProperty` still throws its seek message (**pin**); the `offset`/`count` checks unchanged (**pin**); **`/proc/self/fd` unchanged across every rejection** — a rejected constructor must not close the caller's descriptor |
| **#2137** | `UdpClient` port ∈ {−1, 65536, 70000, `INT_MIN`, `INT_MAX`} → throws with `paramName == "port"`; {0, 65535} still accepted (**control**); the `IPEndPoint` overloads unchanged (**pin**); `TcpClient(IPEndPoint)` binds the endpoint it was given and a subsequent `Connect` uses it; **`/proc/self/fd` unchanged across every rejection** — a rejected port must not leak the socket already created |
| **#2139 / pins** | §6.2's seven measured positives; #2134's and #2138's current behaviour |

## 12. Sanitizer and direct-resource matrix

| Tool | Applicable here? |
|---|---|
| **ASan** | **yes** — buffer/offset arithmetic in `Read`/`Write` and `SendPacketsElement` |
| **UBSan** | **yes** — `static_cast<uint16_t>(port)`, `intcs`↔`uintcs` casts, the `INT_MIN` cases |
| **LSan** | marginal, and **explicitly not a substitute** for descriptor accounting |
| **TSan** | **yes, but only for #2134**, which is blocked. A racing lifetime reproduction is flaky by construction (#2096's recorded reason), so this review does not build one |
| **`/proc/self/fd`** | **required** — the primary instrument for #2136 and #2137 |

---

## 13. Bounded tickets and recommended order

```
#2135  SR-AUD-264  a negative count means "whole buffer"          (P2, S) ── FIRST
#2136  SR-AUD-265  NetworkStream accepts any int, and a closed
                   stream silently accepts writes                 (P2, M) ── SECOND
#2137  SR-AUD-266 (endpoint half) + SR-AUD-267
                   two constructors discard the caller's argument  (P2, M) ── THIRD
#2134  SR-AUD-263  DESIGN/BLOCKED: async members capture a raw
                   this (CCF-019)                                 (P1)    ── blocked
#2138  SR-AUD-266 (family half) DESIGN: how far does IPv6 go?     (P2)    ── needs_user
#2139  documentation and gated-behaviour pins                     (P3, S) ── LAST
```

**Recommended order: #2135, then #2136, then #2137.** #2135 first because it is the smallest and is
confined to one header with no descriptor involved. #2136 second because its closed-state half is a
CCF-022 candidate and the family's evidence is better with it measured. #2137 last of the three
because it is the only one that must prove a **non**-leak.

## 14. Compatible versus blocked or deferred

| Ticket | Compatible? | Why |
|---|---|---|
| #2134 | **no** — blocked | CCF-019; public ownership change, unapproved |
| #2135 | **yes, with a documented narrowing** | a negative `count` stops meaning "whole buffer" |
| #2136 | **yes, with a documented narrowing** | an invalid fd stops constructing; a closed stream starts throwing |
| #2137 | **yes, with a documented narrowing** | an out-of-range port throws; the local endpoint is honoured |
| #2138 | **no** — design; changes every connect/bind/accept path |
| #2139 | **yes** — documentation and pins only |

## 15. Exclusions

- `modules/net`'s `IPAddress`/`IPEndPoint`/`Dns` — reviewed and closed; consumed, never modified.
- `System::Net::NetworkInformation` — different component; **#1962 stays blocked**.
- TLS — permanent deviation, no subject.
- `SocketAsyncEventArgs` / `Begin*`/`End*` — absent from this port.
- Windows and Emscripten runtime behaviour — compile-supported, not the verified baseline.
- CNA and mobile-eggbert — not inspected; **#1773 stays blocked**.

## 16. Completion criteria

This review (#2133) is complete when this document exists, each of the five open findings has
exactly one disposition in §4, each post-audit observation carries a ticket or an explicit
"recorded, not ticketed", and §13's tickets are in `plan.sqlite3`. **It is complete on those terms
and remediates nothing by itself.**

`modules/net-sockets` is closed for *compatible* work when #2135, #2136, #2137 and #2139 are `done`,
SR-AUD-264/265/266(endpoint half)/267 are `remediated`, SR-AUD-263 carries a blocked ticket and a
behaviour pin, and SR-AUD-266's family half carries a design ticket and a pin.

## 17. Implementation record

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.

### 17.1 #2135 — the prediction held, and the diagnostic half turned out to matter more than expected

§4.2's two corrections held without amendment: the negative *offset* was already rejected by the
unsigned-cast idiom two lines below the ternary, and the file-path overload was already correct.

**The repair is smaller than "add a check".** The whole-buffer meaning moved to the one-argument
constructor, where it is what the caller actually asked for, so no negative sentinel travels into
the checked overload at all and the ternary simply ceases to exist. That is why the checked overload
now reads as three ordinary bounds in a row.

**The diagnostic half is what the mutations proved was load-bearing.** M1 restores the ternary in
the buffer overload only, and it fails **two** tests — `EveryNegativeCountIsRejected` *and*
`ARejectionNamesTheCallersValueNotItsUnsignedReinterpretation`. The second failure is the
interesting one: with the ternary back, the negative *offset* path reverts to being caught by the
unsigned bound, and the message goes back to telling a caller who passed `-1` about `4294967295`. A
repair that fixed only the sign check would have left that in place and looked complete. M2
over-repairs with `ThrowIfNegativeOrZero`, so `count == 0` is rejected too, and fails **exactly
one** test — the valid-counts control. M3, a semantics-preserving `uintcs` round-trip on the stored
value, fails **none**.

**+4 tests** (`SharpRuntimeTests_Net_Sockets` 88 → **92**; the module's one pre-existing failure,
`SocketTests.Connect_ByHostname_NoMatchingAddressFamily_Throws`, is unrelated — `/proc/net/if_inet6`
is absent in this environment). **ASan + UBSan + LSan clean over 1,458 constructions**
(`build-probe/2135_probe2_san.log`) covering `INT_MIN` and `INT_MAX` at both parameters across
buffer sizes 0–5 and both overloads, with a live heap-use-after-free control. UBSan is the right
instrument here specifically because `INT_MIN` no longer reaches an unsigned cast.

**No signature, layout, vtable or exception-specification change. Graph unchanged at 41 / 92.**

**Correction, made by the following batch and recorded here rather than silently.** The paragraph
above originally read *"+7 tests (84 → 91)"*. That is wrong in all three figures, and it is the
sole cause of the inherited **16,005 vs 16,002** gate discrepancy — see §18. The measured
registration delta of commit `5087c2c` is **+4** (`SocketsSupportTests.cpp` 33 → 37 `TEST` macros;
`SocketTests.cpp` 17 and `SocketsTests.cpp` 38 unchanged), taking the executable **88 → 92**. The
four are `EveryNegativeCountIsRejected`, `THECONTROLTheValidCountsAreUnchanged`,
`ARejectionNamesTheCallersValueNotItsUnsignedReinterpretation` and
`THEPINTheFilePathOverloadWasAlreadyCorrect`. Nothing about the repair itself changes; only the
count was mis-transcribed.

---

## 18. Historical gate-total reconciliation — the inherited 16,005 was right

The batch that produced §17.1 reported the gate as **16,052** and simultaneously stated that its own
additions were **+50**, which implies a prior total of **16,002** where the inherited handoff said
**16,005**. That three-test difference is resolved here, by measurement, and the **inherited
number was the correct one**.

**Method.** Test *registration* is a property of the sources, so the reconciliation does not need a
rebuild at every commit — only a rebuild-free enumeration at the tip plus per-commit registration
deltas from git. Both were done:

1. **Tip, measured.** All **37** executables enumerated with `--gtest_list_tests`: **16,052**
   registered tests. The two binaries in question were verified newer than their sources
   (`SharpRuntimeTests_Net_Sockets` 07:36:59 vs `SocketsSupportTests.cpp` 07:35:14), so the
   enumeration is of the current tree, not a stale build.
2. **Everything that changed since the previous handoff `a3cfa69`.** `git diff --name-only`
   lists exactly **six** test files: five **new** `net-http-headers` files (7 + 7 + 7 + 14 + 8 =
   **43** `TEST` macros) and `net-sockets`' `SocketsSupportTests.cpp` (**33 → 37**, **+4**). No test
   file elsewhere changed, and **no test was deleted anywhere** (`git show <c> | grep -c '^-TEST'`
   is 0 for every commit in the range).
3. **Therefore `a3cfa69` = 16,052 − 43 − 4 = 16,005**, which is exactly the inherited figure.
4. **The chain before it also holds.** The handoff at `a3cfa69` derived 16,005 as 15,967 + 38 =
   9 + 7 + 8 + 7 + 7; the measured `^+TEST` counts of `7f19852`, `149f064`, `d193768`, `577e836`
   and `c4a25e1` are **9, 7, 8, 7, 7** — the stated decomposition is correct commit by commit.
5. **The `net-sockets` executable has been 88 since 2026-07-31** (`dd09de1`: 33 + 17 + 38) and 92
   since `5087c2c`. **No commit in this repository's history ever had it at 84 or 91**, so the
   figures in §17.1 were not a stale reading of some earlier state either.

**Cause: an arithmetic/transcription error in one report, not a change in the tests.** The
"+7 (84 → 91)" in §17.1 propagated into the handoff's "+50", and 50 − 47 = **3** is the whole
discrepancy. No test was added or removed by any intermediate commit beyond the 47 above, no suite
lost tests, no executable is stale, and test discovery is not environment-dependent here.

**Corrected historical baseline, for future delta arithmetic:**

| Commit | Gate total | How established |
|---|---:|---|
| `a3cfa69` (previous handoff) | **16,005** | inherited **and** re-derived as 16,052 − 47 |
| `f013fe1` (this tip, before this batch) | **16,052** | measured, 37 executables |

The figure to distrust is neither 16,005 nor 16,052 — it is the **"+50"** in between, now **+47**.
