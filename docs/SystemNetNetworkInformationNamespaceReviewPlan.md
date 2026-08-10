<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/net-network-information` namespace review plan (ticket #2187)

*Written 2026-08-10 on branch `claude/remediation-batch-1804-namespace-b1yjh5`. Audit numbering is
**frozen at 364**; nothing in this document creates an `SR-AUD-*` identifier. Post-audit defects
found here take ordinary ticket numbers only (#2191, #2192, #2193, #2194).*

---

## 1. Work unit 1 — why `net-network-information`, verified rather than inherited

`audit/AUDIT_FINDINGS_INDEX.md` was re-parsed from scratch (all 364 rows, grouped by owning
module). The decomposition measured at the start of this batch is:

| Bucket | Count |
|---|---:|
| `remediated` | **167** |
| `confirmed` (plain) | **143** |
| `confirmed (design-complete)` | **54** |
| **total** | **364** |

`confirmed` + `confirmed (design-complete)` = **197 open**. This agrees exactly with the previous
batch's own paragraph at the top of `NEXT.md` (167 / 197, of which 54 design-complete). **The
inherited triple was not stale this time** — unlike the `time-zone` batch, where the prompt's
figures had to be corrected. The prompt for this batch deliberately declined to state a
decomposition and asked for a recount; the recount confirms the previous handoff.

### 1.1 The inherited selection, checked claim by claim

The handoff nominated `net-network-information` on five claims. All five were re-measured here
(`build-probe/2187_probe1_surface.log`, `build-probe/2187_probe2_resources.log`) before anything
changed:

| Inherited claim | Verdict | Evidence |
|---|---|---|
| Only three findings | **true** | index rows 267–269; no other row names this module |
| All three have concrete current-code evidence | **true** | §6 |
| No `/rv` dependency | **true** for the repairs; **false** for one deferred question | §17 |
| Independent of blocked #1962 | **true**, and now proved rather than asserted | §5, §6 |
| Bounded enough to close completely | **true for all three findings**; two post-audit items remain | §4, §18 |

### 1.2 Candidate scoring, recomputed against the alternatives

Candidates are units with **no review plan and no remediated finding**.

| Candidate | Open | high | Compatible-actionable here | Blocked | Approval-gated | Evidence-deferred | Memory/lifetime risk | Public-input exposure | Platform/reference data available | Cohesion | Review status |
|---|---:|---:|---:|---:|---:|---:|---|---|---|---|---|
| **`net-network-information`** | **3** | **0** | **3** | **0** | **0** | 0–1 | low | medium | **yes** — POSIX sockets, `/proc`, `/sys`, resolver all present | **high** — one type (`Ping`) carries all three | none |
| `xml-linq` | 4 | 1 | 3 | **1** — its high **is** CCF-019 (#1899/#1894), design-complete and blocked | 0 | 1 may touch unminted CCF-021 | high, but that half is the blocked half | high | n/a | high | none |
| `globalization` | 7 | 1 | ~3 | 0 | **1** (`Calendar` abstract shape, 82 tests pin it) | **3** — need ICU collation/grapheme data absent here | **high** (TSan-confirmed culture race) | high | **no** | medium | none |
| `io-isolated-storage` | 1 | 1 | 1 | 0 | 0 | 0 | medium (path escape) | high | yes | tiny | none |
| `text-regular-expressions` | 1 | 1 | ~0 | 0 | likely (lifetime) | 0 | high | medium | yes | tiny | none |
| `collections-object-model` | 1 | 1 | ~0 | 0 | likely (lifetime) | 0 | high | low | yes | tiny | none |

**Confirmed selection: `net-network-information`.** It remains the only unreviewed unit with
**zero blocked, zero approval-gated** findings where every finding is actionable in this container.
`xml-linq` has the larger raw count but its one high-severity finding **is** CCF-019, which is
design-complete and blocked, so a review there would mostly defer — exactly the outcome the brief
says to avoid. `globalization` is larger still and would defer three of seven on missing ICU data.
Per the brief, a small fully decidable correctness unit is preferred over a larger namespace that
would mostly defer, and that is what the numbers say.

---

## 2. Namespace scope and file inventory

`modules/net-network-information` — component `Net.NetworkInformation`, target
`sharp_runtime_net_network_information`, `PUBLIC_DEPENDENCIES ComponentModel Core.Base Net
Threading.Tasks`. 23 tracked files.

| Kind | File | Lines | Findings |
|---|---|---:|---|
| header | `IPStatus.hpp` | — | — |
| header | `NetworkAddressChangedEventHandler.hpp` | — | — |
| header | `NetworkAvailabilityChangedEventHandler.hpp` | — | — |
| header | `NetworkAvailabilityEventArgs.hpp` | — | — |
| header | `NetworkChange.hpp` | — | — |
| header | `NetworkInformationException.hpp` | — | named by SR-AUD-254 |
| header | `NetworkInterface.hpp` | — | — |
| header | `NetworkInterfaceComponent.hpp` | — | — |
| header | `NetworkInterfaceType.hpp` | — | — |
| header | `OperationalStatus.hpp` | — | — |
| header | `PhysicalAddress.hpp` | — | — |
| header | `Ping.hpp` | 99 | named by SR-AUD-253, SR-AUD-255 |
| header | `PingException.hpp` | 25 | named by SR-AUD-254 |
| header | `PingOptions.hpp` | 48 | — |
| header | `PingReply.hpp` | 51 | named by SR-AUD-255 |
| body | `NetworkInterface.cpp` | 204 | — |
| body | `PhysicalAddress.cpp` | 149 | — |
| body | **`Ping.cpp`** | **429** | **owns all three findings** |
| test | `NetworkInformationSupportTests.cpp` | 153 | — |
| test | `NetworkInterfaceTests.cpp` | 65 | — |
| test | `PingTests.cpp` | 70 | named by SR-AUD-253 |
| meta | `CMakeLists.txt`, `README.md` | 9 + 9 | — |

**All three findings live in one 429-line body and are reachable only through one type.** That is
the cohesion the selection rested on, and it held.

---

## 3. Complete public-surface inventory

The brief lists a generic `System.Net.NetworkInformation` surface. The **actual** repository
surface is narrower, and the plan uses the repository as authority — nothing below is invented.

### 3.1 Present

| Type | Kind | Public surface |
|---|---|---|
| `Ping` | class | 8 × `Send`, 8 × `SendPingAsync`; no data members (`sizeof(Ping) == 1`, measured) |
| `PingReply` | class | `getStatusProperty`, `getAddressProperty`, `getRoundtripTimeProperty`, `getOptionsProperty` (`const std::optional<PingOptions>&`), `getBufferProperty` |
| `PingOptions` | class | `PingOptions()`, `PingOptions(ttl, dontFragment)`, `get/setTtlProperty`, `get/setDontFragmentProperty` |
| `PingException` | class | `: InvalidOperationException`; `(message)`, `(message, exception_ptr inner)` |
| `NetworkInformationException` | class | `: ComponentModel::Win32Exception`; `()` (from `errno`), `(intcs errorCode)`, `(message)`, `getErrorCodeProperty` |
| `IPStatus` | enum | `Success = 0`, `TimedOut = 11010`, `Unknown = -1`, unreachable-family values |
| `NetworkInterface` | class | `GetAllNetworkInterfaces`, `GetIsNetworkAvailable`, `get{IPv6,}LoopbackInterfaceIndexProperty`, `Supports`, name/type/status/speed/multicast/physical-address properties |
| `NetworkInterfaceType`, `OperationalStatus`, `NetworkInterfaceComponent` | enums | value-pinned by `NetworkInformationSupportTests` |
| `PhysicalAddress` | class | `None`, ctor from bytes, `Parse`, `GetAddressBytes`, `ToString`, equality, hash |
| `NetworkChange` | class | address/availability change event registration |
| `NetworkAvailabilityEventArgs`, the two handler aliases | — | — |

### 3.2 Absent — recorded so the review is not read as claiming coverage it does not have

`IPInterfaceProperties`, `IPv4InterfaceProperties`, `IPv6InterfaceProperties`,
`UnicastIPAddressInformation`, `GatewayIPAddressInformation`, `MulticastIPAddressInformation`,
`IPGlobalProperties`, TCP/UDP connection and listener statistics, and the DNS-suffix surface are
**not ported**. They are named in the batch brief's generic list; none exists in this repository and
**none is invented here**. Interface enumeration is `NetworkInterface::GetAllNetworkInterfaces`
over `getifaddrs` + `/sys/class/net/<name>/speed`, Linux-only, throwing
`PlatformNotSupportedException` elsewhere. The legacy event-based async ping
(`SendAsync`/`SendAsyncCancel`/`PingCompleted`) is deliberately out of scope and already recorded
as such in `Ping.hpp` and `plan.sqlite3`. There is **no cancellation surface** on `Ping` at all, so
the brief's cancellation checks have no target here.

---

## 4. The three findings, dispositions, and the tickets that carry them

**Every finding receives exactly one disposition. None disappears.**

| Finding | Severity | Disposition | Ticket | Outcome |
|---|---|---|---|---|
| **SR-AUD-253** | medium | **compatible implementation** | **#2188** | remediated |
| **SR-AUD-254** | medium | **compatible implementation** | **#2189** | remediated |
| **SR-AUD-255** | medium | **compatible implementation** | **#2190** | remediated |

Post-audit defects found by this review, ordinary ticket numbers only:

| Ticket | What | Disposition |
|---|---|---|
| **#2191** | Both async worker lambdas capture a raw `this` | **compatible implementation** — `Ping` is stateless, `sizeof == 1` |
| **#2192** | A DNS failure escapes the wrapper as `SocketException`; the module's own unresolvable-host `PingException` is practically unreachable | **deferred verification** — needs `/rv` or a managed runtime; current behaviour pinned |
| **#2193** | `sendPingCore` holds a raw descriptor across allocating operations | **compatible implementation** — RAII holder |
| **#2194** | The receive path matches no source/identifier/sequence; every `setsockopt` result is discarded | **blocked** — cannot be exercised while every send fails (#1962) |

---

## 5. Corrected premises

Five corrections, every one measured.

- **SR-AUD-253 reaches all eight `SendPingAsync` overloads, not only the one the audit's probe
  names.** The audit prints one case (`negativeTimeout`). Measured here: **ten probes across all
  eight overloads** — negative timeout, oversized buffer, wildcard address and empty host name —
  every one returns a task and faults later, while the corresponding synchronous door throws at the
  call. There is no partially-correct async door.
- **SR-AUD-253 has a resource consequence the audit states as a possibility and this review
  measures.** `TaskT`'s callable constructor uses `std::async(std::launch::async, …)`, i.e. a real
  OS thread per task. Eight `SendPingAsync(addr, -1)` calls started **real worker threads** (peak
  sampled delta 2 above a 1-thread baseline; the sampling under-counts because each worker
  immediately faults and exits). A caller that validates by calling gets one thread per rejected
  argument.
- **SR-AUD-254's inner exception is not merely "less specific" — the native error code is
  destroyed.** The concrete object thrown is `NetworkInformationException`, `what() == "Win32 error
  13"`, `getErrorCodeProperty() == 13`. What survives the wrapper is `St9exception` / `"std::
  exception"` / no code at all. The cause is `std::make_exception_ptr(e)` where `e` is bound to the
  `std::exception` **base subobject**: `make_exception_ptr` copy-constructs its argument's *static*
  type, so the derived object is never copied. This is slicing at the point of capture, not at the
  point of rethrow.
