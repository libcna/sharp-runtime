# Audit: `modules/core/include/System/ArraySegment.hpp`

## Metadata

- Audit status: AUDITED (339-line header-only implementation, fully read).
- Validation: `build/SharpRuntimeTests_Core_Base --gtest_filter='ArraySegmentTests.*'`
  passed 45/45 on 2026-07-26.
- Independent probe:
  `/tmp/sharp-runtimervc-arraysegment-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-26.

## Assessment

The checked constructor and slicing ranges use the correct unsigned/subtraction
pattern, including a valuable regression for the former addition-overflow
bypass.  The value retains a non-owning `std::vector` pointer, however, and
does not consistently enforce the special invalid state of a default segment.
Both copy routines use forward `std::copy`; the vector form also changes the
fixed-capacity reference contract by resizing its destination.

Reference: [.NET `ArraySegment<T>.CopyTo` contract](https://learn.microsoft.com/en-us/dotnet/api/system.arraysegment-1.copyto?view=net-10.0) and [current .NET `ArraySegment<T>` source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/ArraySegment.cs.html).

## Finding references

- **SR-AUD-018 (extended):** `GetHashCode` masks its sign bit and the tests
  require both a nonnegative value and distinct hashes for different offsets.
  The latter is an invalid collision oracle; the local source additionally has
  no reason to promise the nonnegative-only result required by the test.
- **SR-AUD-043 (extended):** the all-vector constructor casts `array.size()`
  directly to signed 32-bit `intcs` (line 52).  A vector larger than
  `intcs::max()` creates the shared negative metadata state already confirmed
  unsafe in Span/Memory consumers.
- **SR-AUD-044 (extended):** `CopyTo(std::vector<T>&, intcs)` and
  `CopyTo(ArraySegment<T>&)` use forward `std::copy` (lines 225–250).  The
  probe copies the first three `std::string` values of `{a,b,c,d}` into a
  same-vector segment starting at one; both overloads produce `aaaa` instead
  of temporary-preserving `aabc`.

### SR-AUD-054 — high — default ArraySegment operations silently succeed or dereference null instead of throwing

The default constructor deliberately stores `array_ == nullptr`, matching the
existence of .NET's default value.  Current .NET consistently calls
`ThrowInvalidOperationIfDefault` before `Slice`, `CopyTo`, `ToArray`, and
enumeration.  The local `Slice(0)` only checks index against zero then binds
`*array_` to construct another segment.  The sanitizer probe reports
null-reference binding and member access before ASan aborts with a SEGV.

Other local operations are not consistently safer: default `ToArray()` returns
an empty vector through a null-pointer range, and default `CopyTo`/`Contains`
can complete as no-ops under this library rather than reporting the invalid
underlying array.  This is both a parity breach and a reachable null-dereference
path.

### SR-AUD-055 — medium — vector CopyTo resizes an undersized destination instead of enforcing ArraySegment capacity

`CopyTo(std::vector<T>&, intcs)` calculates `needed` then calls
`destination.resize` (lines 225–236).  `CopyTo(std::vector<T>&)` forwards to
that behavior.  .NET requires a dimensioned destination array and forwards to
`Array.Copy`, which rejects an insufficient destination rather than changing
its length.  The local test `CopyTo_VectorWithOffset_ExpandsDest` explicitly
locks in expansion from an empty vector to five values; the header documents
it but does not identify it as an intentional API deviation.

**Impact:** callers can observe an unexpected allocation, vector size change,
and a missing capacity exception.  This matters particularly because the
nearby `CopyTo(ArraySegment<T>)` correctly rejects a short destination.

## Required post-audit verification

Add one private default-state guard and invoke it in every operation that
requires an underlying array, matching the source's `InvalidOperationException`
boundary before pointer/range formation.  Add default-state tests for Slice,
both CopyTo forms, ToArray, search, and range-for under ASan/UBSan.

Replace both forward-copy paths with overlap-aware copying or a temporary and
test both directions with nontrivial values.  Decide whether the vector
adaptation intentionally permits resizing; if not, reject short capacity before
mutation, and if it is retained, document it prominently as a breaking
deviation rather than calling it a direct counterpart.  Coordinate the vector
size narrowing repair with SR-AUD-043.

## Other missing assertions and diagnostics

- No test mutates, shrinks, reallocates, or destroys the non-owning backing
  vector while a segment exists; lifetime and structural-mutation assumptions
  are only implicit.
- `ToArray`/range-for tests cover ordinary and empty backed segments but not
  the default null-array state.
- Copy tests use only non-overlapping integers and do not assert the reference
  behavior for an undersized ordinary destination.

## Final assessment

The bounded construction/slicing repair is sound, but non-owning/default-state
handling and copy semantics leave a sanitizer-confirmed crash and several
observable .NET divergences.  No production source was modified during this
audit.

---

## SR-AUD-054 — REMEDIATED, with one named residual (ticket #2214, 2026-08-10, family CMS-B)

The original evidence above is retained unchanged. **Only SR-AUD-054 is closed by this
ticket.** SR-AUD-055 (the vector `CopyTo` resize) and the SR-AUD-018 extension in this same
report stay `confirmed` and are **not touched**; the SR-AUD-043 extension was closed
earlier by #1852, and the SR-AUD-044 extension is closed by #2216. **No `SR-AUD-*`
identifier was created**; numbering stays frozen at 364. Family record:
`docs/CoreMemorySafetyFamilyPlan.md`.

### The finding is twelve doors, of which only two are memory-unsafe

The audit named `Slice(0)` and said other operations "are not consistently safer".
Enumerated from the source and measured on 2026-08-10 under ASan+UBSan over the
instrumented header body itself (`build-probe/2210_before.log`, case
`obs.segment_default_others`):

| Door | Before | After |
|---|---|---|
| `Slice(intcs)` | UBSan *reference binding to null pointer* (`ArraySegment.hpp:180`) → member call on null (`:73`) → **ASan SEGV** in `vector::size()` | `InvalidOperationException` |
| `Slice(intcs, intcs)` | the same, at `ArraySegment.hpp:198` — **not named by the audit** | `InvalidOperationException` |
| `operator[](intcs)` | `ArgumentOutOfRangeException` | `InvalidOperationException` |
| `operator[](intcs) const` | `ArgumentOutOfRangeException` — **not named by the audit** | `InvalidOperationException` |
| `CopyTo(vector&, intcs)` | silent no-op | `InvalidOperationException` |
| `CopyTo(vector&)` | silent no-op | `InvalidOperationException` |
| `CopyTo(ArraySegment&)` | silent no-op | `InvalidOperationException` |
| `ToArray()` | empty vector | `InvalidOperationException` |
| `Contains(const T&)` | `false` | `InvalidOperationException` |
| `IndexOf(const T&)` | `-1` — **not named by the audit** | `InvalidOperationException` |
| `begin()` / `end()` | `nullptr`, zero iterations | **unchanged — residual #2215** |

**Premise correction: the indexers already threw, with the wrong type.** `count_ == 0` on
a default segment makes the range check fire before the null dereference can, so
`ArraySegment<int>()[0]` raised `ArgumentOutOfRangeException` where .NET raises
`InvalidOperationException`. That is a silent-wrong-*exception* case the audit did not
record, and it is why the guard has to run **first** rather than merely exist.

**Premise correction: only two of the twelve doors are memory-unsafe.** The other ten are
parity breaches with no memory error at all. Saying so keeps the ASan evidence honest —
this finding's sanitizer reports come from `Slice` and nowhere else.

### The repair

One private `throwIfDefault()`, the port's counterpart of .NET's
`ThrowInvalidOperationIfDefault()`, throwing `InvalidOperationException` with .NET's own
`SR.InvalidOperation_NullArray` text, **"The underlying array is null."** It is the first
statement of all ten guarded doors. `CopyTo(ArraySegment&)` checks **this** segment, then
the destination, then the length — .NET's exact order. In `CopyTo(vector&, intcs)` the
guard precedes the destination `resize`, so a rejected call cannot even change the
destination's size.

**Deliberately unguarded, matching .NET:** `getArrayProperty`, `getOffsetProperty`,
`getCountProperty`, `Equals`, `operator==`, `operator!=`, `GetHashCode`, `getEmpty`. .NET's
`Array`/`Offset`/`Count`/`Equals`/`GetHashCode` do not call
`ThrowInvalidOperationIfDefault` either, and a test asserts they are **not** guarded so an
over-correction fails. `ArraySegment<T>::Empty` is a real segment over a shared empty
vector, not a default one, so every guarded door still works on it.

### The residual, stated rather than implied away

`begin()`/`end()` are this port's `GetEnumerator()` counterpart, and .NET's
`GetEnumerator()` throws for a default segment. Guarding them requires **dropping
`noexcept`**, an exception-specification change this repository treats as approval-gated —
the precedent is ticket #1854, which needed explicit user approval for exactly that on
`ReadOnlyMemory`'s constructors. Tracked as **ticket #2215 (`needs_user`)**. The header
carries an explicit `@warning`, and
`CoreMemorySafetyDefaultSegmentTests.PIN_2215_EnumerationDoorIsStillUnguarded` pins the
current behaviour — including two `static_assert`s on the `noexcept` specification — so the
day #2215 ships the pin **fails loudly** instead of the divergence quietly persisting.

### Closure evidence

After-log `build-probe/2214_after.log`: both `Slice` SEGVs and all three UBSan
null-reference reports **absent**, each replaced by
`InvalidOperationException: The underlying array is null.`; the deliberate
`heap-buffer-overflow` control still reports in the same binary. A side effect worth
recording: the guard also removed the probe's `-Wnonnull` warning from
`stl_algobase.h`, which came from `ToArray`/`CopyTo` forming a null-to-null range.

**+8 permanent regressions**: both `Slice` overloads with the exact message; both indexers
with the corrected exception **type**, including a const segment and an independently
out-of-range index; all three `CopyTo` forms with the destination proved unmodified;
source-before-destination-before-length order for `CopyTo(ArraySegment&)`, including the
case where a short destination would otherwise win; `ToArray`/`Contains`/`IndexOf`; the
eight unguarded doors still answering, plus `Empty` still working through every guarded
door; a non-default segment producing every previous result and every previous
out-of-range diagnostic; and the #2215 pin.

`SharpRuntimeTests_Core_Base` **5,617/5,617** (1 pre-existing skip); `ArraySegmentTests`
45/45, `SharpRuntimeTests_Buffers` 618/618 and `SharpRuntimeIntegrationTests` 893/893 all
unchanged and green. Whole repository builds with zero errors and zero warnings.

### Six mutations, six killed — all by assertion

M1 delete the `ToArray` guard. M2 delete the `Contains` guard. M3 delete **only** the
`const` indexer's guard (proving both overloads are separately covered). M4 move the two
guards after the length check in `CopyTo(ArraySegment&)` — killed by the valid-source /
default-destination case, which then reports `ArgumentException`. M5 delete the
`CopyTo(vector&, intcs)` guard. M6 over-reject by adding `|| count_ == 0` — killed by the
unguarded-doors test **and** by the pre-existing `ArraySegmentTests.ToArray_EmptySegment_EmptyVector`.

### Source, ABI and layout consequences

**None.** `throwIfDefault()` is a private, non-virtual member function; `ArraySegment<T>`
has no virtual functions, so no vtable exists to change, and no data member was added,
removed or reordered, so `sizeof`/`alignof` are unchanged. No public signature, no
`noexcept` specification (that is exactly what #2215 is for), no default argument and no
mangled symbol changed. One new intra-module include
(`System/InvalidOperationException.hpp`), so no component boundary or dependency edge moved.
