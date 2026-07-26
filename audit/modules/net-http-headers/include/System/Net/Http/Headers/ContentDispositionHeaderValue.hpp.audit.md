# Audit: `modules/net-http-headers/include/System/Net/Http/Headers/ContentDispositionHeaderValue.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

Convenience Name/FileName setters reach the permissive quoted-value implementation and can serialize CR/LF/NUL.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

The advertised parser rejects valid escaped-quote parameter values.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

The date-property contract relies on a parser that accepts trailing text.

### SR-AUD-323 — medium — RFC 5987 decoder ignores the declared charset

The documented RFC 5987-decoded FileNameStar contract is not met for ISO-8859-1 or unsupported charsets.
