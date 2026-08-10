<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration: `System::Net::Http` rejects CR, LF and NUL at every public protocol-field door

*Narrowing change landed by ticket **#2063** on 2026-08-04, remediating audit finding
**SR-AUD-313** and the reason-phrase half of **SR-AUD-316** (root-cause family **NH-B**).
Durable design record: [`docs/SystemNetHttpNamespaceReviewPlan.md`](SystemNetHttpNamespaceReviewPlan.md)
§4.1, §5.2 and §20.3.*

---

## 1. What changed, in one line

Every public door of `System::Net::Http` that accepts text this port concatenates into an
HTTP/1.1 or MIME **frame** now rejects a carriage return (`\r`), a line feed (`\n`) or a NUL
(`\0`) in that text. Before #2063 every one of those doors accepted such text and wrote it out
verbatim.

## 2. Why

CR and LF terminate a field in an HTTP/1.1 request line, status line or header, and in a MIME
part header. NUL truncates every C-string-shaped consumer downstream of the wire. A value
carrying one of them therefore does not travel as a *value* — it becomes structure. Measured
against the tree at `257106a` (`build-probe/2063_probe1_before.log`):

```
request->setHeader("X-A", "v\r\nX-Injected: yes")   ->  two header fields on the wire
parseUrl("http://ho\r\nst/p")                       ->  host "ho\r\nst"  -> two Host-derived fields
parseUrl("http://host/pa\r\nX: y")                  ->  path "/pa\r\nX: y" -> a second REQUEST LINE
MultipartFormDataContent::Add(c, "na\r\nX-Injected: yes")
                                                    ->  a separately parsed MIME header field
```

The third of those is request smuggling: the injected text lands in the request **line**, not
in a header value.

## 3. This is a source-compatible, behaviour-narrowing change

- **No** signature, member, base class, virtual, vtable, object layout or exception
  specification changed. Every existing call still **compiles**.
- What changed is the set of accepted **inputs**. Code that passed CR/LF/NUL-bearing text
  through one of the doors below now gets an exception where it previously got silent
  serialization.
- No rebuild is required for ABI reasons. A rebuild is required only in the ordinary sense
  that the headers changed.

## 4. The complete door list, and what each throws

| Door | Text validated | Throws |
|---|---|---|
| `HttpRequestMessage::setHeader` | name, value | `System::FormatException` |
| `HttpResponseMessage::setHeader` | name, value | `System::FormatException` |
| `HttpResponseMessage::setReasonPhraseProperty` | value | `System::FormatException` |
| `HttpClient::setDefaultHeader` | name, value | `System::FormatException` |
| `HttpClient::parseUrl` | the **whole** URL string | `System::UriFormatException` |
| `HttpClient::parseStatusLine` | the **whole** status line | `HttpRequestException` |
| `StringContent` ctor | charset, media type | `System::FormatException` |
| `ByteArrayContent` / `ReadOnlyMemoryContent` / `StreamContent` ctors | media type | `System::FormatException` |
| `MultipartContent` ctor | subtype | `System::ArgumentException` |
| `MultipartFormDataContent::Add` | name, file name | `System::ArgumentException` |

Two internal, non-public sites are guarded for the same reason and are listed for
completeness: `HttpClientHandler::Send` rejects a **response header line** carrying one of the
three characters (`HttpRequestException`), and it rejects a `Cookie` header value synthesised
from `CookieContainer` (`System::FormatException`) — the one request header value on that path
with no public door in front of it.

### Why three exception types rather than one

`System::UriFormatException` **is** a `System::FormatException`, so nine of the ten public
doors report through one catchable base: `catch (const System::FormatException&)` covers
every protocol-field rejection. The two multipart doors use `System::ArgumentException`
because the *same parameters* already report their *other* defects that way
(`ArgumentException::ThrowIfNullOrWhiteSpace` on `subtype`, `name` and `fileName`;
`ArgumentException` on `boundary`), and splitting one parameter's diagnostics across two
exception families would be arbitrary. `parseStatusLine` uses `HttpRequestException` because
it parses a **server response**: a malformed response is this module's response error, not the
caller's format error.

**.NET's exact type and `paramName` for a CR/LF-bearing header value are not known here.**
`/rv/tmp/runtime/src/libraries/` is absent from this container (re-verified 2026-08-04). The
choices above are **this port's**, recorded as choices rather than claimed as matches —
`docs/SystemNetHttpNamespaceReviewPlan.md` §15.

## 5. What is still accepted, deliberately

- A **space** and a **horizontal tab** inside a header value — both are legal there.
- An **empty** header value.
- A `"`, a `;`, and every other C0 control character. Only the three characters that
  terminate a frame are rejected; widening the set further would narrow the accepted input
  beyond what the frame grammar requires and beyond the evidence available here.
- **Percent-encoded** CR/LF in a URL (`http://host/p%0d%0aX:%20y`). Escaping is precisely how
  a caller passes such bytes safely, and the escaped form is ordinary text.
- The content **body**. A body is payload, not a protocol field:
  `StringContent("line1\r\nline2\r\n")` is unchanged and always will be.

## 6. How to migrate

1. **If you were relying on the old behaviour to emit extra fields**, stop. Set the fields
   individually with `setHeader`.
2. **If your text can legitimately contain a newline**, it is not a header value. Put it in
   the body, or escape it (percent-encoding for a URL, a MIME-appropriate encoding for a
   parameter).
3. **If the text comes from user input or from a remote peer**, this exception is the point:
   catch `System::FormatException` (which also catches `System::UriFormatException`) around
   the door and reject the input, rather than forwarding it.

```cpp
try {
    request->setHeader("X-Trace", untrustedValue);
} catch (const System::FormatException&) {
    // untrustedValue tried to end the field. Reject it here.
}
```

## 7. What this change does **not** do

- It does **not** make the header maps case-insensitive (`SR-AUD-315`, blocked **#2068**).
- It does **not** de-duplicate the `Host`/`User-Agent`/`Accept`/`Connection` fields the
  handler writes unconditionally before the caller's map (also **#2068**).
- It does **not** constrain the status-code **domain** — `HttpResponseMessage(-1)` still
  constructs (`SR-AUD-316`'s code half, blocked **#2069**).
- It does **not** encode `StringContent` through its declared charset (`SR-AUD-317`, deferred
  **#2070**).
- It does **not** bound response reads (`SR-AUD-318`'s limits half, blocked **#2071**).
- It does **not** repair the `HttpClient` async-lambda ownership defect (`SR-AUD-310`,
  CCF-019, blocked **#2066**).

Each of those five behaviours is **pinned** by a test in `NetHttpGatedBehaviourPins`
(`modules/net-http/tests/System/Net/HttpClientTests.cpp`), so none of them can change without
a deliberate, visible edit to that suite.

## 8. Where the evidence lives

| Artefact | What it holds |
|---|---|
| `build-probe/2063_probe1_doors.cpp` | the complete pre/post behaviour matrix for every door |
| `build-probe/2063_probe1_before.log` | measured **before** — every door open |
| `build-probe/2063_probe2_after_2063.log` | measured **after** — every door closed, legal text unchanged |
| `build-probe/2063_probe3_asan.log` | ASan/UBSan/LSan clean over 130 rejections and 19 acceptances, with a control heap-buffer-overflow proving instrumentation |
| `HttpControlCharacterTests` (21 tests) | the permanent regressions |
| `NetHttpGatedBehaviourPins` (7 tests) | the disclosure pins for the blocked and deferred tickets |
