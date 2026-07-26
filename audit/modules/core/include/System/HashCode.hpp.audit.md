# Audit: `modules/core/include/System/HashCode.hpp`

## Metadata

- Audit status: AUDITED (156-line header-only implementation, fully read).
- Validation: `HashCodeTests.*` passed 25/25 in `SharpRuntimeTests_Core_Base`
  on 2026-07-25.  The name filter includes 11 tests in the dedicated
  `HashCodeTests.cpp` plus 14 duplicate-named tests in the still-unreviewed
  multi-surface `SystemTypesRemainingTests.cpp`.
- Sanitizer probe: `/tmp/sharp-runtimervc-hashcode-audit-probe.cpp`, built with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` on 2026-07-25.

## Assessment

The normal FNV-1a accumulator uses defined unsigned arithmetic, a process-wide
seed, and consistent `Combine`, vector, span, and custom-comparer behavior for
valid input.  Exact .NET hash values need not match because the header clearly
documents its FNV adaptation rather than the runtime's xxHash32 algorithm.

The `ReadOnlySpan<uint8_t>` overload trusts its signed length then casts it to
`size_t`.  The local public `ReadOnlySpan` constructor accepts a negative
length, so `HashCode::AddBytes(ReadOnlySpan<uint8_t>(oneByte, -1))` becomes an
effectively unbounded raw-pointer walk.  This is not merely an invalid caller
precondition hidden in the raw pointer extension: it is reachable through the
managed-shaped span overload and the documented .NET constructor rejects a
negative length.  The ASan probe confirms the resulting read past a one-byte
stack array.

References: [current .NET HashCode source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/HashCode.cs.html)
and [ReadOnlySpan constructor contract](https://learn.microsoft.com/en-us/dotnet/api/system.readonlyspan-1.-ctor?view=net-10.0).

## Finding references

### SR-AUD-043 — high — AddBytes converts a negative public span length into an unbounded raw read

`ReadOnlySpan<uint8_t>(oneByte, -1)` is accepted locally, then
`HashCode::AddBytes(const ReadOnlySpan<uint8_t>&)` casts `-1` to
`std::size_t` and forwards it to the unchecked pointer loop.  ASan reports a
stack-buffer-overflow in `HashCode::AddBytes(const uint8_t*, size_t)` on the
second byte.  The defect is reachable without constructing a raw pointer/size
pair and causes a process crash or disclosure-oriented out-of-bounds read.
`Span.hpp` is the now-confirmed upstream enabling cause and must be repaired
together, but every public byte-consuming span overload must independently
avoid signed-to-unsigned length escalation.

## Required post-audit verification

Reject negative lengths in `Span` and `ReadOnlySpan` construction with the
project's `ArgumentOutOfRangeException` convention, then have `AddBytes` check
its span length before converting to `size_t` as defense in depth.  Add ASan
tests for negative span length, default/empty span, a one-byte span, and the
raw-pointer overload's documented null/length preconditions.  Audit all
`getLengthProperty()` → unsigned conversions, not only HashCode.

## Other missing assertions and diagnostics

- No dedicated test gives `AddBytes` a negative, empty-default, or malformed
  span; all calls use vectors or valid positive raw ranges.
- The raw pointer overload has neither a non-null precondition nor a defensive
  diagnostic for `data == nullptr && length != 0`.
- `ToHashCode_DefaultIsNonZero` and several tests require unequal inputs to
  hash differently, although hash collisions and a zero final hash are valid;
  the associated test-contract pattern extends SR-AUD-018.
- The header should explicitly distinguish its non-.NET raw pointer/vector
  convenience overloads from the safe `ReadOnlySpan<byte>` counterpart.

## Final assessment

The valid-input accumulator behaves consistently, but the span path contains
an ASan-confirmed high-severity out-of-bounds read caused by unchecked signed
length conversion.  No implementation was modified during this audit.
