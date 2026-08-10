# Audit: `modules/text/include/System/Text/DecoderFallback.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

Replacement and exception buffers are coherent for their implemented one-shot
usage. The public raw pointer `GetFallbackString` has no null/count validation,
and local conversion callers generally bypass this interface in favor of their
own loops.

## Finding references

- SR-AUD-286 — high — raw signed length validation is inconsistent across the
  encoding surface.

## Other missing assertions and diagnostics

- Test null/negative raw fallback arguments, replacement strings with
  non-ASCII UTF-8, reset/re-entry, and exact offending-sequence indexes.

## Final assessment

No independent new finding is confirmed.
