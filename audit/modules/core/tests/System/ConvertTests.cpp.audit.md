# Audit: `modules/core/tests/System/ConvertTests.cpp`

## Metadata

- Audit status: AUDITED (555 lines, 123 tests, full read).
- Validation: `ConvertTests.*` passed 204/204, including the split companion
  file, on 2026-07-25.

## Assessment

This is a substantial regression suite for normal parsing, rounding, base
conversion, raw C-string overload dispatch, hex, and ordinary Base64 round
trips. Its historical-regression comments are valuable. All numeric invalid
vectors are finite and Base64 failures cover only length and bad alphabet
characters, leaving the confirmed public boundary gaps invisible.

## Finding references

- **SR-AUD-026:** only in-range `ToByte(long)`, positive signed-to-unsigned,
  and valid `ToChar(int)` values are asserted; wrap-prone inputs are absent.
- **SR-AUD-027:** no NaN or infinity input exercises direct floating-to-int
  conversion after rounding.
- **SR-AUD-028:** no test requires accepted Base64 whitespace or rejects
  leading/middle/excess `=` padding.

## Required post-audit verification

Add exact `OverflowException` vectors for every direct conversion family and
Base64 grammar vectors described in the owning reports. Preserve the current
positive round trips; they do not substitute for invalid-input coverage.

## Final assessment

Strong repair regression history, with missing negative assertions in three
separate high-risk conversion families.
