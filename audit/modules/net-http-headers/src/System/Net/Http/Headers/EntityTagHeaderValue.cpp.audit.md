# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/EntityTagHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

The entity-tag-specific quoted-string helper duplicates the permissive control-character handling.  The direct probe constructs and serializes an ETag containing CR/LF, which the reference parser rejects.

Required remediation: route entity-tag validation through the corrected quoted-string grammar.
