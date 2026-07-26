# Audit: `modules/core/include/System/BitConverter.hpp`

## Metadata

- Audit status: AUDITED (259 lines, header-only conversions fully read).
- Validation: `BitConverterTests.*` passed 67/67 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Sanitizer probe: `/tmp/sharp-runtimervc-bitconverter-audit-probe.cpp`, built
  with `-fsanitize=address -fno-omit-frame-pointer` on 2026-07-25.

## Assessment

The byte-layout and bit-reinterpretation helpers use `memcpy` safely for
aligned/aliasing-independent normal reads, and the separately implemented hex
formatter has recently gained bounds validation.  In contrast, every typed
vector `To*` overload passes its unchecked data pointer and signed index
straight to the raw read.  A caller can therefore request a negative index or
fewer bytes than the target width and cause undefined memory reads instead of
the .NET exception contract.

## Finding references

- **SR-AUD-041:** `ToInt32(vector{0x78,0x56,0x34,0x12}, -1)` and
  `ToInt32(vector{0x78,0x56,0x34}, 0)` both reach ASan-confirmed
  heap-buffer-overflow reads (one byte before and one byte after the vector,
  respectively).  The same unchecked forwarding pattern exists for Boolean,
  character, integer, floating, Half/BFloat16, and 128-bit vector decoders.
  .NET validates `startIndex` and the required width; `ToInt32` documents
  `ArgumentOutOfRangeException` for a negative index and `ArgumentException`
  for insufficient bytes:
  <https://learn.microsoft.com/en-us/dotnet/api/system.bitconverter.toint32?view=netframework-4.8.1>.

## Required post-audit verification

Centralize vector index/remaining-width validation before any pointer
arithmetic, using checked nonnegative arithmetic and type-specific byte widths.
Match the project's selected .NET adaptation for the negative-index and
insufficient-buffer exception classes, then add negative, `size`, and
`size - width + 1` vectors for every width family.  Preserve raw-pointer
overloads only with an explicit documented precondition, or validate their
nonnegative index where possible.

## Other missing assertions and diagnostics

- The 67 tests cover vector round-trips only at index zero with exactly enough
  bytes; none tests a negative or end index, a short vector, or 16-byte input.
- `ToString` has strong negative/bounds regression tests, but no equivalent
  validation is exercised for the typed decoders next to it.

## Final assessment

The normal byte conversions work, but a broad public malformed-input surface
has sanitizer-confirmed memory-safety failures.  No implementation was
modified during this audit.
