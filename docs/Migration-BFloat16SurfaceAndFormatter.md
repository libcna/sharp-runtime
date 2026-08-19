<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `BFloat16` gains sixteen members, and its `ToString()` stops saying `"inf"` (ticket #2382)

*2026-08-19.* `System::Numerics::BFloat16` reaches the line this port already decided for
`System::Half`, its sibling 16-bit float. Almost all of it is additive; **one existing member's
output changed for 256 of the 65,536 bit patterns** (§3). Landed under SA-5; `sizeof(BFloat16)` is
unchanged at **2**, so nothing needs rebuilding for layout.

The decomposition this implements is `docs/BFloat16SurfaceDecomposition.md` (#2340).

---

## 1. What was added

| Group | Members |
|---|---|
| classification | `IsFinite`, `IsNegative`, `IsNormal`, `IsSubnormal`, `IsZero` |
| ordering and identity | `CompareTo(BFloat16)`, `Equals(BFloat16)`, `GetHashCode()` |
| parsing | `Parse(string)`, `Parse(string, IFormatProvider*)`, `TryParse(string, BFloat16&)`, `TryParse(string, IFormatProvider*, BFloat16&)` |
| formatting | `ToString(string)`, `ToString(IFormatProvider*)`, `ToString(string, IFormatProvider*)`, `TryFormat(Span<char>, intcs&, string)` |

`IsZero` is the one member `System::Half` does not have. Including it follows the reference rather
than exceeding the decomposition: .NET's `BFloat16` declares it **public**, while .NET's `Half`
exposes it only through the generic-math interface this port does not implement.

The header also gained the `@note Status` deviation block `Half` carries, so the surface this type
deliberately omits is **declared**. That omission being undeclared is what let SR-AUD-176 be raised
as a defect in the first place.

## 2. "Half's line" is not "Half's bodies"

The decomposition says to bring `BFloat16` to the *surface* `Half` carries. Copying `Half`'s
*implementations* would have been wrong, and for the identity trio it would have been silently
wrong:

.NET's `BFloat16` delegates all three to `float` — `((float)this).CompareTo(...)`,
`.Equals(...)`, `.GetHashCode()` (`BFloat16.cs:354, 379, 389`). This port's `Half` instead
implements its own bit-level versions. For `CompareTo` and `Equals` the two agree. For
**`GetHashCode` they do not**: `Half::GetHashCode` masks to 16 bits and can only return a value
below `0x10000`, whereas .NET's `BFloat16` hash is `float`'s, over the widened value. Copying
`Half` here would have compiled, satisfied the hash contract, and produced the wrong number. A
test asserts the hash is `Single`'s and is above `0x10000`.

The classification constants differ for the same reason: `IsNormal`'s boundary is `0x0080` and
`IsSubnormal`'s upper bound `0x007F` — eight exponent bits and a seven-bit significand, not
`Half`'s `0x0400`/`0x03FF`. Both mutations are caught.

`IsNegative` is a **sign-bit test**, not a comparison (`BFloat16.cs:181-185` is
`(short)(value._value) < 0`), so `IsNegative(-0.0f)` and a negative NaN are both `true`.

## 3. The behaviour change: `ToString()` had a second formatter

`BFloat16::ToString()` was a bare `std::to_chars` — a **second formatter** beside
`System::Single::ToString`. Compared exhaustively over all 65,536 bit patterns, **256 disagreed**:
the two infinities and all 254 NaNs.

| | Was | Is |
|---|---|---|
| `BFloat16(+inf).ToString()` | `"inf"` | `"Infinity"` |
| `BFloat16(-inf).ToString()` | `"-inf"` | `"-Infinity"` |
| any NaN `.ToString()` | `"nan"` / `"-nan"` | `"NaN"` |
| every finite value | — | **byte-identical** |

.NET's is `Number.FormatFloat(...)` (`BFloat16.cs:394`), which never produces the lowercase C
forms. There is now one formatter, so the two cannot drift apart again, and a test asserts the
agreement across all 65,536 patterns rather than a sample.

**This was found by a mutation, not by a test.** Dropping `ToString(format)`'s empty-format branch
was *not caught* — which is only possible if the two formatters agree. Measuring whether they
really did is what surfaced the 256 that do not.

To migrate: if you match on `BFloat16::ToString()`'s output for a non-finite value, expect .NET's
spelling. The change is a repair in the direction of the reference, and `ToString("F2")` and
friends already produced `"Infinity"` before this ticket, so the type was inconsistent with itself.

## 4. Two guards deleted rather than defended

`ToString(const std::string&)` no longer names the non-finite values before consulting the format,
and no longer short-circuits an empty format. Both were mutations that could not be caught, and
both for good reason: `Single::ToString` already answers `"NaN"`/`"Infinity"`/`"-Infinity"` at
every format kind, and once `ToString()` became `Single::ToString(value, "")` the empty-format
branch was the *same call* rather than a check. `System::Half` has both guards and both are dead
there for the same reason — which is **#2383**'s business, not this ticket's.

## 5. Evidence

Eleven mutations, nine caught, two equivalences deleted:

| Mutation | Result |
|---|---|
| `IsNormal` uses `Half`'s `0x0400` boundary | caught |
| `IsSubnormal` uses `Half`'s `0x03FF` bound | caught |
| `IsNegative` becomes a comparison | caught |
| `GetHashCode` copies `Half`'s masking | caught |
| `Equals` becomes `operator==` | caught |
| `TryParse` leaves the out parameter untouched on failure | caught |
| `TryFormat` writes a truncated prefix | caught |
| `IsZero` swallows subnormals | caught |
| `ToString()` back to `std::to_chars` | caught |
| non-finite guard in `ToString(format)` | **equivalence — deleted** (§4) |
| empty-format branch in `ToString(format)` | **equivalence — deleted** (§4) |

`Fix2382_ClassificationCoversTheWholeDomain` also asserts that every one of the 65,536 patterns is
in **exactly one** of zero / subnormal / normal / non-finite, which is what makes the four
predicates a partition rather than four independent claims.

The declined surface is pinned by a `static_assert` that `Abs`, `BitIncrement`, `CopySign` and
`Sqrt` are **absent**, with a message naming #2383 — so adding one to `BFloat16` alone fails the
build. The `requires` clauses take a **dependent** parameter, because gcc evaluates a
non-dependent one eagerly and hard-errors instead of yielding false (measured on #2299, recorded
in CLAUDE.md, and hit again here).

## 6. Downstream, measured

`cna` and `mobile-eggbert` reference `BFloat16` in **zero** places. Neither was modified.
