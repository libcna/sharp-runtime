# Audit: `modules/diagnostics/include/System/Diagnostics/StackTrace.hpp`

## Metadata

- AUDITED: explicit frame-list storage, indexing, and formatting.
- Evidence: declaration review and six direct StackTrace tests.

## Assessment

The type is accurately documented as a caller-built frame container rather
than a managed stack walker. Bounds return null as its local contract states.

## Other missing assertions and diagnostics

- Cover negative indexes, copy isolation, multi-line exact formatting, and
  source-less/mixed frame sequences.

## Final assessment

No standalone finding. No source or test changed.
