# Date/time exact parsing, providers, styles, and kind design

**Ticket:** #1938, design for #1929 row 4
**Date:** 2026-08-02
**Scope:** design and retained characterization only; no production behavior is approved or changed

## Decision summary

#1929 row 4 is not one implementation decision. The historical premise that the
port merely lacks some `ParseExact` semantics is incomplete: all public exact
parsing overloads are absent, the declared provider and culture types are not
wired together, styles are declared but unused, and `DateTimeKind` is declared
without storage or behavior. The work therefore has separate source/API, ABI,
culture, validation, timezone, and bridge boundaries.

The recommended decomposition is:

1. **4A:** add a deliberately invariant, string-only, single-format
   `ParseExact`/`TryParseExact` surface for `DateOnly` and `TimeOnly`, backed by a
   reusable exact-format scanner. This is the only group suitable for the next
   approval batch.
2. **4B:** repair and connect the provider/culture model before adding any
   provider-taking date/time API.
3. **4C:** specify and implement style validation and style-controlled parsing
   effects after provider and kind prerequisites exist.
4. **4D:** add `DateTimeKind` storage and behavior as an independent
   representation/API decision; local-time conversion remains dependent on a
   reliable timezone model.
5. **4E:** add the remaining single-format, provider-taking, multi-format, and
   span-like exact overloads in bounded stages after 4B--4D.
6. **4F:** correct `XmlConvert` format/mode bridges only after their target exact
   and kind contracts exist.

No provider parameter may be accepted and ignored. No exact API may silently
reuse the current permissive/ad-hoc formatting implementation as its grammar.
No kind bit may be invented without an explicit representation decision.

## Authoritative row definitions and unchanged rows 1--3

The historical #1929 description predates several remediations. Its durable row
definitions, after the appended correction notes in the existing decision
packet, are:

| Row | Decision boundary | 2026-08-02 state |
|---|---|---|
| 1 | General parsing of unpadded date fields | Unapproved and unimplemented |
| 2 | General fractional parsing beyond seven digits, including rounding | Unapproved and unimplemented; one through seven digits are already exact |
| 3 | General parsing of short or compact timezone offsets | Unapproved and unimplemented |
| 4 | Exact parsing, provider/culture use, styles, and `DateTimeKind` | Designed here; no implementation approved |
| 5 | 100-nanosecond preservation through seven fractional digits | Previously approved and remediated |
| 6 | `TimeOnly` full-tick construction/conversion | Previously approved and remediated |

### Row 1: unpadded dates

Current `DateTime`, `DateTimeOffset`, and `DateOnly` general parsing accepts the
fixed-width `yyyy-MM-dd` form. Characterization accepted `2024-06-15` and
rejected `2024-6-15` and `2024-06-5`. Current .NET general parsing accepts these
unpadded examples. Widening would turn inputs currently useful as validation
failures into values and could make previously unambiguous separators/field
boundaries ambiguous. The recommendation remains to preserve and document the
narrow fixed-width general grammar.

Row 1 does not require ParseExact. A future approved exact custom format such as
`M/d/yyyy` can opt into widths defined by that format without widening general
`Parse`.

### Row 2 remainder: fractions beyond seven digits

`DateTime`, `DateTimeOffset`, `TimeOnly`, and `TimeSpan` accept one through seven
fractional digits and preserve each 100-nanosecond tick. The retained probe
measured:

| Input | Current port result |
|---|---|
| `2024-06-15T10:20:30.1234567` | `638540436301234567` ticks |
| `10:20:30.1234567` as `TimeOnly` | `372301234567` ticks |
| `10:20:30.1234567` as `TimeSpan` | `372301234567` ticks |
| Any corresponding eighth fractional digit | rejected |

The pinned .NET general date/time parser consumes the complete ASCII fraction,
converts it to ticks, and rounds. For example `.12345674` rounds to `1,234,567`
fraction ticks and `.12345676` to `1,234,568`. Exact formats use at most seven
`f`/`F` specifiers; this general-parser remainder is not a reason to allow an
eighth exact-format digit.

`DateTimeOffset` inherits the date/time clock precision and adds its offset.
`TimeOnly` and `TimeSpan` preserve the same tick unit, although .NET
`TimeSpan.ParseExact` has its own standard/custom duration grammar.
`XmlConvert.ToDateTime` ultimately observes date/time parsing, while .NET
`XmlConvert.ToTimeSpan` uses XML Schema duration rules and is a separate
SR-AUD-354 concern. The recommendation remains to reject fractions longer than
seven digits rather than introduce rounding into the port.

### Row 3: short and compact offsets

The current accepted suffix grammar for `DateTime` and `DateTimeOffset` is
`Z`/`z` or exactly `+HH:MM`/`-HH:MM`. The retained probe accepted `+02:05` and
rejected `+2:5`, `+2`, and `+0205`. Current .NET general parsing accepts the
short forms, including `+2:5` as 125 minutes. Widening creates new boundary and
round-trip ambiguities, especially beside time digits.

`DateTimeOffset` preserves `+02:05` as an offset of `75,000,000,000` ticks. The
current port's `DateTime` accepts `Z` and `+02:05` but discards both: the no-zone,
`Z`, and offset-bearing examples produced identical clock ticks and there is no
kind state. The recommendation remains to preserve and document the exact
`HH:MM` general grammar. Correct kind/offset semantics are row 4D and do not
authorize row 3 widening.

No measurement contradicted the current row 1--3 correction notes, so this
document does not rewrite their historical wording.

## Corrected row-4 premises

1. All five exact families are **missing APIs**, not incorrectly implemented
   overloads. The compile-time absence is the current ParseExact/TryParseExact
   result.
2. `System::DateTimeKind` exists (`Unspecified=0`, `Utc=1`, `Local=2`), but
   `DateTime` has no kind field, property, kind-taking constructor, or kind-aware
   conversion.
3. `System::Globalization::DateTimeStyles` and `TimeSpanStyles` exist, but no
   date/time parser consumes them.
4. `IFormatProvider` exists, but no production type derives from it.
   `CultureInfo`, `DateTimeFormatInfo`, and `NumberFormatInfo` therefore cannot
   currently be passed through a .NET-like provider path.
5. `DateTimeFormatInfo::CurrentInfo` is invariant rather than being obtained
   from `CultureInfo::CurrentCulture`; current culture is mutable process-global
   state and is already covered by open SR-AUD-280. Unknown culture metadata is
   covered by open SR-AUD-285.
6. Core.Base cannot simply include Globalization parsing types: the current
   component direction is Globalization to Core.Base, and the Core.Base test
   dependency on Globalization does not authorize a production cycle.
7. The available local-timezone model caches a current OS offset and lacks
   historical transition/DST rules. It is insufficient for general .NET kind
   conversion semantics.
8. `XmlConvert` format and serialization-mode overloads exist but currently
   ignore their controlling argument and delegate to general parsing. Those are
   existing-body bridge defects, not proof that exact APIs exist.

