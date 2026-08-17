<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the two obsolete HTTP-date forms are accepted, and a short year stops being wrong by 1900 years (ticket #2130)

*2026-08-17.* The shared HTTP-date parser accepted only the preferred IMF-fixdate form. RFC 9110
§5.6.7 requires a **recipient** to accept three. All three are accepted now.

It also uncovered a latent defect: a two-digit year on an IMF-fixdate was **accepted with the
wrong value**.

Landed under `docs/StandingApprovals.md` SA-5. Implementation-only header; no public signature,
layout, vtable or `noexcept` change.

---

## 1. What changed

| Value | Was | Is |
|---|---|---|
| `Sun, 06 Nov 1994 08:49:37 GMT` | accepted | **unchanged** |
| `Sunday, 06-Nov-94 08:49:37 GMT` (RFC 850) | rejected | accepted, 1994-11-06T08:49:37Z |
| `Sun Nov  6 08:49:37 1994` (asctime) | rejected | accepted, the same instant |
| `Sun, 06 Nov 94 08:49:37 GMT` | **accepted as the year 94 AD** | accepted as **1994** |
| trailing text after any of them | rejected | **rejected** |
| an embedded NUL | rejected | **rejected** |
| `Sun, 06 Nov 1994 08:49:37 UTC` and fifteen other lenient .NET forms | rejected | **rejected** — see §4 |

All three required forms parse to the same instant, and a test asserts that rather than checking
each separately.

## 2. Why this stopped being a deferral

#2125 recorded, correctly, that neither obsolete form had *ever* been accepted here — so its
full-consumption repair could not have narrowed a required form away — and that closing the gap
*"is a WIDENING and belongs to #2130, which is deferred because `/rv` is absent and .NET's own
behaviour cannot be established here."*

It can be now. `HttpDateParser.TryParse` tries strict `"r"` and then **twenty-one** format
strings, four of them RFC 850 and one ANSI C `asctime`
(`Common/src/System/Net/HttpDateParser.cs:9-32`).

## 3. The latent defect the widening uncovered

`Sun, 06 Nov 94 08:49:37 GMT` was **not** rejected. The `%d` conversion read `94` literally, so
the parser reported the year **94 AD** — a silently wrong instant, off by nineteen centuries, and
no test had noticed. .NET accepts the same text and reads 1994
(`"ddd, d MMM yy H:m:s 'GMT'"`, `HttpDateParser.cs:17`).

So this row is a **correction**, not a widening: the port's answer was wrong rather than merely
strict.

The window is .NET's: `DateTimeFormatInfo.InvariantInfo`'s Gregorian calendar has
`TwoDigitYearMax == 2029`, so `00`..`29` are 2000..2029 and `30`..`99` are 1930..1999. The naive
`1900 + yy` — which turns `06` into 1906 — is caught by a mutation. Only an **exactly**
two-digit token is expanded, so a four-digit year keeps meaning exactly what it says.

## 4. What is deliberately *not* adopted

.NET's remaining sixteen formats are **leniency**, not required forms: a `UTC` zone token
instead of `GMT`, no zone token at all, a missing day-of-week, and RFC 5322 numeric offsets.
Adopting them would accept text RFC 9110 does not define as an HTTP-date — a much larger
widening than this ticket asked for — and each carries its own ambiguity: a bare time with no
zone is only UTC because .NET *assumes* it is, and an RFC 5322 numeric offset means the value is
**not** UTC, which every consumer of this parser currently assumes it is.

That is ticket **#2360**, and the current behaviour is pinned so it cannot land by accident.

## 5. To migrate

Nothing, unless you were relying on an obsolete-format `Date`, `Expires`, `Last-Modified`,
`Retry-After` or `If-Range` header being **rejected**. Those senders are non-conforming but
RFC 9110 requires you to accept them.

If you were reading a two-digit-year IMF date, the value you got was wrong and is now right.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `System::Net::Http` — **zero sites in both**.
Neither repository was modified.
