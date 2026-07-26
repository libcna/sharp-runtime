# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/MediaTypeHeaderValueTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

The parser tests omit an escaped quote followed by a semicolon inside a quoted parameter (SR-AUD-320).
