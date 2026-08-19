<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `CookieContainer` enforces capacity, aging and eviction (ticket #2042)

*2026-08-19.* Storage was **unbounded in every direction** — no capacity, no per-domain capacity,
no maximum cookie size, and no expiry cleanup, so an expired cookie was retained and only hidden
from emission while `Count` grew for ever. Measured before: 10,000 cookies from one origin were
all kept.

Landed under `docs/StandingApprovals.md` **SA-5** and **SA-3**. `sizeof(CookieContainer)` grows by
three `intcs`; **downstream consumers must be recompiled**. `Add` now **discards stored data**,
which is the behaviour change to read before upgrading.

---

## 1. The gate was two claims, and the reference dissolves both

#2042 was blocked because *"every bound is a number somebody must choose"* **and** *"gated on
evidence, since .NET's exact default capacities cannot be established here"*.

They are the same claim, and it is false. .NET **defines** the numbers:

```csharp
public const int DefaultCookieLimit = 300;
public const int DefaultPerDomainCookieLimit = 20;
public const int DefaultCookieLengthLimit = 4096;
                                        // CookieContainer.cs:69-71
```

Nobody chooses them; they are derived, like every other constant in this port.

## 2. What changed

| | Was | Is |
|---|---|---|
| 2,000 cookies from one origin | all 2,000 kept | **20** kept, the newest |
| 40 domains × 20 cookies | all 800 kept | **≤ 300** |
| a cookie whose `Value` exceeds 4096 | stored | `CookieException` |
| a cookie added already expired | stored, hidden from emission | **not stored** — it is a removal command |
| a cookie that expires while stored | retained for ever | purged when aging runs |
| re-adding an existing Name/Domain/Path | replaced | **unchanged** — consumes no slot |
| `Capacity`, `PerDomainCapacity`, `MaxCookieSize` | absent | present, with .NET's validation |

**The per-domain limit binds at 19, not 20**, and that is .NET's arithmetic rather than an
off-by-one: `min_count = min(domain_count × fraction, min(perDomain, capacity) − 1)`
(`CookieContainer.cs:441`). Aging must *free* a slot for the cookie being added, so a domain at
the limit is cut to one below it and then grows back to 20.

`MaxCookieSize` is the one limit that **reports** instead of evicting, it bounds the **Value**
alone — not the name, not the whole cookie — and its message is .NET's
(`CookieContainer.cs:235-237`, `Strings.resx:87-89`). When a capacity limit cannot be satisfied
the new cookie is **silently rejected**, which is also .NET's (`if (… && !AgeCookies(…)) return;`).

## 3. One structural difference, stated rather than glossed

.NET evicts from the least-recently-used **path collection** of a domain, because its storage is
a table of domains each holding path collections with their own timestamps. This container is a
flat list with no collections to time-stamp, so it drops the oldest **stored** cookie in the
affected scope.

Within a collection .NET does the same thing — `cc.RemoveAt(0)` is its oldest entry — so the
difference is only *which* domain loses a cookie when the total limit binds, never whether the
bound is enforced. No extra state was needed: insertion order **is** vector order, because `Add`
appends and an identity match is replaced in place.

## 4. Evidence

Six mutations, **all caught** — three of them only after the test that was supposed to catch them
was fixed, and each of those is recorded because the reason is reusable:

| Mutation | Result |
|---|---|
| the domain target is the limit rather than one below it | caught |
| the size limit covers the whole cookie, not the Value | caught |
| an expired cookie is stored instead of removing | caught |
| the `Capacity` setter ignores `PerDomainCapacity` | caught |
| **the capacity check runs before the identity replacement** | caught **after** §4.1 |
| **no expiry purge during aging** | caught **after** §4.2 |

**4.1 — the domain must be *at* its limit.** Below the limit the capacity check does nothing and
the two orders agree, so a case that filled to 19 could not see the difference. Filling to 20 can.

**4.2 — the expired cookie must not also be the oldest.** If it is, plain oldest-first eviction
removes it anyway and the purge is invisible. Eleven live cookies now precede it, so the purge
saves `n0` and its absence gives it away.

The expiry test **waits**, and that is legitimate where #2031's `SIGSTOP` shape was not: `Expires`
is a fixed instant and sleeping past it can only overshoot, whereas that one needed to land inside
a microsecond window.

**A bug in my own test hung the suite once**, and it is worth naming: `std::count_if(c.GetHeader().begin(), c.GetHeader().end(), …)`
takes iterators into **two different temporaries**. The header is bound to a named string now.

## 5. Downstream, measured

`cna` and `mobile-eggbert` reference `CookieContainer` in **zero** code sites. All six
cross-module consumer suites this ticket requires were run and are green: `Net` 340, `Net_Http`
201, `Net_Sockets` 132, `Net_WebSockets` 105, `Net_NetworkInformation` 63, `Net_Http_Json` 15.

A caller storing more than 20 cookies per domain, or more than 300 in total, will now lose the
oldest. That is the repair.
