<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `CharUnicodeInfo`'s three numeric queries answer the whole code space (ticket #2336)

*2026-08-19.* The sixteen-code-point reduction is **gone**. `GetDecimalDigitValue`,
`GetDigitValue` and `GetNumericValue` now read a generated Unicode 16.0 table, and
`System::Char::GetNumericValue` widens with them.

Landed under **SA-4**, second in its stated unlock order (#2315 → **#2336** → #2018 → #2338).
No signature, layout, vtable or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `GetDecimalDigitValue(U+0665)` ٥ | `-1` | `5` |
| `GetDigitValue(U+2460)` ① | `-1` | `1` |
| `GetNumericValue(U+216B)` Ⅻ | `-1.0` | `12.0` |
| `GetNumericValue(U+2153)` ⅓ | `-1.0` | `0.333…` |
| `GetNumericValue(U+0F33)` | `-1.0` | **`-0.5`** (§4) |
| `GetNumericValue(U+2189)` | `-1.0` | **`0.0`** (§4) |
| the original sixteen code points | — | **unchanged** |
| CJK ideographs | `-1.0` | **`-1.0`** — unchanged, and deliberately (§3) |

Over the BMP, **370** code points now have a decimal digit value, **465** a digit value and
**742** a numeric value, against 10, 13 and 16 before. Over the whole code space the figures are
760, 888 and 1,919.

## 2. Decimal and Digit are two properties, not one built on the other

The old `GetDigitValue` was *"the decimal value, else three hard-coded superscripts"*. .NET reads a
**different nibble of the same table byte** — the high nibble for `GetDecimalDigitValue`
(`CharUnicodeInfo.cs:151`) and the low one for `GetDigitValue` (`:185`), each minus one so that
absence normalises to `-1`.

The old shape agreed over the thirteen code points it covered and cannot in general: `Numeric_Type`
partitions into Decimal ⊂ Digit ⊂ Numeric, and a character can be Digit without being Decimal
(every circled and superscript digit) or Numeric without being either (`U+216B`). A mutation that
swaps the nibbles is caught, as is one that drops either minus-one.

## 3. What did *not* change, and why that is right

`GetNumericValue` of a CJK ideograph is still `-1.0`. Python's `unicodedata` answers `5.0` for
`U+3405`, and the difference is **not** an error in this table: those values live in the **Unihan**
database (`kPrimaryNumeric` and friends), which `UnicodeData.txt` field 8 does not carry and which
.NET's `CharUnicodeInfo` therefore does not either. Matching .NET is the goal; matching Python
would be a divergence.

Measured, **all 83** code points where Python has a numeric value and this table does not are CJK
ideographs, every one of general category `Lo`.

## 4. Two sentinel traps this widening creates

Both are new hazards, and both are asserted rather than assumed:

* **`-1` is a sentinel *and* a plausible value's neighbour.** `U+0F33` TIBETAN DIGIT HALF ZERO
  really is **`-0.5`** — the only negative numeric value in the BMP. A caller testing
  `value < 0` for "no value" is now wrong. `-1.0` itself is not a Unicode numeric value, so the
  sentinel survives — but only just.
* **`0.0` is a real value.** `U+2189` VULGAR FRACTION ZERO THIRDS is exactly zero, so a caller
  testing `value != 0` for "has a value" is wrong in the other direction.

The correct test is `!= -1.0`, which is .NET's own contract.

## 5. The cross-check SA-4 requires

Against Python 3.13.5 `unicodedata` (UCD 15.1.0), over all 1,114,112 code points:

| Property | Mismatches | Cause |
|---|---|---|
| decimal digit value | 80 | all Unicode 16.0 additions (the Garay digits `U+10D40`…) |
| digit value | 80 | the same 80 |
| numeric value | 163 | **80** the same 16.0 additions, **83** Unihan-only (§3) |

**There is no code point where this table and Python both claim a numeric value and disagree** —
every difference is a presence/absence difference with a named cause. That is a stronger statement
than a mismatch count, and it is the one worth recording.

## 6. A gap this ticket exposed but deliberately did not fix

The four `(string, index)` overloads index a code **unit** where .NET reads a code **point** —
.NET routes all four through `CharUnicodeInfo.GetCodePoint`, which combines a surrogate pair.

It was **invisible until now**: before #2315 every non-ASCII code point answered
`OtherNotAssigned` and before #2336 every one answered `-1`, so a supplementary character and a
lone surrogate gave the same answer. With the tables in they differ — 80 supplementary code points
carry a decimal digit value and 1,177 a numeric value at UCD 16.0.

It is **ticket #2385**, not part of this one: it is a different axis (index arithmetic, not data)
and it needs its own decision about an unpaired surrogate. The single-character `charcs` overloads
are correct as they stand, and .NET's `char` overloads have the same limit.

## 7. Evidence

Five mutations, **all caught**: nibbles swapped, each minus-one dropped, the level-1 shift wrong,
the level-3 mask widened. The last is caught by five *pre-existing* `Char` tests as well as the new
ones, which is what shows the table is load-bearing for the shipped API and not just for its own
suite.

The generator is `scripts/gen_unicode_tables.py` — renamed from
`gen_unicode_category_table.py`, now emitting one header per table from the one source of record,
because #2018 and #2338 will extend it rather than adding generators of their own. `--check`
regenerates and diffs both headers.

## 8. Downstream, measured

`cna` and `mobile-eggbert` reference `CharUnicodeInfo` in **zero** places. `System::Char::
GetNumericValue` widens with this change; neither consumer calls it either. Neither was modified.
