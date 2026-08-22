<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `XmlConvert`'s four discarded arguments — #1945

**A behaviour change on four members, under SA-5.** No signature, layout, vtable or `noexcept`
specification moved. `modules/xml` gains `TimeZone` as a **private** (and test) dependency; the
module graph goes **41 / 94 → 41 / 95** and the catalogue is regenerated.

## The defect

Four members accepted a second argument and **threw it away** — spelled `/*format*/` and
`/*mode*/` in the bodies:

```cpp
DateTime       ToDateTime(const std::string& s, const std::string& /*format*/) { return DateTime::Parse(s); }
DateTime       ToDateTime(const std::string& s, XmlDateTimeSerializationMode /*mode*/) { return DateTime::Parse(s); }
DateTimeOffset ToDateTimeOffset(const std::string& s, const std::string& /*format*/) { return DateTimeOffset::Parse(s); }
std::string    ToString(const DateTime& value, XmlDateTimeSerializationMode /*mode*/) { return value.ToString(); }
```

So `ToDateTime("2024-06-15", "HH:mm:ss")` **succeeded** and returned a date: the value was parsed by
an entirely different grammar, with no diagnostic. That is the SR-AUD-168 shape, four times over.

**The mode half carried a premise that had stopped being true.** Its comment read *"System::DateTime
does not track DateTimeKind (see its own doc-comment), so Local/Utc/Unspecified/RoundtripKind cannot
be distinguished here"*. **#1941 phase 1 gave `DateTime` a `Kind`** and **phase 2 made it convert by
that kind** — the sentence described a runtime that no longer exists.

## Why this could land while #1942 stays blocked

#1941 phase 2 had to take an `ILocalTimeZone` **as a parameter**, because `Core.Base` cannot name a
time zone and .NET reaches `TimeZoneInfo.Local` internally. **`modules/xml` is under no such
constraint**: `TimeZone` depends on `Core.Base` alone, so taking it as a private dependency is not a
cycle, and `System::TimeZone::CurrentTimeZone()` is already an `ILocalTimeZone`.

So the deviation #1941 recorded is resolved **here, by the module that can actually name the zone**,
and `XmlConvert`'s signatures stay exactly .NET's — no zone parameter, because none is needed.

## The mode matrix, transcribed cell by cell

.NET's `SwitchToLocalTime` / `SwitchToUtcTime` (`XmlConvert.cs`) — **only two of the eight cells
move the ticks**:

| | `Local` mode | `Utc` mode |
|---|---|---|
| kind `Local` | unchanged | **converts** (`ToUniversalTime`) |
| kind `Utc` | **converts** (`ToLocalTime`) | unchanged |
| kind `Unspecified` | **stamps** `Local` | **stamps** `Utc` |

`Unspecified` mode **strips** the kind while keeping the ticks; `RoundtripKind` does nothing; an
undefined value raises `ArgumentException` with .NET's verbatim text (`Sch_InvalidDateTimeOption`),
reachable only by casting in from outside the enumeration.

The switch is written **once** and shared by both doors, because .NET writes it twice and two copies
of one matrix is how two doors come to disagree.

## RoundtripKind follow-up

The first #1945 implementation recorded a real limitation: `RoundtripKind` and `Unspecified` were
then observationally identical because no XSD kind marker crossed the string boundary. That is
historical evidence, not the current contract. The later SA-16.3/SA-16.5 follow-up closed both
halves in the correct XML layer:

* `renderXsdDateTime` writes `Z` for Utc, a numeric local offset for Local, and no marker for
  Unspecified;
* `splitXsdKindMarker` reads the same three shapes and converts a numeric offset to the represented
  instant;
* both `ToDateTime` doors share that reader, and the permanent regression
  `RoundtripKindNowRoundtripsThroughAString` distinguishes RoundtripKind from Unspecified.

This deliberately does not route through general `DateTime::Parse`. Core.Base cannot obtain an
implicit local timezone without reversing the TimeZone -> Core.Base dependency; its general parser
therefore retains the documented subset rule of consuming a zone suffix without applying it.
`XmlConvert` can reach `TimeZone::CurrentTimeZone()` and is the layer that promises the complete
XSD round trip.

## `ToDateTimeOffset(s, format)`

.NET routes both format-taking doors through invariant `ParseExact` with
`AllowLeadingWhite | AllowTrailingWhite`. #1943 later added the real
`DateTimeOffset::ParseExact`, including explicit `zzz`/`K` capture, but the #1945 composition was
not revisited: it still parsed a `DateTime` and attached the process-local offset. That stale body
could not parse an explicit offset at all and rejected the outer whitespace `XmlConvert` promises.

#2418 closes that post-#1941 ripple. Both format-taking doors now call their own style-aware exact
parser with `TimeZone::CurrentTimeZone()` supplied at the C++ dependency boundary:

* `ToDateTime` can apply `z`/`K` semantics and accepts leading/trailing XML input whitespace;
* `ToDateTimeOffset` captures an explicit offset (including non-hour offsets such as `+05:30`),
  while an input without an offset still receives the process-local offset.

The format grammar therefore has one owner per result type; the obsolete DateTime-plus-offset
composition and its false claim that `DateTimeOffset::ParseExact` was absent are gone.

The same consumer pass also corrected the argument-free DateTimeOffset writer. `XmlConvert` is an
XSD conversion surface, so it now emits `yyyy-MM-ddTHH:mm:ss[.fffffff]zzz`, trims trailing
fractional zeroes, and preserves the explicit offset. Delegating to DateTimeOffset's ordinary
general `ToString()` had emitted a space-separated display string instead of an XSD `dateTime`.

## Evidence

The original seven-mutation reading was **five caught and two proven equivalences**:

* M1 (discard the format again), M2 (`Unspecified` converts instead of stamping), M3 (`Utc` stamps
  instead of converting), M5 (an undefined mode passes through), M7 (a zero offset instead of the
  local one) — **caught**.
* M4 and M6 were equivalences **at that checkpoint** because they swapped RoundtripKind and
  Unspecified while the public surface could not distinguish them. The later marker write/read
  implementation invalidated that equivalence and replaced its declaration pin with positive
  round-trip coverage. They are no longer equivalent mutations against current HEAD.

**Two mutations were invalid as first written and were reformulated rather than counted.** M2's
first spelling stamped `Utc` and *then* called `ToUniversalTime`, which is a **no-op** on a `Utc`
value — a no-op is not a mutation. M7's first spelling wrote `TimeSpan::Zero()`, and `Zero` is a
value rather than a call, so `-Werror` rejected it.

**A mistake of my own, recorded because it is the second occurrence.** The test block was appended
with `cat >>` to a path that did not exist, creating a stray untracked `XmlConvertTests.cpp` with no
includes — **exactly what #2412 recorded** for `DateOnlyTests.cpp`. The real home is
`XmlSupportTests2.cpp`. Twice is a pattern, not a slip: verify the target exists before appending.

Gate: **17,700 / 38, 0 failed, 0 skipped** (+6; `SharpRuntimeTests_Xml` 518 → 524). Downstream:
**zero sites** in both consumers.

## Not fixed here, and surfaced rather than passed over

Running `scripts/check_selective_components.sh` for this change found it **already red, and red on a
clean tree** — `forbidden_text_json_collections` compiles because #1889 legitimately gave
`Text.Json` a public `Collections.Core` dependency. It has been failing since 2026-08-19 behind a
green test count, because that script is not part of CLAUDE.md rule 2's gate. Filed as **#2415**
with the measurement and a recommended repair.
