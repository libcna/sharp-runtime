# Audit: `modules/net-http/include/System/Net/Http/HttpRequestMessage.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [HttpRequestMessage.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/HttpRequestMessage.cs).
- Evidence: source review and `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp`.

## Assessment

Raw string headers are stored in a case-sensitive `unordered_map`; HTTP field
names are case-insensitive.  A request `accept` does not suppress a client
default `Accept`, producing duplicate semantic fields.  Names/values also pass
directly to the terminal serializer without CR/LF validation (SR-AUD-313).

### SR-AUD-315 — medium — request/response header maps treat HTTP field names as case-sensitive

`setHeader("content-type", "text/plain")` followed by
`getHeader("Content-Type")` returns empty in the direct probe.  The same raw
key comparison makes default-header merging duplicate differently cased names,
where managed `HttpHeaders` uses case-insensitive field-name lookup.

Required remediation: use a validated case-insensitive field-name container
for request/default/response headers, preserving intended multi-value behavior.

## Missing assertions and diagnostics

Tests cover one exact-case round trip only; add mixed-case lookup, default
override, duplicate-field, invalid-name, and CR/LF value cases.
