<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Net::Http` namespace review — ticket #2062

Owning ticket **#2062**. This document is the durable record; it **remediates nothing by
itself**. Every claim below was measured against the tree at `e62b22a` with
`build-probe/2062_probe1_nethttp_defects.cpp`,
`build-probe/2062_probe2_fdleak.cpp` and `build-probe/2062_probe3_ctl.cpp`
(logs `2062_probe1_before.log`, `2062_probe2_fdleak.log`, `2062_probe3_ctl.log`).

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-04, `/rv` does not exist.
Every statement about .NET below therefore comes from repository-contained evidence only: the
per-file audit reports, existing doc-comments transcribed from .NET when the module was
written, and this module's own tests. Where a repair would need .NET's exact grammar and no
repository evidence pins it, a **deferred-verification ticket** is created instead of a guess.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit
defects found by this review carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket #1773 stays blocked.

---

## 1. Why this unit was selected

Measured from `audit/AUDIT_FINDINGS_INDEX.md` at `e62b22a`, every coherent unit with at least
six open findings and no existing durable namespace review:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `modules/core` | 72 | 9 | 59 | 4 | 12% | 1 | 47 | — (family plans only) |
| `modules/io` | 11 | 0 | 11 | 0 | 0% | 0 | 2 | none |
| **`modules/net-http`** | **9** | **2** | **7** | **0** | **22%** | **0** | **0** | **none** |
| `modules/xml` | 8 | 2 | 6 | 0 | 25% | 0 | 0 | none |
| `modules/time-zone` | 7 | 0 | 7 | 0 | 0% | 0 | 0 | none |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none |
| `modules/text-json` | 7 | 1 | 6 | 0 | 14% | 1 | 0 | none |
| `modules/net-websockets` | 6 | 2 | 4 | 0 | 33% | 0 | 0 | none |

Units already reviewed and therefore excluded regardless of count: `modules/threading` (17
open), `modules/runtime` (14), `modules/text` (11), `modules/uri` (10), `modules/buffers` (5),
`modules/net` (5), `modules/diagnostics`.

**`modules/core` is excluded on coherence, not on count.** 72 open findings across 119 files
is not one review unit; it is the residue of a dozen cross-cutting families that already have
their own plans (`docs/CCF-*`, `NumericWrapperBoundaryPlan`, `DateTimeValidationBoundaryPlan`,
…). Reviewing it as a namespace would produce an unbounded document and a ticket queue nobody
could execute. It should continue to be worked family by family.

### Applying the stated selection priorities, in order

1. **High-severity memory or lifetime defects.** `net-http` has SR-AUD-310, an
   **ASan-confirmed use-after-free**: a task outliving a destroyed `HttpClient` reads freed
   `defaultHeaders_` through a captured raw `this`. `xml` has SR-AUD-351 (a node detached from
   an unrelated tree) — an ownership defect, but no confirmed dangling read.
   `net-websockets` has the same UAF shape (SR-AUD-247), on 6 findings rather than 9.
2. **Public-input crashes or corruption.** `net-http`'s inputs are **remote-attacker-controlled
   by construction**: SR-AUD-311/312/318 are all driven by a server's response. This review
   additionally measured a **file-descriptor leak of one socket per failing request** from four
   independent server-controlled failure paths (§4.7) — a remote resource-exhaustion channel
   the audit filed only as a clause. `xml`'s inputs are documents, which is a weaker adversary
   model for a game runtime.
3. **Useful compatible queue.** `net-http` yields five compatible tickets (§18.1). `xml` would
   yield roughly four. Comparable; not decisive.
4. **Coherent dependency boundary.** `net-http` is 24 headers and 5 bodies, ~2,300 lines, one
   CMake component. Coherent — with one caveat recorded in §21: the CR/LF-injection shape it
   shares with `Net.Http.Headers` (SR-AUD-319, SR-AUD-322) crosses the module line, and this
   review deliberately scopes to `modules/net-http` and records the promotion rule instead.
5. **No existing complete review.** Both qualify.

**Selected: `modules/net-http`.** Priorities 1 and 2 favour it decisively; 3 and 4 are neutral.
`modules/xml` is the recommended next unit and `modules/net-websockets` the one after, since
its UAF is the same shape as SR-AUD-310 and should be judged against whatever #2063 concludes.

---

## 2. Scope and file inventory

Component `Net.Http` (`modules/net-http/CMakeLists.txt`): `TYPE STATIC`,
`PUBLIC_DEPENDENCIES Core.Base IO Net Threading Threading.Tasks`,
`PRIVATE_DEPENDENCIES Uri`, `TEST_DEPENDENCIES Net.Sockets`.

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 24 | 1,449 |
| implementation | 5 | 819 |
| tests | 1 (`HttpClientTests.cpp`) | 1,166 |

**In scope:** everything under `modules/net-http/`.

**Out of scope, and why:** `modules/net-http-headers` (its own component, its own five
findings, §21), `modules/net-http-json`, `modules/net`, `modules/net-sockets`. TLS/HTTPS is a
documented permanent deviation (`CLAUDE.md` — "Symmetric/asymmetric cryptography … out of
scope"), so `parseUrl`'s rejection of every non-`http` scheme is **correct as written** and is
not a finding.

---

## 3. Complete public-surface inventory

| Type | Kind | Public members | Notes |
|---|---|---|---|
| `HttpClient` | class, non-copyable | 2 ctors, `Send`, `Get`, `Post`, `GetString`, `GetByteArray`, 5 `*Async`, `get/setBaseAddressProperty`, `get/setDefaultHeader`, **`ParsedUrl` + `static parseUrl`**, **`ParsedStatusLine` + `static parseStatusLine`** | the two parsers are **public static** — callable, and tested, with no socket |
| `HttpClientHandler` | `HttpMessageHandler` | ctor/dtor, `Send`, `Dispose`, cookie/redirect/proxy properties | the only socket code in the module |
| `HttpMessageHandler` | abstract | `Send`, `Dispose` | |
| `HttpMessageInvoker` | class | `Send`, `SendAsync`, `Dispose` | |
| `DelegatingHandler` | `HttpMessageHandler` | `get/setInnerHandlerProperty`, `Send` | |
| `HttpRequestMessage` | class | method/uri/content/headers/options accessors, `setHeader` | headers are a raw `std::unordered_map<std::string,std::string>` |
| `HttpResponseMessage` | class | status/reason/content/headers accessors, `getIsSuccessStatusCodeProperty`, `EnsureSuccessStatusCode`, `setHeader`, `getHeader` | same raw map |
| `HttpContent` | abstract | `ReadAsString`, `ReadAsByteArray`, `getContentTypeProperty`, `getCharSetProperty` | |
| `ByteArrayContent`, `StringContent`, `ReadOnlyMemoryContent`, `StreamContent`, `FormUrlEncodedContent`, `MultipartContent`, `MultipartFormDataContent` | `HttpContent` | ctors + the four overrides | |
| `HttpMethod` | value type | 9 static factories, `getMethodProperty`, comparison | |
| `HttpRequestOptions`, `HttpRequestOptionsKey<T>` | class/template | typed option get/set/remove | |
| `HttpRequestException`, `HttpIOException`, `HttpProtocolException` | exceptions | | |
| `HttpCompletionOption`, `HttpVersionPolicy`, `HttpRequestError` | enums | | |

---

## 4. Every open finding, with its measured disposition

| Finding | Severity | Reproduced? | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-310 | high | yes (audit's ASan run; not re-run here) | **approval-sensitive** — ownership | #2066 (blocked) |
| SR-AUD-311 | medium | yes, and **wider than filed** | compatible | **#2064** |
| SR-AUD-312 | medium | yes, and **wider than filed** | compatible | **#2064** |
| SR-AUD-313 | high | yes, and **wider than filed** | compatible | **#2063** |
| SR-AUD-314 | medium | not re-run | approval-sensitive — public semantics | #2067 (blocked) |
| SR-AUD-315 | medium | yes | approval-sensitive — public type change | #2068 (blocked) |
| SR-AUD-316 | medium | yes | **split**: reason-phrase half compatible (**#2063**), status-code-domain half approval-sensitive (#2069, blocked) |
| SR-AUD-317 | medium | yes | deferred verification | #2070 |
| SR-AUD-318 | medium | yes — **one leaked descriptor per failing request** | **split**: descriptor leak compatible (**#2065**), read limits approval-sensitive (#2071, blocked) |

### 4.1 SR-AUD-313 + the request URI — CR/LF reaches the wire (high) → **#2063, compatible**

The finding names header, media-type and content-disposition concatenation.
`HttpClientHandler::Send` writes `req << k << ": " << v << "\r\n"` for every entry of the
request's header map with no validation, so
`request->setHeader("X-A", "v\r\nX-Injected: yes")` emits two header fields. Measured:
accepted, stored, and serialized verbatim.

**Premise correction — the request URI is a second, unnamed injection vector.** Measured:

| Input | `parseUrl` result |
|---|---|
| `http://ho<CR>st/p` | `host = "ho\rst"` — accepted |
| `http://ho<CR><LF>st/p` | `host = "ho\r\nst"` — accepted |
| `http://ho<NUL>st/p` | `host = "ho\0st"` — accepted |

