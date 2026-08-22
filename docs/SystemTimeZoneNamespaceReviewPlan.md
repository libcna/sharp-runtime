<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/time-zone` namespace review plan (ticket #2176)

*Written 2026-08-10 on branch `claude/remediation-batch-1804-namespace-b1yjh5`. Audit numbering is
**frozen at 364**; nothing in this document creates an `SR-AUD-*` identifier. Post-audit defects
found here take ordinary ticket numbers only.*

---

## 1. Work unit 1 — why `time-zone`, verified rather than inherited

`audit/AUDIT_FINDINGS_INDEX.md` was re-parsed from scratch (all 364 rows, grouped by owning
module). The decomposition measured today is:

| Bucket | Count |
|---|---:|
| `remediated` | **161** |
| `confirmed` (plain) | **150** |
| `confirmed (design-complete)` | **53** |
| **total** | **364** |

**Correction to the inherited brief.** The handoff prompt for this batch stated *160 remediated /
154 confirmed / 50 design-complete*. That is not what the index says. The numbers above are the
authoritative recount and they agree exactly with the previous batch's own handoff paragraph at the
top of `NEXT.md` (161 / 203 confirmed of which 53 design-complete). The prompt's triple was stale;
the index and `NEXT.md` agree with each other and with this recount.

### 1.1 Candidate scoring, recomputed

Candidates are units with **no review plan and no remediated finding**.

| Candidate | Open | high | med | Actionable here | Blocked | Approval-gated | System data present | Memory risk | Public-input exposure | Cohesion |
|---|---:|---:|---:|---:|---:|---:|---|---|---|---|
| **`time-zone`** | **7** | **0** | **7** | **7** | **0** | **0** | **yes — 499 TZif zones installed** | none | medium (zone ids) | **high** — 4 headers, 2 bodies, 4 suites, 114 tests |
| `globalization` | 7 | 1 | 6 | ~2 | 0 | 1 (`Calendar` abstract, 82 tests pin the shape) | **no** — needs ICU collation/grapheme data | high (TSan-confirmed culture race) | high | medium |
| `xml-linq` | 4 | 1 | 3 | 3 | **1** — its high **is** CCF-019 (#1899/#1894) | 0 | n/a | high, but that half is the blocked one | high | high |
| `net-network-information` | 3 | 0 | 3 | 3 | 0 | 0 | partial | low | medium | small; adjacent to blocked #1962 |
| `core` | 72 | 9 | 59 | many | — | several | n/a | mixed | high | **not a namespace** — already carved by seven `CCF-*` plans, 47 findings remediated |
| `io` | 7 | 0 | 7 | — | — | — | — | — | — | already has `docs/SystemIONamespaceReviewPlan.md` |

**`time-zone` is confirmed as the correct next unit**, and for a stronger reason than the previous
batch could give: every one of its seven findings was **reproduced in this container** before any
code was changed (§6). It is the only remaining candidate that is simultaneously the largest by
open count among genuinely unreviewed namespaces, zero-high, zero-blocked, zero-approval-gated, and
fully decidable from data that is physically present.

### 1.2 Reference availability at the review checkpoint

At the #2176 review checkpoint the .NET reference tree was unavailable. That blocked
*message-text* parity questions and nothing else in this namespace: five of the seven findings were
**self-consistency or validation** questions (an out parameter that is not written, a factory with
no validation, an equality/hash pair that disagree with each other, a property that contradicts its
own doc-comment), and the audit reports for the other two carry **direct current-.NET probe
results** recorded at audit time. The remaining reference questions were recorded instead of
guessed and were subsequently answered and closed by #2186; §17 now preserves those outcomes.

---

## 2. Namespace scope and file inventory

Physical component `SharpRuntime::TimeZone` → target `sharp_runtime_time_zone`,
`PUBLIC_DEPENDENCIES Core.Base`, one edge in the 41-module / 92-edge graph.

| File | Lines | Role |
|---|---:|---|
| `modules/time-zone/include/System/TimeZoneInfo.hpp` | 719 | the whole public surface: `TimeZoneInfo` + nested `TransitionTime` + nested `AdjustmentRule` |
| `modules/time-zone/include/System/TimeZone.hpp` | 62 | legacy abstract `TimeZone` + `CurrentTimeZone()` |
| `modules/time-zone/include/System/TimeZoneNotFoundException.hpp` | 22 | lookup failure |
| `modules/time-zone/include/System/InvalidTimeZoneException.hpp` | 60 | corrupt zone data; non-TZif files now throw it (#2186) |
| `modules/time-zone/src/System/TimeZoneInfo.cpp` | 309 | POSIX/Windows lookup, `Local()`, the CLDR IANA↔Windows table |
| `modules/time-zone/src/System/TimeZone.cpp` | 39 | the `CurrentTimeZone()` adapter |
| `modules/time-zone/tests/System/TimeZoneInfoTests.cpp` | 608 | 75 tests |
| `modules/time-zone/tests/System/AdjustmentRuleTests.cpp` | 205 | 19 tests |
| `modules/time-zone/tests/System/TimeZoneTests.cpp` | 71 | 8 tests |
| `modules/time-zone/tests/System/TimeZoneNotFoundExceptionTests.cpp` | 45 | 12 tests |

Baseline: `SharpRuntimeTests_TimeZone` = **114 tests, 114 passing**.

**There is no TZif parser in this module.** Resolution is `stat()` on
`/usr/share/zoneinfo/<id>` followed by `setenv("TZ", id)` + `tzset()` + `localtime_r()`; libc owns
every byte of TZif parsing. The brief's "TZif parser safety" matrix therefore does not apply as
written, and §14 records what replaced it.

---

## 3. Complete public-surface inventory

### `TimeZoneInfo` — properties
`getIdProperty`, `getDisplayNameProperty`, `getStandardNameProperty`, `getDaylightNameProperty`,
`getBaseUtcOffsetProperty`, `getSupportsDaylightSavingTimeProperty`, `getHasIanaIdProperty`.

### `TimeZoneInfo` — instance methods
`IsDaylightSavingTime(DateTime)`, `IsDaylightSavingTime(DateTimeOffset)`, `GetUtcOffset(DateTime)`,
`GetUtcOffset(DateTimeOffset)`, `IsAmbiguousTime(DateTime)`, `IsAmbiguousTime(DateTimeOffset)`,
`IsInvalidTime(DateTime)`, `GetAmbiguousTimeOffsets(DateTime)`,
`GetAmbiguousTimeOffsets(DateTimeOffset)`, `GetAdjustmentRules()`, `ConvertTimeToUtc(DateTime)`,
`HasSameRules`, `Equals`, `GetHashCode`, `ToString`, `operator==`, `operator!=`.

### `TimeZoneInfo` — statics
`Utc()`, `Local()`, `ClearCachedData()`, `FindSystemTimeZoneById`, `TryFindSystemTimeZoneById`,
`GetSystemTimeZones()`, `GetSystemTimeZones(bool)`, `CreateCustomTimeZone`,
`ConvertTimeBySystemTimeZoneId` ×3, `ConvertTime` ×3, `ConvertTimeFromUtc`, `ConvertTimeToUtc`,
`TryConvertIanaIdToWindowsId`, `TryConvertWindowsIdToIanaId`.

### `TimeZoneInfo::TransitionTime`
`getTimeOfDayProperty`, `getMonthProperty`, `getWeekProperty`, `getDayProperty`,
`getDayOfWeekProperty`, `getIsFixedDateRuleProperty`, `validateTimeOfDay`, `CreateFixedDateRule`,
`CreateFloatingDateRule`, `Equals`, `GetHashCode`, `operator==`, `operator!=`.

### `TimeZoneInfo::AdjustmentRule`
`getDateStartProperty`, `getDateEndProperty`, `getDaylightDeltaProperty`,
`getDaylightTransitionStartProperty`, `getDaylightTransitionEndProperty`,
`getBaseUtcOffsetDeltaProperty`, `getNoDaylightTransitionsProperty`,
`getHasDaylightSavingProperty`, `GetHashCode`, `Equals`, `operator==`, `operator!=`,
`CreateAdjustmentRule` (5-arg and 6-arg).

### `TimeZone` (legacy, abstract)
`getStandardNameProperty`, `getDaylightNameProperty`, `GetUtcOffset(DateTime)`,
`IsDaylightSavingTime(DateTime)`, `CurrentTimeZone()`.

### Absent by design
Serialization (`ToSerializedString` / `FromSerializedString`), `CreateCustomTimeZone` overloads
taking adjustment rules, `TimeZoneInfo::Utc`/`Local` as fields, `AdjustmentRule` public
constructor. Recorded in §18.

---

## 4. The seven findings, dispositions, and the tickets that carry them

| Finding | Severity | Disposition | Ticket | Compatible? |
|---|---|---|---|---|
| **SR-AUD-223** legacy `CurrentTimeZone` freezes one offset and never reports DST | medium | **compatible implementation** | **#2182** | yes — file-local adapter, no public shape touched |
| **SR-AUD-224** failed `TryFind` leaves a stale zone in the out parameter | medium | **compatible implementation** | **#2177** | yes — one inline body |
| **SR-AUD-225** `CreateCustomTimeZone` accepts empty ids and out-of-range offsets | medium | **compatible implementation** | **#2178** | yes — new rejections, audit-probe-evidenced |
| **SR-AUD-226** `AdjustmentRule` accepts `dateEnd < dateStart` | medium | **compatible implementation** | **#2179** | yes — new rejection, audit-probe-evidenced |
| **SR-AUD-227** equality is case-sensitive while the hash is case-insensitive | medium | **compatible implementation** | **#2180** | yes — also repairs an internal contract breach |
| **SR-AUD-229** IANA lookup records the *current* offset as `BaseUtcOffset` | medium | **compatible implementation** | **#2181** | yes — the implementation contradicts its own doc-comment |
| **SR-AUD-228** `HasSameRules` reduces every rule set to offset + a bool | medium | **accepted permanent deviation** | **#2185** (closed after the proposed sampled-rule design proved insufficient) | **no** — faithful repair requires out-of-scope TZif rule parsing |

**No finding disappears and no finding is reclassified as a false premise.** All seven are real and
all seven reproduce (§6).

Post-audit defects found during this review, with ordinary ticket numbers and **no** `SR-AUD-*`
identifier:

| Ticket | Defect |
|---|---|
| **#2183** | `FindSystemTimeZoneById` accepts the **7 non-TZif regular files** shipped inside `/usr/share/zoneinfo`, and accepts malformed identifiers (`America//New_York`, `./America/New_York`, an embedded NUL) |
| **#2184** | the `TZ` save/restore window is not exception-safe, and an **empty-but-set** `TZ` is deleted rather than restored |
| **#2186** | subsequently closed five reference-parity questions: three repairs and two already-correct outcomes |

