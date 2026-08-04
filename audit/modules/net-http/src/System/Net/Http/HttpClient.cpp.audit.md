# Audit: `modules/net-http/src/System/Net/Http/HttpClient.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [HttpClient.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/HttpClient.cs).
- Evidence: `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp` and ASan
  `/tmp/sharp-runtime-net-http-audit/http_client_async_lifetime.cpp`.

## Assessment

### SR-AUD-310 — high — HttpClient asynchronous methods retain a raw client pointer after the client may be destroyed

Every async wrapper creates a `TaskT` lambda capturing `[this, ...]`.  A probe
starts `GetAsync` through a no-network custom handler, destroys its
`HttpClient`, then waits.  ASan reports heap-use-after-free in
`HttpClient::Send` at `HttpClient.cpp:158` while iterating `defaultHeaders_`.
Managed async operations retain the operation/client state until completion.

Required remediation: move operation state needed by async paths into shared,
owned state (or otherwise retain the client through completion), make disposal
and outstanding work explicit, and add ASan lifetime regression coverage.

### SR-AUD-311 — medium — ad-hoc URL parser accepts malformed authority text and loses query-only request targets

The direct probe accepts `:80trailer`, negative and out-of-range ports, and
`[::1]trailer`; `http://example.com?query=without-slash` becomes a host named
`example.com?query=without-slash` with path `/`.  `std::stoi` also accepts a
numeric prefix without checking complete consumption.  The managed route is a
validated `Uri` and preserves the authority/query grammar.

Required remediation: parse via the local Uri contract or a complete authority
parser; require full decimal-port consumption/range 1..65535, exact bracket
termination, and `/?#` target separation.

**REMEDIATED — ticket #2064, 2026-08-04.** *(Appended; the original finding text
above is preserved verbatim.)*

Every claim above reproduced. The repair routes both numeric fields through one
file-local full-consumption parser (the entire text must be ASCII digits, no
sign, no whitespace, no tail, and the value must fall inside the domain) and
ends the authority at the first `/`, `?` or `#`. That is CCF-002's remedy shape
reduced to what this module needs; no new family was minted.

**One defect this finding does not name** (review plan §6.3): the host was never
lowercased while the scheme already was, so `HOST.EXAMPLE` and `host.example`
were distinct hosts to the cookie container, to the `Host:` header a server
compares, and to any cache keyed on the parsed result (RFC 3986 §3.2.2). It is
lowercased now.

**Three deliberate divergences from the required-remediation text above, each
with its reason:**

1. **The port domain is 0…65535, not 1…65535.** `port = *DIGIT` in RFC 3986
   §3.2.3 admits `0`, .NET's `Uri` accepts it, and the derived ticket's
   acceptance criteria name `host:0` as a case that must stay **accepted**.
   Rejecting it would be a narrowing this repository has no evidence for.
2. **Parsing does not go "via the local Uri contract".** `System::Uri` is a
   `PRIVATE_DEPENDENCY` of this component and carries its own open findings
   (SR-AUD-141…148, blocked tickets #1995–#1999, plus #2003 and #2005). Routing
   `parseUrl` through it would import an unreviewed grammar into a public static
   member and couple two blocked queues. The bounded parser is the smaller
   change.
3. **Exact bracket termination is NOT implemented.** `http://[::1]x/p` still
   returns host `::1` with the trailing `x` silently discarded. It shares the
   authority split with the userinfo question (`http://user@host/p` still
   reaches DNS as `user@host`), neither is in the derived ticket's row list, and
   rejecting either is a narrowing with no repository-contained evidence behind
   it. Both are recorded as **deferred ticket #2072**; nothing about them is
   approved and nothing about them is claimed closed. **This clause of the
   required remediation therefore remains open.**

Closure evidence: +9 permanent regressions plus 5 interaction tests with #2063,
seven mutations each reverted from an exact backup (reverting the full-consumption
parser fails 4 tests, the authority split 2, the lowercasing 2, the fragment
handling 2), and ASan/UBSan/LSan clean with `HttpClient.cpp` compiled **from
source** plus a control heap-buffer-overflow proving instrumentation
(`build-probe/2064_probe2_asan.log`). Not one pre-existing test needed updating.
No signature, layout, vtable or exception-specification change.

### SR-AUD-312 — medium — status-line parser turns malformed response text into arbitrary or successful status codes

`parseStatusLine("HTTP/1.1 200trailer OK")` returns 200; `NOTHTTP 999
arbitrary`, 99, and 1000 are accepted too.  The parser only finds spaces and
uses prefix-accepting `std::stoi`, instead of validating an HTTP version and
exactly three status digits.

Required remediation: require supported `HTTP/1.0`/`HTTP/1.1` framing,
exactly three ASCII digits in the valid range, and a complete deterministic
error category.

**REMEDIATED — ticket #2064, 2026-08-04.** *(Appended; the original finding text
above is preserved verbatim.)*

