<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the HTTP-date parser accepts .NET's sixteen lenient formats (ticket #2360)

*2026-08-18.* Every header that carries an HTTP-date now accepts the same text .NET accepts. #2130
adopted RFC 9110 §5.6.7's three **required** forms; this adds the sixteen further formats
`HttpDateParser` tries beyond them.

**Pure widening.** No value that parsed before parses differently or fails now — guaranteed by
construction, see §5. Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or
`noexcept` change.

---

## 1. What is newly accepted

All of these now parse to 1994-11-06T08:49:37Z:

| Value | `HttpDateParser.cs` |
|---|---|
| `Sun, 06 Nov 1994 08:49:37 UTC` | `:12` |
| `Sun, 06 Nov 1994 08:49:37` | `:13` — no zone |
| `06 Nov 1994 08:49:37 GMT` | `:14` — no day-of-week |
| `06 Nov 1994 08:49:37 UTC` | `:15` |
| `06 Nov 1994 08:49:37` | `:16` |
| `Sun, 06 Nov 94 08:49:37 UTC` | `:18` |
| `Sun, 06 Nov 94 08:49:37` | `:19` |
| `06 Nov 94 08:49:37 GMT` | `:20` |
| `06 Nov 94 08:49:37 UTC` | `:21` |
| `06 Nov 94 08:49:37` | `:22` |
| `Sunday, 06-Nov-94 08:49:37 UTC` | `:23` |
| `Sunday, 06-Nov-94 08:49:37 +00:00` | `:24` |
| `Sunday, 06-Nov-94 08:49:37` | `:25` |
| `Sun, 06 Nov 1994 08:49:37 +00:00` | `:28` |
| `06 Nov 1994 08:49:37 +00:00` | `:30` |
| plus inner whitespace anywhere | `DateTimeStyles.AllowInnerWhite` |

A numeric offset is **applied**, not ignored: `… 08:49:37 -05:00` is 13:49:37 UTC. A missing zone
is read as UTC, which is `DateTimeStyles.AssumeUniversal` rather than an assumption of this port's.

## 2. Twenty-one format strings are not twenty-one grammars

They are three shapes crossed with three axes, and transcribing the **cross** rather than the list
is what makes the gaps visible:

| shape | day-of-week | date | year | zone |
|---|---|---|---|---|
| RFC 1123 / 5322 | `Ddd,` or absent | `d MMM yyyy` | 4 or 2 | `GMT` \| `UTC` \| `zzz` \| absent |
| RFC 850 | `Dddddd,` only | `d-MMM-yy` | 2 only | `GMT` \| `UTC` \| `zzz` \| absent |
| asctime | `Ddd` no comma | `MMM d … yyyy` | 4 only | absent only |

**Two cells of that cross are missing from .NET's list**, and are therefore rejected here: a
two-digit year combined with a numeric offset, with and without a day-of-week. Neither
`ddd, d MMM yy H:m:s zzz` nor `d MMM yy H:m:s zzz` appears between `HttpDateParser.cs:9` and `:32`.

Writing the parser as a cross would have added those two silently. *The obvious completion of the
pattern* is exactly the widening that has no reference behind it, so they are excluded explicitly
and pinned.

## 3. The zone token

From `zzz`, whose parser is `ParseTimeZoneOffset` (`DateTimeParse.cs:3285-3345`): a sign, one or
two hour digits, then an **optional** `':'` before two minute digits. So `-05:00` and `-0500` are
both accepted and mean the same thing, and a minute field of 60 or more is rejected
(`DateTimeParse.cs:3334`). Only `GMT` and `UTC` are named zones; `EST` is not in .NET's list and
is still refused.

## 4. Which door sees which forms — a correction

`Retry-After` dispatches on the **first character**: a digit means delta-seconds, and the whole
value must then be digits. So `Retry-After: 06 Nov 1994 08:49:37 GMT` is still rejected there.

That is not a defect, and my first cut of the tests assumed it was. .NET does exactly the same and
says so: *"We either have a timespan or a date/time value. Determine which one we have by looking
at the first char. If it is a number, we have a timespan, otherwise we assume we have a date."*
(`RetryConditionHeaderValue.cs:94-98`). `If-Range` has no such ambiguity, so it is the door the
tests use to exercise the whole grammar.

## 5. Why this cannot break an existing value

The lenient arm runs **after** the three strict arms, so any value they accept never reaches it.
The widening is safe by construction rather than by inspection.

Measured on top of that: reordering the chain so the lenient arm runs *first* changes no test
result, because the two agree on everything both accept. The ordering is a defensive property, and
that mutation demonstrates it is not load-bearing — recorded as an equivalence rather than counted
as a caught mutation.

## 6. Two pre-existing leniencies this ticket surfaced and did **not** repair

The three strict arms are `sscanf` conversions, and `sscanf`'s `%d` and `%[A-Za-z]` do not bound a
field width. So the port accepts two shapes .NET rejects:

* an **abbreviated** weekday on the hyphenated RFC 850 shape — .NET spells it `dddd`, full names
  only, so `Sun, 06-Nov-94 08:49:37 GMT` is rejected there and accepted here;
* a **three-digit** year on the IMF-fixdate shape — .NET spells it `yyyy`, exactly four, so
  `Sun, 06 Nov 199 08:49:37 GMT` is accepted here and read literally as the year 199.

Both are **narrowings**, and #2360 is a widening. #2005 recorded that bundling a narrowing into a
verified widening is the thing to avoid, so they are ticket **#2376** and are pinned by
`Pin2376_TheStrictArmsAreWiderThanTheirFormatStrings`. The new lenient arm already enforces both
bounds; only the three strict arms are affected.

## 7. Evidence

| Mutation | Caught |
|---|---|
| Complete the cross — allow a two-digit year with a numeric offset | ✅ |
| Let the hyphenated shape take an abbreviated weekday | ✅ — **only after** probing through a zone the strict arm declines |
| Let the space shape take a full weekday name | ✅ |
| Accept any year width | ✅ — same, only after the strict arm was routed around |
| A missing zone is not read as UTC | ✅ |
| Drop the offset minute `< 60` check | ✅ |
| Require the `':'` before the offset minutes | ✅ |
| Accept any three-letter zone token, not only `GMT`/`UTC` | ✅ (2 tests) |
| Run the lenient arm first | **equivalent — see §5** |

Two of the eight needed a stronger test, and both for the same reason: a strict arm already
accepted the input and answered before the lenient one could be wrong about it.

## 8. Downstream, measured

Per SA-2 condition 5: neither `cna` nor `mobile-eggbert` references `RetryConditionHeaderValue`,
`RangeConditionHeaderValue`, `Retry-After` or `If-Range` — **zero sites in both**. Neither
repository was modified. A widening could not change their answers in any case.
