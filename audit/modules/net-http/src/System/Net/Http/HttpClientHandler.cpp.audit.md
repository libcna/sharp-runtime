# Audit: `modules/net-http/src/System/Net/Http/HttpClientHandler.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [HttpClientHandler.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/HttpClientHandler.cs).
- Evidence: source review; loopback integration cannot create a socket in this
  sandbox (`Socket::Socket: socket() failed`).

## Assessment

### SR-AUD-313 — high — HTTP/MIME serializer accepts CR/LF-bearing metadata and emits extra fields

Request/default headers are written as raw `name + ": " + value`; content
type/charset, multipart subtype, and form-data name/file name take the same
unvalidated serialization path.  The direct multipart probe emits separately
parsed injected fields from CR/LF-bearing subtype/name/file input.  Managed
header value objects reject newline/NUL and quote/encode parameters.

Required remediation: centralize field-name/value and media/disposition
parameter validation/escaping before any serialization; reject CR, LF, NUL,
invalid tokens, and duplicate protected fields rather than writing raw text.

**REMEDIATED — ticket #2063, 2026-08-04.** *(Appended; the original finding text
above is preserved verbatim.)*

The finding's own list — *"content type/charset, multipart subtype, and
form-data name/file name take the same unvalidated serialization path"* — is
**correct and complete**, and the namespace review's §4.1 paraphrase of it
("header, media-type and disposition concatenation") narrowed the derived ticket
to headers only. Measured against `257106a`
(`build-probe/2063_probe1_doors.cpp`, log `2063_probe1_before.log`), **ten**
public doors were open, including every one this finding names:

| Door | Open before |
|---|---|
| `HttpRequestMessage::setHeader` name and value | yes |
| `HttpResponseMessage::setHeader` name and value | yes |
| `HttpClient::setDefaultHeader` name and value | yes |
| `HttpResponseMessage::setReasonPhraseProperty` | yes |
| `HttpClient::parseUrl` — the authority | yes |
| `HttpClient::parseUrl` — the **path**, i.e. the request **LINE** | yes |
| `HttpClient::parseStatusLine` — the whole line | yes |
| `StringContent` charset + media type, and the three other contents' media type | yes |
| `MultipartContent` subtype | yes |
| `MultipartFormDataContent::Add` name and file name | yes |

Two of those were not in the derived ticket's original scope and are the reason
this note exists. The **path** vector is worse than the authority one the review
named: `HttpClientHandler::Send` writes
`method << " " << purl.path << " HTTP/1.1\r\n"`, so a CRLF in the path injects a
second **request line**, not one header field. And this report's own multipart
claim reproduced exactly — `MultipartFormDataContent::Add(content, "na\r\nX-Injected: yes")`
emitted `Content-Disposition: form-data; name="na\r\nX-Injected: yes"`.

The repair rejects CR, LF and NUL at all ten doors through one shared
`System::Net::Http::detail` helper, and validates the **whole URL string** once
at the top of `parseUrl` rather than the parsed components. Two internal sites
are guarded for the same reason: a **response header line** carrying one of the
three characters (`HttpRequestException`, so a malformed response stays a
response error) and the `Cookie` header value synthesised from
`CookieContainer` — the one request header value on that path with no public
door in front of it.

Not done here, and deliberately: **escaping** and **duplicate protected fields**.
The required-remediation text above asks for both. Escaping a MIME parameter
changes emitted bytes for text that is currently legal, and de-duplicating the
`Host`/`User-Agent`/`Accept`/`Connection` fields the handler writes
unconditionally requires the case-insensitive lookup that blocked ticket
**#2068** introduces. Both stay open; the duplicate-`Host` behaviour is
**pinned** by `NetHttpGatedBehaviourPins.Pin2068_HandlerEmitsADuplicateDefaultHeader`.
The `invalid tokens` clause is likewise out of scope: only the three characters
that terminate a frame are rejected, and a wider token grammar has no
repository-contained evidence behind it (`/rv/tmp/runtime/` absent, re-verified
2026-08-04).

Closure evidence: +21 permanent regressions in `HttpClientTests.cpp` covering
each character at the start, middle and end of every door, an end-to-end
assertion that an injected request URI **opens no socket at all**, mutation
check (emptying the shared predicate fails exactly the 13 rejection tests while
all 8 acceptance tests and all 7 pins stay green), and ASan/UBSan/LSan clean
over 130 rejections and 19 acceptances with the four changed `.cpp` bodies
compiled **from source** plus a control heap-buffer-overflow proving
instrumentation (`build-probe/2063_probe3_asan.log`). No signature, layout,
vtable or exception-specification change. Deliberate narrowing, documented in
`docs/Migration-HttpControlCharacterRejection.md`.

