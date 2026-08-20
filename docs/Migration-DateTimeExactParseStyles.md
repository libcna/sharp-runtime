<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `DateTime::ParseExact` honours `DateTimeStyles` — #1942 (SA-16.1)

**Additive plus one behaviour change**, under SA-5 and **SA-16.1**. No layout, vtable or `noexcept`
specification moved. Two #2414 pins are **inverted rather than deleted**, because they were
asserting the boundary this ticket exists to move.

## What was blocked, and what unblocked it

#2414 gave `DateTime` a `ParseExact` and deliberately left the style-taking overloads **absent and
pinned absent**, recording why: `AssumeLocal` and `AdjustToUniversal` **convert** rather than merely
stamping a kind, .NET reaches `TimeZoneInfo.Local` internally, and `Core.Base` cannot name a time
zone at all. That is a decision, and **SA-16.1 took it**: the zone is a **parameter**.

That is #1941 phase 2's own shape — `ToLocalTime(zone)` — so the port answers the question once
rather than twice. Both alternatives were declined on the record: accepting only the *stamping*
styles would make a legal .NET style illegal here, and a registration hook is the hidden global
state and static-initialisation-order dependency **#1940 already refused**.

## The grammar gained a zone token

`z`, `zz`, `zzz` and `K` were rejected in **every** mode. They are now admitted behind
`ExactParseOptions::allowZoneToken`, which `DateTime`'s doors set and `DateOnly`'s and
`TimeOnly`'s do not — they have no kind and no offset to carry, and a case pins that the widening
did not reach them. `g` (the era) is still rejected everywhere, and that is a **different** absence:
this port has no era table.

Widths differ and collapsing them changes which inputs parse: **`zzz` and `K` carry `:mm`; `z` and
`zz` do not**. `K` alone **matches the empty string**, which is .NET's rule rather than leniency —
`K` renders empty for an `Unspecified` kind, so a round trip through `o` must read back what it
wrote.

**`o` got its `K` back.** #2414 had to drop it because the pattern would not have compiled, and its
header said a later ticket adding a zone token must revisit that row. This is that row: `o` is now
.NET's pattern in full.

## The matrix, transcribed from `DetermineTimeZoneAdjustments`

**Two cases, and the second is not the first with a default.** When the input carried a zone the
`Assume*` styles do **not** apply at all — .NET says so in its own comment.

**No zone in the input** — four of five outcomes return without converting anything:

| Styles | Result |
|---|---|
| neither `Assume*` | `Unspecified`, ticks untouched |
| `AssumeLocal` | **stamps** `Local`, ticks untouched |
| `AssumeUniversal` + `AdjustToUniversal` | **stamps** `Utc`, ticks untouched |
| `AssumeLocal` + `AdjustToUniversal` | **converts** local → UTC |
| `AssumeUniversal` alone | **converts** UTC → **local** |

That last row is the one a reader most expects to be different, and it is .NET's: the offset is set
to zero and the code **falls through to the local adjustment**, so an assumed-universal value comes
back as `Local`, not `Utc`.

**A zone in the input** — three outcomes:

* `RoundtripKind` **and a literal `Z`** → stamp `Utc`, ticks untouched.
* `AdjustToUniversal` → remove the offset, kind `Utc`.
* otherwise → remove the offset, then convert to local, kind `Local`.

**`RoundtripKind` fires only for a literal `Z`.** .NET tests `ParseFlags.TimeZoneUtc`, which only
`Z` sets, so `+00:00` — which names the same instant — is **converted** rather than stamped. **No
assertion about the value can separate those two**, because both are 12:00 UTC; only the kind can,
and mutation M1 makes them equal and is caught.

An offset outside **±14:00** is a **failure**, not a clamp.

## The one deviation, stated rather than implied

The style-taking overloads take a `const System::ILocalTimeZone*`, defaulted to null. **A null zone
is only an error when a style actually needs one** — so the default costs nothing, and a converting
style with no zone raises `ArgumentNullException` naming `zone`, with a message telling the caller
to pass `System::TimeZone::CurrentTimeZone()`.

**The zone-less doors therefore report rather than silently mismatching.** Before this,
`ParseExact(s, "…K")` failed to parse because the token did not exist; now it names the missing
zone. That is a diagnostic where there was a silent failure, and a case asserts that the same input
and the same door **succeed** under `RoundtripKind` — which is what shows the throw is about the
conversion rather than about the token.

## Evidence

Nine mutations, **all caught**. **M7 was NOT CAUGHT at first and found a defect in my test, not the
code**: the ±14:00 bound looked pinned by `"+15:00"`, but the scanner's own coarse `hours > 14`
already refuses that, so removing the exact bound changed nothing there. **`+14:59` is the row that
separates the two guards** — it passes the hour check and exceeds the exact bound — and both signs
are now asserted, because a bound written on the magnitude and one written on the signed value
differ at exactly one end.

The tests **reuse #1941 phase 2's `FixedZone`** rather than declaring a second one: two zone doubles
in one file is the shape that lets two suites drift apart on what "fixed" means.

Gate: **17,714 / 38, 0 failed, 0 skipped** (+6; `SharpRuntimeTests_Core_Base` 6,126 → 6,132).
Module graph **41 / 95**, unchanged. Downstream: **zero sites**.
