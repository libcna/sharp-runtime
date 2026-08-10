# Audit: `modules/net-http/include/System/Net/Http/StringContent.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [StringContent.cs](/rv/tmp/runtime/src/libraries/System.Net.Http/src/System/Net/Http/StringContent.cs).
- Evidence: `/tmp/sharp-runtime-net-http-audit/net_http_probe.cpp`.

## Assessment

The constructor stores a caller-provided charset label but always copies the
input UTF-8 bytes unchanged.  The direct probe creates UTF-8 `é` labelled
`utf-16` and emits bytes `c3 a9`, not a UTF-16 payload.  The managed constructor
uses the supplied `Encoding` to form the bytes.

### SR-AUD-317 — medium — StringContent advertises arbitrary charsets while always emitting the original UTF-8 bytes

Consumers may decode an otherwise valid payload under the declared but false
charset.  The C++ API must either support a bounded set of real encoders or
reject/omit non-UTF-8 labels rather than serialize contradictory metadata.

## Missing assertions and diagnostics

Tests check ASCII only.  Add UTF-8 multibyte text under default charset,
non-UTF-8 requested charset behavior, invalid charset syntax, and header
injection rejection (SR-AUD-313).
