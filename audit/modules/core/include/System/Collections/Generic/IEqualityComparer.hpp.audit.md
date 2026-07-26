# Audit: `modules/core/include/System/Collections/Generic/IEqualityComparer.hpp`

## Metadata

- AUDITED: 47-line generic equality-comparer interface, fully read.
- Validation: `GenIEqualityComparerTest.*` passed 4/4 on 2026-07-27; it is
  runtime evidence only because its full collections fixture is still pending
  its own audit report.
- Reference basis: local current-.NET `IEqualityComparer<T>` contract and
  first-party HashCode, immutable collection, equality comparer, and reference
  comparer consumers.

## Assessment

The abstract `Equals` and `GetHashCode` operations, const receiver, and
virtual destructor provide a usable C++ representation for the generic managed
interface.  Existing consumers use it polymorphically for HashCode and
immutable collection comparer overloads.  The four focused cases demonstrate
equal/unequal values and the required equal-values/same-hash relation.

Managed nullable generic parameters cannot be represented literally by C++
`const T&`; pointer/optional-like native T choices must express absence at the
caller type.  That is an ordinary native adaptation, not evidence of a broken
interface declaration.

## Other missing assertions and diagnostics

- The focused fixture only uses `int`; it omits pointer/null, string,
  move-only, throwing, stateful, and deliberately colliding comparer cases.
- No test confirms HashCode's `noexcept` callers handle comparer exceptions
  correctly; HashCode's existing exception policy is reviewed separately.
- The type is an abstract interface, so no test verifies destruction through a
  base pointer or mismatched hash/equality diagnostics.

## Final assessment

The generic comparer declaration is coherent and consumed.  No new finding
and no source or test change.