---

## 5. Corrected audit premises

Every correction below is a measurement, recorded in `build-probe/2176_probe*.log`.

### 5.1 SR-AUD-229 is three properties, not one, and it reaches 158 of 499 zones

The audit records `BaseUtcOffset`. The same snapshot-of-`time(nullptr)` also feeds
**`StandardName` and `DaylightName`**, which are written from `tm_zone` at that instant, so both
names take the *daylight* abbreviation for a zone queried during its DST period and the two names
are always identical:

```
America/New_York   port: base=-240m std=EDT  dst=EDT      truth: std EST -300m, daylight EDT -240m
Europe/Prague      port: base=+120m std=CEST dst=CEST     truth: std CET +60m, daylight CEST +120m
Europe/Dublin      port: base= +60m std=IST  dst=IST      truth: std GMT   0m, daylight IST  +60m
```

Scale, measured over **every** installed zone (`2176_probe4_allzones.log`): 499 TZif zones,
**158 observe DST somewhere in 2025**, and **141 of 499 report the wrong `BaseUtcOffset` today**
(2026-08-10, northern summer). The remaining 17 are the southern-hemisphere DST zones, which are
correct today and wrong in January — the month-dependence the audit names, demonstrated in both
directions from one run.

### 5.2 SR-AUD-229 is a contradiction of the port's own documentation, not only of .NET

