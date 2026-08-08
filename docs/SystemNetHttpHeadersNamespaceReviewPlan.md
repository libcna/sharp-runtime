<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Net::Http::Headers` (`modules/net-http-headers`) namespace review — ticket #2122

Owning ticket **#2122**. This document is the durable record; it **remediates nothing by itself**.
Every claim is measured against the tree at `577e836`.

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-08. Every statement about .NET
comes from repository-contained evidence only: the per-file audit reports, doc-comments transcribed
from .NET when the module was written, this module's own tests, and the RFC grammar this
repository's own `Net`/`Net.Http`/`Net.WebSockets` validators already encode. Where a repair would
need .NET's exact behaviour and no repository evidence pins it, a **deferred-verification ticket**
is created instead of a guess.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit defects
carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket **#1773 stays blocked**.

Primary evidence: `build-probe/2122_probe1_headers.cpp`, log `build-probe/2122_probe1_before.log`.

---

## 1. Why this unit was selected — re-derived by measurement, not inherited

Re-parsed from `audit/AUDIT_FINDINGS_INDEX.md` at this tip. **Every** unit with ≥4 open findings:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `modules/core` | 72 | 9 | 59 | 4 | 12% | 1 | 47 | family plans only |
| `modules/threading` | 17 | 6 | 11 | 0 | 35% | 0 | 21 | **yes** |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7% | 12 | 8 | **yes** |
| `modules/text` | 11 | 1 | 10 | 0 | 9% | 11 | 3 | **yes** |
| `modules/uri` | 10 | 0 | 10 | 0 | 0% | 10 | 4 | **yes** |
| `modules/io` | 7 | 0 | 7 | 0 | 0% | 0 | 6 | **yes** |
| `modules/time-zone` | 7 | 0 | 7 | 0 | **0%** | 0 | 0 | none |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none |
| `modules/net-http` | 6 | 1 | 5 | 0 | 16% | 2 | 3 | **yes** (closed) |
| `modules/buffers` | 5 | 1 | 4 | 0 | 20% | 3 | 13 | **yes** |
| `modules/net-sockets` | 5 | 1 | 4 | 0 | 20% | 0 | 0 | none |
| `modules/net` | 5 | 1 | 4 | 0 | 20% | 5 | 5 | **yes** |
| **`modules/net-http-headers`** | **5** | **2** | **3** | **0** | **40%** | **0** | **0** | **none** |
| `modules/numerics` | 4 | 0 | 4 | 0 | 0% | 0 | 0 | none |
| `modules/text-json` | 4 | 1 | 3 | 0 | 25% | 1 | 3 | **yes** (closed) |

**Unreviewed coherent units with ≥6 open:** `time-zone` (7) and `globalization` (7). (`core` at 72
is not a coherent unit — it is 47 files across a dozen subsystems, and already carries family plans;
it has been excluded on that basis by every previous review and is excluded here for the same
reason, not because it is inconvenient.)

`net-http-headers` is **below** the ≥6 threshold at 5. It is selected anyway, and the case does
**not** rest on CCF-021:

1. **Highest high-severity ratio in the repository — 40% (2 of 5).** `globalization` is 14%;
   `time-zone` is **0%**. Two `high` findings in one 5-finding module is the densest concentration
   anywhere.
2. **Defect class.** Both `high` findings are **protocol-field injection**: caller text becomes
   additional header structure. `time-zone`'s seven are all correctness divergences in a local,
   trusted data path (system zone rules). `globalization`'s one `high` is a concurrency defect and
   its other six are collation/grapheme/casing divergences.
3. **Decidability with `/rv` absent — decisive.** `net-http-headers`' defects are settled by a
   **public grammar this repository already encodes**: `System::Net::detail::ContainsProtocolFieldTerminator`
   and `HttpHeaders::Add`'s own token check. `time-zone`'s seven are *all* "what exactly does .NET
   do" questions (custom-zone equality, `BaseUtcOffset` invariance, `HasSameRules`), and five of
   `globalization`'s seven are too (grapheme clusters, collation, culture casing, IDN unassigned
   code points). Reviewing either of those today produces a **queue of deferred-verification
   tickets**, not compatible work — the same reasoning that selected `text-json` over them last
   batch, and it has not changed.
4. **Dependency readiness.** Its upstream (`Net.Http`, `Net.WebSockets`) is closed for compatible
   work, and the repair target for the `high` findings **already has one body in the repository**.
5. **Module cohesion — the best of any candidate.** One CMake component, one namespace, one
   directory: 26 headers (2,080 lines), 25 sources (4,084 lines), 21 test files (2,695 lines).
6. **No existing review**, and **zero** findings remediated — the module has never been touched.
7. **Fourth, not first: it completes CCF-021's membership.** Four separate documents name this
   module as the promotion trigger. That is a genuine benefit and it is deliberately listed last,
   because a unit must not be chosen to close a family.

**Selected: `modules/net-http-headers`**, on 1–3 primarily.

---

## 2. Scope and file inventory

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 26 | 2,080 |
| implementation | 25 | 4,084 |
| tests | 21 | 2,695 |

One component: `Net.Http.Headers`, `TYPE STATIC`,
`PUBLIC_DEPENDENCIES Collections.Core Core.Base Uri`.

**In scope:** everything under `modules/net-http-headers/`.

**Out of scope, and why:**

- `modules/net-http`'s raw header map and its wire serialization — reviewed and **closed** (#2062,
  #2063, #2064). §4.6 records why that boundary matters more than it looks.
- `modules/net`'s `ContainsProtocolFieldTerminator` — the shared predicate is **consumed**, never
  modified.
- HTTP/2 and HTTP/3 header compression (HPACK/QPACK) — **absent from this port**; there is no
  subject. Recorded rather than invented.
- `HttpHeadersNonValidated` — present as a type; §3 records what it actually does.

---

## 3. Complete public-surface inventory

| Area | Types |
|---|---|
| Collections | `HttpHeaders` (base), `HttpRequestHeaders`, `HttpResponseHeaders`, `HttpContentHeaders`, `HttpHeadersNonValidated` |
| Name/value core | `NameValueHeaderValue`, `NameValueWithParametersHeaderValue` |
| Media type | `MediaTypeHeaderValue`, `MediaTypeWithQualityHeaderValue` |
| Content disposition | `ContentDispositionHeaderValue` |
| Conditional / ETag | `EntityTagHeaderValue`, `RangeConditionHeaderValue` |
| Range | `RangeHeaderValue`, `RangeItemHeaderValue`, `ContentRangeHeaderValue` |
| Auth | `AuthenticationHeaderValue` |
| Caching | `CacheControlHeaderValue`, `RetryConditionHeaderValue` |
| Transfer | `TransferCodingHeaderValue`, `TransferCodingWithQualityHeaderValue` |
| Agent / proxy | `ProductHeaderValue`, `ProductInfoHeaderValue`, `ViaHeaderValue`, `WarningHeaderValue` |
| Quality | `StringWithQualityHeaderValue` |

**The structural fact that shapes every finding below:** each typed value carries **its own copy**
of the two primitives it needs — a *quoted-string validator* and a *delimiter splitter* — and the
copies disagree. `NameValueHeaderValue::isValidQuotedString` correctly consumes quoted-pairs
(`\"`); the parameter/list splitters in `MediaTypeHeaderValue`, `TransferCodingHeaderValue`,
`CacheControlHeaderValue`, `ContentDispositionHeaderValue`, `HttpRequestHeaders` and
`HttpResponseHeaders` merely **toggle on every quote**. And the RFC 1123 date parser is copied into
**six** files, each `sscanf`-based and none checking full consumption.

