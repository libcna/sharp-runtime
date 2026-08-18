<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the date/time parse grammar widens to .NET's (ticket #1929)

*2026-08-18.* `System::DateTime`, `System::DateTimeOffset` and `System::DateOnly` now accept a
**one- or two-digit month and day**, and all three accept .NET's full **time-zone offset**
grammar rather than a fixed `±HH:MM`.

**This is a pure widening.** No string that parsed before parses differently or fails now, so
there is no source break, no signature change, no layout change, and nothing to migrate. This
note exists because text .NET accepts is now accepted here too, and a reader should know which
text that is.

Decided by the user on 2026-08-18, on ticket #1929's four recorded respects. Landed under
`docs/StandingApprovals.md` SA-5 (derived from the reference, never guessed).

---

## 1. What is newly accepted

| Input | Was | Is |
|---|---|---|
| `"2024-6-15"` | rejected | 15 June 2024 |
| `"2024-06-5"` | rejected | 5 June 2024 |
| `"2024-6-5T1:2:3-05:00"` | rejected | that instant at −05:00 |
| `"…+8"` / `"…-8"` | rejected | ±8 h |
| `"…+08"` | rejected | +8 h |
| `"…+2:5"` | rejected | **125 min** — 2 h 05 m |
| `"…+02:5"`, `"…+2:05"` | rejected | 125 min |
| `"…+800"`, `"…+0800"` | rejected | +8 h |
| `"…+205"`, `"…+0205"` | rejected | 125 min |
| `"…-0530"` | rejected | −330 min |
| everything that parsed before | — | **the identical value** |

## 2. What is still rejected, and why

| Input | Why |
|---|---|
| `"24-06-15"`, `"204-06-15"` | the year is exactly four digits — see §4 |
| `"2024-006-15"`, `"2024-06-015"` | a field is at most two digits |
| `"2024 -06-15"`, `"2024-06- 15"` | internal whitespace is grammar, and only the **outer** boundary is trimmed (#1929 row 5) |
| `"…+2:60"`, `"…+0860"` | a minute field of 60 or more — .NET's own check, `DateTimeParse.cs:565-568` |
| `"…+12345"` | a run of five digits is not an offset |
| `"…+"`, `"…+2:"` | a sign or a colon with nothing after it |
| `"…+02:00junk"` | the string must be consumed in full (#1879) |
| `"June 15 2024"` | month names and culture patterns remain out of scope |

## 3. Where each rule comes from

Both halves are transcribed, not invented.

**The date.** .NET's lexer classifies a run of one or two digits as a `NumberToken` and a run of
three or more as a `YearNumberToken` (`Globalization/DateTimeParse.cs:5593-5605`). `"2024-6-15"`
therefore produces exactly the token sequence `"2024-06-15"` produces — year, number, number — and
reaches the same year-month-day terminal state. .NET accepts it; this port now does too.

**The offset.** `ParseTimeZone` (`DateTimeParse.cs:530-556`) reads one digit run after the sign:

* length **1 or 2** — that is the hour, and a `':'` may follow with a **1- or 2-digit** minute;
* length **3 or 4** — the hour is `value / 100` and the minute is `value % 100`;
* anything else fails.

That integer split is why `"+800"` and `"+0800"` agree, and why `"+205"` means 125 minutes.

## 4. Two deliberate stops

**The year is not widened.** .NET would read a one- or two-digit year through
`Calendar.ToFourDigitYear`, whose century window is culture state (`TwoDigitYearMax`) that this
port has no way to carry. A short year stays a failure rather than silently landing in a century
nobody chose.

**The ±14 h bound stays on `DateTimeOffset` and does not move into the shared grammar.** That is
where .NET puts it: `ParseTimeZone` permits an hour up to 99, and the
`DateTimeOffset.MinOffset`/`MaxOffset` test runs later, at the two sites that actually store an
offset (`DateTimeParse.cs:2777,2875`). `System::DateTime` parses an offset and **discards** it —
it has no `DateTimeKind` (`docs/DateTimeValidationBoundaryPlan.md` §16.4) — so it must not inherit
a bound on a value it never keeps. `DateTime::TryParse("…+99")` succeeds and
`DateTimeOffset::TryParse("…+99")` fails, and a test pins both halves of that sentence.

## 5. One structural change worth knowing about

`DateTimeOffset::TryParse` used to copy its input into a `std::string`, search for a `'+'` or
`'-'` **starting at character 10**, and hand the prefix to `DateTime::TryParse`. That split is
correct only while the date part is exactly ten characters wide. Widening the month and day makes
`"2024-6-5"` eight characters, so the search was about to start inside the date — or, for a bare
short date, past the end, which the `input.size() < 10` precheck was quietly standing in for.

The three doors now share one grammar in
`modules/core/include/System/detail/DateTimeTextScanner.hpp`. They had drifted apart precisely
because they did not.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` calls `DateTime::Parse`, `DateTime::TryParse`,
`DateTimeOffset::Parse`/`TryParse` or `DateOnly::Parse`/`TryParse` at all — **zero sites in
both**. Neither repository was modified. Since the change is a widening, even a caller that did
parse dates would see no existing input change its value.
