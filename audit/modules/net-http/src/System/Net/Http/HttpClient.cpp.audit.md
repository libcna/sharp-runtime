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

### SR-AUD-312 — medium — status-line parser turns malformed response text into arbitrary or successful status codes

`parseStatusLine("HTTP/1.1 200trailer OK")` returns 200; `NOTHTTP 999
arbitrary`, 99, and 1000 are accepted too.  The parser only finds spaces and
uses prefix-accepting `std::stoi`, instead of validating an HTTP version and
exactly three status digits.

Required remediation: require supported `HTTP/1.0`/`HTTP/1.1` framing,
exactly three ASCII digits in the valid range, and a complete deterministic
error category.

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
