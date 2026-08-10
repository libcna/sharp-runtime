# Audit: `modules/net-http/include/System/Net/Http/HttpRequestOptionsKey.hpp`

Audit status: AUDITED.

The immutable key-name wrapper supports the typed `HttpRequestOptions` routes.
Its type parameter is compile-time-only, as in the managed generic key.  No
independent defect was confirmed.

Missing coverage: empty names, copy/move stability, equal text with distinct
`TValue`, and temporary-key use.
