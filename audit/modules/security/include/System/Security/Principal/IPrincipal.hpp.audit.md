# Audit: `modules/security/include/System/Security/Principal/IPrincipal.hpp`

## Metadata

- AUDITED: abstract identity ownership and role-membership contract.
- Evidence: `GenericPrincipal` implementation/tests and local .NET principal
  source were reviewed.

## Assessment

The shared-pointer identity return is a coherent native adaptation of the
managed principal reference.  Its one concrete implementation preserves the
identity instance and exposes a role query.  The actual Unicode comparison
defect belongs to that implementation and is tracked as SR-AUD-246.

## Other missing assertions and diagnostics

- Test calls through `IPrincipal`, destruction through the virtual base, and
  an implementation returning a distinct identity object.
- Document that principal role semantics are implementation-defined at this
  interface boundary and must be tested by each concrete principal.

## Final assessment

No separate interface defect was demonstrated. No source or test was changed.
