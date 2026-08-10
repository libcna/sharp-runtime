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
