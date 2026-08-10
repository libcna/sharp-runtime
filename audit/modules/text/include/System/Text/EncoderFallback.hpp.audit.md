# Audit: `modules/text/include/System/Text/EncoderFallback.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The reduced UTF-8-byte representation is documented. Exception and replacement
buffers work in the UTF-8 implementation, but other encodings expose the
setting while not consulting it.

## Finding references

- SR-AUD-292 — medium — configured fallback policies are ignored outside the
  UTF-8 implementation.

## Other missing assertions and diagnostics

- Test multi-byte replacement text, fallback indexes, empty replacement,
  surrogate-pair adaptation, and each concrete encoder's exception path.

## Final assessment

SR-AUD-292 applies through consumers.
