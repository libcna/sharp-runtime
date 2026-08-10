# Audit: `modules/core/include/System/Span.hpp`

## Metadata

- Audit status: AUDITED (483-line combined `Span<T>`/`ReadOnlySpan<T>` header,
  fully read).
- Validation: `ReadOnlySpanTests.*:SpanTests.*` passed 25/25 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Independent probes: `/tmp/sharp-runtimervc-hashcode-audit-probe.cpp` and
  `/tmp/sharp-runtimervc-span-overlap-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-25.
  LeakSanitizer is disabled only because it cannot run under the sandbox tracer.

## Assessment

The recent unsigned range checks correctly close the prior `Slice(start,
length)` addition-overflow bypass and ordinary index/slice/copy cases pass.
Two foundational public contracts remain broken.  Both span constructors accept
a negative `intcs` length without validation, exposing invalid metadata to every
consumer; and all four copy paths use `std::copy`, which does not implement the
overlap-safe `Span` contract for general C++ values.

References: [ReadOnlySpan pointer/length constructor contract](https://learn.microsoft.com/en-us/dotnet/api/system.readonlyspan-1.-ctor?view=net-10.0),
[Span.CopyTo overlap contract](https://learn.microsoft.com/en-us/dotnet/api/system.span-1.copyto?view=net-10.0),
and [ReadOnlySpan.CopyTo overlap contract](https://learn.microsoft.com/en-us/dotnet/api/system.readonlyspan-1.copyto?view=net-10.0).

## Finding references

### SR-AUD-043 — high — Span constructors allow negative length metadata that later becomes an unbounded read

`Span<T>(pointer, -1)` and `ReadOnlySpan<T>(pointer, -1)` store the negative
length unchanged.  The .NET pointer/length constructor throws
`ArgumentOutOfRangeException` for a negative length.  Locally, this invalid
public state reaches pointer arithmetic, range construction, string creation,
and consumers such as `HashCode::AddBytes`, which casts it to `size_t`.
The linked HashCode ASan probe constructs a one-byte `ReadOnlySpan<uint8_t>`
with `-1` and reports a stack-buffer-overflow on the second read.  Vector
constructors have the same latent state transition when a `size_t` size exceeds
the 32-bit `intcs` range and narrows negative.

### SR-AUD-044 — high — CopyTo and TryCopyTo corrupt overlapping nontrivial spans

`Span<T>::CopyTo`, `Span<T>::TryCopyTo`, `ReadOnlySpan<T>::CopyTo`, and
`ReadOnlySpan<T>::TryCopyTo` all call forward `std::copy`.  .NET requires the
entire source to be copied even when source and destination overlap.  The
independent probe moves the first three elements of
`{"a","b","c","d"}` one position right through `Span<std::string>`;
the local result is `{"a","a","a","a"}` rather than
`{"a","a","b","c"}`.  `int` happens to work under this libstdc++ build
because an implementation optimization behaves like `memmove`, masking the
generic defect rather than satisfying the contract.  The independently audited
static `MemoryExtensions::CopyTo` repeats the same forward-copy error and
reproduces `aaaa` for an overlapping nontrivial range.  `Memory::CopyTo` and
`Memory::TryCopyTo` also invoke forward `std::copy`; their direct probe has the
same result, while ReadOnlyMemory forwards to the affected ReadOnlySpan paths.
`Array::Copy`'s vector overload independently repeats the same forward
assignment error: a rightward `std::string` aliasing copy changes `abcd` to
`aaaa`.  Both `ArraySegment::CopyTo` overloads have the same result for
overlapping nontrivial same-vector segments.

## Required post-audit verification

Validate pointer/length constructors before storing metadata, reject negative
lengths, and prevent `vector::size()` narrowing beyond `intcs::max()`.  Audit
all existing span consumers for length-to-unsigned conversions; SR-AUD-043 is
already ASan-confirmed in `HashCode`.

Replace forward copying with overlap-aware direction selection (`std::copy` or
`std::copy_backward`) or a temporary, preserving `TryCopyTo`'s no-write rule
when the destination is short.  Add tests for left/right overlap with `int`,
`std::string`, and a custom observable copy type across all four copy methods.
Run malformed-length and overlapping tests under ASan/UBSan.

## Other missing assertions and diagnostics

- No test rejects negative pointer/length construction for either mutable or
  read-only spans, or covers an oversized vector-to-`intcs` conversion.
- The dedicated suite tests only non-overlapping integer copies; it misses the
  nontrivial type path that exposes SR-AUD-044.
- Default empty spans rely on pointer arithmetic/range construction with a null
  pointer in `end()` and `ToArray()`; these common cases pass locally but lack
  a documented C++ precondition or explicit empty fast path.
- Raw pointer constructors and accessors do not state the non-null requirement
  for nonzero lengths, making it difficult for consumers to distinguish native
  ownership preconditions from managed-shaped validation.

## Final assessment

The range-slice repair is sound, but this foundational header still permits
malformed length state to reach memory operations and silently corrupts
overlapping nontrivial copies.  No implementation was modified during this
audit.

### Partially remediated (SR-AUD-043a) — ticket #1852 (2026-07-30)

`Span(T*, intcs)` and `ReadOnlySpan(const T*, intcs)` now throw
`ArgumentOutOfRangeException("length")` when `length < 0`, and the
`Span`/`ReadOnlySpan` vector constructors route `v.size()` through the shared
`System::detail::checkedSpanLength` (`System/detail/SpanLength.hpp`), which throws
`ArgumentOutOfRangeException` when the source holds more than `INT32_MAX`
elements instead of silently narrowing to a negative length. The `length_` field
stays signed `intcs` and every signature is unchanged (no layout/ABI change) —
matching .NET, which keeps `_length` a signed `int` and validates at
construction. This closes the **reachable** SR-AUD-043 exploit: a negative-length
span can no longer be constructed, so it can never reach `HashCode::AddBytes`.
ASan reproduced the pre-fix `heap-buffer-overflow READ` at `HashCode.hpp:76`
(`build-probe/1852_span_hashcode_prefix.log`) and confirmed a clean
construction-time throw post-fix (`…_postfix.log`). The **SR-AUD-043b** tail (the
`ReadOnlyMemory` `noexcept`/`constexpr` ctors and `HashCode::AddBytes noexcept`)
stays open as approval-gated ticket #1854. SR-AUD-044 (overlap copy) is untouched
and remains open. `docs/ConversionBoundaryFamilyPlan.md` §19.3.

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

