# Audit: `modules/security-cryptography/include/System/Security/Cryptography/HKDF.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

RFC 5869 Extract, Expand, and DeriveKey pass the SHA-256 vectors and reviewed output limits; no additional behavior defect was reproduced.

## Missing assertions and diagnostics

Add SHA-1/384/512/SHA-3 vectors, zero and exact-maximum output, unsupported-algorithm diagnostics, and Extract/Expand equivalence.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
