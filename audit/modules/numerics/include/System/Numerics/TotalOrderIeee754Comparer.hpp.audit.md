# Audit: `modules/numerics/include/System/Numerics/TotalOrderIeee754Comparer.hpp`

## Metadata

- Audit status: AUDITED (71-line header-only implementation, fully read).
- Validation: `TotalOrderIeee754ComparerTests.*` passed 6/6 in
  `SharpRuntimeTests_Numerics` on 2026-07-25.
- Independent probe: `/tmp/sharp-runtimervc-totalorder-audit-probe.cpp`, built
  with `-fsanitize=undefined,address` and run with
  `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot run beneath the
  sandbox tracer.  It checked all 65,535 adjacent raw `Half` pairs and 100,000
  random raw `float`/`double` pairs against an independent total-order key.

## Assessment

The three implemented specializations correctly map sign-magnitude IEEE binary
encodings to total order: negative values reverse the signed integer comparison,
so negative NaNs sort before negative infinity and signed zeros remain distinct.
The probe found no comparator mismatch or sanitizer diagnostic, including raw
NaN patterns.

However, each specialization inherits only the local `IComparer<T>`.  Current
.NET's `TotalOrderIeee754Comparer<T>` also implements `IEqualityComparer<T>`
and defines equality as `Compare(x, y) == 0`; that counterpart is material
because total-order equality distinguishes raw NaN payloads and signed zero in
the same way as ordering.  The project already provides
`System::Collections::Generic::IEqualityComparer<T>`, but this header neither
includes nor implements it.  A compile-time probe confirms that
`TotalOrderIeee754Comparer<float>` cannot bind to that interface.

Reference: [current .NET TotalOrderIeee754Comparer source](https://source.dot.net/System.Private.CoreLib/src/runtime/src/libraries/System.Private.CoreLib/src/System/Numerics/TotalOrderIeee754Comparer.cs.html).

## Finding references

### SR-AUD-042 — medium — total-order comparer omits the equality-comparer contract

`TotalOrderIeee754Comparer<float>`, `<double>`, and `<Half>` cannot be supplied
where the local `IEqualityComparer<T>` is required and expose neither
`Equals(x,y)` nor `GetHashCode(x)`.  This is an observable API and collection
compatibility gap: a caller cannot use the supplied total-order semantics for
key equality, and substituting ordinary floating equality disagrees on signed
zero and NaN bit patterns.  The missing inheritance is confirmed by a
compile-time `std::is_base_of_v` check in the independent probe.

## Required post-audit verification

Make all three specializations implement `IEqualityComparer<T>` in addition to
`IComparer<T>`.  Define `Equals` through `Compare == 0` and a bit-compatible
hash that guarantees equal total-order values hash alike.  Add tests that bind
each specialization through both interfaces; cover `-0/+0`, identical and
distinct NaN bit patterns, and collection insertion/lookup if a compatible
hash collection is available.

## Other missing assertions and diagnostics

- The six tests omit negative NaNs, signaling/quiet NaNs, payload ordering,
  `Half` signed zero, and all equality/hash behavior.
- No test checks polymorphic binding to the existing equality-comparer
  interface, so the complete interface omission passes the suite.
- The header claims to be a .NET counterpart but does not list its intentional
  generic/specialization limitations or the omitted equality contract.

## Final assessment

The ordering calculation is sound for its three specializations, but the public
type is incomplete relative to the companion .NET comparer and cannot preserve
total-order equality in local hash-based APIs.  No implementation was modified
during this audit.
