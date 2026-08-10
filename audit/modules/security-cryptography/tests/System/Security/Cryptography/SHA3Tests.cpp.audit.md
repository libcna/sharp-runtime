# Audit: `modules/security-cryptography/tests/System/Security/Cryptography/SHA3Tests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

SHA3, SHAKE, and HMAC-SHA3 vectors pass.

## Missing assertions and diagnostics

Add multi-block SHA3-384/512, incremental/reset/zero-length SHAKE256, and fixed HMAC-SHA3-384/512 vectors.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