## Current public surface and implementation inventory

`provider` below means a .NET-shaped `IFormatProvider*`; `styles` means the
appropriate declared enum. “Absent” means no declaration, symbol, or body.

| Type | Current parsing declarations | Exact single | Exact multiple | Provider/style exact | Implementation/delegation |
|---|---|---|---|---|---|
| `DateTime` | `Parse(const std::string&)`; `TryParse(const std::string&, DateTime&)` | Absent | Absent | Absent | `modules/core/src/System/DateTime.cpp`; independent invariant scanner |
| `DateTimeOffset` | same two shapes | Absent | Absent | Absent | `modules/core/src/System/DateTimeOffset.cpp`; extracts strict suffix, delegates clock text to `DateTime` |
| `DateOnly` | same two shapes | Absent | Absent | Absent | `modules/core/src/System/DateOnly.cpp`; independent fixed date scanner |
| `TimeOnly` | same two shapes | Absent | Absent | Absent | `modules/core/src/System/TimeOnly.cpp`; independent time scanner |
| `TimeSpan` | same two shapes | Absent | Absent | Absent | `modules/core/src/System/TimeSpan.cpp`; independent duration scanner |

No type has span-based parsing, an array/vector of candidate formats, a
provider-taking general parser, or a styles-taking general parser. No overload
omitting provider/styles can delegate to a richer overload because the richer
overload is absent.

Current general parsing assumes invariant ASCII digits and fixed punctuation,
trims outer ASCII whitespace, and rejects trailing non-whitespace input. The
five `Parse` functions throw `FormatException` with HResult `0x80131537`. Their
current stable English messages are:

| Type | Current message |
|---|---|
| `DateTime` | `String was not recognized as a valid DateTime: X` |
| `DateTimeOffset` | `String was not recognized as a valid DateTimeOffset.` |
| `DateOnly` | `String was not recognized as a valid DateOnly: X` |
| `TimeOnly` | `String was not recognized as a valid TimeOnly: X` |
| `TimeSpan` | `String was not recognized as a valid TimeSpan: X` |

On `TryParse` failure, the first four types assign `MinValue`. `TimeSpan`
currently leaves the caller's output unchanged. Future exact APIs must choose
their failure-output contract explicitly; group 4A selects `MinValue` for its
two types and does not alter general `TryParse`.

### Related bridges and callers

| Surface | Current behavior | Row-4 treatment |
|---|---|---|
| `XmlConvert::ToDateTime(s, format)` | ignores `format`, calls general `DateTime::Parse` | 4F existing-body semantic correction |
| `XmlConvert::ToDateTimeOffset(s, format)` | ignores `format`, calls general parse | 4F existing-body semantic correction |
| `XmlConvert::ToDateTime(s, mode)` | ignores mode/kind conversion | 4F after 4D |
| XML multi-format overloads present in .NET | absent | 4F additive API/symbol decision |
| HTTP RFC1123 header parsers | independent exact-shaped scanners | explicitly unchanged; no automatic delegation |
| cookie parsing | calls general `DateTime::TryParse` | explicitly unchanged |
| `ToString(format)` implementations | ad-hoc subsets; unsupported tokens can become literals | evidence only; not an exact-parser grammar oracle |

## Pinned current .NET evidence

