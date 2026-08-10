# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/RetryConditionHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

Retry-After's date branch accepts `... GMT trailing`.  This is a parser acceptance divergence with potentially misleading retry behavior.

Required remediation: use the same complete-consumption date parser as the other header types.
