# Audit: `modules/buffers/include/System/Buffers/MemoryPool.hpp`

## Metadata

- Audit status: AUDITED (100-line public header-only implementation, fully
  read).
- Validation: `MemoryPoolTests.*` passed 11/11, as part of the complete 63/63
  `Batch6BuffersTests.cpp` focused filter in `SharpRuntimeTests_Buffers` on
  2026-07-26.
- ASan reproducer: `/tmp/sharp-runtimervc-memorypool-dispose-probe.cpp`, built
  with `-fsanitize=address` and `build/libsharp_runtime_core.a`, obtains a
  `Memory<int>`, disposes its owner, then indexes the saved memory. ASan reports
  a null read / `DEADLYSIGNAL` in the access.
- Reference: local .NET runtime `MemoryPool.cs`, `ArrayMemoryPool.cs`, and
  `ArrayMemoryPool.ArrayMemoryPoolBuffer.cs`, plus the local MemoryPool test
  suite, were reviewed.

## Assessment

The shared instance uses thread-safe C++ local-static initialization, validates
the documented `-1`/negative/maximum rent boundary, and corrects the default
element count for `sizeof(T)`. Its heap owner has no terminal disposed state,
which both diverges from the source API and makes retained C++ memory views
unsafe after a normal public lifecycle operation.

## SR-AUD-071 — high — MemoryPool owner permits post-dispose access and invalidates retained Memory into a native fault

`MemoryPoolHeapOwner_::Dispose` clears and shrinks its vector, but
`getMemoryProperty` has no disposed check and returns a zero-length `Memory`
after disposal. Current .NET clears its backing-array field and the `Memory`
getter explicitly throws `ObjectDisposedException` whenever the owner is
disposed. The current direct test locks in `getMemoryProperty().Length == 0`
after disposal instead of the required exception.

The problem is worse for a view retrieved before disposal. `Memory<T>` stores
the vector address and original length. After `clear`/`shrink_to_fit`, its
`getSpanProperty()` recreates a span from the now-null data pointer with that
old nonzero length. The ASan probe writes before disposal, disposes the owner,
then indexes the saved view; it reports a native null-read crash rather than a
managed disposal diagnostic.

## Finding references

- **SR-AUD-070 (extended):** `MemoryPoolHeapOwner_` also constructs its
  vector with a size, so it has the same undocumented default-constructible
  `T` requirement independently confirmed for `ArrayBufferWriter<T>`.

## Other missing assertions and diagnostics

- The direct test checks that an owner becomes empty after `Dispose`, but never
  requests Memory again expecting `ObjectDisposedException`, reads a view
  retained before disposal, or tests no access after destruction.
- No test verifies repeated disposal's terminal-state consistency, independent
  live rents, shared-pool concurrent renting, or the documented shared-pool
  dispose no-op exposed by .NET's `MemoryPool<T>::Dispose` API.
- `std::vector` allocations can leak native `length_error`/`bad_alloc` and
  require default construction; allocation-size and exception taxonomy are not
  translated to the documented system exception surface.
- The implementation says it is a pool but deliberately does not reuse a
  returned vector. That policy is documented locally, yet no allocation/reuse
  metric or lifecycle diagnostic distinguishes it from an ordinary allocator.

## Final assessment

Input size validation is sound, but the owner fails the essential post-dispose
contract and turns retained-memory use into an ASan-confirmed native fault.
No source or test was modified during this audit.

## Design closure for SR-AUD-071 (ticket #2056, 2026-08-04): DESIGN-COMPLETE — NOT REMEDIATED

The audit evidence above is retained unchanged. SR-AUD-071 stays **open**, now marked
`confirmed (design-complete)`. The owning review is
[`docs/BuffersNamespaceReviewPlan.md`](../../../../../../docs/BuffersNamespaceReviewPlan.md)
(ticket #2048) §4.3; **no `SR-AUD-*` identifier was issued.**

Both halves reproduced against the shipped body: `Rent(16)` → `Memory` length 16;
`Dispose()`; `getMemoryProperty()` returns length **0** without throwing; and the `Memory`
obtained *before* disposal still reports length **16** over storage `shrink_to_fit` released.

**One premise corrected: this is one finding but two defects, with different blast radii, and
conflating them would have cost something real.**

- **(a) the post-dispose getter.** .NET's `ArrayMemoryPoolBuffer.Memory` throws
  `ObjectDisposedException`. Reproducing that needs a terminal flag, because a **live**
  `Rent(0)` and a **disposed** owner are indistinguishable from the vector alone — both empty,
  both zero capacity — and that indistinguishability is itself pinned by
  `MemoryOwnerDisposedPinTests.ZeroLengthRentIsIndistinguishableFromADisposedOwner`. Measured,
  `sizeof(MemoryPoolHeapOwner_<int>)` is **32** (vptr 8 + `std::vector` 24), `alignof` 8, with
  **no padding to reuse**; a `bool` takes it to **40**. That is an object-layout change to a
  class template in a public header, plus a semantic change from "returns empty" to "throws".
- **(b) the retained view.** `System::Memory<T>` stores a pointer and a length with no owner
  liveness. Repairing that is a `Memory<T>` ownership change in `Core.Base` — CCF-019's shape
  — with a blast radius far outside this module, and it is deliberately not scoped here.

**Blocked ticket #2056** carries both. Nothing about it is approved.

**What did land (#2061, doc-only, zero executable change):** `IMemoryOwner<T>` now documents
both consequences at the point a caller meets them, including the explicit instruction not to
retain a `Memory<T>` past its owner's `Dispose()`. Five pins guard the current behaviour and
were mutation-checked — making the getter throw fails three of them, while the retained-view
pin stays green, which is what shows the mutation was targeted rather than indiscriminate.

### SR-AUD-070's site in this file is remediated (#2054, 2026-08-04)

The *"SR-AUD-070 (extended)"* note in this report — `MemoryPoolHeapOwner_` constructs its
`std::vector<T>` sized, so `T` must be default-constructible — is now stated in
`MemoryPool<T>`'s own Doxygen block and `static_assert`ed in the owner's constructor body.

One measured correction to how that requirement presents: `DefaultMemoryPool_<T>::Rent` is
`virtual`, so its body — and through it the owner's constructor — is instantiated for the
**vtable**. The requirement therefore bites when the shared pool is instantiated, not only
when `Rent` is called. Naming `MemoryPool<NoDefault>` and taking its `sizeof` stay legal,
before and after, so the assert is in the constructor body rather than at class scope. A
custom subclass backed by storage that does not value-initialize is free of the requirement,
and the header says so. SR-AUD-070's full remediation record is in the `ArrayBufferWriter.hpp`
report. This does **not** touch SR-AUD-071 or blocked ticket #2056.
