# Audit: `modules/threading/include/System/Threading/Interlocked.hpp`

## Metadata

- AUDITED: 56-line raw-reference atomic-operation adapter, fully read.
- Validation: focused `ThreadingTests.Interlocked_*` passed 10/10 on
  2026-07-27. A four-thread increment/read TSan probe completed at the expected
  count with no race diagnostic.
- Reference basis: current .NET integer Interlocked semantics and defined
  unchecked integral arithmetic.

## Assessment

The implemented 32-/64-bit integer Increment, Decrement, Add, Exchange,
CompareExchange, and Read paths use sequentially consistent compiler atomics
and preserve the reviewed ordinary contract under contention. No new
evidence-backed defect is demonstrated.

## Other missing assertions and diagnostics

- Tests omit 64-bit Exchange/CompareExchange/Read, contention for every
  operation, signed min/max unchecked wrap, and failure-path CompareExchange
  return values.
- They omit pointer/reference, floating-point, generic, bitwise, and
  memory-barrier overloads from the broader managed surface; the header labels
  itself a partial adaptation but provides no explicit coverage matrix.
- No fixture checks interaction with non-Interlocked ordinary reads/writes,
  alignment boundaries, or ABI/platform lock-free guarantees.

## Final assessment

The implemented integer subset is coherent under the reviewed TSan scenario.
No source or test was changed.
