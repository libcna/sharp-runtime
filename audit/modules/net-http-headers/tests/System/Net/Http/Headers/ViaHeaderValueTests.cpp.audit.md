# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/ViaHeaderValueTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

The test suite omits embedded-NUL received-by rejection (SR-AUD-319).
