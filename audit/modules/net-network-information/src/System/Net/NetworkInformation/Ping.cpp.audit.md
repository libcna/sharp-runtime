# Audit: `modules/net-network-information/src/System/Net/NetworkInformation/Ping.cpp`

## Metadata

- AUDITED: POSIX ICMP packet construction, error wrapping, synchronous and
  Task dispatch.
- Validation: ASan/UBSan C++ probe, current-.NET probe, and focused fixture;
  runtime ICMP cannot start under the sandbox socket policy.

## Assessment

Packet construction, checksum layout, argument limits, and basic status mapping
were reviewed against current .NET. Three independently observable Task/error
contract defects are confirmed below. The receive path also lacks a regression
that proves source/identifier/sequence association before accepting a reply.

## SR-AUD-253 — medium — async ping defers mandatory argument validation into a faulted task

Every `SendPingAsync` overload creates `TaskT` around a lambda that calls
`Send`, so `checkArgs` executes on the worker. The native probe prints
`negativeTimeout=returned-task;task-error=ArgumentOutOfRangeException`; current
.NET prints `negativeTimeout=sync-ArgumentOutOfRangeException` because its
public overload validates before constructing the async operation. This changes
caller control flow and can launch a thread for invalid input.

## SR-AUD-254 — medium — Ping exception wrapping slices the concrete native cause

`sendWithExceptionWrapping` catches `const std::exception& e` then stores
`make_exception_ptr(e)`. A sandbox-denied socket path produces a PingException
whose rethrown cause is `std::exception` with message `std::exception`, rather
than `NetworkInformationException` and its native error information. Current
.NET retains the concrete underlying exception when wrapping Ping failures.

## SR-AUD-255 — medium — no-options Ping overloads fabricate a default options value

The string synchronous no-options overload and all no-options async overloads
forward `PingOptions()` rather than absence. Current .NET forwards `null`; its
loopback probe prints `defaultOptions=null`. A successful native reply from
those overloads consequently exposes a present default `PingReply.Options`
value and may apply options that the caller did not request.

## Other missing assertions and diagnostics

- Validate `setsockopt` results; match replies by destination, identifier, and
  sequence before returning one; exercise timeout, malformed packet, IPv6,
  cancellation, concurrent operation, destruction, and option-null behavior.
- Preserve the original `exception_ptr` with a catch-all/rethrow mechanism
  rather than materializing the `std::exception` base subobject.

## Final assessment

SR-AUD-253 through SR-AUD-255 are confirmed. No source or test changed.