The handler then writes `Host: ` + that host, so a caller-controlled or redirect-derived URL
injects header fields exactly as a header value does. **A repair that validates only header
values leaves the door open**, which is precisely the mistake this review exists to prevent.

**Third vector, same ticket:** `HttpResponseMessage::setReasonPhraseProperty` accepts
`"OK\r\nX-Injected: yes"`. That one is not on the wire in this port (the reason phrase is only
ever *read* from a response), but it is public, it is what a server sends, and leaving it
unvalidated while validating the other two would be arbitrary.

**Compatible.** Rejecting a control character in a header name, a header value, a request
authority and a reason phrase narrows the accepted input set to what .NET already rejects, adds
no type, no member and no signature. The narrowing is real and must be documented as a
migration note.

### 4.2 SR-AUD-311 — `parseUrl` accepts what it must reject (medium) → **#2064, compatible**

Measured, all accepted today:

| Input | Result | Should be |
|---|---|---|
| `http://host:80abc` | port **80** | reject — `std::stoi` stops at the first non-digit |
| `http://host:-1/p` | port **−1** | reject |
| `http://host:99999/p` | port **99999** | reject — outside 0…65535 |
| `http://[::1]:70000` | port **70000** | reject |
| `http://host?q=1` | host **`host?q=1`**, path `/` | host `host`, path `/?q=1` |
| `http://host#frag` | host **`host#frag`**, path `/` | host `host`, path `/` |
| `http://HOST.EXAMPLE/p` | host `HOST.EXAMPLE` | host lowercased (scheme already is) |

`http://user:pass@host/p` and `http://host:2147483648/p` already throw `UriFormatException`,
and `http:///p` (empty authority) already throws — those three stay unchanged.

The authority-only query is the worst of these: the query string becomes part of the DNS name
**and** part of the `Host:` header, and the request line then asks for `/`. A caller who writes
`client.Get("http://api.example/?key=secret")` sends the secret in the `Host` header and
requests the wrong resource.

**Compatible**, and it belongs with §4.3 because both are the same root cause (§5.1).

### 4.3 SR-AUD-312 — `parseStatusLine` accepts what it must reject (medium) → **#2064, compatible**

Measured:

| Status line | Parsed code |
|---|---|
| `HTTP/1.1 200trailer OK` | **200** |
| `HTTP/1.1 2 OK` | **2** |
| `HTTP/1.1 -5 OK` | **−5** |
| `HTTP/1.1 99999 OK` | **99999** |
| `GARBAGE 200 OK` | **200** — the version token is never examined |
| `HTTP/9.9 200 OK` | **200** |

`HTTP/1.1  200 OK` (double space) throws, which is arguably too strict but is left alone: no
repository evidence pins .NET's behaviour for it and no test covers it.

The `-5` row is the one with a memory-shaped consequence: it is cast to
`static_cast<System::Net::HttpStatusCode>(statusCode)` and stored, so a public enum holds a
value no enumerator names.

**Compatible**: require three digits, reject a non-numeric remainder, require an `HTTP/` version
token of the shape `HTTP/<digit>.<digit>`.

### 4.4 SR-AUD-316 — response construction accepts any number (medium) → **split**

Measured: `HttpResponseMessage(static_cast<HttpStatusCode>(-1))`, `(0)`, `(1000)` and `(99999)`
all construct, and `getIsSuccessStatusCodeProperty()` answers `false` for each.

- **Reason-phrase half → #2063, compatible.** CR/LF in the reason phrase is a control-character
  rejection like every other in §4.1.
- **Status-code-domain half → #2069, blocked.** Rejecting an out-of-domain code means either
  making the constructor throw (a behaviour change on a public constructor that today cannot
  fail, i.e. an exception-specification and semantic change) or replacing the parameter type.
  Neither is compatible, and `EnsureSuccessStatusCode`'s message text would change with it.
  Pinned by #2063's disclosure tests, not implemented.

### 4.5 SR-AUD-315 — the header maps are case-sensitive (medium) → **#2068, blocked**

Measured: setting `Content-Type` and `content-type` on one `HttpRequestMessage` leaves **two**
entries; `HttpResponseMessage::getHeader("content-type")` returns `""` after
`setHeader("Content-Type", …)`.

HTTP field names are case-insensitive, so this is a genuine defect. But
`getHeadersProperty()` **returns a reference to `const std::unordered_map<std::string,
std::string>&`**, which is public surface: changing the comparator changes the returned type,
breaking every caller that names it. That is the `Migration-CollectionsFloatingComparers`
shape. **Blocked**, behaviour pinned by #2063.

A second, related defect this review measured and the finding does not name:
`HttpClientHandler::Send` writes `Host`, `User-Agent`, `Accept` and `Connection`
**unconditionally**, before the caller's headers. A caller who sets `Host` therefore sends
**two** `Host` fields. That is request-smuggling-adjacent and is folded into #2068 rather than
into the compatible ticket, because de-duplicating requires the case-insensitive lookup #2068
introduces.

### 4.6 SR-AUD-310 — the async lambdas capture raw `this` (high) → **#2066, blocked**

`SendAsync`, `GetAsync`, `PostAsync`, `GetStringAsync` and `GetByteArrayAsync` each build a
`TaskT<…>` from a lambda capturing `[this, …]`. A task that outlives its `HttpClient` reads
freed `defaultHeaders_` and a freed `handler_`. The audit confirmed it under ASan; this review
did not re-run it because the finding is not in dispute and re-running costs a build.

