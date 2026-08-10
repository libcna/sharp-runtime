# Audit: `modules/net/src/System/Net/Dns.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [Dns.cs](/rv/tmp/runtime/src/libraries/System.Net.NameResolution/src/System/Net/Dns.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

The POSIX/WinSock paths are scoped and released correctly on successful calls.
The local literal fast path duplicates parsing and bypasses the common .NET
literal policy.

### SR-AUD-304 — medium — DNS IPv4 literal fast path accepts invalid text, wildcard addresses, and wrong-family results

`tryParseIPv4` uses `sscanf("%u")`, so `-0.0.0.1` is accepted as `0.0.0.1` on
the audited libc; an out-of-range conversion is undefined by the C scanf
contract.  `GetHostAddresses("0.0.0.0")` returns the wildcard address although
current .NET rejects `IPAddress.Any`, and an IPv4 literal queried with
`AddressFamily::Unix` returns one IPv4 result instead of the empty
family-filtered result.  The shared .NET route first parses with `IPAddress`
and then applies wildcard and family policy.

Required remediation: remove the duplicate `sscanf` parser, use
`IPAddress::TryParse`, reject wildcard literals consistently, and apply the
requested family filter before returning a literal.

## Missing assertions and diagnostics

DNS tests exercise ordinary IPv4/IPv6 literals but omit negative text,
oversized numeric fields, wildcard literals, and a literal with a mismatched
or non-IP `AddressFamily`.

## Final assessment

Confirmed fast-path validation and family-filter mismatch.

---

## Correction and remediation record — ticket #2039, 2026-08-04

*Everything above is the original audit text and is preserved verbatim. This section is
appended, not a rewrite.* Evidence: `build-probe/2039_probe1_before_after.log`, one probe source
built before and after with `Dns.cpp` and `IPAddress.cpp` compiled **from source**, plus
`build-probe/2039_probe2_dns_leak.cpp` for the LSan half.

### Correction 1 — the duplicate results do NOT come from the duplicate parser

This is the load-bearing correction, and it is measured against `getaddrinfo` **directly**
(probe §E), not inferred:

| Call | `ai_socktype = 0` | `ai_socktype = SOCK_STREAM` |
|---|---|---|
| `getaddrinfo("1.2.3")` | **3** entries, `st=1,2,3` | **1** |
| `getaddrinfo("localhost")` | **3** entries | **1** |
| `getaddrinfo("127.0.0.1")` | **3** entries | **1** |

`hints.ai_socktype` was `0`, which asks for one `addrinfo` **per socket type**, so *every* answer
came back three times — including every resolved **name**, which the finding never mentions, and
`GetHostEntry(8.8.8.8)`, which returned **six** entries for two distinct addresses. The
`sscanf` parser never even saw `"1.2.3"`: it yields three conversions, not four, so it declined
and the text fell through to the resolver.

**Consequence for the repair:** replacing the literal parser alone would have made
`GetHostAddresses("1.2.3")` return one address **by accident** — because `IPAddress::TryParse`
accepts three-part short forms and the fast path returns a single value — while leaving
`localhost` tripled. The repair therefore sets `ai_socktype = SOCK_STREAM` (what this
repository's own `TcpClient`, `UdpClient` and `Socket` already pass) **and** deduplicates.

### Correction 2 — the duplicate parser disagreed about *valid* input, not only invalid input

| Text | Through `Dns` (sscanf) | Through `IPAddress::Parse` |
|---|---|---|
| `"0177.0.0.1"` | **177.0.0.1** (decimal) | **127.0.0.1** (octal) |
| `"+1.2.3.4"` | 1.2.3.4 | rejected |
| `" 1.2.3.4"` | 1.2.3.4 | rejected |
| `"1.2.3"` | declined → resolver | 1.2.0.3 |
| `"-0.0.0.1"` | 0.0.0.1 | rejected |

One module returned **two different addresses** for one valid literal depending on which door
was used. Only the last row is named by the finding.

### Correction 3 — plan §7.3's "empty" prediction contradicts this repository's own contract

`docs/SystemNetNamespaceReviewPlan.md` §7.3 predicts that
`GetHostAddresses("127.0.0.1", Unix)` should become **empty**. It raises
`SocketException(HostNotFound)` instead, because two pre-existing tests —
`DnsTests.GetHostAddresses_RequestingIPv6Only_MismatchedIPv4Literal_Throws` and
`..._RequestingIPv4Only_MismatchedIPv6Literal_Throws` — already pin the mismatched-literal case
as **throwing**, and their comment records the reasoning verbatim: an empty vector is
*"indistinguishable from 'checked and found nothing'"*. Following §7.3 would have reversed a
documented prior decision without approval. The plan is corrected in its §17.6; the code follows
the repository.

No `SR-AUD-*` identifier was issued for any of these; numbering stays frozen at 364.

### Remediation

1. **One literal parser.** `tryParseIPv4`/`tryParseIPv6Literal` are replaced by a single
   `tryParseIPLiteral` over `IPAddress::TryParse`. The `#1961` recursion guard uses it too, which
   makes that guard **stricter**: the old pair could miss a literal written in a spelling the
   `sscanf` parser declined, and a missed literal is exactly the input that re-entered the
   function.
2. **The family filter reaches the literal path.** A literal answers only when the requested
   family is `Unspecified` or its own; otherwise `SocketException(HostNotFound)` naming the host
   and the family.
3. **Duplicates.** `ai_socktype = SOCK_STREAM`, plus a first-occurrence-preserving dedup using
   **binary `IPAddress` equality** — family, address bits and, for IPv6, the scope id. No
   canonicalisation: `1.2.3.4` and `::ffff:1.2.3.4` are distinct families and both survive, as do
   `fe80::1%7` and `fe80::1%9`. Ordering is the resolver's, which has already applied RFC 6724
   destination selection.
4. **Messages.** All four throw sites keep their exception type, `SocketError` and native code
   and gain the platform resolver's own description (`gai_strerror` on POSIX, the WSA code on
   Windows). The tests assert the *contract* — type, code, no `"Win32"`, the host named — not the
   C library's exact wording.
5. **`Dns.hpp`'s note is made true.** It claimed resolution was "restricted to IPv4", that the
   hints "always request AF_INET regardless of the family argument", and that `InterNetworkV6`
   "returns an empty result". All three were false.

**Deliberately unchanged:** `"0.0.0.0"` and `"::"` still resolve to themselves — the gated
**#2043** — now pinned by `DnsWildcardPinTests` so that ticket cannot land silently.

**One residual, recorded not hidden:** the family filter reaches a **literal** but not a resolved
**name**, so `GetHostAddresses("localhost", Unix)` still returns an IPv4 address. Narrowing that
would remove a currently-succeeding result for every name, which is outside this ticket's
approval-free envelope; ordinary inactive ticket **#2046** carries it.

| Sanitizer | Result |
|---|---|
| LSan | **discriminating** — clean across **12,800** calls over both the success and failure paths; a control build abandoning one `addrinfo` list per iteration reports `12800 byte(s) leaked in 200 allocation(s)` |
| ASan | clean (same binary) |
| UBSan / TSan | not applicable — no undefined conversion, no shared state, no thread |

**Tests:** `modules/net/tests/System/Net/DnsLiteralAndDuplicateTests.cpp` (16).

**Consequences:** no signature, `noexcept`, virtual, vtable, data member or object-layout change.
The header changed only in its doc-comments.

Status: `confirmed` → **`confirmed (design-complete)`** — three of four halves remediated by
#2039; the wildcard half remains open as the blocked, design-complete **#2043**.
