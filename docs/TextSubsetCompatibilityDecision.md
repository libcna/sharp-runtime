<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# How narrow should this port's numeric and date/time surface be? — the #1927/#1928/#1929 decision

Written 2026-07-31 by the Group E subset batch, after #1897 landed. It exists so
the three tickets the Groups A–D batch filed as *inactive* can be decided **once,
against one measured picture**, instead of one at a time against three partial ones.

**Nothing in this document is implemented.** Every port row is **measured**
(`build-probe/1927_subset_matrix.log`, `1927_round_sweep.log`,
`1927_round_detail.log`, `1927_round_examples.log`, reproducible with
`build-probe/1927_subset_probe.cpp` and `1927_round_sweep.cpp`). No `SR-AUD-*`
identifier was issued; audit numbering stays frozen at **364**.

---

## 0. Correction to this document's own commission

The batch instruction framed #1927, #1928 and #1929 as one family asking:

> *How narrow should sharp-runtime's numeric and date/time **text subsets** be
> relative to current .NET?*

**Measured, that is the right question for #1929 and the wrong one for the other
two.** Preserving the original framing rather than silently substituting one:

| Ticket | What it actually is | Text subset? |
|---|---|---|
| **#1927** | `Single::Round`/`Double::Round` re-implement the scale/round/divide inline instead of delegating to `MathF::Round`/`Math::Round`, so a large magnitude **overflows to infinity** | **no** — a returned *value*, no text involved |
| **#1928** | `Math::Round(x,digits,mode)` throws .NET's message **minus the leading "Rounding"**, while `MathF`, `Single` and `Double` all use .NET's verbatim | **no** — an exception *message* |
| **#1929** | the four date/time parsers implement a deliberately narrower grammar than .NET | **yes** |

What all three *do* share is one question, and it is the one this document
answers:

> **Where this port's observable numeric and date/time surface differs from
> current .NET, is the difference a defect to close or a subset to document?**

§§1–5 answer it with evidence. §6 is the consolidated decision packet. §7
restates #1899's separate question, which the same packet has to carry.

---

## 1. Inventory — the surfaces in scope

Measured from the public headers, not assumed:

| Surface | Parse/TryParse overloads | Style / provider overloads | In scope because |
|---|---|---|---|
| `Decimal` | 2 | none | grouping, overflow taxonomy |
| `Double`, `Single` | 2 each | none | grouping, exponent range, signed zero |
| `Half` | 4 | yes | inherits the float rules |
| `Int32/64/16`, `UInt32/64/16`, `Byte`, `SByte` | 4 each | **yes** (`NumberStyles` + provider) | grouping, whitespace, overflow |
| `DateTime`, `DateTimeOffset` | 2 each | none | **#1929** |
| `DateOnly`, `TimeOnly` | 2 each | none | **#1929** |
| `TimeSpan` | 2 | none | fractional seconds, whitespace |
| `XmlConvert::To{Decimal,Single,Double,TimeSpan,DateTime,DateTimeOffset}` | bridges | — | they delegate, so they inherit every row below |
| `Buffers::Text::Utf8Parser::TryParse` | 9 integral + float/bool/date | standard-format char | a second, independent numeric grammar |
| `String::Format` / composite | — | — | closed by #1884; emitted text only |
| `Single`/`Double` `ToString(value, format)` | — | — | closed by #1863; §5 re-checks the round trip |

**Deliberately out of scope**, for want of evidence that they are implicated:
`Convert::To*`, `Uri` escaping, `Encoding`, and every non-numeric,
non-date/time text API.

**A structural fact that shapes every option below:** `DateTime`,
`DateTimeOffset`, `DateOnly`, `TimeOnly` and `TimeSpan` have **no
style-taking and no provider-taking overload at all**, and no
`ParseExact`/`TryParseExact`. The integral types have four overloads each and a
real `NumberStyles` parameter. So a "strict mode / compatible mode" answer is
*already expressible* for integers and *not expressible at all* for date/time
without new API.

---

## 2. Measured matrix — numeric text

