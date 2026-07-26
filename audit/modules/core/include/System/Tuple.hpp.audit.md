# Audit: `modules/core/include/System/Tuple.hpp`

## Metadata

- Audit status: AUDITED (798-line public template header, fully read).
- Supporting validation: `TupleTests.*:TupleExtensionsTests.*:TupleCompareTests.*`
  passed 94/94 in `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-tuple-audit-probe.cpp`. Its `nan` mode
  prints `0,0,0,0`; UBSan `hash` mode reports the signed overflow below; its
  `mutation` mode compiles and prints `99,2` after overwriting `Item1`.

## Assessment

The one-to-eight arity layouts, factory wrapping of the eighth item, ordinary
lexicographic paths, basic conversion helpers, and conventional text rendering
are coherent for the covered integral and string-like values.  The header is
template-generic but relies directly on raw C++ equality, ordering, hashing,
stream insertion, copyability, and (for `Tuple8`) a `Rest` that happens to
support every selected operation. Those constraints are not stated at the
public type boundary.

## SR-AUD-046 — medium — Tuple uses raw C++ operations instead of .NET default component comparers

`detail::tupleCompare` performs raw `<` in both directions and every `TupleN`
equality operator performs raw `==`; `Tuple8` applies the same policy to
`Rest`. The probe shows that `Tuple1<float>(NaN)` compares equal to
`Tuple1<float>(1.0f)` and is unequal to itself; the same occurs in a Tuple8
first component. Current local .NET `Tuple.cs` delegates structural comparison
and equality to `Comparer<object>.Default` and `EqualityComparer<object>.Default`,
which retain .NET floating comparison/equality semantics. This extends
SR-AUD-046 rather than creating a duplicate finding.

## SR-AUD-062 — high — tupleHashCombine overflows signed intcs for reachable hash inputs

`detail::tupleHashCombine` computes `((h1 << 5) + h1) ^ h2` in signed
`intcs`. C++20 gives the shift's bit behavior, but the subsequent signed
addition remains undefined when it is not representable. The header-only UBSan
reproducer invokes `Tuple2<int,int>(0x03ffffff, 0).GetHashCode()` and reports:

```
Tuple.hpp:23:27: runtime error: signed integer overflow:
67108863 + 2147483616 cannot be represented in type 'int'
```

The same combine helper is used by Tuple2 through Tuple8. Local .NET's
`Tuple.CombineHashCodes` executes defined unchecked `int` arithmetic. The
repair must use unsigned/wider bit arithmetic and then explicitly convert to
the intended 32-bit result, preserving equal-object hash consistency without
performing an overflowing signed addition first.

## SR-AUD-063 — medium — tuple components are publicly mutable despite the immutable .NET contract

Every `TupleN` is a `struct` with public mutable `ItemN` fields (and public
`Rest`). The probe modifies `Tuple::Create(1, 2).Item1` to produce `99,2`.
Current local `Tuple.cs` exposes private `readonly` backing fields through
getter-only properties. This port has not documented a mutable-value-type
adaptation, so consumers can mutate a value intended to be an immutable tuple,
including after using it as a structural key or value object.

## Other missing assertions and diagnostics

- `Tuple8` accepts any `TRest` at construction. .NET requires an internal
  tuple rest and throws for an invalid rest; the C++ template instead gives a
  delayed member-instantiation error or permits a structurally unrelated type.
  A future API pass should constrain this deliberately.
- The documented deeper-rest hash adaptation is not tested. Although exact
  hash values should not be persisted, equal values must remain equal-hashed
  across nested rest shapes.
- No declaration prevents a selected item from lacking `std::hash`, stream
  insertion, `==`, or `<`; public diagnostics occur only at the first selected
  member operation.

## Final assessment

Normal arity behavior is broad, but generic comparison extends the known NaN
defect, hash computation has UBSan-confirmed UB, and the public type loses
Tuple's immutable contract. No source or test was modified during this audit.
