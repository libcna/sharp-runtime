# Audit: `modules/net-http/include/System/Net/Http/HttpClient.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [HttpClient.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/HttpClient.cs).
- Evidence: `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp` and
  `/tmp/sharp-runtime-net-http-audit/http_client_async_lifetime.cpp`.

## Assessment

The public parsed-URL/status helpers expose permissive parser behavior
(SR-AUD-311/312).  The async methods return a task after capturing `this` and
the class has no ownership mechanism that keeps the client alive
(SR-AUD-310).  The documented HTTPS/redirect omissions are intentional scope,
not classified as fresh findings.

## Missing assertions and diagnostics

Tests omit an outstanding task outliving its client, malformed port suffixes,
port range checks, bracket suffixes, authority-only queries, invalid HTTP
versions, and non-three-digit status text.