---

## 4. Every open finding, with its measured disposition

All five reproduced against `577e836`.

| Finding | Sev | Measured | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-319 | **high** | **confirmed and wider** — §4.1 | compatible | **#2124** |
| SR-AUD-320 | med | **confirmed, premise sharpened** — §4.2 | compatible | **#2126** |
| SR-AUD-321 | med | **confirmed exactly as filed** — §4.3 | compatible | **#2125** |
| SR-AUD-322 | **high** | **confirmed, and half of it is already correct** — §4.4, §18.1 | **remediated (#2123)** | **#2123** |
| SR-AUD-323 | med | **confirmed and wider** — §4.5 | split: compatible + deferred | **#2127**, **#2129** |

### 4.1 SR-AUD-319 — confirmed, and **two doors the finding does not name**

Measured, every door the finding names does accept CR/LF/NUL and serialize it verbatim:

| Door | `p="safe\r\nX-Injected: value"` | NUL |
|---|---|---|
| `NameValueHeaderValue(name, value)` ctor | **accepted, serialized** | **accepted** |
| `NameValueHeaderValue::setValueProperty` | **accepted, serialized** | **accepted** |
| `NameValueHeaderValue::Parse` | **accepted, serialized** | **accepted** |
| `EntityTagHeaderValue(tag)` | **accepted, serialized** | **accepted** |
| `WarningHeaderValue(code, agent, text)` — quoted text | **accepted** | agent: **accepted** |
| `ContentDispositionHeaderValue::setNameProperty` | **accepted** | — |
| `ContentDispositionHeaderValue::setFileNameProperty` | **accepted** | — |
| **`MediaTypeHeaderValue` parameters** | **accepted** — *not named by the finding* | — |
| **`TransferCodingHeaderValue` parameters** | **accepted** — *not named by the finding* | — |

**Two corrections:**

1. **`ViaHeaderValue` is narrower than the family, and the finding is exactly right about it.**
   `isValidReceivedBy` **rejects** CR and LF (`"The receivedBy value is not valid"`) and admits
   **only NUL**. The finding says "rejects several delimiters but omits NUL" — measured, precisely
   so. It is the one door where the repair is a single added character class, not a new validator.
2. **The parameter-carrying types are two more doors.** `MediaTypeHeaderValue` and
   `TransferCodingHeaderValue` expose `getParametersProperty()` returning a mutable
   `std::vector<NameValueHeaderValue>`; a CR/LF-bearing parameter pushed there serializes verbatim
   through `ToString()`. They are not separate defects — they *inherit* `NameValueHeaderValue`'s —
   which is exactly why the repair must be at the shared validator and not at each named type.

**The repair target already has one body in the repository**:
`System::Net::detail::ContainsProtocolFieldTerminator`, which #2063 (ten HTTP doors) and #2089
(three WebSocket doors) both call. See §7 for the component-edge consequence.

### 4.2 SR-AUD-320 — confirmed; the splitter **does** track quotes, it just cannot see escapes

| Input | Result |
|---|---|
| `text/plain; p="a\";b"` (escaped quote, then `;` inside the string) | **rejected** |
| `gzip; p="a\";b"` | **rejected** |
| `attachment; filename="a\";b"` | **rejected** |
| `text/plain; p="a;b"` (**unescaped** `;` inside quotes) | **accepted** — **the control** |

The control matters: the splitter is not quote-blind, it is *escape*-blind. `NameValueHeaderValue::isValidQuotedString`
**already consumes quoted-pairs correctly**, so the module contains both a correct scanner and
several incorrect ones. This is the same "the repair target already exists one file away" shape as
#2103, #2101, #2111 and #2114.

**This is a widening, not a narrowing**: text that is valid per RFC 9110 starts being accepted.

### 4.3 SR-AUD-321 — confirmed exactly as filed, in **six** copies

| Door | `Sun, 06 Nov 1994 08:49:37 GMT trailing` |
|---|---|
| `RetryConditionHeaderValue::Parse` | **accepted**, trailing text discarded |
| `HttpContentHeaders::getExpiresProperty` | **parsed** |
| `HttpResponseHeaders::getDateProperty` | **parsed** |
| `HttpResponseHeaders::getDateProperty("garbage")` | **not parsed** — **the control** |

Also present, per the per-file reports, in `RangeConditionHeaderValue` (If-Range),
`HttpRequestHeaders` (Date) and `ContentDispositionHeaderValue` (creation/modification/read dates).
The control holds, so this is a **full-consumption** defect, not a permissive-grammar one.

### 4.4 SR-AUD-322 — confirmed, and **the validating sibling is already correct**

| Door | CR/LF name | NUL name | space in name | CR/LF value | empty name |
|---|---|---|---|---|---|
| `TryAddWithoutValidation` | **accepted** | **accepted** | **accepted** | **accepted** | rejected |
| `Add` | **rejected** | **rejected** | **rejected** | **rejected** | rejected |

`TryAddWithoutValidation("X-Bad\r\nInjected: yes", "v")` returns `true` and `ToString()` emits

```
X-Bad\r\nInjected: yes: v\r\n\r\n
```

— **two header fields where the caller supplied one**. That is a header-injection primitive with a
`bool`-returning API that reports success.

**The correction that shapes the repair:** `Add` already rejects all four classes, in both name and
value, with clear messages (*"The header name is not a valid HTTP token"*, *"The value contains
invalid CR, LF, or NUL characters."*). So the repair is **not** to write a validator — it is to
route `TryAddWithoutValidation`'s **name** through the one `Add` already uses, while deliberately
**preserving** the intentionally-unvalidated **value** behaviour. That asymmetry is the whole
finding, and it is what the per-file report means by *"validate names through the token/descriptor
path while preserving the intended unvalidated-value behavior"*.

### 4.5 SR-AUD-323 — confirmed, and there is a second defect in the same decoder

| Input | Measured | Expected per RFC 5987 |
|---|---|---|
| `filename*=iso-8859-1''foo-%E4.html` | **10 bytes**, raw `E4` | 11 bytes, UTF-8 `C3 A4` |
| `filename*=UTF-8''foo-%C3%A4.html` | 11 bytes, `C3 A4` | correct |
| `filename*=bogus''x` | **accepted** | unsupported charset must be rejected |
| **`filename*=UTF-8''a%0D%0Ab`** | **accepted; decoded value contains a raw CR/LF** | — |

The last row is **not** SR-AUD-323 and **not** SR-AUD-319: the decoder hands the *caller* a
CR/LF-bearing string. `ToString()` re-encodes it percent-escaped, so it does **not** inject on
serialization — but any consumer that puts `getFileNameStarProperty()` into another header, a log
line, a filename or a `Content-Disposition` it builds itself does. Filed separately as **#2129**.

The ISO-8859-1 half needs a transcoding decision (§10) and is split accordingly.

### 4.6 Where the wire actually is — a premise correction that changes the severity framing

`modules/net-http`'s `HttpRequestMessage` stores headers as a **raw
`std::unordered_map<std::string, std::string>`**, *not* as `HttpRequestHeaders`
(`HttpRequestMessage.hpp:20` says so explicitly), and `HttpClientHandler.cpp:280` serializes from
that raw map. **So this module is not on this repository's own wire path.** Its ten wire doors are
`Net.Http`'s, and #2063 already guards them.

That does **not** reduce these findings to cosmetics — the module's entire purpose is to *produce
header text*, and `ToString()` demonstrably emits an injected field — but it does change the honest
framing from *"remotely exploitable today in this repository"* to *"an injection primitive handed
to every consumer that serializes it"*. Recorded here so no ticket, and no CCF-021 promotion,
overstates it.

---

## 5. Structural root-cause families

- **NH-H — one rule, many copies, and the copies disagree.** SR-AUD-319 (quoted-string validation),
  SR-AUD-320 (delimiter splitting, 7 copies), SR-AUD-321 (HTTP-date parsing, 6 copies). **This is
  the module's dominant cause and it accounts for three of five findings.** It is the same shape as
  `text-json`'s **TJ-E** and `io`'s **I-F**, and the same shape #2114 and #2116 repaired by
  *extracting* the rule rather than copying it again.
- **NH-I — an "unvalidated" door validates less than its own contract requires.** SR-AUD-322.
  Distinct from NH-H: the correct rule exists in the sibling, the broken door simply does not call
  it.
- **NH-J — a decoder ignores the encoding it was told to use.** SR-AUD-323.
- **NH-K — a decoded value carries protocol structure to the caller.** §4.5's last row, **#2129**.
  New shape; not CR/LF-on-the-wire and therefore **not** a CCF-021 member (§8.3).
- **NH-L — a singleton header is stored as a list and joined with a comma.** §6.1's new defect.
  Request-smuggling shape; **not** CCF-021 (no field terminator is involved).

---

## 6. Post-audit defects (no `SR-AUD-*` identifier)

### 6.1 Singleton headers are joined with a comma, and `Transfer-Encoding` + `Content-Length` coexist

| Sequence | Serialized |
|---|---|
| `Content-Length: 10` then `Content-Length: 20` | **`Content-Length: 10,20`** |
| `Host: a.example` then `Host: b.example` | **`Host: a.example,b.example`** |
| `Transfer-Encoding: chunked` + `Content-Length: 5` | **both emitted** |

RFC 9110 §8.6 requires a `Content-Length` field-value to be a single `1*DIGIT`; a comma-joined pair
is exactly the message a smuggling chain relies on two intermediaries disagreeing about. RFC 9110
§7.2 requires exactly one `Host`. RFC 9112 §6.1 requires `Content-Length` to be ignored or the
message rejected when `Transfer-Encoding` is present.

**Where to enforce this is a genuine design question** — at the collection, at the typed accessor,
or at serialization — and it changes what a long-standing public API accepts. Filed as the design
ticket **#2128**, not silently implemented.

### 6.2 Measured positives, recorded so they are not re-investigated

- **`HttpHeaders::Add` is correct** for CR/LF/NUL in **both** name and value, for whitespace-padded
  names, and for obs-fold (`"a\r\n b"` is rejected). It is the repair target, not a defect.
- **`Content-Length` value parsing is correct**: `"not-a-number"`, `"99999999999999999999999"`
  (overflow) and `"-1"` all fail to parse. **No integer-overflow defect at this door.**
- **Quality values are validated**: `q=2.5`, `q=abc` and `q=-1` are all rejected.
- **Header lookup is case-INSENSITIVE.** `Contains("content-type")` finds `Content-Type`. Note this
  contradicts nothing here — `net-http`'s SR-AUD-315 case-sensitivity finding is about that
  module's **raw map**, a different type in a different component. Recorded so the two are never
  merged.
- **`ViaHeaderValue` already rejects CR and LF** (§4.1).
- **An unescaped `;` inside a quoted parameter is accepted** (§4.2's control).
- **No `std::` exception escaped any door probed.**

---

## 7. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` | Component graph |
|---|---|---|---|---|---|
| **#2123** | `TryAddWithoutValidation` starts returning `false` for invalid names | none | none | none | **+1 edge** (see below) |
| **#2124** | narrows: CR/LF/NUL stop being accepted at 9 doors | none | none | none | **+1 edge** |
| **#2125** | narrows: a trailing-garbage date stops parsing | none | none | none | none |
| **#2126** | **widens**: escaped quoted-pairs start being accepted | none | none | none | none |
| **#2127** | narrows + changes decoded bytes for ISO-8859-1 | none | none | none | none |
| **#2128** | **DESIGN** — singleton enforcement changes accepted input | none expected | none | none | none |
| **#2129** | narrows: a CR/LF-bearing RFC 5987 value stops decoding | none | none | none | none |

**The component-edge consequence, and it is the sharpest planning fact in this document.**
`Net.Http.Headers` declares `PUBLIC_DEPENDENCIES Collections.Core Core.Base Uri` — it does **not**
depend on `Net`. `System::Net::detail::ContainsProtocolFieldTerminator` lives in
`modules/net/include/System/Net/detail/ProtocolFieldValidation.hpp`, in the `Net` component. So
using the family's single predicate here **requires a new component edge**, taking the graph from
**41 modules / 91 edges** to **41 / 92** and requiring `scripts/generate_component_catalog.py` to be
re-run. The alternative — a fourth copy of the predicate — is cause NH-H, the very defect this
module is being reviewed for. **The edge is the right answer and must be declared explicitly**, as
`PRIVATE_DEPENDENCIES` (the predicate is used in `.cpp` bodies only, so no public header gains a
`Net` include).

**No ticket in this module needs an object-layout or vtable change.** That is unusual for a
namespace review at this stage and it is why the compatible queue here is long rather than gated.

---

## 8. CCF mapping

### 8.1 CCF-021 — membership is now complete; see §11 for the full evidence table

This module holds **two of the family's five findings, both `high`**, and four separate documents
name this review as the promotion trigger. §11 discharges the evidence obligation in full. **CCF-021
is NOT minted by this review** — see §11.5.

### 8.2 CCF-022 — no member here

X-D is *a public lifecycle state recorded but not enforced*. Nothing in this module has a lifecycle
state at all: the header values are value types and `HttpHeaders` has no closed/disposed concept.
**No member.** #2109 is unaffected.

### 8.3 What is deliberately **not** a CCF-021 member

- **SR-AUD-320, SR-AUD-321** — grammar/consumption defects, no field terminator involved.
- **SR-AUD-323 and #2129** — an *encoding* defect and a *decoded-value* defect. #2129 is the closest
  call in the whole family: it does involve CR/LF. It is excluded because the family's policy is
  *"reject CR/LF/NUL at the door before any byte reaches the wire"*, and #2129's CR/LF never reaches
  the wire — `ToString()` percent-encodes it. It travels **inward, to the caller**, which is cause
  NH-K and a different guarantee. Recorded explicitly, because "it has a CR in it" is exactly how a
  family acquires a member it cannot govern.
- **#2128** — a singleton/framing defect. No field terminator; the comma is a legal character.
- **CCF-012** — not a member; CCF-012 is composite-format brace grammars. **CCF-012 is not marked
  closed.**
- **CCF-019** — no member; nothing here holds a borrowed pointer across a lifetime boundary.

---

## 9. Parsing, serialization, mutation and thread-safety consequences

- **Parsing.** #2125 narrows (full consumption); #2126 widens (quoted-pairs); #2127 changes decoded
  bytes for one charset and narrows for unsupported ones. #2124/#2123 narrow at construction and
  insertion.
- **Serialization.** After #2123/#2124, **no `ToString()` in this module can emit a byte that
  terminates a header field**, for any input reachable through a public door. That is the precise
  guarantee, and §4.6 explains why it is stated as *"cannot emit"* rather than *"cannot put on the
  wire"*.
- **Mutation.** `getParametersProperty()` returns a **mutable reference** to the parameter vector
  (§4.1), so validation at construction is not sufficient — the repair must sit in
  `NameValueHeaderValue` itself, which every parameter is. This is why the repair is at the shared
  validator and not at each container type. **No mutation-during-enumeration defect was found**: the
  collections hand out copies (`GetValues` returns by value).
- **Thread safety.** Nothing in this module is documented thread-safe and nothing is. No finding
  names it and this review does not open one. **Recorded so a future reader does not assume it was
  checked and found safe — it was checked and found absent**, which is different. There is no
  process-global mutable state here, so unlike `text`'s T-G there is no shared-object hazard either.
- **Lazy parsing.** The typed accessors (`getDateProperty`, `getExpiresProperty`, …) parse **on
  every call** from the stored raw string; there is no cached parsed value, so the
  *cached-parsed-versus-raw-inconsistency* item on the batch checklist **has no subject here**.
  Recorded rather than invented.
- **Partial mutation before failure.** `Add` validates the name and the value **before** inserting,
  so a rejected `Add` leaves the collection unchanged — measured. `NameValueHeaderValue::setValueProperty`
  validates before assigning. **No partial-mutation defect found.**

---

## 10. Deferred evidence — what `/rv` would settle and this review will not guess

- Whether .NET's `HttpHeaders.TryAddWithoutValidation` returns `false` or throws for an invalid
  name — the per-file report states `false`, transcribed at audit time from
  `TryGetHeaderDescriptor`; #2123 follows that and records it as this port's choice.
- .NET's exact ISO-8859-1 → UTF-8 conversion behaviour for RFC 5987, and its exact treatment of an
  unsupported charset label (reject the parameter, or drop it silently) — **#2127** carries this and
  the ISO-8859-1 half is scoped as a deferred-verification split if the evidence proves
  insufficient.
- Whether .NET enforces singleton `Content-Length`/`Host` at the header collection or leaves it to
  the message layer — **#2128**, a design ticket for exactly this reason.
- Whether .NET's HTTP-date parser accepts the two obsolete formats (RFC 850, asctime) that RFC 9110
  §5.6.7 requires a *recipient* to accept — **#2130**, deferred verification. #2125 must not
  accidentally narrow those away while fixing full consumption.

---

## 11. CCF-021 — complete candidate membership and policy evidence

### 11.1 Every candidate member, enumerated across every reviewed namespace

| # | Finding / ticket | Module | Public door | Remotely controlled input | Field boundary crossed | Failure | Repair | State |
|---|---|---|---|---|---|---|---|---|
| 1 | **SR-AUD-313** + SR-AUD-316 (reason half) | `net-http` | 10 doors: method, URL, header name/value, status reason, … | caller and **peer** text | HTTP/1.1 request line and header field | injected request line / header | `ContainsProtocolFieldTerminator` | **remediated** (#2063) |
| 2 | **SR-AUD-248** | `net-websockets` | `SetRequestHeader`, the request URI, `Host:` | caller text, **and a `System::Uri`** | WebSocket handshake request line and headers | smuggled `GET /admin HTTP/1.1` | same predicate | **remediated** (#2089) |
| 3 | **SR-AUD-319** | `net-http-headers` | 9 doors (§4.1) | caller text | header field value | injected header field via `ToString()` | same predicate | **confirmed** → **#2124** |
| 4 | **SR-AUD-322** | `net-http-headers` | `TryAddWithoutValidation` (name) | caller text | header **field name** | injected header field via `ToString()` | `Add`'s own token check + same predicate | **confirmed** → **#2123** |
| 5 | **SR-AUD-316** (reason half) | `net-http` | `HttpResponseMessage` reason phrase | caller text | HTTP status line | — | same predicate | **remediated** (#2063) |

Members 1 and 5 share a ticket and are counted as one row in the cross-cutting appendix's table of
five; the appendix's own count is **five findings across three modules**, and this table reproduces
it exactly. **Nothing is added and nothing is dropped.**

### 11.2 Do all five share one structural cause? **Yes — and the evidence is stronger than adjacency.**

- **One cause:** caller- or peer-supplied text is concatenated into a protocol frame without
  rejecting the characters that terminate a field, so the text becomes additional protocol
  structure.
- **One predicate:** `System::Net::detail::ContainsProtocolFieldTerminator` — CR, LF, NUL. It has
  **exactly one body in the repository**, serving ten HTTP doors and three WebSocket doors today.
- **One validation timing:** at the **public door**, before the text is stored, not at
  serialization. Members 1 and 2 both landed that way.
- **One guarantee:** for members 1, 2 and 5, *no byte reaches the wire*. **For members 3 and 4 the
  guarantee must be stated one step earlier** — *no field terminator appears in the serialized
  text* — because §4.6 measured that this module is not on this repository's wire path. **This is a
  real difference and a promotion must state it rather than paper over it.** It is a difference in
  *where the boundary is*, not in the cause, the predicate or the timing.

### 11.3 Distinctions a promotion must not lose

1. **SR-AUD-249 is NOT a member** — a token-grammar defect (RFC 7230 separators in a subprotocol),
   not field-terminator injection. Already recorded in the cross-cutting appendix; re-verified here.
2. **SR-AUD-320/321 are NOT members** — grammar and full-consumption defects.
3. **#2129 is NOT a member** — CR/LF travelling *inward to the caller*, cause NH-K (§8.3).
4. **#2128 is NOT a member** — singleton/framing, no terminator involved.
5. **The identifier `CCF-021` has been proposed for two different families.** The `SearchValues.hpp`
   per-file report proposes it for a *public generic surface* shape, which `AUDIT_PROGRESS.md`
   records as "module-local and deliberately not minted as CCF-021". **Whoever mints must say which
   family it names.** Re-verified at this tip; unchanged.

### 11.4 What promotion would and would not change

- It would **not** change any finding's status. SR-AUD-319 and SR-AUD-322 stay `confirmed` until
  #2124 and #2123 land.
- It would **not** change ticket ownership. #2063 and #2089 stay closed; #2123 and #2124 stay this
  module's.
- It **would** give the three remediated members and the two open ones one named place recording
  that the predicate has one body, and it would make a sixth copy of the rule a visible family
  violation rather than a local choice.

### 11.5 The mint is a decision, and this review does **not** take it

The evidence obligation is now **discharged**: membership is complete (five findings, three
modules), the cause is one, the predicate is one, the timing is one, and the single boundary
difference is stated. Two things are still not this review's to decide:

1. **Every promotion sentence in the audit corpus is passive and names no agent** — the same
   obstacle #2109 identified for CCF-022 and did not resolve.
2. **The membership would be minted with two of five members open**, and one of them (#2124)
   requires a **new component edge** (§7). A family minted the day two members are unrepaired is
   defensible; minting it the day before deciding whether the edge is acceptable is not.

**CCF-021 is NOT minted.** Filed as decision ticket **#2131**, with three bounded options, a
recommendation and one exact approval sentence.

---

## 12. Test matrix

| Ticket | Required cases |
|---|---|
| **#2123** | CR, LF, CRLF, NUL, space, tab, and each RFC 9110 separator in a name → `false`; the collection is **unchanged** afterwards and `ToString()` contains no injected field; **the value stays unvalidated** (a CR-bearing *value* must still be accepted, or the ticket has changed a second contract); empty name still `false`; `Add`'s existing behaviour pinned |
| **#2124** | CR, LF, NUL at all **nine** doors of §4.1 including the two parameter-carrying types; `ViaHeaderValue`'s existing CR/LF rejection **pinned**; valid quoted strings with escapes still accepted; `ToString()` byte-checked for `\r`, `\n`, `\0` |
| **#2125** | `GMT trailing` at all six copies; the `"garbage"` control still fails; a valid date still parses; RFC 850 / asctime behaviour **pinned as-is** (#2130) |
| **#2126** | `p="a\";b"` at all seven splitters; the unescaped-`;` control still accepted; a trailing backslash and an unterminated quote still rejected |
| **#2127** | `iso-8859-1''foo-%E4.html` → 11 UTF-8 bytes; `UTF-8''` unchanged; `bogus''x` rejected; empty charset |
| **#2129** | `%0D`, `%0A`, `%00` in an RFC 5987 value → rejected; `ToString()` round-trip unchanged for valid values |
| **pins** | §6.2's seven measured positives |

## 13. Sanitizer and direct-resource matrix

| Tool | Applicable here? |
|---|---|
| **ASan** | **yes** — every parser is a hand-written index walk over `std::string`; truncated inputs and off-by-one at the end of a quoted string are the shape to hunt |
| **UBSan** | **yes** — index arithmetic, `sscanf` field widths, `intcs` narrowing in quality/range/status values |
| **LSan** | marginal — the module allocates only `std::string`/`std::vector`; kept in the run because it is free |
| **TSan** | **no subject** — no concurrency and no process-global mutable state (§9) |
| **`/proc/self/fd`** | **not applicable** — this module opens no descriptor |

---

## 14. Bounded tickets and recommended order

```
#2123  SR-AUD-322  TryAddWithoutValidation accepts invalid names   (P1, S) ── FIRST
#2124  SR-AUD-319  CR/LF/NUL at nine typed value doors             (P1, M) ── SECOND
#2125  SR-AUD-321  six copies of a non-consuming HTTP-date parser  (P2, M)
#2126  SR-AUD-320  seven escape-blind delimiter splitters          (P2, M)
#2127  SR-AUD-323  the RFC 5987 charset decoder                    (P2, M)
#2128  DESIGN: singleton headers and TE/CL coexistence             (P1, M) ── needs_user
#2129  an RFC 5987 value decodes to raw CR/LF for the caller       (P2, S)
#2130  DEFERRED VERIFICATION: RFC 850 / asctime HTTP-dates         (P3)
#2131  DECISION: mint CCF-021?                                     (P2)   ── needs_user
#2132  documentation and gated-behaviour pins                      (P3, S) ── LAST
```

**Recommended order: #2123, then #2124.** #2123 first because it is the only door that returns
`true` while producing an injected field — a caller checking the return value is told it succeeded —
and because its repair target already exists in the same file. #2124 second because it needs the
new component edge and should land once #2123 has proved the edge is acceptable.

## 15. Compatible versus blocked or deferred

| Ticket | Compatible? | Why |
|---|---|---|
| #2123 | **yes, with a documented narrowing** | a `true` return becomes `false` for invalid names |
| #2124 | **yes, with a documented narrowing** + **one new component edge** | |
| #2125 | **yes, with a documented narrowing** | |
| #2126 | **yes — a widening** | previously-rejected valid text starts parsing |
| #2127 | **yes in part** | the ISO-8859-1 transcoding needs §10's evidence |
| #2128 | **no** — design; changes what a long-standing public API accepts |
| #2129 | **yes, with a documented narrowing** | |
| #2130 | **no** — deferred verification |
| #2131 | **no** — decision |

## 16. Exclusions

- `modules/net-http`'s raw header map and wire serialization — closed by #2062/#2063/#2064.
- `modules/net`'s shared predicate — consumed, never modified.
- HPACK/QPACK — absent from this port.
- CNA and mobile-eggbert — not inspected; **#1773 stays blocked**.

## 17. Completion criteria

This review (#2122) is complete when this document exists, each of the five open findings has
exactly one disposition in §4, each post-audit defect carries a ticket or an explicit
"recorded, not ticketed", §14's tickets are in `plan.sqlite3`, and §11 discharges the CCF-021
evidence obligation. **It is complete on those terms and remediates nothing by itself.**

`modules/net-http-headers` is closed for *compatible* work when #2123–#2127, #2129 and #2132 are
`done`, SR-AUD-319/320/321/322/323 are `remediated`, and #2128 carries a design ticket and a
behaviour pin.

## 18. Implementation record

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.

### 18.1 #2123 — the prediction held exactly, and one mutation was needed to prove the *restraint*

§4.4 predicted the repair would be "route the name through `Add`'s validator, preserve the
unvalidated value". Implementation confirmed it without amendment: two call sites, one predicate
already in the same file, **no new component edge** (the predicate used is this module's own
`isToken`, not the `Net` component's — that edge is #2124's problem, not this ticket's).

**The interesting mutation is Q2, not Q1.** Q1 restores the defect and fails 3 tests — expected. Q2
does the opposite: it *over*-repairs, validating the **value** as well, and fails **exactly one**
test, `THECONTRACTTheVALUEIsStillDeliberatelyUnvalidated`. Without that test the over-repair would
have looked like a stricter, better fix while silently erasing the only reason the door exists. A
narrowing ticket needs a test that fires when the narrowing goes too far, not only one that fires
when it does not go far enough.

**Coverage:** every invalid class the probe measured, plus all seventeen RFC 9110 separators, plus
the collection asserted **unchanged** after each rejection — not merely that `false` was returned.

**+7 tests** (`SharpRuntimeTests_Net_Http_Headers` 373 → **380**). **ASan + UBSan + LSan clean over
53,256 operations** — 5,969 accepted, 47,287 rejected (`build-probe/2123_probe1_san.log`) — across
every byte value in both name and value position, every truncation prefix of a rich
`Content-Disposition` value, unterminated quotes, trailing backslashes, truncated percent-escapes,
and 8,000 fuzzed name/value pairs, with the repaired body compiled into the instrumented TU and a
live heap-use-after-free control.

**No signature, layout, vtable or exception-specification change. Graph unchanged at 41 / 91.**

### 18.2 #2124 — the prediction held on the mechanism and was wrong on three facts

§4.1's mechanism was exactly right: the field-terminator check lived in the **`else` branch** of
`NameValueHeaderValue::checkValueFormat`, so it ran only for an unquoted value, and the repair had
to sit in `NameValueHeaderValue` because five types hand out their parameter vector by mutable
reference. Measured over 414 door/payload pairs (`build-probe/2124_probe1_doors.cpp`, logs
`2124_probe1_before.log` / `2124_probe1_after.log`): **145 accept-and-emit results before, zero
after** — the 24 residuals in the "after" log are all non-terminator payloads matching the CRLF
that `HttpHeaders::ToString()` legitimately emits as framing.

**Three facts §4.1 got wrong, and how.**

1. **`ViaHeaderValue` did not already reject CR *and* LF.** §4.1 called the finding "exactly
   right" about it and said only NUL was its gap. `isValidReceivedBy` rejects on
   `find_first_of(" \t\r/,")`: **LF is not in that set and never was**, and neither is NUL.
   `ViaHeaderValue("1.1", "safe\nX")` was accepted and `ToString()` emitted a raw LF.
   `WarningHeaderValue::isValidAgent` is the same filter with the same gap. **The cause of the
   error is instructive**: the probe payload was `"safe\r\nX-Injected: yes"`, which rejects on
   its CR, so the LF was never tested independently. A per-character matrix would have caught it;
   a per-scenario one did not.
2. **Five mutable parameter/extension vectors, not two.** §4.1 named `MediaTypeHeaderValue` and
   `TransferCodingHeaderValue`; `ContentDispositionHeaderValue`, `CacheControlHeaderValue`
   (`getExtensionsProperty`) and `NameValueWithParametersHeaderValue` do it too. And there is a
   **tenth door no document names**: `RangeConditionHeaderValue(const std::string&)`, which
   forwards to `EntityTagHeaderValue` and inherited its gap.
3. **The module already held FOUR hand-written copies of the terminator rule**, and they
   disagreed: `HttpHeaders::checkValueChars`, `AuthenticationHeaderValue::containsNewLineOrNull`
   and `HttpRequestHeaders::checkNoNewlineOrNul` were all correct;
   `NameValueHeaderValue::checkValueFormat` was correct only outside quotes; and
   `ViaHeaderValue`/`WarningHeaderValue` had a fourth, narrower spelling that was not the rule at
   all. **That is cause NH-H applied to the terminator rule itself** — the module's dominant
   defect shape, hiding inside the repair target.

**The component edge, resolved.** §7 predicted it and it was needed: `Net.Http.Headers` now
declares `PRIVATE_DEPENDENCIES Net`, taking the graph from **41 / 91 to 41 / 92**, with
`docs/ComponentCatalog.md` regenerated and `validate_module_boundaries.py` confirming no cycle.
`PRIVATE` is correct because no public header here includes anything from `Net`, so no consumer's
include surface grows. **It was declared, not deferred**, on the basis that `CLAUDE.md` reserves
per-action approval for pushes, tags, merges, the two-job ceiling and broad header refactors — not
for declaring a component edge — and that #1814 declared a *public* edge (90 → 91) as ordinary
work under the same policy. The alternative, a fifth copy of the predicate, **is** cause NH-H.

**Where the repair sits, and why it is that small.** Because every parameter *is* a
`NameValueHeaderValue`, one check in its constructor and `setValueProperty` closes all five
mutable vectors, `ContentDispositionHeaderValue`'s `Name`/`FileName` (they route through
`setOrAddParameter`), and every parameter-carrying `TryParse` (they route through
`NameValueHeaderValue::Parse`). `TryParse` needed its own guard because it assigns the fields
directly and never reaches `checkValueFormat`. `EntityTagHeaderValue`, `WarningHeaderValue` (text
**and** agent) and `ViaHeaderValue` carry their own copies of the quoting/host grammar and each
needed the predicate added. The three already-correct doors were re-expressed through the shared
body with **no change to what they accept**, pinned by its own test.

**One thing the repair adds that no ticket asked for**, because
`ProtocolFieldValidation.hpp`'s doc-comment states it as a companion rule with no code: the new
rejection messages **do not echo the offending text**. The pre-existing messages in this module
do (`"The value is not valid: " + value`), which is why the terminator check is a separate,
earlier branch at every door rather than folded into the grammar failure below it.

**Mutations.** **M1 under-repair** — restore the pre-#2124 shape, terminator check back in the
`else` branch: rebuilt (binary hash changed), **6 tests fail**. **M2 over-repair** — reject every
C0 control and DEL rather than the three terminators: **exactly one test fails**,
`THECONTROLNonTerminatingCharactersAreStillAccepted`. Without that control the over-repair would
have looked like a stricter, better fix while narrowing the accepted set past what the frame
grammar requires — the same lesson #2123's Q2 taught, in the opposite direction. **M3 control** —
re-spell the shared predicate call at one site as the literal pre-#2124 expression, semantics
identical: **0 failures**, proving the suite measures behaviour and not spelling.

**+14 tests** (`SharpRuntimeTests_Net_Http_Headers` 380 → **394**), in a new file
`HttpHeaderValueTerminatorTests.cpp`. **ASan + UBSan + LSan clean over 167,926 operations**
(84,379 accepted / 83,547 rejected, `build-probe/2124_probe2_san.log`) across every byte value in
a value position and as a quoted-pair escapee, every truncation prefix of a rich
`Content-Disposition` value, degenerate quoting (unterminated quotes, trailing backslashes,
truncated percent-escapes), 4,000 fuzzed values and embedded-NUL values — with all eight repaired
bodies compiled into the instrumented TU and a live heap-use-after-free control proving the
instrumentation answered. **TSan has no subject** and **`/proc/self/fd` is not applicable** (§13).

**No signature, layout, vtable or exception-specification change. Graph 41 / 91 → 41 / 92.**

### 18.3 #2125 — the copies were **seven**, and the interesting question was where to stop

§4.3 counted **six** copies of the `sscanf` HTTP-date parser. There are **seven**: it did not name
`WarningHeaderValue`'s date field, which extracts the date from inside a quoted string and hands it
to its own byte-identical copy. All seven are now one body,
`modules/net-http-headers/src/System/Net/Http/Headers/HttpDateParser.hpp` — an
**implementation-only** header beside the sources, not under `include/`, so it does not become
public surface (the placement and the plain relative include follow `modules/xml/src`'s
`XPathAstInternal.hpp` / `XmlNodeChangeEvents.hpp`).

**The restraint is the substance of this ticket.** The obvious repair is a hand-written fixed-width
scanner, and it would be wrong here: it would *also* reject text `sscanf` accepts — a signed day
field, an over-wide year — and with `/rv` absent this repository has no evidence for what .NET does
with any of it. The conversion string is therefore kept **verbatim** with `%n` appended, so every
value that parsed before parses to the same instant and every value that failed before still fails.
Full consumption is what #2125 owns; grammar is not.

**Two things added that the ticket did not name**, both because the shared body made them visible:
an **embedded NUL is rejected before `c_str()`**, because otherwise `sscanf` would report a complete
match over a prefix of a value the caller never bounded; and **trailing whitespace stays accepted**,
because it was accepted before and is not what the finding is about.

**The obsolete formats, measured — this is the pin §14's acceptance criteria demanded.** The
conversion string requires a comma immediately after a three-letter day name, which **both** the
RFC 850 and the ANSI C `asctime` forms fail. Neither was **ever** accepted, so #2125 cannot have
narrowed a required form away. RFC 9110 §5.6.7 requires a *recipient* to accept all three, so this
is a real gap — and closing it is a **widening**, which is **#2130**'s question and stays deferred.

**Mutations, and one that exposed a vacuous test.** **M1 under-repair** (drop the consumption loop):
**2 tests fail**. **M2 over-repair** (require `consumed == size`, rejecting trailing whitespace):
**passed everything on the first attempt** — because the test asserted through
`RetryConditionHeaderValue::TryParse`, which **trims its input before the date parser ever sees
it**, so the assertion could not discriminate. Re-pointed at `HttpResponseHeaders`, which reads the
stored raw value untrimmed, M2 then failed **exactly one** test. The mutation did not find a bug in
the repair; it found a bug in the test, which is the other thing mutations are for. **M3 control**
(re-spell `std::isspace` as an explicit six-character comparison): **0 failures**.

**+7 tests** (`SharpRuntimeTests_Net_Http_Headers` 394 → **401**). Sanitizer coverage folded into
the batch probe: **ASan + UBSan + LSan clean over 200,040 operations** (116,000 accepted / 84,040
rejected, `build-probe/2124_probe2_san.log`), now including every truncation prefix and every
single-byte corruption of a valid HTTP-date, both obsolete forms, and NUL-bearing dates, with
twelve repaired bodies compiled into the instrumented TU and a live heap-use-after-free control.

**No signature, layout, vtable or exception-specification change. Graph unchanged at 41 / 92.**

### 18.4 #2126 — six were escape-blind; the seventh tracked no quoting at all

§4.2's sharpening held for six of the seven: they *do* track quotes and simply cannot see a
quoted-pair, which is why an **unescaped** `;` inside a quoted parameter was accepted (the control)
while `text/plain; p="a\";b"` was rejected. All six now call one body,
`src/System/Net/Http/Headers/HeaderFieldSplitter.hpp` (implementation-only, beside the sources).

**The seventh is worse than the plan says, and no document names it.**
`NameValueWithParametersHeaderValue::TryParse` split on a bare `input.find(';')` with **no quote
tracking whatsoever**, so §4.2's own control case — the unescaped delimiter inside quotes — was
split *there* too. Describing all seven as "escape-blind" understates one of them; it is folded
into the same body and has its own test, because that assertion is the only thing that
distinguishes *escape*-blind from *quote*-blind.

**The scanner deliberately does not validate.** An unterminated quote or a trailing backslash still
produces one final segment that the caller's own grammar check rejects, exactly as before.
Splitting and validating stay separate, which is why the widening cannot leak into acceptance of
malformed quoting — pinned by its own test.

**A backslash OUTSIDE a quoted-string stays an ordinary character**, because `quoted-pair` occurs
only inside `quoted-string` and `comment`.

**Mutations, and a second vacuous assertion caught.** **M1 under-repair** (ignore the escape):
**2 tests fail**. **M2 over-repair** (honour the escape outside quotes too): **passed everything on
the first attempt**. The assertion was `EXPECT_FALSE(MediaTypeHeaderValue::TryParse(...))`, and the
over-repair merges two segments into one still-invalid segment — so `TryParse` returns `false`
either way and the test could not see the difference. Re-pointed at `Accept`, whose `parseList`
*skips* an unparsable element rather than failing the whole value, the observable becomes the
element **count** (2-with-one-invalid versus 1-long-invalid) and M2 fails **exactly one** test.
That is the second time in this batch a mutation found a vacuous test rather than a broken repair;
both are recorded rather than quietly fixed. **M3 control** (respell the append as
`current.append(1, c).append(1, …)`): **0 failures**.

**+7 tests** (`SharpRuntimeTests_Net_Http_Headers` 401 → **408**). Sanitizer coverage folded into
the batch probe, now with the four parameter-carrying bodies also compiled in: **ASan + UBSan +
LSan clean over 200,040 operations**. Note the accept/reject split is unchanged by #2126, and that
is expected rather than suspicious — the probe counts *thrown* results, and a `TryParse` widening
changes a `bool`, not a throw.

**This is a widening.** Nothing that parsed before stops parsing (pinned), and text valid per
RFC 9110 starts parsing. **No signature, layout, vtable or exception-specification change. Graph
unchanged at 41 / 92.**

### 18.5 #2127 and #2129 — one decoder, two defects, and one narrowing neither ticket named

Both live in `ContentDispositionHeaderValue`'s `tryDecode5987`, so they landed together while
staying separate tickets in the records.

**#2127 (SR-AUD-323, cause NH-J).** The charset label was parsed only far enough to locate the
delimiters and then discarded. `filename*=iso-8859-1''foo-%E4.html` produced **ten** bytes with a
raw `0xE4` — not text, in a `std::string` the rest of this runtime reads as UTF-8 — and
`filename*=bogus''x` was accepted. `UTF-8` and `ISO-8859-1` are now honoured case-insensitively per
RFC 5987 §3.2.1, an ISO-8859-1 octet is transcoded (11 bytes, `C3 A4`), and anything else —
**including an empty label** — is rejected.

**The §10 evidence gap did not have to split the ticket.** Whether .NET rejects an unsupported
label or drops it silently is undecidable here with `/rv` absent, so the plan scoped a possible
split. It was not needed: rejection is what RFC 5987 §3.2.1 requires of a recipient, and it is the
failure mode this decoder already used for every other malformed input — the getter reports the
parameter absent. Recorded as **this port's choice** in the property's doc-comment rather than as a
claim about .NET.

**A third defect neither the finding nor the review names.** The escape bound was
`i + 2 < encoded.size()`, so a **truncated** escape at the end of the value (`a%`, `a%C`) fell
through to the literal branch and was kept as text — silently turning malformed input into a
plausible-looking file name, at a door whose whole job is producing file names. It is rejected, for
the same reason the decoder already rejected a non-hex escape. This is a narrowing, and it is
recorded here because it is not in #2127's acceptance criteria.

**#2129 (post-audit, cause NH-K).** `filename*=UTF-8''a%0D%0Ab` decoded to a raw CR/LF handed to
the caller. It uses the family's predicate because the same three characters are the hazard —
**not** because it is a CCF-021 member. §8.3's exclusion stands and is now pinned from both sides:
the terminator case is rejected, and a decoded **TAB or ESC is still returned**, so the narrowing
cannot creep from "the three characters that terminate a field" to "control characters".

**Mutations, four, all discriminating.** #2127 under-repair (discard the label again): **2 tests
fail**. #2127 over-repair (accept `UTF-8` only): **1 test fails** — the ISO-8859-1 transcoding.
#2129 under-repair (drop the inward check): **1 test fails**. #2129 over-repair (reject every C0
control): **1 test fails** — and it is the scope control, not the defect test, which is the point.

**+8 tests** (`SharpRuntimeTests_Net_Http_Headers` 408 → **416**). ASan + UBSan + LSan clean over
200,040 operations in the batch probe, whose corpus already walks every truncation prefix of a rich
`Content-Disposition` value and every truncated percent-escape.

**No signature, layout, vtable or exception-specification change. Graph unchanged at 41 / 92.**

### 18.6 #2132 — the pins, and one fact about #2128 the review did not have

§6.2's positives and #2128's current behaviour are pinned in
`HeadersGatedBehaviourPins.cpp`; #2130's deferred question is pinned in
`HttpDateConsumptionTests.cpp` instead, beside the parser it constrains, rather than in a file a
reader would have to know to look in.

**The new fact, and it changes what #2128 has to decide.** Over a comma-joined singleton the two
typed accessors already **disagree**:

| Header | `Add` twice | Typed accessor |
|---|---|---|
| `Content-Length` | serializes `Content-Length: 10,20` | reports the value **absent** |
| `Host` | serializes `Host: a.example,b.example` | returns the **joined text** |

So this port is already half-way to option (b) — enforce at the typed accessor — for one header, by
accident of the numeric parse failing on `"10,20"`, and not at all for the other. #2128 cannot pick
an option without reconciling those two, and that was not visible when the ticket was written.

**#2128 is NOT implemented**, and this is the boundary the batch instruction draws: pinning what a
`needs_user` ticket currently does is the opposite of quietly deciding it. Every pin here is
written so that a failure means *someone took the decision*, not *something regressed*.

**+7 tests** (`SharpRuntimeTests_Net_Http_Headers` 416 → **423**). No production change.

---

## 19. Namespace reconciliation — every finding and defect, one disposition each (2026-08-08)

### 19.1 The five audit findings

| Finding | Sev | Disposition | Ticket | Status |
|---|---|---|---|---|
| SR-AUD-319 | high | **remediated** | #2124 | done |
| SR-AUD-320 | med | **remediated** | #2126 | done |
| SR-AUD-321 | med | **remediated** | #2125 | done |
| SR-AUD-322 | high | **remediated** | #2123 | done |
| SR-AUD-323 | med | **remediated** | #2127 | done |

**All five. The module went from zero remediated findings to five in two batches.**

### 19.2 The post-audit defects (ordinary ticket numbers, no `SR-AUD-*`)

| Defect | Ticket | Disposition |
|---|---|---|
| an RFC 5987 value decodes to a raw CR/LF for the caller (NH-K) | **#2129** | **done** |
| singleton headers comma-joined; TE + CL coexist (NH-L) | **#2128** | **`needs_user`** — design; behaviour **pinned** by #2132 |
| RFC 850 / asctime HTTP-dates | **#2130** | **`todo`, deferred verification** — the port side is measured and pinned (§18.3); the .NET side needs `/rv` |
| documentation and gated-behaviour pins | **#2132** | **done** |

### 19.3 Defects found during implementation that no document had

Recorded here because they are the difference between what the review predicted and what
implementation measured, and because three of them changed what shipped:

1. **`ViaHeaderValue` and `WarningHeaderValue` accepted a bare LF and a bare NUL** — §4.1 said the
   opposite. Closed by #2124 (§18.2).
2. **`RangeConditionHeaderValue(std::string)` is a tenth field-terminator door.** Closed by #2124.
3. **Five mutable parameter/extension vectors, not two.** Governed by #2124.
4. **Four hand-written copies of the terminator rule inside the module.** Unified by #2124.
5. **A seventh HTTP-date parser copy** (`WarningHeaderValue`). Unified by #2125.
6. **The seventh list splitter tracked no quoting at all** (`NameValueWithParametersHeaderValue`),
   not merely no escapes. Unified by #2126.
7. **A truncated percent-escape was kept as literal text** in the RFC 5987 decoder. Closed by #2127.
8. **The two typed singleton accessors disagree** over a comma-joined value (§18.6). Handed to
   #2128, not decided.

### 19.4 Is the namespace complete?

**Yes, except for gated and deferred work.** §17's criterion was: #2123–#2127, #2129 and #2132
`done`, SR-AUD-319/320/321/322/323 `remediated`, and #2128 carrying a design ticket and a behaviour
pin. **Every clause is satisfied.** What remains is exactly two items, both correctly classified:

- **#2128** — `needs_user`. A genuine architecture decision about what a long-standing public API
  accepts, now with the §18.6 fact it was missing.
- **#2130** — `todo`, deferred verification. Undecidable here: it asks what **.NET** does, and
  `/rv/tmp/runtime/` is absent. The port-side half is measured and pinned.

**No ticket in this namespace is blocked**, and none needed an object-layout, vtable or
exception-specification change — §7 predicted that and it held. The only architectural change was
the one component edge §7 predicted, `Net.Http.Headers` → `Net`, **41 modules / 91 → 92 edges**.

**Test count: 373 → 423** across the two batches (`SharpRuntimeTests_Net_Http_Headers`).

