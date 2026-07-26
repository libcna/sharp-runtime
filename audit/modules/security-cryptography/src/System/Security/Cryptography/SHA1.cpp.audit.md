# Audit: `modules/security-cryptography/src/System/Security/Cryptography/SHA1.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The compression, padding, digest serialization, and reset path were read with the corresponding focused vectors passing.

## Missing assertions and diagnostics

Add all padding-boundary, multi-block, long-message, disposal, and invalid-range regressions.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
