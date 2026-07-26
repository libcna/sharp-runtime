# Audit: `modules/security-cryptography/include/System/Security/Cryptography/HMACSHA256.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The concrete HMAC wrapper selects its named hash and expected digest size; focused vectors or smoke checks cover the shared construction.

## Missing assertions and diagnostics

Add published vectors, key-replacement, long-key, and disposal-zeroization coverage. It inherits SR-AUD-332.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
