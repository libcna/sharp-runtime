# Audit: `modules/buffers/tests/System/Buffers/ReadOnlyMemoryTests.cpp`

## Metadata

- Audit status: AUDITED (190 lines, 23 tests, fully read).
- Validation: `ReadOnlyMemoryTest.*` passed 23/23 in `SharpRuntimeTests_Buffers`
  on 2026-07-25.

## Assessment

The suite covers ordinary construction, bounds checks, both slice forms,
capacity failure, equality, pinning, and ArraySegment conversion.  It includes
the repaired two-argument addition-overflow vector but misses the one-argument
`INT_MIN` arithmetic path, raw negative-length state, and overlap semantics.

## Finding references

- **SR-AUD-043 (extended):** `PtrLengthCtor` uses only a valid positive length;
  no negative pointer-length or oversized-vector path is tested.
- **SR-AUD-044 (extended):** CopyTo/TryCopyTo use distinct integer vectors and
  cannot reveal overlap corruption delegated to `ReadOnlySpan`.
- **SR-AUD-049:** `SliceStart` covers only a normal value.  `SliceOutOfRange`
  covers the two-argument form, leaving `Slice(INT_MIN)` untested even though
  UBSan confirms overflow before the intended exception.
- **SR-AUD-018 (extended):** `GetHashCode_DifferentRegion_LikelyDifferent`
  asserts `EXPECT_NE` for two unequal regions.  Hash collisions are valid and
  this creates a non-portable test restriction.

## Required post-audit verification

Add direct one-argument slice tests for `-1`, `INT_MIN`, `Length + 1`, and both
valid endpoints under UBSan.  Add a bounded nontrivial overlap copy test and
raw constructor validation tests.  Replace the hash-uniqueness assertion with
same-region consistency/equality checks.

## Other missing assertions and diagnostics

- The short-destination TryCopyTo test does not verify that destination data is
  unchanged.
- No test observes a default/empty span, `ToArray`, `Pin`, or `Slice(0)` path.
- The suite has no vector-reallocation/lifetime diagnostic despite views and
  pinned handles being non-owning.

## Final assessment

Normal behavior is well represented, but the decisive extreme-input and
overlap cases are absent and a hash collision is incorrectly forbidden.  No
test was modified during this audit.
