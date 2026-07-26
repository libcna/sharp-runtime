# Audit: `modules/net-http/include/System/Net/Http/HttpRequestError.hpp`

Audit status: AUDITED.

The ordinal enum mirrors the implemented subset's category names.  Most
terminal socket/parser failures nevertheless construct the default `Unknown`
`HttpRequestException`; the lack of framing diagnostics and resource limits is
tracked in SR-AUD-318 rather than attributed to this value declaration.

Missing coverage: category assignment for DNS, connect, malformed status,
malformed headers, premature EOF, and configuration-limit failures.
