# Audit: `modules/net-http/include/System/Net/Http/FormUrlEncodedContent.hpp`

Audit status: AUDITED.

The encoder preserves ordering, uses `+` for spaces, and percent-encodes UTF-8
bytes and non-unreserved characters.  Its documented vector-only adaptation
cannot express the nullable keys/values accepted by the managed enumerable,
but no behavioral defect within its stated C++ input domain was confirmed.

Missing coverage: empty key/value entries, NUL bytes, a large collection, and
comparison against the managed escaping corpus.
