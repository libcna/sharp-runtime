<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `TcpClient`, `TcpListener` and `UdpClient` carry IPv6 (ticket #2363)

*2026-08-19.* The three high-level socket wrappers were `AF_INET` with a hand-built
`sockaddr_in` on every connect, bind, accept and receive path. They now carry either family, on
.NET's rules. Ticket **#2138**'s unconditional IPv6 refusal is **removed**, not made unreachable.

No public signature was removed and nothing needs recompiling: `sizeof(TcpClient)`,
`sizeof(TcpListener)` and `sizeof(UdpClient)` are all **unchanged** (§4). Two accessors were
added. Landed under SA-3 and SA-5.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `TcpClient(IPEndPoint(v6, 0))` | `ArgumentException` | binds an `AF_INET6` socket |
| `TcpListener(v6, port)`, `TcpListener(IPEndPoint(v6, …))` | `ArgumentException` at construction | listens on IPv6 |
| `TcpClient::Connect(IPEndPoint(v6, …))`, unbound | `ArgumentException` | connects |
| `TcpClient::Connect("::1", port)` | `SocketException(HostNotFound)`, *"DNS failed"* | connects |
| `TcpClient::Connect(host, port)` with several addresses | only the **first** was tried | **every** result is tried in turn |
| `TcpListener::AcceptTcpClient()` | peer family always `InterNetwork` | the peer's real family |
| `UdpClient(IPEndPoint(v6, 0))` | `ArgumentException` | binds an `AF_INET6` socket |
| `UdpClient::Receive` from an IPv6 sender | a **fabricated** IPv4 address (§3) | the real sender |
| `UdpClient()`, `UdpClient(port)` | IPv4 | **IPv4 — unchanged, see §2.3** |
| `UdpClient::Connect(IPEndPoint)` of the wrong family | `ArgumentException` | `ArgumentException` — same sentence, narrower condition |
| every IPv4 path | — | **unchanged** |

`IPV6_V6ONLY` is left at the operating system's default, as .NET does — it is exposed there as
`Socket.DualMode` and never forced at construction (`Socket.cs:745-770`). On Linux that default
is `net.ipv6.bindv6only = 0`, so an IPv6 listener also accepts IPv4 peers.

## 2. Three premise corrections

The ticket's description disagreed with the reference in three places.

**2.1 .NET does not simply "resolve with `AF_UNSPEC` and walk the list".** Half of that is right.
`Socket.Connect(string host, int port)` calls `IPAddress.TryParse(host)` **first**
(`Socket.cs:919-923`) and only falls back to `Dns.GetHostAddresses` plus the array overload.

**2.2 That settles the question #2138 deferred — and settles it away from #2359.** #2138 left the
hostname door alone because *"deciding whether `::1` at a hostname parameter is a literal or a
name belongs with #2359"*. Measured, it does not: a URI authority has bracket syntax and a socket
hostname parameter does not, and **.NET answers the socket question inside the socket code**.
`getaddrinfo` with `AF_UNSPEC` parses a literal of either family natively, so this port reaches
.NET's answer without a second address parser. **#2359 is untouched by this ticket.**

**2.3 Not every `AF_INET` constant should have gone.** The ticket said the family must come "from
the endpoint or the resolved result rather than a constant" throughout. For `UdpClient()` and
`UdpClient(int port)` the constant **is** .NET: `public UdpClient() : this(AddressFamily.
InterNetwork)` (`UDPClient.cs:24`) and `public UdpClient(int port) : this(port,
AddressFamily.InterNetwork)` (`:47`). Only `TcpClient`'s parameterless path is dual-mode
(`TCPClient.cs:368-383`). Making UDP's default dual-stack would be a deviation dressed as a
repair; a test pins that it was not made.

## 3. The one defect that really was silent

#2138 measured the family limitation twice and found *nothing was ever silently narrowed* — every
path failed loudly. That holds for the send and bind paths, and it does **not** hold for
`UdpClient::Receive`, which #2138 did not probe. It read the sender through a `sockaddr_in`
overlaid on a 28-byte `sockaddr_in6`: the four bytes at the IPv4 address offset land **inside** the
IPv6 address, so a caller received a plausible, entirely fabricated IPv4 address for a datagram
that came from somewhere else. That is the one place the old code answered rather than refused.

## 4. Cost, measured

`TcpClient` and `UdpClient` each needed to remember their family — .NET carries the same state
(`private AddressFamily _family;`, `TCPClient.cs:17`; `UDPClient.cs:21`). **The first cut stored an
`AddressFamily` and grew `sizeof(TcpClient)` from 24 to 32**: the three bytes of padding after
`connected_` cannot hold a 4-byte, 4-aligned enum, so the scalar block rounds up and the
`shared_ptr` moves. The shadow-struct layout pin is what caught it.

It is stored as a `bool` instead, which fits and gives up nothing: this port's own `IPAddress` is
`bool isIPv6_` with the family **computed** (`IPAddress.cpp:326-328`), so neither type can carry a
third family however the state is spelled. The public accessors still return `AddressFamily`.
Every `sizeof` is therefore unchanged and **no consumer must rebuild**.

## 5. Where the refusal survives

A socket that **already exists** has a family, and an endpoint of another family cannot be used
with it. That is `Socket.cs:1757-1759`, and it is the only surviving refusal:

* `UdpClient::Connect(IPEndPoint)` — a `UdpClient` always owns a socket;
* `TcpClient::Connect(IPEndPoint)` on a client **bound** to a local endpoint.

The sentence is unchanged from #2138 — *"The supplied EndPoint of AddressFamily {0} is not valid
for this Socket, use {1} instead."* — and both families are now filled in rather than one being a
literal. A door that *creates* the socket from the endpoint it was handed does not check at all.

`UdpClient::Connect(hostname, port)` filters the resolved addresses to the ones its socket can
carry, which is .NET's `IsAddressFamilyCompatible` (`UDPClient.cs:743-745`). A default (IPv4)
`UdpClient` therefore still cannot reach an IPv6-only host — .NET's behaviour, not a residue —
but the diagnosis moved from `HostNotFound` *"DNS failed"* to `AddressFamilyNotSupported` naming
the filter. To reach an IPv6 peer over UDP, construct the client from an IPv6 local endpoint.

## 6. To migrate

Almost nothing. Code that **caught** `ArgumentException` around an IPv6 endpoint no longer sees it
and should drop the handler. Code that relied on `Connect(hostname, port)` reaching only IPv4 —
there is no way to have relied on that deliberately, since it reported the refusal as a DNS
failure — now reaches IPv6 hosts too.

The one behavioural surprise worth naming: `Connect(hostname, port)` now tries every resolved
address, so a call that used to fail fast against an unreachable first address now takes as long
as the addresses it works through before reporting the **last** error.

## 7. Evidence

Nine mutations, eight caught:

| Mutation | Caught by |
|---|---|
| hostname hints back to `AF_INET` | `Fix2363_AnIPv6LiteralAtAHostnameParameterResolves` |
| accept path reports a constant family | `Fix2363_AnIPv6EndpointConnectsBindsAndAccepts` |
| listener socket family back to `AF_INET` | two cases, via the IPv6 availability probe |
| family-agreement check dropped from `UdpClient::Connect` | `Fix2363_TheFamilyRefusalSurvivesOnlyWhereASocketAlreadyExists` |
| `UdpClient`'s default made dual-stack | `Fix2363_UdpClientsDefaultStaysIPv4BecauseDotNetsDoes` + 2 pre-existing |
| resolved-address filter removed from `UdpClient::Connect` | same, via the **message** — both routes give `EAFNOSUPPORT`, so the code alone cannot tell them apart |
| `Receive` forced back to `AF_INET` | `Fix2363_UdpReceiveReportsAnIPv6SenderRatherThanFabricatingOne` |
| `TcpClient::Connect` takes only the head of the list | `Fix2363_TheResolverResultListIsWALKEDNotJustItsHead` |
| **`BuildIPSockAddr` drops the scope id** | **NOT CAUGHT** |

The uncaught one is reported rather than papered over. A scope id is only observable with a
link-local address (`fe80::/10`) on a machine that has a link-local interface, and SA-6 makes a
test that depends on such a machine property a defect in the test rather than evidence. It is not
a regression either: the line is `Socket.cpp`'s own, moved rather than written, and `Socket`'s
behaviour is unchanged by the move.

Three cases needed restructuring during the mutation pass and the reason is worth keeping: a
helper thread parked in `AcceptTcpClient()` turns a failed connect into a **hang**, so the first
mutation was caught only as a timeout. Connecting first and accepting afterwards — the listen
backlog holds the connection — makes every one of them fail by name instead.

Three live-socket cases consult an `IPv6LoopbackAvailable()` probe and assert loudly when this
machine has no IPv6, per SA-6.

## 8. Downstream, measured

`cna` and `mobile-eggbert` were both searched for `TcpClient`, `TcpListener`, `UdpClient` and
`System::Net::Sockets`: **zero code sites in either**. The only two matches are prose inside
`cna/plan_metal.md`. Neither repository was modified.
