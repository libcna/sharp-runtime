# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/WarningHeaderValueTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

Add quoted-control and embedded-NUL-agent rejection cases (SR-AUD-319), plus malformed RFC 1123 suffix cases (SR-AUD-321).
