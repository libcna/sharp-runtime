# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/TransferCodingHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

The parameter splitter misinterprets an escaped quote and then treats a semicolon inside the quoted string as a new parameter.  The direct `gzip; p="a\\\";b"` probe returns false.

Required remediation: share an escaped quoted-pair-aware splitter.
