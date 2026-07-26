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
