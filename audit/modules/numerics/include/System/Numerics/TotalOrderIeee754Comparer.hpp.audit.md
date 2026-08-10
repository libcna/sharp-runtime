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

---

## Post-audit partial remediation — tickets #2169 / #2170 (2026-08-10)

*Appended by review #2167. The original report above is retained verbatim; its ordering assessment
and its 65,535-pair probe result are re-confirmed, not superseded.*

**The finding was split on a measurement.** `build-probe/2167_probe2_layout.cpp` compiled three
shapes side by side:

| Shape | `sizeof` | `alignof` |
|---|---:|---:|
| as it shipped | 8 | 8 |
| **#2169** — non-virtual `Equals`/`GetHashCode` with `IEqualityComparer<T>`'s exact signatures | **8** | 8 |
| **#2170** — `IEqualityComparer<T>` added as a second base | **16** | 8 |

**Ticket #2169 (done)** delivers the *semantics* the finding is about with **no layout change**:
all three specializations expose `Equals(const T&, const T&) const` and
`GetHashCode(const T&) const`, bit-pattern based, agreeing with `Compare == 0` on every vector
tested (`±inf`, `±max`, `±min normal`, `±subnormal`, both signed zeros, ordinary values and two
distinct NaN payloads). `-0` and `+0` are **not** equal and hash differently; identical NaN
payloads are equal and distinct ones are not; the binary64 hash **folds** both halves rather than
truncating, so two values differing only above bit 32 do not collide. A `static_assert` on `sizeof`
pins the layout so the gate below cannot be crossed silently, and `std::is_same_v` assertions on
the member-pointer types keep the signatures aligned with the interface.

**Ticket #2170 (needs_user)** is the remaining half — polymorphic binding, which needs the second
base and therefore the 8 → 16 growth. Public object-layout growth has required explicit
per-action user approval in this repository (#1788 `LinkedList<T>` 40→48, #1789
`BitArray::Enumerator` 32→40) and has been blocked without one (#1889). The risk here is
**measurably low** and recorded so the decision is cheap: the type is header-only, stateless, and
has **zero in-repository users outside its own tests**. Approval sentence in
`docs/SystemNumericsNamespaceReviewPlan.md` §12.

**Status:** `confirmed` → **`confirmed (design-complete)`**.