`TimeZoneInfo.hpp:364-369` already says *"DST transitions are not modelled; this is always the
standard offset."* The POSIX implementation did not deliver the standard offset. The original
repair made it agree with the documented contract. The later #2418 ripple review corrected the
Windows formula independently: standard offset is `-(Bias + StandardBias)`, while
`DaylightBias` remains excluded.

### 5.3 SR-AUD-223 is *two* independent defects in a 39-line file

The adapter freezes the offset **and** hard-codes `IsDaylightSavingTime` to `false`. Fixing the
first without the second still reports "no DST" for a July date in New York. Both are repaired by
#2182; both are pinned separately.

### 5.4 SR-AUD-225 leaves a signed-overflow door reachable from a public factory

`CreateCustomTimeZone("X", TimeSpan::MinValue, …)` is accepted. `ConvertTimeToUtc` then evaluates
`-baseUtcOffset_` and the port throws *"Negating the minimum value of a twos complement number is
invalid."* — `TimeSpan` already guards the negation, so this is **not** undefined behaviour, but it
is a public door that produces a diagnostic about two's-complement arithmetic instead of rejecting
the argument. The ±14-hour validation closes the door at its source. Recorded because the audit
does not mention it and because it is the reason CCF-004 is *not* implicated here (§8).

### 5.5 SR-AUD-226 has two overloads, and three adjacent validations the audit does not name

Both `CreateAdjustmentRule` overloads accept the reversed range. Measurement also shows
`dateStart` with a time-of-day component, a `daylightDelta` of +15 hours, and a `daylightDelta` of
30 seconds are all accepted. Only the reversed range carried audit evidence at this checkpoint;
the other three went to #2186 rather than being repaired on a recollection. #2186 later answered
them from the reference and landed the required validation.

### 5.6 SR-AUD-227's hash is locale-dependent, which the audit does not say

`GetHashCode` lowercases through `std::tolower`, whose result depends on the process `LC_CTYPE`.
Two zones whose ids differ only in the case of a non-ASCII byte can therefore hash equal or unequal
depending on a global the caller may change at any time. #2180 replaces both sides with one
ordinal ASCII fold so that equality and hash are computed by the *same* function.

### 5.7 SR-AUD-228's repair is a measured object-layout change

