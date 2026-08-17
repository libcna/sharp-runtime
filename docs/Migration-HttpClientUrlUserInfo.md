<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `parseUrl` separates userinfo and rejects junk after an IPv6 bracket (ticket #2072)

*2026-08-17.* `HttpClient::parseUrl` had no userinfo rule, so `http://user@host/p` returned the
host `"user@host"` — a string that then went to `getaddrinfo` as a DNS name and into the `Host:`
header. And `http://[::1]x/p` returned host `"::1"` with the `x` **silently discarded**.

Landed under `docs/StandingApprovals.md` SA-5. No public signature, layout, vtable or `noexcept`
change.

---

## 1. What changed

| URL | Was | Is |
|---|---|---|
| `http://user@example.com/p` | host `"user@example.com"` | host `"example.com"` |
| `http://user:pass@example.com/p` | `UriFormatException` — **by accident** (see §3) | host `"example.com"` |
| `http://user:pass@example.com:8080/p` | `UriFormatException` | host `"example.com"`, port 8080 |
| `http://a@b@example.com/p` | host `"a@b@example.com"` | host `"example.com"` |
| `http://user@/p` | host `"user@"` | `UriFormatException` (empty host) |
| `http://[::1]x/p` | host `"::1"`, `x` **discarded** | `UriFormatException` |
| `http://[::1]/p`, `http://[::1]:8080/p` | — | **unchanged** |
| a URL with no `@` in its authority | — | **unchanged** |

## 2. Why

RFC 3986 §3.2 spells the authority `[ userinfo "@" ] host [ ":" port ]`, and §3.2.2 allows only
`":" port` after an IPv6 literal's closing bracket. .NET keeps `UserInfo` as its own component
and its `Host` never contains it.

Silently discarding text after the bracket is the more insidious of the two: the URL the caller
wrote and the URL the client connected to differed, with no diagnostic.

**The last `@` is the delimiter.** RFC 3986 §3.2.1's `userinfo` production admits `@`, and the
`host` production does not, so the final one splits. Using the first would make `b@example.com`
the host of `http://a@b@example.com/`.

## 3. `http://user:pass@host/` used to throw for the wrong reason

It did fail before — but because `rfind(':')` made `"pass@example.com"` the **port** text, which
then failed the port grammar. The failure was accidental, and the message said "invalid port".
It now succeeds with the host `example.com`, which is what it always denoted.

## 4. The userinfo is discarded, deliberately

`ParsedUrl` has no userinfo field, this handler has no authentication path to hand one to, and
inventing one is new API. What matters for this ticket is that the userinfo stops being part of
the **host** — a name that reaches DNS and the `Host:` header.

If you were relying on userinfo reaching the server, it never did: it was going into the host
name, where it made the lookup fail.

## 5. To migrate

Nothing, unless you pass URLs with credentials in them. Those used to fail (with a misleading
message) or resolve wrongly; they now parse to the host they name, and the credentials are
dropped rather than smuggled into a header.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `HttpClient` or `System::Net::Http` — **zero sites
in both**. Neither repository was modified.
