# Audit: `modules/net-http/include/System/Net/Http/HttpResponseMessage.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [HttpResponseMessage.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/HttpResponseMessage.cs).
- Evidence: `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp`.

## Assessment

The constructor and setter accept arbitrary enum casts, while the reference
rejects status values below zero or above 999.  Reason phrases also accept
embedded newlines.  The direct probe constructs both `-1` and `1000` and
retains each unchanged.

### SR-AUD-316 — medium — response messages accept out-of-range status values and unvalidated reason phrases

Invalid public status codes are preserved and can flow through
`EnsureSuccessStatusCode`; an embedded CR/LF reason is not rejected.  The
managed constructor/setter validate the [0,999] range and prohibit newline or
NUL reason phrases.

Required remediation: validate status-code range at all public writes, reject
invalid reason characters, and add matching argument/format diagnostics.

## Missing assertions and diagnostics

Tests cover only common enum values and reason display.  Add -1/1000,
embedded CR/LF/NUL, default reason behavior, and case-insensitive header lookup
coverage (SR-AUD-315).