`sizeof(TimeZoneInfo)` is **160** bytes on LP64 — measured, not derived
(`build-probe/2185_layout.log`): 4 × `std::string` = 128, `TimeSpan` = **24** (not the 8 an
estimate suggests; this port's `TimeSpan` is wider than a bare tick count), `bool` = 1, 7 bytes of
tail padding. A second `bool` fits that padding and is layout-neutral, confirmed at 160; a
`std::vector<std::shared_ptr<AdjustmentRule>>` (24 bytes) takes it to **184**, and reordering the
members does not help — the best packing available is also 184. `HasSameRules` cannot return
`false` where .NET does without rule data to compare, so the complete repair needs that vector.
This was the measured layout cost of #2185's initial proposal. Later probes showed that the
proposal itself could not work: monthly sampling cannot distinguish New York from Havana, and a
single sampled year cannot represent rule eras. Faithful repair requires TZif rule structures,
which the 2026-08-19 user decision placed outside this practical subset. The 160 → 184 cost is
therefore recorded but not paid; SR-AUD-228 is an accepted permanent deviation, not a live gate.

> **Correction.** Sections 5.7 and 11 of this document first carried an *estimate* of 144 → 168,
> written before the shape was compiled, and that estimate reached
> `docs/Migration-TimeZoneStandardOffset.md`, `README.md` and commit `9210d8f`'s message. The
> measured numbers are **160 → 184**. The substantive claim is unaffected — #2177–#2184 add no
> member and `sizeof` is 160 before and after, now pinned by a `static_assert` in
> `TimeZoneInfoTests.cpp` — but the arithmetic behind the gate is restated here from measurement.
> `9210d8f` is already pushed and is not rewritten; this is the correction of record.

### 5.8 `FindSystemTimeZoneById` validates a file it does not necessarily read

`zoneFileExists()` checks `S_ISREG` under `/usr/share/zoneinfo/` and then hands the **identifier**
to `setenv("TZ", …)`. glibc interprets a `TZ` value that is not a path as a POSIX rule string, so
`zone.tab` resolves as POSIX rule *"zone"*, offset 0. Seven non-TZif regular files ship inside
`/usr/share/zoneinfo` and **all seven are accepted as time zones today**:

```
zone.tab  zone1970.tab  iso3166.tab  tzdata.zi  leapseconds  leap-seconds.list   -> accepted, +0m
```

A `':'` prefix on the `TZ` value — the documented glibc way to force file interpretation — was
measured and **does not help**: bare and `':'`-prefixed values give byte-identical results for all
nine ids tried (`2176_probe3_design.log` §A). The repair is therefore a **TZif magic check** at the
door, not a change to how `TZ` is set.

---

## 6. Reproduction — every finding, measured before any change

`build-probe/2176_probe1_surface.log`, one process, against the shipped library:

| Finding | Measurement |
|---|---|
| SR-AUD-223 | `TZ=America/New_York`: `CurrentTimeZone().GetUtcOffset` = **−240 min for both January and July**, `IsDaylightSavingTime` = **0 for both**; system truth is −18000 s/isdst 0 and −14400 s/isdst 1 |
| SR-AUD-224 | `TryFind("Mars/Olympus", out)` → `false`, `out` still `"UTC"`; with a fresh `out` it stays null, so the stale value is the caller's |
| SR-AUD-225 | `id=""` accepted; `+15h`, `−15h`, 90-second and `TimeSpan::MinValue` offsets all accepted |
| SR-AUD-226 | `CreateAdjustmentRule(2025-01-02, 2025-01-01, …)` accepted on **both** overloads |
| SR-AUD-227 | `Equals("Zone","zone")` = **0** while `GetHashCode` values are **equal** |
| SR-AUD-228 | `HasSameRules(America/New_York, America/Havana)` = **1**; .NET returns false |
| SR-AUD-229 | `America/New_York` records `base=−240m std=EDT dst=EDT`; standard is −300m/EST |

---

## 7. Shared root causes

| Cause | Findings | Statement |
|---|---|---|
| **TZ-A — one instant stands in for a year** | SR-AUD-223, SR-AUD-229 | Zone metadata is read at `time(nullptr)` and frozen, so the answer depends on the month the process runs in |
| **TZ-B — no input validation at a public factory** | SR-AUD-225, SR-AUD-226, #2183 | Factories and the lookup door copy or accept whatever they are given |
| **TZ-C — a result path that does not write its result** | SR-AUD-224 | A `false` return leaves the out parameter untouched |
| **TZ-D — equality and hash computed by different code** | SR-AUD-227 | Two functions answer one question and disagree |
| **TZ-E — an abbreviated model cannot express the contract it advertises** | SR-AUD-228 | Offset + one bool cannot distinguish two rule sets |
| **TZ-F — process-global state mutated without an unwind path** | #2184 | `setenv("TZ")` is undone by straight-line code, not by a scope guard |

TZ-A, TZ-B and TZ-C are each fixed once and the fix covers every site of that cause. TZ-E is the
one that needs storage the type does not have.

---

## 8. Cross-cutting family check — no new CCF, and CCF-004 is not implicated

- **CCF-004 (defined arithmetic)** — closed at 8/8. The only signed-overflow-shaped door in this
  namespace is §5.4's `-TimeSpan::MinValue`, and `TimeSpan` **already** guards it with a thrown
  diagnostic rather than wrapping. UBSan over the production bodies confirms it (§14). This is a
  *validation* defect, not an arithmetic-UB defect, so **CCF-004 gains no member** and none is
  claimed.
- **The NUL-truncation family** (#2003 Uri, #2085 XmlWriter) gains a **local occurrence** in #2183:
  `FindSystemTimeZoneById("America/New_York\0junk")` is accepted and stores the full 21-byte id. It
  is repaired here as part of the identifier door. **No CCF is minted** — three occurrences in
  three modules is exactly the promotion question #2131/#2109 already hold open, and this review
  does not pre-empt them.
- **CCF-019 / CCF-021 / CCF-022** — untouched. Nothing here hands out a borrowed raw pointer.

---

## 9. Dependency graph between the tickets

```
#2177 (TryFind out param)            independent
#2178 (CreateCustomTimeZone guards)  independent
#2179 (AdjustmentRule range)         independent
#2180 (ordinal-case equality)        independent
#2184 (TZ scope guard)               ── prerequisite ──┐
#2183 (TZif + identifier door)       ───────────────── ┼─> both edit the same locked region
#2181 (standard offset + names)      ── needs #2184 ───┘   of FindSystemTimeZoneById
#2182 (legacy adapter per date)      ── needs #2181 (correct standard/daylight names) and the
                                        shared POSIX support header #2184 introduces
#2185 (SR-AUD-228)                   closed as a permanent deviation after design probes
#2186 (reference verification)       completed after the reference became available
```

Implementation order: **#2177 → #2178 → #2179 → #2180 → #2184 → #2183 → #2181 → #2182**, then the
two record-only tickets and the audit reconciliation.

---

## 10. Severity

All seven findings are **medium**; there are no high-severity findings in this namespace and none
was created. The highest *practical* impact is SR-AUD-229 (141 of 499 zones wrong today, silently)
and SR-AUD-223 (every DST-sensitive legacy query wrong for half the year, silently). Both are
wrong-answer classes with no diagnostic; neither is a memory-safety class.

---

## 11. Compatibility and final-disposition matrix

| Ticket | Class | Source break | ABI / layout / vtable | Behaviour change | Gate |
|---|---|---|---|---|---|
| #2177 | compatible | none | none | `TryFind` failure now nulls the out parameter | none |
| #2178 | **documented break** | none | none | four previously-accepted inputs now throw | audit managed probe |
| #2179 | **documented break** | none | none | reversed date range now throws | audit managed probe |
| #2180 | **documented break** | none | none | case-variant ids now compare equal | audit managed probe |
| #2181 | **documented break** | none | none | `BaseUtcOffset`/`StandardName`/`DaylightName` become invariant | contradicts own doc-comment |
| #2182 | compatible | none | none (file-local class) | legacy adapter becomes date-sensitive | none |
| #2183 | **documented break** | none | none | 7 data files and 4 malformed id shapes now throw | none needed — they were never zones |
| #2184 | compatible | none | none | an empty-but-set `TZ` is restored as empty | none |
| #2185 | **accepted deviation** | none | none; the measured 160 → 184 proposal was not taken | `HasSameRules` remains one-directionally permissive | closed: TZif rule parsing is out of scope |
| #2186 | **documented compatibility repairs** | none | none | conversions clamp; adjustment-rule validation and non-zone exception classification now match .NET | closed from reference evidence |

**No public signature, virtual function, vtable slot, object layout, mangled symbol or
`noexcept` specification changes in #2177–#2184.** Verified by `static_assert` in the tests and by
§13's layout pin.

---

## 12. Date/time semantic consequences

- **`BaseUtcOffset` becomes invariant.** For a DST zone queried during its daylight period,
  `ConvertTime`/`ConvertTimeFromUtc`/`ConvertTimeToUtc` shift by the daylight delta relative to
  today's answer — for `America/New_York` in July, one hour. This makes the conversion *consistent*
  with the documented "DST is not modelled" contract instead of silently depending on process start
  month. It is the deliberate consequence, it is documented in
  `docs/Migration-TimeZoneStandardOffset.md`, and it is pinned by tests.
- **The in-repository blast radius is nil in this container.** The only consumers are
  `TimeProvider::getLocalTimeZoneProperty`, `FileSystemInfo`'s four
  `ConvertTimeFromUtc`/`ConvertTimeToUtc` calls and one `IOStreamTests` assertion, and all of them
  go through `TimeZoneInfo::Local()`. `/etc/localtime → Etc/UTC` here, so `Local()` has zero offset
  and no DST and none of those results move. On a DST host, `FileSystemInfo`'s local timestamps
  become standard-time-based, which is the same trade the property contract already states.
- **`TimeZone::CurrentTimeZone()` becomes the one date-sensitive surface in the module.** That is
  deliberate: the legacy `TimeZone` contract *is* per-date, it only ever describes the process-local
  zone, and it can answer per date without storing anything. `TimeZoneInfo` keeps its documented
  fixed-offset model. The asymmetry is stated in both headers.
- **Ambiguous local times resolve to standard** in the legacy adapter. Raw `mktime()` with
  `tm_isdst = -1` was measured to depend on unrelated preceding calls; #2186 established .NET's
  standard reading and the adapter now asks both interpretations explicitly.
- **Invalid local times normalise forward** (02:30 on a spring-forward date answers with the
  post-transition offset) for the same reason.

---

## 13. Platform consequences

| Platform | Effect |
|---|---|
| Linux/POSIX | all of it; this is the tested baseline |
| Windows | #2418 repairs the `Bias + StandardBias` standard-offset formula, keeps standard and daylight names distinct, and makes the legacy `TimeZone` adapter select the per-year system rule for local-wall-clock and UTC-instant questions. The branch was reviewed for MSVC portability, but the tracked CI is Ubuntu-only and does not compile or execute it. |
| Emscripten | The zero-offset model remains, but `Local()` is now a distinct Local-identity object rather than the canonical Utc singleton, so Kind propagation remains truthful. Non-Local/non-UTC database lookup is still unsupported. |
| macOS | uses the POSIX branch. `/usr/share/zoneinfo` is present on macOS, so the TZif check and the month scan behave as on Linux. Not executed here — the repository's tracked CI is Ubuntu-only |

No POSIX header enters any public `.hpp`. The new shared helper lives in
`modules/time-zone/src/System/TimeZonePosixSupport.hpp`, a **private** header under `src/`, which is
an established pattern in this repository (5 such headers exist in `net-http-headers`, `xml` and
`net-sockets`).

---

## 14. Test matrix

Real installed zones chosen for distinct properties, all verified present
(`2176_probe1_surface.log` §0):

| Zone | Property it exercises |
|---|---|
| `Etc/UTC`, `UTC` | no transitions; `UTC` is a **symlink** to `Etc/UTC` |
| `Etc/GMT+5`, `Etc/GMT-14` | fixed offsets, including the +14 h extreme |
| `America/New_York` | northern DST; ambiguous and invalid local times |
| `America/Havana` | same offset and DST flag as New York, **different rules** (SR-AUD-228) |
| `Europe/Prague` | northern DST, positive offset |
| `Europe/Dublin` | standard offset **zero**, DST positive |
| `Africa/Casablanca` | DST active in **both** January and July — the scan's fallback path |
| `Asia/Kolkata` (+05:30), `Asia/Kathmandu` (+05:45), `Asia/Tehran` (+03:30) | non-whole-hour offsets, no DST |
| `Australia/Lord_Howe` | **30-minute** DST delta |
| `Pacific/Chatham` | +12:45 standard, +13:45 daylight |
| `Australia/Sydney` | southern hemisphere — correct today, wrong in January |
| `America/Phoenix` | never observes DST |
| `EST5EDT` | an id that is **also** a valid POSIX rule string |
| `zone.tab`, `iso3166.tab`, `tzdata.zi`, `leapseconds`, `leap-seconds.list`, `zone1970.tab` | the six shipped **non-TZif** regular files — malformed-input fixtures that need no file to be written |

Transition-boundary matrix, applied to `America/New_York` for 2025 (spring forward 2025-03-09
02:00, fall back 2025-11-02 02:00) in both the legacy adapter and `TimeZoneInfo`: 01:59 / 02:00 /
02:30 (gap) / 03:00 on the spring date; 00:59 / 01:00 / 01:30 (repeated) / 03:00 on the autumn
date; an ordinary standard date; an ordinary daylight date; both year boundaries. Directions
covered: local → UTC, UTC → local, and zone A → zone B (`America/New_York` → `Asia/Kolkata`).

Identifier matrix: `""`, `" "`, `".."`, `"../../etc/passwd"`, `"/etc/passwd"`,
`"America//New_York"`, `"America/./New_York"`, `"./America/New_York"`, `"america/new_york"`,
`"America"` (a directory), `"America/New_York/"`, an embedded NUL, `"UTC"`, `"Local"`.

Range matrix: `DateTime::MinValue` and `DateTime::MaxValue` against ±14-hour zones in all four
conversion directions; `TimeSpan::MinValue`/`MaxValue` at the factory.

**Replacing the TZif-parser matrix.** This module parses no TZif, so the brief's parser matrix
(truncated header, invalid counts, bad type index, missing abbreviation terminator, leap records)
has no code here to test — libc owns it. What *is* testable, and is tested, is the **door**: does a
file that is not a TZif file get accepted as a zone? Six shipped non-TZif files answer that
question without writing a byte, and #2183 is the repair.

---

## 15. Sanitizer matrix

| Sanitizer | Applies? | Why |
|---|---|---|
| **UBSan** | **yes** | offset arithmetic: `TimeSpan` negation, `FromSeconds(double)` conversions, the tick multiplications in the month scan, `intcs`/`long` mixing in the POSIX helpers |
| **ASan** | **yes** | `tm_zone` is a pointer into libc-owned storage that `tzset()` may invalidate; the month scan reads it repeatedly across `setenv`/`tzset` cycles. Also covers the `stat`/`fopen` paths #2183 adds |
| **LSan** | **yes** (with ASan) | the failure paths of #2183 must not leak the `FILE*` or a partially built zone |
| **TSan** | **yes, and it is discriminating here** | the module holds **real shared mutable state**: Core.Base's `System::detail::processTimeZoneMutex()`, the `Local()` function-local static, and process-global `TZ`/`tzset()`. Concurrent `Local()` and `FindSystemTimeZoneById()` is exactly the race the shared mutex prevents |

TSan is run because there is shared mutable state to exercise, not as a formality — and a clean
TSan run is reported as "no report on this workload", never as "the module is thread-safe".

---

## 16. Bounded tickets

| # | P | Size | Title |
|---|---|---|---|
| #2176 | P2 | M | review `modules/time-zone` and produce this plan |
| #2177 | P2 | XS | SR-AUD-224 — `TryFindSystemTimeZoneById` must clear its out parameter on failure |
| #2178 | P2 | S | SR-AUD-225 — `CreateCustomTimeZone` must validate the id and the offset |
| #2179 | P2 | S | SR-AUD-226 — `AdjustmentRule` must reject an end date before its start date |
| #2180 | P2 | S | SR-AUD-227 — zone equality must be ordinal case-insensitive, like its hash |
| #2181 | P2 | M | SR-AUD-229 — `BaseUtcOffset`, `StandardName` and `DaylightName` must be invariant |
| #2182 | P2 | M | SR-AUD-223 — the legacy `CurrentTimeZone` adapter must answer per date |
| #2183 | P2 | M | a non-TZif file and a malformed identifier are accepted as time zones |
| #2184 | P3 | S | the `TZ` save/restore window is not exception-safe and drops an empty-but-set `TZ` |
| #2185 | P2 | — | CLOSED — SR-AUD-228 accepted as a permanent one-directional deviation; faithful TZif rule parsing is out of scope |
| #2186 | P3 | — | CLOSED — five reference-parity questions answered; three repairs and two already-correct outcomes |

---

## 17. Reference questions closed by #2186

| Question | Reference answer and disposition |
|---|---|
| Do `ConvertTimeFromUtc`/`ConvertTimeToUtc` clamp at `DateTime::MinValue`/`MaxValue`? | Yes. The shared safe constructor clamps once at the final result; repaired and tested. |
| Which adjacent `AdjustmentRule.CreateAdjustmentRule` shapes are invalid? | Non-date-only boundaries, deltas outside **-23..+14 hours**, and deltas not expressed in whole minutes are rejected. The original ±14/sub-minute recollection was corrected from source. |
| What are the exact resource strings for those argument diagnostics? | Transcribed from the reference and pinned. |
| Does `TryFindSystemTimeZoneById` return false for every failure, and what does the throwing door do with non-zone data? | The Try door returns false for every failure; the throwing door uses `InvalidTimeZoneException` for existing non-TZif data and `TimeZoneNotFoundException` for an absent id. Repaired and tested. |
| Which offset wins for an ambiguous local wall clock? | The standard-time interpretation wins. The legacy adapter already chose that result; the reference confirmed it. |

`docs/Migration-TimeZoneClampAndValidation.md` records the source locations, mutations, and exact
regressions. Nothing in this section remains deferred.

---

## 18. Exclusions

- **`DisplayName` is the raw id** for system zones (`"Europe/Prague"`, not
  `"(UTC+01:00) Central European Standard Time"`). Producing .NET's text needs CLDR display data
  this repository does not carry. Out of scope; unchanged.
- **Serialization** (`ToSerializedString` / `FromSerializedString`) — absent, and stays absent per
  `CLAUDE.md`'s permanent-deviation list.
- **`ClearCachedData()` invalidation and `GetSystemTimeZones()` enumerating the database** remain
  documented practical-subset feature gaps. They are pinned beside #2186's regressions but were
  not among its five reference questions and are not represented as deferred verification.
- **Modelling DST inside `TimeZoneInfo`** (`GetUtcOffset`, `IsDaylightSavingTime`,
  `IsAmbiguousTime`, `IsInvalidTime`, `GetAmbiguousTimeOffsets`, `GetAdjustmentRules`) — the
  header's documented limitation. #2185 established that faithful parity requires TZif rule
  parsing, which is an accepted permanent deviation for this practical subset.
- **`InvalidTimeZoneException`** is now thrown for an existing non-TZif data file; #2186 settled
  the earlier #2183 uncertainty from the reference. Missing identifiers still throw
  `TimeZoneNotFoundException`, and the Try door returns false for both.
- **Windows and Emscripten runtime behaviour** — #2418 reviewed and repaired these conditional
  branches and made their tests compile-condition-aware, but the repository's tracked CI is
  Ubuntu-only: there is no Windows/Emscripten cross-build or runtime result to claim here.

---

## 18a. SR-AUD-228 design and final disposition (ticket #2185)

### 18a.1 The defect, restated from measurement

`HasSameRules` compares `baseUtcOffset_` and `supportsDst_` and nothing else. With #2181 landed,
`America/New_York` and `America/Havana` both report a standard offset of −05:00 and
`SupportsDaylightSavingTime` true, so the method returns **true**; .NET returns false, because their
transition dates differ. Fixing #2181 did not fix this and could not: the two zones agree on
everything this type stores.

Note the direction of the failure. This port can never return `false` where .NET returns `true` —
it can only be **too permissive**. That bounds the risk of leaving it as it is, and it is why the
finding is medium rather than high.

### 18a.2 The initially proposed repair, later rejected by measurement

Give `TimeZoneInfo` a `std::vector<std::shared_ptr<AdjustmentRule>> rules_`, populate it in
`FindSystemTimeZoneById` from the year scan #2181 already performs (each scan already observes the
standard offset, the daylight offset, whether a transition occurs and in which months), return it
from `GetAdjustmentRules()`, and make `HasSameRules` compare `baseUtcOffset_`, `supportsDst_` **and**
the rule vector element-wise via the `AdjustmentRule::Equals` that already exists.

Two later probes invalidated that proposal. The existing monthly scan produces byte-identical
2025 samples for New York and Havana even though their transition hours differ. A finer one-year
transition scan still cannot model the several historical rule eras that .NET returns as separate
`AdjustmentRule` values. Faithful content requires the TZif POSIX footer or its full transition
table; libc exposes resolved instants, not the recurrence rules. Sampling at any granularity would
therefore invent .NET-named adjustment rules rather than implement them.

Three alternatives were considered and rejected:

| Alternative | Why not |
|---|---|
| Compare `id_` as well | Wrong for the case .NET gets right: two *differently named* zones with identical rules must compare **true**. Comparing ids would turn a permissive answer into a wrong one. |
| Compare the observed UTC-offset function over a sampled window | Invents an algorithm .NET does not use, and its answer depends on the sampling window. The review declines to invent. |
| Derive rules on demand inside `HasSameRules` without storing them | Avoids the layout change but makes an equality-shaped method perform twelve `setenv`/`tzset` cycles per zone per call, under the process-global timezone lock. |

### 18a.3 The measured layout cost of the rejected proposal

`build-probe/2185_layout.log`, three shapes compiled side by side against the real
`AdjustmentRule`:

```
TimeZoneInfo (real)          sizeof 160  align 8      <- shipped, unchanged by #2177-#2184
model: as shipped            sizeof 160  align 8
model: + one bool            sizeof 160  align 8      <- the tail padding absorbs it
model: + rule vector         sizeof 184  align 8      <- the repair
model: + rule vector, reordered  sizeof 184  align 8  <- no ordering avoids it
```

`std::string` is 32, `TimeSpan` is **24**, `bool` is 1: 128 + 24 + 1 = 153, padded to 160, leaving
seven bytes — enough for another `bool`, not for a 24-byte vector.

`TimeZoneInfo` has no virtual functions, so no vtable is involved and no mangled name changes; the
break is purely that a translation unit compiled against the old header believes the object is 160
bytes and one compiled against the new header believes 184. Instances are handed out through
`std::shared_ptr` from the factories and as `const&` from `Utc()`/`Local()`, so a consumer is
unlikely to embed one by value — but the type is copyable and nothing prevents it, which is exactly
the situation `README.md`'s 2026-07-29 `BitArray::Enumerator` entry records as linking with zero
diagnostics and then giving silently wrong answers.

### 18a.4 Final disposition

The original approval question was:

> *"`System::TimeZoneInfo` may grow from 160 to 184 bytes (one
> `std::vector<std::shared_ptr<AdjustmentRule>>`) so that `HasSameRules` and `GetAdjustmentRules`
> can distinguish two zones that share a base offset and a daylight flag but not their transition
> rules, requiring every consumer to be rebuilt."*