The reference is dotnet/runtime commit
[`0eb5481340ea675857c7a7abf18f68a60b52a686`](https://github.com/dotnet/runtime/tree/0eb5481340ea675857c7a7abf18f68a60b52a686),
dated 2026-08-01. Source and tests were inspected at this immutable commit:

- [`DateTime.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/DateTime.cs)
- [`DateTimeOffset.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/DateTimeOffset.cs)
- [`DateOnly.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/DateOnly.cs)
- [`TimeOnly.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/TimeOnly.cs)
- [`TimeSpan.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/TimeSpan.cs)
- [`DateTimeParse.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/Globalization/DateTimeParse.cs)
- [`DateTimeFormatInfo.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.CoreLib/src/System/Globalization/DateTimeFormatInfo.cs)
- [`XmlConvert.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Private.Xml/src/System/Xml/XmlConvert.cs)
- System.Runtime tests named `DateTimeTests.cs`, `DateTimeOffsetTests.cs`,
  `DateOnlyTests.cs`, `TimeOnlyTests.cs`, and `TimeSpanTests.cs` under
  `src/libraries/System.Runtime/tests/System.Runtime.Tests/System/`.

Documentation is useful for intent; measured/reference test behavior and these
bodies are primary where wording differs.

## Reference overload inventory

The following counts include string and span shapes; they are not a proposal to
add all shapes at once.

| Type | .NET ParseExact family | .NET TryParseExact family |
|---|---|---|
| `DateTime` | string/span single format and string/span multi-format; provider; style where applicable (5 shapes) | string/span single and multi-format with provider/style (4 shapes) |
| `DateTimeOffset` | analogous 5 shapes | analogous 4 shapes |
| `DateOnly` | string/span single and multi-format, each providerless or provider-taking (7 shapes) | same four axes, 8 shapes |
| `TimeOnly` | same as `DateOnly` | same as `DateOnly` |
| `TimeSpan` | string/span single and multi-format, with provider and optional `TimeSpanStyles` (6 shapes) | single/multi string/span with provider and optional styles (8 shapes) |

For the port, every public overload would be a source/API addition and a new
linkable symbol. A C++ span abstraction, nullable input, and nullable candidate
array do not presently exist on these types and must not be invented as an
incidental translation detail.

## Exact-format grammar matrix

### Reading the matrix

For every row, sharp-runtime `ParseExact` and `TryParseExact` are **API absent**;
there is no runtime result to characterize. `P` means the pinned .NET
`ParseExact` succeeds; `T` means its `TryParseExact` succeeds. `F/F` means
`ParseExact` throws `FormatException` and Try returns `false` with the type's
default output, unless the taxonomy column says otherwise. Exact parsing
consumes the complete input or rejects it; it never reports a partial consumed
prefix.

The matrix records representative rather than locale-exhaustive values. Ticks
are 100-nanosecond ticks; the measured port day number for 2024-06-15 is
`739051`.

| ID | Type/category; input / format | Pinned .NET exact result and value | Kind/ticks/day | Failure/status taxonomy | Divergence |
|---|---|---|---|---|---|
| G01 | `DateTime` round-trip; `2024-06-15T10:20:30.1234567` / `O` | P/T, same clock | Unspecified; `638540436301234567` | full match | missing functionality |
| G02 | same with `Z` / `O`, `None` | P/T, converted according to local zone | Local; zone-dependent ticks | full match | missing + timezone dependency |
| G03 | same with `Z` / `O`, `RoundtripKind` | P/T, same UTC clock | Utc; `638540436301234567` | full match | missing + kind dependency |
| G04 | `O` with six fractions or unpadded date/time | F/F | default | fixed widths and exactly seven fractions | intentional candidate for rejection |
| G05 | sortable; `2024-06-15T10:20:30` / `s` | P/T | Unspecified; `638540436300000000` | full match | missing |
| G06 | universal sortable; `2024-06-15 10:20:30Z` / `u` | P/T | Unspecified by exact parse without kind style | same clock ticks | missing; standard format semantics must be pinned |
| G07 | RFC1123 English; `Sat, 15 Jun 2024 10:20:30 GMT` / `r` | P/T | Unspecified; same clock ticks | fixed English literal grammar | missing; HTTP scanner is separate |
| G08 | date-only standard; DateOnly `2024-06-15` / `O` | P/T, 2024-06-15 | day `739051` | full match | proposed 4A |
| G09 | DateOnly RFC1123; `Sat, 15 Jun 2024` / `R` | P/T, 2024-06-15 | day `739051` | invariant English | proposed 4A |
| G10 | TimeOnly round-trip; `10:20:30.1234567` / `O` | P/T | `372301234567` ticks | exactly seven fractions | proposed 4A |
| G11 | TimeOnly short round-trip; `10:20:30` / `R` | P/T | `372300000000` ticks | no fraction | proposed 4A |
| G12 | custom numeric; `2024-6-5` / `yyyy-M-d` | P/T, 2024-06-05 | day/ticks for that date | width follows each token | proposed DateOnly 4A; no general-parse widening |
| G13 | one-character custom; `6` / `%M` | P/T when other fields default/are valid for target | target-dependent | `%` forces a custom one-token format | scanner capability; date defaulting excluded from 4A |
| G14 | repeated specifier; `6` / `MM` | F/F | default | requires two digits | proposed 4A |
| G15 | single-quoted literal; `2024-x-06-15` / `yyyy-'x'-MM-dd` | P/T | day `739051` | exact literal | proposed 4A |
| G16 | double-quoted literal; same / `yyyy-"x"-MM-dd` | P/T | day `739051` | exact literal | proposed 4A |
| G17 | escaped character; `2024/06/15` / `yyyy\\/MM\\/dd` | P/T | day `739051` | slash is literal, not date separator | proposed 4A |
| G18 | culture date separator; `06.15.2024` / `MM/dd/yyyy`, provider with `.` | P/T | date clock/day | `/` expands to provider separator | missing; 4B/4E, excluded 4A |
| G19 | culture time separator; `10.20.30` / `HH:mm:ss`, provider with `.` | P/T | `372300000000` ticks | `:` expands to provider separator | missing; 4B/4E, excluded 4A |
| G20 | literal separator; `06/15/2024` / `MM'/'dd'/'yyyy` | P/T regardless of provider separator | date clock/day | quoted literal | proposed invariant literal support |
| G21 | fixed fraction; `10:20:30.123` / `HH:mm:ss.fff` | P/T | `372301230000` ticks | all `f` digits required | proposed 4A |
| G22 | optional fraction; `10:20:30.12` / `HH:mm:ss.FFF` | P/T | `372301200000` ticks | omitted low digits are zero | proposed 4A |
| G23 | optional fraction and dot; `10:20:30` / `HH:mm:ss.FFF` | P/T; optional separator behavior is handled by .NET parser | `372300000000` ticks | exact source/test rule, not formatting guess | proposed 4A with explicit tests |
| G24 | eighth fraction specifier / `ffffffff` or 8 digits | F/F or malformed format | default | at most seven | intentional rejection |
| G25 | AM/PM; `10:20 PM` / `hh:mm tt` invariant | P/T, 22:20 | `804000000000` ticks | exact invariant designator | proposed TimeOnly 4A |
| G26 | missing/incorrect AM/PM with `tt` | F/F | default | required designator | proposed rejection |
| G27 | era; date plus `g`/`gg`, valid provider/calendar | P/T where calendar supports it | calendar-derived date | provider/calendar semantics | 4B/4E only; excluded 4A |
| G28 | offset; `2024-06-15 10:20 +02:05` / `yyyy-MM-dd HH:mm zzz` | P/T | DateTime local/default conversion; DTO preserves +02:05 | exact offset token | 4C/4D/4E; does not widen row 3 general grammar |
| G29 | single `z`; `+2` with `z` | P/T where value/range valid | offset-dependent | format explicitly requests short offset | 4E only; not row 3 approval |
| G30 | kind token; `...Z` / `yyyy-MM-ddTHH:mm:ssK` | P/T | style and suffix determine kind | `K` accepts kind representations | 4D/4E only |
| G31 | leading/trailing whitespace, style `None` | F/F | default | exact outer whitespace rejected | proposed 4A invariant rule |
| G32 | same with matching AllowLeading/TrailingWhite | P/T | parsed target value | style validation precedes scan | 4C only |
| G33 | inner whitespace, `AllowInnerWhite` | P/T only in positions allowed by exact parser | target value | not arbitrary deletion | 4C only |
| G34 | trailing input after otherwise valid exact text | F/F | default | full consumption required | proposed 4A |
| G35 | missing required input field | F/F | default | full format must be satisfied | proposed 4A |
| G36 | empty input or empty format | F/F | default | `FormatException`; Try false | proposed 4A for representable strings |
| G37 | null input/format | Parse throws `ArgumentNullException`; Try normally false for input/format | default | parameter names vary by type (`s`/`input`, `format`) | C++ `std::string` cannot express null; API-design boundary |
| G38 | unmatched quote, terminal escape, or invalid `%` format | F/F in the applicable exact family | default | malformed format taxonomy can be type-specific | proposed 4A `FormatException` |
| G39 | unsupported custom token | F/F or treated as literal only where .NET custom grammar specifies literal behavior | default | must be token-specific | 4A uses an explicit allowlist, never formatting fallback |
| G40 | two candidate formats, first fails/second succeeds | P/T with second result | second result's kind/ticks | ordered complete attempts | 4E multi-format only |
| G41 | duplicate successful candidate formats | P/T, first success | same result | duplicates are harmless | 4E multi-format only |
| G42 | empty candidate list | Parse `FormatException`; Try false/default in base families | default | exact type tests are authoritative | 4E only |
| G43 | null candidate list/null member | argument or format exception depending type/shape; DateOnly/TimeOnly tests pin null/empty members as `FormatException` even in Try | default | C++ representation must be explicit | 4E API-design boundary |
| G44 | provider month/day name, e.g. localized `dddd, dd MMMM yyyy` | P/T with matching `DateTimeFormatInfo` | calendar-derived date/day | provider names/case rules | 4B/4E only |
| G45 | non-invariant separator forms | P/T when provided format data defines them | target value | provider-owned separator matching | 4B/4E only |
| G46 | non-ASCII/native digit forms | provider/calendar-specific; no blanket acceptance is established | target-specific | must be tested per provider | no policy approval; do not assume support |
| G47 | TimeSpan standard constant; `10:20:30.1234567` / `c` | P/T | `372301234567` ticks | invariant constant grammar | 4E |
| G48 | TimeSpan `g`/`G` | P/T for provider-specific standard grammar | exact duration ticks | provider affects separators/pattern | 4B/4E |
| G49 | TimeSpan custom `d.hh:mm:ss.fffffff` with escaped separators | P/T | exact duration ticks | literal separators must be escaped/quoted | 4E |
| G50 | TimeSpan out-of-range exact input | single Parse can throw `OverflowException`; multi-format Parse reclassifies failure as `FormatException`; Try false/default | default | HResult `0x80131516` for overflow | 4E taxonomy must be pinned |

The standard `d`, `D`, `f`, `F`, `g`, `G`, `M`/`m`, `t`, `T`, `U`, and
`y`/`Y` date/time formats are provider pattern lookups in .NET. They belong to
4B/4E, not the invariant 4A subset. Standard `O`, `R`, `s`, and `u` have
well-defined invariant expansions but date/time kind effects still keep
`DateTime`/`DateTimeOffset` out of 4A.

### Exception and validation observations

- .NET exact `FormatException` uses HResult `0x80131537`;
  `ArgumentException` uses `0x80070057`, `ArgumentNullException`
  `0x80004003`, and `OverflowException` `0x80131516`.
- Stable English messages must be pinned only where this repository elects to
  make them a contract. Localized .NET resource text is not a portable message
  contract.
- In the DateTime and DateTimeOffset string style overloads, reference bodies
  validate styles before testing a null input. DateOnly/TimeOnly string wrappers
  perform their own null/format checks before the span core in several shapes.
- An invalid style is an exception even for `TryParseExact`; Try is not a
  blanket no-throw API.

## Provider and culture analysis

### Current port model

| Concern | Actual 2026-08-02 behavior | Consequence |
|---|---|---|
| `IFormatProvider` | Abstract Core.Base interface with virtual destructor and `void* GetFormat(const std::type_info&) const` | Custom providers are expressible, but no production implementation exists |
| `CultureInfo` | Globalization value type; does not derive from `IFormatProvider` | Cannot be passed where `IFormatProvider*` is expected |
| `DateTimeFormatInfo` | Globalization value type; does not derive from provider; invariant-oriented data | Cannot be directly selected by provider dispatch |
| `NumberFormatInfo` | Also disconnected from provider dispatch | Numeric culture behavior does not automatically help date/time parsing |
| invariant provider | No production invariant `IFormatProvider` object | Only a providerless invariant policy is currently usable |
| null provider | Representable as a pointer, but no date/time overload accepts one | Semantics are not yet defined in the port |
| current culture | Mutable process-global `CultureInfo`; not thread-local | Racy/non-.NET state, open SR-AUD-280 |
| current date/time format | `DateTimeFormatInfo::CurrentInfo` behaves invariantly rather than following `CultureInfo::CurrentCulture` | “Use current culture” would be internally inconsistent |
| cached data | Culture objects contain substantial embedded format objects | New inheritance/base state would propagate layout changes |
| unsupported provider | No date/time path exists | Must not be accepted and ignored in a future path |
| custom provider | A test-only null provider exists; no production consumer asks it for `DateTimeFormatInfo` | Contract is unproved |

Measured object layouts on this toolchain were `CultureInfo` 3344 bytes,
`DateTimeFormatInfo` 2704 bytes, and `NumberFormatInfo` 600 bytes, all with
8-byte alignment. Making these types polymorphic or adding an
`IFormatProvider` base can alter their object layout, vtables, construction,
and mangled symbols. The main module direction is also material:
`Globalization` publicly depends on `Core.Base`; the reverse dependency shown
for Core.Base is test-only.

### Pinned .NET provider resolution

`DateTimeFormatInfo.GetInstance(provider)` uses the current culture for null,
returns cached data for a `CultureInfo`, returns a direct
`DateTimeFormatInfo`, asks a custom provider for `DateTimeFormatInfo`, and falls
back to the current culture when that request returns null. This depends on a
coherent current-culture model, real provider implementations, calendar data,
localized names/patterns, and stable object ownership.

### Viable policies

| Policy | Exact supported subset and fallback | API/dependencies | Cost/risk | Decision |
|---|---|---|---|---|
| P0 invariant providerless | no provider argument; invariant ASCII digits, invariant names/punctuation; no fallback | new exact overloads only | lowest; explicit divergence | selected for 4A only |
| P1 provider accepted but ignored | any pointer behaves invariantly | misleading provider overloads | silent semantic trap and future migration break | rejected |
| P2 null/invariant plus custom `IFormatProvider` returning existing `DateTimeFormatInfo` | null means invariant until current culture is repaired; returned DTFI is honored; unsupported/null return falls back invariant | Core parser would need a culture-neutral descriptor or a new Core-to-Globalization edge | medium runtime and ownership work; differs from .NET null/current | possible transitional 4B option, not selected |
| P3 make `CultureInfo` and `DateTimeFormatInfo` providers and match .NET lookup | null/current, both known concrete types, custom providers, current fallback | provider inheritance, module ownership refactor, SR-AUD-280/285 resolution, calendar/name data | high source/ABI/layout/vtable and concurrency risk | recommended end state, blocked |
| P4 expose parsing only through a Globalization facade | full culture data available without reversing module edge | non-.NET public API and duplicate type entry points | permanent source divergence | rejected |

The selected long-term policy is P3, but it is not implementation-ready and
must not be approved together with 4A. 4B must first choose an ABI transition
for provider types, restore a coherent per-thread or otherwise explicitly
synchronized current culture, define unknown-culture behavior, and route a
minimal immutable parsing view across the component boundary. Null then means
current culture; known and custom providers are honored; a custom provider that
returns no date/time format falls back to current culture, matching the pinned
reference. There is no accepted “invariant fallback on all errors” policy.

## DateTimeStyles matrix

The port declares the values below in Globalization, while all target parsers
are in Core.Base and accept none of them. Adding these enum types directly to
Core.Base declarations without changing ownership would create a component
dependency violation.

| Style/value | Pinned .NET legality and exact-parse effect | Current port | Dependency/decision |
|---|---|---|---|
| `None` (`0`) | exact format only; default date/time zone rules apply | declared, unused | 4C after exact surface exists |
| `AllowLeadingWhite` (`1`) | permits leading whitespace | declared, unused | 4C scanner policy |
| `AllowTrailingWhite` (`2`) | permits trailing whitespace | declared, unused | 4C scanner policy |
| `AllowInnerWhite` (`4`) | permits parser-defined extra inner whitespace | declared, unused | 4C; must not mean arbitrary character deletion |
| `AllowWhiteSpaces` (`7`) | union of the previous three | declared, unused | 4C |
| `NoCurrentDateDefault` (`8`) | DateTime: missing date defaults to 0001-01-01 rather than current date | declared, unused | provider/current-date dependency; not useful for DateOnly exact subset that requires a complete date |
| `AdjustToUniversal` (`16`) | normalizes DateTime to UTC; normalizes DateTimeOffset to offset zero | declared, unused | 4D plus timezone |
| `AssumeLocal` (`32`) | no-zone DateTime is local; no-zone DTO uses local offset | declared, unused | 4D plus reliable local zone |
| `AssumeUniversal` (`64`) | no-zone input is assumed UTC; result conversion also depends on `AdjustToUniversal` | declared, unused | 4D plus reliable local zone |
| `RoundtripKind` (`128`) | preserves `Utc` for a round-trip `Z`; otherwise participates in kind rules | declared, unused | 4D |

### Legal combinations and validation

| Family | Allowed bit domain | Combinations rejected by pinned .NET | Parameter | Port policy boundary |
|---|---|---|---|---|
| `DateTime` | all declared DateTimeStyles bits | undeclared bits; `AssumeLocal|AssumeUniversal`; `RoundtripKind` with any of `AssumeLocal`, `AssumeUniversal`, `AdjustToUniversal` | `style` | reproduce in 4C, before input parsing |
| `DateTimeOffset` | date/time bits except effective `NoCurrentDateDefault` | same assume conflict; Roundtrip conflict; `NoCurrentDateDefault` rejected; reference compatibility path strips `RoundtripKind` and `AssumeLocal` after validation | `styles` | reproduce only with a pinned compatibility test |
| `DateOnly` | `AllowWhiteSpaces` mask only | every other bit or undeclared bit | `style` | optional later 4C overload; 4A has no style argument |
| `TimeOnly` | `AllowWhiteSpaces` mask only | every other bit or undeclared bit | `style` | optional later 4C overload; 4A has no style argument |
| `TimeSpan` | `None` or `AssumeNegative` | every other numeric value | `styles` | 4C/4E; `AssumeNegative` affects custom formats, not `c/g/G` |

Invalid styles produce `ArgumentException` (`0x80070057`), not a parse failure,
and `TryParseExact` also throws. Validation order must be tested separately for
each overload shape; it cannot be generalized from one wrapper. No current
port stable message exists. A future implementation should pin exception type,
HResult, and parameter name, while treating the localized body text as
non-contractual unless separately approved.

## DateTimeKind matrix

Current `DateTime` stores only ticks. Its measured layout is 16 bytes with
8-byte alignment; `DateTimeOffset` is 48/8, `DateOnly` 12/4, `TimeOnly` 16/4,
and `TimeSpan` 24/8. In the retained probe, `DateTime` parsed the same
`638540436300000000` clock ticks for no offset, `Z`, and `+02:05`.

The .NET results below describe the pinned reference. “Local ticks” require the
controlled test timezone and can vary around daylight-saving transitions.

| Input/style | Pinned `DateTime` value/kind | Pinned `DateTimeOffset` | Current port | Required group |
|---|---|---|---|---|
| no zone, `None` | clock unchanged, Unspecified | local offset attached | clock unchanged, no kind; DTO uses current-offset helper | 4D/timezone |
| no zone, `AssumeLocal` | clock unchanged, Local | local offset attached (compatibility validation strips flag) | style overload absent | 4C/4D |
| no zone, `AssumeUniversal` without adjust | input treated UTC then converted to Local | zero offset is assumed/preserved unless later normalization | absent | 4C/4D/timezone |
| no zone, `AssumeUniversal|AdjustToUniversal` | clock unchanged, Utc | zero offset, UTC clock | absent | 4C/4D |
| `Z`, `None` | converted to Local | offset zero represents same instant | suffix discarded by DateTime; DTO offset preserved | 4D/timezone |
| `Z`, `RoundtripKind` | UTC clock, Utc | Roundtrip flag has no DateTimeOffset kind to preserve | absent | 4D |
| numeric offset, `None` | converted to Local instant, Local | supplied offset preserved | offset discarded by DateTime; DTO preserves strict `HH:MM` | 4D/timezone |
| numeric offset, `AdjustToUniversal` | UTC instant, Utc | UTC instant with zero offset | absent | 4C/4D |
| no zone round-trip format/reparse | Unspecified remains Unspecified | DTO preserves represented instant/offset | formatter/parser cannot carry kind | 4D |
| local round-trip format/reparse | `K` emits local offset; exact parse recreates Local under round-trip rules | offset carried explicitly | unavailable | 4D/timezone |
| UTC round-trip format/reparse | `K`/`O` emits `Z`; `RoundtripKind` recreates Utc | offset zero carried | unavailable | 4D |

### Selected kind representation policy

4D should use the high bits of an unsigned 64-bit date payload for kind/ambiguous
local state, as the pinned .NET design does, while preserving an API that
returns pure tick counts. This can preserve `sizeof(DateTime)` on this toolchain,
but it is still a private representation and semantic change, not “ABI neutral.”
It affects constructors, comparison/arithmetic auditing, serialization, and
every direct tick access. It also adds public constructors/properties/conversion
symbols. A naive separate field is rejected because it is likely to grow
`DateTime` and transitively `DateTimeOffset`.

The first 4D implementation may add storage, `Kind`, kind-taking construction,
and `SpecifyKind` without claiming reliable local/UTC conversion. `ToLocalTime`,
`ToUniversalTime`, `AssumeLocal`, and local offset parsing remain blocked until
the TimeZone component supplies date-sensitive transition rules or an approved
documented lesser contract. Tests must set and record a controlled timezone;
the developer machine timezone is never an oracle.

## Implementation decomposition and dependencies

| Group/ticket | Bounded scope | Status after design | Depends on | Can combine with |
|---|---|---|---|---|
| 4A / #1939 | invariant string-only single-format exact DateOnly/TimeOnly plus private scanner | needs user approval | none | only its own tests/benchmarks |
| 4B / #1940 | provider/culture ownership and lookup | blocked | SR-AUD-280, SR-AUD-285, component/ABI choice | no semantic exact group |
| 4C / #1942 | styles validation and parse effects | blocked | 4A/4E target overload, 4B for current date, 4D/timezone for zone styles | whitespace-only DateOnly/TimeOnly could later be a sub-batch |
| 4D / #1941 | kind representation, public kind surface, then timezone-dependent conversion | needs user approval for storage-only phase; conversion blocked | representation approval; TimeZone for conversion | not with 4A |
| 4E1 / #1943 | DateTime/DTO/TimeSpan single-format provider-taking exact APIs | blocked | 4A scanner, 4B, 4C, 4D | no additive multi/span surface |
| 4E2 / #1944 | all multi-format and any span-like overload/API shapes | blocked | corresponding single-format APIs and C++ representation decision | may combine format-loop internals, not public approval |
| 4F / #1945 | XmlConvert exact format/multi-format/mode bridge corrections | blocked | 4D and 4E1/4E2 | no general grammar row |

Ticket numbering remains in `plan.sqlite3`; no SR-AUD identifier is created and
the frozen audit population stays 364.

### 4A exact selected contract

4A is intentionally smaller than .NET and does not pretend to be culture-aware.
It adds exactly these declarations:

```cpp
static DateOnly ParseExact(const std::string& input,
                           const std::string& format);
static bool TryParseExact(const std::string& input,
                          const std::string& format,
                          DateOnly& result);

static TimeOnly ParseExact(const std::string& input,
                           const std::string& format);
static bool TryParseExact(const std::string& input,
                          const std::string& format,
                          TimeOnly& result);
```

`DateOnly` supports invariant standard `O`/`o` (`yyyy-MM-dd`) and `R`/`r`
(`ddd, dd MMM yyyy`) with weekday agreement. Its custom formats must specify one
complete year/month/day, using `y`--`yyyy`, `M`/`MM`/`MMM`/`MMMM`, and
`d`/`dd` plus optional matching `ddd`/`dddd`; invariant English names,
`%` single-specifier escape, single/double quoted literals, and backslash
literal escapes are supported. Missing date components, default-current-date
behavior, eras, calendars, time fields, zones, `/` as a culture placeholder,
unsupported repetitions/tokens, and unmatched literals are rejected.

`TimeOnly` supports invariant standard `O`/`o`
(`HH:mm:ss.fffffff`) and `R`/`r` (`HH:mm:ss`). A custom format must specify one
hour and minute using `H`/`HH` or `h`/`hh` plus `m`/`mm`; seconds `s`/`ss` are
optional and default to zero. A 12-hour form requires `t`/`tt` and uses
invariant AM/PM. `f`--`fffffff` requires the exact digit count;
`F`--`FFFFFFF` permits omitted low digits. The same percent, quote, and escape
literal mechanisms are supported. Date/era/zone tokens, provider separator
placeholders, more than seven fractional specifiers/digits, mixed 12/24-hour
fields, duplicate fields, and malformed/unsupported formats are rejected.

Both types require complete input consumption and reject leading, trailing, or
extra inner whitespace. Parse throws the type's current `FormatException`
family with HResult `0x80131537` for input mismatch or malformed/unsupported
format. Try returns `false` and writes `MinValue` for every such failure. It does
not throw for a representable string failure. C++ `std::string` cannot represent
null, so 4A creates no null contract. General `Parse`/`TryParse`, formatting,
rows 1--3, providers, styles, kind, multi-format input, span input, and XML
bridges remain byte-for-byte semantically unchanged.

### Rejected decomposition alternatives

- **One “match .NET ParseExact” ticket:** rejected because it hides missing
  public APIs, provider ABI, culture concurrency, timezone, and bridge changes.
- **Add every overload with invariant behavior:** rejected because provider
  names would lie and null/current behavior would be wrong.
- **Reuse `ToString(format)` token handling:** rejected because formatting has
  an ad-hoc unsupported-token policy and is not a validating grammar.
- **Store kind in a new field:** rejected due layout propagation.
- **Treat current fixed OS offset as full Local semantics:** rejected because
  historical and DST conversions would be nondeterministic or incorrect.
- **Fold `XmlConvert` into exact parsing:** rejected because it changes existing
  bodies and serialization-mode semantics with a separate rollback boundary.
- **Add arrays/spans incidentally:** rejected because each is an additive public
  API and representation decision.

## Source, API, ABI, layout, vtable, and symbol analysis

| Group | Existing-body semantics | Public/API and symbols | Layout/vtable | Components |
|---|---|---|---|---|
| 4A | only new bodies/private scanner; current methods unchanged | four overload declarations and four mangled symbols | none expected; no virtuals or fields | Core.Base only |
| 4B | culture lookup/state changes | likely provider factories/interfaces and provider-accepting future signatures | potentially changes CultureInfo/DTFI/NFI layouts and vtables | requires ownership refactor; must preserve acyclic graph |
| 4C | style validation and accepted text/value rules | style-taking overload symbols; enum values/default arguments must be explicit | no target-value layout by itself | currently crosses Globalization/Core.Base boundary |
| 4D | tick access, construction, formatting/parsing, conversions audited | Kind property, constructors, SpecifyKind/conversion symbols | packed plan aims to preserve size but changes representation; no new vtable | TimeZone required only for conversion phase |
| 4E1 | new exact parsing bodies | provider/style overloads for three types; new symbols | none expected beyond 4D | Core/Globalization architecture from 4B |
| 4E2 | ordered format loops | vector/array/span-like overloads and symbols; overload resolution changes | no value layout expected | may require a shared span/container abstraction |
| 4F | **changes existing XmlConvert bodies** plus additive multi-format symbols | XML declarations/symbols; overload resolution changes | no object layout/vtable expected | XML to exact/core/timezone behavior |

No group changes return types, exception specifications, `constexpr`, or enum
numeric values unless a later approval says so. None is allowed to change an
existing default argument incidentally. New overloads preserve existing binary
symbols but are ABI **additions**, not ABI-neutral changes; source overload
resolution must be tested for ambiguity.

## Migration consequences

4A is additive at source/link level, but downstream code can begin depending on
its intentionally invariant subset and new overload resolution. A later
provider overload must not reinterpret the providerless result. 4B can change
observable current-culture behavior and provider object ABI. 4C turns invalid
style combinations into early exceptions and may widen whitespace only where
explicitly requested. 4D changes serialization/round-trip and offset-bearing
DateTime results even if layout size is held. 4E changes accepted exact inputs
only through new APIs. 4F changes existing XmlConvert results/exceptions and is
therefore the highest direct migration risk in row 4.

Rows 1--3 stay isolated: a format token that explicitly requests a short field,
fraction width, or offset does not authorize the same form in general parsing.

## Test strategy

### 4A

- Table-driven positive/negative cases for every standard/custom token,
  repetition boundary, literal form, fraction width, AM/PM boundary, leap day,
  weekday agreement, range limit, whitespace position, trailing/missing input,
  malformed format, and unsupported token.
- Assert exact ticks/day numbers and exact `TryParseExact` failure output.
- Assert type, HResult, and selected stable port message for Parse failures.
- Compile-only positive consumer tests for each new overload and negative
  fixtures proving provider/style/multi-format/span shapes remain absent.
- Retain differential matrices against the pinned .NET source/tests, while
  marking intentional invariant/subset divergences rather than adding them as
  failures to the ordinary passing suite.
- Assert all existing general parsing tests unchanged, including #1879, #1880,
  #1929, #1930, and #1931 evidence represented in current tests/documents.

### Later groups

- 4B: invariant, null, each supported concrete provider, custom provider
  returning DTFI, custom provider returning null, unknown culture, concurrent
  culture changes, calendar/name/separator fixtures, and ownership lifetime.
- 4C: every bit, invalid bit, pairwise conflict, validation order, all whitespace
  positions, Try exceptions, parameter names, and HResults.
- 4D: all kind matrix rows under an explicitly controlled UTC zone and a zone
  with DST transition/gap/ambiguity; tick/layout/serialization and round-trip
  invariants.
- 4E: ordered/duplicate/empty/null/malformed candidate collections, overflow
  taxonomy, all standard formats, provider names/separators, and source overload
  ambiguity fixtures.
- 4F: each existing XML overload before/after, modes Local/Utc/Unspecified/
  RoundtripKind, invalid mode taxonomy, leading/trailing whitespace, multi-format
  ordering, and unchanged XSD duration exclusions.

No future-behavior assertion belongs in the ordinary passing suite before its
group is approved. Expected divergence remains in retained probes/design
fixtures.

### Sanitizer relevance

This design batch changes no production object and makes no sanitizer claim.
Future sanitizers can detect scanner bounds errors, invalid lifetimes, races in
provider caches, and arithmetic UB. They cannot establish cultural correctness,
exception order, ABI compatibility, DateTimeKind meaning, timezone/DST policy,
or accepted-language equivalence.

## Performance strategy

The hot costs are format tokenization, provider/current-culture lookup,
localized name matching, repeated candidate loops, timezone conversion,
kind normalization, temporary strings, and allocation. Each group needs a
separate comparable corpus:

| Group | Benchmark corpus and counters |
|---|---|
| 4A | O/R plus short/long custom DateOnly/TimeOnly successes and early/late failures; cached vs uncached format; allocations and ns/parse against current general parser characterization |
| 4B | null/invariant/known/custom/fallback providers; cold and cached culture data; contention and allocations |
| 4C | None, whitespace flags, early-invalid styles, and whitespace-heavy failures; branch/cost delta |
| 4D | unspecified/UTC/local, offset-bearing, normalization, and transition dates; conversion latency without hiding OS lookup |
| 4E2 | 1, 2, 8, and 32 candidate formats with first/middle/last/no match and duplicates; verify linear attempt count and no avoidable per-attempt allocations |
| 4F | XML single/multi/mode paths compared with direct exact calls |

Scanner reuse should separate immutable compiled format tokens from target
value assembly. Caching is not approved with 4A: it requires bounded lifetime,
thread safety, eviction, and provider identity rules. A simple stack/vector
tokenization baseline must be measured before any cache is proposed.

## Rollback strategy

Each ticket is one semantic unit and must have an isolated commit series. 4A can
be rolled back by removing its four additive symbols and private scanner without
touching general parsing. 4B requires a provider ABI migration/compatibility
plan before implementation because reverting a published layout is not enough
for already-built consumers. 4C overloads can be removed before publication or
their bodies reverted independently after publication while retaining symbols.
4D needs an explicit serialization/version plan; conversion behavior can be
rolled back separately from packed kind storage. 4E1 and 4E2 remain separate so
multi-format/API additions do not block single-format rollback. 4F reverts only
XML bridge bodies/symbol additions and never rewrites the core exact engine.

## Explicit exclusions

This design does not approve or implement #1929 rows 1--3, extra fractional
precision, all-digit fractions, short/compact general offsets, any provider,
culture, style, kind, exact, multi-format, span, or XML behavior. It does not
touch #1894, #1899, blocked #1773, declined #1888/#1889/#1896, or wontfix
#1926. It does not address SR-AUD-354 TimeSpan XML duration parsing. It does not
change HTTP/cookie parsing, formatting, public enum values, local timezone
policy, or audit numbering.

## Exact copyable approval wording

The following paragraphs are deliberately standalone. Approval of one does not
approve any other paragraph.

### Approval 4A / #1939 — recommended next batch

> Approve #1939 only: add the four string-only, providerless declarations
> `DateOnly::ParseExact(input, format)`,
> `DateOnly::TryParseExact(input, format, result)`,
> `TimeOnly::ParseExact(input, format)`, and
> `TimeOnly::TryParseExact(input, format, result)` specified in
> `docs/DateTimeExactParsingAndKindDesign.md` section “4A exact selected
> contract”. Implement invariant standard DateOnly `O/o` and `R/r`, TimeOnly
> `O/o` and `R/r`, and only the complete custom numeric/name/fraction/AM-PM and
> quote/escape subset listed there. Require full input consumption; reject
> whitespace, missing fields, provider separator placeholders, eras/calendars,
> zones, unsupported/malformed formats, and fractions beyond seven digits.
> Parse failures and malformed formats throw the existing type-specific
> `FormatException` family with HResult `0x80131537`; Try failures return false
> and set `MinValue`. Additive declarations and mangled symbols are approved;
> no fields, vtables, enum values, default arguments, provider/style overloads,
> multi-format or span APIs are approved. Do not change general parsing,
> formatting, XML bridges, #1929 rows 1--3, DateTime, DateTimeOffset, TimeSpan,
> providers, culture, styles, kind, timezone behavior, or any existing result,
> exception, message, or failure output.

Before/after examples: before, all four calls fail to compile because the API is
absent. After, `DateOnly::ParseExact("2024-06-15", "O")` returns day `739051`,
`TimeOnly::ParseExact("10:20:30.1234567", "O")` returns
`372301234567` ticks, `DateOnly::ParseExact("2024-6-5", "yyyy-M-d")`
succeeds, and the same unpadded text still fails general `DateOnly::Parse`.
`TimeOnly::TryParseExact("10:20:30.12345678", "HH:mm:ss.ffffffff",
out)` returns false and writes `MinValue`.

### Approval 4B / #1940 — do not approve until dependencies close

> Approve #1940 provider/culture infrastructure only, after separately closing
> SR-AUD-280 and SR-AUD-285 premises and accepting the recorded ABI transition:
> make `CultureInfo` and `DateTimeFormatInfo` usable through
> `IFormatProvider`, route date/time parsing through an immutable format-data
> view without a Core.Base-to-Globalization dependency cycle, make null provider
> mean the repaired current culture, honor direct CultureInfo,
> DateTimeFormatInfo, and custom-provider DateTimeFormatInfo results, and fall
> back to current culture only when a custom provider returns no date/time
> format, as pinned .NET does. Approve the necessary provider vtables, layouts,
> factory/adapter symbols, and component moves exactly as enumerated in the
> implementation plan produced after ABI measurement. Do not add or change any
> DateTime-family Parse/ParseExact overload, accepted input, result, style,
> kind, timezone conversion, or XML behavior in this ticket.

This wording is intentionally conditional: the final ABI transition inventory
does not yet exist, so #1940 remains blocked and is not safe to approve now.
Before, no production provider can reach date/time format data. After the
eventual bounded infrastructure change, a null/known/custom provider lookup
returns the specified immutable data, but no parser consumes it until 4E.

### Approval 4D / #1941 — storage-only phase; separate from conversion

> Approve #1941 phase 1 only: encode `DateTimeKind::Unspecified`, `Utc`, and
> `Local` plus the required ambiguous-local marker in reserved high bits of an
> unsigned 64-bit DateTime payload while preserving the measured 16-byte
> `DateTime` and 48-byte `DateTimeOffset` layouts on every supported ABI. Add a
> `Kind` accessor, kind-taking DateTime construction, and `SpecifyKind`; keep
> `Ticks` pure, range-check existing constructors before packing, and audit all
> arithmetic, comparison, hashing, formatting, serialization, and direct-payload
> paths so their previously kindless results stay unchanged except for the new
> explicitly kind-taking APIs. New constructors/accessors/functions and mangled
> symbols plus the private representation change are approved; no vtable or
> enum-value change is approved. Do not approve `ToLocalTime`,
> `ToUniversalTime`, offset/Z parse conversion, AssumeLocal, AssumeUniversal,
> AdjustToUniversal, RoundtripKind parsing, provider/culture, ParseExact, XML,
> or timezone/DST semantics in this phase.

Before, `DateTime` cannot represent kind. After phase 1,
`SpecifyKind(DateTime(638540436300000000), Utc)` has identical `Ticks` and
reports `Utc`; every existing constructor continues to produce Unspecified.
Local/UTC conversion remains unavailable. This phase cannot be combined with
4A. A phase-2 approval must name a date-sensitive timezone provider and the
full kind matrix before any conversion is implemented.

### Approval 4C / #1942 — blocked styles contract

> Approve #1942 only after #1940/#1941 and the target exact overloads exist:
> validate exactly the DateTimeStyles/TimeSpanStyles bit domains, conflicts,
> parameter names, HResults, and per-family rules in the design matrix;
> invalid styles throw `ArgumentException` even from TryParseExact before input
> scanning. Implement AllowLeadingWhite, AllowTrailingWhite, and parser-defined
> AllowInnerWhite; DateOnly/TimeOnly allow only their union; TimeSpan allows only
> None/AssumeNegative and applies AssumeNegative only to custom formats.
> Implement NoCurrentDateDefault, AssumeLocal, AssumeUniversal,
> AdjustToUniversal, and RoundtripKind only with the provider/current-date and
> date-sensitive timezone dependencies named in the approved implementation
> plan, producing exactly the design kind matrix. Add only explicitly listed
> style-taking overload symbols. Do not alter providerless overloads, general
> grammar rows 1--3, provider infrastructure, kind representation, multi-format
> API shape, or XML bridges.

Before, every style overload is absent. After, for example, a DateOnly exact
style overload with `AllowLeadingWhite` accepts leading whitespace while None
rejects it; passing `AssumeUniversal` to DateOnly throws ArgumentException with
parameter `style` even through TryParseExact. Zone-affecting examples remain
blocked until the controlled timezone contract is available.

### Approval 4E1 / #1943 — remaining single-format exact APIs

> Approve #1943 only after #1940--#1942: add the explicitly inventoried
> string-only, single-format provider/style ParseExact and TryParseExact
> overloads for DateTime, DateTimeOffset, and TimeSpan, plus the provider/style
> extensions for DateOnly and TimeOnly. Implement the pinned standard/custom
> grammar categories and provider resolution in the design matrices, the exact
> style validation/order, full-consumption rule, tick/day/offset results, and
> DateTimeKind matrix. Pin per-overload exception type, HResult, and parameter
> name; Try writes the type default on parse failure and still throws for invalid
> styles/arguments. Approve only the listed declarations and symbols; no
> multi-format collection, span-like API, XML bridge change, provider layout,
> kind representation, general Parse widening, or #1929 rows 1--3 is approved.

Before, `DateTime::ParseExact` and corresponding DTO/TimeSpan APIs are absent.
After, the approved provider/style call with round-trip `Z` and RoundtripKind
returns the same ticks with kind Utc, DTO `zzz` preserves the requested offset,
and TimeSpan `c` preserves seven fractional ticks; unlisted overload shapes
still fail to compile.

### Approval 4E2 / #1944 — multi-format and span-like API shapes

> Approve #1944 only after each corresponding single-format exact API is
> complete: add the exact, enumerated C++ candidate-collection and, if a
> repository span abstraction has been separately selected, span-like
> ParseExact/TryParseExact overloads for DateTime, DateTimeOffset, DateOnly,
> TimeOnly, and TimeSpan. Candidate formats are attempted in order with complete
> independent state; first success wins; duplicates are allowed; empty,
> malformed, unsupported, null-equivalent, and overflowing candidates follow
> the per-type exception/status matrix, including TimeSpan single-versus-multi
> overflow taxonomy. Approve each declaration, container/span representation,
> overload-resolution consequence, mangled symbol, and source-compatibility
> fixture explicitly. Do not change single-format results, provider/style/kind
> infrastructure, general parsing, XML, or rows 1--3.

Before, candidate collections and spans are not representable. After, an
approved two-format call whose first candidate fails and second succeeds
returns the second exact value; an empty candidate list has the pinned
per-family failure. This group must remain separate from 4E1 because its public
API representation and rollback are independent.

### Approval 4F / #1945 — XmlConvert bridge

> Approve #1945 only after #1941/#1943 and any required #1944 shapes: change
> existing `XmlConvert::ToDateTime(input, format)` and
> `ToDateTimeOffset(input, format)` bodies to call the approved invariant exact
> APIs with AllowLeadingWhite|AllowTrailingWhite; add only the pinned
> multi-format XML overloads explicitly listed in the implementation inventory;
> and implement DateTime serialization modes Local, Utc, Unspecified, and
> RoundtripKind with the approved kind/timezone contract. Invalid modes throw
> the pinned ArgumentException taxonomy. Existing declarations and their
> symbols remain, new overloads are ABI additions, and the changed bodies are an
> intentional semantic correction. Do not change XmlConvert TimeSpan/XSD
> duration behavior, general Parse, provider/culture infrastructure, exact
> scanner grammar, #1929 rows 1--3, or SR-AUD-354.

Before, the format and mode arguments are ignored. After, a mismatched format
throws instead of being parsed generally, leading/trailing whitespace follows
the explicitly named styles, and mode changes kind/conversion exactly as
approved. This cannot share the 4A rollback boundary.

## Durable local evidence

The design batch retains `build-probe/1938_current_behavior_probe.cpp` and
`build-probe/1938_current_behavior_probe.log` locally. The source uses only
current public APIs and compile-time detection, and the log records layouts,
enum values, rows 1--3, exact-API absence, exceptions, HResults, and Try failure
outputs. These build-probe paths are ignored by repository policy, so the
committed tables in this document are the durable review evidence. Historical
ignored #1879/#1880/#1929/#1930/#1931 raw build-probe directories are not
present in this isolated checkout; their committed tests, matrices, correction
notes, and Git history were used instead. No sibling worktree or downstream
repository was inspected.
