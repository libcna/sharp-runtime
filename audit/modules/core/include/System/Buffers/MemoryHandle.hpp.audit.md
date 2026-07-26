# Audit: `modules/core/include/System/Buffers/MemoryHandle.hpp`

## Metadata

- Audit status: AUDITED (48-line public declaration; deferred `Dispose` body in
  `IPinnable.hpp` reviewed).
- Validation: `MemoryHandleTests.*` passed 3/3 within the complete 37/37
  Batch16 focused filter on 2026-07-26.
- Reference: local .NET `System/Buffers/MemoryHandle.cs` was reviewed.

## Assessment

The handle stores a pointer/pinner and explicit `Dispose` clears both after one
`Unpin`, matching the manual IDisposable path.  The C++ documentation however
claims callers may let the destructor perform deterministic RAII cleanup.  No
user-defined destructor exists, so ordinary scope exit never calls `Dispose`.

## SR-AUD-088 — medium — MemoryHandle documents RAII cleanup but scope exit never unpins

`MemoryHandle` has only a defaulted implicit destructor.  Its deferred
`Dispose` implementation in `IPinnable.hpp` calls `pinnable_->Unpin()` only
when the caller explicitly invokes `Dispose`.  Consequently a handle returned
from `IPinnable::Pin` that leaves scope without an explicit call silently
retains the pin/resource state, contrary to this header's “or let the
destructor do it” public guidance.

The direct fixture proves only explicit `Dispose_ClearsPointer` and explicit
`DisposeHandle_CallsUnpin`; it has no scoped-handle assertion.  A minimal
`IPinnable` counter stub shows `MemoryHandle handle(pointer, &stub);` leaves
the counter at zero after the enclosing scope, while an explicit `Dispose`
increments it.  Current .NET requires explicit IDisposable disposal; C++ may
choose that adaptation, but then the RAII/destructor claim must not invite a
leak-prone call pattern.

## Other missing assertions and diagnostics

- No scoped cleanup, move/copy, double Dispose, exception-from-Unpin, pinner
  lifetime, or dangling pointer test exists.
- The public raw `void*` and raw non-owning `IPinnable*` give no ownership or
  thread-safety diagnostics.  Pin counters and nested handles need explicit
  policy before native resource-backed implementations are added.
- `MemoryHandleTests` uses a simple pointer and never verifies unpin on
  destructor, pinner state, or a handle copied/moved across scopes.

## Final assessment

Explicit disposal works, but the documented RAII alternative does not exist.
No production or test source was modified during this audit.
