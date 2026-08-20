<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `TimeSpan::ParseExact` — #1943 (TimeSpan half)

**Purely additive, under SA-5.** No existing signature, layout, vtable, `noexcept` specification or
accepted input changed; nothing needs migrating and no consumer rebuilds.

## The gap

`System::TimeSpan`'s entire parse surface was `Parse(s)` and `TryParse(s, result)` — **there was no
`ParseExact` in any spelling**, so a caller could not parse a duration against a stated layout.
`System::Globalization::TimeSpanStyles` existed in `modules/globalization` and **nothing could
consume it**, exactly the shape #1997 group A-3 found for `UriCreationOptions`.

`TimeSpanStyles.hpp` moved into `Core.Base` — #1940's shape C for the third time (after
`DateTimeFormatInfo` and `DateTimeStyles`), and again **not one `#include` line changed**, because
module ownership is by logical path uniqueness. Graph unchanged at **41 / 95**; catalogue
regenerated.

## Why this is a separate scanner, and why sharing one would be wrong

The obvious implementation reuses `InvariantExactDateTimeParser.hpp`. **It would be wrong in a way
that passes most tests.** .NET keeps `TimeSpanParse.TryParseByFormat` apart from `DateTimeParse`,
and the token table shows why:

* **An unquoted literal is an ERROR.** `TryParseByFormat`'s `switch` ends in
  `default: return result.SetInvalidStringFailure();`, so **`"hh:mm"` is not a valid `TimeSpan`
  format** — the colon must be `"hh':'mm"` or `"hh\:mm"`. The date/time scanner *matches* an
  unquoted literal against the input, so a shared scanner would silently **accept a format .NET
  rejects**.
* **There is no sign token at all** — no `-`, no `+`.
* **Each component may appear at most once**, tracked by five `seen*` flags.
* **`d`'s digit rule is not the others'**: one specifier means **1..8** digits, more than one means
  **exactly** that many. A scanner written "exactly `tokenLen`" everywhere gets days wrong and
  passes every hour/minute/second row.
* **`f` requires its digits where `F` makes them optional** — .NET calls the same reader for both
  and simply **ignores the result** for `F`.

## `AssumeNegative` is the only route to a negative result

Because the custom grammar has no sign token, `ParseExact("-01:30", "hh':'mm")` **fails** while
`ParseExact("01:30", "hh':'mm", nullptr, AssumeNegative)` is minus ninety minutes. That is what
makes `TimeSpanStyles` load-bearing rather than decorative, and it is the property a reader most
expects to be wrong.

**The standard formats ignore the style**, as .NET does: `c` carries its own sign, so
`AssumeNegative` must not flip a value that already said what it was. Mutation M6 makes it flip and
is caught.

## Bounds are per component, not a total range

.NET's own constants (`TimeSpanParse.cs:59-62`): days ≤ 10675199, hours ≤ 23, minutes ≤ 59,
seconds ≤ 59. So `"25"` against `"hh"` **fails** rather than carrying into days — the reading a
total-range check would give. Mutation M4 makes it a total range and is caught.

## What is implemented, and what is pinned absent

| Specifier | State |
|---|---|
| `c`, `t`, `T` | implemented — one format under three names, delegating to the general `TryParse`, which is that same invariant constant grammar |
| custom formats | implemented |
| `g`, `G` | **pinned absent** |

`g` and `G` are .NET's **localized** standard formats and two things stop them: they need a
culture's decimal separator, which this port has no database for (#2410's boundary), and their
grammars have **optional components** (`g` is `[-][d':']h':'mm':'ss[.FFFFFFF]`) that the
custom-format scanner cannot express — each would need its own hand-written arm. A later ticket
adding them trips the pin.

An empty format and any other single character are **bad format specifiers**, as in .NET.

## A `Try*` method that throws

An illegal `TimeSpanStyles` **raises** where a parse failure returns false — .NET's own shape — and
**validation runs before the result is written**, so a rejected style leaves the caller's variable
untouched. That is two claims and each has its own assertion.

## Evidence

Nine mutations, **all caught**. Two were **invalid as first written and reformulated rather than
counted**: M2's anchor did not match the file (a whitespace mismatch, so the edit never applied —
an ambiguous or absent anchor is a harness state, not a finding), and M9's first spelling left a
local unused and was rejected by `-Werror=unused-variable`.

Gate: **17,708 / 38, 0 failed, 0 skipped** (+8; `SharpRuntimeTests_Core_Base` 6,118 → 6,126).
Module graph **41 / 95**, unchanged; catalogue regenerated. Downstream: **zero sites** in `cna` and
`mobile-eggbert`.

## What #1943 still has left

`DateTimeOffset::ParseExact`. It needs a zone for the **no-offset** case: .NET's
`DateTimeStyles.None` gives the result the **local** offset, and `Core.Base` cannot name a zone —
the same decision #1942 is waiting on. **An offset is not a time zone**, so a format carrying an
explicit offset would need no zone at all; adding an offset token to the exact grammar is the route,
and it is recorded here rather than taken, because the no-offset default still needs the answer.
`XmlConvert::ToDateTimeOffset(s, format)` composes the two today (#1945) in the module that *can*
name a zone, and a later `DateTimeOffset::ParseExact` should **absorb** that body rather than sit
beside it.
