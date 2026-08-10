# Audit: `modules/security-cryptography/include/System/Security/Cryptography/HashAlgorithm.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The reduced buffer-only API validates ranges and disposal before hashing. Fixed vectors and two TryComputeHash paths pass.

## Missing assertions and diagnostics

Add disposal, bad offset/count, failure cleanup, and getHash lifecycle assertions for SHA-1/2/3 and HMAC.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
