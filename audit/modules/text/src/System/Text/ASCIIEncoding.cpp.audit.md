# Audit: `modules/text/src/System/Text/ASCIIEncoding.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

UTF-8 input decoding for `GetBytes` is well formed, but `GetString` silently
returns empty for a negative count and applies `data[index + i]` without
validating a negative index. An ASan probe with index -1 reports a
stack-buffer-underflow. Both conversion directions hard-code `?` instead of
using configured exception/replacement fallbacks.

## Finding references

- SR-AUD-286 — high — unchecked signed raw range reaches an out-of-bounds read.
- SR-AUD-292 — medium — configured fallback policies are ignored.

## Other missing assertions and diagnostics

- Test negative index/count, null data with nonzero count, short raw buffers,
  exception fallback, and non-ASCII scalar counts.

## Final assessment

SR-AUD-286 and SR-AUD-292 apply.
