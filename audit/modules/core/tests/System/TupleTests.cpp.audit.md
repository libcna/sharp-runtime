# Audit: `modules/core/tests/System/TupleTests.cpp`

## Metadata

- Audit status: AUDITED (254 lines, 33 direct tests, fully read).
- Validation: the combined two-file Tuple filter
  `TupleTests.*:TupleExtensionsTests.*:TupleCompareTests.*` passed 94/94 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The suite covers normal Tuple2–Tuple4 construction/equality and mixed values,
Tuple1–Tuple3 lexicographic comparison, and Tuple2–Tuple4 conversion/
deconstruction helper paths. It is an ordinary integral/string happy-path
suite; later arities live in the paired direct source.

## Finding references

- **SR-AUD-046 (extended):** its one double equality case uses only finite
  values and all comparison inputs are integral. It cannot expose that raw
  component operations make a NaN tuple compare equal to a finite tuple and
  unequal to itself.
- **SR-AUD-063:** no test attempts to write a public `ItemN` after
  construction, so the suite does not establish Tuple's .NET immutability
  contract or document a deliberate departure.

## Other missing assertions and diagnostics

- No Tuple1/2/3 hash tests, NaN/signed-zero, empty/embedded-NUL text,
  noncopyable values, or invalid generic-operation diagnostics.
- Extensions omit arities five through seven in this file, Tuple8/nested rest,
  const/reference behavior, and move preservation.
- Comparison tests assert only signs for small ascending integers; they omit
  equal prefixes at high arity, duplicate values, and an unsupported component
  comparison diagnostic.

## Final assessment

Useful ordinary Tuple2–Tuple4 coverage, but generic and immutable-boundary
behavior is absent. No test was modified during this audit.
