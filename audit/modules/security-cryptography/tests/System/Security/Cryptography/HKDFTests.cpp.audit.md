# Audit: `modules/security-cryptography/tests/System/Security/Cryptography/HKDFTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

Two RFC 5869 SHA-256 cases plus length/error paths pass.

## Missing assertions and diagnostics

Add all supported algorithms, zero/exact maximum output, unsupported hash diagnostics, and DeriveKey equivalence.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
