# Audit: `modules/console/include/System/ConsoleKeyInfo.hpp`

## Metadata

- AUDITED: key character/key/modifier state, validation, and equality.
- Validation: constructor, flags, equality, and out-of-range test cases passed
  within Console 123/123.

## Assessment

The compact value type preserves ordinary keyboard state and rejects a key code
outside the documented byte range.  Its `char` character is a visible native
limitation relative to managed UTF-16; no separate contradiction was
demonstrated in its documented terminal-adapter scope.

## Other missing assertions and diagnostics

- Add boundary key values, invalid modifier casts, non-ASCII/multibyte input,
  hash/format parity, and values produced by real ReadKey paths.

## Final assessment

No declaration-level defect was demonstrated. No source or test was changed.
