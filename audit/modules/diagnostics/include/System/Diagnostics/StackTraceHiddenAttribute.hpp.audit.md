# Audit: `modules/diagnostics/include/System/Diagnostics/StackTraceHiddenAttribute.hpp`

## Metadata

- AUDITED: marker attribute declaration.
- Evidence: declaration review and marker-instantiation test.

## Assessment

The passive marker is consistent with the non-reflective C++ diagnostics
subset; no stack formatter consumes it.

## Other missing assertions and diagnostics

- Add formatter integration only if attribute-aware stack filtering is designed.

## Final assessment

No standalone finding. No source or test changed.
