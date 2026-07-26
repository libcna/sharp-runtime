# Audit: `modules/text/include/System/Text/UTF7Encoding.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The RFC-2152 direct/shifted conversion and signed index/count checks are much
stronger than other encodings. However malformed input always uses an internal
replacement and never consults the inherited decoder fallback setting.

## Finding references

- SR-AUD-292 — medium — configured fallback policies are ignored outside the
  UTF-8 implementation.

## Other missing assertions and diagnostics

- Test exception fallback, raw pointer capacity limits, unclosed shifted
  sequences, all malformed base64 tails, and nonzero offsets.

## Final assessment

SR-AUD-292 applies.
