# Audit: `modules/buffers/tests/System/Buffers/MemoryTests.cpp`

## Metadata

- Audit status: AUDITED (392 lines, 49 tests, fully read).
- Validation: `MemoryTests.*` passed 49/49 in `SharpRuntimeTests_Buffers` on
  2026-07-25.

## Assessment

The suite provides useful normal and negative coverage for vector-backed
subranges, slices, capacity failures, conversion, and pinning.  It does not
exercise overlap-safe copying, unrepresentable lengths, or the lifetime
preconditions of non-owning views, so it passes despite the extensions to
SR-AUD-043 and SR-AUD-044.

## Finding references

- **SR-AUD-043 (extended):** no test can represent a vector larger than
  `intcs::max()` and no assertion defines the required rejection behavior.
- **SR-AUD-044 (extended):** CopyTo/TryCopyTo use only separate integer vectors;
  they omit left/right overlapping memory slices and an observable nontrivial
  copy type.
- **SR-AUD-018 (extended):** `GetHashCode_NonNegative` requires a hash to be
  nonnegative.  .NET hash codes are signed `int` values with no nonnegative
  guarantee; the assertion unnecessarily rejects valid implementations just as
  the previously reported uniqueness/nonzero assertions do.

## Required post-audit verification

Add overlap direction tests with `std::string` or a copy-observable type and
run them under ASan/UBSan.  Add a guardable large-size/narrowing boundary test
or document why such a vector is rejected before view creation.  Replace the
nonnegative-hash assertion with equal-region hash consistency and keep
distribution properties outside contract tests.

## Other missing assertions and diagnostics

- `TryCopyTo_DestTooShort_ReturnsFalse` does not assert that the one destination
  element remains unchanged.
- There is no zero-length slice at every valid endpoint, no default-memory
  `Slice(0)`, and no subrange conversion to `ReadOnlyMemory` value test.
- Pin tests do not identify vector reallocation invalidating the returned
  pointer; the public non-owning precondition is not exercised or diagnosed.

## Final assessment

The suite gives solid ordinary coverage but omits the generic-copy and boundary
cases that expose the confirmed memory-view findings.  No test was modified
during this audit.
