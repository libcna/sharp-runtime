# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/MediaTypeHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

The local top-level splitter toggles on escaped quotes.  Valid parameters such as `p="a\\\";b"` are rejected before `NameValueHeaderValue` can parse them.

Required remediation: use a shared escaped-quoted-string scanner and test semicolon/comma delimiter cases.
