<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Ping` correlates its reply and reports a refused socket option (ticket #2194)

*2026-08-19.* `sendPingCore` accepted the first datagram on its socket without correlating it to
the request, and every `setsockopt` discarded its return value, so an option the kernel rejected
was silently not applied.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change.

---

## 1. The blocker was environmental and is gone

#2194 was blocked as unexercisable: *"every send fails at socket creation because
`ping_group_range` is `1 0`"*. This container's range is now **`0 2147483647`**, a
`SOCK_DGRAM/IPPROTO_ICMP` socket opens, and a real loopback round trip succeeds. Measured before
implementing anything.

The ticket's own instruction — *"Do not implement a raw-socket fallback to unblock it, that is
#1962"* — is respected: nothing here touches socket creation.

## 2. The acceptance criterion is wrong on two of its three fields

It asks that *"a reply is accepted only when its **source**, **identifier** and **sequence** match
the request"*. Measured:

* **Identifier — cannot be matched.** On a Linux ping socket the kernel **rewrites** the ICMP
  identifier. Probed directly: a request written with `id 0x1234` came back as `0x94d4`, the
  socket's own port. .NET checks the identifier (`Ping.RawSocket.cs:230`) because its raw-socket
  path writes an id the kernel leaves alone; here that check would reject **every** reply.
* **Source address — must not be matched.** .NET does not check it either; it reports
  `socketConfig.EndPoint.Address`, the address it *sent to* (`:245`). Requiring the source to
  equal the destination would reject the legitimate error replies that come from an intermediate
  router — `TimeExceeded` above all, which is exactly what a caller setting a low `Ttl` is asking
  for.
* **Sequence — matched.** It survives the ping socket unchanged (probed: `0x5678` out, `0x5678`
  back), it is ours, and it is the field that distinguishes this request from another.

An ICMP **error** quotes the original request after its own header, so the sequence is read from
the quoted request — the same *"original IP+ICMP request is in the payload"* rule .NET follows
(`Ping.RawSocket.cs:166-190`).

## 3. What changed

| | Was | Is |
|---|---|---|
| a datagram that does not correlate | reported as this request's answer | skipped; reading continues |
| the read timeout across skips | — | the **original** deadline, never restarted |
| a `setsockopt` the kernel refuses | silently ignored | `NetworkInformationException` |
| `Ttl = 256` (legal to `PingOptions`, illegal to the kernel) | ignored, normal reply | reported |
| an ordinary reply | — | **unchanged** |

`Ttl = 256` is the reachable door and that is measured, not guessed: `PingOptions` rejects only
`ttl <= 0` — which is .NET's own bound — so `0` never reaches a socket, while `256` passes the
type and `IP_TTL` refuses it with `EINVAL`.

## 4. The correlation is defensive on this platform, and the mutation pass is what established it

Four mutations, two caught:

| Mutation | Result |
|---|---|
| `setsockopt` failures discarded again | caught |
| the matcher compares the **identifier** instead of the sequence | caught — **8 tests**, every live one |
| **the correlation is computed and ignored** | **NOT caught** |
| **the deadline restarts on every foreign datagram** | **NOT caught** |

**The third is the useful one, and it exposed a vacuous test of mine.** A first case claimed to
prove the skip by putting an unrelated echo on the wire. It proves nothing: a Linux ping socket is
demultiplexed by the identifier the **kernel** assigned, so a reply belonging to another socket is
never queued on ours. A foreign datagram cannot arrive here at all.

So the check is **defensive rather than load-bearing on this platform**. It is kept, and the
reason is specific: **#1962** would add a raw-socket fallback, and a raw ICMP socket receives
*every* ICMP datagram on the host — at which point this becomes essential and its absence would be
a live defect instead of a latent one. The test was rewritten to assert what it actually
establishes, and both the test and `Ping.cpp` say so.

**The fourth cannot be caught deterministically.** Observing a restarted timeout needs a
*sustained* flood of foreign datagrams plus a wall-clock assertion — and under the mutation the
call would not fail but **hang**, so the test would be both flaky and useless, the shape #2352 and
#2105 were repaired for. The original deadline is kept because a busy or hostile socket must not
extend this call without bound, which is the same class of defect #2032 removed from
`WaitForExit`.

## 5. Downstream, measured

`cna` and `mobile-eggbert` reference `System::Net::NetworkInformation::Ping` in **zero** code
sites. Neither was modified.
