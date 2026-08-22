<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# DateTime gains ParseExact — ticket #2414

**Purely additive. No existing signature, layout, vtable, `noexcept` specification or accepted
input changed, and no consumer needs to be edited or rebuilt for this.** It is recorded because
the ticket it came out of, #1942, is *not* closed by it, and because two of .NET's standard
patterns are transcribed with a named loss.

> **Current-state note (post-#1942).** The body of this document records the deliberately narrow
> #2414 checkpoint. #1942 subsequently added the `DateTimeStyles` overloads, admitted `z`/`K` for
> DateTime, restored `K` to the `o`/`O` pattern, and takes an explicit `ILocalTimeZone` whenever a
> style must convert. The providerless, zone-free shapes below still return Unspecified when their
> input carries no zone; the historical claims that *every* format has no zone token and *every*
> result is Unspecified are not the current full API contract.

## What was there before

`System::DateTime`'s entire parse surface was:

```cpp
static DateTime Parse(const std::string& s);
static bool     TryParse(const std::string& s, DateTime& result);
```

There was **no `ParseExact` overload of any kind.** This is why #1942 — the exact-parsing
`DateTimeStyles` contract — had nowhere to land: there was no exact-parsing member for a style or a
format provider to reach. It is the same cycle #2412 resolved for `DateOnly` and `TimeOnly` one
type over, and it is recorded here rather than rediscovered a third time.

## Why the scanner, not the type, was the obstacle

`System::detail::MatchExactFormat` took a `bool forDate` and ran **one** of two blocks:

* the date block handled `y`/`M`/`d` and then **rejected every time token** as unsupported;
* the time block handled `H`/`h`/`m`/`s`/`f`/`t` and rejected every date token.

So a format naming both families could not be matched by any spelling — not an omission in
`DateTime` but a consequence of the scanner's shape. The parameter is now
`ExactTokenSet { Date, Time, DateAndTime }`.

**Admitting both families resolves no ambiguity**, which is what makes the widening safe: the two
token sets are disjoint, because `M` is a month and `m` a minute and both languages are
case-sensitive here. The change is that the other family's tokens **fall through** to the other
block instead of being rejected, and each rejection survives, guarded on that family not being
admitted at all. `DateOnly` and `TimeOnly` pass `Date` and `Time` and are byte-for-byte unchanged
in behaviour — pinned by `WideningDidNotOpenTheSingleFamilyModes`, and by mutation M3, which admits
both families for `DateOnly` and is caught.

## New API

```cpp
static DateTime ParseExact   (const std::string& input, const std::string& format);
static DateTime ParseExact   (const std::string& input, const std::string& format,
                              const System::IFormatProvider* provider);
static bool     TryParseExact(const std::string& input, const std::string& format,
                              DateTime& result);
static bool     TryParseExact(const std::string& input, const std::string& format,
                              const System::IFormatProvider* provider, DateTime& result);
```

A null `provider` means the invariant culture, which is what .NET's own null means.

## Standard formats: a separate table, and two of them lose something

The `DateTime` table is **not** the date-only and time-only tables concatenated — `s` uses a `T`
separator where `u` uses a space, and `R` ends in a literal `GMT` the date-only `R` does not have.

| Format | Pattern | Note |
|---|---|---|
| `s` | `yyyy-MM-ddTHH:mm:ss` | .NET's, in full. |
| `u` | `yyyy-MM-dd HH:mm:ss'Z'` | .NET's, in full. The `Z` is a **literal**, not a zone token, so it is required and sets no kind. |
| `R`/`r` | `ddd, dd MMM yyyy HH:mm:ss 'GMT'` | .NET's, in full. Validates the weekday. |
| `o`/`O` | `yyyy-MM-ddTHH:mm:ss.fffffff` | **At the #2414 checkpoint**, .NET's trailing `K` was absent. #1942 restored it and admitted the corresponding zone token. |

**At the #2414 checkpoint the exact grammar carried no zone token at all** — `z`, `K` and `g` were
rejected in every mode — which was the boundary the `o` row ran into. #1942 moved that boundary by
admitting `z`/`K` for DateTime and DateTimeOffset while retaining `g` as the separate unsupported
era token. The old declaration pin was inverted rather than deleted.

## What #2414 deliberately did not do, and how #1942 closed it

**The `DateTimeStyles`-taking overloads were absent and pinned as absent at this checkpoint.** They
needed a local time zone that the original signature had nowhere to get:

* `AssumeLocal` and `AssumeUniversal` only **stamp** a kind — those would be fine.
* `AdjustToUniversal` must **convert**, and conversion needs a zone. .NET reaches
  `TimeZoneInfo.Local` internally; `Core.Base` cannot, and #1941 phase 2 resolved exactly that one
  level down by **taking the zone as a parameter**. So the styles overload needs the same decision
  made about its signature.
* `RoundtripKind` has **nothing to preserve**, because the grammar carries no zone token, so an
  input can never state its own kind.

That was a user decision of the same shape as SA-15.1's and was not taken in #2414. #1942 later
took it consistently: the new overloads accept `ILocalTimeZone*`, require it only on a converting
path, and retain the no-zone call for every style/input combination that does not convert.

## Behaviour worth knowing

* **A complete date is mandatory; time components default to midnight.** `ParseExact("2024-06-15",
  "yyyy-MM-dd")` is a valid call returning 00:00:00, as in .NET. The reverse is **not** symmetric:
  a format binding only a time is **refused**, because `NoCurrentDateDefault` — the style that
  decides what happens then — is #1942's, and inventing a default under a ticket that has not been
  asked about it is what this refusal prevents.
* **At #2414 the kind was always `Unspecified`**, for every input and format then supported. The
  post-#1942 style-taking surface can now return Local or Utc; an unqualified, zone-free input with
  no kind-affecting style remains Unspecified.
* **The twelve-hour clock is neither `+12` nor a no-op**: 12 AM is hour 0 and 12 PM is hour 12, so
  both ends are special cases. Mutation M5 writes it as a plain `+ 12` and is caught.

## Evidence

Eight valid mutations, all caught (M1, M3, M4, M5, M6, M7, M8, M9). **M2 is a proven equivalence
and is recorded at the site rather than counted**: dropping the `!allowDate` condition from the
time block's date-token rejection changes nothing, because every date-token arm ends in `continue`
and a date token therefore cannot reach that line while the date block is running. The condition is
kept because it states *why* the rejection is correct, and it becomes load-bearing the day an arm
above stops consuming its token.

M7 was **invalid as first written** (`-Werror=unused-parameter`) and was reformulated rather than
counted.

**A first run of M4–M8 was invalid and is recorded rather than quietly discarded.** The restore step
after M3 used `git checkout` on a file whose #2414 change was still **uncommitted**, so the tree was
broken for five consecutive runs and all five reported "BUILD FAILED" — a harness state read as five
mutation verdicts. It is the #2374 restore mistake in a new form, and the check that caught it is
the same one: five identical failures in a row are a harness result, not a finding.

Gate: **17,683 / 38, 0 failed, 0 skipped**, `SharpRuntimeTests_Core_Base` 6,111 → 6,118.
Downstream: **zero `ParseExact` sites** in `cna` and `mobile-eggbert` — the member did not exist.