Port column measured. .NET column derived from `/rv/tmp/runtime` source with the
file and line given; **no .NET runtime is installed on this machine**, so no .NET
row here was executed. Rows are marked ✅ agree / ⚠️ port narrower / ⬆️ port wider
/ ❓ .NET behaviour not established.

### 2.1 Group separators and malformed grouping

The rule .NET actually implements is
`Common/src/System/Number.Parsing.Common.cs:156`: a group separator is accepted
whenever `AllowThousands` is set, **at least one digit has been seen**, and no
decimal separator has been seen yet. **There is no group-size validation and no
requirement that digits follow.**

| Input | `Decimal` (port) | `Double` (port) | .NET | |
|---|---|---|---|---|
| `"1,5"` | `15` | — | `15` | ✅ |
| `"1,234.5"` | `1234.5` | `1234.5` | `1234.5` | ✅ |
| `" 1,234.5 "` | `1234.5` | `1234.5` | `1234.5` | ✅ |
| `"1,23,456"` | `123456` | `123456` | `123456` — no size check | ✅ |
| `"1,2345"` | `12345` | — | `12345` | ✅ |
| `",5"` | reject | — | reject — no digit yet | ✅ |
| `"1,"` | `1` | — | `1` — separator accepted, then `StateDigits` ends the parse | ✅ |
| `"1,,5"` | `15` | — | `15` — the guard is per-character, not per-run | ✅ |
| `"1.234,5"` | reject | — | reject — `,` after `.` fails the `StateDecimal` guard | ✅ |
| `"1_000"` | reject | reject | reject | ✅ |

**Finding: nothing to decide here.** #1858 and #1865 did not merely add grouping,
they reproduced .NET's rule **including its permissiveness**. The four rows an
onlooker would call "malformed grouping" (`1,23,456`, `1,2345`, `1,`, `1,,5`) are
accepted by .NET too, for the reason quoted above. Ticket #1929 does not claim
otherwise; this section exists because the batch instruction asked for the rows
and because "the port accepts `1,,5`" reads like a defect until the reference is
read.

`Int32` **rejects** `"1,234"` (✅ — .NET's `NumberStyles.Integer` has no
`AllowThousands`, `NumberStyles.cs:63`).

### 2.2 Exponent range, signed zero, whitespace, trailing text

| Input | Port | .NET | |
|---|---|---|---|
| `"1e999"` / `"-1e999"` | `+∞` / `−∞`, no throw | same — .NET Core 3.0+ returns infinity | ✅ |
| `"1e-999"` / `"-1e-999"` | `0` / `−0` | same | ✅ |
| `"-0"`, `"-0.0"` (`Double`) | `−0` preserved | `−0` preserved | ✅ |
| `"-0"` (`Decimal`) | `0` | `0` | ✅ |
| `"-0"` (`Int32`) | `0` | `0` | ✅ |
| `"3.5e38"` (`Single`) | `+∞` | `+∞` | ✅ |
| `"1.7976931348623159E+308"` | `+∞` | `+∞` | ✅ |
| `"Infinity"`, `"-Infinity"`, `"NaN"` | accepted | accepted (invariant symbols) | ✅ |
| `".5"`, `"5."` | `0.5`, `5` | `0.5`, `5` | ✅ |
| `" 1.5 "`, `" 12 "` | accepted | accepted — `AllowLeadingWhite\|AllowTrailingWhite` | ✅ |
| `"1.5junk"`, `"12.0"` (int), `"0x10"` (int) | reject | reject | ✅ |
| `"2147483648"` (`Int32`) | `OverflowException` | `OverflowException` | ✅ |
| `"79228162514264337593543950336"` (`Decimal`) | `OverflowException` | `OverflowException` | ✅ |

**Finding: the numeric half of the family is closed.** Every row measured after
the Groups A–D batch agrees with .NET. **There is no numeric text-subset
decision left to take** — which is itself the answer to a third of the question
this document was commissioned to ask.

---

## 3. Measured matrix — date/time text (#1929)

This is where the port really is narrower, and it is narrower in **six** ways,
not the four #1929 lists.

### 3.1 The four #1929 records

