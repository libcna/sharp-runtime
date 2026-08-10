# Audit: `modules/core/tests/System/IndexTests.cpp`

## Metadata

- Audit status: AUDITED (77 lines, 13 tests, fully read).
- Validation: `IndexTests2.*` passed 13/13 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The direct tests exercise construction, start/end forms, positive end offsets,
negative constructor rejection, documented normal out-of-range offset behavior,
implicit conversion, equality, hash consistency, and text formatting.  They
are a useful public API smoke suite but only use small ordinary integer values.

## Finding references

- **SR-AUD-057:** `GetOffset_OutOfRange_DoesNotThrow_MatchesDotNet` correctly
  preserves the no-validation policy, but no test covers a negative `length`
  combined with a maximal end-based Index.  The UBSan repro therefore exposes
  signed overflow while all 13 tests pass.

## Other missing assertions and diagnostics

- No test covers `INT_MIN`/`INT_MAX`, length zero for a nonzero end index, or
  the defined C++ result intended to correspond to .NET unchecked arithmetic.
- Hash tests establish only equal-input consistency, appropriately avoiding an
  invalid no-collision expectation.

## Final assessment

The normal API surface is covered, but boundary arithmetic needs a sanitizer
regression before a repair can be accepted.  No test was modified during this
audit.
