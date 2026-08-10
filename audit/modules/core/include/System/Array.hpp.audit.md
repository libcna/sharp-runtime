# Audit: `modules/core/include/System/Array.hpp`

## Metadata

- Audit status: AUDITED (554-line header-only implementation, fully read).
- Validation: `build/SharpRuntimeTests_Core_Base --gtest_filter='ArrayTests.*'`
  passed 80/80 on 2026-07-26; the direct fifteen-suite range/copy/search
  filter passed 38/38 on 2026-07-27 and its source is fully audited in
  `Batch11ArrayTests.cpp.audit.md`.
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

---

## SR-AUD-052 — REMEDIATED (ticket #1869, 2026-07-30, CCF-011)

The original evidence above is retained unchanged. **Only SR-AUD-052 is closed by
this ticket.** SR-AUD-044, SR-AUD-046, SR-AUD-051 and SR-AUD-053 in this same
report remain `confirmed`; none of them is touched.

All seventeen delegate-taking overloads — `Sort` ×2, `BinarySearch` ×2,
`ConvertAll`, `Exists`, `Find`, `FindLast`, `FindAll`, `FindIndex` ×3,
`FindLastIndex` ×3, `ForEach`, `TrueForAll` — now call one shared private
`requireCallable(callable, paramName)` before any element is examined, throwing
`System::ArgumentNullException` with the parameter's own .NET name.

**Parameter renames, deliberate.** The twelve `Predicate<T>`-shaped parameters
were named `predicate`; .NET names them `match`, and the name is carried in the
observable message (`Value cannot be null. (Parameter 'match')`). They are
renamed to `match` in signature and doc-comment so the two agree. In C++ a
function-parameter name contributes nothing to the interface — no designated
arguments, no mangling — so this is source-compatible and ABI-neutral.
`comparison`, `converter` and `action` already matched .NET and are unchanged.

**Validation order reproduces .NET's asymmetry.** `Array.FindIndex` validates
`startIndex`, then `count`, then `match` (`Array.cs:1599-1622`);
`Array.FindLastIndex` validates `match` first and `startIndex`/`count` second
(`Array.cs:1671-1706`). The port now does the same, in both directions, and two
permanent tests pin it.

**Correction to the finding's premise (measured 2026-07-30).** The finding says
an empty `std::function` "throws `std::bad_function_call` only if iteration
reaches it; on an empty vector, `Exists` returns false without any error". That
understates the silent set: `Sort(oneElement, {})` and
`BinarySearch(oneElement, …, {})` were **also** silent, because `std::sort` and
the search loop never compare below two elements and an empty array never enters
the loop (`array.sort.comparison.one=no-throw`,
`array.binarysearch.emptyarray=no-throw`, `build-probe/1866_prefix.log`).
Additionally, `Array::FindLastIndex` reported its *range* error before the
callable, where .NET reports the callable first; correcting that is inseparable
from adding the check at all, so it is folded in here and receives **no** new
`SR-AUD-*` identifier (the numbering stays frozen at 364). The historical text
above is left as written, per this repository's practice.

**One observable behaviour change, deliberate.** A call that used to return an
ordinary result — an empty (or one-element, for the sort/search entries) array
with an empty callable — now throws. This is .NET's documented behaviour for the
identical call, and the call was already wrong; `TrueForAll(empty, {})` in
particular used to return the vacuous `true` and hide the invalid argument
entirely. Recorded as B1 in `docs/EmptyCallableBoundaryPlan.md` §9.

Closure evidence: 13 new permanent regressions in `ArrayTests.cpp` (every
overload for empty and non-empty input; the exact `paramName` for all four names;
no partial mutation by a rejected `Sort`; both validation orders; `TrueForAll`'s
lost vacuous-true fast path together with proof that a real predicate still gets
it; catchability as `System::Exception`; and a regression pass asserting every
overload still produces its pre-existing result with a real callable).
`ArrayTests` 93/93, `SharpRuntimeTests_Core_Base` 5,293/5,293, whole-repository
build clean with zero errors and zero warnings. The direct probe
`build-probe/1866_empty_callable_probe.cpp`, compiled **with**
`-fsanitize=address,undefined` so this header-only template change is itself
instrumented, exits 0 with zero AddressSanitizer, UndefinedBehaviorSanitizer and
LeakSanitizer reports (`build-probe/1869_postfix_asan.log`); all 26 `array.*`
cases report `ArgumentNullException` with the expected parameter name and the two
ordering cases report the expected exception type.