- **SR-AUD-254 has a second door the audit does not name, and it is the opposite defect.** A DNS
  failure is raised **outside** the wrapper (`Dns::GetHostAddresses` is called before
  `sendWithExceptionWrapping`), so `System::Net::Sockets::SocketException` escapes `Send(host…)`
  verbatim. The module simultaneously declares `PingException("Could not resolve host name or
  address.")` on the next line for an empty address list — a branch `Dns::GetHostAddresses` makes
  practically unreachable, because it throws on every resolver failure. **Under-wrapping and
  over-wrapping of the same conceptual failure, three lines apart.** Which one .NET produces is
  #2192, deferred.
- **SR-AUD-255 is three sites, and the fourth sibling is already correct.** `Ping.cpp:351`, `:404`
  and `:410` fabricate `PingOptions()`; `Ping.cpp:357` (`Send(addr, timeout, buffer)`) already
  forwards `nullptr`. The audit's summary says "string synchronous and all no-options async paths",
  which is right; the useful correction is that the **repair target is a self-inconsistency inside
  one file**, so it needs no .NET reference to justify.

---

## 6. Reproduction — every finding, measured before any change

`build-probe/2187_probe1_surface.cpp` → `build-probe/2187_probe1_surface.log`, one process, before
any source changed.

| Finding | Reproduced | Needs a socket? | Result |
|---|---|---|---|
| SR-AUD-253 | **yes**, 10/10 doors | **no** | `NO sync throw; task FAULTED with ArgumentOutOfRangeException` / `ArgumentException` |
| SR-AUD-254 | **yes**, 9/10 wrapping doors | yes — it is reached *because* the socket fails | `inner: St9exception : std::exception`, `SLICED = 1` |
| SR-AUD-255 | **structurally only** | yes | every door threw `PingException`; the reply-side consequence needs a reply |

