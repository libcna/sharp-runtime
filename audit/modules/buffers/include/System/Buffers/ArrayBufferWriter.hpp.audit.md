# Audit: `modules/buffers/include/System/Buffers/ArrayBufferWriter.hpp`

## Metadata

- Audit status: AUDITED (160-line public header-only implementation, fully
  read).
- Validation: the dedicated `ArrayBufferWriterTest.*` filter passed 13/13;
  together with the direct BinaryPrimitives and enumerator fixtures, the
  combined Buffers filter passed 54/54 in `SharpRuntimeTests_Buffers` on
  2026-07-26.  Earlier `Batch6BuffersTests.cpp` evidence passed 63/63.
- Reproducer: compiling
  `/tmp/sharp-runtimervc-arraybufferwriter-nondefault-probe.cpp` with C++20
  fails at `GetSpan(1)`: `std::vector::resize` requires
  `NonDefault::NonDefault()`. The probe's type has only
  `explicit NonDefault(int)`.
- Reference: local .NET runtime
  `src/libraries/Common/src/System/Buffers/ArrayBufferWriter.cs` and the
  generic `System.Memory` ArrayBufferWriter tests were reviewed.

## Assessment

For default-constructible primitive values, the implementation follows the
source growth shape, rejects negative hints/counts, tracks written capacity,
and exposes non-empty free memory after default growth. The `std::vector`
backing, however, silently adds a generic constraint that is neither present
in the documented public type nor compatible with .NET's unconstrained `T`.

## SR-AUD-070 — medium — ArrayBufferWriter<T> silently requires a default-constructible T

Every initial allocation and growth calls `std::vector<T>::resize`, and
`Clear` writes `T{}`. Therefore a perfectly valid C++ element type without a
default constructor cannot use `GetMemory` or `GetSpan`; the compiler errors
inside the standard library rather than at a documented public requirement.
The standalone reproducer reaches the failing resize from `GetSpan(1)` and
reports no matching `NonDefault()` constructor.

The local .NET source declares no constructor constraint on `T`: its `T[]`
backing has the runtime default value for every generic type. A C++ mapping may
need an allocation/storage adaptation, but it must either support the declared
generic surface or make the restriction explicit and provide a clear public
diagnostic before users select the type.

## Other missing assertions and diagnostics

- The direct and mixed fixtures instantiate only scalar default-constructible
  values.  They omit a non-default-constructible, move-only,
  reference-owning, throwing-copy, and oversized element type.
- No test checks zero-hint `GetMemory`/`GetSpan`, invalid negative hints,
  advance beyond free capacity after a partial write, or the source guarantee
  that old views must not be used after `Advance`/growth.
- `currentLength + growBy` is signed `intcs` arithmetic before capacity
  handling, whereas .NET checks against `ArrayMaxLength` before allocation.
  Huge capacity/size-hint behavior, overflow avoidance, and native
  `length_error`/`bad_alloc` to `OutOfMemoryException` taxonomy are untested.
- `ReadOnlyMemory`/`ReadOnlySpan` views retain a raw/vector reference across
  resize. The source contract invalidates prior views for writing, but this C++
  API offers no stale-view diagnostic; dereferencing one after vector
  reallocation is native undefined behavior.

## Final assessment

The primitive happy path is sound under its preconditions, but the generic
public type has an undocumented compile-time exclusion and unverified
large-allocation/old-view behavior. No source or test was modified during this
audit.

## Post-audit record (tickets #2051 and #2054, 2026-08-04)

The audit evidence above is retained unchanged. The owning review is
[`docs/BuffersNamespaceReviewPlan.md`](../../../../../../docs/BuffersNamespaceReviewPlan.md)
(ticket #2048); **no `SR-AUD-*` identifier was issued and numbering stays frozen at 364.**

**A defect this report filed only as an untested area turned out to be reachable undefined
behaviour, and was repaired (#2051).** The *"Other missing assertions"* section says
*"`currentLength + growBy` is signed `intcs` arithmetic before capacity handling"*. Measured,
that is not merely untested: an `ArrayBufferWriter<char>` of capacity **1** and a single
`GetSpan(2147483647)` reached

```
ArrayBufferWriter.hpp:42:71: runtime error: signed integer overflow:
    1 + 2147483647 cannot be represented in type 'int'
terminate called after throwing an instance of 'std::length_error'
```

— UBSan-confirmed UB requiring no large allocation, followed by a **native** exception
escaping a public door. .NET performs the same addition, but C#'s unchecked `int` **wraps by
definition** and .NET catches the wrap immediately with `if ((uint)newSize > MaxArrayLength)`;
in C++ the wrap is undefined, so the idiom cannot be ported as written. The growth total is
now computed in `longcs` and compared against a private `MaxArrayLength = 0x7FFFFFC7`
(.NET's own private constant, `Array.MaxLength`), throwing `System::OutOfMemoryException`
when even `writtenCount + sizeHint` exceeds it. The throw precedes the `resize`, so capacity
and written count survive the failure unchanged — asserted, not assumed. CCF-004.

Closure evidence: **7 permanent regressions**, including one that writes two bytes, provokes
the failure and then re-reads the written span. UBSan report present before and absent after,
with the header compiled from source into both probe builds. Source and ABI consequences:
none — `MaxArrayLength` is private and `static constexpr`, so it is neither public surface
nor part of the layout; `sizeof(ArrayBufferWriter<char>)` stays **40**, now `static_assert`ed.

**SR-AUD-070 remains `confirmed`** and is ticket **#2054** (`todo`, compatible, not yet
implemented). The review counted the sites independently and found **four** production sites,
not the two this report names: `checkAndResizeBuffer`'s `resize`, `Clear`'s `T{}`,
`MemoryPoolHeapOwner_`'s sized-vector constructor, and `SharedArrayPool<T>::Rent` plus
`ArrayPool<T>::Return(clearArray=true)` — the last of which appears here only as a
cross-reference note in the `ArrayPool` report. The planned repair states each requirement in
the doc-comment and adds a `static_assert` **at the point where the requirement is already
enforced**, so exactly the same set of programs continues to compile; a class-scope assert is
explicitly rejected because it would reject a mere declaration that compiles today.
