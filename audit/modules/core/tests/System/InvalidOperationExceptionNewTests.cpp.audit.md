# Audit: `modules/core/tests/System/InvalidOperationExceptionNewTests.cpp`

## Metadata

- AUDITED: 55-line dual-fixture source, fully read.
- Validation: `InvalidOperationExceptionNewTests.*` and
  `NullReferenceExceptionNewTests.*` passed 8/8 within the selected 58-test
  member/type-access exception filter on 2026-07-27.

## Assessment

InvalidOperation cases verify default/custom/inner text and
`COR_E_INVALIDOPERATION` (`0x80131509`) across its three C++ constructors;
they correct the older header report's stale claim that only a shared smoke
fixture existed. The colocated NullReference supplements its separately
audited dedicated fixture with default text, outer-inner text, and
SystemException catchability. No implementation defect was reproduced.

## Missing assertions and diagnostics

- InvalidOperation does not assert exact default text, cause identity/rethrow,
  UTF-8 input, copy/move, or a state-transition consumer route.
- Its `const char*` test uses only ordinary non-null ASCII; no null-policy
  boundary is documented or exercised.
- The supplemental NullReference cases do not read HResult, but its dedicated
  fixture already checks every constructor's `E_POINTER` value.

## Final assessment

The source closes the normal InvalidOperation HResult gap from the earlier
test inventory and adds useful NullReference smoke coverage. No source or test
was modified.