| # | Shape | Port | .NET | Reference | |
|---|---|---|---|---|---|
| 1 | `"2024-6-15"`, `"2024-06-5"` — unpadded date fields | **reject** | accept | `DateTimeParse.cs` lexer | ⚠️ |
| 2 | `".1234"`, `".1234567"` — 4+ fraction digits | **reject** | accept, 100 ns kept | `DateTimeParse.cs:479-492` `ParseFraction` — reads *all* digits, returns `digits > 0` | ⚠️ |
| 3 | `"+2:5"`, `"+2"`, `"+0205"` — short/compact offsets | **reject** | accept; `"+2:5"` = **125 min** | `DateTimeParse.cs:530-548` `ParseTimeZone` — 1–2-digit hour, optional `':'` + 1–2-digit minute, **or** a 3–4-digit `HHmm` block | ⚠️ |
| 4 | `ParseExact`, provider overloads, `DateTimeKind` | absent | present | — | ⚠️ |

Row 3's `"+2:5"` is the one the Groups A–D packet got wrong: §C.4 called the
port's 125-minute reading *"a wrong answer that survives round-tripping"*, and
measured, **the port and .NET agreed**. #1879 then made the port reject it. That
correction is already recorded in `docs/DateTimeValidationBoundaryPlan.md`
§20.3.1 and is repeated here because it changes what "widening" would mean: for
`"+2:5"` it would **restore** the port's own pre-#1879 answer.

### 3.2 Two more, measured in this batch and not in any ticket

| # | Shape | Measured | |
|---|---|---|---|
| **5** | **Whitespace policy is inconsistent inside the port.** `Int32::TryParse(" 12 ")`, `Double::TryParse(" 1.5 ")` and `Decimal::TryParse(" 1,234.5 ")` all **accept** surrounding whitespace; `DateTime::TryParse(" 2024-06-15 ")` and `TimeSpan::TryParse(" 01:02:03 ")` **reject** it. .NET **accepts in every one of the five** — `NumberStyles.cs:63/70/73` for the numbers, `DateTimeParse.cs` `SkipWhiteSpaces()` for the dates, `TimeSpanParse.cs:696` `input = input.Trim()` for the span | ⚠️ and internally inconsistent |
| **6** | **The port's two time grammars disagree with each other.** `TimeOnly::TryParse("1:2:3")` **accepts** (`H{1,2}:m{1,2}:s{1,2}`); `DateTime::TryParse("2024-06-15 1:2:3")` **rejects** (`HH:mm:ss`). `TimeSpan::TryParse("1:2:3")` also accepts, and `TimeSpan::TryParse("10:20:30.1234567")` accepts a **7-digit** fraction while `DateTime` rejects 4 | internally inconsistent |

Row 6 is the sharpest result in this document. **The port already parses a
7-digit fraction, in `TimeSpan`, today** — so the justification recorded in
`modules/core/src/System/DateTime.cpp:419-423` and in
`DateTimeValidationBoundaryPlan.md` §20.3.1 …

> *"the port honours milliseconds only, so a 4-digit fraction is text it cannot
> represent"*

… is **wrong about the representation**. `DateTime` holds `longcs ticks_` with
`TicksPerMillisecond = 10000` (`DateTime.hpp:33,56`), i.e. **100-nanosecond
resolution, exactly like .NET**, and `DateTime(longcs ticks)` and
`AddTicks(longcs)` are both public. What cannot represent a 4-digit fraction is
the **parser's own `int ms` intermediate and the millisecond constructor it feeds** —
a choice, not a limit. The original text is preserved in both places; this is the
correction, not a rewrite.

### 3.3 Rows where the port is **wider** than .NET

| Shape | Port | .NET | |
|---|---|---|---|
| `DateOnly::TryParse("2024-06-15Z")` | **accept** | reject — `DateOnly` has no zone concept | ⬆️ |
| `DateTime::TryParse("…+02:00")` | accept, offset **ignored** | accept, offset **applied** (converts to local) | ⬆️/divergent |

The second is not a grammar question but a semantic one: the port has no
`DateTimeKind` (§16.4 of the date/time plan), so it cannot apply an offset. It is
listed so the widening/narrowing picture is complete, and it is **not** proposed
for change here — it needs `DateTimeKind`, which is a much larger ticket.

