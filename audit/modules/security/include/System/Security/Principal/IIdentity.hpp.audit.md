# Audit: `modules/security/include/System/Security/Principal/IIdentity.hpp`

## Metadata

- AUDITED: abstract name, authentication-type, and authentication-state
  contract.
- Evidence: `GenericIdentity`, `GenericPrincipal`, their tests, and the local
  managed `IIdentity` consumers were reviewed.

## Assessment

The small virtual interface is a clear C++ adapter for the identity properties
used by this component.  Optional strings are an explicit native way to
represent unavailable values; its concrete generic identity always supplies
empty strings just as the managed generic type does.

## Other missing assertions and diagnostics

- Add a minimal custom `IIdentity` implementation exercised through
  `GenericPrincipal`, including absent optional values and destructor
  polymorphism.
- Document ownership expectations for identities retained through shared
  pointers so callers can distinguish this from managed reference semantics.

## Final assessment

No interface-level defect was demonstrated. No source or test was changed.
