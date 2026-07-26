# Audit: `modules/security-cryptography/include/System/Security/Cryptography/HashAlgorithmName.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The wrapper recognizes documented MD5, SHA-1/2, and SHA-3 OIDs. SHA-256 and unknown-OID behavior is covered.

## Missing assertions and diagnostics

Add null/empty values, every supported OID, SHA-3 OID round trips, and diagnostic assertions.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
