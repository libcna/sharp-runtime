# Audit: `modules/core/include/System/ReadOnlySpan.hpp`

## Metadata

- Audit status: AUDITED (6-line forwarding header, fully read).
- Validation: `ReadOnlySpanTests.*` passed 24/24 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This header correctly centralizes the actual `ReadOnlySpan<T>` definition in
`Span.hpp`; it introduces no independent logic.  Its behavior, public API,
and both confirmed defects are therefore owned by the combined implementation
report.

## Finding references

- **SR-AUD-043:** negative `ReadOnlySpan<T>` lengths are accepted by the
  definition included from `Span.hpp` and can become unbounded reads.
- **SR-AUD-044:** `ReadOnlySpan<T>::CopyTo`/`TryCopyTo` use forward copy and
  corrupt overlapping nontrivial ranges.

See [`Span.hpp.audit.md`](Span.hpp.audit.md).

## Required post-audit verification

Keep this forwarding include path while fixing the shared implementation; test
both `System/Span.hpp` and `System/ReadOnlySpan.hpp` include paths so they
cannot drift or produce incompatible definitions.

## Final assessment

The forwarding header itself is sound.  No implementation was modified during
this audit.
