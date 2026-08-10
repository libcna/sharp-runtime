# Audit: `modules/security/include/System/Security/SecurityAttributes.hpp`

## Metadata

- AUDITED: CAS/transparency enum and attribute metadata adapters.
- Evidence: local .NET source/reference declarations, the 38/38 native
  fixture, and the project task matrix were inspected.

## Assessment

These lightweight attributes preserve the implemented stored values for
`AllowPartiallyTrustedCallersAttribute` and `SecurityRulesAttribute`; the
remaining types are markers.  The task matrix marks these CAS/transparency
types, including `SecurityCriticalScope`, as ignored, and modern .NET Core
does not enforce their old transparency semantics.  The C++ subset therefore
must remain described as metadata rather than a security enforcement system.

## Other missing assertions and diagnostics

- The fixture only instantiates markers.  Add explicit getter/setter,
  copy/move, invalid-enum, and default-value checks for the two stored
  attributes.
- Record the omitted `SecurityCriticalScope` constructor/property and managed
  attribute-target metadata in the ignored-surface inventory, so a later scope
  change is not mistaken for an implementation regression.

## Final assessment

No unsupported-CAS discrepancy is promoted to a confirmed finding under the
recorded ignored-type boundary. No source or test was changed.
