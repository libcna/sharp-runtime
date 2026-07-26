# Audit: `modules/buffers/include/System/Buffers/Binary/BinaryPrimitives.hpp`

## Metadata

- Audit status: AUDITED (643-line public header-only implementation, fully
  read).
- Validation: `BinaryPrimitivesTests.*` passed 14/14 within the complete 63/63
  `Batch6BuffersTests.cpp` focused filter on 2026-07-26. The nine
  reverse-endianness tests in the pending Batch17 fixture are supporting
  evidence only, not a completed file-wide test audit.
- Reference: local .NET `BinaryPrimitives.ReadLittleEndian.cs`,
  `ReadBigEndian.cs`, `WriteLittleEndian.cs`, `WriteBigEndian.cs`, and
  `ReverseEndianness.cs` were reviewed.

## Assessment

The audited integer, floating-point, and 128-bit paths consistently validate
the public span length before `memcpy`, use byte-copy rather than unsafe
reinterpret casts, and use correctly paired host/endian conversion helpers.
Throwing methods use the expected system exception, while `TryRead` resets its
output before false. No focused evidence established an implementation defect
in the currently compiled GCC/Clang surface.

## Other missing assertions and diagnostics

- The direct fixture omits every `TryRead`/`TryWrite` result-and-output path,
  including destination nonmutation on false, negative view lengths, exact
  versus oversized buffers, and all big-endian false boundaries.
- Floating-point cases omit `-0`, subnormal, infinity, both NaN signs/payloads,
  and byte-exact round trips. They therefore do not independently establish
  that the `memcpy` bit paths preserve all IEEE payloads.
- There is no 128-bit read/write/reverse-endian known-vector or short-span
  test, even though these overloads are compiled on GCC/Clang.
- The public 128-bit overloads are intentionally excluded on MSVC because the
  local `Int128`/`UInt128` wrappers require compiler `__int128`. Current .NET
  exposes the APIs on supported Windows runtimes; classify this only after an
  explicit project MSVC/API-baseline decision, not as an unverified defect.
- No big-endian CI target exercises the alternate helper branch. The tests use
  expected byte vectors, but every run here executes the little-endian host
  implementation.

## Final assessment

Within the locally supported native toolchain, BinaryPrimitives has sound
length and byte-order handling. Test breadth and the MSVC 128-bit public API
decision remain the material follow-up evidence gaps. No source or test was
modified during this audit.
