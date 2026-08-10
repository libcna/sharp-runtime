# Audit: `modules/security/include/System/Security/Authentication/AuthenticationException.hpp`

## Metadata

- AUDITED: default, message, and inner-exception construction and inheritance.
- Validation: the security fixture passed 38/38; a direct native/current-.NET
  probe observed `System error.` and `0x80131501` for the default instance in
  both implementations.

## Assessment

The class preserves the expected `SystemException` hierarchy and forwards the
message and native `std::exception_ptr` cause through the runtime's exception
adapter.  Current .NET's default result agrees with the inherited native
message and HResult.

## Other missing assertions and diagnostics

- Assert the default message and HResult, causal-inner identity, and repeated
  catch through `SystemException` rather than only construction and one custom
  message.
- Exercise an authentication producer boundary when TLS transport becomes a
  supported component; this header alone cannot validate retry semantics.

## Final assessment

No authentication-exception mismatch was demonstrated. No source or test was
changed.
