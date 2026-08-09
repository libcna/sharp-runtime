# Audit: `modules/security-cryptography/include/System/Security/Cryptography/DeriveBytes.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## Assessment

The abstract base offers sequential derivation and reset, but its default Dispose implementation is empty. Concrete types owning secrets must override it.

## Missing assertions and diagnostics

Require every concrete derivation type to reject post-disposal use and prove zeroization; Rfc2898DeriveBytes fails that obligation (SR-AUD-331).

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.

---

## Remediation record — ticket #2160 (2026-08-09)

This report's obligation — "require every concrete derivation type to reject post-disposal use and
prove zeroization; Rfc2898DeriveBytes fails that obligation (SR-AUD-331)" — is now met by the only
concrete derivation type this component has. `DeriveBytes::Dispose()` itself is **unchanged**: an
empty default is the right shape for a base that owns nothing, and it is what makes the derived
override a vtable-slot fill rather than a vtable change.
