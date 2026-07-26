# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/HttpResponseHeaders.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

Response list parsing repeats the escaped-quote-insensitive comma splitter, corrupting valid quoted list values.

Required remediation: share the corrected list scanner with request headers.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

The response Date parser accepts `GMT trailing` instead of rejecting the value as the reference `HttpDateParser` does.

Required remediation: use a centralized, full-consumption HTTP-date parser.