### 6.1 The socket capability, measured — this is #1962's territory and stays there

```
socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP) = -1  errno=13 (Permission denied)
socket(AF_INET, SOCK_RAW,   IPPROTO_ICMP) = 3
ping_group_range = 1	0
```

Identical to the inherited state, re-measured rather than trusted. `ping_group_range = "1 0"` is an
empty range (low > high), so **no** group may open an unprivileged ping socket, and the runtime
opens only `SOCK_DGRAM`/`IPPROTO_ICMP` (`Ping.cpp:148`). `SOCK_RAW` succeeds here. **That gap is
#1962 and this review does not touch it.**

### 6.2 Why SR-AUD-254 is not an artefact of #1962

The wrapper's defect is unconditional: `std::make_exception_ptr(e)` slices whatever was thrown, on
every platform, for every cause. The denied socket is merely the cause that happens to be reachable
in this container — it supplies a concrete `NetworkInformationException(13)` for the wrapper to
destroy. Under #1962's eventual fix the same wrapper would destroy whatever the raw-socket path
throws instead. **Repairing #2189 changes nothing about which socket is opened, and leaves every
`PingTests` failure exactly where it was.**

---

## 7. Shared root causes

Three findings, **two** root causes, neither of which is "ICMP is denied".

- **Root cause A — the forwarding layer collapses two distinct argument shapes into one.** Eight
  `SendPingAsync` overloads funnel into two four-argument overloads that take `const PingOptions&`,
  and every convenience overload therefore has to invent an options value to call them (SR-AUD-255),
  while the funnel itself does all its work inside the worker lambda (SR-AUD-253). One structural
  change — a file-local core that takes `const PingOptions*` and is called after synchronous
  validation — removes both, and removes the raw `this` capture (#2191) as a side effect, because
  the worker no longer needs an object to call a member on.
- **Root cause B — the exception boundary captures by static type.** `catch (const std::exception&
  e)` + `std::make_exception_ptr(e)` is a boundary that can only ever transmit `std::exception`
  (SR-AUD-254). The same boundary is also drawn in the wrong *place* — `Dns` is outside it (#2192).

---

## 8. Cross-cutting family check — no new CCF, and none is pre-empted

- **The raw-`this` capture (#2191) does not join the blocked stateful family** (#2066 `HttpClient`,
  #2088/#2096 `ClientWebSocket`, #2134 `Socket`, SR-AUD-263/310). Those owners hold real state, so
  the repair needs an ownership decision the repository has deliberately gated. `Ping` holds
  **none** — `sizeof(Ping) == 1`, measured — so the capture is deletable outright with no ownership
  question. Fixing it here **does not close, weaken or pre-empt** the blocked family, and this
  review mints no CCF for it.
- **CCF-019 is untouched and remains open.** No borrowed-view lifetime boundary exists in this
  module: `PingReply` owns its address, options and buffer by value.
- **CCF-021 (#2131) and CCF-022 (#2109) remain unminted.** Nothing here is a protocol-field
  terminator or an embedded-NUL occurrence.
- **CCF-004 gains no member.** No arithmetic in this module is undefined; the checksum accumulates
  in `uint32_t` and folds, and the identifier/sequence counters are `uint16_t`.

---

## 9. Dependency graph between the tickets

```
#2187 (review, this document)
  ├── #2189  SR-AUD-254   exception boundary        — independent, lands first
  ├── #2188  SR-AUD-253   synchronous validation  ─┐
  ├── #2190  SR-AUD-255   options absence         ─┼─ one rewrite of the forwarding layer
  ├── #2191  raw `this` capture                   ─┘
  ├── #2193  RAII descriptor                        — independent, lands last
  ├── #2192  DNS wrapping question                  — deferred, pinned only
  └── #2194  receive-path matching / setsockopt     — blocked on #1962's testability
```

`#2188`, `#2190` and `#2191` **must land together**: each one alone would leave the forwarding
layer half-converted, and the file-local `const PingOptions*` core that #2190 needs is the same core
#2188 must validate in front of and #2191 must call instead of a member on `this`.

---

## 10. Severity

All three findings are `medium` in the index, and this review does not re-grade any of them.
Reasons, restated from the measured behaviour:

| Finding | Why medium and not high | Why medium and not low |
|---|---|---|
| SR-AUD-253 | No memory unsafety; the exception type and message are already correct, only their delivery is wrong | Changes caller control flow, and starts an OS thread per invalid argument |
| SR-AUD-254 | No memory unsafety; the outer exception type and message are correct | Destroys the entire diagnostic payload of every Ping failure — a caller cannot tell permission-denied from network-unreachable |
| SR-AUD-255 | One-directional: the port can only report options the caller did not supply, never drop options they did | Silently applies a TTL the caller never asked for, and misreports the reply |

---

## 11. Compatible / gated / deferred matrix

| Item | Compatible | Gated | Deferred | Blocked | Rationale |
|---|:--:|:--:|:--:|:--:|---|
| #2188 SR-AUD-253 | **✔** | | | | Same exception type and message; only the delivery point moves. No signature, layout, vtable or symbol change. |
| #2189 SR-AUD-254 | **✔** | | | | The outer `PingException` type and message are unchanged; only the inner exception gains back its real type. No public declaration changes. |
| #2190 SR-AUD-255 | **✔** | | | | Behaviour change **toward** .NET on a path the audit confirms is wrong, on doors that currently fabricate. §12 records the two consequences. No public declaration changes. |
| #2191 raw `this` | **✔** | | | | `sizeof(Ping) == 1`; removing a capture from a lambda in a `.cpp` body. |
| #2193 RAII fd | **✔** | | | | File-local holder in a `.cpp` body; no behaviour change on any non-throwing path. |
| #2192 DNS wrapping | | | **✔** | | Needs `/rv` or a managed runtime; both absent. Current behaviour pinned. |
| #2194 receive matching | | | | **✔** | Needs a working ICMP send. That is #1962, which stays blocked. |
| #1962 raw-socket fallback | | | | **✔** | Explicitly out of scope for this batch. |

---

## 12. Source / ABI / layout / vtable / `noexcept` consequences

**None of the five implemented tickets changes any of them.**

| Property | Before | After |
|---|---|---|
| `Ping.hpp` | 99 lines | **byte-identical — not edited** |
| `PingReply.hpp`, `PingOptions.hpp`, `PingException.hpp`, `NetworkInformationException.hpp` | — | **byte-identical — not edited** |
| `sizeof(Ping)` | 1 | 1 |
| Virtuals / vtables | none in `Ping`/`PingReply`/`PingOptions` | unchanged |
| Mangled symbols | 16 public `Ping` members | unchanged — every edit is inside `Ping.cpp` or file-local |
| `noexcept` | none declared | unchanged |
| Component graph | 41 modules / 92 edges | unchanged |

Every repair is confined to `modules/net-network-information/src/System/Net/NetworkInformation/Ping.cpp`
plus tests. **A consumer needs no rebuild for layout reasons.**

### 12.1 The two behaviour consequences of #2190, stated explicitly

1. **`PingReply::getOptionsProperty()` reports absence** where it used to report a default
   `PingOptions{ttl = 128, dontFragment = false}`, for `Send(host, timeout, buffer)`,
   `SendPingAsync(addr, timeout, buffer)` and `SendPingAsync(host, timeout, buffer)`. Callers that
   dereferenced the optional unconditionally on those three doors must check `has_value()` — which
   they already had to do for the fourth door, `Send(addr, timeout, buffer)`, which has always
   returned absence.
2. **The socket no longer has TTL forced to 128** on those three doors; it keeps the system default
   (64 on Linux). This is the on-the-wire half, and it is the behaviour .NET has: options are
   applied only when the caller supplies them.

Both changes are one-directional toward .NET and toward the module's own already-correct fourth
door. No existing test asserts the old behaviour.

---

## 13. Exception-boundary consequences

| Door | Before | After |
|---|---|---|
| Every `Send` that reaches the socket and fails | `PingException` + inner `std::exception` (`"std::exception"`, no code) | `PingException` + inner **`NetworkInformationException`** (`"Win32 error 13"`, `getErrorCodeProperty() == 13`) |
| A cause that is not a `std::exception` | **dropped** — the `catch (const std::exception&)` never sees it, it propagates unwrapped | preserved: caught by `catch (...)` and stored with `std::current_exception()` |
| `PlatformNotSupportedException` | rethrown unwrapped | **unchanged** — still rethrown unwrapped |
| `Send(host…)` with an unresolvable host | `SocketException`, unwrapped | **unchanged** — pinned, question deferred to #2192 |
| Every `SendPingAsync` with an invalid argument | task faults with `ArgumentOutOfRangeException`/`ArgumentException` | **throws the same type with the same message, synchronously** |

`std::current_exception()` inside a handler returns an `exception_ptr` to the **currently handled
exception object**, not a copy of the handler's parameter, so the dynamic type, the message and the
native error code all survive. Lifetime is owned by the `exception_ptr`'s shared state and outlives
the `catch` block; nothing borrows the handler's frame.

---

## 14. Platform consequences

| Platform | Effect |
|---|---|
| Linux/POSIX | The five repairs are all in the platform-independent forwarding layer or in the `SHARP_RUNTIME_PING_POSIX` core; behaviour as §12/§13. |
| Windows | No PAL exists; `sendPingCore` throws `PlatformNotSupportedException` and the wrapper still rethrows it unwrapped. **#2188's synchronous validation now runs before that throw**, so an invalid argument is reported as an argument error on Windows too rather than as "not supported". That is the .NET ordering. |
| Emscripten | Same as Windows. |
| BSD/Darwin | The `struct icmp` branch is compiled but not executed here; none of the five repairs touches it. |

No platform gains or loses a supported operation. **`/proc/net/if_inet6` is absent in this
container** (the one `SocketTests` failure, a different module) and `getifaddrs` +
`/sys/class/net/*/speed` are present and were used by `NetworkInterfaceTests`, which passes.

---

## 15. Resource and lifetime consequences

| Aspect | Before | After |
|---|---|---|
| OS threads for an invalid async argument | **one per call** (`std::async(std::launch::async, …)`) | **zero** — the throw happens before the task is constructed |
| Descriptors on a reachable failure | balanced (measured delta 0 over 1000 sends) | balanced |
| Descriptors on a throwing allocation between `socket()` and `close()` | **leaked** | released by the RAII holder (#2193) |
| `this` captured by a worker lambda | **yes**, both four-argument async overloads | **no** — the worker calls file-local functions |
| `PingReply` ownership | by value throughout | unchanged |

`Ping` has no `Dispose`, no cancellation and no concurrent-operation contract, so the brief's
disposal-during-operation, cancellation and completion-after-owner-destruction checks have exactly
one applicable target: the `this` capture, which #2191 removes.

---

## 16. Test matrix

| # | Test | Pins | Deterministic here? |
|---|---|---|---|
| 1–8 | `SendPingAsync_*_ThrowsSynchronously` — negative timeout × 4 overload families, oversized buffer, wildcard address, empty host, IPv6 wildcard | SR-AUD-253 at every async door | **yes** |
| 9 | `SendPingAsync_InvalidArgument_StartsNoWorker` | the thread consequence | **yes** |
| 10 | `SendPingAsync_ExceptionTypeMatchesSyncDoor` | the exception type/message did not drift | **yes** |
| 11 | `Send_WrappedFailure_PreservesConcreteInnerException` | SR-AUD-254: inner is `NetworkInformationException`, not `std::exception` | **yes** — needs a *failing* socket, which is what this container has |
| 12 | `Send_WrappedFailure_PreservesNativeErrorCode` | the native code survives | **yes** |
| 13 | `Send_PlatformNotSupported_IsNotWrapped` | the pass-through arm | compile-guarded |
| 14 | `Send_UnresolvableHost_ThrowsSocketExceptionUnwrapped` | #2192's current behaviour, so the deferred question cannot be answered silently | **yes** |
| 15–18 | `PingReply_AbsentOptions_*` | SR-AUD-255's mechanism: absence is representable and distinct from a default | **yes** |
| 19 | `NoOptionsDoors_ReportAbsentOptions` | SR-AUD-255 end-to-end | **guarded** — skips when ICMP is unavailable, §17 |
| 20 | `Send_DescriptorsBalanced` | #2193 | **yes**, with a deliberate leaked-fd control |

**Why test 19 is guarded and the five existing `PingTests` are not.** The five existing failures are
the repository's standing evidence for #1962 and the brief forbids weakening them; they stay
failing. A *new* test whose only purpose is to observe a reply cannot assert anything here, and
adding a sixth deliberate failure would blur the gate's meaning. It therefore skips with a message
naming #1962, and the honest consequence — that SR-AUD-255's end-to-end half is unobserved in this
container — is stated here, in §17 and in the batch record rather than papered over.

---

## 17. Missing evidence — what is deferred and exactly why

- **#2192, whether .NET wraps a DNS failure in `PingException`.** `/rv` is absent and no managed
  runtime is installed. Both readings are defensible from this repository alone, and the module
  contains both. Current behaviour is pinned by test 14.
- **SR-AUD-255's end-to-end half.** Requires one successful ICMP round trip. Blocked by #1962 by
  construction. The structural half — that absence is forwarded and that `PingReply` represents it
  distinctly — is pinned deterministically by tests 15–18.
- **#2194 in full.** Requires a successful send.
- **Exact .NET resource strings.** Not verified for any message this review touches; no message is
  changed by any of the five repairs, so no new divergence is introduced.
- **Windows/Emscripten/BSD behaviour** is read from the source, compiled, and not executed.

---

## 18. Exclusions — what this review deliberately does not do

- **#1962 is not implemented.** No raw-socket fallback is added. No `SOCK_RAW` path enters
  production code. The five `PingTests` failures are not weakened, disabled, skipped or
  recategorized, and are not reclassified as environment-only.
- **No `SR-AUD-*` identifier is created.** Audit numbering stays frozen at 364.
- **No CCF is minted or closed.** CCF-019 stays open; CCF-021/#2131 and CCF-022/#2109 stay unminted.
- **No blocked, `needs_user`, design-only or deferred ticket is implemented** — #2185, #2186, #2170,
  #2172, #2175, #2150, #2152, #2155, #2166, #2134, #2138, #2131, #2109 and #1773 are untouched.
- **No approval is consumed.** This batch requests none and spends none.
- **No absent .NET type is invented** — §3.2.
- **No system networking configuration is modified**, and no test requires external internet
  reachability.

---

## 19. Completion criteria

`modules/net-network-information` is **closed except for exactly two recorded items** when:

1. SR-AUD-253, SR-AUD-254 and SR-AUD-255 are all `remediated` in `audit/AUDIT_FINDINGS_INDEX.md`
   and in their per-file report. ✔ (§4)
2. #2188, #2189, #2190, #2191 and #2193 are `done`, each with permanent discriminating tests. ✔
3. #2192 is recorded `todo` as deferred verification with the current behaviour pinned, and #2194 is
   recorded `blocked` on #1962's testability. ✔
4. No public signature, layout, vtable, `noexcept` or mangled symbol changed. ✔ (§12)
5. #1962 is untouched and still blocked, and the six known gate failures are unchanged. ✔
6. The full 37-executable gate shows no additional failure. ✔

**The namespace is therefore "closed except for #2192 (deferred verification) and #2194 (blocked on
#1962)", not "fully closed"** — and that is the measured outcome, not a shortfall: both remainders
need evidence this container cannot produce, and neither is a finding the audit raised.
