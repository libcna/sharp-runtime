# Audit: `modules/core/tests/System/BufferTests.cpp`

## Metadata

- Audit status: AUDITED (333 lines, 38 tests, fully read).
- Validation: `BufferTests.*` passed 38/38 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The suite exercises raw and vector normal copies, vector range diagnostics,
primitive byte access, overlap, MemoryCopy capacity errors, and a useful set
of regression checks for the previously repaired vector bounds path. It clearly
distinguishes raw and vector overloads in name, but only the checked vector
overloads receive invalid-input testing.

## Finding references

- **SR-AUD-067:** raw-pointer negative `srcOffset`, `dstOffset`, and `count`
  are absent. An ASan probe shows `count=-1` reaches `memmove` as a
  negative-size parameter rather than throwing an argument exception.
- **SR-AUD-051 (extended):** typed-vector tests instantiate only primitive
  `int32_t`; no test proves the documented trivially-copyable restriction is
  enforced. `std::vector<std::string>` byte copying is accepted and reaches
  ASan-confirmed double-free at destruction.

## Other missing assertions and diagnostics

- Raw pointer tests omit source/destination null, negative offsets, short
  capacity, large count, and zero-count-with-null-pointer boundaries.
- `MemoryCopy` tests omit negative destination size, unsigned overload,
  null/overlap variants for both signatures, and source extent (which cannot
  be derived from raw pointers).
- Byte order is not portable: `GetByte_LittleEndian_FirstByteOfInt32` combines
  byte positions in a little-endian order and will not validate its intended
  property on a big-endian target.
- No test covers ByteLength/GetByte/SetByte with unsupported nontrivial T,
  huge-size narrowing, or adjacent byte writes inside multibyte primitives.

## Final assessment

The focused suite validates repaired checked-vector behavior but leaves the
raw metadata and generic nontrivial-type defects unexercised. No test was
modified during this audit.
