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
