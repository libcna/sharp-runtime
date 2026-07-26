# Audit: `modules/net-websockets/include/System/Net/WebSockets/WebSocketException.hpp`

## Metadata

- AUDITED: error-code mapping, message constructors, native error adaptation,
  and causal exception handling.
- Validation: direct C++/current-.NET probe constructed HeaderError with a
  distinct inner exception.

## SR-AUD-250 — medium — WebSocketException silently discards its public inner exception

The C++ `(WebSocketError, message, exception_ptr)` constructor explicitly
casts `innerException` to void, and the probe observes no retained inner cause.
Current .NET's matching constructor preserves the supplied exception
(`InnerException` is the original instance).  This loses the causal failure
needed to diagnose handshake/transport faults.

## Assessment

The reduced native-error-code overload scope is documented, but it does not
justify dropping an overload that is present in the C++ surface.  Basic error
code/message construction remains coherent.

## Other missing assertions and diagnostics

- Test all constructors, exact error code/native HResult behavior, and inner
  exception identity/rethrow paths.
- Keep any unsupported P/Invoke overloads explicitly separate from causal
  exception propagation.

## Final assessment

SR-AUD-250 is directly reproduced. No source or test was changed.
