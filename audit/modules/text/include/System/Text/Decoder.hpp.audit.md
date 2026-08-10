# Audit: `modules/text/include/System/Text/Decoder.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The wrapper explicitly documents that it is stateless and forwards conversion
to its encoding. Its optional vector `count = -1` is a C++ convenience, not a
complete managed Decoder API; no independent implementation failure was
demonstrated beyond the underlying encoding range findings.

## Finding references

- SR-AUD-286 — high — forwarding preserves unsafe raw decode-range behavior.

## Other missing assertions and diagnostics

- Test null encoding construction, `count = -1` versus other negative values,
  split sequences, reset, and stale output on failures.

## Final assessment

No independent new finding is confirmed.
