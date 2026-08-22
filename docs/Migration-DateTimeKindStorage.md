<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `DateTime` stores a `DateTimeKind` (ticket #1941, phase 1)

*2026-08-19.* `System::DateTime` now stores and reports a `DateTimeKind`. It gains
`getKindProperty()`, a `DateTime(ticks, kind)` constructor and `SpecifyKind`.

> **Post-#1941 ripple audit, 2026-08-22.** This document originally described the deliberately
> storage-only phase-1 boundary as though it were the lasting `DateTime` contract. It was not:
> after kind-aware conversion and parsing landed, the existing static values and arithmetic also
> had to match .NET. `Now` and `Today` now carry local wall-clock ticks and `Local`, `UnixEpoch`
> carries `Utc`, and every DateTime-returning arithmetic operation preserves the source's complete
> internal kind encoding. The historical approval boundary below remains recorded as history.

**No source break, and no rebuild requirement**: the addition is additive, and the representation
change is private with `sizeof(DateTime)` unchanged at **16** and `sizeof(DateTimeOffset)` at
**48**. Landed under `docs/StandingApprovals.md` SA-3 and the #1929 row 4D approval, whose phase-1
boundary is quoted in `docs/DateTimeExactParsingAndKindDesign.md`.

---

## 1. What was in the original phase, and what was deliberately not

| | |
|---|---|
| **In** | kind storage; `Kind`; `DateTime(ticks, kind)`; `SpecifyKind`; range-check before packing |
| **Out** | `ToLocalTime`, `ToUniversalTime`, offset/`Z` parse conversion, `AssumeLocal`, `AssumeUniversal`, `AdjustToUniversal`, `RoundtripKind`, provider/culture, `ParseExact`, XML bridges |

A phase-2 approval had to name a **date-sensitive timezone provider** before any conversion could
exist. Phase 2 subsequently landed with explicit zone-taking overloads. The declaration pin still
asserts that the no-argument .NET forms are absent; it no longer claims that conversion itself is
absent.

## 2. Why the layout does not move

`MaxTicks` is `0x2BCA2875F4373FFF`, so 62 bits carry every representable value and two are free.
.NET packs the kind into exactly those (`DateTime.cs:118-123,137`), and this port does the same. A
separate member would have grown the object and forced every consumer to rebuild.

## 3. The member is renamed, and that is the safety property

`ticks_` became `dateData_`, reachable only through a private `ticks()` that masks. A bare `ticks_`
used to mean *the tick count*; after packing it would silently mean *the tick count with two flag
bits on top*, and every one of the thirty reads inside this type would have had to be remembered.
Renaming makes the compiler visit each one.

## 4. Tick identity stayed stable; result metadata needed a follow-up

Every constructor that does not take a kind produces `Unspecified` — .NET's default and this
port's previous universal behaviour.

The kind does **not** participate in identity, comparison, hashing, formatting, or component
access. .NET's `operator ==` is
`((d1._dateData ^ d2._dateData) << 2) == 0`, which shifts the flag bits away (`DateTime.cs:1862`),
and `GetHashCode` uses `Ticks` (`:1445-1449`). Comparison, equality, hashing and formatting
therefore still give their previous answers, and tests assert each.

The phase-1 implementation also made arithmetic return `Unspecified`. That preserved the old
kindless observation but was not .NET's lasting contract: `AddTicks`, `AddMonths` and
`Subtract(TimeSpan)` OR `InternalKind` into their results, and all other DateTime-returning
arithmetic delegates to those roots. The follow-up preserves the raw two flag bits rather than
round-tripping through the public `Kind`, so the hidden ambiguous-local encoding will not be lost.

The same audit found three static-value ripples:

- `Now` had UTC ticks despite being the local-time property, and reported `Unspecified`;
- `Today` truncated those UTC ticks and constructed another `Unspecified` value;
- `UnixEpoch` used the tick-only constructor and therefore reported `Unspecified` instead of
  `Utc`.

`Now` now decomposes `system_clock` through the reentrant platform local-time API
(`localtime_r`/`localtime_s`) and then builds local wall-clock ticks while retaining the clock's
sub-second remainder. This follows Core's existing local-clock architecture without introducing a
dependency on the higher `TimeZone` component; Emscripten retains Core's established UTC-as-local
fallback. `Today` is the kind-preserving midnight truncation of that value.

## 5. The reserved fourth encoding

.NET has four encodings, not three: `LocalAmbiguousDst` is the marker a local time inside a
repeated DST hour carries, and `Kind` folds it onto `Local` with a bit trick whose own comment
explains it (`DateTime.cs:1463-1465`).

Nothing in phase 1 set it, and the public constructor refuses kind `3` exactly as .NET's does. The
fold is transcribed anyway so an eventual ambiguity-capable local conversion can set that encoding
without changing a shipped accessor. Current zone-taking conversion still does not produce it.

**A consequence, recorded rather than hidden**: that makes the fold unreachable in this phase, so a
mutation removing it is **not caught**, and was measured not to be.

## 6. Downstream — an opportunity, not a break

`cna`'s XNB `DateTimeReader` (`plan_xnb.md:475`, task XNB-18C) records a **documented deviation**:
it reads the `UInt64`, masks the `DateTimeKind` out of the top two bits and **discards** it,
because *"System::DateTime remains Status: Partial (does not store Kind at all); fixing that
pre-existing sharp-runtime gap would mean touching every existing DateTime constructor, judged out
of scope for this reader task and deferred separately."*

That gap is closed. The reader can now pass the parsed kind to `DateTime(ticks, kind)`, which is
what FNA does. Recorded as ticket **#2381** rather than performed, since `cna` may be read but not
edited.

Measured: **zero** `sizeof(DateTime)` sites in either consumer, so nothing depends on the layout.

## 7. Evidence

| Mutation | Caught |
|---|---|
| `ticks()` stops masking (the packing leaks into `Ticks`) | yes (3 tests) |
| The mask is one bit too wide | yes (3 tests) |
| The kind validation goes away | yes |
| The range check goes away | yes — **only after** a row was added for it |
| The ambiguous-local fold is dropped from `Kind` | **not caught — see §5** |
| `Now` returns UTC-axis ticks or loses `Local` | yes — fixed `EST5`, bounded by system-clock reads |
| `Today` truncates the UTC date or loses `Local` | yes — same fixed-zone boundary |
| `UnixEpoch` loses `Utc` | yes |
| Any public DateTime-returning arithmetic spelling loses the source kind | yes — all twelve doors |
