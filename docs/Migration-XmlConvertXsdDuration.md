<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `XmlConvert` TimeSpan uses the XSD `duration` form (ticket #2080)

*2026-08-17.* `XmlConvert::ToString(TimeSpan)` emitted .NET's **native colon form** and
`XmlConvert::ToTimeSpan` parsed it. .NET's use the XML Schema `duration` lexical form and never
look at the colon form at all.

Landed under `docs/StandingApprovals.md` SA-5. No public signature, layout, vtable or `noexcept`
change. **This changes emitted text and narrows accepted input** — read §1.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `ToString(TimeSpan::FromDays(1))` | `"1.00:00:00.0000000"` | `"P1D"` |
| `ToString(TimeSpan::FromTicks(0))` | `"00:00:00"` | `"PT0S"` |
| `ToString(TimeSpan::FromMinutes(90))` | `"01:30:00"` | `"PT1H30M"` |
| `ToString(TimeSpan::FromDays(-1))` | `"-1.00:00:00"` | `"-P1D"` |
| `ToTimeSpan("P1D")`, `ToTimeSpan("PT1H30M")` | `FormatException` | parsed |
| `ToTimeSpan("1.00:00:00")` | parsed | **`FormatException`** |
| `ToTimeSpan("PT.5S")` | `FormatException` | 0.5 s — see §3 |

`ToString` and `ToTimeSpan` round-trip, and a test asserts that over eight values including
negatives, zero and a single tick.

## 2. Why, and the three questions the deferral could not answer

The deferral listed exactly three sub-questions with no repository-contained answer. The
reference answers all three:

* **The year/month conversion factors.** `XsdDuration` *estimates* — 365 days to the year, 30 to
  the month — and says so in as many words (`XsdDuration.cs:243-244`). Reproduced rather than
  improved on: a "better" estimate would disagree with every .NET-produced value. Note `P12M`
  rounds to a year, because the algorithm is `(years + months/12) * 365 + (months % 12) * 30`.
* **Whether the native colon form stays accepted.** It does **not**. `XsdDuration`'s parser
  requires a leading `P`, and `XmlConvert.ToTimeSpan` goes straight to it
  (`XmlConvert.cs:1109-1127`). Accepting both would make the method's contract "either
  grammar", which no reference or schema defines.
* **The exception identity.** `FormatException`, always. `XmlConvert.ToTimeSpan` wraps *every*
  `XsdDuration` failure — including its `OverflowException` — in one, with the comment "Remap
  exception for v1 compatibility" (`:1118-1122`).

## 3. Two details that surprised the first cut

**`PT.5S` is valid.** .NET's `'.'` branch is the one component that does **not** check whether
any digits preceded it, and XML Schema agrees: `duSecondFrag` admits `('.' fracFrag)` with no
leading digit. The first cut of the test listed `PT.5S` as malformed; the expectation was wrong,
not the parser, and there is now a row recording that so it is not "fixed" later.

**One line of the reference is dead.** `XsdDuration.TryParse` ends with
`if (numDigits != 0) goto InvalidFormat;` before its "no trailing characters" check. Every path
that reaches it has already rejected `pos >= length`, so `pos < length` holds and the next check
rejects regardless. The port omits it, and a mutation confirmed the omission changes nothing.

## 4. Two tests from ticket #1836 are rewritten, not deleted

`#1836` pinned that an out-of-range day count *raises* rather than wrapping to a negative
duration. That property is unchanged and still asserted — in the new grammar, and with
`FormatException` rather than `OverflowException`, which is .NET's own remapping.

## 5. To migrate

If you write XML, this is the fix: `xs:duration` is what a schema-validating consumer expects,
and the colon form was never valid there.

If you were round-tripping through these two methods, you still are — the pair is consistent.

If you were feeding `XmlConvert::ToTimeSpan` a colon-form string from elsewhere, use
`System::TimeSpan::Parse`, which is the method for that grammar and is unchanged.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `XmlConvert` or `System::Xml` — **zero sites in
both**. Neither repository was modified.
