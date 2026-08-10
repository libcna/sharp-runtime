# Audit: `modules/net-http-headers/include/System/Net/Http/Headers/HttpContentHeaders.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

Expires and Last-Modified accept malformed trailing data.
