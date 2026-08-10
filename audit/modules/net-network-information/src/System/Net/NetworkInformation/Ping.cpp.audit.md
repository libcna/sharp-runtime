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

---

## Remediation record — tickets #2188, #2189, #2190, #2191, #2193 (2026-08-10)

**SR-AUD-253 → remediated. SR-AUD-254 → remediated. SR-AUD-255 → remediated.**
Original evidence above is retained unchanged; everything below is an addition.

Design record: `docs/SystemNetNetworkInformationNamespaceReviewPlan.md` (ticket #2187).

### SR-AUD-253 — ticket #2188

**Premise correction: this reaches all eight `SendPingAsync` overloads, not the one this report's
probe names.** Measured before any change (`build-probe/2187_probe1_surface.log`): ten probes across
all eight overloads — negative timeout, oversized buffer, wildcard address and empty host name —
every one returned a task and faulted later, while the corresponding synchronous door threw at the
call. There was no partially-correct async door.

**The report's "can launch a thread for invalid input" is now measured, not conjectured.** `TaskT`'s
callable constructor is `std::async(std::launch::async, …)`, i.e. a real OS thread per task; eight
`SendPingAsync(addr, -1)` calls started real workers (`build-probe/2187_probe2_resources.log`).

**Repair.** The two four-argument doors call `checkArgs` before constructing the task. The two
three-argument host doors mirror .NET's shape — reject an empty host, short-circuit an IP literal to
the address overload, otherwise validate timeout and buffer before the resolver runs. The remaining
four delegate. Exception types and messages are unchanged; only the delivery point moved, which a
test pins by comparing the async rejection's message against the synchronous door's.

### SR-AUD-254 — ticket #2189

**Premise correction: the native error code is destroyed, not merely the type.** The object thrown
is `NetworkInformationException` with `what() == "Win32 error 13"` and
`getErrorCodeProperty() == 13`; what survived the wrapper was `St9exception` / `"std::exception"` /
no code. The mechanism is `std::make_exception_ptr(e)` binding to the `std::exception` **base
subobject** — slicing at the moment of capture, not at rethrow.

**A second door this report does not name, and it is the opposite defect.** `Dns::GetHostAddresses`
runs **outside** `sendWithExceptionWrapping`, so a resolver failure escapes `Send(host…)` as
`System::Net::Sockets::SocketException`, while the module declares
`PingException("Could not resolve host name or address.")` three lines later on a branch
`Dns::GetHostAddresses` makes practically unreachable. Under-wrapping and over-wrapping of one
conceptual failure. Which .NET produces cannot be settled here — no `/rv`, no managed runtime — so
it is **ticket #2192 (deferred verification)** and the current behaviour is pinned by test.

**Repair.** `catch (...)` storing `std::current_exception()`, which refers to the exception object
currently being handled, so the dynamic type, the message and the native error code all survive; the
catch-all also preserves a cause that does not derive from `std::exception`, which the old handler
let escape unwrapped. This is exactly the "catch-all/rethrow mechanism" this report's own *Other
missing assertions* section prescribes. The `PlatformNotSupportedException` pass-through arm and the
outer exception's type and message are unchanged.

### SR-AUD-255 — ticket #2190

**Premise correction: three sites, and the fourth sibling was already correct.** `Ping.cpp:351`,
`:404` and `:410` fabricated `PingOptions()`; `Ping.cpp:357` (`Send(address, timeout, buffer)`)
already forwarded `nullptr`. The repair is therefore justified by an inconsistency **inside one
file**, independently of the .NET comparison this report makes.

**Repair.** All three forward absence. Two documented consequences, both toward .NET: a reply from
those doors reports no options rather than a default `{ttl = 128, dontFragment = false}`, and the
socket no longer has its TTL forced to 128.

**Limitation, recorded rather than hidden.** The end-to-end consequence needs a reply, and every
send in this container fails at socket creation (`ping_group_range = "1 0"`, ticket #1962). A
mutation restoring the fabrication therefore leaves the suite green here — the only test that can
observe it skips. The structural half (that absence is representable and distinct from a default) is
pinned deterministically; the end-to-end pin is guarded and will discriminate wherever an
unprivileged ICMP socket exists.

### Post-audit defects in the same body — ordinary ticket numbers only

Audit numbering is frozen at 364; none of these carries an `SR-AUD-*` identifier.

- **#2191 (done).** Both four-argument async lambdas captured a raw `this` and called a non-static
  member on it from the worker, so destroying the `Ping` while its task ran called a member function
  on a destroyed object. `sizeof(Ping) == 1` — the class declares no data members — so the capture
  was removable outright. This does **not** resolve or pre-empt the blocked stateful raw-`this`
  family (#2066, #2088, #2134, SR-AUD-263/310), whose owners hold real state.
- **#2192 (todo, deferred verification).** The DNS wrapping asymmetry above.
- **#2193 (done).** `sendPingCore` held a bare `int` across five allocating operations; an exception
  from any of them leaked the descriptor. It now holds a non-copyable `OwnedDescriptor`. This also
  closed a second window: the manual `close` after `recv()` ran **before** the `errno` reads that
  report the recv failure, and `close()` may set `errno`.
- **#2194 (blocked).** This report's *Other missing assertions* also asks that replies be matched by
  destination, identifier and sequence, and that `setsockopt` results be validated. Neither can be
  exercised while every send fails at socket creation. Blocked on #1962, which stays blocked; a
  raw-socket fallback must **not** be added to unblock it.

### Verification

Module suite **39 → 62 tests** (+23), 5 failing throughout — the same five #1962 failures, unweakened
— and 1 skip (the guarded SR-AUD-255 end-to-end pin). Mutations: restoring `make_exception_ptr(e)`
failed exactly the three slicing tests; moving `Dns` inside the wrapper failed exactly the #2192 pin;
deferring validation at one async door failed exactly eight tests; restoring the fabricated options
failed none, which is recorded as a limitation of this container. ASan+UBSan+LSan, non-recovering
UBSan and TSan over the production module bodies: **exit 0, zero reports**. No public signature,
member, virtual, vtable, object layout, `noexcept` or mangled symbol changed — `Ping.hpp` is
untouched.
