# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/ViaHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

`isValidReceivedBy` rejects several delimiters but omits NUL.  A direct probe accepts an embedded-NUL received-by value and `ToString()` preserves its 18-byte serialized form.

Required remediation: use a shared control-character predicate before token/host validation.
