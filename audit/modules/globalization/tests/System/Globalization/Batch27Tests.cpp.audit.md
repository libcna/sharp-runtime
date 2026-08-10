# Audit: `modules/globalization/tests/System/Globalization/Batch27Tests.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

This batch tests only ASCII CompareInfo behavior and sequential global
CurrentCulture changes.  It explicitly expects unknown culture names to fall
back rather than fail, and has no thread isolation coverage.

## Finding references

- SR-AUD-280 — high — no concurrency test reaches racy global current culture.
- SR-AUD-283 — medium — ASCII-only comparison coverage misses Unicode/options.
- SR-AUD-285 — medium — unknown-culture fallback is asserted as success.

## Other missing assertions and diagnostics

- Add TSan two-thread tests, invalid culture names, Unicode casing/diacritics,
  ignored options, and invalid ordinal combinations.

## Final assessment

The test contract fails to protect SR-AUD-280, SR-AUD-283, and SR-AUD-285.