### 3.4 Rows where the port and .NET agree, after #1879

`"2024-06-15junk"`, `"2024-06-15 10:xx:00"`, `"2024-02-30"`, `"10:20:30.abc"`,
`"2024-06-15T10:20:30."`, `"+15:00"` (beyond ±14 h), `"01:02:03junk"`,
`DateOnly::TryParse("2024-06-15 10:20:30")`, `TimeOnly::TryParse("2024-06-15T10:20:30")`,
`"0001-01-01T00:00:00"`, `"9999-12-31T23:59:59.999"` — **all ✅**. #1879 is why,
and none of it is reopened here.

---

## 4. Measured matrix — #1927 and #1928

### 4.1 #1927 — and a corrected premise that changes its risk class

The ticket's acceptance criteria say the repair *"changes the returned VALUE for
currently-valid input (large magnitudes, **and possibly the last ulp for ordinary
values**, since the delegate uses an exact power-of-ten lookup table rather than
`std::pow`)"*. That parenthesis is the reason it was filed as a semantic change
rather than a defect fix. **Measured over 240,000 samples, it is false.**

`build-probe/1927_round_sweep.log`, deterministic PRNG, 20,000 samples per band,
`digits` uniform over its whole legal range:

| Band | `Double::Round` vs `Math::Round` | of which "current returns ∞" |
|---|---|---|
| 1e-9 … 1e-3 | **0 differ** | 0 |
| 1e-3 … 1e3 | **0 differ** | 0 |
| 1e3 … 1e8 | **0 differ** | 0 |
| 1e8 … 1e15 | **0 differ** | 0 |
| 1e15 … 1e18 | 1,908 (9.54 %) | 0 |
| 1e18 … 1e30 | 1,811 (9.06 %) | 0 |
| 1e30 … 1e300 | 9,632 (48.16 %) | **8,628** |

| Band | `Single::Round` vs `MathF::Round` | of which "current returns ∞" |
|---|---|---|
| 1e-6 … 1e-3, 1e-3 … 1e3, 1e3 … 1e8 | **0 differ** | 0 |
| 1e8 … 1e20 | 1,524 (7.62 %) | 0 |
| 1e20 … 3e38 | 16,782 (83.91 %) | **16,745** |

**Below each type's round limit (1e16 for `double`, 1e8 for `float`) the two
funnels agree bit for bit, in every one of 240,000 samples.** The "last ulp for
ordinary values" concern is measured away.

And every difference *above* the limit is the current code being **wrong**
(`1927_round_examples.log`):

```
INFINITY  x=-1.8894988582774554e+299 d= 9  current=-inf                    delegate=x exactly
FINITE    x= 6.6932819227719057e+299 d= 7  current=6.6932819227719064e+299 delegate=x exactly
```

A `double` of magnitude ≥ 1e16 has **no representable fractional part**, so the
mathematically correct rounded value *is* `x`. The delegate returns exactly that
in every sampled case; the current code returns either `∞` or a value one ulp
away. **#1927 is therefore a defect fix, not a semantic widening**, and its risk
class should be read down accordingly.

Concrete headline cases: `Double::Round(1e300, 15)` → `inf` (delegate:
`1e300`); `Single::Round(3.0e38f, 6)` → `inf` (delegate: `3.0e38f`).

### 4.2 #1928 — the message, at all five call sites

| Site | Message thrown today | .NET |
|---|---|---|
| `Math::Round(double,intcs,MidpointRounding)` | **`digits must be between 0 and 15, inclusive.`** | `Rounding digits must be between 0 and 15, inclusive.` |
| `Math::Round(double,intcs)` | **same wrong text** (it forwards) | same as above |
| `MathF::Round(float,intcs,MidpointRounding)` | `Rounding digits must be between 0 and 6, inclusive.` | ✅ `Strings.resx:1931` |
| `Double::Round(double,intcs)` | `Rounding digits must be between 0 and 15, inclusive.` | ✅ `Strings.resx:1928` |
| `Single::Round(float,intcs)` | `Rounding digits must be between 0 and 6, inclusive.` | ✅ |

