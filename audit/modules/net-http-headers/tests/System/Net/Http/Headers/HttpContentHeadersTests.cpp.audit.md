# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/HttpContentHeadersTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

Add malformed trailing-date coverage for Expires and Last-Modified (SR-AUD-321).
