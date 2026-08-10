# Audit: `modules/runtime/include/System/Runtime/InteropServices/NativeMemory.hpp`

## Metadata

- AUDITED: 234-line inline allocation implementation, fully read.
- Validation: `NativeMemoryTests.*` passed 20/20 on 2026-07-27.
- Reference basis: local current `NativeMemory.cs` and POSIX allocation paths.

## Assessment

The reviewed Alloc multiplication guard, zero allocation adaptation, overlap
copy, alignment validation, and POSIX aligned-reallocation header are coherent.
In particular, POSIX AlignedRealloc copies the lesser of the stored old adjusted
size and new size, avoiding the earlier new-size read beyond the old block. No
standalone defect was reproduced.

## Other missing assertions and diagnostics

- Missing AllocZeroed element-count overflow, Realloc shrink/zero-size, and
  allocator-failure paths; platform `calloc` overflow behavior is not a stable
  substitute for explicit contract evidence.
- Missing aligned zero-byte, alignment smaller than pointer size, changed
  alignment during reallocation, and near-`size_t` round-up overflow vectors.
- Clear/Copy/Fill null-plus-zero and null-plus-positive contracts are not
  documented/tested separately; the managed positive-null behavior is
  intentionally undefined.

## Final assessment

Normal raw allocation and the reviewed POSIX alignment repair pass focused
evidence. No source or test was modified during this audit.
