<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `DateTime::ToString` gains the standard formats — #2416

**A behaviour change on a public member for every one-character format**, under SA-5. No signature,
layout, vtable or `noexcept` specification moved.

## The defect, measured rather than reasoned

`build-probe/1945_fmt2.cpp`, taken while landing SA-16.5:

| format | before | .NET |
|---|---|---|
| `"o"` / `"O"` | `"o"` / `"O"` | `2024-06-15T12:00:00.0000000` |
| `"u"` | `"u"` | `2024-06-15 12:00:00Z` |
| `"R"` / `"r"` | `"R"` / `"r"` | `Sat, 15 Jun 2024 12:00:00 GMT` |
| `"s"` | **`"0"`** | `2024-06-15T12:00:00` |
| `"d"` | **`"15"`** | `06/15/2024` |
| `"%d"` | **`"%15"`** | `15` |

**Three distinct defects in one member.**

1. An unrecognised standard specifier was emitted **as a literal** — `"o"` in, `"o"` out. A caller
   asking for the round-trip format silently got the letter.
2. A one-character **custom** specifier was accepted where .NET requires `%d`, so `"s"` and `"d"`
   returned a number — a **plausible wrong answer** rather than a visible failure.
3. `%` was not the escape at all, so the spelling .NET requires produced a stray percent sign.

## Why it mattered now

#2414 and #1942 gave the **parse** side a standard-format table, and #1939 had already recorded the
exact rule the format side was missing — *"A ONE-CHARACTER format that is not standard is not
silently custom either: .NET requires `%d`"*. So **the two halves of one type disagreed about what
`o` means**: `ParseExact(x, "o")` read .NET's roundtrip pattern while `ToString("o")` emitted the
letter. That is the #2393 shape, one type over.

## The table already existed and was simply not called

`DateTimeFormatInfo::GetAllDateTimePatterns(char)` carries **all nineteen** specifiers, is
culture-aware, and throws for an unknown one. `DateTime::ToString` never consulted it. **So this is
a wiring repair rather than a new table** — which also means the patterns a provider supplies are
honoured for free, the same route #1940 opened for the month and day names.

**Two exceptions, two contracts.** `ToString` raises `FormatException` for a bad specifier where
`GetAllDateTimePatterns` raises `ArgumentException` — .NET's own split, and emitting the character
as a literal (which is what this did) is neither.

## Two custom tokens came with it

The custom formatter had **no `t`/`tt`** and **no `K`**, both of which the parse side has read since
#1939 and #1942 respectively — two more rows where the halves disagreed. `o`'s pattern ends in `K`,
so the round trip needs it.

**`K` for a `Local` value emits nothing**, and that is stated rather than left to be discovered: a
local marker needs this process's zone, which `Core.Base` cannot name — SA-16.1's boundary, here
with no parameter to carry one. It is emitted empty rather than guessed. `Unspecified` emits nothing
too, which is the **same rule** as `K` matching the empty string on the parse side.

## Evidence

Seven mutations, **all caught**. The round trip is asserted as a **property** —
`ParseExact(d.ToString("o"), "o", …RoundtripKind) == d`, kind included — rather than as two tables
that happen to match today.

**A first cut of one case wrote `ToString("K")` and threw**, because a one-character format is the
*standard* reading and `K` is not one of the nineteen — **the test tripped over its own subject**.
The row is kept as evidence that the rule bites, with `"%K"` beside it.

A separate case pins that **multi-character formats are still custom**, which is the regression this
repair could most plausibly have caused, and that the no-argument `ToString()` is untouched.

Gate: **17,725 / 38, 0 failed, 0 skipped** (+5; `SharpRuntimeTests_Core_Base` 6,136 → 6,141; no
other executable moved, so nothing that formatted a date changed its answer). Downstream: **zero
sites** in both consumers.
