# Audit: `modules/security-cryptography/CMakeLists.txt`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The static module declares exactly the documented public Core.Base dependency; no boundary divergence was found.

## Missing assertions and diagnostics

Keep the selective consumer build in CI so public headers remain usable with that dependency alone.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
