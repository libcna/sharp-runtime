# Audit: `modules/text/tests/System/Text/UTF7EncodingTests.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: included in the 233/233 focused Text run on 2026-07-27.

## Assessment

RFC examples, optional characters, astral text, and several malformed shift
cases are covered. The tests do not verify inherited replacement/exception
fallback configuration or raw pointer capacity at nonzero offsets.

## Finding references

- SR-AUD-292 — medium — UTF7 ignores inherited configured fallback policy.

## Other missing assertions and diagnostics

- Test replacement versus exception fallback, empty/malformed shifted forms,
  negative/large offsets, and precise diagnostic indexes.

## Final assessment

The suite leaves SR-AUD-292 unprotected.