### SR-AUD-318 — medium — terminal response parsing has unbounded buffering, weak framing checks, and leaks connected sockets on exceptions

`recvLine`, `recvAll`, chunks, and body accumulation have no configured size or
line limit.  `stoll`/`stoul` accept numeric prefixes; negative Content-Length
falls into EOF framing, malformed header lines are silently skipped, and chunk
trailing CRLF is not checked.  Any parsing/receive exception after
`connectToHost` bypasses the sole `platformClose(fd)` at the normal return
path.  A hostile or failed peer can therefore retain descriptors and consume
unbounded memory rather than yield a classified bounded error.

Required remediation: use RAII socket ownership; enforce configurable limits
for status/header/body/chunk sizes; parse complete nonnegative decimal/hex
tokens; reject bad field/framing lines and map failures to `HttpRequestError`.

## Missing assertions and diagnostics

The six loopback tests are environment-limited here.  They also omit suffix and
negative lengths, duplicate/conflicting Content-Length, bad chunk CRLF,
oversized lines/bodies, malformed headers, descriptor closure after errors,
lower-case `head`, and CR/LF request/header serialization.

## Post-audit record (ticket #2065, 2026-08-04): SR-AUD-318's leak half is remediated

The audit evidence above is retained unchanged. The owning review is
[`docs/SystemNetHttpNamespaceReviewPlan.md`](../../../../../../docs/SystemNetHttpNamespaceReviewPlan.md)
(ticket #2062); **no `SR-AUD-*` identifier was issued and numbering stays frozen at 364.**

**A clause this report filed last turned out to be the module's most consequential compatible
defect, and was repaired.** SR-AUD-318 bundles three claims — unbounded buffering, weak framing,
and *"exceptions after connect bypass the only socket close"*. The third is separable, and
quantifying it changed its priority:

| Server response | Requests | Threw | Descriptors leaked (before) | After |
|---|---:|---:|---:|---:|
| well-formed | 20 | 0 | 0 | 0 |
| garbled status line | 20 | 20 | **20** | **0** |
| `Content-Length: abc` | 20 | 20 | **20** | **0** |
| chunk size `ZZZ` | 20 | 20 | **20** | **0** |
| body shorter than `Content-Length` | 20 | 20 | **20** | **0** |

One descriptor per failing request, from four independent failure paths, **every one of them
chosen by the remote peer**. A server that answers roughly 1,024 requests with a garbled status
line exhausts a default `RLIMIT_NOFILE`, after which the process cannot open a file, a socket or
a pipe. That is a remote resource-exhaustion channel, not a hygiene issue, which is why the
review made it P1 and first in the execution order rather than leaving it inside a medium
finding.

**The repair** is a `SocketGuard` that owns the descriptor for the rest of `Send()`. It
deliberately keeps the **original** close point on the success path — `Close()` is called
exactly where `platformClose(fd)` used to be, and is idempotent, so the destructor is a no-op
after it has run. The only behaviour that changes is that a failing path now closes too.

**Closure evidence.** 5 permanent regressions, one per mode, asserting the process's own
open-descriptor delta is 0 **and** that the expected number of requests actually threw — so a
mode that stopped failing would be reported rather than counted as a pass. Mutation-checked:
emptying the guard's destructor fails exactly the four failure-path tests, each with the
pre-repair count of 19, while the success-path test stays green. That asymmetry is what proves
the mutation was targeted and that the success path's close comes from the retained explicit
call rather than from the destructor.

**Sanitizers.** ASan/UBSan/LSan clean, with `HttpClientHandler.cpp` and `HttpClient.cpp`
compiled **from source** into the probe — `Net.Http` is a `STATIC` component, so a probe that
merely linked `libsharp_runtime_net_http.a` would have measured an uninstrumented body. Stated
plainly: **LSan does not cover this defect.** It tracks memory allocations, not file
descriptors, so a clean LSan run says nothing about a leaked socket. The `/proc/self/fd` count
is the instrument, and no clean sanitizer run is offered as a substitute for it. On a platform
without `/proc/self/fd` the tests **skip** rather than pass — a missing instrument is not a
passing measurement.

**Source, ABI and layout consequences: none.** `SocketGuard` is file-local to
`HttpClientHandler.cpp`; no header, signature, exported symbol or object layout changed.

**Not remediated, and not claimed.** The other two thirds of SR-AUD-318 are untouched:
`recvAll` and a `Content-Length`-bounded `recvExact` still accumulate without bound, and
`std::stoul(chunkLine, nullptr, 16)` still accepts a chunk size up to `SIZE_MAX`. Bounding them
is a **public-surface addition** — .NET's own knob is
`HttpClient.MaxResponseContentBufferSize`, which this port has no equivalent of — so it is
blocked ticket **#2071**, and SR-AUD-318 stays `confirmed` for that half.
