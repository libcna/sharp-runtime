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
