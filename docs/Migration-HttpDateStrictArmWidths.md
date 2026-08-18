<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the HTTP-date arms now match the format strings they transcribe (ticket #2376)

*2026-08-18.* `Sun, 06 Nov 199 08:49:37 GMT` was accepted and read as the year **199 AD**;
`Sun, 06-Nov-94 08:49:37 GMT` was accepted with an abbreviated weekday on a shape .NET spells
`dddd`. Both are now rejected.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. Why this exists as its own ticket

#2360 adopted .NET's sixteen lenient formats — a **widening** — and its own new tests surfaced
these two rows. Repairing them is a **narrowing**, and #2005 recorded that bundling a narrowing
into a verified widening is the thing to avoid. So they were pinned and split out, and this is
that ticket.

## 2. What changed

| Input | Was | Is |
|---|---|---|
| `Sun, 06 Nov 199 08:49:37 GMT` | accepted, year **199 AD** | rejected |
| `Sun, 06 Nov 9 …`, `Sun, 06 Nov 19945 …` | accepted | rejected |
| `Sun Nov  6 08:49:37 199` and `… 19945` | accepted | rejected |
| `Sun, 06-Nov-94 08:49:37 GMT` | accepted | rejected — that shape is `dddd` |
| `Xyz, 06 Nov 1994 08:49:37 GMT` | accepted | rejected — `ddd` is a name table |
| every form RFC 9110 §5.6.7 requires | — | **unchanged**, to the same instant |

## 3. The defect

The three strict arms are `sscanf` conversions, and `sscanf`'s `%d` and `%[A-Za-z]` do not bound
a field. .NET's are exact:

* `yyyy` and `yy` are `ParseDigits` with an **exact** width, so nothing between or beyond them
  parses;
* `ddd` is `MatchAbbreviatedDayName` — a table of seven names, not any three letters;
* `dddd` is `MatchDayName` — full names only.

## 4. The ticket named two sites and there are three

It listed the abbreviated weekday on the hyphenated RFC 850 shape and the three-digit year on
IMF-fixdate. **asctime carries the same unbounded year and the same unvalidated weekday**
(`HttpDateParser.cs:26` spells it `ddd MMM d H:m:s yyyy`), and repairing two thirds of one rule is
not repairing it.

## 5. And the lenient arm needed the same rule

The first cut left `Xyz, 06 Nov 1994 08:49:37 GMT` parsing, because #2360's lenient arm checked
the weekday's **length** rather than its name — so a value the strict arm had just refused for a
bad weekday was accepted one arm later. The lenient arm now consults the same table. That is the
hazard of having two parsers for one grammar, and it is why the fix had to reach both.

## 6. To migrate

These forms were never valid HTTP-dates. `Sun, 06 Nov 199 …` in particular did not merely fail —
it produced a wrong instant, off by more than eighteen centuries, with no diagnostic.

## 7. Evidence

| Mutation | Caught |
|---|---|
| The IMF-fixdate year-width check goes away | yes |
| The hyphenated shape accepts an abbreviated weekday again | yes |
| The asctime year-width check goes away | yes |
| The lenient arm checks the weekday length again, not the name | yes |
| The abbreviated-name table has a typo | yes |
| The full-name table has a typo | yes |

Two mutations were invalid as first written — removing an entry from a fixed-size `std::array`
leaves a null pointer that the comparison then dereferences — and were reformulated as typos
rather than counted.

## 8. Downstream

Neither `cna` nor `mobile-eggbert` references `RetryConditionHeaderValue` or
`RangeConditionHeaderValue` — zero sites in both, the same measurement #2360 recorded.
