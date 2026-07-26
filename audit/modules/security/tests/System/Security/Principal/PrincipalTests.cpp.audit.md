# Audit: `modules/security/tests/System/Security/Principal/PrincipalTests.cpp`

## Metadata

- AUDITED: seven principal/identity/impersonation fixture cases.
- Validation: the complete security fixture passed 38/38; direct C++/.NET
  comparison supplied the Unicode role regression evidence.

## SR-AUD-246 — medium — ASCII-only tests miss Unicode role-membership divergence

All role tests use ASCII spellings, so they pass even though
`GenericPrincipal` fails the direct `ÄDMIN` / `ädmin` membership pair that
current .NET accepts through Unicode `OrdinalIgnoreCase`.  The implementation
evidence and remediation target are recorded in the `GenericPrincipal` report.

## Assessment

The fixture correctly protects null identity rejection, role-vector copying
only indirectly, normal ASCII case-insensitive lookup, and basic generic
identity state.  Its character-domain coverage is insufficient for the public
role contract.

## Other missing assertions and diagnostics

- Add the non-ASCII regression, empty roles, copied-after-construction source
  vector, interface-dispatch, and UTF-8 identity/authentication-type cases.
- State the comparison contract in failure messages so an ASCII-only fallback
  cannot look like managed ordinal matching.

## Final assessment

SR-AUD-246 is not detectable by the current ASCII-only fixture. No source or
test was changed.
