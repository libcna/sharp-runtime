# Audit: `modules/time-zone/include/System/TimeZoneInfo.hpp`

## Metadata

- AUDITED: 718-line `TimeZoneInfo` public surface, including transition and
  adjustment value types, lookup, conversion, equality, and factory APIs.
- Validation: `TimeZoneInfoTests.*` plus `AdjustmentRuleTests.*` passed 99/99
  on 2026-07-27; the complete TimeZone fixture set previously passed 114/114.
  Direct C++20/current-.NET 10 probes exercised failed TryFind, factories,
  equality, HasSameRules, and an IANA system zone.
- Reference basis: current .NET 10 `TimeZoneInfo` public behavior and installed
  API reference documentation.  Explicitly documented native DST/serialization
  limitations are retained as adaptations, not duplicate findings.

## SR-AUD-224 — medium — failed TryFind leaves a caller's stale zone in its out parameter

`TryFindSystemTimeZoneById` catches lookup failure but never resets `result`.
Pass an existing UTC result and query `Mars/Olympus`: C++ prints
`tryFind_found=0 result=UTC`; current .NET prints
`tryFind_found=False result=null`.  Callers that honor the false return can
still accidentally reuse a stale, unrelated zone.

## SR-AUD-225 — medium — CreateCustomTimeZone accepts empty IDs and offsets outside the managed range

The factory copies every input without validation.  C++ accepts both an empty
ID and a +15-hour offset; current .NET rejects them with `ArgumentException`
and `ArgumentOutOfRangeException`, respectively.  The supported managed range
is ±14 hours and an ID is required, so the native API creates values its
counterpart cannot represent.

## SR-AUD-226 — medium — AdjustmentRule accepts an effective end date before its start date

Both adjustment factories blindly retain their date range.  A 2025-01-02
start with 2025-01-01 end returns normally in C++; the identical current-.NET
call throws `ArgumentException`.  The 19 green AdjustmentRule tests cover only
ascending valid ranges.

## SR-AUD-227 — medium — zone equality is case-sensitive while its hash is case-insensitive and .NET treats custom IDs case-insensitively

`Equals` compares `id_` byte-for-byte whereas `GetHashCode` lowercases it.
Custom `"Zone"` and `"zone"` compare false in C++ but true in current .NET.
This creates incompatible equality semantics and a confusing local
equality/hash boundary that the same-ID-only fixture cannot expose.

## SR-AUD-228 — medium — HasSameRules reduces distinct adjustment histories to current offset plus a DST flag

The method compares only `baseUtcOffset_` and `supportsDst_`, discarding every
actual rule.  C++ reports `America/New_York` and `America/Havana` as same-rule
zones; current .NET reports false.  The public native type advertises
HasSameRules even though its abbreviated state cannot represent the required
identity.

## SR-AUD-229 — medium — IANA lookup records the current daylight offset as BaseUtcOffset rather than the zone's standard offset

POSIX lookup stores `tm_gmtoff` for `time(nullptr)` directly as
`baseUtcOffset_`.  During the current New York daylight period C++ reports
`baseOffset_newYork=-4`, while current .NET reports the invariant standard
`-5`.  This also makes custom/system conversions depend on the month in which
the zone object was created, contrary to the property contract.

## Assessment

UTC and ordinary fixed-offset custom conversions work under the documented
subset.  The six findings arise from input/result handling and metadata that
the native surface does claim to provide; they are independent of the explicit
no-DST transition adaptation.

## Other missing assertions and diagnostics

- Add failed TryFind result-clearing, empty/overlong/offset-minute factory,
  reversed/range-invalid adjustment-rule, and case-variant equality/hash tests.
- Add fixed calendar-date New York/Havana metadata/rule comparisons in a
  process-isolated timezone database environment.
- Test null-equivalent/native empty inputs, DateTime/DateTimeOffset kind and
  range boundaries, conversion overflow, ClearCachedData observable behavior,
  and IANA/Windows mapping collision/territory choices.

## Final assessment

