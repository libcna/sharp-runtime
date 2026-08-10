# Audit: `modules/buffers/tests/System/Buffers/EnumeratorTests.cpp`

## Metadata

- Audit status: AUDITED (37 lines, four tests, fully read).
- Validation: `ReadOnlySequenceEnumeratorTest.*` passed 4/4 in the combined
  direct `SharpRuntimeTests_Buffers` filter on 2026-07-26.  The filter's three
  direct fixtures passed 54/54.
- Related implementation: `ReadOnlySequence.hpp`; default-versus-empty
  enumeration is confirmed as SR-AUD-074.

## Assessment

The normal contiguous sequence case is covered sufficiently to show one
segment followed by completion.  The fixture's apparent empty-sequence test
does not assert the result of `MoveNext`, however, and therefore passes even
though the current implementation returns true for a default sequence.

## Finding references

- **SR-AUD-074 (extended):** `EmptySequenceNoMoveNext` calls `e.MoveNext()`
  but discards its return value and asserts only that `Current` is empty.  The
  current implementation's incorrect `true` transition for a default sequence
  remains invisible, so this test cannot serve as evidence for its name or
  intended .NET parity.

## Other missing assertions and diagnostics

- Add an explicit `EXPECT_FALSE(e.MoveNext())` for default state and a separate
  explicit-empty sequence assertion; .NET distinguishes their enumeration
  behavior, while the current C++ adaptation collapses them.
- `getCurrentProperty()` is never checked before the first transition or after
  completion.  The fixture does not define whether such access is empty,
  invalid, or diagnostic.
- No test verifies repeated `MoveNext` after completion, enumerator copy/move
  behavior, source lifetime, or a sequence that contains an empty segment.
- `GetEnumeratorNotNull` is a misleading name for a value-returning C++ API:
  it checks only a successful first transition and provides no construction or
  lifetime diagnostic.
- The public type currently has no constructed multi-segment sequence route;
  this fixture consequently cannot test segment ordering/provenance or the
  advertised linked-segment behavior recorded by SR-AUD-087.

## Final assessment

Four tests pass, but one named behavioral contract has no assertion at all and
therefore masks the already confirmed default-enumerator parity defect.  No
source or test was modified during this audit.