**One string, at `Math.hpp:588`, reached by two overloads.** Three of the four
funnels already carry .NET's text verbatim; #1862 gave `Double::Round` the last
of them, which is what made the inconsistency visible. The parameter name
(`digits`) and exception type (`ArgumentOutOfRangeException`) are already correct
everywhere. **No test in the repository pins the wrong text** — grepped; only
`SingleTests.cpp:403` and `DoubleTests2.cpp:352` pin messages, and both pin the
correct one.

---

## 5. Emitted text and round trips

Re-measured after #1863, because a subset decision that broke a round trip would
be worse than the subset:

| Call | Emitted | Re-parses to the same `double`? |
|---|---|---|
| `ToString(1234.5)` | `1234.5` | ✅ |
| `ToString(1234.5, "N2")` | `1,234.50` | ✅ |
| `ToString(1.25, "E2")` | `1.25E+000` | ✅ |
| `ToString(0.1, "R")` / `"G17"` | `0.1` / `0.10000000000000001` | ✅ |
| `ToString(-0.0)` | `-0` | ✅ (sign preserved) |
| `ToString(1e300, "G17")` | `1.0000000000000001e+300` | ✅ |

**Every emitted form round-trips**, including the two #1863 changed. Note the
consequence for §3: because `ToString(1234.5, "N2")` now emits a **group
separator**, and `Parse` now **accepts** one, the port's own emit→parse loop is
closed. It would *not* be closed if the grouping half of #1858/#1865 were
reverted.

---

## 6. The decision packet

### 6.1 What the evidence leaves to decide

