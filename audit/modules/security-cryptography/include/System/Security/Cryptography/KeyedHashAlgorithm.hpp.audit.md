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

---

## Remediation record — ticket #2159 (2026-08-09)

This report's requirement — "require derived classes caching transformed keys to wipe them" — is
now enforced by `HMAC` itself rather than only requested (SR-AUD-332, **remediated**).

Two corrections to this base's own behaviour landed with it. `Dispose` cleared `keyValue_` with
`std::fill`, which GCC 13.3 at `-O2` **deletes outright** when the buffer is dying — measured by
disassembly in `docs/SystemSecurityCryptographyNamespaceReviewPlan.md` §4.3 — so it now goes through
`SharpRuntimeDetail::SecureMemory`, a `volatile` write loop the compiler may not elide. And
`setKeyProperty` move-assigned over the old key, releasing its block unerased; it now erases first.
A destructor was added so that the ordinary C++ path — an object that goes out of scope without
`Dispose` — erases too, with all four copy/move operations explicitly defaulted so the destructor
cannot silently turn a move into a copy and leave the source holding a live key.
