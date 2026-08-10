# Audit: `modules/net-http/include/System/Net/Http/HttpMethod.hpp`

Audit status: AUDITED.

Token validation, ordinal case-insensitive equality, hash equality, known
singleton parsing, and embedded-NUL rejection agree with the local reference.
The transport compares the emitted method text to the literal `"HEAD"` rather
than `HttpMethod` equality; that downstream behavior belongs to SR-AUD-318's
response-framing review.

Missing coverage: non-ASCII bytes, every permitted token punctuation, hash
collisions, and a lower-case constructed `head` request.
