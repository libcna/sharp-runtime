# Audit: `modules/core/tests/System/MemoryExtensionsTests.cpp`

## Metadata

- Audit status: AUDITED (703 lines, 92 tests, fully read).
- Validation: `MemoryExtensionsTests.*` passed 92/92 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

The suite has broad happy-path coverage for vector/string slicing, search,
equality, mutation, trim, and generic trim overloads.  It correctly protects
the recently repaired `AsSpan` range checks and deliberately tests NaN for
equality APIs.  It leaves the copy capacity, overlap, ordering, and Unicode
whitespace contracts unobserved, so all 92 tests pass despite SR-AUD-046
through SR-AUD-048.

## Finding references

- **SR-AUD-044:** `CopyTo_CopiesElements` uses a same-sized non-overlapping
  `int` destination, so it cannot reveal forward-copy corruption of nontrivial
  overlapping values.
- **SR-AUD-046:** NaN assertions cover Contains/IndexOf/Count/Replace and one
  single-element prefix, but omit `SequenceCompareTo`, `BinarySearch`, and
  default `Sort`, where equality is not the ordering contract.
- **SR-AUD-047:** no test supplies a shorter destination or asserts the
  required `ArgumentException`; the independent ASan probe writes past a
  one-element vector.
- **SR-AUD-048:** whitespace tests exercise only ASCII space.  They omit
  U+00A0 and other documented .NET whitespace characters.

## Required post-audit verification

Add capacity and overlap tests for both static `CopyTo` overloads; use a
bounded vector or guarded allocation so the short-destination test is run
under ASan/UBSan.  Assert a deterministic exception before any write and
test both overlap directions with a nontrivial copy type.

Add float NaN order tests: `SequenceCompareTo({NaN}, {1}) < 0`, default sort
places NaN before finite values, and binary search locates NaN in a span sorted
under the same comparison contract.  Add U+00A0 Unicode-whitespace trim tests
once the project's string-code-unit policy is decided.

## Other missing assertions and diagnostics

- There are no empty-span checks for every search/last-search/trim overload,
  including all-empty trim input.
- Only the `ReadOnlySpan` primary forms are broadly covered; the shallow
  `Span<T>` forwarding overloads lack direct tests.
- `Overlaps_Disjoint_False` does not cover adjacent, identical, contained,
  reversed, or differently allocated address ranges under a portability-aware
  implementation.
- Tests do not exercise a vector/string larger than `intcs::max()` or document
  the intended behavior of that narrowing boundary.

## Final assessment

The test file is useful broad normal-path coverage, but it omits the exact
preconditions and special values that expose the three confirmed
MemoryExtensions findings.  No test was modified during this audit.
