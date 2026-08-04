<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Net` namespace review and remediation plan

Ticket **#2034**, written 2026-08-04 on branch
`feature/remediation-batch-approval-packages-next-review`.

The seventh namespace review in the post-audit remediation programme, after `System::Threading`
(#1950), `System::Threading::Tasks`/`Channels` (#1964), `System::Runtime` (#1972), `System::Uri`
(#1987), `System::Text` (#2006) and `System::Diagnostics` (#2023). Same contract: **every confirmed
finding in the namespace gets exactly one disposition, no finding disappears between the audit
index and this plan, and every premise is re-measured against the shipped library before it is
relied upon.**

**Nothing in §§1–15 is implemented by writing them.** The measured before-matrix is
`build-probe/2034_probe1_net_before.log` (source `..._net_before.cpp`) plus
`build-probe/2034_probe2_asan.log` for the memory-safety half. **All ten premises reproduced**;
§4 records the four the audit understates or mis-states.

**No `SR-AUD-*` identifier is issued by this review.** Audit numbering stays frozen at **364**.

---

## 1. Why `System::Net` is next — the selection, re-derived

Re-derived by measurement over `audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-04, **not** inherited
from the previous handoff (which named `modules/buffers` and `modules/xml`). Every un-reviewed
module with ≥ 4 open findings, plus every module with a ≥ 25 % high ratio:

