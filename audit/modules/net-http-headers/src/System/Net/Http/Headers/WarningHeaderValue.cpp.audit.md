# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/WarningHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

The quoted text helper accepts CR/LF/NUL, and the fallback host-like agent test also admits NUL.  These values can be emitted by `ToString()`, unlike the .NET constructors and parsers.

Required remediation: reject controls in both quoted and host-like branches; add byte-exact negative tests.
