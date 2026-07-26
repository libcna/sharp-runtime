# Audit: `modules/net-sockets/include/System/Net/Sockets/SelectMode.hpp`

## Metadata

- AUDITED: poll/select mode constants.
- Evidence: direct enum fixture and `Socket::Poll` source review.

## Assessment

The three supported values match .NET. `Poll` has no explicit invalid-enum
diagnostic and routes any other value to the error set; this is recorded as a
test/diagnostic gap rather than a separate confirmed result under the blocked
socket environment.

## Other missing assertions and diagnostics

- Test every mode, invalid casts, closed sockets, zero/finite/infinite timeouts,
  and readiness/error distinctions with native logging.

## Final assessment

No separate finding. No source or test changed.
