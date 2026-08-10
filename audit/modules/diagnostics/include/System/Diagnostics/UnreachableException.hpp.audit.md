# Audit: `modules/diagnostics/include/System/Diagnostics/UnreachableException.hpp`

## Metadata

- AUDITED: default, message, and inner-exception construction.
- Evidence: declaration review and three direct tests.

## Assessment

The exception derives from the local Exception hierarchy and provides the
expected nonempty fallback diagnostic in its supported constructor set.

## Other missing assertions and diagnostics

- Add inner-exception identity, empty/UTF-8 message, and HResult coverage.

## Final assessment

No standalone finding. No source or test changed.
