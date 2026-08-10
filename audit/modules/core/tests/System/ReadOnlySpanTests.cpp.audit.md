# Audit: `modules/core/tests/System/ReadOnlySpanTests.cpp`

## Metadata

- Audit status: AUDITED (240 lines, 25 tests, fully read).
- Validation: `ReadOnlySpanTests.*:SpanTests.*` passed 25/25 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The suite has useful ordinary construction, index, slice, range, copy, string,
identity-equality, and prior slice-overflow coverage.  It never constructs a
malformed span or overlapping range, so all 25 happy/ordinary boundary tests
pass while both current public failures remain invisible.

## Finding references

- **SR-AUD-043:** no mutable or read-only pointer/length constructor test
  passes a negative length or an oversized vector size, and no consumer test
  observes the resulting invalid metadata before it reaches an unsigned count.
- **SR-AUD-044:** every CopyTo/TryCopyTo test uses disjoint `int` buffers.  No
  test checks the documented overlapping-copy behavior or a nontrivial type;
  an independent `std::string` probe produces `a,a,a,a` instead of `a,a,b,c`.

## Required post-audit verification

Add negative-length exception tests for both span types and boundary tests for
empty/default pointer behavior.  Add left/right overlap tests for `CopyTo` and
`TryCopyTo` with `int` and `std::string`, plus a short destination assertion
that verifies no destination element was changed.  Run the negative-length
consumer path under ASan.

## Other missing assertions and diagnostics

- `Span<T>` has only one direct test (index exception); its slicing, copying,
  filling, clearing, conversion, and equality behavior is not independently
  asserted.
- Neither test suite distinguishes documented managed validation from native
  pointer lifetime/non-null preconditions.
- The range-for test does not cover default empty spans, where begin/end must
  remain safe without meaningful storage.

## Final assessment

The tests protect the earlier slice arithmetic repair and common valid behavior,
but lack the malformed-state and overlap cases necessary to detect SR-AUD-043
and SR-AUD-044.  No test was modified during this audit.