**Premise correction (review plan §6.2): the version token was never parsed at
all.** This report reads as though an existing version check were too lenient;
measured, there was **no** check. `parseStatusLine("GARBAGE 200 OK")` returned
200, so a response that is not HTTP was reported as a successful HTTP response.
There was nothing to tighten; there was a check to **add**.

The token must now match `HTTP/<digit>.<digit>` (RFC 9112 §2.3) and the status
code must be exactly three ASCII digits (RFC 9112 §4), through the same
full-consumption parser the port uses. `200trailer`, `2`, `20`, `-5`, `+5` and
`99999` are all rejected. The `-5` row is the one with a public consequence: the
handler casts the parsed value into `System::Net::HttpStatusCode`, a **public
enum**, which therefore held a value no enumerator names.

**One deliberate divergence from the required-remediation text above.** It asks
for *"supported `HTTP/1.0`/`HTTP/1.1` framing"*. This port accepts any
well-formed `HTTP/<digit>.<digit>` token, so `HTTP/9.9 200 OK` parses. A version
this port does not speak is the **server's** behaviour to report to the caller,
not a parse error, and no repository-contained evidence says .NET rejects it
(`/rv/tmp/runtime/` absent, re-verified 2026-08-04). Recorded as this port's
choice and **pinned** by `UnknownButWellFormedVersionIsAccepted`; narrowing to
1.0/1.1 remains available as a separate, evidence-backed decision. `HTTP/1.1 099
OK` is likewise **accepted** as the code 99 and pinned — three digits is all the
grammar asks for.

**A sanitizer premise was corrected by measurement.** The review's §12 nominated
UBSan's invalid-enum-value report as the instrument for the `-5` cast. Measured
with a dedicated discrimination control (`build-probe/2064_probe3_enumctl.cpp`,
log `2064_probe3_enumctl.log`), `-fsanitize=enum -fno-sanitize-recover=undefined`
**reports nothing and exits 0**: `HttpStatusCode` is an `enum class` with the
implicit `int` underlying type, so every `int` is inside its value range and
there is no undefined behaviour to see. **UBSan is not a discriminating
instrument here**, the clean run is recorded as a non-discriminating
confirmation, and the behavioural test `NoStatusCodeCanEscapeTheEnumDomain` is
the closure evidence.

Closure evidence: +7 permanent regressions, the shared #2063 interaction suite,
and an end-to-end test that a `GARBAGE 200 OK` server is rejected **without**
reintroducing #2065's descriptor leak. Mutation-checked: disabling the version
check fails exactly 2 tests; dropping the three-digit requirement fails exactly
1. No signature, layout or ABI change.

### SR-AUD-314 — medium — HttpClient permits a request message to be sent repeatedly

Unlike the reference's `HttpRequestMessage.MarkAsSent` lifecycle guard,
`HttpClient::Send` has no request state check.  A no-network counting handler
in the direct probe receives the same request twice (`request-reuse-calls=2`).
Repeated sends can reuse content and per-request state after an operation has
already begun, contrary to the managed request lifetime contract.

Required remediation: represent sent/disposed request state, atomically reject
the second send before header mutation/dispatch, and define a safe retry route
that creates a new request when needed.

## Missing assertions and diagnostics

Existing tests cover nominal URL/status cases and only nonnumeric failures.
They omit every demonstrated suffix/range/version/query case, request reuse,
base-address validation, and the ASan client-lifetime scenario.
