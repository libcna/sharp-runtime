# Audit: `modules/net-http/include/System/Net/Http/HttpClientHandler.hpp`

Audit status: AUDITED.

The capability flags honestly report the reduced implementation and mutable
cookie container validates null input.  The public terminal handler exposes no
response-size limits or timeout configuration, matching the unbounded parser
and error-path socket-leak finding SR-AUD-318.

Missing coverage: bounded headers/body/chunks, cancelled or disposed in-flight
operations, and malformed response framing.
