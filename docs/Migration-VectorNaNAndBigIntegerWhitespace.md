<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — vector `Min`/`Max` propagate NaN from either operand, and `BigInteger::Parse` accepts whitespace (ticket #2174)

*2026-08-18.* #2174 asked four `System::Numerics` parity questions that were *"blocked on evidence,
not on approval: `/rv` is absent and no managed runtime is available to probe"*. `/rv` answers all
four. **Two were already matches, one is a match with a corrected explanation, and one was a real
divergence in both directions.**

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. The four answers

| Question | .NET | This port, before | Outcome |
|---|---|---|---|
| 1. `Complex.ToString` brackets and separator | `<real; imaginary>` (`Complex.cs:367-375`) | `<real; imaginary>` | **already correct** |
| 2. `BigInteger.Parse` whitespace | accepted — `NumberStyles.Integer` (`BigInteger.cs:798-801`) | **threw** | **repaired** |
| 3. `BigInteger << int.MaxValue` | the same unbounded work | the same | **already correct** |
| 4. Vector `Min`/`Max` NaN, `Clamp` inverted bounds | NaN from **either** side; `Clamp` follows HLSL | NaN from the **first** side only; `Clamp` correct | **half repaired** |

## 2. Question 1 — the two citations disagreed and the source wins

SR-AUD-277 called `<a; b>` a divergence, citing .NET's constructor doc example `(26.1, 18.06)`;
the same report linked the .NET source, which builds `<`, real, `; `, imaginary, `>`. The doc
example is stale prose about a *constructor*, not about `ToString`. The port was right, and the
pin is now a transcription rather than a placeholder.

## 3. Question 2 — the port was stricter than the style it claims to implement

`BigInteger.Parse(string)` is `Parse(value, NumberStyles.Integer)`, and `NumberStyles.Integer` is
`AllowLeadingWhite | AllowTrailingWhite | AllowLeadingSign`. So the whitespace is **the default
style's own contract**, not a leniency this port declined.

| Input | Was | Is |
|---|---|---|
| `" 1"`, `"1 "`, `"  -42\t"` | `FormatException` | parsed |
| `"\v\f5\v\f"` | `FormatException` | parsed — 0x0B and 0x0C are inside .NET's set |
| `"\xC2\xA0" "5"` (non-breaking space) | `FormatException` | `FormatException` — outside it |
| `"1 2"`, `"- 1"` | `FormatException` | unchanged — inner whitespace is grammar |
| `"   "`, `""` | `FormatException` | unchanged |

The trimmed set is .NET's `IsWhite` — `0x20` and `0x09..0x0D`
(`Number.Parsing.Common.cs:309`) — not `std::isspace`. See §7 for what that does and does not buy.

## 4. Question 3 — not a defect, and the ticket's own measurement was one-sided

The ticket measured that `BigInteger(1) << INTCS_MAX` does not return within 30 seconds and asked
whether that is a defect. It is not: .NET's `operator <<` has no early-out for a large positive
shift (`BigInteger.cs:2566-2588`), so it allocates `shift / BitsPerLimb` limbs — 256 MB of them
for `int.MaxValue` bits. .NET does the same unbounded work.

The ticket also recorded that `<< INTCS_MIN` *"returns 0 promptly"*. That is true for a **positive**
value only. .NET special-cases `int.MinValue` as a right shift, and `BigInteger`'s `>>` is
**arithmetic**, so a negative value converges on **−1**, not 0. **My first version of the test
asserted 0 for both signs and the port was right.**

## 5. Question 4 — one half was a real divergence, in a place the ticket's probe could not see

The ticket measured *"`Vector3::Min` and `Max` BOTH return `(NaN,1,1)` for a NaN operand"* — with
the NaN in the **first** operand. With it in the second, the port **discarded** it:

```cpp
Vector3::Max({0, 1, 1}, {NaN, 1, 1})   // was {0, 1, 1} — the NaN vanished
```

because `std::max(a, b)` is `a < b ? b : a`, which is asymmetric under NaN. .NET's is

```csharp
ConditionalSelect(LessThan(y, x) | IsNaN(x) | (Equals(x, y) & IsNegative(y)), x, y)
```
*(`VectorMath.cs:1512-1524`, and its mirror for `Min` at `:1598-1610`.)*

Read it for each operand: `x` NaN selects `x`; `y` NaN makes all three disjuncts false and so
selects `y`. **NaN propagates from either side**, and the second row is the one that is easy to
miss, because nothing in .NET's expression mentions `IsNaN(y)`.

**A signed-zero rule came along with it.** The third disjunct is not symmetric — `Max` tests
`IsNegative(y)` and `Min` tests `IsNegative(x)` — so `Max(+0, -0)` is `+0` from either argument
order and `Min(+0, -0)` is `-0` from either. `std::max`/`std::min` return whichever operand they
were handed second, so they got this wrong too, and it is fixed by the same transcription.

`Clamp` with inverted bounds needed nothing: .NET is `Min(Max(value, min), max)` with the explicit
comment *"We must follow HLSL behavior in the case user specified min value is bigger than max
value"* (`Vector128.cs:422-426`), so `Clamp(5, 10, 0)` is `0` — and that is what this port already
produced.

## 6. To migrate

Geometry code that fed a NaN into `Min`/`Max` as the **second** argument and relied on it being
swallowed will now see the NaN propagate. That is the point: a NaN discarded is a wrong number
with no diagnostic, and #2175 made the same argument for `Normalize`.

## 7. Evidence

| Mutation | Caught |
|---|---|
| `LaneMax` back to `std::max` (a NaN in `y` is discarded) | ✅ (2 tests) |
| Drop the `IsNaN(x)` disjunct from `LaneMin` | ✅ |
| `LaneMin`'s signed-zero disjunct tests `y` instead of `x` | ✅ |
| `BigInteger::Parse` trims only the leading side | ✅ |
| `BigInteger::Parse` trims `std::isspace` instead of .NET's set | **equivalent in the "C" locale** |

The last is recorded rather than counted. In the `"C"` locale the two sets are identical
(`' '`, `\t`, `\n`, `\v`, `\f`, `\r`); they diverge only after a `std::setlocale` call, and a test
that changed the global locale inside a shared test binary would leak that change into every other
suite in it. The explicit set is chosen for locale-independence, not because a test distinguishes
it, and the comment at the site says exactly that.

## 8. Downstream

Neither `cna` nor `mobile-eggbert` references `System::Numerics` — **zero sites in both**, the same
measurement #2175 recorded. `cna`'s 46 `Normalize`/`Min`/`Max` call sites resolve to its own
`Microsoft::Xna::Framework` vectors in `cna/modules/math/`, which this change does not touch.
