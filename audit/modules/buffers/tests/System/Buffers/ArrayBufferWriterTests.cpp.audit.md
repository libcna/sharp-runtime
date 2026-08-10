# Audit: `modules/buffers/tests/System/Buffers/ArrayBufferWriterTests.cpp`

## Metadata

- Audit status: AUDITED (94 lines, thirteen tests, fully read).
- Validation: `ArrayBufferWriterTest.*` passed 13/13 in the combined direct
  `SharpRuntimeTests_Buffers` filter on 2026-07-26.  The filter's three direct
  fixtures passed 54/54.
- Related implementation: `ArrayBufferWriter.hpp`; its generic constraint is
  recorded as SR-AUD-070.

## Assessment

The fixture gives useful coverage of default and explicit capacity construction,
advance accounting, written views, clear/reset distinction, and the two
documented `Advance` failures.  It uses only `int` and `char`, so it verifies
the ordinary vector-backed path but cannot establish the public generic
contract advertised by the header.

## Finding references

- **SR-AUD-070 (extended):** all allocations and `Clear` instantiate only
  default-constructible scalar types.  The suite therefore cannot expose the
  undocumented `std::vector::resize`/`T{}` requirement confirmed by the
  standalone non-default-constructible probe.

## Other missing assertions and diagnostics

- No test calls `GetMemory(-1)` or `GetSpan(-1)`, requests a zero hint, or
  asserts exception type and parameter name for invalid size hints.
- The growth test only checks a lower bound after the first allocation; it
  does not cover a second growth, exact/oversized hints, capacity arithmetic at
  `intcs` boundaries, allocation failure, or the source `ArrayMaxLength`
  limit.
- No test retains a span or memory across growth, `Advance`, `Clear`, reset,
  copy, or move.  Such a test must define the native stale-view contract before
  dereferencing a vector-invalidated pointer.
- The suite omits non-default-constructible, move-only, throwing-copy,
  reference-owning, and oversized element types.  It also never reaches the
  implementation through an `IBufferWriter<T>&` polymorphic consumer.
- Clear verifies one scalar slot only; it does not distinguish destruction,
  zeroing, and retained capacity for a nontrivial element type.

## Final assessment

All thirteen scalar happy-path tests pass, but the suite leaves input-boundary,
growth/lifetime, and generic-type diagnostics unasserted.  No source or test
was modified during this audit.
