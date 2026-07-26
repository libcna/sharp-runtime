# Audit: `modules/core/tests/System/FormattableStringTests.cpp`

## Metadata

- Audit status: AUDITED (65 lines, 11 tests, fully read).
- Validation: `FormattableStringTests2.*` passed 11/11 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The focused tests cover storage, simple argument retrieval/substitution,
zero/multiple arguments, the culture-no-op adaptation, and positive
out-of-range argument access.  They are only normal-path examples of a
composite formatter, and therefore preserve the faulty replacement strategy.

## Finding references

- **SR-AUD-015 (extended):** no test covers escaped/malformed braces, missing
  indices, format/alignment components, repeated nonsequential placeholders,
  or an argument containing `{n}`.  The audit probe demonstrates that these
  gaps permit reinterpreting argument data and silently retaining missing
  placeholders rather than following composite-format grammar.

## Other missing assertions and diagnostics

- No negative index is supplied to `GetArgument`, no returned vector copy is
  mutated to demonstrate isolation, and no non-null provider is passed.
- The tests duplicate much of the integration smoke coverage without adding
  grammar/error vectors.

## Final assessment

The direct suite demonstrates basic storage and replacement but lacks the
format grammar that defines this API.  No test was modified during this audit.
