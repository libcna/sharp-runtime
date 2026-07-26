# Audit: `modules/net-security/tests/System/Net/Security/SecuritySupportTests.cpp`

## Metadata

- AUDITED: complete 13-test header-only metadata fixture.
- Validation: `build/SharpRuntimeTests_Net_Security` passed 13/13.

## Assessment

The tests establish representative enum values/flags, ALPN well-known values,
basic construction bounds, equality/hash equality, and UTF-8/hex rendering.
They provide useful smoke coverage but not generated or sanitizer-level
confidence for the broad value surface.

## Other missing assertions and diagnostics

- Add a UBSan regression for the valid 255-byte `SslApplicationProtocol`
  hash overflow (SR-AUD-240), plus expected bit-pattern parity after a defined
  unsigned/wrapping implementation is chosen.
- Add default protocol, 1/255-byte boundary, all invalid UTF-8 categories,
  string encoding failures, full TLS enum generation comparison, unknown
  flags, and enum underlying-type checks.

## Final assessment

All current tests pass but do not expose SR-AUD-240. No source or test was
changed during this audit.
