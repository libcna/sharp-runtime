# Audit: `modules/net-http-json/tests/System/Net/Http/Json/HttpClientJsonExtensionsTests.cpp`

## Metadata

- AUDITED: local-loopback GET and POST JSON integration tests.
- Validation: both cases are environment-blocked in this sandbox because
  TcpListener creation fails with `Operation not permitted` before assertions.

## Assessment

The mini server offers useful end-to-end coverage where local sockets are
permitted.  Its two tests do not cover PUT/PATCH/DELETE, failure statuses,
malformed JSON, cancellation, or client/task lifetime.

## Other missing assertions and diagnostics

- Re-run in a local-network-permitted environment; add all verb/error paths,
  content headers/charset, concurrent requests, and ASan in-flight client
  destruction coverage.

## Final assessment

No source conclusion is drawn from the sandbox network limitation.  No source
or test was changed.
