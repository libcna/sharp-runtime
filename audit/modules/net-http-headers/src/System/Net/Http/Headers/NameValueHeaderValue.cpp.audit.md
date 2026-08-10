# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/NameValueHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

`isValidQuotedString` accepts CR, LF and NUL in quoted strings.  The direct probe constructs `p="safe\\r\\nX-Injected: value"` and serializes the extra field verbatim.  Current .NET's `HttpRuleParser.GetQuotedStringLength` rejects all three characters.

Required remediation: share a byte-safe quoted-string validator with the reference CR/LF/NUL rules and add constructor, setter and TryParse regressions.
