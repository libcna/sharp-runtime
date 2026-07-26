# Audit: `modules/net-http-headers/tests/System/Net/Http/Headers/NameValueHeaderValueTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

## Missing assertions and diagnostics

The fixture lacks CR/LF/NUL quoted-value cases for constructor, setter and TryParse.  Add rejection assertions and a check that no serialized output contains a field delimiter (SR-AUD-319).
