# Audit: `modules/core/include/System/TupleExtensions.hpp`

## Metadata

- Audit status: AUDITED (123-line header-only helper, fully read).
- Supporting validation: `TupleTests.*:TupleExtensionsTests.*:TupleCompareTests.*`
  passed 94/94 in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The helper copies Tuple1 through Tuple7 to/from `std::tuple` correctly for the
covered value types. It is a port-specific interop convenience rather than a
direct full .NET class and deliberately offers no Tuple8 conversion or
flattening. It inherits source tuple mutability and template constraints but
does not independently alter comparison or hash behavior.

## Finding references

- No independent evidence-backed finding. See the paired `Tuple.hpp` report
  for the underlying tuple comparison, hash, and mutability findings.

## Other missing assertions and diagnostics

- There is no Tuple8/nested-rest conversion, const/reference preservation,
  move-only-element, or conversion-failure coverage.
- Every conversion copies elements; the public API neither documents copying
  cost nor gives a diagnostic for a noncopyable selected element.
- `std::tuple` is described as a ValueTuple equivalent even though it has
  different API, mutability, and formatting semantics; this is an adaptation
  that should remain explicit to consumers.

## Final assessment

The narrow arity-one-through-seven copy interop works for covered values. No
source or test was modified during this audit.
