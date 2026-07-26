# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/EntityTagHeaderValueTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

No test rejects CR/LF/NUL inside a quoted entity tag or verifies safe serialization (SR-AUD-319).
