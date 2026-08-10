# Audit: `modules/net-http/include/System/Net/Http/HttpVersionPolicy.hpp`

Audit status: AUDITED.

All three ordinal values are declared, but the request message has no version
or version-policy property and the HTTP/1.1 transport never consumes this enum.
This is an explicit API-surface limitation documented in the request header,
not an additional hidden implementation failure.

Missing coverage: a compile-time consumer and clear API-baseline decision if
version negotiation is later added.