| Module / namespace | Open | high | med | low | high % | Existing plan? | Reviewed? | Public deps | Cohesion |
|---|---|---|---|---|---|---|---|---|---|
| `modules/core` | 72 | 9 | 59 | 4 | 13 % | many family plans | partly, by family | — | **poor** — not a namespace; `System::` spans String, Convert, DateTime, Decimal, Math, Guid, Span, Exception |
| `modules/threading` | 17 | 6 | 11 | 0 | 35 % | **yes** | **yes** (#1950) | — | — |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7 % | **yes** | **yes** (#1972) | — | — |
| `modules/buffers` | 11 | 3 | 8 | 0 | 27 % | **partial** — `Base64FamilyPlan.md` | no | Core.Base | good |
| `modules/io` | 11 | **0** | 11 | 0 | **0 %** | partial ×2 | no | Core.Base, Uri | medium |
| `modules/text` | 11 | 1 | 10 | 0 | 9 % | **yes** | **yes** (#2006) | — | — |
| `modules/uri` | 10 | 0 | 10 | 0 | 0 % | **yes** | **yes** (#1987) | — | — |
| **`modules/net`** | **10** | **3** | **7** | 0 | **30 %** | **no** | **no** | Collections.Core, ComponentModel, Core.Base, Uri | **good** — one module, one namespace |
| `modules/net-http` | 9 | 2 | 7 | 0 | 22 % | no | no | — | separate namespace |
| `modules/xml` | 8 | 2 | 6 | 0 | 25 % | no | no | Core.Base, Uri | medium (+4 in `xml-linq`) |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14 % | no | no | Core.Base | good, but needs ICU data absent here |
| `modules/time-zone` | 7 | **0** | 7 | 0 | 0 % | no | no | Core.Base | good |
| `modules/text-json` | 7 | 1 | 6 | 0 | 14 % | no | no | Core.Base, Text | good |
| `modules/net-websockets` | 6 | 2 | 4 | 0 | 33 % | no | no | — | separate namespace, only 6 |
| `modules/security-cryptography` | 2 | 2 | 0 | 0 | 100 % | no | no | — | **mostly out of scope** by `CLAUDE.md`'s permanent deviation |

`System::Net` wins on the rule the previous six reviews used:

- **Severity, and the severity's character.** 30 % `high` — the highest of any un-reviewed
  namespace with more than six findings. Two of the three highs are **memory-safety**
  (a heap-buffer-overflow read on a public method; an unchecked signed→`size_t` index into a
  `std::vector`), and the third is a **security** defect: a cookie supplied by one origin is stored
  and later emitted for an unrelated domain. That is the highest-consequence single finding in any
  un-reviewed namespace.
- **No existing plan at all.** `docs/` contains no `Net` document, and no open ticket referenced
  SR-AUD-300 … SR-AUD-309 before this review. `modules/buffers`, its nearest competitor on the
  numbers, is **partially covered** by `docs/Base64FamilyPlan.md`.
- **Dependency readiness.** Its four public dependencies are `Collections.Core`, `ComponentModel`,
  `Core.Base` and **`Uri`** — and `System::Uri` was reviewed and repaired only days ago
  (#1987–#1994, #2000–#2005). Three of this namespace's ten findings parse URI-shaped text, so the
  dependency being freshly correct is a positive, not a cost.
- **Not blocked.** **#1962 is `modules/net-network-information`** (`Ping`), a different module and
  a different namespace, so it does not gate anything here — verified, not assumed.
- **Cohesion.** One module, one namespace. The eight sibling `Net` modules are *different
  namespaces* (`System::Net::Http`, `::Sockets`, `::WebSockets`, …), so the "the Net family spans
  eight modules" objection recorded by the #2023 review is about the **family**, not about this
  namespace — the same distinction `System::Text` made when it owned one of three components.
- **A useful compatible queue.** Measured, **five** of the eight repairs need no approval (§7.1),
  including both memory-safety defects.

Not chosen, with reasons recorded so they are not re-litigated: `modules/core` is not a namespace
and is already being drained by cross-cutting families; `modules/io` has **zero** high findings;
`modules/buffers` has a partial plan and its findings are lower-consequence than a cross-origin
cookie leak (it is the recommended **next** review, §13); `modules/time-zone` has zero highs;
`modules/globalization` needs culture/ICU data this container does not have;
`modules/security-cryptography`'s two findings sit inside `CLAUDE.md`'s permanent
out-of-scope deviation.

---

## 2. Scope and file inventory

Measured. `modules/net` is a `STATIC` target,
`PUBLIC_DEPENDENCIES Collections.Core ComponentModel Core.Base Uri`, with a platform `SETUP`
function and no private or test dependency.

### 2.1 Public headers (29 files) and implementation (10 files, 1,974 lines)

| File | Impl lines | Public surface | Findings |
|---|---|---|---|
| `IPAddress.hpp` / `.cpp` | 495 | ctors (uint32, byte vector, 16-byte + scope), `Parse`/`TryParse`, `GetAddressBytes`, `ScopeId`, `MapToIPv4`/`v6`, statics `Any`/`Loopback`/`Broadcast`/`None` | **301** |
| `IPEndPoint.hpp` / `.cpp` | 136 | ctors, `Address`, `Port`, `Parse`/`TryParse`, `ToString`, `Serialize`/`Create` | **302** |
| `IPNetwork.hpp` / `.cpp` | 104 | ctor, `BaseAddress`, `PrefixLength`, `Contains`, `Parse`/`TryParse` | **303** |
| `SocketAddress.hpp` / `.cpp` | 135 | ctors (family+size, `IPAddress`+port), `Family`, `Size`, indexer, `GetIPEndPoint`, `ToString` | **300** |
| `Dns.hpp` / `.cpp` | 257 | 2 × `GetHostAddresses`, 3 × `GetHostEntry`, `GetHostName` | **304** |
| `Cookie.hpp` | header-only | 4 ctors, `Name`/`Value`/`Domain`/`Path`/`Expires`/`Secure`/`HttpOnly`/`Expired`, `DomainImplicit`/`PathImplicit` | **306** |
| `CookieCollection.hpp` | header-only | `Add`, `Count`, 2 × `operator[]`, iteration | **307** |
| `CookieContainer.hpp` / `.cpp` | 150 | 2 × `Add`, `GetCookieHeader`, `GetCookies`, `Count`, `SetCookies` | **305**, **308** |
| `WebUtility.hpp` | header-only | `HtmlEncode`/`HtmlDecode`, `UrlEncode`/`UrlDecode` | **309** |
| `WebHeaderCollection`, `WebProxy`, `CredentialCache`, `DnsEndPoint`, `NetworkCredential`, `IPHostEntry`, the enums and the exception types | 617 | — | — |

`Cookie`, `CookieCollection` and `WebUtility` are **fully header-inline**, which is the central ABI
fact of §8: a repair to any of the three makes every consumer **recompile**, and any data-member
change there is an **object-layout change**. Everything else has an out-of-line body and is
relink-only.

### 2.2 Tests (6 files, 1,635 lines, one executable)

`SharpRuntimeTests_Net`: **240 tests** measured 2026-08-04 — `NetTests.cpp` (885), `DnsTests.cpp`
(175), `CredentialCacheTests.cpp` (154), `WebProxyTests.cpp` (146), `EndPointTests.cpp` (139),
`WebHeaderCollectionTests.cpp` (136). No test covers an undersized `SocketAddress`, an invalid
collection index, a cross-origin cookie domain, or a scope id outside `uint32`.

### 2.3 Cross-module callers — unlike `System::Diagnostics`, these exist

Measured by grep over `modules/` and `tests/`. The finding-bearing types are used by
**`modules/net-sockets`** (6 header sites + 2 source + 2 test), **`modules/net-network-information`**
(2 header + 1 source), **`modules/net-websockets`**, **`modules/net-http`**,
**`modules/net-http-json`** and `tests/integration`. So every narrowing here has a real
in-repository blast radius and each ticket must run the affected sibling suites, not only
`SharpRuntimeTests_Net`.

---

## 3. Confirmed finding inventory — all 10, with the measured current behaviour

Every row re-measured 2026-08-04. "Measured" quotes `2034_probe1_net_before.log`, or
`2034_probe2_asan.log` where marked.

| ID | Sev | Cause | Measured now | Disposition |
|---|---|---|---|---|
| **300** | high | **N-A** | `SocketAddress(InterNetwork, 2).GetIPEndPoint()` → **ASan `heap-buffer-overflow` READ at `SocketAddress.cpp:106`, 0 bytes after a 2-byte region**; an `AddressFamily::Unix` buffer decodes as `0.0.0.0:0`; `SocketAddress(Loopback, 70000)` → **`127.0.0.1:4464`**; `SocketAddress(Loopback, -1)` → **`127.0.0.1:65535`** | **#2035**, compatible |
| **301** | med | **N-B** | `IPAddress(bytes, -1)` → `ScopeId` **4294967295**; `IPAddress(bytes, 4294967296)` → **0**; `setScopeIdProperty(-5)` → **4294967291** | **#2036**, compatible |
| **302** | med | **N-C** | `IPEndPoint::Parse("[::1]ignored:80")` → **`[::1]:80`** — `ignored` silently discarded | **#2037**, compatible |
| **303** | med | **N-D** | `IPNetwork(fe80::1 scope 7, 64).BaseAddress.ScopeId` → **0** — the scope id the constructor was given is lost | **#2038**, compatible |
| **304** | med | **N-C** | `GetHostAddresses("-0.0.0.1")` → **`0.0.0.1`**; `("0.0.0.0")` → **the wildcard**; `("127.0.0.1", Unix)` → **one IPv4 result**, family filter not applied; **and `("1.2.3")` → three identical `1.2.0.3`** | **#2039**, compatible + **#2043** gated wildcard half |
| **305** | high | **N-E** | a cookie added from `origin.invalid` with `Domain=.unrelated.invalid` is returned by `GetCookieHeader(http://unrelated.invalid/)` as **`session=isolated`** | **#2040**, DESIGN, blocked |
| **306** | med | **N-E** | `Cookie("n","v","/explicit").PathImplicit` → **true**; `Cookie("n","v","/p",".d.invalid").DomainImplicit` → **true**; the equivalent **setters** correctly clear the flag | **#2040**, DESIGN, blocked |
| **307** | high | **N-B** | `collection[Count]` → **SIGSEGV**; `collection[-1]` returned a garbage `Cookie` whose `Name` printed as `(null)` — both undefined behaviour | **#2041**, compatible |
| **308** | med | **N-F** | 10,000 cookies from one origin → `Count == 10000`; no capacity, per-domain, size, aging or eviction policy | **#2042**, DESIGN, blocked |
| **309** | med | **N-G** | `HtmlEncode` escapes exactly `& < > " '` and passes all non-ASCII through; `HtmlDecode` **already** handles `&copy;`, `&#169;` **and** `&#xA9;` | **#2044**, DESIGN, deferred |

**Ten findings in, ten out.** None is a duplicate, none is a false premise, none is already
remediated, and none receives a "no action" disposition.

---

## 4. Corrections to the audit record

Historical audit text preserved; these are appended corrections, each measured.

### 4.1 SR-AUD-304 has a fourth defect the finding does not name: **duplicate results**

`GetHostAddresses("1.2.3")` returns **three identical `1.2.0.3` entries**. The finding names an
over-permissive parser, a wildcard result and a missing family filter; it does not name the
duplication, which is a *separate* wrong answer on a *valid* input and is the one half of #2039
that is unambiguously a plain bug rather than a policy narrowing.

### 4.2 SR-AUD-304's `999.999.999.999` case does **not** reach the literal parser

`GetHostAddresses("999.999.999.999")` **throws** rather than returning anything — the text falls
through to real resolution and fails there. The finding's premise is about `-0.0.0.1` and
`0.0.0.0`, both of which reproduce exactly; the out-of-range case is a different path and is
recorded here so a future ticket does not write a test asserting the wrong door.

**A second observation on the same door:** the thrown message is **`"Win32 error 11001"`** on a
POSIX build. A WSA error name leaking into the POSIX branch is a disclosure defect in its own
right, carries no `SR-AUD-*` identifier, and is folded into #2039 rather than filed separately.

### 4.3 SR-AUD-307's own reproduction is the *less* reliable of the two

The finding says `collection[-1]` "terminates with a segmentation fault". Measured **without**
ASan, `collection[-1]` **returned normally** with a garbage `Cookie`, while `collection[Count]`
**crashed with SIGSEGV**. Both are undefined behaviour and the finding stands, but the specific
case it names is the one that can silently *succeed* — which is worse, not better, and a
regression test must cover both.

### 4.4 SR-AUD-309 describes the **encode** direction only

`HtmlEncode` is indeed a five-entity subset. `HtmlDecode` is **not**: measured, it already handles
decimal (`&#169;`) and hexadecimal (`&#xA9;`) character references and named entities beyond the
five (`&copy;`). So "HTML encoding/decoding is a five-entity byte subset" is half right, and the
asymmetry — a decoder that understands more than the encoder produces — is itself the shape of the
finding.

### 4.5 Three audit statements that are correct and are **not** corrected

- SR-AUD-300's ASan claim is exactly right, reproduced at the **same line number**
  (`SocketAddress.cpp:106`) with the allocation stack showing the 2-byte `std::vector`.
- SR-AUD-305's reproduction is exact, including the cookie value.
- SR-AUD-301's narrowing reproduces in **both** directions (negative *and* above `UInt32.MaxValue`)
  and on **both** doors (constructor and setter), which the finding claims and which is confirmed.

---

## 5. Root causes

### N-A — a decode path with no layout validation

**Member: SR-AUD-300.** `GetIPEndPoint` reads fixed offsets 2–7 (IPv4) or 2–27 (IPv6) from a
`std::vector<bytecs>` whose size the caller chose, without checking family or size; the
`SocketAddress(const IPAddress&, intcs)` constructor accepts any `intcs` port and truncates it to
16 bits. Compatible — reading past the end of a heap allocation has no defined result, and a
truncated port is a defined-but-meaningless one.

### N-B — a signed public parameter converted to unsigned with no domain check

**Members: SR-AUD-301, SR-AUD-307.** Structurally one cause in two places: `IPAddress`'s
`longcs scopeId` becomes a `uint32_t` field, and `CookieCollection::operator[]`'s `intcs index`
becomes a `size_t` handed straight to `std::vector::operator[]`. This is CCF-004's shape one layer
up — not undefined *arithmetic* but an undefined *conversion domain* at a public boundary.
Compatible in both cases.

### N-C — a parser that discards what it does not understand

**Members: SR-AUD-302, SR-AUD-304.** `IPEndPoint::TryParse`'s bracketed branch finds `]`, then
looks for the *last* `:`, so anything between them evaporates. `Dns` carries a **duplicate**
`sscanf("%u")` IPv4 literal parser beside the real `IPAddress::TryParse`, and that duplicate
accepts a leading `-`, produces duplicates, and bypasses the family filter. Mostly compatible; the
wildcard-rejection half is split out as gated (§7.2).

### N-D — a transformation that drops carried state

**Member: SR-AUD-303.** `clearNonPrefixBits` rebuilds an IPv6 address from masked bytes and
constructs the result with scope id 0, so information the constructor was handed is silently lost.
Compatible — a silent wrong answer, the #1837 precedent.

### N-E — cookie origin policy is absent

**Members: SR-AUD-305, SR-AUD-306.** There is no equivalent of .NET's
`Cookie.VerifyAndSetDefaults`: an explicit `Domain` is never checked against the source URI, and
the three-and-four-argument constructors assign `path_`/`domain_` **directly** rather than through
their setters, so the implicit flags stay set and container insertion overwrites the caller's
values. The two are one policy because the implicit flags are the *input* to the domain-matching
rule the first half must define. **Gated.**

### N-F — unbounded public state

**Member: SR-AUD-308.** No capacity, per-domain capacity, maximum size, expiry cleanup or
eviction. Gated: every bound is a number somebody must choose, and adding one starts discarding
data.

### N-G — a declared-subset text transformation

**Member: SR-AUD-309.** The same shape as `System::Text`'s T-M (#2019): a policy about which
characters must be escaped, whose target cannot be verified in this container. **Deferred**, and
deliberately coupled to #2019 (§7.2).

---

## 6. Findings and surfaces that are *not* in this namespace's queue

| Item | Why not |
|---|---|
| `System::Net::Sockets`, `::Http`, `::WebSockets`, `::NetworkInformation`, `::Mime`, `::Security` | **different namespaces**, different modules, their own future reviews. #1962 lives in `NetworkInformation` and is untouched here |
| `WebHeaderCollection`, `WebProxy`, `CredentialCache`, `NetworkCredential`, `DnsEndPoint`, `IPHostEntry` | audited, **no** confirmed finding |
| `HttpStatusCode`, `HttpVersion`, `HttpRequestHeader`/`HttpResponseHeader`, `DecompressionMethods`, `WebExceptionStatus` | enums and constants; audited, no finding |
| `HttpListener`, `WebClient`, `WebRequest`/`WebResponse`, `ServicePointManager`, `NetworkCredential` domain auth | **not ported**; absent features are not remediation |
| TLS / `SslStream` | `CLAUDE.md`'s permanent out-of-scope deviation |
| Downstream migration | CNA and mobile-eggbert; **#1773 stays blocked** and downstream use was not investigated |

---

## 7. Compatible versus approval-sensitive classification

### 7.1 Compatible — no approval required

| Ticket | Cause | Findings | What changes observably |
|---|---|---|---|
| **#2035** | N-A | 300 | `GetIPEndPoint` validates family and minimum size and throws instead of reading out of bounds; the `IPAddress`+port constructor rejects a port outside `0..65535` |
| **#2036** | N-B | 301 | an IPv6 scope id outside `0..4294967295` raises `ArgumentOutOfRangeException` instead of wrapping |
| **#2037** | N-C | 302 | `IPEndPoint::Parse`/`TryParse` reject text between `]` and `:` instead of discarding it |
| **#2038** | N-D | 303 | `IPNetwork`'s masked base address **keeps** the scope id it was constructed with |
| **#2039** | N-C | 304 (3 of 4 halves) | the duplicate `sscanf` literal parser is replaced by `IPAddress::TryParse`, so `-0.0.0.1` is rejected, a literal is returned **once**, the requested family filter is applied, and the POSIX error message stops naming a Win32 code |
| **#2041** | N-B | 307 | both `CookieCollection` indexers validate `0 <= index < Count` and throw instead of invoking undefined behaviour |

Why each is compatible in one line: **#2035 and #2041 change only paths that today read out of
bounds or crash; #2036, #2037 and #2039's rejection half narrow inputs whose current results are
defined but meaningless (a wrapped scope id, a silently truncated endpoint, a negative-signed
address literal); #2038 and #2039's duplicate half replace silent wrong answers.** §7.3 tabulates
every narrowed row before it is made.

### 7.2 Approval-sensitive — designed here, not implemented

| Ticket | Cause | Findings | Gate |
|---|---|---|---|
| **#2040** | N-E | 305, 306 | the cookie origin policy: `Add` starts **rejecting** cookies it stores today, and constructor-supplied path/domain stop being overwritten — a change in which cookies a container emits |
| **#2042** | N-F | 308 | the storage bound: what limits, and whether they are default or opt-in. Adding one starts **discarding** stored data |
| **#2043** | N-C | 304 (wildcard half) | `GetHostAddresses("0.0.0.0")` currently returns a usable wildcard address and would start throwing |
| **#2044** | N-G | 309 | the HTML escaping policy — **deferred, not merely blocked**: the target is unverifiable here, exactly as `System::Text`'s #2019 |

### 7.3 The complete narrowed-row table for the compatible batch

| Call | Before (measured) | After |
|---|---|---|
| `SocketAddress(InterNetwork, 2).GetIPEndPoint()` | **heap-buffer-overflow read**, returned `0.0.0.0:0` | `ArgumentException` |
| `SocketAddress(Unix, 16).GetIPEndPoint()` | `0.0.0.0:0` | `ArgumentException` — **the one row that removes a non-faulting result** |
| `SocketAddress(Loopback, 70000)` | `127.0.0.1:4464` | `ArgumentOutOfRangeException("port")` |
| `SocketAddress(Loopback, -1)` | `127.0.0.1:65535` | `ArgumentOutOfRangeException("port")` |
| `SocketAddress(InterNetwork, 16)` (well-formed) | works | **identical** |
| `IPAddress(bytes, -1)` / `setScopeIdProperty(-5)` | wraps to `4294967295` / `4294967291` | `ArgumentOutOfRangeException("value")` |
| `IPAddress(bytes, 0 … 4294967295)` | works | **identical** |
| `IPEndPoint::Parse("[::1]ignored:80")` | `[::1]:80` | `FormatException` |
| `IPEndPoint::Parse("[::1]:80")`, `"1.2.3.4:80"` | works | **identical** |
| `IPNetwork(fe80::1%7, 64).BaseAddress.ScopeId` | `0` | `7` — **a changed return value on a working call** |
| `IPNetwork` over IPv4, or IPv6 with scope 0 | works | **identical** |
| `Dns::GetHostAddresses("-0.0.0.1")` | `0.0.0.1` | `FormatException` (or the resolver's failure) |
| `Dns::GetHostAddresses("1.2.3")` | **three** identical `1.2.0.3` | **one** `1.2.0.3` |
| `Dns::GetHostAddresses("127.0.0.1", Unix)` | one IPv4 result | empty |
| `Dns::GetHostAddresses("0.0.0.0")` | wildcard | **unchanged here** — split to the gated #2043 |
| `collection[-1]`, `collection[Count]` | garbage `Cookie` / **SIGSEGV** | `ArgumentOutOfRangeException("index")` |
| `collection[0 … Count-1]` | works | **identical** |

Two rows deserve the reviewer's eye and are called out rather than hidden: the `Unix`-family
`GetIPEndPoint`, which today returns a non-faulting (if meaningless) endpoint, and `IPNetwork`'s
changed `ScopeId`, which is a **different value from a working call**. Either can be split into its
own gated ticket without disturbing the rest.

---

## 8. Compatibility proofs and the source / ABI / layout consequence matrix

### 8.1 Declarations

| Ticket | Signature | `noexcept` | virtual / vtable | data members | mangled names |
|---|---|---|---|---|---|
| #2035 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2036 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2037 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2038 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2039 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2041 | unchanged | unchanged | unchanged | unchanged | unchanged — but **header-inline**, so consumers recompile |
| #2040 | unchanged | unchanged | unchanged | **possibly `Cookie`** — if the policy needs a stored origin, that is an **object-layout change** in a header-only type | unchanged |
| #2042 | **additive** if limits are exposed | unchanged | unchanged | **`CookieContainer`** gains limit fields — it has an out-of-line body, but the members live in the header | unchanged |
| #2043 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2044 | **additive** if a `Create(…)` opt-in is added | unchanged | unchanged | unchanged (`WebUtility` is all-static) | unchanged |

**The central ABI fact of this namespace, and it is the opposite of `System::Diagnostics`'s.**
`Process` was a pimpl and had no in-repository caller; here `Cookie`, `CookieCollection` and
`WebUtility` are **header-inline** and there are **real cross-module consumers** (§2.3). So:

- every compatible ticket except #2041 touches only a `.cpp` body and is **relink-only**;
- **#2041 touches a header-only class**, so `net-sockets`, `net-http`, `net-websockets`,
  `net-network-information` and the integration tests all **recompile**;
- **#2040 is the only ticket in the namespace that could change an object layout**, and only if the
  chosen policy requires storing the origin on the `Cookie` — which the recommended option avoids
  by validating at insertion time instead. §11 makes that explicit.

### 8.2 Recompilation

`IPAddress.cpp`, `IPEndPoint.cpp`, `IPNetwork.cpp`, `SocketAddress.cpp`, `Dns.cpp` and
`CookieContainer.cpp` are out-of-line bodies: #2035–#2039 and #2043 are relink-only.
`CookieCollection.hpp`, `Cookie.hpp` and `WebUtility.hpp` are header-inline: #2041, #2040 and #2044
force a recompilation.

---

## 9. Downstream consumer impact

**Not estimated, by instruction.** CNA and mobile-eggbert were not read, searched, built, tested or
modified, and no filesystem search left this repository. **#1773 stays `blocked`.**
In-repository callers **were** measured (§2.3) and are numerous, unlike `System::Diagnostics`'s:
each ticket must run `SharpRuntimeTests_Net_Sockets`, `SharpRuntimeTests_Net_Http`,
`SharpRuntimeTests_Net_WebSockets`, `SharpRuntimeTests_Net_NetworkInformation` and
`SharpRuntimeIntegrationTests` alongside `SharpRuntimeTests_Net`.

---

## 10. Test matrix

Permanent, add-only, in `modules/net/tests/System/Net/`.

| Area | Cases required |
|---|---|
| **#2035 SocketAddress** | every family × {size below the minimum, exactly the minimum, above it}; `Unix` and every unsupported family rejected with exact type and message; the port domain `-1`, `0`, `65535`, `65536`, `70000`, `INTCS_MIN`, `INTCS_MAX`; a well-formed IPv4 and IPv6 round trip byte-identical before and after; **an ASan case over the undersized buffer** |
| **#2036 scope id** | `-1`, `0`, `1`, `4294967295`, `4294967296`, `LONGCS_MIN`, `LONGCS_MAX` on **both** the constructor and the setter; exact `paramName`; every in-range value round-trips |
| **#2037 IPEndPoint** | `[::1]:80`, `[::1]ignored:80`, `[::1]`, `[::1]:`, `[::1]:99999`, `[fe80::1%7]:80`, `1.2.3.4:80`, `1.2.3.4:80junk`; `TryParse` returns `false` without throwing wherever `Parse` throws |
| **#2038 IPNetwork** | IPv6 with scope 0 and non-zero across prefix lengths 0/1/63/64/127/128; IPv4 unaffected; `Contains` still agrees with the masked base |
| **#2039 Dns** | `-0.0.0.1`, `1.2.3`, `1.2.3.4`, `::1`, `0.0.0.0` (**unchanged**, #2043); each literal returned exactly **once**; the family filter for `InterNetwork`, `InterNetworkV6` and `Unix`; the POSIX failure message contains no `Win32` |
| **#2041 CookieCollection** | `-1`, `0`, `Count-1`, `Count`, `Count+1`, `INTCS_MIN`, `INTCS_MAX` on **both** indexer overloads; exact type, `paramName`, message; an empty collection; **an ASan case** |
| **gated pins** | mandatory, per the #2022/#2028 lesson: the cross-origin cookie is emitted (#2040); the constructor leaves the implicit flags set (#2040); 10,000 cookies are retained (#2042); `GetHostAddresses("0.0.0.0")` returns the wildcard (#2043); `HtmlEncode` passes non-ASCII through while `HtmlDecode` understands `&copy;`/`&#169;`/`&#xA9;` (#2044). **Every pin must be shown discriminating** |
| **layout pins** | `sizeof`/`alignof` of `Cookie`, `CookieCollection`, `IPAddress`, `IPEndPoint`, `SocketAddress` — the three header-inline types are where a layout change would be silent |

---

## 11. Sanitizer matrix

| Sanitizer | Applies to | What it must show |
|---|---|---|
| **ASan** | **#2035, #2041** | the `heap-buffer-overflow` at `SocketAddress.cpp:106` and the out-of-range `CookieCollection` access present **before** and absent **after**, with the changed bodies compiled **from source** into the probe. The before half is **already run and reproduced** (`build-probe/2034_probe2_asan.log`) |
| **UBSan** | #2036, #2041 | the `longcs` → `uint32_t` and `intcs` → `size_t` conversions. Expected **non-discriminating** for the conversions themselves (they are implementation-defined, not undefined) — record it as a non-result rather than as evidence, the #2024 precedent |
| **LSan** | #2039, #2042 | no leak across a failed `getaddrinfo` and across 10,000 cookie insertions |
| **TSan** | **none** | `System::Net`'s finding-bearing types hold no shared mutable state and start no thread. Stated as inapplicable rather than silently skipped — but **`CookieContainer` is a plausible cross-thread object**, so if #2042's design adds background aging, TSan becomes applicable to it |

---

## 12. Reference evidence actually available, per repair

`/rv/tmp/runtime/src/libraries/` re-verified **absent** 2026-08-04; no .NET runtime is installed.

| Cause | Evidence available here | Sufficient? |
|---|---|---|
| **N-A** | ASan; the type's own `getSizeProperty()`/`getFamilyProperty()`; this repository's own `ArgumentException` port precedent | **yes** — a heap overflow needs no reference |
| **N-B** (scope id) | `IPAddress`'s own `uint32_t` storage; the repository's `ArgumentOutOfRangeException` precedent (#1953, #2024) | **yes** |
| **N-B** (indexer) | the C++ standard: `std::vector::operator[]` out of range is undefined | **yes** |
| **N-C** (IPEndPoint) | the function's own two other branches, which do **not** discard trailing text | **yes** — transcribed from the port |
| **N-C** (Dns) | `IPAddress::TryParse` **in the same module**, which the duplicate parser exists beside | **yes** — the repair is deletion |
| **N-D** | the constructor that accepted the scope id | **yes** |
| **N-E** | .NET's exact `Cookie.VerifyAndSetDefaults` domain rule, and whether a public-suffix list is involved | **no** — gated on approval **and** on evidence |
| **N-F** | .NET's exact default capacities | **no** |
| **N-G** | .NET's exact default HTML escape set | **no** — same gap as `System::Text` #2019 |

**No repair in §7.1 depends on a .NET behaviour that could not be established here.** That is the
criterion separating the two columns, and it is the same one the previous six reviews used.

---

## 13. Recommended execution order

1. **#2034** — this plan (no code).
2. **#2041** (N-B, SR-AUD-307) — first: it is the one defect that **crashes**, and it is a
   three-line guard in a header.
3. **#2035** (N-A, SR-AUD-300) — second: the other memory-safety defect, ASan-provable both ways.
4. **#2036** (N-B, SR-AUD-301) — the sibling narrowing, same cause, different file.
5. **#2037** and **#2038** — independent single-file parser/transform repairs; may share one commit
   (the #2007/#2008 precedent) only if their tests stay separable.
6. **#2039** (N-C, SR-AUD-304) — last of the compatible batch, because deleting the duplicate
   parser is the largest single behaviour surface and benefits from #2036 being settled first.
7. **A disclosure-and-pins ticket** — mandatory, the #2012/#2022/#2028 lesson: make the
   `Cookie`/`CookieContainer`/`WebUtility`/`Dns` doc-comments true and **pin** every gated
   behaviour, each pin shown discriminating.
8. **#2040, #2042, #2043, #2044** — design records only; none implemented without its §14 approval
   sentence.

**After this namespace, the measured next candidate is `modules/buffers`** (11 open, 3 high, 27 %,
partial plan) — but re-derive it rather than trusting this sentence, as §1 did.

---

## 14. Approval package — the four gated causes

Requested **only** when this namespace's compatible half is done; **none is requested by writing
this**, and the consolidated request will follow `docs/ConsolidatedApprovalPackage.md`'s format.

### 14.1 #2040 — N-E, the cookie origin policy (SR-AUD-305 + SR-AUD-306)

**Now:** any explicit `Domain` is stored and later emitted for that domain regardless of the URI it
came from (measured); constructor-supplied `Path`/`Domain` leave the implicit flags set, so
container insertion overwrites them. **.NET:** `Cookie.VerifyAndSetDefaults` validates the domain
relation before storage and raises `CookieException` — **inferred, not verifiable here**.
**Alternatives:** (A) validate the explicit domain as a suffix of the request host with the
leading-dot and host-only rules, rejecting with `CookieException`, and fix the constructors to go
through their setters — recommended; (B) additionally consult a public-suffix list, which no data
source here supports; (C) store the origin on the `Cookie` and filter at emission — **an object
layout change in a header-only type**, and it keeps the bad data; (D) document the reduction and
leave. **Recommended: A**, precisely because it needs **no layout change**.

> Approve making `System::Net::CookieContainer::Add(uri, cookie)` validate an explicitly supplied
> `Domain` against the request URI's host — accepting the leading-dot and host-only rules and
> raising `System::Net::CookieException` otherwise — and making `Cookie`'s path- and
> domain-accepting constructors clear the corresponding implicit flags exactly as their setters do,
> accepting that cookies which are stored and emitted today begin to be rejected, and that a
> container stops overwriting a constructor-supplied path or domain. Ticket **#2040**.

### 14.2 #2042 — N-F, the storage bound (SR-AUD-308)

**Now:** unbounded (10,000 retained, measured). **Alternatives:** (A) .NET's documented defaults —
whose exact numbers are **not verifiable here**; (B) expose `Capacity`/`PerDomainCapacity`/
`MaxCookieSize` with generous defaults and make the unbounded behaviour opt-in; (C) clean expired
cookies on insertion only — the smallest step that bounds nothing but removes dead state;
(D) document and leave. **Recommended: C now, B when a number can be justified.**

> Approve bounding `System::Net::CookieContainer`'s storage — by removing expired cookies on
> insertion, and by adding public capacity, per-domain capacity and maximum-cookie-size limits with
> stated defaults — accepting that a container begins to discard stored cookies, and stating which
> defaults, because .NET's exact values cannot be verified in this container. Ticket **#2042**.

### 14.3 #2043 — N-C, the wildcard literal (SR-AUD-304's fourth half)

**Now:** `GetHostAddresses("0.0.0.0")` returns the wildcard address, which is a usable value.
**Proposed:** reject it as .NET does (inferred). Split out of #2039 precisely because it is the one
half of that finding that removes a **working, meaningful** result.

> Approve making `System::Net::Dns::GetHostAddresses` and `GetHostEntry` reject the wildcard
> literals `0.0.0.0` and `::` rather than returning them as resolved addresses, accepting that a
> call which succeeds today begins to throw. Ticket **#2043**.

### 14.4 #2044 — N-G, the HTML escaping policy (SR-AUD-309)

**Now:** `HtmlEncode` escapes five ASCII characters and passes all non-ASCII through; `HtmlDecode`
already understands numeric references and more named entities than the encoder produces (§4.4).
**This is the same gap as `System::Text`'s #2019** and must be decided **with** it, or not at all:
two HTML encoders in one repository with two different escape sets is the CCF-012 shape.
**Deferred**, not merely blocked — the target is unverifiable here.

> Approve giving `System::Net::WebUtility::HtmlEncode` a stated escape policy — **and state which**,
> together with `System::Text::Encodings::Web`'s (#2019), because two HTML encoders in one
> repository must not diverge and .NET's exact default set cannot be verified in this container.
> Ticket **#2044**.

---

## 15. Explicit exclusions and completion criteria

**Excluded:** the eight sibling `Net` namespaces (§6); `WebHeaderCollection`, `WebProxy`,
`CredentialCache` and the enums, all audited without a finding; absent APIs (`HttpListener`,
`WebClient`, `WebRequest`); TLS; and downstream migration (§9).

`System::Net` is complete when **all ten** findings are `remediated` or carry the
`confirmed (design-complete)` qualifier with a blocked implementation ticket, and:

1. #2035–#2039 and #2041 are `done` with permanent tests;
2. #2040, #2042, #2043 and #2044 each carry a durable design here **and** a blocked ticket whose
   notes name the approval sentence **and** a permanent behaviour-pinning test — mandatory, not
   optional;
3. the whole 37-executable gate is green apart from the known environment/#1962 failures;
4. `SharpRuntimeTests_Net` has grown by the §10 matrix, add-only, from its measured baseline of
   **240**, and the five sibling suites named in §9 still pass;
5. ASan is **run** for #2035 and #2041 and shown discriminating in both directions;
6. no `SR-AUD-*` identifier was created — numbering stays at **364**.

---

## 16. Status

| Ticket | Cause | Findings | State |
|---|---|---|---|
| #2034 | — | maps all 10 | this document |
| #2035 | N-A | 300 | **todo**, compatible |
| #2036 | N-B | 301 | **todo**, compatible |
| #2037 | N-C | 302 | **todo**, compatible |
| #2038 | N-D | 303 | **todo**, compatible |
| #2039 | N-C | 304 (3 halves) | **todo**, compatible |
| #2041 | N-B | 307 | **todo**, compatible — do first, it crashes |
| #2040 | N-E | 305, 306 | **blocked**, design complete (§14.1) |
| #2042 | N-F | 308 | **blocked**, design complete (§14.2) |
| #2043 | N-C | 304 (wildcard half) | **blocked**, design complete (§14.3) |
| #2044 | N-G | 309 | **blocked**, deferred (§14.4), coupled to #2019 |
