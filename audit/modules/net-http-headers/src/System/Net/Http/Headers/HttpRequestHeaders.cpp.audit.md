# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/HttpRequestHeaders.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

List-valued request headers use a comma splitter that ignores quoted-pair escaping, so valid quoted authentication/parameter values can be split into different logical entries.

Required remediation: use the shared escaped-string scanner for every list getter and preservation path.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

The repeated local HTTP-date parser accepts a valid prefix followed by arbitrary text.  The direct probe shows `Date: ... GMT trailing` yields a value.

Required remediation: replace all copies with one complete-consumption parser and assert invalid tails remain unavailable.
