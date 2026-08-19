<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Numerics::BFloat16` — surface decomposition (ticket #2340, SR-AUD-176)

*2026-08-19.* #2340 exists because SR-AUD-176 had **no owner**: #1766 discovered it, #2261
explicitly rejected it and recorded it as *"left confirmed and unclaimed"*, and #2317 mentioned it
only as a methodological example. This document is its decomposition, and the ticket's own six-unit
plan does not survive measurement.

**Nothing is implemented here.** The output is two bounded tickets, **#2382** and **#2383**.

---

## 1. The measurement

| | Count |
|---|---|
| public members in .NET's `ref/System.Runtime.cs` `BFloat16` surface | **193** |
| interfaces .NET's `BFloat16` implements | **36** |
| `BFloat16.cs` | 2,152 lines |
| members on this port's `BFloat16` | ~28 |
| members on this port's **`Half`** | ~40 |

The finding's arithmetic is right and its conclusion is wrong, because it compares `BFloat16`
against **.NET** when the relevant comparison is against **`Half`**.

## 2. What the finding missed: this repository already decided this, for the sibling type

`System::Half` is the other 16-bit float in this port, and its header carries an explicit
`@note Status: Partial` block (`Half.hpp:25-53`) recording three deviations, decided and
documented long before this ticket:

1. **The ~40 conversion operators** to and from every numeric primitive — including the `checked`
   variants, *"a C# 11 language feature with no C++ equivalent"* — are **not ported**, because
   every one of them is defined in .NET as a trivial `(Half)(float)value` round-trip that a caller
   can write in one line through `ToSingle()`/`FromSingle()`.
2. **The ~40 `MathF`-mirroring statics** — `Exp`, `Log`, `Sin`, `Cos`, `Sqrt`, `Pow`, `Round`,
   `Lerp`, `FusedMultiplyAdd`, **`BitIncrement`/`BitDecrement`**, `ReciprocalEstimate` — are **not
   ported** for the same reason: *"Duplicating the entire float math surface as Half overloads
   adds no capability, only near-identical forwarding boilerplate."*
3. **Generic-math interface conformance** (`INumber<Half>`, `IFloatingPointIeee754<Half>`,
   `IMinMaxValue<Half>`) is **out of scope**, *"consistent with this codebase's position on C#
   generic-math machinery elsewhere."*

**Four of the ticket's six proposed units are therefore already-decided deviations, not missing
work** — and adding any of them to `BFloat16` alone would leave the port's two 16-bit floats
inconsistent with each other, which is worse than either policy applied consistently:

| Ticket's proposed unit | Verdict |
|---|---|
| (3) `BitIncrement`/`BitDecrement` | covered by Half deviation 2 — **named there verbatim** |
| (4) numeric conversions | covered by Half deviation 1 |
| (6) the generic-math contract | covered by Half deviation 3 |
| `CopySign` (part of unit 1) | covered by Half deviation 2 |

## 3. The real gap, measured member by member

Diffing the two headers gives the whole of it — **fifteen members** `Half` has and `BFloat16`
does not:

| Group | Members |
|---|---|
| classification | `IsFinite`, `IsNegative`, `IsNormal`, `IsSubnormal` |
| ordering and identity | `CompareTo(BFloat16)`, `Equals(BFloat16)`, `GetHashCode()` |
| parsing | `Parse(string)`, `Parse(string, IFormatProvider*)`, `TryParse(string, BFloat16&)`, `TryParse(string, IFormatProvider*, BFloat16&)` |
| formatting | `ToString(string)`, `ToString(IFormatProvider*)`, `ToString(string, IFormatProvider*)`, `TryFormat(Span<char>, intcs&, string)` |

`BFloat16` already has `IsNaN`, `IsInfinity`, `IsPositiveInfinity` and `IsNegativeInfinity`, which
the finding listed as missing — so **the finding's classification claim is a third wrong**.

Every one of the fifteen is a forward to `System::Single` in `Half`'s own body, so the unit is
bounded by construction and adds no new algorithm. It is **#2382**.

## 4. A premise correction the ticket's own gate rests on

The ticket says units 5 and 6 *"carry a reference-text dependency of the #2252/#2260 class: .NET's
exact format and exception strings are unverifiable while `/rv` is absent."*

**`/rv/tmp/runtime` is present in this container** (`docs/StandingApprovals.md`), so that gate is
dissolved. It would not have applied anyway: #2382's formatting members forward to
`System::Single`, whose format and exception strings this port already settled.

## 5. What is left, and why it is a policy question rather than a port gap

Everything else `BFloat16` lacks, `Half` lacks too, **deliberately**. Reopening it is a decision
about the port's position on 16-bit floats, not a defect in one type:

* `Abs`, `Sign`, `Clamp`, `CopySign`, `MaxMagnitude`/`MinMagnitude`, `BitIncrement`/`BitDecrement`;
* the trigonometric, exponential, logarithmic, root, hyperbolic and power families;
* the conversion operators to and from the other numeric primitives;
* generic-math conformance.

That is **#2383**, and its first constraint is written into it: it must be decided **for `Half`
and `BFloat16` together, or not at all**. Generic-math conformance is excluded from it outright —
it is a permanent deviation of this port, not a 16-bit-float question.

## 6. Outcome

SR-AUD-176 has an owner and a written decomposition. It decomposes into **two** tickets, not six:

* **#2382** (P3, S) — bring `BFloat16` to `Half`'s decided line: the fifteen members in §3, plus
  the `@note Status` deviation block, so the omissions in §5 are **declared** rather than looking
  like oversights. Additive; nothing observable changes.
* **#2383** (P3, needs a decision) — whether this port's 16-bit floats should carry the
  `MathF`-forwarding and conversion surface both currently decline. Must move `Half` and
  `BFloat16` together.

#2262 already fixed the rounding defect (SR-AUD-175) and is not reopened.
