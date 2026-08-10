# Audit: `modules/core/tests/System/UInt64Tests.cpp`

## Metadata

- Audit status: AUDITED (84 lines, 17 tests, full read).
- Validation: Core.Base `UInt64Test.*` passed 17/17 on 2026-07-25.

## Assessment

The file checks full-range decimal parse, negative-sign rejection (including
`-0`), range overflow, malformed width translation, and division-by-zero.  It
is a useful complement to the broader integration suite, but all output and
interval tests use supported/ordered inputs.

## Finding references

- **SR-AUD-021:** it tests malformed width but not an unknown format type such
  as `"Q"`, allowing silent decimal fallback to remain unobserved.
- **SR-AUD-022:** no `Clamp(min > max)` regression exists for UInt64.
- **SR-AUD-023:** no `"B"`/`"b"` binary-format assertion exists, so the
  current decimal fallback passes the entire suite.

## Required post-audit verification

Add exact `System::FormatException` checks for `"Q"` and malformed/oversized
precision, an `ArgumentException` check for inverted Clamp bounds, and binary
format vectors for 5, zero, padded width, and `MaxValue`.

## Final assessment

Useful parser failure coverage but insufficient API-format/invalid-interval
coverage.  The gaps are confirmed in the mirrored implementation reports, and
this audit makes no test changes.
