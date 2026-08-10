# Audit: `modules/security-cryptography/include/System/Security/Cryptography/detail/Sha512Core.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The shared SHA-384/SHA-512 compression core uses conventional big-endian schedule and 80-round FIPS arithmetic; dependent vectors pass.

## Missing assertions and diagnostics

Add direct multi-block cross-checks and document/test the practical 64-bit input-length accounting limit.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
