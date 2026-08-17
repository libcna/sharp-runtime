<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `CompositeFormat::Parse` shares one grammar with `String::Format` (ticket #2020)

*2026-08-17.* `System::Text::CompositeFormat::Parse` had its own hand-written scanner. It now
uses `System::detail::scanCompositeFormat`, the same one `System::String::Format` and
`System::FormattableString::ToString` use. This changes which format strings `Parse` accepts —
in **both** directions.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout, vtable or `noexcept`
change, and `String::Format` and `FormattableString::ToString` are **byte-identical** before
and after.

---

## 1. What changed, exactly

`Parse` now rejects six shapes it used to accept, and accepts six it used to reject.

| Format string | Was | Is | Why |
|---|---|---|---|
| `{0,not-a-width}` | accepted, count 1 | `FormatException` | an alignment must be a number |
| `{0,-}` | accepted, count 1 | `FormatException` | a `-` must be followed by digits |
| `{0,}` | accepted, count 1 | `FormatException` | so must a bare comma |
| `{0,- 5}` | accepted, count 1 | `FormatException` | no space between `-` and the digits |
| `{0,x}`, `{0,+5}` | accepted, count 1 | `FormatException` | same rule |
| `{0 }` | `FormatException` | accepted, count 1 | .NET consumes spaces after the index |
| `{0  ,5}`, `{0 :X}` | `FormatException` | accepted, count 1 | …and before the alignment comma |
| `{0,5 }`, `{0,  5  }` | `FormatException` | accepted, count 1 | …and after the alignment digits |
| `{0, -5}` | `FormatException` | accepted, count 1 | …and before the minus sign |

Everything else is unchanged. In particular `{ 0 }` — a **leading** space — is still rejected,
and that asymmetry is .NET's own: the first character after `{` is read as a digit before any
whitespace rule applies (`CompositeFormat.cs:186-190`).

## 2. Why the narrowing half is a repair

The old scanner found the index, then skipped to the closing brace without looking at anything
in between. An alignment component was therefore never validated at all, so a typo in a format
string — `{0,-}` for `{0,-5}` — parsed cleanly and reported the right argument count, and the
error surfaced only when something later actually formatted with it. `String::Format` has
rejected those since #1884.

That gap is what CCF-012 predicts: *altering just one API preserves divergent brace rules*.
There were three composite-format parsers in this repository; there is now one, and the test
`Fix2020_ParseAndFormatNowAgreeOnEveryBraceRule` asserts the agreement over a corpus rather
than asserting it in prose.

## 3. The finding's index-limit claim was wrong, and nothing changed there

Ticket #2020's own description says `Parse("{1500000}")` should be rejected "where .NET's
`AppendFormatHelper` index limit is 1,000,000", and `docs/SystemTextNamespaceReviewPlan.md`
§14.8 is built on that reading.

**.NET has two composite-format grammars and they differ on exactly this point.**
`CompositeFormat.TryParseLiterals` describes itself as *"copied from string.Format"*
(`CompositeFormat.cs:113-116`), and it is — with two conditions removed:

| | index digits | alignment digits |
|---|---|---|
| `AppendFormatHelper` (`ValueStringBuilder.AppendFormat.cs:99,140`) | `while (IsAsciiDigit(ch) && index < IndexLimit)` | `… && width < WidthLimit` |
| `TryParseLiterals` (`CompositeFormat.cs:201,258`) | `while (IsAsciiDigit(ch))` | `while (IsAsciiDigit(ch))` |

Nothing else between the two differs. So .NET's `Parse` has **no** index limit and **no**
alignment limit, this port's answers of `1000001` / `1500001` / `10000001` / `2147483647` were
already right, and adopting the formatter's limits would have introduced a *new* divergence
while claiming to remove one. The single axis is a parameter — `CompositeDigitPolicy` — rather
than a duplicated function.

## 4. The one deliberate deviation

`Parse` throws `FormatException` for an argument index above `2147483646`. .NET does not: it
accumulates the index into an `int` in an unchecked context, so `{2147483648}` **wraps** to a
negative `ArgIndex` that the constructor counts as neither a literal nor a hole
(`CompositeFormat.cs:48-56`), and one value earlier `{2147483647}` computes
`Math.Max(0, int.MaxValue + 1)` and reports `MinimumArgumentCount == 0` for a format string
that needs two billion arguments.

C++ signed overflow is undefined rather than wrapping, so reproducing that is not merely
undesirable but unavailable. Ticket #2010 chose the exception and #2020 keeps it. Every index
.NET answers *correctly* is answered identically.

The alignment value has no such problem because this port's `CompositeFormat` never exposes it
— `Format` and `MinimumArgumentCount` are the whole public surface — so accumulation simply
saturates, and `{0,99999999999999999999}` is accepted exactly as .NET accepts it.

## 5. To migrate

If a format string of yours is in the "was accepted, is `FormatException`" half of §1, it was
already wrong: `String::Format` would have rejected it. Fix the alignment component.

If you were *working around* the old rejection of `{0 }` by stripping spaces, you can stop.

## 6. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched. Neither `cna` nor
`mobile-eggbert` names `CompositeFormat` at all — **zero sites in both**. Each has exactly one
composite-format call site; `cna`'s is `SDL_render_ngage.cpp`'s unrelated Symbian
`TDes::Format`, and `mobile-eggbert`'s is
`WindowsPhoneSpeedyBlupi/Worlds.cpp:192` — `String::Format("worlds/world{0}.txt", …)`, a
single-digit index through the formatter, whose grammar this ticket does not touch. Neither
repository was modified.
