# Audit: `modules/security-cryptography/include/System/Security/Cryptography/KeySizes.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The immutable three-value range carrier fulfills its present passive metadata role and basic accessors pass.

## Missing assertions and diagnostics

Add boundary and copy/move checks if this type becomes a validator.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
