# Audit: `modules/console/tests/System/ConsoleTests.cpp`

## Metadata

- AUDITED: basic output, formatting, error, color, window, and beep smoke
  tests.
- Validation: complete Console fixture passed 123/123.

## Assessment

The tests cover broad API reachability but frequently use `EXPECT_NO_THROW`
without capturing output or asserting managed validation/error behavior.

## Other missing assertions and diagnostics

- Add output/error capture, formatting culture/special values, EOF/redirected
  input, null C-string handling, and invalid color/cursor tests
  (SR-AUD-243/244).
- Test terminal bounds, reset/default color semantics, title escape handling,
  ANSI suppression, and concurrent static Console state access.

## Final assessment

All current tests pass but do not expose SR-AUD-243/244. No source or test was
changed during this audit.