Source, ABI and layout consequences: none. `Array` is a static-only class
(`Array() = delete`) with no data members; every entry is a function template, so
no mangled symbol exists to change. No `noexcept` specification, virtual function
or default argument changed.

The plan for this family is `docs/EmptyCallableBoundaryPlan.md` (ticket #1866).


---

## SR-AUD-046 — REMEDIATED (CCF-010, tickets #1904-#1910, 2026-07-31)

The original evidence above is retained unchanged. **Only SR-AUD-046 is closed
by this family**; every other finding in this report stays `confirmed` and none
of them is touched.

One shared policy, `modules/core/include/System/detail/ComparisonPolicy.hpp`,
now states the port's counterpart of `Comparer<T>.Default` and
`EqualityComparer<T>.Default` once — `compareValues`, `equalValues`,
`hashValue`, `DefaultLess`/`DefaultGreater`, `moveNaNsToFront` and
`defaultSort`, each `if constexpr`-gated so every non-floating instantiation
generates exactly the code that was there before. All 66 comparison sites
across `Array.hpp`, `MemoryExtensions.hpp`, `Nullable.hpp`, `ValueTuple.hpp`,
`Tuple.hpp` and `Linq.hpp` route through it.

**The finding understates its own severity, and the correction is the point.**
`Array::Sort` and `MemoryExtensions::Sort` did not merely place NaN wrongly:
raw `<` over a NaN-bearing range is not a strict weak ordering, so
`std::sort`'s precondition was violated and the **finite** elements came out
unsorted — measured at **64 of 196** size/density/placement shapes, worst case
**3,874 inversions** in 65,536 elements, with **AddressSanitizer,
UndefinedBehaviorSanitizer and `_GLIBCXX_DEBUG` all silent**. Both now use
.NET's own repair (`ArraySortHelper.cs:285-305`): move every NaN to the front
in a pre-pass, then sort the NaN-free remainder, so no NaN ever reaches the
comparator. The 196-shape sweep reads `corrupted=0` afterwards.

All **28** measured defect rows now return .NET's value; the **8** rows that
were already correct are unchanged and now pinned — including
`Nullable<T>::operator==`, which is C#'s *lifted* `==` and is `false` for
NaN vs NaN in .NET too, and `MemoryExtensions::SequenceCompareTo` of two NaNs.
`Linq::Min` and `Linq::Max` follow the reference's deliberately **asymmetric**
NaN rules rather than being made symmetric.

Evidence: 70 permanent add-only regressions in
`modules/core/tests/System/ComparisonContractTests.cpp`; whole repository
**14,815 tests across 37 executables**, from 14,745; build clean with zero
errors and zero warnings; `nm --extern-only` identical before and after
(6,168 symbols); `sizeof`/`alignof` identical for all eleven measured
instantiations. **Ten mutations**, one of them a deliberate *negative* control
that must still pass. ASan/UBSan/LSan clean with activation proved separately;
TSan recorded **not applicable** — nothing in the family has shared mutable
state. Full record: `docs/ComparisonContractPlan.md`.

---

## SR-AUD-051 — REMEDIATED (ticket #2213, 2026-08-10, family CMS-A)

The original evidence above is retained unchanged. **Only SR-AUD-051 is closed by this
ticket** — including its extension into `Buffer.hpp` recorded in that file's own report.
SR-AUD-044 and SR-AUD-053 in this same report stay `confirmed`; SR-AUD-044 is closed
separately by ticket #2216 and **SR-AUD-053 (`MaxLengthProperty`) is not touched at all**.
**No `SR-AUD-*` identifier was created**; numbering stays frozen at 364. Family record:
`docs/CoreMemorySafetyFamilyPlan.md`.

### The finding is four doors, not two

The audit recorded two reproductions (nontrivial `T`, and negative `length`). Measured on
2026-08-10 under AddressSanitizer over the instrumented header body itself
(`build-probe/2210_before.log`), the raw overload fails in **four** distinct ways, of which
two are new:

| Input | Before | After |
|---|---|---|
| `length = -1` | **AddressSanitizer: unknown-crash** in `memcpy` (`Array.hpp:154`) | `ArgumentOutOfRangeException (Parameter 'length')` |
| `srcIndex = -4` | **stack-buffer-underflow** in `memcpy` — **not recorded by the audit** | `ArgumentOutOfRangeException (Parameter 'srcIndex')` |
| two `std::string` elements | **double-free** at `operator delete` | copies cleanly; ASan **and** LeakSanitizer silent |
| overlapping `int` ranges | **memcpy-param-overlap** — **not recorded by the audit** | `1123`, .NET's `memmove` answer |

The audit predicted `negative-size-param` for the negative length and "attempting free on
address which was not `malloc()`-ed" for the nontrivial case; this toolchain reports
`unknown-crash` and `double-free`. GCC 13.3 at `-O1` with `_FORTIFY_SOURCE` inlines
`__memcpy_chk`, so libsanitizer's `memcpy` interceptor — which is what emits
`negative-size-param` — never runs. The after-log is `build-probe/2213_after.log`; the
deliberate `heap-buffer-overflow` control still reports in the same binary.

### The repair keeps generic value semantics

The audit offered two designs: constrain the overload to trivially copyable values, or
"preserve generic value semantics with directional element assignment". **The second was
chosen**, because .NET's `Array.Copy` works for every element type and constraining this
overload would have been a source break with no .NET counterpart. The body now:

1. rejects a negative `srcIndex`, then `dstIndex`, then `length`, **before forming any
   pointer** — which is the difference between throwing and having already computed an
   invalid pointer;
2. rejects a null buffer **when `length > 0`**. A null pointer with a zero length is the
   ordinary C++ empty-range idiom and is accepted; .NET rejects a null array
   unconditionally, and that divergence is deliberate and documented in the header;
3. copies through `System::detail::copyOverlapAware` (new,
   `System/detail/OverlapCopy.hpp`) — element assignment, with the direction **selected**
   from the operands' relative addresses using `std::less` (relational comparison of
   pointers into different objects is unspecified in ISO C++; `std::less` is required to
   totally order them).