This is **CCF-019** verbatim — *"a copyable public handle retaining a raw pointer with no owner
liveness"* — the family that already blocks #1888/#1889/#1894/#1899/#1959/#1970. Repairing it
means either `HttpClient` becoming `enable_shared_from_this` (a public base-class change) or
`TaskT` gaining ownership semantics (a `Threading.Tasks` change with a blast radius far outside
this module). **Blocked. This review does not close CCF-019 and does not claim to.**

### 4.7 SR-AUD-318 — post-connect throws leak the socket (medium) → **split; leak half is #2065, compatible**

`connectToHost` returns a bare descriptor and `platformClose(fd)` is called at exactly **one**
point, after the body has been read. Every throw between the two leaks it. Measured with
`build-probe/2062_probe2_fdleak.cpp` — 20 requests to a local server, counting
`/proc/self/fd`:

| Server response | Requests | Threw | Descriptors leaked |
|---|---:|---:|---:|
| well-formed | 20 | 0 | **0** |
| garbled status line | 20 | 20 | **20** |
| `Content-Length: abc` | 20 | 20 | **20** |
| chunk size `ZZZ` | 20 | 20 | **20** |
| body shorter than `Content-Length` | 20 | 20 | **20** |

**One descriptor per failing request, from four independent failure paths, every one of them
chosen by the remote peer.** A server that answers 1,024 requests with a garbled status line
exhausts a default `RLIMIT_NOFILE`. The audit filed this as a clause inside a medium finding;
measured, it is the most consequential compatible defect in the module, and #2065 is priority
**P1**.

The repair is an RAII descriptor guard around `connectToHost`'s result — entirely inside
`HttpClientHandler.cpp`, no header, no signature, no layout. Note the correct closure
semantics: the guard must close on **every** exit, including the successful one, because the
handler always sends `Connection: close`.

**The read-limits half is not compatible.** `recvAll` and a `Content-Length`-bounded
`recvExact` accumulate without bound, and `std::stoul(chunkLine, nullptr, 16)` accepts a chunk
size up to `SIZE_MAX`. Imposing a maximum response size is a new public policy — .NET spells it
`HttpClient.MaxResponseContentBufferSize`, a property this port does not have — so it is a
public-surface addition. **#2071, blocked.**

### 4.8 SR-AUD-314 — no sent-state guard (medium) → **#2067, blocked**

.NET throws `InvalidOperationException` when an `HttpRequestMessage` is sent twice. Adding that
means `HttpRequestMessage` gaining state, i.e. an object-layout change to a public class, plus
a new throw from `Send`. **Blocked**, behaviour pinned by #2063.

### 4.9 SR-AUD-317 — the declared charset and the emitted bytes can disagree (medium) → **#2070, deferred verification**

Measured: `StringContent("\xc3\xa9", "utf-16", "text/plain")` emits `c3 a9` — UTF-8 bytes under
a `charset=utf-16` label.

The repair is not obvious and the evidence to choose it is **absent**. .NET's `StringContent`
*encodes* through the supplied `Encoding`; this port's `HttpContent` has no encoding parameter
at all, only a charset **string**, and `System::Text`'s encoding factories are themselves the
subject of blocked tickets #2013–#2017. Three candidate repairs (encode through
`System::Text::Encoding`; reject a charset the port cannot encode; document the label as
advisory) differ in what they break, and choosing between them without the .NET reference would
be a guess. **Deferred verification ticket #2070**, with the current behaviour pinned by #2063
so no future option can land silently.

---

## 5. Structural root-cause families

### 5.1 NH-A — a hand-rolled parser accepts a prefix and calls it the whole value

Members: SR-AUD-311 (`std::stoi` on the port), SR-AUD-312 (`std::stoi` on the status code, and
a version token that is never examined). Root cause: `std::sto*` **stops at the first invalid
character and reports success**, and no site checks that the whole field was consumed.

This is **CCF-002's shape** — *"a `std::sscanf`/`std::sto*` parser that accepts a valid prefix
plus arbitrary tail"* — already remediated in `System::DateTime`, `TimeOnly` and `XmlConvert`
by routing through a full-consumption scanner, and still open as SR-AUD-321 in
`Net.Http.Headers`. **#2064 cites CCF-002 rather than minting a family.**

### 5.2 NH-B — a control character crosses a public door into a protocol frame

Members: SR-AUD-313, the request-URI vector of §4.1, SR-AUD-316's reason-phrase half. Root
cause: every string that reaches the wire is concatenated with no notion of which characters
terminate a field.

The same shape is open in `Net.Http.Headers` as SR-AUD-319 and SR-AUD-322, and in
`Net.WebSockets` as SR-AUD-248. **This review does not mint a CCF for it.** §21 records the
promotion rule: three modules is a pattern, but two of the three are unreviewed, and minting a
cross-cutting family from one module's evidence is the mistake the Buffers review's §5.3
already declined to make. Mint **CCF-021** when `Net.Http.Headers` or `Net.WebSockets` is
reviewed, citing all three.

### 5.3 NH-C — a raw descriptor with one close on one path

Member: SR-AUD-318's leak half. Root cause: a POSIX/Winsock handle owned by control flow rather
than by an object. Module-local; one site; **#2065**.

### 5.4 NH-D — a borrowed `this` with no owner liveness → **CCF-019**

Member: SR-AUD-310. No new family; #2066 cites CCF-019 and does not claim to close it.

### 5.5 NH-E — a public container type is the contract

Members: SR-AUD-315, and the unconditional-default-headers defect of §4.5. Root cause:
`getHeadersProperty()` hands back `const std::unordered_map<std::string, std::string>&`, so the
comparator is public. This is the `Migration-CollectionsFloatingComparers` shape. **#2068,
blocked.**

### 5.6 NH-F — the type cannot represent the constraint it needs

Members: SR-AUD-316's status-code half, SR-AUD-314, SR-AUD-318's limits half, SR-AUD-317.
Root cause: the repair needs state or a member the public type does not have. All blocked or
deferred; all pinned by #2063.

---

## 6. Corrected premises

| # | The record said | Measured |
|---|---|---|
| 6.1 | SR-AUD-313 is about header, media-type and disposition concatenation | The **request URI** is a third vector: CR, LF and NUL pass through `parseUrl` into the `Host:` header. A header-only repair leaves the door open. |
| 6.2 | SR-AUD-312 is about "prefix numeric parsing … and invalid versions" | The version token is **never parsed at all** — `GARBAGE 200 OK` yields 200. There is nothing to make stricter; there is a check to add. |
| 6.3 | SR-AUD-311 lists "prefix ports, invalid ranges, bracket suffixes, authority-only queries" | Confirmed, plus **the host is never lowercased** while the scheme is, so `HOST.EXAMPLE` and `host.example` are distinct hosts to every downstream consumer. |
| 6.4 | SR-AUD-318: "exceptions after connect bypass the only socket close" | Confirmed and **quantified**: one descriptor per failing request, from four remote-controlled paths, 20/20 every time. That promotes the leak half from a clause in a medium finding to a P1 compatible ticket. |
| 6.5 | SR-AUD-315 is about case-distinct header names | Confirmed, plus the handler writes `Host`/`User-Agent`/`Accept`/`Connection` **unconditionally before** the caller's map, so a caller-set `Host` produces two `Host` fields. |
| 6.6 | SR-AUD-316 is one finding | It is **two**, with different blast radii: the reason phrase is a compatible control-character rejection, the status-code domain is a public constructor's exception specification. |
| 6.7 | — | `parseUrl` and `parseStatusLine` are **public static members with public nested result structs**, not internals. Every change to them is public-surface-visible, and both are already exercised by `HttpClientTests.cpp`. |

