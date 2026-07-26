# Audit: `modules/buffers/include/System/Buffers/ArrayBufferWriter.hpp`

## Metadata

- Audit status: AUDITED (160-line public header-only implementation, fully
  read).
- Validation: `ArrayBufferWriterTests.*` passed 10/10, as part of the complete
  63/63 `Batch6BuffersTests.cpp` focused filter in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
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

- The complete direct batch only instantiates `uint8_t` and `int`; it omits a
  non-default-constructible, move-only, reference-owning, throwing-copy, and
  oversized element type.
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
