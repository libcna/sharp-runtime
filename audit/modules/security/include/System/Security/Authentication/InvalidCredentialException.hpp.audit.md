# Audit: `modules/security/include/System/Security/Authentication/InvalidCredentialException.hpp`

## Metadata

- AUDITED: constructors and `AuthenticationException` subtype relationship.
- Validation: the security fixture passed 38/38; the direct native/current-.NET
  default probe agrees on `System error.` and `0x80131501`.

## Assessment

The exception retains the message/cause constructors and is catchable through
the authentication base type, matching the implemented .NET-facing subset.
No TLS stream is present in this component, so the class does not itself
establish a credential-validation execution path.

## Other missing assertions and diagnostics

- Assert default message/HResult and causal-inner preservation directly for
  this derived type.
- Add producer-side coverage only with a future supported authentication
  transport; do not imply that this metadata component negotiates credentials.

## Final assessment

No constructor or hierarchy mismatch was demonstrated. No source or test was
changed.