| Question | Verdict | Needs approval? |
|---|---|---|
| numeric grouping, malformed grouping, exponent range, signed zero, numeric whitespace, overflow taxonomy | **closed** — port matches .NET on every measured row (§2) | **no** |
| emitted text and round trips | **closed** (§5) | **no** |
| `Single`/`Double` `Round` overflowing to ∞ (#1927) | **defect**, not a subset — measured (§4.1) | **yes**, one line of wording |
| `Math::Round`'s message (#1928) | **defect** — one string, no test pins it (§4.2) | **yes**, one line of wording |
| date/time grammar (#1929), six respects | **genuine policy question** (§3) | **yes**, and it is the only real one |

### 6.2 Policy options for the date/time half (#1929 only)

| | Option | What it accepts | Source | ABI | Migration risk | Tests | Consistency |
|---|---|---|---|---|---|---|---|
| **P1** | **Exact current-.NET acceptance** — unpadded fields, all-digit fractions, every offset form, whitespace | everything .NET does | none | none | **highest**: text the port has *always* rejected starts parsing, so a caller's validation-by-parse-failure silently weakens | large; every widened shape needs a value assertion | would still miss `ParseExact`/provider/`DateTimeKind`, so "exact .NET" is unreachable anyway |
| **P2** | **Keep the historical fixed-width subset**, document it permanently | today's set | none | none | **none** | few (doc-comments + the existing pins) | leaves rows 5 and 6 — the port disagreeing with *itself* — standing |
| **P3** | **Widen only unambiguous invariant forms** — whitespace (row 5), and make the port self-consistent (row 6): `DateTime`'s time fields accept 1–2 digits like `TimeOnly`'s, and its fraction accepts up to 7 digits like `TimeSpan`'s, keeping 100 ns | a strict superset of today | none | none | **low**: every widened shape is one .NET already accepts *and* one the port already accepts somewhere else | moderate; each widened shape plus every current shape unchanged | **removes both internal inconsistencies** |
| **P4** | **Strict and compatible modes** | both | **new API** | additive | low | doubles the date/time matrix | date/time types have **no style parameter at all** (§1), so this means inventing one — and .NET's own switch is `DateTimeStyles`, which the port does not have |
| **P5** | **Additive overloads / explicit options** — e.g. `TryParseExact`, or a `DateTimeParseOptions` | opt-in only | none (additive) | additive | **none** | large but self-contained | the honest long-term answer for row 4, independent of rows 1–3 |
| **P6** | **Documentation only** | today's set | none | none | none | none | same as P2, plus a stated deviation table |

Two options can be combined and one cannot: **P3 + P5 is coherent** (make the
port self-consistent now, add explicit-format API later); **P1 is not reachable**
while rows 4 and `DateTimeKind` are absent, so approving "adopt .NET's grammar"
would in practice deliver P3 plus rows 1 and 3 and still not be .NET.

### 6.3 Recommendation

**Take #1927 and #1928 now, as defect fixes. Take P3 for #1929. Leave rows 1, 3
and 4 for a separate, later decision.**

- **#1927** — measured to change nothing below the round limits and to be
  strictly more correct above them (§4.1). Recommend **approve**.
- **#1928** — one string, no test pins it, three sibling sites already correct.
  Recommend **approve**.
- **#1929 rows 5 and 6** (whitespace; `DateTime`'s time fields and fraction) —
  these are the rows where **the port contradicts itself**, and where widening
  restores agreement with both .NET *and* the port's own other parsers. A
  4–7-digit fraction is representable — `DateTime` is tick-based (§3.2) — so
  accepting it is not "refusing what we cannot represent", it is completing what
  the representation already supports. Recommend **approve**.
- **#1929 rows 1 and 3** (unpadded date fields; short/compact offsets) —
  recommend **defer**. Both are pure widenings of a subset the port has always
  had; neither fixes an inconsistency; and row 3 in particular would re-accept
  `"+2:5"` only months after #1879 was approved to reject it. Deciding them needs
  evidence about what real input the port is fed, which this repository does not
  have.
- **#1929 row 4** (`ParseExact`, providers, `DateTimeKind`) — recommend
  **defer to P5**, as new API, not as a grammar decision.

### 6.4 Exact ticket mapping and commit plan

| Ticket | Scope if approved | Files | Own commit |
|---|---|---|---|
| **#1927** | `Single::Round(float,intcs)` and `Double::Round(double,intcs)` delegate to `MathF::Round(x,digits,ToEven)` / `Math::Round(x,digits,ToEven)` | `Single.hpp`, `Double.hpp` | `fix(core): stop Single/Double Round overflowing to infinity (#1927)` |
| **#1928** | `Math.hpp:588`'s message becomes .NET's verbatim | `Math.hpp` | `fix(core): use .NET's rounding-digits message in Math::Round (#1928)` |
| **#1929a** | rows 5+6 only: date/time and `TimeSpan` parsers trim surrounding whitespace; `DateTime`'s time fields take 1–2 digits; `DateTime`/`TimeOnly` fractions take 1–7 digits at 100 ns | `DateTime.cpp`, `DateTimeOffset.cpp`, `DateOnly.cpp`, `TimeOnly.cpp`, `TimeSpan.cpp`, `DateTimeTextScanner.hpp` | `fix(core): make the date/time parsers agree with each other and with .NET on whitespace and precision (#1929 rows 5-6)` |
| **#1929b** | rows 1+3, if ever approved | same | separate commit, separate approval |
| **#1929c** | row 4, if ever approved | new API | separate ticket under P5 |

**#1927, #1928 and #1929a can be approved together** — none changes a public
signature, object layout, vtable or mangled name, and none is reachable by the
others. **#1929b must be decided separately**, because it is the only one that
re-accepts text #1879 was explicitly approved to reject.

### 6.5 Copyable approval wording

> **(1)** Approve making `System::Single::Round(float, intcs)` and
> `System::Double::Round(double, intcs)` delegate to
> `MathF::Round(x, digits, MidpointRounding::ToEven)` and
> `Math::Round(x, digits, MidpointRounding::ToEven)` exactly as .NET's
> `Single.Round`/`Double.Round` do, so that a magnitude at or above the type's
> round limit is returned unchanged instead of overflowing to infinity.
> Measured: no value below the round limit changes. No public signature or
> object-layout change. Ticket **#1927**.
>
> **(2)** Approve changing the message thrown by
> `System::Math::Round(double, intcs, MidpointRounding)` — and therefore by
> `Math::Round(double, intcs)` — from `"digits must be between 0 and 15,
> inclusive."` to .NET's verbatim `"Rounding digits must be between 0 and 15,
> inclusive."`, matching `MathF::Round`, `Single::Round` and `Double::Round`.
> Ticket **#1928**.
>
> **(3)** Approve making the date/time parsers self-consistent and
> .NET-consistent in exactly two respects: (a) `DateTime`, `DateTimeOffset`,
> `DateOnly`, `TimeOnly` and `TimeSpan` `Parse`/`TryParse` ignore leading and
> trailing whitespace, as the numeric parsers and .NET already do; and (b)
> `DateTime`'s time fields accept one or two digits as `TimeOnly`'s already do,
> and `DateTime`'s and `TimeOnly`'s fractional second accepts one to seven
> digits at 100-nanosecond resolution as `TimeSpan`'s already does. Every
> currently-accepted input must keep its exact current value. No public
> signature or object-layout change. Ticket **#1929**, rows 5 and 6 only.
>
> **(4)** *(separate decision, not bundled)* Approve **or decline** widening the
> date grammar to accept unpadded month/day (`"2024-6-15"`) and the short and
> compact time-zone offsets (`"+2"`, `"+2:5"`, `"+0205"`) that .NET accepts —
> noting this re-accepts text ticket #1879 was explicitly approved to reject.
> Ticket **#1929**, rows 1 and 3.

### 6.6 Rejected alternatives, and why

| Rejected | Why |
|---|---|
| **P1 "adopt .NET's grammar"** as a single approval | unreachable — `ParseExact`, providers and `DateTimeKind` are absent, so it would silently mean "P3 + rows 1 and 3" while reading as full parity |
| **P4 strict/compatible modes** | the date/time types have no style or provider parameter at all; this means inventing an option type .NET spells `DateTimeStyles`, for two disputed rows |
| **Reverting the numeric grouping work** | §2 measures it as a faithful port of .NET's own rule, and §5 shows the emit→parse loop now closes because of it |
| **Bundling #1929b with #1927/#1928** | it is the only row that reverses a decision the user took three weeks of tickets ago; it must be visible on its own |
| **Absorbing #1927 into #1862's approval** | #1862 covered invalid-argument *rejection*; #1927 changes a returned value. Different contract, even though measurement now shows the change is a fix |

---

## 7. The one other question the same packet must carry — #1899

`docs/OwnedTreeLifetimeContractPlan.md` §45 revised #1899's evidence this batch
and found four errors in its recorded options. The question to the user, restated
with those corrections folded in:

> For the Xml.Linq borrowed views (`Extensions::Ancestors`/`AncestorsAndSelf` —
> **four** overloads, all range-taking, two name-filtered — and
> `XElement::getAttributesProperty()`), take
> **(D)** additive visitor spellings, one per existing overload, whose borrowed
> pointers cannot outlive the call;
> **(G)** D plus `[[deprecated]]` on the borrowed spellings, so every borrowed
> call site gets a compiler diagnostic while none stops compiling;
> **(F)** an off-by-default lifetime registry that *detects* a live borrow across
> destruction in test builds at zero release cost and zero ABI; or
> **(B)** `getAttributesProperty()` by value — noting that measurement now shows
> this to be a **silent** ABI break (the Itanium mangled name does not encode the
> return type, and the accessor *is* emitted as a weak symbol), i.e. the same
> class of break the user **declined** in #1889, and noting that the accessor is
> one of **eighteen** borrowed `const&` accessors in these headers?

**Recommendation: D + G, F if a detection net is wanted, B only in a coordinated
ABI-breaking release with #1889.**

---

## 8. What this document deliberately does not do

- It does **not** implement anything. #1927, #1928 and #1929 stay `todo` and
  inactive; #1899 stays `blocked`.
- It does **not** estimate downstream migration. **CNA and mobile-eggbert were
  not inspected**, and #1773 stays `blocked`.
- It does **not** execute .NET. Every .NET row is derived from
  `/rv/tmp/runtime` source at the cited line; rows that could not be established
  that way are marked ❓ rather than guessed. None in the tables above is ❓.
- It issues **no** new `SR-AUD-*` identifier. Numbering stays frozen at **364**.
