<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `DateOnly` and `TimeOnly` gain invariant `ParseExact` (ticket #1939)

*2026-08-19.* Four additive members: `DateOnly::ParseExact`/`TryParseExact` and
`TimeOnly::ParseExact`/`TryParseExact`, each taking an input and a format string.

**Purely additive.** No existing result, exception, message or failure output changed; general
`Parse`/`TryParse`, formatting, providers, styles, kind and the XML bridges are untouched. Landed
under the #1929 row 4A approval in `docs/DateTimeExactParsingAndKindDesign.md`.

---

## 1. What is supported

| | `DateOnly` | `TimeOnly` |
|---|---|---|
| standard | `O`/`o` = `yyyy-MM-dd`; `R`/`r` = `ddd, dd MMM yyyy` | `O`/`o` = `HH:mm:ss.fffffff`; `R`/`r` = `HH:mm:ss` |
| custom fields | `y`–`yyyy`, `M`/`MM`/`MMM`/`MMMM`, `d`/`dd`, optional `ddd`/`dddd` | `H`/`HH` or `h`/`hh`, `m`/`mm`, optional `s`/`ss`, `f`–`fffffff`, `F`–`FFFFFFF`, `t`/`tt` |
| literals | `%`, `'…'`, `"…"`, `\x` | same |
| required | one complete year, month and day | one hour and minute; 12-hour needs `t` |

Rejected: providers, styles, kind, eras, calendars, zones, multi-format and span APIs, time tokens
in a date format and vice versa, duplicate fields, mixed 12/24-hour fields, more than seven
fractional specifiers or digits, unmatched literals, and any leading, trailing or extra inner
whitespace. The whole input must be consumed.

`Parse` throws the type's existing `FormatException` family with HResult `0x80131537` for **both**
an input mismatch and a malformed format — the approval says so, and .NET agrees. `Try` returns
`false` and writes `MinValue`.

## 2. The digit-width rule is .NET's, not the specifier count

`ParseDigits(str, 1)` reads **one or two** digits; `ParseDigits(str, n)` for `n > 1` reads
**exactly** `n`. So `yyyy-M-d` accepts both `2024-6-5` and `2024-06-15`, while `yyyy-MM-dd`
accepts only the padded form. A scanner written as *"count the specifiers, read that many digits"*
gets the single-specifier case wrong in both directions, and a mutation to that effect is caught.

`yy` uses the invariant calendar's fixed `TwoDigitYearMax` of 2029. That is a property of the
**invariant culture**, not culture state, which is why it is available here even though #1929
declined a two-digit year in the *general* parser — there the width is the discriminator and
nothing says what was meant; here the format says it.

## 3. One of the approval's own examples is now stale

It reads: *"`DateOnly::ParseExact("2024-6-5", "yyyy-M-d")` succeeds, **and the same unpadded text
still fails general `DateOnly::Parse`**."*

**#1929 landed the day before** and widened the general grammar to a one- or two-digit month and
day, so the general parser accepts it now. The example's *point* — that exact and general parsing
are separate grammars — is unaffected, and the test asserts it with text the general parser really
does reject (`15/06/2024`).

## 4. Two decisions worth naming

**`:` and `/` are literals, not rejected placeholders.** .NET treats them as the culture's time
and date separator placeholders, and the approval says to reject *"provider separator
placeholders"*. Rejecting the **characters** would go further than that means — the approval's own
worked example is `TryParseExact("10:20:30.12345678", "HH:mm:ss.ffffffff")`, which must fail on the
eight `f`s and not on the colons. For the invariant culture the placeholders resolve to exactly
those characters, so matching them literally gives the same answer; what is out of scope is a
*provider's* ability to change them, which is 4B by name.

**A one-character format that is not standard is not silently custom.** .NET requires `%d` for a
single-specifier custom format, because a bare `d` is the standard short-date pattern. Preserving
that stops `d` from quietly meaning something this port does not implement.

## 5. Evidence

| Mutation | Caught |
|---|---|
| One specifier reads exactly one digit | yes |
| Full input consumption is not required | yes |
| Weekday agreement is dropped from `R` | yes |
| A 12-hour form no longer requires a designator | yes |
| `f` becomes as permissive as `F` | yes |
| The name match stops preferring the longest candidate | **equivalent — see below** |

The last is recorded rather than counted. For the four **invariant** tables it is defensive rather
than load-bearing: no invariant month or day name is a prefix of another, and the abbreviated and
full tables are never consulted in the same call. The rule is kept because it is what makes the
loop correct for any table, and the comment at the site says exactly that.

## 6. Downstream

Additive only — nothing to migrate. Neither consumer calls `DateOnly` or `TimeOnly` parsing.
