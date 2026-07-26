# Audit: `modules/net-http-headers/include/System/Net/Http/Headers/TransferCodingHeaderValue.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

The parameter parser rejects valid escaped delimiters.
