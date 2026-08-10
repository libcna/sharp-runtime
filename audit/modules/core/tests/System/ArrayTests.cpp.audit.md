# Audit: `modules/core/tests/System/ArrayTests.cpp`

## Metadata

- Audit status: AUDITED (492 lines, 80 tests, fully read).
- Validation: `ArrayTests.*` passed 80/80 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The suite is strong on ordinary vector behavior and on the range-validation
work that preceded this audit.  It covers all basic operation families with
`int`, several string happy paths, and the unusual empty-array backward-search
rules.  It does not exercise aliasing, raw-pointer safety, float comparison,
empty delegates, or exact `MaxLength`, leaving all Array findings green.

## Finding references

- **SR-AUD-044:** `Copy_VectorSubrange` uses different source/destination
  vectors.  It cannot reveal the forward-copy corruption when the same vector
  is copied rightward through nontrivial elements.
- **SR-AUD-046:** Sort and binary-search vectors contain integers only; there
  are no float NaN ordering/search assertions.
- **SR-AUD-051:** the only raw copy test uses valid, non-overlapping `int`
  arrays.  It omits negative inputs, overlapping pointers, and nontrivial
  values whose ownership cannot be byte-copied.
- **SR-AUD-052:** every callable is a valid lambda.  No test supplies an empty
  `std::function` to any overload, particularly an empty array where the
  current source silently returns a normal result instead of failing.
- **SR-AUD-053:** `MaxLength_IsPositive` checks only that the value is positive
  and cannot detect its 56-element divergence from current .NET.

## Required post-audit verification

Add left/right aliasing tests for both vector `Copy` overloads with
observable nontrivial assignment, then run under ASan/UBSan.  Isolate raw
pointer tests in a sanitizer target: reject negative signed metadata before
pointer formation, verify overlap semantics, and ensure any permitted type
has valid copy/lifetime semantics.

Add NaN `Sort`/`BinarySearch` vectors, empty `std::function` checks on both
empty and nonempty arrays for every callable family, and an exact
`Array::MaxLength` parity assertion or an explicit documented adaptation test.

## Final assessment

The 80 tests make the recently added bounds checks valuable regression
coverage, but normal-path `int` fixtures miss the memory-safety and
comparison/diagnostic contracts confirmed by this audit.  No test was modified
during this audit.