---

## 7. Dependency graph of the tickets

```
#2065  socket descriptor leak (P1)            ── independent
#2063  control characters + disclosure (P1)   ── independent
#2064  full-consumption parsers (P2)          ── independent of both,
                                                 but SHARES parseUrl with #2063:
                                                 land #2063 first, #2064 second
#2066  CCF-019 ownership          ── blocked
#2067  sent-state guard           ── blocked, pinned by #2063
#2068  case-insensitive headers   ── blocked, pinned by #2063
#2069  status-code domain         ── blocked, pinned by #2063
#2070  charset vs bytes           ── deferred verification, pinned by #2063
#2071  response read limits       ── blocked, pinned by #2063
```

---

## 8. Compatible versus approval-sensitive

| Ticket | Compatible? | Why |
|---|---|---|
| #2063 | yes, **with a documented narrowing** | rejects input .NET already rejects; no type, member or signature change |
| #2064 | yes, **with a documented narrowing** | same |
| #2065 | yes, fully | one `.cpp`, no observable behaviour change on any path that does not leak |
| #2066 | **no** — public base class or `TaskT` ownership | CCF-019 |
| #2067 | **no** — object-layout change to a public class | |
| #2068 | **no** — the returned map type is public | |
| #2069 | **no** — a public constructor gains a throw | |
| #2070 | **no** — evidence absent | |
| #2071 | **no** — public-surface addition | |

---

## 9. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` |
|---|---|---|---|---|
| #2063 | narrows accepted input on four public doors | none | none | none — no member is `noexcept` today |
| #2064 | narrows accepted input on two public statics | none | none | none |
| #2065 | none | none | none | none |
| #2066–#2071 | not implemented | — | — | — |

`sizeof`/`alignof` of every public type in the module must be unchanged by #2063–#2065 and are
`static_assert`ed by #2063's pin suite.

---

## 10. Observable semantic consequences

- **#2063** — four doors begin to throw for input they accepted: a header name or value, a
  request authority, or a reason phrase containing `\r`, `\n` or `\0`. A caller relying on
  passing such text through now gets an exception. This is a **deliberate narrowing** and needs
  a migration note.
- **#2064** — `parseUrl` begins to reject `host:80abc`, `host:-1`, `host:99999`;
  `http://host?q=1` begins to parse as host `host` + path `/?q=1` (**a value change, not a
  rejection**, and the more dangerous of the two to leave alone); the host is lowercased.
  `parseStatusLine` begins to reject a non-three-digit code and a malformed version token.
- **#2065** — none observable except that descriptors stop leaking.

---

## 11. Test matrix

| Ticket | Required cases |
|---|---|
| **#2063** | header name and value containing `\r`, `\n`, `\0`, and each at the start, middle and end; a lone `\r`; a lone `\n`; a legal value containing a space and a tab (must still be accepted); an empty value (accepted); a request authority with each control character; a reason phrase with each; the exact exception type and `paramName`; every existing `HttpClientTests` case still green |
| **#2064** | every row of §4.2's and §4.3's tables, in both directions; `host:0` and `host:65535` (accepted); `host:65536` (rejected); `[::1]:65535`; the empty port `host:` (already throws, unchanged); `http://host?q=1` → host `host`, path `/?q=1`; `http://host#f` → host `host`; `HOST.EXAMPLE` → `host.example`; `HTTP/1.1 200 OK` unchanged; `HTTP/1.0 204` accepted; `HTTP/1.1 099` rejected or accepted — **pin whichever, do not leave it unpinned** |
| **#2065** | the four failure modes of §4.7, each ×20, asserting the `/proc/self/fd` count is unchanged; the success path ×20, same assertion; the assertion must be skipped rather than failed where `/proc/self/fd` does not exist |
| **#2063 pins** | for every blocked ticket: response construction with `-1`/`1000`; `getHeader` case-sensitivity in both directions; sending one `HttpRequestMessage` twice succeeding; a `utf-16`-labelled UTF-8 payload emitting `c3 a9`; `sizeof` of the six public types |

---

## 12. Sanitizer matrix

| Ticket | ASan | UBSan | LSan | TSan |
|---|---|---|---|---|
| #2063 | not expected to fire — no memory defect | not expected | no | no |
| #2064 | no | **yes — `std::stoi` on an out-of-range port already throws, but the `-5` status code is cast into an enum**; check for an invalid-enum-value report | no | no |
| #2065 | no | no | **not applicable** — LSan tracks memory, not descriptors; the `/proc/self/fd` count **is** the instrument, and this must be stated rather than substituting a clean LSan run for it | no |
| #2066 | **yes, discriminating** — the audit's existing UAF reproduction | no | no | possibly |

Every sanitizer conclusion must prove the production body was instrumented: `Net.Http` is a
**static library**, so a probe that links `build/libsharp_runtime_net_http.a` gets an
**uninstrumented** body. Either rebuild the component into the sanitizer tree or compile the
`.cpp` into the probe directly, and say which.

---

## 13. Ticket split

### 13.1 Compatible, ready

| # | P | Size | Scope | Findings | Family |
|---|---|---|---|---|---|
| **#2065** | P1 | S | own the socket descriptor with RAII so a post-connect throw cannot leak it | SR-AUD-318 (leak half) | NH-C |
| **#2063** | P1 | M | reject control characters at four public doors; disclose and PIN every gated behaviour | SR-AUD-313, 316 (reason half) | NH-B |
| **#2064** | P2 | M | full-consumption `parseUrl`/`parseStatusLine` with real domain checks | SR-AUD-311, 312 | NH-A / CCF-002 |

### 13.2 Blocked on approval

| # | P | Scope | Findings |
|---|---|---|---|
| **#2066** | P1 | `HttpClient`'s async lambdas capture a raw `this` — CCF-019 | SR-AUD-310 |
| **#2067** | P2 | sent-state guard on `HttpRequestMessage` — object layout | SR-AUD-314 |
| **#2068** | P2 | case-insensitive header storage, and no duplicate default header — public map type | SR-AUD-315 |
| **#2069** | P2 | reject an out-of-domain status code — public ctor gains a throw | SR-AUD-316 (code half) |
| **#2071** | P2 | bounded response reads — public-surface addition | SR-AUD-318 (limits half) |

### 13.3 Deferred verification

| # | P | Scope | Findings |
|---|---|---|---|
| **#2070** | P3 | does .NET encode `StringContent` through the charset, or is the label advisory? | SR-AUD-317 |

---

## 14. Recommended execution order

1. **#2065** — the descriptor leak. Highest measured consequence, smallest blast radius, one
   `.cpp`, no public surface.
2. **#2063** — control characters and the disclosure/pin suite. Must precede #2064 because both
   touch `parseUrl` and the pins must exist before anything else moves.
3. **#2064** — the parsers.
4. Stop. #2066–#2071 need approval or evidence that does not exist.

---

## 15. Deferred evidence

`/rv/tmp/runtime/src/libraries/` is absent, re-verified 2026-08-04. The following are **not**
decided by this review and must not be guessed:

