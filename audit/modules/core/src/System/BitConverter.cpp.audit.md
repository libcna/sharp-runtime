# Audit: `modules/core/src/System/BitConverter.cpp`

## Metadata

- Audit status: AUDITED (60 lines, full read).
- Validation: `BitConverterTests.*` passed 67/67 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.

## Assessment

This implementation is limited to `ToString` and correctly distinguishes the
raw-pointer precondition from vector-aware bounds checking.  It fixes the
earlier negative-start out-of-bounds read in the formatter and preserves the
documented empty-vector boundary.  No independent defect was confirmed here;
the remaining BitConverter finding belongs to inline typed decoders in the
header.

## Finding references

- **SR-AUD-041:** the nearby typed vector `To*` decoders remain unchecked in
  `BitConverter.hpp`; this file's `ToString` checks are a useful local model
  for their required malformed-input validation.

## Other missing assertions and diagnostics

- The formatter tests cover negative and overlong ranges well, including the
  formerly unsafe `startIndex == -1` path.
- No shared helper exists to make the same checked index/remaining-width policy
  available to the inline typed decoders.

## Final assessment

The source-backed formatting subset is robustly bounded.  No implementation
was modified during this audit.
