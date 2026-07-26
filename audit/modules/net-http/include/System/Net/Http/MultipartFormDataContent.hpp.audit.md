# Audit: `modules/net-http/include/System/Net/Http/MultipartFormDataContent.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [MultipartFormDataContent.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/MultipartFormDataContent.cs).
- Evidence: `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp`.

## Assessment

`name` and `fileName` reject only whitespace; quotes, backslashes, CR, and LF
are copied into a pre-formatted `Content-Disposition` line.  The direct probe
shows injected separate `X-Injected` and `Y-Injected` MIME fields.  The
reference assigns through a validating header value object.

See SR-AUD-313 for required common header-value remediation.

## Missing assertions and diagnostics

Add quote/backslash escaping, CR/LF/NUL rejection, non-ASCII filename encoding,
empty-name/file diagnostics, and exact byte output for all three `Add` forms.
