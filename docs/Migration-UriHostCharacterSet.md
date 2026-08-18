<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a `Uri` host must be a valid DNS name (ticket #2359)

*2026-08-18.* `Uri("http://exa mple.com/")` now raises `UriFormatException`. It used to be
accepted, reporting the host `"exa mple.com"`.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. What changed

| Input | Was | Is |
|---|---|---|
| `http://exa mple.com/` | host `"exa mple.com"` | `UriFormatException` |
| `http://ex$ample.com/`, `ex(`, `ex,`, `ex\|`, and a tab | accepted | `UriFormatException` |
| a NUL in the host | carried through | `UriFormatException` |
| `http://my_host-1.example.com/` | accepted | **unchanged** — `_` is in .NET's set |
| `http://192.168.1.1/`, `http://[::1]/` | accepted | **unchanged** |
| a non-ASCII host such as `bücher.example` | accepted | **unchanged** — the IRI path takes non-ASCII |
| a host containing U+0085 | accepted | `UriFormatException` — U+0080–U+009F are excluded |
| a space in the **path**, **query** or **fragment** | accepted | **unchanged** |

## 2. The trace #2005 asked for, and what it corrects

#2005 settled the surrounding-whitespace half from the reference and deliberately stopped, because
this half *"needs a trace through .NET's host parser, not a line to quote"*, naming six
`ParsingError.BadHostName` sites in `Uri.cs`.

The trace is four steps, and **it corrects the framing**: which of the six sites a space reaches
does not matter, because none of them is conditional in the way the ticket implied.

1. `DomainNameHelper.IsValid` does `hostname.IndexOfAnyExcept(s_validChars)`, where
   `s_validChars` is exactly `-0123456789A-Z_a-z.` (`DomainNameHelper.cs:36-37`). A space is not
   in it, and is not one of the delimiters that *truncate* the host instead, so it returns `false`
   (`:100-119`).
2. The IRI path uses `s_iriInvalidChars`, which **lists the space explicitly** (`:40-45`).
3. With no host type parsed, `CheckAuthorityHelper` reaches
   `if ((syntaxFlags & UriSyntaxFlags.AllowAnyOtherHost) != 0)` (`Uri.cs:3902-3928`) and, failing
   it, sets `ParsingError.BadHostName`.
4. **No built-in scheme sets `AllowAnyOtherHost`** — measured across all fourteen `UriSyntaxFlags`
   constants in `UriSyntax.cs`. So the rule is uniform: every scheme .NET ships rejects a host it
   cannot parse.

## 3. Scope, and where the narrowing deliberately stops

A **bracketed IPv6 literal** takes `IPv6AddressHelper` and is exempt. An **IPv4 literal** needs no
exemption at all: digits and dots are already in `s_validChars`.

The **path, query and fragment** are different productions and are untouched — a space in a path
still parses. Keeping both halves in one test is what shows the narrowing stopped where .NET's
does.

**Non-ASCII is accepted**, because .NET's IRI path takes anything outside ASCII *except*
U+0080–U+009F. Decoding the scalar rather than waving the bytes through is what makes that
distinction reachable at all, and it costs nothing: #2354 put the decoder in `Core.Base` earlier
the same day and `modules/uri` already depends on it. The graph is unchanged at 41/93.

## 4. A tightening this brings along

`ClientWebSocketHandshakeValidationTests.ATerminatorInTheUriHostIsRejected` asserted that a URI
with CR/LF in its host is refused by `ClientWebSocket::ConnectAsync`, because the host is
concatenated into a `Host:` header. **That URI can no longer be built at all** — CR and LF are not
in the DNS host set — so the refusal moved one step earlier, to `Uri`'s constructor.

The assertion moved with it. Leaving it at `ConnectAsync` would have *passed for the wrong
reason*: the constructor now throws first, outside the `EXPECT_THROW`. The WebSocket guard is
still exercised, through the **path**, which the host rule does not govern.

## 5. To migrate

Percent-encode or strip the offending characters before constructing the `Uri`. A host containing
a space was never resolvable, and a host containing CR or LF was a header-injection vector.

## 6. Evidence

| Mutation | Caught |
|---|---|
| Drop the host character check entirely (the pre-#2359 behaviour) | yes (3 tests) |
| Remove the bracketed-IPv6 exemption | yes (4 tests, all pre-existing) |
| Wave any non-ASCII byte through without decoding | yes |
| The underscore leaves the valid set | yes |

## 7. Downstream

Neither `cna` nor `mobile-eggbert` constructs a `System::Uri` — zero sites in both.
