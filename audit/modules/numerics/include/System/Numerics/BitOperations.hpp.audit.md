# Audit: `modules/numerics/include/System/Numerics/BitOperations.hpp`

## Metadata

- Audit status: AUDITED (113-line header-only implementation, fully read).
- Validation: `BitOperationsTests.*` passed 13/13 in
  `SharpRuntimeTests_Numerics` on 2026-07-25.
- Independent probe: `/tmp/sharp-runtimervc-bitoperations-audit-probe.cpp`,
  built with `-fsanitize=undefined,address` on 2026-07-25.  LeakSanitizer is
  unavailable under the sandbox tracer, so the run used
  `ASAN_OPTIONS=detect_leaks=0`; it exhaustively checked every 16-bit input
  against C++20 `<bit>` and a reference bit reversal, then checked 100,000
  pseudo-random 32/64-bit values including negative and width-crossing rotation
  offsets.

## Assessment

The implemented operations are compact, defined C++20 mappings.  Unsigned
wraparound makes both `RoundUpToPowerOf2(0)` and overflow return zero as
documented, `std::countl_zero`/`countr_zero` retain their specified zero
counts, and `std::rotl`/`rotr` correctly normalize arbitrary signed offsets.
The independent probe found no mismatch or sanitizer diagnostic in the tested
surface.

The public surface is nonetheless a partial adaptation of .NET's
`System.Numerics.BitOperations`.  The project provides no `Crc32C` overloads,
although the current .NET source and API listing expose byte, UInt16, UInt32,
and UInt64 variants.  It also lacks an exact `TrailingZeroCount(longcs)`
overload: ordinary calls convert to `ulongcs` and retain the same bit result,
but code taking the exact overload cannot bind to it.  The probe confirms the
second fact with `exact_int64_tzc=0`.  The repository does not state which
.NET API baseline or deliberate omission policy applies, so this is recorded
as an adaptation-scope decision rather than a confirmed runtime defect.

References: [current .NET BitOperations source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/Numerics/BitOperations.cs.html)
and [current API member list](https://learn.microsoft.com/en-us/dotnet/api/system.numerics.bitoperations?view=net-10.0).

## Finding references

No confirmed implementation finding.  The missing `Crc32C` members and exact
signed-64 trailing-zero overload need an explicit compatibility-baseline
decision before they can be classified as an omission defect.

## Required post-audit verification

Define the supported .NET API baseline for Numerics.  If it includes the
current `Crc32C` methods, add portable software fallback tests with known CRC-32C
vectors and test every byte/16/32/64 overload.  Add a compile-time signature
test for `TrailingZeroCount(longcs)`, then exercise zero, `MinValue`, `-1`, and
the 64th-bit cases.  Keep a defined unsigned implementation for signed input;
the present conversion has the right trailing-zero bit semantics.

## Other missing assertions and diagnostics

- The checked-in tests cover only 32-bit values despite every core operation
  except `ReverseBits` also accepting 64-bit input.
- They omit `RoundUpToPowerOf2` overflow boundaries (`0x80000001` and
  `0x8000000000000001`), 64-bit zero counts, all negative/width-crossing
  rotation offsets, signed `TrailingZeroCount`, and non-endpoint reverse-bit
  patterns.
- The header describes a 32-bit-only custom `ReverseBits` API but does not
  identify it as a project extension or state why no 64-bit counterpart exists.

## Final assessment

The existing implementation is correct for the tested bit operations and has
no sanitizer-observed defect.  Its exact supported API boundary is incomplete
and undocumented; resolving that policy and adding 64-bit coverage are the
appropriate follow-up work.  No implementation was modified during this audit.
