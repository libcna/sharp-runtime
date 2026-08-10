# Audit: `modules/core/tests/System/TupleNewTests.cpp`

## Metadata

- Audit status: AUDITED (387 lines, 61 direct tests, fully read).
- Validation: the combined two-file Tuple filter
  `TupleTests.*:TupleExtensionsTests.*:TupleCompareTests.*` passed 94/94 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The suite has broad normal arity-one-through-eight construction, equality,
text, factory, basic hash, and extension coverage. It correctly preserves the
recent eighth-item `Rest` factory regression. All inputs are copyable integral
or ASCII string values, however, so public generic, arithmetic, mutation, and
nested-rest boundaries remain untested.

## Finding references

- **SR-AUD-018 (extended):** `Tuple1_GetHashCode_Differs` and
  `Tuple2_GetHashCode_DiffValues_Differ` require distinct tuples to have
  different hashes. Unequal values may legally collide; retain only the
  equal-values-imply-equal-hash direction.
- **SR-AUD-046 (extended):** no NaN/signed-zero component exercises the raw
  comparison/equality divergence confirmed by the probe.
- **SR-AUD-062:** all hash inputs are small values, so none reaches the
  `tupleHashCombine` signed-addition overflow that UBSan confirms for a
  reachable Tuple2 input.
- **SR-AUD-063:** tests only read public components and do not assert that a
  Tuple rejects or prevents mutation after construction.

## Other missing assertions and diagnostics

- No invalid `TRest`, nested Tuple8 rest/hash behavior, or Tuple8 comparison
  whose first seven items are equal and rest has a generic edge-case value.
- No null-like/empty, Unicode, locale, move-only, non-streamable, unhashable,
  or non-orderable template boundary test.
- Existing Tuple8 factory coverage does not check that a constructed tuple has
  the immutable/reference-like ownership semantics of its .NET counterpart.

## Final assessment

The recent arity-eight regression coverage is valuable, but it does not cover
the generic, hash-overflow, or immutability defects. No test was modified during
this audit.
