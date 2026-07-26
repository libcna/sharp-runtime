# Audit: `modules/net-http/README.md`

Audit status: AUDITED.

The overview accurately states that this is a synchronous HTTP/1.1-over-plain-
TCP adaptation and identifies unsupported TLS, redirects, and full header
objects.  It does not disclose the confirmed malformed-response, serializer
validation, and async-lifetime defects recorded in SR-AUD-310 through
SR-AUD-318.

Missing coverage: document the bounded-response and handler-lifetime contract
once those defects are remediated.
