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

## A limitation found by a test failing, and declared rather than hidden

`RoundtripKind` exists to carry a kind **through a string**, and in this port **it cannot**: this
runtime's `DateTime::ToString()` emits no kind marker — no trailing `Z`, no offset — where .NET's
`XsdDateTime` does, and `DateTime::Parse` sets no kind from one either.

Two consequences, both pinned:

* **Through the parse door, `Local` and `Utc` always stamp and never convert**, because a parsed
  value is always `Unspecified`. Through the format door — where the caller hands over a `DateTime`
  that still has its kind — they convert.
* **`RoundtripKind` and `Unspecified` are observationally identical**, measured over every input
  kind and both doors.

This is the same no-zone-token boundary #2414 recorded one level down. Closing it means teaching
`DateTime::Parse` to set a kind from a `Z`, which is **#1942's `RoundtripKind` work** and needs the
zone decision that ticket is waiting on. The pin fails the day it lands.

## `ToDateTimeOffset(s, format)`

.NET is `DateTimeOffset.ParseExact(s, format, InvariantCulture, DateTimeStyles.None)`, and **this
port has no `DateTimeOffset::ParseExact`** — an exact `DateTimeOffset` needs a zone and `Core.Base`
cannot name one, which is #1943's remaining half.

**Composing it here is not a second grammar.** With no zone token in the format — and this port's
exact grammar has none at all — .NET's `DateTimeStyles.None` gives the result the **local** offset,
so parsing with the one exact grammar and attaching the local zone's offset is what .NET computes,
not an approximation of it. A later `DateTimeOffset::ParseExact` should **absorb** this body rather
than sit beside it, and the site says so.

## Evidence

Seven mutations, **five caught, two proven equivalences**:

* M1 (discard the format again), M2 (`Unspecified` converts instead of stamping), M3 (`Utc` stamps
  instead of converting), M5 (an undefined mode passes through), M7 (a zero offset instead of the
  local one) — **caught**.
* **M4 and M6 are equivalences, and the proof is the limitation above**: they swap the
  `RoundtripKind` and `Unspecified` arms' bodies, and those two modes are indistinguishable through
  the public surface. An assertion that could catch them would have to distinguish two modes this
  port cannot distinguish, and both mutations preserve the equality the declaration test asserts.
  The arms are kept apart because they are .NET's and because they separate when #1942 lands.

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
