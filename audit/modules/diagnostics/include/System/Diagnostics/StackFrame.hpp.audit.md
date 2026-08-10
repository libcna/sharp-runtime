# Audit: `modules/diagnostics/include/System/Diagnostics/StackFrame.hpp`

## Metadata

- AUDITED: frame storage, unknown offsets, and textual rendering.
- Evidence: declaration review and nine direct StackFrame tests.

## Assessment

The explicit source-location container correctly distinguishes unknown offsets
from unknown source information in its supported subset. It does not capture a
live native stack, as the larger StackTrace documentation states.

## Other missing assertions and diagnostics

- Test negative source coordinates, embedded NUL path text, and method/native
  data construction if those fields become writable.

## Final assessment

No standalone finding. No source or test changed.
