# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/ContentDispositionHeaderValueTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

Add CR/LF/NUL convenience-setter rejection (SR-AUD-319), escaped quote/semicolon parsing (SR-AUD-320), malformed date suffixes (SR-AUD-321), and UTF-8/ISO-8859-1/unsupported RFC 5987 charset vectors (SR-AUD-323).