`Array::Copy`'s documented capacity limitation is unchanged and still stated: a raw pointer
carries no length, and that remains the caller's responsibility.

### The `Buffer` extension is closed by refusing to compile

`Buffer::BlockCopy(const std::vector<T>&, …)`, `ByteLength`, `GetByte` and `SetByte` said
in their doc-comments that `T` had to be primitive or trivially copyable and enforced
nothing, so `std::vector<std::string>` compiled, reached `memmove` and produced an
ASan-confirmed **double-free**. All four now call one shared private
`requireTriviallyCopyable<T>()`. .NET rejects the equivalent call at run time
(`ArgumentException("Object must be an array of primitives.")`); in C++ the element type is
known at compile time, so the faithful counterpart is a `static_assert`.

**This is the family's only compile-time source break.** It is stated rather than smoothed
over: a call with an owning element type stops compiling. Every such call was already
corrupting memory, no in-repo call site is affected (every one uses `int`, `double`,
`bytecs` or an enum), and the migration is named in the header and in the fixture —
`System::Array::Copy` for elements, or the `std::vector<bytecs>` overload for bytes.
`std::is_trivially_copyable_v` is deliberately **wider** than .NET's "primitive": it also
admits trivially copyable structs and enums, which is the property that actually makes the
byte copy defined.

### Closure evidence

**+9 permanent regressions** in `modules/core/tests/System/CoreMemorySafetyTests.cpp`:
all four negative-argument rejections including `INTCS_MIN`; the exact `paramName` and the
`srcIndex` → `dstIndex` → `length` order; null rejected only when it would be touched, and
the zero-length null idiom still accepted; owning elements copied as values with distinct
storage afterwards; overlap correct in **both** directions plus exact-self, adjacent, zero
length and one element; ordinary disjoint copies unchanged; catchability as
`System::Exception`; the requirement pinned as exactly `is_trivially_copyable`; and every
trivially copyable element type (struct, enum, `double`) still working through all four
`Buffer` members.

