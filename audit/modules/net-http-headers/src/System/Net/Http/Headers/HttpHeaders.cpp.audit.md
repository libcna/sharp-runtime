# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/HttpHeaders.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-322 — high — TryAddWithoutValidation accepts invalid header names

The public method only rejects an empty name; it accepts CR/LF-bearing names and `ToString()` emits them verbatim.  The direct probe reports success for `X-Bad\\r\\nInjected: yes`.  Current .NET uses `TryGetHeaderDescriptor`, returning false for invalid names even when value validation is intentionally bypassed.

Required remediation: validate names through the token/descriptor path while preserving the intended unvalidated-value behavior, and add rejection plus serialization-safety assertions.
