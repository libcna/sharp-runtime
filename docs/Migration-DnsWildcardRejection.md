<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Dns::GetHostAddresses` rejects the unspecified addresses (ticket #2043)

*2026-08-17.* `Dns::GetHostAddresses("0.0.0.0")` returned the wildcard address. .NET raises
`ArgumentException`.

Landed under `docs/StandingApprovals.md` SA-5. No public signature, layout, vtable or `noexcept`
change. **This removes a result that previously worked** — read §2.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `GetHostAddresses("0.0.0.0")` | `{0.0.0.0}` | `ArgumentException` |
| `GetHostAddresses("::")` | `{::}` | `ArgumentException` |
| the same with an explicit `AddressFamily` | resolved | `ArgumentException` |
| `GetHostAddresses("127.0.0.1")`, `"::1"`, `"8.8.8.8"`, `"0.0.0.1"` | — | **unchanged** |

The rejection comes **before** the address-family check, matching `Dns.cs:686-690`, which tests
the wildcard immediately after `TryParse` and before anything else looks at the value. So
`GetHostAddresses("0.0.0.0", InterNetworkV6)` is an `ArgumentException`, not a `SocketException`
about the family — and a test asserts that ordering.

## 2. Why a working result is being removed

This ticket was split out of #2039 precisely because it is the one half of SR-AUD-304 that takes
away something that worked, so it needed evidence rather than judgement. The reference supplies
it in three places — `Dns.cs:686-690` on the string path, and `:46-50` and `:158-162` on the
`IPAddress` overloads — all raising `ArgumentException(SR.net_invalid_ip_addr)`.

.NET's message says why, and the port transcribes rather than paraphrases it:

> IPv4 address 0.0.0.0 and IPv6 address ::0 are unspecified addresses that cannot be used as a
> target address.

They name *every local interface* to `bind`, and *nothing at all* to `connect`. Resolving one to
itself hands the caller a target it cannot use.

## 3. To migrate

If you passed `0.0.0.0` or `::` to `GetHostAddresses`, you were about to connect to an address
that cannot be connected to. Use `IPAddress::Any` / `IPAddress::IPv6Any` directly for a `bind`,
where they mean what you want, and do not route them through name resolution.

## 4. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `System::Net::Dns` — **zero sites in both**.
Neither repository was modified.
