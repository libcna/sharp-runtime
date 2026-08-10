# Audit: `modules/net-http/tests/System/Net/HttpClientTests.cpp`

## Metadata

- Audit status: AUDITED.
- Focused command: `build/SharpRuntimeTests_Net_Http --gtest_color=no`.
- Result: 126 passed; 6 loopback-server cases fail before their assertions in
  this sandbox with `Socket::Socket: socket() failed`.

## Assessment

The no-network content, URL, status, options, multipart, handler, and cookie
tests exercise the ordinary happy paths.  The environment-limited loopback
tests stay enabled and are not interpreted as source failures.

The current assertions miss all confirmed Net.Http defects: async client
lifetime (SR-AUD-310), malformed URL and status grammar (SR-AUD-311/312), raw
HTTP/MIME field validation (SR-AUD-313), case-insensitive header semantics
(SR-AUD-315), response status/reason validation (SR-AUD-316), charset byte
agreement (SR-AUD-317), and bounded/strict terminal framing (SR-AUD-318).

## Required regression additions

- ASan test: retain `GetAsync` task while destroying its `HttpClient`, using a
  deterministic no-network handler.
- Direct parser tests for decimal suffixes/ranges, IPv6 suffixes, query-only
  targets, non-HTTP versions, and exact three-digit codes.
- Serialization tests rejecting CR/LF/NUL and escaping quoted multipart
  disposition parameters.
- Case-insensitive request/default/response header behavior and duplicate
  suppression.
- StringContent Unicode bytes for each accepted charset policy.
- Network-permitted loopback tests for malformed framing, bounds, descriptor
  closure, and lower-case HEAD response semantics.
