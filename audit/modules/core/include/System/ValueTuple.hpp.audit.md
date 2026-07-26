# Audit: `modules/core/include/System/ValueTuple.hpp`

## Metadata

- Audit status: AUDITED (874-line public template header, fully read).
- Supporting validation: the combined dedicated and aggregate
  `ValueTuple*` Core.Base filter passed 53/53 on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-valuetuple-audit-probe.cpp` builds with
  `c++ -std=c++20 -Wall -Wextra -Wpedantic -I modules/core/include` and prints
  `0,0,0,0,0,0` for `ValueTuple1<float>` and `ValueTuple8` NaN comparisons /
  self equality where .NET's default comparers produce ordering and equality.

## Assessment

The zero through eight element layouts, factory flattening convention, and
ordinary lexicographic behavior are coherent for the covered integral and
string-like values.  The design deliberately uses C++ template operations:
stream insertion for text, `std::hash<T>` for hashes, `==` for equality, and
`<` for comparison.  That much broader domain than C#'s value-type/
comparer-based generic surface is not expressed through concepts; errors emerge
only when a selected member is instantiated.  `ValueTuple8` additionally
requires a `Rest` type with `Equals`, `GetHashCode`, `CompareTo`, and
`ToString`, without a public constraint or early diagnostic.

## Finding references

### SR-AUD-046 — medium — raw C++ tuple comparisons do not preserve .NET floating comparer semantics

Every generic tuple's `CompareTo` uses a pair of raw `<` operations and its
equality uses raw `==`; `ValueTuple8` applies the same policy to its first
seven elements and calls the same methods on `Rest`.  Thus a NaN item compares
as neither less nor greater than a finite item, so `CompareTo` returns zero,
and `Equals` says a tuple does not equal itself.  The standalone reproducer
shows this for `ValueTuple1<float>` and a NaN in `ValueTuple8`.

Current local .NET `ValueTuple.cs` instead calls `Comparer<T>.Default.Compare`
for each comparison and `EqualityComparer<T>.Default.Equals` for each equality
component.  Its floating comparer gives NaN a stable order relative to finite
values and its equality comparer treats NaN as equal to itself.  This extends
SR-AUD-046; it is the same confirmed policy defect already present in Array,
MemoryExtensions, and Nullable rather than an independent issue.

## Other missing assertions and diagnostics

- Direct tests use only integral and `std::string` values.  They omit
  float/double NaN, signed zero, a custom equality/comparison type, and a
  type with a valid .NET-like comparer but no raw C++ relational operators.
- No test verifies a hash/equality invariant for any tuple containing NaN or
  probes the implementation-defined narrowing of `std::hash<T>` to `intcs`.
- The direct suite has no `ValueTuple8` test; aggregate smoke tests cover its
  normal construction but not invalid `Rest`, nested rest, comparison, hash,
  or NaN behavior.
- There is no boundary diagnostic for an unsuitable `TRest` or for a selected
  `T` that lacks streaming, hashing, equality, or ordering.  A future API pass
  should make the intended C++ constraints explicit rather than allowing a
  member-specific template error.

## Final assessment

The normal arity/factory behavior is materially covered, but the generic
comparison/equality implementation extends the known floating-comparer parity
defect and its constraints are not diagnosable at the public type boundary.
No source or test was modified during this audit.