**+4 negative consumer sites**, `test/consumer/core_buffer_trivially_copyable_negative.cpp`
(component `Core.Base`), one per constrained member, each `#else` branch carrying the
accepted trivially-copyable spelling. Baseline compiles with zero diagnostics; all four
sites rejected.

`SharpRuntimeTests_Core_Base` **5,609/5,609** (1 pre-existing skip); `ArrayTests`,
`Batch11ArrayTests`, `BufferTests` and `Batch13BufferTests` unchanged and green. Whole
repository builds with zero errors and zero warnings.

### Six mutations, six killed — and one that survived first, reported

M1 delete the `srcIndex` guard → 2 fail. M2 delete the `dstIndex` guard → 2 fail. M3
`if (length >= 0)` for the null check (over-rejects the legal zero-length null idiom) → 1
fail. M4 forward-only `std::copy` (restore the defect) → 1 fail. M6 drop one
`requireTriviallyCopyable` → the negative fixture checker reports
`site 1 COMPILED; lines 53-59 are legal again`.

**M5 — an unconditional `std::copy_backward` — survived the first run, and that is the most
useful result of the ticket.** libstdc++ lowers **both** `std::copy` and
`std::copy_backward` over a trivially copyable type to `__builtin_memmove`, which is
correct in either direction, so the `int` overlap assertions could not tell a
direction-*selecting* copy from an unconditionally backward one. This is the same masking
the audit noticed for the forward case, generalised: **a direction assertion in this family
proves nothing unless its element type is non-trivially-copyable.** A `std::string`
left-overlap assertion was added; M5 then failed with `dddd` where `bcdd` is required.

### Source, ABI and layout consequences

None, other than the deliberate compile-time rejection above. `Array` and `Buffer` are
static-only classes with no data members and no virtual functions; every entry is a
function template or an inline static member, so no `sizeof`, `alignof`, vtable, `noexcept`
specification, default argument or mangled symbol changed. `System/detail/OverlapCopy.hpp`
is a new header inside the same `Core.Base` module — no component boundary or dependency
edge changed.

---

## SR-AUD-044 — REMEDIATED (ticket #2216, 2026-08-10, family CMS-C)

The original evidence above is retained unchanged. **No `SR-AUD-*` identifier was
created**; numbering stays frozen at 364. Family record:
`docs/CoreMemorySafetyFamilyPlan.md`. The finding was recorded in three reports
(`Span.hpp`, `Array.hpp` as an extension, `ArraySegment.hpp` as an extension); this note is
appended to each, and **only SR-AUD-044 is closed** — every other finding in every one of
those reports is untouched.

### All twelve doors, measured before and after

