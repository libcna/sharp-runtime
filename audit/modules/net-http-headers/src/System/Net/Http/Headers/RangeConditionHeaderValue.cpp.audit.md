# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/RangeConditionHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

If-Range date parsing accepts a valid prefix plus trailing bytes.  The direct probe confirms `TryParse(... GMT trailing)` succeeds; the reference parser consumes the full input.

Required remediation: use a shared strict HTTP-date parser.
