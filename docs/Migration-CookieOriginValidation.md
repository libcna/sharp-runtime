<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — cookie origin validation and constructor-supplied values (ticket #2040)

*2026-08-17.* `System::Net::CookieContainer` now validates an explicitly supplied `Domain`
against the URI a cookie arrives from, and `System::Net::Cookie`'s constructors now mark the
values they are given as **explicit**. The two are one change because the implicit flags are the
input to the domain rule.

Everything here is **source-, ABI-, layout-, vtable- and `noexcept`-compatible**: no signature,
member, base, virtual, object layout or exception specification changed. What changed is
behaviour, and it changed in the strict direction — calls that succeeded now throw, and values
that were silently replaced are now kept.

The rule is transcribed from `Cookie.VerifyAndSetDefaults` (`Cookie.cs:358-424`) rather than
inferred; the design record is `docs/SystemNetNamespaceReviewPlan.md` §14.1.1.

---

## 1. What starts throwing

`CookieContainer::Add(uri, cookie)` throws `System::Net::CookieException` when the cookie carries
an **explicitly supplied** `Domain` that either is not a valid domain name or does not
domain-match `uri`'s host. The cookie is not stored.

```cpp
CookieContainer container;
Cookie cookie("session", "isolated");
cookie.setDomainProperty(".unrelated.invalid");
container.Add(Uri("http://origin.invalid/"), cookie);
// was: stored, and later emitted by GetCookieHeader(Uri("http://unrelated.invalid/"))
// now: CookieException, "The 'Domain'='.unrelated.invalid' part of the cookie is invalid."
```

`SetCookies(uri, header)` reaches `Add` and therefore rejects the same relation — which is the
case that matters, because that is the path a `Set-Cookie` header from a server travels.

**To migrate:** scope the cookie to a domain the origin actually belongs to, or leave `Domain`
unset and let it default to the origin's host.

### 1.1 Two narrowings that come with the shared rule

`domainMatches` is now the rule `Cookie.HostMatchesDomain` states, and it is used both to validate
at insertion and to match at emission — one function, as in .NET. That brought in two conditions
this port did not have (`Cookie.cs:347-349`):

| Case | Was | Is |
|---|---|---|
| `Domain=invalid` on host `origin.invalid` | matched — a single-label domain matched every host under it | **rejected**; a domain without a dot requires an exact match |
| `Domain=2.3.4` on host `1.2.3.4` | matched — the host is an IP literal, but the suffix rule did not care | **rejected**; an IP-literal host requires an exact match |
| `Domain=1.2.3.4` on host `1.2.3.4` | matched | matched, unchanged |
| `Domain=.example.invalid` on host `www.example.invalid` | matched | matched, unchanged |

Both narrowings also apply at **emission**, so a cookie stored before the upgrade under a
single-label domain stops being emitted for subdomains. That is the same direction as the
insertion check and is deliberate.

## 2. What stops being overwritten

`Cookie`'s path- and domain-accepting constructors route through their setters, so
`PathImplicit`/`DomainImplicit` become `false` — exactly as .NET's constructors do by writing
`Path = path;` and `Domain = domain;` (`Cookie.cs:108-118`).

```cpp
container.Add(Uri("http://sub.origin.invalid/some/where"),
              Cookie("n", "v", "/explicit", ".origin.invalid"));
// was: stored with Path="/some/where" and Domain="origin.invalid" — the caller's values were
//      discarded, because the constructors left both flags set
// now: stored with Path="/explicit" and Domain=".origin.invalid"
```

**To migrate:** code that passed a path or domain to a constructor and relied on the container
replacing it must stop passing it. Code that passed one and expected it to be honoured now gets
what it asked for — and, if the domain does not match the origin, an exception instead of silent
substitution.

## 3. What did not change

- A cookie with **no** explicit `Domain` still has the request host applied, and is **never**
  validated — the host it is being set to is by construction its own origin.
- Defaulting from the URI does **not** clear `DomainImplicit`, matching `SetDomainAndKey`
  (`Cookie.cs:310-314`); the flag records where the value came from.
- `Path` defaulting is unchanged apart from no longer overriding a caller-supplied path.
- Every legitimate parent-domain cookie still works: `Domain=.example.invalid` added from
  `www.example.invalid` is stored and emitted for `api.example.invalid`.
- No layout change, so **no consumer rebuild is required** for this ticket.

## 4. Downstream, measured

Per `docs/StandingApprovals.md` SA-2 condition 5, both consumer checkouts were searched: neither
`cna` nor `mobile-eggbert` names `Cookie`, `CookieContainer` or any cookie type — **zero sites in
both**. Neither repository was modified.
