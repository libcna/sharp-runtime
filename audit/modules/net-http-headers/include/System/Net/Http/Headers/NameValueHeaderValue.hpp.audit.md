# Audit: `modules/net-http-headers/include/System/Net/Http/Headers/NameValueHeaderValue.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

The public constructor, setter and parser expose the permissive quoted-string path.  Missing assertions cover CR, LF and embedded NUL in quoted values.
