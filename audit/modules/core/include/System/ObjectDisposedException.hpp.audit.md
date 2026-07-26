# Audit: `modules/core/include/System/ObjectDisposedException.hpp`

## Metadata

- Audit status: AUDITED (99-line public declaration, fully read).
- Validation: `ObjectDisposedExceptionNewTests.*` passed 3/3 within the audited
  124/124 Core.Base shared exception filter on 2026-07-26.

## Assessment

The type correctly derives from InvalidOperationException, exposes the object
name, message constructors, and conditional guards. No standalone defect was
reproduced in the reviewed native adaptation.

## Other missing assertions and diagnostics

- Tests do not assert message/object-name composition, `COR_E_OBJECTDISPOSED`,
  null/empty/non-ASCII C strings, inner identity, or both ThrowIf overloads.
- ThrowIf takes an object-name string rather than .NET's instance/caller
  expression model; this adaptation and its diagnostic limitations are not
  stated at the public call site.
- No lifecycle consumer verifies throwing only after real Dispose state change.

## Final assessment

The public construction/guard shape is coherent. No source or test was modified.
