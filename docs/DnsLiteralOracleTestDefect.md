<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# A `Dns` test asserted the resolver's opinion, not the port's (ticket #2375)

*2026-08-18.* `DnsLiteralTests.MalformedLiteralTextIsStillRejected` failed on this container
while the production code was correct. The test was the defect, and it is fixed rather than
disabled, weakened or skipped.

---

## 1. The failure

```
../modules/net/tests/System/Net/DnsLiteralAndDuplicateTests.cpp:102: Failure
Expected: (void)Dns::GetHostAddresses(text) throws an exception of type SocketException.
  Actual: it throws nothing.
1.2.3.
```

Reproducible 5 runs out of 5, and reproducible **with the working tree stashed** — so it is not a
regression from the change that was in flight (#1929, which touches only `modules/core` date and
time parsing and cannot reach `modules/net`).

## 2. What was actually happening

The test asserted that six strings *fail to resolve*. That is not a property of this port. It is
the **resolver's** opinion, and any wildcard DNS server can change it.

Measured on this container:

| Text | Answer | Time |
|---|---|---|
| `"1.2.3"` | `1.2.0.3` | **0.03 ms** — libc's digits-and-dots shortcut, no network |
| `"1.2.3."` | `1.2.0.3` | **13.39 ms** — a DNS round trip to `10.28.9.85` |
| `"definitely-not-a-real-host-xyz123."` | fails | 36.65 ms |

The trailing dot makes it a fully-qualified name, so libc declines the shortcut
(`inet_aton("1.2.3.")` fails, verified) and asks the configured nameserver, which answers. .NET's
`Dns.GetHostAddresses` calls the same `getaddrinfo` on Linux and would return the same address.

The file's own header claimed *"These tests use only IP literals and names this container
resolves from `/etc/hosts`, so none of them needs a network."* That sentence was measured false
for exactly this row, and it is now corrected in place rather than left standing.

This is the same shape as two earlier findings: #2320's rows that passed only because this
machine has a `~/Desktop`, and #2351's rows that hard-coded a tzdata version. `docs/StandingApprovals.md`
SA-6 calls a test that passes only because of a machine property a defect **in the test**.

## 3. The repair

Two assertions replace one, and neither depends on the resolver:

1. **`IPAddress::TryParse` must reject the text.** This is the whole of SR-AUD-304 — the finding
   was that `Dns` had a *second, disagreeing* IPv4 parser that gave malformed text a literal
   reading. That claim holds on every machine.
2. **Whatever happens next must match an independent oracle.** The test calls `getaddrinfo`
   itself, through a different door, and requires `Dns::GetHostAddresses` to agree: throw
   `SocketException` when the resolver has no answer, and otherwise return exactly the resolver's
   address set. This is the oracle pattern #2351 established for tzdata.

The oracle is POSIX-only and `#ifdef`-guarded; on Windows assertion 1 still runs.

## 4. Why this is stronger, not weaker

The old test could only ever fail in two ways: the port invents a literal (the real defect), or
the resolver changes its mind (noise). The new one keeps the first and removes the second, and it
adds a check the old one could not express — that a *successful* resolution is reported
faithfully, with no fabricated entries and no duplicates.

Mutation evidence:

| Mutation | Caught |
|---|---|
| Return an empty vector instead of raising on resolution failure | ✅ (also by `SignedAndSpacedIPv4Text_IsRejected`) |
| Give malformed text a literal reading by trimming trailing `' '`/`'.'` | ✅ |

The test count does not move: one test is rewritten, not added.
