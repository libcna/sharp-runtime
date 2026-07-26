# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/HttpHeadersTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

Add TryAddWithoutValidation invalid-name tests for spaces, CR, LF and NUL, with false return and no serialized field (SR-AUD-322).
