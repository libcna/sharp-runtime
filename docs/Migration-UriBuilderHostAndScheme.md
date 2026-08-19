<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `UriBuilder` brackets an IPv6 host and lower-cases the scheme (ticket #1996, groups G-1 and G-2)

*2026-08-19.* `setHostProperty("::1")` rendered `http://::1/`, which `Uri` rejects, and
`setSchemeProperty("HTTP")` kept `HTTP` and rendered `HTTP://…`. Both now match .NET.

#1996 splits into four independently landable groups and names **G-1 + G-2 as the recommended
minimum**. Both are alignments to the reference, so **SA-5** covers them. G-3 (scheme validation,
which #1996 calls *"the only narrowing"*) and G-4 (relative promotion) are **not** taken and stay
with the ticket. No signature, layout, vtable or `noexcept` change in either group.

**This changes the text `ToString()` emits.** Read §1 and §3 before upgrading.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `setHostProperty("::1")` → `ToString()` | `http://::1/` (unparseable) | `http://[::1]/` |
| `setHostProperty("2001:db8::1")` | unbracketed | `http://[2001:db8::1]/` |
| `setHostProperty("[::1]")` | unchanged | **unchanged** — not double-wrapped |
| `setHostProperty("h:abc")` | `http://h:abc/` (unparseable) | `http://[h:abc]/` (§3) |
| `setHostProperty("example.com")`, `"192.0.2.1"`, `""` | — | **unchanged** |
| `setSchemeProperty("HTTP")` | `HTTP` | `http` |
| `setSchemeProperty("bad scheme")` | stored as given | **unchanged** — G-3 owns that |

## 2. The two rules, transcribed

**Host (`UriBuilder.cs:167-197`).** The trigger is .NET's own: the value must contain one of
`s_hostReservedChars`, `":/\?#@[]"`, and then a `:` specifically — .NET's comment calls it a
*"probable ipv6 address"*. A value already `[...]` is left alone.

**Scheme (`UriBuilder.cs:169-181`).** The setter ends in `value = value.ToLowerInvariant();`.
Invariant, so the fold here is ASCII-only: `std::tolower` consults the global C locale, and a
process that installed a Turkish one would fold `I` to a dotless `i` and change what the scheme
means — the hazard #2316 removed from `CharUnicodeInfo`.

## 3. One deliberate divergence, forced by taking G-1 without G-3

.NET's host setter **wraps first and then throws**: `"[::1"` becomes `"[[::1]"` and is refused
because the inside holds a `[` (`:183-187`). This group does not take the rejection — #1996's own
note reserves "a setter that never threw starts throwing" for a group this is not — and wrapping
without rejecting would leave the nonsense `"[[::1]"` stored, turning a value this port's `Uri`
already refuses (**#1991**) into one it might not.

So a value that already carries a bracket is left exactly as given. That keeps #1991's refusal
where it was, and the reason is at the site rather than in this document alone.

**`h:abc` is not a divergence, it is .NET.** A host containing `:` is bracketed on .NET's own
"probable ipv6 address" rule, and the port's `Uri` does not validate a literal's *content*
(`docs/SystemUriNamespaceReviewPlan.md` §15.4) — so `http://[h:abc]/` now parses where
`http://h:abc/` did not. Two existing pins asserted the old unparseability and were updated: their
subject is *hash obtainability on an unparseable rendering*, which is now asserted on `"[::1"` and
the empty host, both of which still are.

## 4. Evidence

Five mutations, four caught:

| Mutation | Result |
|---|---|
| no bracketing at all | caught |
| bracket unconditionally (ignores the `:` trigger) | caught — 4 cases |
| an already-bracketed value is wrapped again | caught — 3 cases |
| the scheme is not lower-cased | caught |
| **the fold uses `std::tolower`** | **equivalence in the "C" locale** |

The last is reported rather than papered over, and it is the same case **#2174** recorded: the
test binary runs in the "C" locale, where `std::tolower` agrees with the explicit fold on every
byte. Distinguishing them needs the global locale changed inside a shared binary, which #2174
considered and declined for exactly this reason. The explicit fold is kept because the guarantee
is about what happens when a process *does* install another locale — which is not something a test
in this binary can observe. The note is at the site.

`Decl1996_G3AndG4AreNotTakenAndStayPinned` asserts the three behaviours this group deliberately
left alone, so neither can be added without a decision.

## 5. Downstream

`cna` and `mobile-eggbert` reference `UriBuilder` in **zero** places. A first-party caller that
set an IPv6 host and read `ToString()` gets a string `Uri` now accepts; one that set an
upper-case scheme now sees it lower-cased, which is what .NET has always done.
