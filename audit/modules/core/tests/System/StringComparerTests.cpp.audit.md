# Audit: `modules/core/tests/System/StringComparerTests.cpp`

## Metadata

- Audit status: AUDITED (268 lines, 42 tests, fully read).
- Validation: `OrdinalComparerTests.*:CultureAwareComparerTests.*:StringComparerTests.*`
  passed 42/42 in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The suite exercises factory type selection, ignore-case flags, ordinary ASCII
ordering/equality, equal-value hash consistency, and `FromComparison` mapping
for all six valid enum values. It proves normal dispatch but not the public
failure, Unicode, culture, or comparer-law boundaries.

## Finding references

- **SR-AUD-018 (extended):**
  `GetHashCode_CaseSensitive_DifferentCaseDifferentHash` asserts that two
  unequal strings must have different hashes. This can pass only by accident
  for the current standard-library hash and rejects a valid collision. Keep
  the equal-values-must-share-a-hash test, but do not assert the inverse.

## Other missing assertions and diagnostics

- No test checks `FromComparison(static_cast<StringComparison>(invalid))`
  throws `ArgumentException`.
- No test compares all public factories' `Compare`, `Equals`, and hash result
  for the same input pairs, including the law that equality implies equal hash.
- The suite has no UTF-8/non-ASCII, embedded-NUL, empty, long-input, locale,
  or Unicode simple-case-folding vector. It therefore cannot make the
  repository's documented byte/ASCII and culture-fallback adaptation visible.
- Factory tests inspect implementation type with `dynamic_pointer_cast` rather
  than only public behavior, so they constrain an otherwise replaceable
  implementation choice.

## Final assessment

The ASCII happy-path coverage is broad for this compact header, but it misses
failure and adaptation boundaries and includes one invalid hash assertion.
No test was modified during this audit.