One shared helper, `System::detail::copyOverlapAware`
(`modules/core/include/System/detail/OverlapCopy.hpp`, introduced by #2213), now carries
every copy. It **selects** the direction from the operands`\` relative addresses using
`std::less` — relational comparison of pointers into different objects is *unspecified* in
ISO C++, while `std::less` is required to order them totally — and copies with
`std::copy_backward` only when the destination begins strictly inside the source.

Right-overlap result for `std::string` elements, copying the first three of `{a,b,c,d}` one
place right. .NET`\`s answer is `aabc`:

| Door | Before | After |
|---|---|---|
| `Span<T>::CopyTo` | `aaaa` | `aabc` |
| `Span<T>::TryCopyTo` | `aaaa` | `aabc` |
| `ReadOnlySpan<T>::CopyTo` | `aaaa` | `aabc` |
| `ReadOnlySpan<T>::TryCopyTo` | `aaaa` | `aabc` |
| `Memory<T>::CopyTo` | `aaaa` | `aabc` |
| `Memory<T>::TryCopyTo` | `aaaa` | `aabc` |
| `ReadOnlyMemory<T>::CopyTo` / `TryCopyTo` (forward to the ReadOnlySpan doors) | `aaaa` | `aabc` |
| `MemoryExtensions::CopyTo` (both overloads) | `aaaa` | `aabc` |
| `Array::Copy(vector, idx, vector, idx, len)` and `ConstrainedCopy` | `aaaa` | `aabc` |
| `ArraySegment<T>::CopyTo(vector&, intcs)` (and the one-argument form) | `aaaa` | `aabc` |
| `ArraySegment<T>::CopyTo(ArraySegment<T>&)` | `aaaa` | `aabc` |
| `Array::Copy(const T*, …)` — the raw door, closed by #2213 | ASan `memcpy-param-overlap` | `1123` |

Logs: `build-probe/2210_before.log` and `build-probe/2216_after.log`.

### Premise corrections

1. **This finding is not sanitizer-decidable for eleven of its twelve doors.** Overlapping
   element assignment is perfectly memory-safe; it merely loses data. All eleven were run
   under AddressSanitizer **and** UndefinedBehaviorSanitizer, before and after, with **zero
   reports** either way, while a deliberate `heap-buffer-overflow` control fired in the same
   binary. The evidence is the measured values above, and this is stated rather than left
   for a reader to assume the sanitizers proved something.
2. **One door *is* sanitizer-decidable and the audit did not say so.** The raw-pointer
   `Array::Copy` uses `memcpy`, and ASan reports **`memcpy-param-overlap`** for overlapping
   `int` ranges. That door belongs to SR-AUD-051 and was closed by #2213.
3. **Left overlap was already correct** — `Array::Copy(v, 1, v, 0, 3)` produced `bcdd`,
   exactly .NET`\`s answer, before and after. A repair that unconditionally copied backward
   would have *broken* the direction that already worked, so the fix selects rather than
   reverses. A mutation confirms it.
4. **`int` cannot prove any of this.** libstdc++ lowers **both** `std::copy` and
   `std::copy_backward` over a trivially copyable type to `__builtin_memmove`, which is
   correct in either direction. Every direction assertion in the new suite therefore uses
   `std::string`; `int` appears only as a value regression. Measured in #2213`\`s mutation
   run, where an unconditionally backward copy passed every `int` case.

### Closure evidence

**+10 permanent regressions** in `modules/core/tests/System/CoreMemorySafetyTests.cpp`:
every door in both directions with `std::string`; the degenerate shapes (source ==
destination, adjacent, zero length, one element, zero-length slice at the end, nested
slices); non-overlapping copies keeping their previous results, including SR-AUD-055`\`s
deliberate `resize` behaviour; a short destination still throwing, and a `false`
`TryCopyTo` still writing nothing; an observable element type proving the assignment count
is exactly the length in **both** branches and **zero** for an identical range; and the
move/copy matrix, pinned as `static_assert(std::is_trivially_copyable_v<…>)` on all five
view types plus copy/move construction, copy/move assignment over a populated target,
moved-from reuse and destruction order — the property that makes "no operation on one view
invalidates another" true rather than merely asserted.

`SharpRuntimeTests_Core_Base` **5,627/5,627** (1 pre-existing skip), `Buffers` 618/618,
`Collections.Core` 2,763/2,763, `Text` 296/296, integration 893/893. Whole repository
builds with zero errors and zero warnings.

### Five mutations, five killed

M1 always forward (restore the defect) → 9 fail. M2 always backward (the over-correction
premise correction 3 warns about) → 8 fail. M3 invert the overlap test → 9 fail. M4 drop
the `source == destination` short circuit → the assignment-count test fails (4 assignments
where 0 are required). M5 **skip exactly one door** (revert `ArraySegment::CopyTo(
ArraySegment&)` to a forward `std::copy`) → only that door`\`s test fails, proving each door
is covered separately rather than by one shared assertion.

### Source, ABI and layout consequences

**None.** Every changed body is an inline member or a function template; no data member,
`sizeof`, `alignof`, virtual function, `noexcept` specification, default argument or
mangled symbol changed. `TryCopyTo` keeps its `noexcept` and its no-write-on-`false` rule.
The new `System/detail/OverlapCopy.hpp` is inside the `Core.Base` module, so no component
boundary or dependency edge moved.

