# Audit: `modules/buffers/include/System/Buffers/IPinnable.hpp`

## Metadata

- Audit status: AUDITED (52-line public abstract header, fully read).
- Validation: `IPinnableTests.*` passed 2/2 within the complete 37/37
  `Batch16BuffersTests.cpp` focused filter in `SharpRuntimeTests_Buffers` on
  2026-07-26.
- Reference: local .NET
  `src/libraries/System.Private.CoreLib/src/System/Buffers/IPinnable.cs` and
  `MemoryHandle.cs` were reviewed.

## Assessment

The two virtual members and `MemoryHandle::Dispose` bridge preserve the basic
manual-unpin relationship. Explaining that a moving GC is absent is the right
C++ adaptation. The header's deferred inline Dispose definition is safe with
respect to complete type information and clears both handle pointer and
pinnable callback after one disposal.

## Other missing assertions and diagnostics

- No test checks element-index validation, multiple outstanding handles,
  double Dispose (and exactly-once Unpin), manager destruction before handle
  disposal, or a null/invalid native pointer.
- The Batch16 `StubPinnable` returns a fabricated pointer and only verifies a
  boolean after explicit Dispose; it does not establish RAII destruction,
  ordering, thread safety, or actual storage stability.
- The interface does not state the C++ replacement for GC pinning when a
  vector reallocates. A raw pointer can still become invalid despite Pin being
  a native no-op, so owners need a clear no-reallocation/lifetime contract.

## Final assessment

The abstract compatibility surface is coherent for manual native lifetime,
with no standalone evidence-backed defect. Concrete manager/pool pinning must
eventually state reallocation and ownership guarantees. No source or test was
modified during this audit.
