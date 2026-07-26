# Audit: `modules/buffers/include/System/Buffers/Binary/BinaryPrimitives.hpp`

## Metadata

- Audit status: AUDITED (643-line public header-only implementation, fully
  read).
- Validation: the dedicated `BinaryPrimitivesTest.*` filter passed 37/37;
  together with the direct ArrayBufferWriter and enumerator fixtures, the
  combined Buffers filter passed 54/54 in `SharpRuntimeTests_Buffers` on
  2026-07-26.  Earlier `Batch6BuffersTests.cpp` evidence passed 63/63.
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

- The direct fixture covers selected `TryRead`/`TryWrite` paths, but omits
  destination nonmutation on false, negative view lengths, exact versus
  oversized buffers, and most big-endian false boundaries.
- Floating-point cases omit `-0`, subnormal, infinity, both NaN signs/payloads,
  and byte-exact round trips. They therefore do not independently establish
  that the `memcpy` bit paths preserve all IEEE payloads.
- The new 128-bit tests provide basic known layout, round-trip, and selected
  short-span evidence, but omit negative signed values, all big-endian false
  paths, byte-exact multiword vectors, and every 128-bit `TryRead` direction.
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
