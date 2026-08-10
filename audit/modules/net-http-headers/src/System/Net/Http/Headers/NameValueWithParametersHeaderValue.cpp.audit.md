# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/NameValueWithParametersHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

The parser locates and splits semicolons without consuming quoted-pairs.  It cannot represent otherwise-valid quoted parameter values containing escaped delimiters.

Required remediation: replace raw `find(';')` splitting with the common escaped-string scanner.
