# Audit: `modules/core/include/System/Random.hpp`

## Metadata

- Audit status: AUDITED (436 lines, full read).
- Public API: seeded compatible PRNG, integer/float/byte generation, shared
  generator, and collection/string utilities.
- Validation: 74 Random tests passed in the focused Core.Base run.

## Assessment

The seeded algorithm and its intended byte-for-byte .NET compatibility are
well documented.  Template range operations carefully distinguish unsupported
wide ranges, and the utility methods validate empty choices and negative
lengths.  The public `Shared` contract is not fulfilled by its implementation.

## Finding reference

**SR-AUD-010** in
[`Random.cpp.audit.md`](../../src/System/Random.cpp.audit.md): the header
promises a thread-safe shared generator, but the returned singleton exposes
unsynchronized mutable generator state to every caller.

## Final assessment

Seeded single-threaded behavior is strong; `Random::Shared` needs a
thread-safety repair and concurrency regression evidence (SR-AUD-010).