SR-AUD-224 through SR-AUD-229 are confirmed by direct C++/current-.NET
comparison.  No production or test source was changed.

---

## Remediation record — tickets #2177, #2178, #2179, #2180, #2181; #2185 gated (2026-08-10)

Original evidence above is retained unchanged; this section appends what measurement added. All six
findings in this report reproduced in one process before any change was made
(`build-probe/2176_probe1_surface.log`).

| Finding | Outcome | Ticket |
|---|---|---|
| SR-AUD-224 | **remediated** | #2177 |
| SR-AUD-225 | **remediated** | #2178 |
| SR-AUD-226 | **remediated** | #2179 |
| SR-AUD-227 | **remediated** | #2180 |
| SR-AUD-229 | **remediated** | #2181 (record in the paired `.cpp` report) |
| SR-AUD-228 | **confirmed (design-complete)** — gated | #2185 |

**SR-AUD-224.** The catch-all now calls `result.reset()` before returning false, matching .NET's
assignment of null. Deleting that one line fails four of the six new pins and leaves the two
controls green.

**SR-AUD-225, with a correction this report does not record.** Besides the empty id and the
±15-hour offset, measurement found a **90-second offset** and **`TimeSpan::MinValue`** accepted,
and the latter left a public door onto `TimeSpan`'s negation guard — `ConvertTimeToUtc` reported
*"Negating the minimum value of a twos complement number is invalid."* rather than the argument
being refused where it was supplied. `CreateCustomTimeZone` now rejects an empty id
(`ArgumentException`) and validates the offset (`ArgumentOutOfRangeException` beyond ±14 hours,
`ArgumentException` finer than whole minutes); ±14:00 exactly is still accepted. The whole-minute
rule is an **extension** of the finding, justified by this report's own sentence that the factory
"creates values its counterpart cannot represent", and is listed in ticket #2186 for text-level
verification.

**SR-AUD-226, and a second overload.** This report names one call; measurement found **both**
`CreateAdjustmentRule` overloads accepting the reversed range, and both now throw. Equal dates stay
legal. Three adjacent validations measured as *also* missing — a `dateStart` carrying a time-of-day,
a `daylightDelta` beyond ±14 hours, and a sub-minute `daylightDelta` — were deliberately **not**
repaired: the managed probe recorded here covers only the reversed range, and they are carried by
ticket #2186 and held by a `PIN_` test.

**SR-AUD-227, and a defect this report does not name.** The old `GetHashCode` folded case through
`std::tolower`, whose answer depends on the process `LC_CTYPE`. `Equals` and `GetHashCode` now share
one ordinal ASCII fold, which repairs the .NET divergence and the equality/hash contract breach in
the same change and removes the locale dependence.

**Missing assertions this report asked for, now present** (+37 tests in `TimeZoneInfoTests.cpp`):
failed-`TryFind` result clearing (four shapes), empty/overlong/offset-minute factory rejection,
reversed and range-invalid adjustment rules on both overloads, case-variant equality and hash
agreement including non-ASCII bytes and the `@`/`[` boundary bytes, fixed calendar-date New
York/Havana metadata and rule comparison, `DateTime` range boundaries in all four conversion
directions, and `ClearCachedData`'s observable behaviour. Null-equivalent and empty native inputs
are covered by the identifier matrix. IANA↔Windows mapping collision and territory choice remain
untested and are recorded as an exclusion.

**SR-AUD-228 is the one finding this batch did not close**, and deliberately so. The port stores no
adjustment rules, so `HasSameRules` cannot return `false` where .NET does — the failure is
one-directional, never too strict. The selected repair, three rejected alternatives, and the
measured gate (`sizeof(TimeZoneInfo)` **160 → 184**, with no member ordering that avoids it) are in
`docs/SystemTimeZoneNamespaceReviewPlan.md` §18a. Four `PIN_` tests and a `static_assert` hold the
current behaviour so the question cannot be answered silently.

**No public signature, virtual function, vtable slot, object layout, mangled symbol or `noexcept`
specification changed** by any of #2177–#2181; `sizeof(TimeZoneInfo)` is 160 before and after.
