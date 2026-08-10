# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/CacheControlHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

Cache-Control uses another quote-toggle comma splitter.  Escaped quotes can cause a comma inside a quoted extension value to become a structural list separator.

Required remediation: centralize quote-aware list scanning and cover escaped quotes in extension values.
