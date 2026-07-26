# Audit: `modules/security-cryptography/include/System/Security/Cryptography/SHA256Managed.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The legacy-name wrapper delegates to its modern hash implementation; the focused suite only uses a basic output check.

## Missing assertions and diagnostics

Replace length-only smoke coverage with a known digest and disposal/reuse assertion.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
