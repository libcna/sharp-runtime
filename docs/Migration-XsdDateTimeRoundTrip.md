<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `DateTimeOffset::ParseExact`, and `XmlConvert`'s XSD round trip — SA-16.2, SA-16.3, SA-16.5, SA-16.6

Two halves landed together, because the second needed the first's grammar.

## Half A — `DateTimeOffset::ParseExact` (#1943, SA-16.2), which **closes #1943**

**An offset is not a time zone**, and that measurement is what made this possible. A format carrying
an explicit offset (`zzz` or `K`) needs **no zone database whatever**: the offset is read from the
input and stored, `DateTimeOffset` being a `DateTime` plus a `TimeSpan`. #1942 had already added the
offset token to the exact grammar, so this half is the type that can actually carry one.

**The value is captured, not adjusted.** This is not the `DateTime` matrix with a different result
type: the parsed wall-clock time stays exactly as written and only the *offset* is chosen. Mutation
M1 adjusts it and is caught by four cases.

Only the **no-offset** case needs a zone, because .NET's `DateTimeOffsetTimeZonePostProcessing`
gives such a result the **local** offset — its own comment says *"AssumeLocal causes the offset to
default to Local. This flag is on by default for DateTimeOffset."*

**SA-16.6 accepted the cost knowingly, and it is larger here than for `DateTime`.** There only a few
styles convert; here **every format without an offset token** needs a zone, so the most ordinary
call raises `ArgumentNullException`. The message therefore names **all three routes out** —
`CurrentTimeZone()`, a `zzz`/`K` format, or `AssumeUniversal` — because a caller who hits it has
three genuinely different fixes and no way to guess them from *"zone was null"*.

The parameter name is **`styles`** here where `DateTime`'s is **`style`**. .NET varies it **by
overload** rather than using one name, and both are transcribed as they are rather than harmonised.
`DateTimeOffset.hpp`'s "out of scope" list is corrected in place — leaving a header describing an
absence it no longer has is the SR-AUD-168 defect.

## Half B — `XmlConvert` round-trips a kind (#1945 extended, SA-16.3 + SA-16.5)

**#1945 declared that a kind could not cross a string in this port** and pinned it, writing that the
pin *"fails the day #1942 teaches `Parse` to read a `Z`"*. **The decision went further than that
sentence.**

**The writing half (SA-16.5): the full `XsdDateTime` form.** Today's `2024-06-15 12:00:00` becomes
`2024-06-15T12:00:00Z` — **two changes, not one**. Appending only the marker would have repaired the
round trip and still left the document wrong, because an XSD `dateTime` literal requires the `T`.
The fraction is emitted only when non-zero and its trailing zeroes are **trimmed** (`.5`, not
`.5000000`), and an **`Unspecified` value writes no marker at all**, which is what stops a value
acquiring a kind it never had.

**The reading half does not go through `DateTime::Parse`, and that is deliberate.** SA-16.4 left the
general `Parse` alone — it still parses a zone and **discards** it — so a round trip built on it
could never carry a kind however the writing half rendered. **.NET does not use `DateTime.Parse`
here either**: `XmlConvert.ToDateTime` builds an `XsdDateTime`, which parses the zone itself. So the
marker is split off before `Parse` ever sees the text.

**A numeric offset is converted, not stamped.** It names an *instant*, so merely stamping would make
`+05:00` and `+02:00` produce the same local wall-clock time — the offset read and thrown away
again, which is the defect this ticket exists to end.

**The marker is matched as a SHAPE**, `+hh:mm` / `-hh:mm`, rather than by scanning backwards for a
sign: `2024-06-15` ends in `06-15`, which a looser rule accepts as an offset. Mutation M5 writes the
looser rule and is caught.

**Both `ToDateTime` doors go through the same one.** The mode-taking overload delegates to the
one-argument form rather than calling `Parse` itself; two doors reading one grammar two ways is the
#2393 shape, and mutation M6 restores it and is caught.

**#1945's declaration pin is inverted, not deleted**, and its recorded proven equivalence is over:
`RoundtripKind` and `Unspecified` are now distinguishable, which is what #1945's mutations M4 and M6
relied on being false.

## Evidence

**Half A: seven mutations, all caught.** M3 was **invalid as first written twice and is recorded
rather than counted**: the first spelling left `zone->GetUtcOffset` running after the guard, so it
segfaulted — and a segfault is not a verdict, it is undefined behaviour the harness must not read as
a pass. Reformulated as the plausible *"be lenient, default to zero"* repair, it is caught.

**Half B: seven mutations, all caught.**

Gate: **17,720 / 38, 0 failed, 0 skipped** (+6; `Core_Base` 6,132 → 6,136, `Xml` 524 → 526). Module
graph **41 / 95**. Downstream: **zero sites** in both consumers.

## Found on the way and filed rather than bundled

**#2416.** A probe taken to check the format side measured that **`DateTime::ToString` has no
standard-format table at all**: `ToString("o")` emits the literal `"o"`, `ToString("s")` returns
`"0"` (reading `s` as *seconds*), and `"%d"` renders `"%15"` (the `%` escape is not honoured). So
after #2414 and #1942 gave the **parse** side a standard table, **the two halves of one type
disagree about what `o` means** — `ParseExact(x, "o")` reads .NET's roundtrip pattern while
`ToString("o")` emits the letter. That is the #2393 shape one type over, it is a behaviour change on
a public member for every one-character format, and it is its own ticket.
