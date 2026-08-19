<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `CharUnicodeInfo`'s `(string, index)` overloads read a code point (ticket #2385)

*2026-08-19.* `GetUnicodeCategory`, `GetDecimalDigitValue`, `GetDigitValue` and `GetNumericValue`
taking a `std::u16string` and an index now combine a surrogate pair, as .NET's do. No signature,
layout or vtable change.

---

## 1. What changed

Only where the index lands on the **high half of a valid surrogate pair**:

| Call on `u"𝟎"` (U+1D7CE MATHEMATICAL BOLD DIGIT ZERO) | Was | Is |
|---|---|---|
| `GetUnicodeCategory(s, 0)` | `Surrogate` | `DecimalDigitNumber` |
| `GetDecimalDigitValue(s, 0)` | `-1` | `0` |
| `GetDigitValue(s, 0)` | `-1` | `0` |
| `GetNumericValue(s, 0)` | `-1.0` | `0.0` |
| the same at index **1** | `Surrogate` / `-1` | **unchanged** |
| every BMP index | — | **unchanged**, swept over all 65,536 |
| bounds checking | — | **unchanged** |

## 2. This was exposed by #2315 and #2336, not introduced by them

.NET routes all four through `CharUnicodeInfo.GetCodePoint` (`CharUnicodeInfo.cs:145, 179, 258,
395`); this port indexed a code **unit** and handed a lone surrogate to the single-character
overload. The divergence has always been there and was **unobservable**: while every non-ASCII
code point answered `OtherNotAssigned` and every one answered `-1`, a supplementary character and
a lone surrogate gave the same answer. With the tables in they differ — 80 supplementary code
points carry a decimal digit value and 1,177 a numeric value at UCD 16.0.

## 3. Three conditions, all of them .NET's

`GetCodePoint` (`CharUnicodeInfo.cs:436-465`) combines only when the unit at the index is a
**high** surrogate, the successor is in range, and the successor is a **low** surrogate.
Otherwise it returns the unit itself. So all of these answer `Surrogate`:

* a lone high surrogate, and a high surrogate at the end of the string;
* a high surrogate followed by a non-surrogate, or by another high surrogate;
* a **low** surrogate at the index — including the low half of a **valid pair**.

That last one is the easy mistake: looking backwards from the low half would be "helpful" and
.NET does not do it. Each of the six shapes has a case.

## 4. The single-character overloads are correct and out of scope

A `charcs` cannot hold a supplementary code point, so those overloads have no pair to combine —
and .NET's `char` overloads have exactly the same limit. They are untouched.

## 5. Evidence

Five mutations, four caught, one equivalence:

| Mutation | Result |
|---|---|
| the successor's low-surrogate check dropped | caught |
| the plane offset `+ 0x10000` dropped | caught |
| the shift `<< 10` becomes `<< 11` | caught |
| **a low surrogate at the index also combines** | caught **only after a row was added** |
| the successor's bounds check dropped | **equivalence — see below** |

The fourth is worth recording because it went uncaught first. With a low surrogate accepted at the
index, the only successor that would then combine is *another low surrogate*, and no case in the
file had that shape. A `{lo, lo}` row was added and the comment there says why it exists.

The fifth is a genuine equivalence, not a gap. `CheckIndex` has already established
`i < s.size()`, so `i + 1 <= s.size()`, and `std::basic_string::operator[](size())` is
**specified** to return a reference to a null character — defined behaviour, not a read past the
end — and a `u'\0'` successor makes the low-surrogate test underflow well clear of `0x3FF`. The
bound is kept because it is .NET's and because resting a surrogate scan on `operator[]`'s
null-terminator guarantee is a subtler contract than a bounds test, one that would silently become
wrong if this moved to a span. The note is at the site.

One #2315 pin (`SurrogateCategory_ReachedByEveryOverload`) had two rows inverted: it asserted that
both halves of a pair report `Surrogate`, which was true of code-unit indexing.

## 6. Downstream, measured

`cna` and `mobile-eggbert` reference `CharUnicodeInfo` in **zero** places. Neither was modified.
