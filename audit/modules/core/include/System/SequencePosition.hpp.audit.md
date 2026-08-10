# Audit: `modules/core/include/System/SequencePosition.hpp`

## Metadata

- Audit status: AUDITED (49-line public header-only implementation, fully
  read).
- Validation: `SequencePositionTests.*` passed 6/6, as part of the complete
  63/63 `Batch6BuffersTests.cpp` focused filter in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-sequenceposition-audit-probe.cpp`, built
  with `-std=c++20 -Wall -Wextra -Wpedantic`, mutates both public components
  after construction and prints `1,7`.
- Reference: local .NET runtime
  `src/libraries/System.Memory/src/System/SequencePosition.cs` and its four
  `SequencePosition` comparison tests were reviewed.

## Assessment

Construction, accessors, and raw pointer/integer equality are simple and work
for the sole current single-segment consumer. The C++ layout deliberately
substitutes a `void*` for a managed object reference, but it unnecessarily
exposes mutable representation details that the .NET type expressly reserves
for the sequence creator.

## SR-AUD-069 — medium — SequencePosition exposes mutable public representation instead of an opaque readonly position

Current .NET `SequencePosition` is a readonly struct with private readonly
`object` and `int` fields; its documentation says parts of the position must
not be interpreted by anything except the creator. The C++ `struct` instead
publishes mutable `void* object_` and `intcs integer_`. Any caller can rewrite
either component after a sequence returns a position, including to an
unrelated/dangling pointer or an offset that the owning sequence did not
create. The standalone probe sets an instance from `(&first, 3)` to
`(&second, 7)` through those public fields.

The header also has no equivalent value-level `Equals(object)` or
`GetHashCode` contract. Operator equality is sufficient for the current local
calls, but it does not provide the source type's equality/hash integration or
communicate the important distinction that equal component pairs need not
point at the same logical sequence location.

## Other missing assertions and diagnostics

- The direct tests only compare null-segment positions; they omit equal and
  unequal non-null segment pointers, default equality, and the documented
  warning that positional equality is not sequence-location identity.
- No test prevents or reports post-construction mutation of `object_` or
  `integer_`, a dangling segment pointer, or a foreign position passed to a
  future multi-segment sequence implementation.
- No hash/equality-container compatibility test exists, although local .NET
  tests require equal non-null, null, and boxed positions to share a hash.
- The `void*` adaptation has no ownership/lifetime diagnostic and cannot
  represent a managed boxed value (such as the reference test's integer
  segment) without an external allocation convention.

## Final assessment

The current single-segment use works, but the public mutable representation
breaks the source type's opaque readonly boundary before a multi-segment
implementation can rely on it. No source or test was modified during this
audit.
