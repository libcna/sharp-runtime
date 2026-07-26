# Audit: `modules/core/include/System/Array.hpp`

## Metadata

- Audit status: AUDITED (554-line header-only implementation, fully read).
- Validation: `build/SharpRuntimeTests_Core_Base --gtest_filter='ArrayTests.*'`
  passed 80/80 on 2026-07-26.
- Independent probe: `/tmp/sharp-runtimervc-array-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-26.

## Assessment

The vector overloads have careful, recently strengthened range validation and
implement a useful partial .NET Array surface.  The dedicated tests validate
normal results plus a broad set of index/count errors.  The implementation
then drops from checked vector semantics to raw pointer arithmetic and
unconstrained `memcpy`, and it delegates float ordering and empty callable
behavior directly to C++ defaults rather than the documented .NET contract.

References: [.NET `Array.Copy` contract](https://learn.microsoft.com/en-us/dotnet/api/system.array.copy?view=net-10.0), [.NET `Array.Exists` contract](https://learn.microsoft.com/en-us/dotnet/api/system.array.exists?view=net-10.0), and [current .NET `Array` source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/Array.cs.html).

## Finding references

### SR-AUD-044 (extended) — high — vector `Array::Copy` corrupts a right-overlapping range

The five-argument vector overload (lines 103–110) assigns forward from `src`
to `dst`.  Both references may designate the same vector, unlike the current
tests' separate `int` vectors.  The probe copies three `std::string` elements
from offset zero to offset one in `{a,b,c,d}` and prints `aaaa`, not the
temporary-preserving `.NET` result `aabc`.  This is another independently
reproduced consumer of the overlap-safe copy requirement in SR-AUD-044.

### SR-AUD-046 (extended) — medium — Array default sort and binary search use C++ float operators instead of .NET ordering

All default `Sort` and `BinarySearch` overloads (lines 40–43 and 218–251)
use raw `<`/`==`.  The probe sorts `{3, NaN, 1}` as `1,3,NaN`; .NET's default
float comparison orders NaN before finite values.  Searching the already
.NET-ordered `{NaN,1,3}` for NaN returns `-1`, because `NaN == NaN` and
`NaN < value` are both false.  `std::sort` is not justified with a comparator
whose NaN equivalence relation does not implement the reference comparison
contract.  This extends the confirmed MemoryExtensions ordering finding.

### SR-AUD-051 — high — raw-pointer `Array::Copy` applies unchecked `memcpy` to arbitrary objects and negative lengths

The public raw-pointer overload (lines 113–124) accepts every `T`, every
signed index, and every signed length, then computes pointer offsets and casts
`length * sizeof(T)` to `size_t` for `std::memcpy`.  It has no constraint to
trivially copyable types, no negative-argument check, and chooses `memcpy`
even though the reference explicitly specifies `memmove` overlap behavior.

**Reproductions:**

- Copying two `std::string` values through this overload emits GCC's
  `-Wclass-memaccess` warning and later produces ASan's `attempting free on
  address which was not malloc()-ed`, because byte copying duplicates string
  ownership state rather than assigning elements.
- Copying two `int` buffers with `length == -1` produces ASan
  `negative-size-param: (size=-4)` in `memcpy`; negative indexes similarly
  form pointers before their arrays.

**Impact:** a normal template instantiation with an owning/nontrivial value
can corrupt lifetime state, while invalid public signed arguments become
out-of-bounds memory operations.  The source comment acknowledges unavailable
raw-buffer *upper* bounds, but that does not justify accepting negative public
arguments or violating the value-copy/overlap semantics it labels as
`Array::Copy`.

### SR-AUD-052 — medium — Array delegate overloads do not validate empty `std::function` at the public boundary

`Sort` with comparison, comparison `BinarySearch`, `ConvertAll`, all
predicate/action search helpers, `ForEach`, and `TrueForAll` invoke their
`std::function` directly.  An empty `std::function` throws
`std::bad_function_call` only if iteration reaches it; on an empty vector,
`Exists` returns false without any error.  The probe prints `empty-no-throw`
for empty `Exists` and `bad_function_call` for a one-element vector.  Current
.NET checks a null delegate before iteration and throws
`ArgumentNullException`, including for empty arrays.

### SR-AUD-053 — low — `Array::MaxLengthProperty` reports `INT32_MAX`, not the .NET runtime limit

Lines 31–33 return 2,147,483,647, while current .NET `Array.MaxLength` is
the documented runtime limit `0x7FFFFFC7` (2,147,483,591).  The difference is
small and does not promise allocation success, but this public counterpart is
an exact observable property with no documented vector-adaptation exception.
The only test asserts positivity, so it cannot detect the mismatch.

## Required post-audit verification

Replace vector forward copy with direction-aware copying or a temporary and
test both overlap directions using `std::string`.  Redesign the raw overload:
either constrain it to trivially copyable values, validate every signed
argument before pointer arithmetic, and use `memmove`, or preserve generic
value semantics with directional element assignment.  Its raw upper-bound
limitation must remain explicitly documented.

Route default ordering through the local/.NET comparison policy and add NaN
sort/search vectors.  Reject empty `std::function` values before examining
array length, consistently across every callable overload, with the project's
mapped argument exception.  Align `MaxLengthProperty` with .NET or document a
deliberate vector-runtime adaptation and test the exact value.

## Other missing assertions and diagnostics

- No copy test uses source and destination aliases, nontrivial elements, raw
  overlap, a negative raw index/length, or a raw `std::string` array.
- Sort and binary-search tests use only integral values; NaN, signed zero, and
  custom-comparer empty/error paths are absent.
- Every callable test supplies a lambda; no empty callable is tested on either
  empty or nonempty vectors, despite their currently different outcomes.
- Range tests do not exercise `INT_MAX` arguments with a bounded fake
  container, so arithmetic boundary diagnostics remain implicit.

## Final assessment

Ordinary vector operations have good range coverage, but public copying,
default float ordering, callable diagnostics, and `MaxLength` parity contain
confirmed gaps.  No production code was modified during this audit.
