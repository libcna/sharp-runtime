<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `DateTime` stores a `DateTimeKind` (ticket #1941, phase 1)

*2026-08-19.* `System::DateTime` now stores and reports a `DateTimeKind`. It gains
`getKindProperty()`, a `DateTime(ticks, kind)` constructor and `SpecifyKind`.

**No source break, and no rebuild requirement**: the addition is additive, and the representation
change is private with `sizeof(DateTime)` unchanged at **16** and `sizeof(DateTimeOffset)` at
**48**. Landed under `docs/StandingApprovals.md` SA-3 and the #1929 row 4D approval, whose phase-1
boundary is quoted in `docs/DateTimeExactParsingAndKindDesign.md`.

---

## 1. What is in this phase, and what is deliberately not

| | |
|---|---|
| **In** | kind storage; `Kind`; `DateTime(ticks, kind)`; `SpecifyKind`; range-check before packing |
| **Out** | `ToLocalTime`, `ToUniversalTime`, offset/`Z` parse conversion, `AssumeLocal`, `AssumeUniversal`, `AdjustToUniversal`, `RoundtripKind`, provider/culture, `ParseExact`, XML bridges |

A phase-2 approval must name a **date-sensitive timezone provider** before any conversion can
exist. `Decl1941_TheConversionSurfaceIsStillAbsent` pins the absence, so phase 2 is a deliberate
act rather than a drift.

## 2. Why the layout does not move

`MaxTicks` is `0x2BCA2875F4373FFF`, so 62 bits carry every representable value and two are free.
.NET packs the kind into exactly those (`DateTime.cs:118-123,137`), and this port does the same. A
separate member would have grown the object and forced every consumer to rebuild.

## 3. The member is renamed, and that is the safety property

`ticks_` became `dateData_`, reachable only through a private `ticks()` that masks. A bare `ticks_`
used to mean *the tick count*; after packing it would silently mean *the tick count with two flag
bits on top*, and every one of the thirty reads inside this type would have had to be remembered.
Renaming makes the compiler visit each one.

## 4. Nothing that existed changed its answer

Every constructor that does not take a kind produces `Unspecified` — .NET's default and this
port's previous universal behaviour.

The kind participates in **nothing** that existed. .NET's `operator ==` is
`((d1._dateData ^ d2._dateData) << 2) == 0`, which shifts the flag bits away (`DateTime.cs:1862`),
and `GetHashCode` uses `Ticks` (`:1445-1449`). Comparison, equality, hashing, arithmetic and
formatting all give the answers they gave before, and a test asserts each.

**Arithmetic does not carry the kind**, and that is stated rather than assumed: propagating it is a
phase-2 question, because it is only meaningful once a conversion exists to be consistent with.

## 5. The reserved fourth encoding

.NET has four encodings, not three: `LocalAmbiguousDst` is the marker a local time inside a
repeated DST hour carries, and `Kind` folds it onto `Local` with a bit trick whose own comment
explains it (`DateTime.cs:1463-1465`).

Nothing in phase 1 sets it, and the public constructor refuses kind `3` exactly as .NET's does. The
fold is transcribed anyway, because phase 2 will start setting that encoding and a fold added later
would be a second change to a shipped accessor.

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
