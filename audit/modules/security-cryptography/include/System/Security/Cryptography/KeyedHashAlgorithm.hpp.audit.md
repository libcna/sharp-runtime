# Audit: `modules/security-cryptography/include/System/Security/Cryptography/KeyedHashAlgorithm.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The base disposal path overwrites and clears keyValue. It cannot erase transformed key material held by derived classes.

## Missing assertions and diagnostics

Require derived classes caching transformed keys to wipe them. HMAC fails this requirement (SR-AUD-332).

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
