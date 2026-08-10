# Audit: `modules/net-http/src/System/Net/Http/MultipartFormDataContent.cpp`

Audit status: AUDITED.

The overloads reject null content and whitespace-only names but build quoted
`Content-Disposition` parameters through direct concatenation.  Quotes and
newlines are neither escaped nor rejected, producing the reproduced
SR-AUD-313 field-breakout behavior.  The local reference delegates this work to
a validating `ContentDispositionHeaderValue`.

Missing coverage: quote/backslash, CR/LF/NUL, Unicode filename, and exact
`filename*` adaptation expectations.
