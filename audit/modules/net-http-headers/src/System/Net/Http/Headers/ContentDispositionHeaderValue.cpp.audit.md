# Audit: `modules/net-http-headers/src/System/Net/Http/Headers/ContentDispositionHeaderValue.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Net.Http.Headers`.
- Evidence: focused target passed 373/373; direct behavior probe at `/tmp/sharp-runtime-net-http-headers-audit/header_value_probe.cpp`.

### SR-AUD-319 — high — typed header values accept forbidden control characters

Name/FileName setters quote caller text then delegate to `NameValueHeaderValue`; the permissive quoted-value path therefore permits CR/LF/NUL serialization.

Required remediation: depend on the corrected quoted-string validator and test all convenience setters.

### SR-AUD-320 — medium — escaped quoted delimiters are split as structural delimiters

`splitTopLevel` toggles state at every quote and ignores backslash escapes.  A valid `filename="a\\\";b"` is rejected by `TryParse`, whereas .NET's length-driven grammar accepts it.

Required remediation: replace quote toggling with a shared scanner that consumes quoted-pairs.

### SR-AUD-321 — medium — RFC 1123 parser accepts trailing garbage

The local `sscanf` parser does not verify complete consumption.  Header values ending `GMT trailing` parse successfully, while current .NET's `HttpDateParser` requires a full value.

Required remediation: centralize full-consumption HTTP-date parsing and add malformed-tail/weekday tests.

### SR-AUD-323 — medium — RFC 5987 decoder ignores the declared charset

`tryDecode5987` percent-decodes bytes but neither validates nor converts the charset.  `iso-8859-1''foo-%E4.html` is reported as a 10-byte raw string rather than UTF-8 `foo-ä.html` (11 bytes); unsupported labels are also accepted.

Required remediation: implement the reference UTF-8 and ISO-8859-1 decoding contract, reject unsupported/malformed encoded values, and test byte output.
