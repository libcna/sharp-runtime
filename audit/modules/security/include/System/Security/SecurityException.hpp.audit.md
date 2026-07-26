# Audit: `modules/security/include/System/Security/SecurityException.hpp`

## Metadata

- AUDITED: constructor messages, native cause, HResult, and explicitly reduced
  CAS diagnostic surface.
- Validation: the complete security fixture passed 38/38; direct native and
  current-.NET probes agree on default `Security error.` and `0x8013150A`.

## Assessment

The implemented constructors consistently set `COR_E_SECURITY`, preserve the
native cause where represented, and agree with the managed default result.
The header openly documents the omitted CAS-era properties and replaces the
managed `(message, Type)` diagnostic with a string-only adaptation; that is a
declared scope boundary rather than an undisclosed behavioral claim.

## Other missing assertions and diagnostics

- Assert custom and causal constructor HResults and retained inner-exception
  identity, not only `what()`.
- Maintain a visible supported-surface table for the omitted managed
  `PermissionType`, `PermissionState`, and related diagnostic properties.

## Final assessment

No unadvertised `SecurityException` mismatch was demonstrated. No source or
test was changed.
