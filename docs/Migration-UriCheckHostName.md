<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `Uri::CheckHostName` — #1997 group A-2 (SR-AUD-151), and the scanner move that made it possible

**Purely additive: no existing signature, layout, vtable, `noexcept` specification or accepted input
changed, and no consumer needs editing or rebuilding.** It lands under **SA-5**. It also **closes
#1997**, whose four groups A-1…A-4 are now all landed.

## The defect

`System::UriHostNameType` has documented `CheckHostName` since the enum was ported, and **nothing in
this runtime could produce a value of that type**. The member did not exist.

## The recorded cost was understated, and the measurement corrects it

#1997's own record priced group A-2 as:

> *"a new public module edge to reach `System::Net::IPAddress`, or a second address-literal parser
> inside this module — the duplication #2354 spent a ticket removing"*

**The first of those is impossible rather than expensive.** `modules/net` declares
`PUBLIC_DEPENDENCIES Collections.Core ComponentModel Core.Base Uri` — it depends on `Uri` **already**
— so an edge from `modules/uri` to `System::Net::IPAddress` is a **cycle**, the dependency inversion
`Guid.cpp` refused for cryptography. A first cut written against `IPAddress` was rejected by the
module-boundary validator, and this is why.

**The route taken is a third one neither option named.** `modules/net`'s IPv4 and IPv6 scanners
lived in an anonymous namespace in `IPAddress.cpp` and are **pure string-to-number scanners** — no
platform call, no dependency on `IPAddress` itself. They moved verbatim to
`modules/core/include/System/detail/IPAddressLiteral.hpp`. Both modules already depend on
`Core.Base`, so:

* **the module graph does not change** — still **41 / 94**;
* there is **one definition**, not two;
* `modules/net/src/System/Net/IPAddress.cpp` is **−197 / +11 lines** and delegates.

`IPAddress`'s own `validatedScopeId`, `formatIPv4` and `formatIPv6` stayed behind: they are about
that type rather than about the grammar. **The move is behaviour-preserving and that is measured
rather than asserted** — `SharpRuntimeTests_Net` runs 340/340 unchanged, which is the whole of
`IPAddress`'s parse coverage.

## The classification, and where the order decides an answer

.NET's order (`Uri.cs:1286-1325`) is transcribed rather than rearranged, and **IPv4-before-DNS is
the step that decides an answer rather than merely tidying**. `CheckHostName` calls
`IPv4AddressHelper.IsValid(name, out end, false, false, false)`, whose `allowIPv6` and
`unknownScheme` both being `false` selects **`ParseNonCanonical`** rather than the canonical
dotted-quad grammar. So:

| Input | Result | Why |
|---|---|---|
| `"1"` | **IPv4** | a bare number is a non-canonical IPv4 — and also a perfectly good DNS label |
| `"0x7F.1"` | **IPv4** | hex-prefixed short form |
| `"3232235777"` | **IPv4** | single 32-bit value |
| `"1.2.3.4.5"` | **Dns** | more than four parts is not IPv4; every label is legal |
| `"example.com"` | **Dns** | |
| `"[::1]"` | **IPv6** | |
| `"::1"` | **IPv6** | see below |

Reordering IPv4 and DNS answers `Dns` for the first three and passes every other row.

## Two rules that are new with this member

**The label rules.** This port's `Uri` constructor only ever asked about the host's **characters**
(#2359's DNS character set). `DomainNameHelper.IsValid` also requires that every label begin with an
ASCII letter or digit, and that every label be **1..63** characters. Deriving `Dns` from the
character set alone answers `Dns` for `"-x"`, for the empty inner label in `"a..b"` and for a
64-character label. **A trailing dot is accepted and ends the walk** — `"example.com."` is a valid
DNS name, easy to get wrong in either direction because an empty final label would otherwise fail
the 1..63 rule.

**An unbracketed IPv6 literal is still IPv6**, which reads like a bug until the reference is read:
.NET's last resort retries `IPv6AddressHelper.IsValid($"[{name}]")` (`Uri.cs:1320-1324`). So
`CheckHostName("::1")` is `IPv6` even though a `Uri` authority requires the brackets. **The two
questions are different** — this one asks what a string *is*, not whether it may appear in an
authority.

## One definition, not two

The constructor's host-character loop was **factored into `detail::hostCharactersAreValid`** and
both callers now use it, so a host the constructor accepts cannot be one `CheckHostName` calls
malformed *on characters*. Two grammars for one question is the **#2393** shape, where it was found
only after a caller could construct a `Uri` this port's own `TryCreate` reported as invalid.

**They are still not the same question, and that is stated rather than left to be discovered:** the
constructor does **not** apply the label rules, so `Uri("http://-x/")` parses while
`CheckHostName("-x")` is `Unknown`. .NET has the same split, reaching `DomainNameHelper` by a
different path with different flags. A case asserts both halves together.

## Evidence

Seven mutations, all caught. **M6 was NOT CAUGHT at first, and it found a defect in my test rather
than in the code.** The "entire name must be consumed" property was asserted with `"[::1]junk"` —
which fails on the *front/back* guard, its last character being `k`, so it never enters the
bracketed branch at all. A body measuring the literal to the **first** `]` instead of to the end of
the name passed it anyway. The input that separates the two is bracketed at both ends with junk
**inside**: `"[::1]]"`. Both rows are now asserted.

Gate: **17,694 / 38, 0 failed, 0 skipped** (+5; `SharpRuntimeTests_Uri` 314 → 319; `Net` unchanged
at 340). Module graph **41 / 94**, unchanged. Negative fixture set **53 / 269**, unchanged — no
spelling was outlawed. Downstream: **zero sites** in `cna` and `mobile-eggbert`, the member having
not existed.
