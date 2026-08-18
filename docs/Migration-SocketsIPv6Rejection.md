<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the socket clients refuse IPv6 deliberately (ticket #2138)

*2026-08-18.* `TcpClient`, `TcpListener` and `UdpClient` are `AF_INET` only. They always refused
an IPv6 endpoint; now they refuse it **on purpose, at the door, in .NET's own words**.

Landed under `docs/StandingApprovals.md` SA-5. The **exception type changes** at six doors —
`SocketException` → `ArgumentException`. No signature, layout, vtable or `noexcept` change.

---

## 1. The finding's wording did not reproduce

SR-AUD-266 described an IPv6 endpoint as *"silently misrepresented"*. Measured twice —
`build-probe/2139_probe1_v6.log` before the ticket, and
`build-probe/2138_probe1_ipv6doors.cpp` / `build-probe/2138_probe2_listener.cpp` at its tip —
**nothing was ever silently narrowed.** No socket connected over IPv4 while pretending to be
IPv6, and no address was truncated. Every path threw.

This was a **diagnostic** defect, not a correctness one, and the repair is scoped to that. The
gated pin already recorded the correction; this ticket acts on it.

## 2. What was actually wrong — three refusals, none of them the operation's own

1. The four endpoint doors reached `IPAddress::getAddressProperty()`, which raised
   `SocketException(OperationNotSupported)` — *"The requested property is not supported for the
   'InterNetworkV6' AddressFamily."* A caller who asked to **connect** was told about an
   unsupported **property**, and the sentence named no operation, no argument and no remedy.
2. `TcpListener` deferred that same accident to `Start()`, because its constructors only stored
   the endpoint. A caller could hold a fully constructed listener that could never listen.
3. The two **hostname** doors refuse for a third, unrelated reason: `hints.ai_family` is
   `AF_INET`, so `getaddrinfo` never resolves an IPv6 literal and the caller is told *"DNS
   failed"* about a literal address that needs no DNS.

## 3. What changed

| Door | Was | Is |
|---|---|---|
| `TcpClient(localEP)` | `SocketException(OperationNotSupported)` | `ArgumentException`, `localEP` |
| `TcpClient::Connect(remoteEP)` | same | `ArgumentException`, `remoteEP` |
| `TcpListener(localEP)` | **accepted**; threw later from `Start()` | `ArgumentException`, `localEP` |
| `TcpListener(addr, port)` | **accepted**; threw later from `Start()` | `ArgumentException`, `addr` |
| `UdpClient(localEP)` | `SocketException(OperationNotSupported)` | `ArgumentException`, `localEP` |
| `UdpClient::Connect(remoteEP)` | same | `ArgumentException`, `remoteEP` |
| `Connect(hostname, port)` — both | `SocketException(HostNotFound)`, *"DNS failed"* | **unchanged** |
| every IPv4 path | — | **unchanged** |

The message is `Socket.cs:1759` and `Strings.resx:156-158`, transcribed:

> The supplied EndPoint of AddressFamily InterNetworkV6 is not valid for this Socket, use
> InterNetwork instead.

Nothing is invented — the port states truthfully which family its sockets are.

## 4. Two deliberate limits

**The hostname doors are untouched.** Deciding whether `"::1"` arriving at a *hostname* parameter
is a literal or a name is the question ticket **#2359** holds for `System::Uri`. Answering it
incidentally inside a socket door would settle it in the wrong place. Their behaviour stays
pinned.

**The family name is a literal, not a lookup.** `IPAddress::getAddressFamilyProperty()` is
`isIPv6_ ? InterNetworkV6 : InterNetwork` over a single `bool` (`IPAddress.cpp:326-328`), so an
`IPAddress` in this port cannot carry a third family. A general name table would be unreachable
code; a first cut wrote one and it was deleted rather than defended.

## 5. To migrate

If you caught `SocketException` around a socket door to detect the IPv6 limitation, catch
`ArgumentException` instead. If you did not pass IPv6 endpoints, nothing changes.

Full dual-stack — `AF_UNSPEC` with a `getaddrinfo` result walk, as .NET does — is ticket
**#2363**. This change makes the gap honest until then; it is not a substitute for it.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `TcpClient`, `TcpListener` or `UdpClient` —
**zero sites in both**. Neither repository was modified.
