# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/HttpContentHeaders.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

Expires and Last-Modified use a local `sscanf` helper that accepts a valid RFC 1123 prefix plus arbitrary trailing text.  The direct Expires probe returns a value for `... GMT trailing`.

Required remediation: centralize strict HTTP-date parsing and add malformed-tail tests.
