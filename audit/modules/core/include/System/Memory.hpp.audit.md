# Audit: `modules/core/include/System/Memory.hpp`

## Metadata

- Audit status: AUDITED (308-line header-only implementation, fully read).
- Validation: `MemoryTests.*` passed 49/49 in `SharpRuntimeTests_Buffers` on
  2026-07-25.
- Independent probe: `/tmp/sharp-runtimervc-memory-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-25.

## Assessment

The recent unsigned slice and subrange checks correctly avoid the former
addition-overflow bypass, and ordinary capacity failures are rejected before
copying.  Two foundational span findings extend here: full-vector construction
narrows `size_t` to `intcs`, and both copy operations use forward `std::copy`
on potentially overlapping views of the same vector.

Reference: [.NET `Memory<T>.CopyTo` contract](https://learn.microsoft.com/en-us/dotnet/api/system.memory-1.copyto?view=net-10.0).

## Finding references

- **SR-AUD-043 (extended):** `Memory(std::vector<T>&)` casts `array.size()` to
  signed 32-bit `intcs` without guarding values above `intcs::max()`.  That
  creates the same invalid negative metadata accepted by `Span` and propagated
  by the conversion operator to `ReadOnlyMemory`; its subsequent `Span`,
  `ToArray`, and pointer arithmetic paths are unsafe.
- **SR-AUD-044 (extended):** `CopyTo` and `TryCopyTo` (lines 178–209) use
  forward `std::copy`.  The sanitizer probe copies three overlapping nontrivial
  `Cell` values from a `Memory` slice at offset zero to one at offset one and
  changes `abcd` to `aaaa`, rather than the required overlap-safe `aabc`.
  .NET explicitly preserves all source contents when `Memory<T>` regions
  overlap.

## Required post-audit verification

Reject vector sizes above `intcs::max()` before storing the length, coordinated
with the `Span`/`ReadOnlyMemory` SR-AUD-043 repair.  Replace forward copy with
an overlap-aware operation or a temporary and test both directions with an
observable nontrivial type.  Retain `TryCopyTo`'s existing no-write behavior on
a short destination.

## Other missing assertions and diagnostics

- No test constructs overlapping `Memory` slices or tests nontrivial element
  assignment; all copy tests use separate `int` vectors.
- No test covers a vector whose size cannot be represented by `intcs`, empty
  vector `Pin`, or vector reallocation after `Memory`/`MemoryHandle` creation.
- The non-owning vector lifetime and reallocation precondition is documented
  for `Pin` but not consistently on constructors, `getSpanProperty`, `ToArray`,
  or conversion to `ReadOnlyMemory`.

## Final assessment

Normal vector-backed memory behavior passes focused tests, but it inherits the
negative-length representation risk and has a probe-confirmed overlap-copy
corruption path.  No implementation was modified during this audit.
