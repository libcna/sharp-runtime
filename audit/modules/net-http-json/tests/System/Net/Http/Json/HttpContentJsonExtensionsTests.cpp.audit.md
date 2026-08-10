# Audit: `modules/net-http-json/tests/System/Net/Http/Json/HttpContentJsonExtensionsTests.cpp`

## Metadata

- AUDITED: synchronous and Task-backed valid JsonContent parsing tests.
- Validation: 2/2 content-only tests passed on 2026-07-27.

## Assessment

The fixture checks valid parsing only.  It omits the null shared_ptr boundary
that ASan confirms as SR-AUD-236.

## Other missing assertions and diagnostics

- Add synchronous/asynchronous null input diagnostics, invalid/empty JSON,
  content exceptions, task fault timing, and JsonDocument lifetime tests.

## Final assessment

The green tests miss SR-AUD-236.  No source or test was changed.
