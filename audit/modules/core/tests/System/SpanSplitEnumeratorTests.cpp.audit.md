# Audit: `modules/core/tests/System/SpanSplitEnumeratorTests.cpp`

## Metadata

- Audit status: AUDITED (138 lines, 11 tests, fully read).
- Validation: `SpanSplitEnumeratorTests.*` passed 11/11 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The suite gives good normal segmentation coverage for single separators,
leading/trailing/adjacent values, no match, empty source, any-of, and one
nonempty sequence.  It lacks the empty exact-sequence state transition, so all
tests pass while that public configuration loops forever.

## Finding references

- **SR-AUD-045:** no test constructs `SpanSplitEnumerator<T>` with
  `std::vector<T>{}` and `treatAsAny=false`; the independent probe shows that
  three consecutive `MoveNext()` calls all return true with the same empty
  segment.

## Required post-audit verification

Add a bounded explicit `MoveNext` test and a range-for collection test for
empty exact sequences on empty and nonempty inputs.  Assert one complete source
segment then completion, matching the selected .NET adaptation.  Add a
documented empty-any-of test as well, so the two empty separator modes cannot
be conflated.

## Other missing assertions and diagnostics

- No test verifies `getCurrentProperty` ranges or compares them to
  `getCurrentSpan`.
- Sequence tests omit a separator at index zero, at the final valid index, and
  adjacent sequence separators.
- No test checks repeated `begin`, post-completion `GetEnumerator`, or malformed
  span input inherited from SR-AUD-043.

## Final assessment

Happy-path split coverage is useful but omits the liveness edge case that
produces SR-AUD-045.  No test was modified during this audit.
