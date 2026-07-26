# Audit: `modules/net-http-headers/include/System/Net/Http/Headers/HttpHeaders.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-322 — high — TryAddWithoutValidation accepts invalid header names

The public contract says the operation is always successful, which disagrees with current .NET: values may bypass parsing, but invalid field names return false.  The implementation admits CR/LF names that serialize as extra fields.

Required remediation: correct both behavior and API documentation; test nonempty invalid names separately from unvalidated values.
