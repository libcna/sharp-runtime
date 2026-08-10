# Audit: `modules/core/tests/System/DBNullTests.cpp`

## Metadata

- Audit status: AUDITED (55 lines, 9 tests, fully read).
- Validation: `DBNullTests.*` passed 9/9 in `SharpRuntimeTests_Core_Base` and
  the complementary integration `DBNullTests.*` filter passed 11/11 on
  2026-07-26.

## Assessment

This focused source supplements the integration smoke tests by covering every
remaining `IConvertible` scalar/Decimal/DateTime conversion of `DBNull`.  Each
expects the precise local `InvalidCastException` category, and one test also
checks the standardized message.  The split is explicitly documented in the
source and the combined filters cover the full public conversion set.

## Other missing assertions and diagnostics

- Only `ToInt32` asserts the exception message; the other conversion paths are
  assumed to retain the same text without a parameterized/shared assertion.
- No conversion is exercised through an `IConvertible` base reference and no
  test verifies that an ignored non-null format provider leaves `ToString`
  unchanged.
- The source has no singleton identity or concurrent-access case; those belong
  to the complementary integration smoke tests and a future thread test.

## Final assessment

The focused negative conversion coverage is complete for its stated purpose.
No test defect was confirmed and no test was modified during this audit.
