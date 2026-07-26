# Audit: `modules/security-cryptography/include/System/Security/Cryptography/Oid.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The reduced OID carrier preserves caller-supplied value/friendly-name pairs and explicitly excludes platform lookup.

## Missing assertions and diagnostics

Add null-to-value, repeated-null, getter isolation, and reduced-scope diagnostics coverage.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
