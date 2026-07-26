# Audit: `modules/threading/include/System/Threading/Volatile.hpp`

## Metadata

- AUDITED: 28-line acquire/release template adapter, fully read.
- Validation: focused `ThreadingTests.Volatile_*` passed 2/2 on 2026-07-27. A
  TSan producer/payload/ready-flag probe validated release publication and
  acquire observation with no race diagnostic.
- Reference basis: current .NET Volatile acquire/release ordering intent.

## Assessment

The generic compiler-atomic load/store paths correctly support the reviewed
scalar publication scenario and the direct integer smoke tests. No new
evidence-backed defect is demonstrated.

## Other missing assertions and diagnostics

- Tests omit bool, pointer, floating-point, unsigned, reference, and
  non-lock-free scalar types, plus all concurrent ordering scenarios.
- They omit exact managed overload availability, unsupported type diagnostics,
  null/reference semantics, alignment, and platform-specific atomic fallback
  behavior.
- The generic signature is broader in spelling than .NET's overload set but
  has no compile-time capability/constraint tests.

## Final assessment

The reviewed scalar memory-order path is coherent. No source or test was
changed.
