# Audit: `modules/net-http/include/System/Net/Http/HttpProtocolException.hpp`

Audit status: AUDITED.

The exception preserves the integer code and exposes the HTTP-protocol error
category.  HTTP/2 and HTTP/3 are explicitly unsupported by this module, so no
runtime producer exists and no independent defect was confirmed.

Missing coverage: negative and large codes, inner exception retention, and
polymorphic catching through all declared base exception types.
