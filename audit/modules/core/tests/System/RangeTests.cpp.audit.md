# Audit: `modules/core/tests/System/RangeTests.cpp`

## Metadata

- Audit status: AUDITED (107 lines, 16 tests, fully read).
- Validation: `RangeTest.*` passed 16/16 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

This dedicated suite covers default/all, explicit, start-at, end-at, and
ordinary from-end ranges; properties, equality/hash consistency, text, and
three normal invalid-range conditions are also checked.  It is a stronger
functional suite than the four duplicate smoke cases in the larger pending
`SystemTypesRemainingTests.cpp` source.

## Finding references

- **SR-AUD-057:** no case combines an extreme `Index::FromEnd` with a negative
  public length.  That omission lets both `Index::GetOffset` and the final
  `end - start` calculation reach UBSan-confirmed signed overflow while all
  16 tests remain green.

## Other missing assertions and diagnostics

- There is no zero-length boundary matrix, `^0` with an invalid length,
  `INT_MIN`/`INT_MAX`, or explicit assertion of the C++ policy needed to match
  .NET's unchecked arithmetic.
- Tests do not exercise consumers such as `SpanSplitEnumerator` or a
  collection slice, so their translation of an `OffsetAndLength` result is not
  covered here.

## Final assessment

The suite covers normal range semantics and ordinary invalid order/bounds, but
not the sanitizer-confirmed arithmetic boundary.  No test was modified during
this audit.