The 2026-08-19 decision is that TZif rule parsing is out of scope. The finding is therefore an
`accepted-deviation`: this implementation can be too permissive but never too strict. The layout
change was not made. The current contract is held by four declaration tests
(`PIN_HasSameRules_CannotDistinguishNewYorkFromHavana`,
`PIN_HasSameRules_OnlyDistinguishesOffsetAndTheDaylightFlag`,
`PIN_GetAdjustmentRules_IsEmptyForEverySystemZone`, and a `static_assert` on `sizeof`), so a future
scope change cannot silently erase the decision.

---

## 19. Completion criteria

`modules/time-zone` is closed for its declared scope because:

1. all seven findings carry exactly one disposition and none has disappeared;
2. #2177–#2184 are implemented, each with permanent tests, and each committed and pushed
   separately;
3. SR-AUD-223/224/225/226/227/229 read `remediated` in `audit/AUDIT_FINDINGS_INDEX.md`;
4. SR-AUD-228 reads `accepted-deviation`, with #2185 recording why sampled rules are not faithful
   and why TZif rule parsing is outside scope;
5. every behaviour change is pinned by a test that fails if it is reverted, proven by mutation;
6. the deliberate breaks are documented in `docs/Migration-TimeZoneStandardOffset.md` and
   `README.md`;
7. the current full 38-executable gate passes without failures or skips;
8. UBSan, ASan+LSan and TSan run over the production bodies with zero reports.

SR-AUD-228 remains a deliberate compatibility limit, not actionable unfinished work.