- whether .NET accepts `HTTP/1.1 099 OK` (#2064 pins whichever this port chooses, and says so);
- whether `HttpClient` lowercases the host of a request URI or leaves it to DNS (#2064 chooses
  lowercasing on the strength of RFC 3986 §3.2.2 and records it as a choice, not a match);
- .NET's exact exception type and `paramName` for a CR/LF-bearing header value — this port will
  use `System::FormatException` with the offending field named, and #2063 records that as this
  port's choice;
- whether `StringContent` encodes through the charset (#2070).

---

## 16. Exclusions

- TLS/HTTPS — a documented permanent deviation; `parseUrl`'s non-`http` rejection is correct.
- `modules/net-http-headers`, `modules/net-http-json`, `modules/net`, `modules/net-sockets` —
  separate components with their own findings.
- Connection pooling, keep-alive, redirects, proxies, HTTP/2 — absent by design, not defects.
- CNA and mobile-eggbert — not inspected; #1773 stays blocked.

---

## 17. Completion criteria

This review (#2062) is complete when this document exists, each of the nine open findings has
exactly one disposition in §4, and §13's tickets are in `plan.sqlite3`. **It is complete on
those terms and remediates nothing by itself.**

`System::Net::Http` is closed for *compatible* work when:

1. #2063, #2064 and #2065 are `done`;
2. SR-AUD-311, 312, 313 are `remediated`, and SR-AUD-316 and SR-AUD-318 carry a split record —
   half remediated, half blocked — in the index and in their per-file reports;
3. SR-AUD-310, 314, 315 are `confirmed (design-complete)` with a blocked ticket and a pin each;
4. SR-AUD-317 carries a deferred-verification ticket and a pin;
5. the repository gate shows no new failure and `SharpRuntimeTests_Net_Http` has grown,
   add-only;
6. the descriptor-leak measurement of §4.7 reads **0** for all five modes.

---

## 18. Promotion rule for family NH-B

If `Net.Http.Headers` (SR-AUD-319, SR-AUD-322) or `Net.WebSockets` (SR-AUD-248) is reviewed and
finds the same shape — *a control character crossing a public door into a protocol frame* —
mint **CCF-021** then, citing all three modules. Do not mint it from this module's evidence
alone.

---

## 19. What this review deliberately did not do

- It did not re-run SR-AUD-310's ASan reproduction. The finding is not in dispute and the
  ticket is blocked either way; re-running costs a sanitizer build for no decision.
- It did not inspect `modules/net-http-headers`, although §5.2 shows the two modules share a
  root cause. Scoping to one component is what makes the ticket queue executable.
- It did not implement anything. Ticket #2062 is the review.

---

## 20. Implementation record — corrections made while implementing

Appended rather than folded into the sections above, so the difference between what the review
predicted and what implementation measured stays visible.

### 20.1 #2065 landed exactly as §4.7 specified, and the sanitizer prediction held

The repair is a file-local `SocketGuard` in `HttpClientHandler.cpp`. Measured after:

| Server response | Requests | Threw | Leaked before | Leaked after |
|---|---:|---:|---:|---:|
| well-formed | 20 | 0 | 0 | **0** |
| garbled status line | 20 | 20 | 20 | **0** |
| `Content-Length: abc` | 20 | 20 | 20 | **0** |
| chunk size `ZZZ` | 20 | 20 | 20 | **0** |
| body shorter than `Content-Length` | 20 | 20 | 20 | **0** |

§12 predicted that **LSan would not cover this** and that the descriptor count would have to be
the instrument. That prediction is **correct** and is recorded as a confirmed non-result rather
than dropped: LSan tracks memory, not descriptors, and a clean LSan run says nothing about a
leaked socket. ASan/UBSan/LSan were still run, with `HttpClientHandler.cpp` and `HttpClient.cpp`
compiled **from source** into the probe — §12's warning about the `STATIC` component was
load-bearing, since linking `libsharp_runtime_net_http.a` would have measured an uninstrumented
body — and are clean in all five modes.

**One design point the review did not state, decided while implementing.** The guard keeps the
**original** close point on the success path: `Close()` is called exactly where
`platformClose(fd)` used to be, and is idempotent, so the destructor is a no-op afterwards. The
alternative — deleting the explicit call and letting the destructor close at the end of the
block — would have held the socket open across response construction and cookie handling. That
is not observably wrong, but it is a change nobody asked for, and the mutation check is sharper
without it: emptying the destructor fails exactly the four failure-path tests, each with the
pre-repair count of 19, while the success-path test stays green. An implementation that relied
on the destructor for both would have failed all five and proved less.

### 20.2 The test asserts the failure count, not only the descriptor delta

Each of the five regressions asserts **both** that the descriptor delta is 0 **and** that the
expected number of requests actually threw. Without the second assertion a mode that quietly
stopped failing — because some future change made the malformed response acceptable — would
report a passing descriptor count over a path that was never exercised. The success-path test
carries the mirror assertion, `threw == 0`.

The tests **skip** rather than fail where `/proc/self/fd` does not exist. A missing instrument
is not a passing measurement, and this is Linux-only by construction.

### 20.3 #2063 — SR-AUD-313 has **ten** public doors, not the four §4.1 named

The single largest correction this implementation made. §4.1 scoped #2063 to *"a header name
and value, a request authority, and a reason phrase"*. Measured against `257106a` with
`build-probe/2063_probe1_doors.cpp` (log `2063_probe1_before.log`), **every** door named by
SR-AUD-313's own audit text was open, and one the audit does not name is worse than any of
them:

| # | Door | Open before? | In §4.1's list? |
|---|---|---|---|
| 1 | `HttpRequestMessage::setHeader` name and value | yes | yes |
| 2 | `HttpResponseMessage::setHeader` name and value | yes | no — symmetric, added |
| 3 | `HttpClient::setDefaultHeader` name and value | yes | no — merged onto every request |
| 4 | `HttpResponseMessage::setReasonPhraseProperty` | yes | yes |
| 5 | `parseUrl` — the **authority** | yes | yes |
| 6 | `parseUrl` — the **path** | yes | **no, and it is request smuggling** |
| 7 | `parseStatusLine` — the whole line | yes | no |
| 8 | `StringContent` charset + media type; `ByteArrayContent`/`ReadOnlyMemoryContent`/`StreamContent` media type | yes | no — **named by the audit**, missed by the review |
| 9 | `MultipartContent` subtype | yes | no — **named by the audit** |
| 10 | `MultipartFormDataContent::Add` name and file name | yes | no — **named by the audit** |

**Door 6 is the load-bearing correction.** `HttpClientHandler::Send` writes
`method << " " << purl.path << " HTTP/1.1\r\n"`, so
`parseUrl("http://host/pa\r\nX: y")` returning path `"/pa\r\nX: y"` puts attacker text in the
request **line**. That is a second request, not a second header field. Had #2063 been
implemented to §4.1's list it would have closed the authority and left this open, which is
exactly the failure mode §4.1 itself was written to prevent one step earlier.

**Doors 8–10 were named by the finding and dropped by the review.** SR-AUD-313's text reads
*"content type/charset, multipart subtype, and form-data name/file name take the same
unvalidated serialization path… The direct multipart probe emits separately parsed injected
fields"*. The review's §4.1 paraphrased that as "header, media-type and disposition
concatenation" and then scoped the ticket to headers. Measured, `MultipartFormDataContent`
with a CR/LF name emits
`Content-Disposition: form-data; name="na\r\nX-Injected: yes"` — the audit's claim, reproduced
exactly. **Closing only the four §4.1 doors would have left SR-AUD-313 marked remediated with
three of its own named vectors still open.**

**The repair validates the whole URL string once**, at the top of `parseUrl`, rather than the
parsed components. That closes doors 5 and 6 in one rule *and* makes #2064's
authority/query/fragment re-split safe by construction: no later regrouping of those bytes can
reintroduce a control character the whole string does not contain.

**Three exception types, for stated reasons** (`docs/Migration-HttpControlCharacterRejection.md`
§4): `System::FormatException` for the protocol-field doors; `System::UriFormatException`
(which **is** a `FormatException`) for `parseUrl`, matching what it already throws;
`HttpRequestException` for `parseStatusLine` and for the handler's response-header-line check,
because a malformed **response** is this module's response error rather than the caller's
format error. `System::ArgumentException` for the two multipart doors, because the same
parameters already report their other defects that way. §15's record stands: .NET's exact type
is **not** known here, and these are recorded as this port's choices.

**Rejected text is not echoed into the exception message.** It is attacker-controlled and
these messages get logged; echoing a CR/LF-bearing value into a log recreates the injection
the rejection exists to prevent. The messages name the **field**, not the value.

**Sanitizers.** ASan/UBSan/LSan clean over 130 rejections and 19 acceptances, with
`HttpClient.cpp`, `HttpClientHandler.cpp`, `MultipartContent.cpp` and
`MultipartFormDataContent.cpp` compiled **from source** into the probe — §12's warning about
the `STATIC` component applies here exactly as it did to #2065 — and a control
heap-buffer-overflow proving the instrumentation is live
(`build-probe/2063_probe3_asan.log`). §12 predicted no sanitizer finding for #2063 and that
prediction **held**; this is recorded as a **non-discriminating confirmation**, not as proof
of a repair.

### 20.4 #2063's pin suite, and the one pin §11 asked for that cannot exist as asked

§11's last row asks for a pin per blocked ticket "for #2066, #2067, #2068, #2069, #2070 and
#2071". **#2066 is different in kind**: its subject is a use-after-free, and §4.6 already says
so — *"a use-after-free is not a behaviour to pin, it is a defect to fix"*. A test that
exercised it would have no defined meaning. What #2066 **must** change and what therefore
**is** pinnable is the *ownership model*, so #2066's pin is a `static_assert` that `HttpClient`
does **not** derive from `std::enable_shared_from_this<HttpClient>`. The ticket's acceptance
criterion and §4.6 are both satisfied, and no undefined behaviour is pinned.

**Object layout is pinned against a probe struct, not a byte count.** The Buffers review's
pins could assert `sizeof(T) == 32` because those types hold no `std::string`. Every public
type here does, and `sizeof(std::string)` is 32 on libstdc++ and 24 on libc++, so a literal
byte count would be a portability trap for the MinGW/Emscripten/Apple-Clang builds the
platform policy requires to keep compiling. The pin instead asserts
`sizeof(HttpRequestMessage) == sizeof(HttpRequestMessageLayoutProbe)`, a struct with the same
declared members — exact on every standard library. Measured on the verified Linux/GCC
baseline and recorded here: `HttpClient` 104, `HttpRequestMessage` 192, `HttpResponseMessage`
112, `StringContent` 104, `ByteArrayContent` 64, `MultipartContent` 96, `HttpMethod` 32.

**Every pin is mutation-checked.** Nine mutations, each applied and then reverted from an exact
backup with `git diff --stat` identical on both sides and no `MUTATION` marker surviving:

| # | Mutation | Expected | Measured |
|---|---|---|---|
| M1 | `ContainsProtocolControlCharacter` returns `false` | the rejection tests fail, the acceptance tests do not | **exactly 13** rejection tests failed; all 8 acceptance tests and all 7 pins stayed green |
| M2 | `HttpClient : std::enable_shared_from_this<HttpClient>` | #2066's pin fails | **compile error**, that static_assert |
| M3 | `bool sentAlready_` added to `HttpRequestMessage` | the layout pin fails | **compile error**, that static_assert |
| M4 | a sent-state guard stored in `HttpRequestOptions` (**no** layout change, so only the behaviour pin can react) | `Pin2067` fails | failed |
| M5 | `HttpResponseMessage::getHeader` made case-insensitive | `Pin2068_HeaderMapsAreCaseSensitive` fails | failed |
| M6 | the handler skips its own `Host` when the caller set one | `Pin2068_HandlerEmitsADuplicateDefaultHeader` fails | failed |
| M7 | the response constructor rejects a code outside 100–599 | `Pin2069` fails | failed |
| M8 | `StringContent::ReadAsByteArray` encodes for `charset == "utf-16"` | `Pin2070` fails | failed |
| M9 | `recvExact` capped at 64 KiB | `Pin2071` fails | failed (see below) |

M4–M9 were applied together in one build; **exactly** the six expected pins reacted and no
other test in the 165 did, which is what makes the attribution sound. M4 is deliberately
layout-neutral: had it added the member, M3's static_assert would have stopped the build and
the *behavioural* pin would never have been exercised.

**One test defect M9 exposed, and fixed.** Under M9 `Pin2071` aborted the whole executable
rather than failing: the client threw, `serverThread.join()` was skipped, and `std::thread`'s
destructor called `std::terminate`. An approved #2071 would have hit exactly that and hidden
every other result behind one abort. The test now joins in all paths and asserts the absence
of a throw explicitly.

### 20.5 Measured behaviours #2063 did **not** change, recorded rather than fixed

All from `build-probe/2063_probe1_before.log`, all still true after #2063, none of them CR, LF
or NUL:

- `parseUrl("http://host/p q")` → path `"/p q"`. A **space** in the request target also breaks
  the request line (`GET /p q HTTP/1.1`), but it is not a frame terminator and no repository
  evidence pins .NET's handling. Recorded, not changed.
- `parseUrl("http://user@host/p")` → host `"user@host"`, which then goes to DNS. Userinfo
  without a password is accepted; with one it already throws (`invalid port`, incidentally).
  Not in §4.2's row list; no evidence for .NET's behaviour. Recorded as ticket **#2072**,
  P3, **deferred**.
- `parseUrl("http://[::1]x/p")` → host `"::1"`, the junk after the bracket silently dropped.
  Recorded as part of **#2072**.
- `parseUrl("http://host: 80/p")` and `"http://host:+80/p"` → port 80. `std::stoi` skips
  leading whitespace and accepts a sign. **This one is #2064's**, not a separate ticket.

### 20.6 #2064 landed every row of §4.2 and §4.3, and §12's sanitizer prediction was **wrong**

Both parsers now route their numeric fields through one file-local
`tryParseWholeUnsignedField(text, maxDigits, maxValue, out)`: the **entire** text must be one
to `maxDigits` ASCII digits and nothing else, and the value must fall inside `[0, maxValue]`.
That is CCF-002's remedy shape — the full-consumption scanner already applied in
`System::DateTime`, `TimeOnly` and `XmlConvert` — reduced to what this module needs. No family
was minted; **#2064 cites CCF-002**, as §5.1 said it would.

Measured before and after with `build-probe/2063_probe1_doors.cpp`
(`2063_probe2_after_2063.log` → `2064_probe1_after.log`). Every §4.2 and §4.3 row moved, and
nothing else did:

| Input | Before | After |
|---|---|---|
| `http://host:80abc` | port 80 | rejected |
| `http://host:-1/p` | port −1 | rejected |
| `http://host:65536/p`, `:99999` | accepted | rejected |
| `http://host: 80/p`, `:+80` | port 80 | rejected — `std::stoi` skipped the space and took the sign |
| `http://host:0`, `:65535` | accepted | **still accepted** |
| `http://[::1]:70000`, `[::1]:80abc` | accepted | rejected |
| `http://host?q=1` | host `host?q=1`, path `/` | host `host`, path `/?q=1` |
| `http://host#frag` | host `host#frag` | host `host`, path `/` |
| `http://host/p#frag` | path `/p#frag` | path `/p` |
| `http://HOST.EXAMPLE/p` | host `HOST.EXAMPLE` | host `host.example` |
| `http://[2001:DB8::1]/p` | host `2001:DB8::1` | host `2001:db8::1` |
| `HTTP/1.1 200trailer OK`, ` 2 `, ` 20 `, ` -5 `, ` +5 `, ` 99999 ` | parsed | rejected |
| `GARBAGE 200 OK`, `HTTP/11 …`, `HTTP/1.1x …`, `HTTP/1.10 …` | code 200 | rejected |
| `HTTP/1.1 200 OK`, `HTTP/1.0 204`, `HTTP/1.1  200 OK` (double space) | unchanged | **unchanged** |

**Not one existing test needed updating.** §13's ticket allowed changing an existing parser
case "with a recorded reason"; none was required, including
`QueryStringPreserved`, whose `http://example.com/search?q=hello` keeps path
`/search?q=hello` because the authority ends at the `/` that precedes the query.

**Two decisions §15 required to be pinned rather than guessed, both taken and both pinned:**

- **`HTTP/1.1 099 OK` is ACCEPTED, as the code 99.** RFC 9112 §4's grammar is `3DIGIT`, and
  `099` is three digits. Pinned by
  `HttpClientStatusLineParseTests.LeadingZeroThreeDigitCodeIsAcceptedAsThatNumber`.
- **`HTTP/9.9 200 OK` is ACCEPTED.** The token satisfies `HTTP/<digit>.<digit>`; a version this
  port does not speak is the server's behaviour to report, not a parse error. Pinned by
  `UnknownButWellFormedVersionIsAccepted`.

Neither is claimed as a .NET match. `/rv/tmp/runtime/` is absent and both are recorded as this
port's choices, exactly as §15 requires.

**One extension of §4.2's rule, applied consistently.** §4.2 lists only `http://host#frag` →
path `/`, i.e. the authority-only fragment. A fragment is client-side only (RFC 9110 §7.1) and
is **never** part of a request target, so it is dropped after a path too: `http://host/p#frag`
→ `/p`. Leaving it in would have written `GET /p#frag HTTP/1.1` on the wire. This changes no
acceptance decision, only a parsed value, and it is the same rule §4.2 already sanctions for
the other position.

**§12's sanitizer prediction for #2064 was wrong, and the correction is recorded rather than
quietly dropped.** §12 says: *"UBSan — **yes**… the `-5` status code is cast into an enum;
check for an invalid-enum-value report."* Measured with a dedicated discrimination control
(`build-probe/2064_probe3_enumctl.cpp`, log `2064_probe3_enumctl.log`): casting `-5` into
`System::Net::HttpStatusCode` under `-fsanitize=enum -fno-sanitize-recover=undefined`
**reports nothing and exits 0**. `HttpStatusCode` is an `enum class` with the implicit `int`
underlying type, so every `int` is inside its value range and there is no undefined behaviour
to detect. **UBSan is therefore NOT a discriminating instrument for this defect**, and the
clean UBSan run over the repaired parser must not be offered as evidence that it is fixed —
the behavioural test `NoStatusCodeCanEscapeTheEnumDomain` is. The defect was real; the
instrument §12 nominated was not the one that could see it.

ASan/UBSan(+`enum`)/LSan are nonetheless clean over 26 parsed and 42 rejected inputs with
`HttpClient.cpp` compiled **from source** and a control heap-buffer-overflow proving
instrumentation (`build-probe/2064_probe2_asan.log`) — recorded as a **non-discriminating
confirmation**.

**Seven mutations, each applied and reverted from an exact backup**, `git diff --stat`
identical on both sides and no `MUTATION` marker surviving:

| # | Mutation | Measured |
|---|---|---|
| N1 | the shared parser `break`s at the first non-digit instead of failing (i.e. `std::stoi` again) | 4 tests fail: both port-domain tests, `StatusCodeMustBeExactlyThreeDigits`, and the pre-existing `NonNumericStatusCode_ThrowsHttpRequestException` |
| N2 | the version-token check is disabled | `VersionTokenMustBeHttpDigitDotDigit` and the end-to-end `NonHttpStatusLineIsRejectedAndLeaksNoDescriptor` |
| N3 | the authority ends at the first `/` again | `AuthorityEndsAtTheFirstSlashQuestionOrHash` and the end-to-end `AuthorityOnlyQueryReachesTheWireAsTheRequestTarget` |
| N4 | `toLowerAscii` returns its argument unchanged | `HostIsLowercasedLikeTheSchemeAlreadyWas` and the interaction test |
| N5 | the fragment is kept in the request target | `FragmentIsNeverPartOfTheRequestTarget` and the interaction test |
| N6 | #2063's control check validates only the parsed **host**, at the end, instead of the whole URL at the start | 4 tests, including `ParseUrl_ControlCharacterInAnyComponent` and `ControlCharacterIsRejectedBeforeAnySplitting` — this is the mutation that proves the two tickets' repairs are **ordered**, not merely co-present |
| N7 | the exactly-three-digits requirement is dropped (1–3 digits accepted) | `StatusCodeMustBeExactlyThreeDigits` |

**Two mutation-harness defects found and fixed, both in the tests this batch added.** N4's
first form deleted the call to `toLowerAscii` and made the function unused, so `-Werror`
stopped the build and a **stale binary** was run — the first N3/N4 readings were consequently
wrong and were re-measured. And N3's first run **hung**: the client never connected, so the
mock server sat in `Accept()` and `join()` never returned. Every loopback test this batch
added now (a) runs its server body inside a `try`/`catch`, (b) catches the client's exception
so `join()` is always reached, and (c) makes a throwaway `pokeMockServer(port)` connection
when the client failed, so `Accept()` returns. Between them a mutated build **fails** instead
of hanging or aborting. A mutation check that hangs proves nothing about the test it was
aimed at.

### 20.7 Completion reconciliation — `System::Net::Http` is closed for compatible work

Re-derived from `audit/AUDIT_FINDINGS_INDEX.md` after #2064, not carried forward from §4.
**Nine findings own this namespace** — SR-AUD-310…318, every one linking to a file under
`modules/net-http/`. One further row, SR-AUD-236, names `HttpContentJsonExtensions.hpp` but is
owned by `modules/net-http-json`, a separate component §16 excludes; it is **not** counted
here and nothing about it changed.

Every one of the nine has **exactly one** disposition:

| Disposition | Count | Findings | Evidence |
|---|---:|---|---|
| `remediated` | 3 | 311, 312 (#2064), 313 (#2063) | permanent regressions + mutation checks + sanitizers |
| **split**: half `remediated`, half `confirmed` + blocked | 2 | 316 (reason half #2063 / code half #2069), 318 (leak half #2065 / limits half #2071) | both halves recorded in the index row and the per-file report |
| `confirmed (design-complete)` + blocked ticket + mutation-checked pin | 2 | 314 (#2067), 315 (#2068) | one named repair each, blocked on a public change |
| `confirmed`, blocked, **not** design-complete | 1 | 310 (#2066, CCF-019) | two competing options, no selection |
| `confirmed`, deferred verification + pin | 1 | 317 (#2070) | evidence absent; `/rv/tmp/runtime/` re-verified missing |

**Correction to §17's completion criterion 3.** That criterion asks for SR-AUD-310, 314 and
315 to be `confirmed (design-complete)`. Applied to 314 and 315, whose repair the review names
**singularly** — a sent-state member, a case-insensitive comparator — each blocked on a stated
public change. **Not applied to 310**: §4.6 records *two* competing options
(`enable_shared_from_this` on `HttpClient`, or ownership semantics on `TaskT`) with no
selection, and §19 says the review deliberately did not re-run its reproduction. That is a
disposition, not a completed design, and marking it design-complete would overstate what
exists. 310 stays plain `confirmed`, blocked as #2066, with an ownership-model pin.

**Every gated behaviour is pinned, and every pin is mutation-checked** (§20.4's table): #2066
by a compile-time ownership-model `static_assert`, #2067 by a behavioural pin **and** a layout
`static_assert`, #2068 by two behavioural pins **and** two return-type `static_assert`s, #2069,
#2070 and #2071 by one behavioural pin each.

**Verified against §17's other criteria:**

1. #2063, #2064 and #2065 are `done`. ✔
2. 311, 312, 313 are `remediated`; 316 and 318 carry split records in both the index and their
   per-file reports. ✔
4. 317 carries a deferred-verification ticket and a pin. ✔
5. `SharpRuntimeTests_Net_Http` **137 → 181**, add-only; no test was removed, weakened,
   skipped, disabled or recategorised. ✔
6. The descriptor-leak measurement reads **0** for all five modes, re-run as part of the
   #2064 end-to-end test that adds a sixth mode (a `GARBAGE` status line). ✔

**No compatible ticket remains.** The queue is #2066 (blocked), #2067 (blocked), #2068
(blocked), #2069 (blocked), #2071 (blocked), #2070 (deferred verification) and #2072 (deferred
verification, post-audit, no `SR-AUD-*` identifier). **`System::Net::Http` is complete except
for gated and deferred work.** Nothing blocked was implemented and nothing blocked is marked
remediated.

**Two cross-reference corrections to this document's own text**, recorded rather than silently
edited: §1 item 4 and §2 both point at "§21" for the NH-B promotion rule, which is **§18**;
there is no §21. And §1's closing sentence says `modules/net-websockets` "should be judged
against whatever **#2063** concludes" — the ticket meant is **#2066**, the CCF-019 ownership
ticket, since the shared shape is SR-AUD-247's use-after-free, not the control-character
family. Neither error changed any decision.

**`modules/xml` remains the recommended next unit**, and §22 records that recommendation
re-derived by measurement rather than inherited.

### 20.8 #2107 — the descriptor INSTRUMENT was defective in BOTH directions, and the ticket named only one

Ticket **#2107** was filed by the `#2089/#2091/#2097` batch after
`HttpClientDescriptorLeakTests` failed once under `local_ci_check.sh` with a delta of **−1** —
*fewer* descriptors after than before, which cannot represent a leak. It is a **test-instrument**
ticket, not a production one: nothing about the guarantees #2063 and #2065 established was ever
in doubt.

**The flake could not be reproduced by load in this container.** 15 isolated runs, 3 full-suite
runs and 12 runs under six spinning CPU hogs all passed. Rather than hunt a 2-in-12 race, the
mechanism was proven **deterministically** by reproducing the instrument exactly and injecting a
server-thread delay (`build-probe/2107_probe1_instrument.cpp`, log
`build-probe/2107_probe1_before.log`). Measured, 3 runs per mode, perfectly repeatable:

| Injected server lag | `before` | `after` (pre-join) | delta | What it means |
|---|---:|---:|---:|---|
| none | 5 | 5 | **0** | the everyday case |
| on the **warm-up** connection | **6** | 5 | **−1** | the baseline is inflated — **the symptom observed in the wild** |
| on the **last** connection | 5 | **6** | **+1** | **a FALSE LEAK REPORT** |

**Two corrections to the ticket's own premise:**

1. **The `+1` direction was never recorded, and it is the more dangerous one.** A `−1` is
   obviously absurd and gets investigated. A `+1` looks exactly like a real descriptor leak and
   would send a maintainer hunting through `HttpClientHandler` for a defect that does not exist.
2. **The ticket's stated root cause names only HALF the fix.** It says the `after` sample is
   taken before `join()`. True, and joining first removes the `+1` completely — but the probe
   shows the **`−1` survives it**, because that direction corrupts the **baseline**, not the
   final sample. A repair that only moved the `join()` would have left the exact symptom that was
   reported. Both ends have to be quiesced.

**The repair**, in `descriptorDeltaOver`:

- the mock server increments an atomic after each `server->Close()`;
- the **baseline** is taken only once the server has closed the **warm-up** connection — waiting
  on that *event*, not on a duration. A deadline exists purely so a wedged server thread **fails**
  the test rather than hanging it, and is never what the measurement relies on. A sleep long
  enough to "usually" work is exactly the fix this ticket forbids;
- the **final** sample is taken **after `serverThread.join()`**;
- the return type became `DescriptorMeasurement`, reporting **both endpoints**, because a bare
  delta cannot distinguish "nothing leaked" from "the instrument measured the wrong thing";
- `expectNoDescriptorLeak` reports a **negative delta as an INSTRUMENT FAULT by name**, rather
  than passing an `EXPECT_LE(0, delta)` or failing as though it were a leak;
- the server lambda now **swallows its own exceptions**. It did not, so an `Accept()` that threw
  would have escaped a `std::thread` and called `std::terminate` — a second latent instrument
  defect, found while reading and repaired here. The file's own comment at the `pokeMockServer`
  helper already *prescribed* this ("pair it with a server lambda that swallows its own
  exceptions"); this lambda simply never did.

**Discrimination proof, all three parts required by the ticket:**

- **A timing variation no longer creates a false result.** The repaired instrument, under the
  *same* injected delays that deterministically produced −1 and +1, gives delta **0** in all
  three modes, 3 runs each (`build-probe/2107_probe2_after.log`).
- **A genuine leak is still detected.** #2065's **original** mutation — emptying the
  `SocketGuard` destructor — was re-applied and re-run against the repaired instrument. The four
  failure-path tests and the sixth call site fail with the **pre-repair count of exactly +19**,
  and `TheSuccessPathStillClosesItsDescriptor` stays **green**, which is precisely the signature
  #2065 recorded. The failure message now also names both endpoints ("descriptor count went from
  6 to 25"). The production tree was restored from an exact backup, md5-identical, with an empty
  `git diff`.
- **Stability.** **50/50 runs passed under six spinning CPU hogs**, plus 3 clean full-executable
  runs afterwards.

**No production code was changed to make the instrument stable** — the ticket's explicit
prohibition. The only files touched are the test file and the probes.

**One incidental finding, recorded not ticketed:** running four copies of this suite
concurrently **deadlocks**. Each copy's mock server does a fixed number of `Accept()` calls and
`join()`s unconditionally, so a copy whose client is starved leaves its server blocked in
`Accept()` forever. That is a property of the whole mock-server pattern in this file, not of
`descriptorDeltaOver`, and it only bites a use nothing in the repository actually makes —
`ctest` runs each executable once. Recorded so that a future attempt to parallelise inside one
executable knows about it.
