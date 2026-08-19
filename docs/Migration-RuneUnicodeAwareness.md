<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Rune`'s six classification and casing members are Unicode-aware (ticket #2018)

*2026-08-19.* `System::Text::Rune::IsLetter`, `IsDigit`, `IsLetterOrDigit`, `IsUpper`, `IsLower`,
`ToUpper` and `ToLower` were ASCII-only while `IsWhiteSpace` was Unicode-aware — the
self-contradiction SR-AUD-294 rests on. All of them now answer from the generated UCD 16.0 tables.

Landed under **SA-4**, third in its stated unlock order (#2315 → #2336 → **#2018** → #2338). No
signature, layout or vtable change; `sizeof(Rune)` is unchanged.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `IsLetter(U+00E9)` é | `false` | `true` |
| `IsLetter(U+03B1)` α | `false` | `true` |
| `IsDigit(U+0660)` ٠ | `false` | `true` |
| `IsUpper(U+00C9)` É | `false` | `true` |
| `IsLower(U+00E9)` é | `false` | `true` |
| `ToUpper(U+00E9)` | unchanged | `U+00C9` |
| `ToLower(U+03B1)`… `ToUpper` | unchanged | `U+0391` |
| `ToLower(U+10400)` (supplementary) | unchanged | `U+10428` |
| the ASCII range | — | **unchanged** |
| `IsWhiteSpace` | Unicode-aware | **unchanged** |

`IsLetter` is the **five** letter categories (`Lu Ll Lt Lm Lo`), not the cased two, so a titlecase
letter and a Hebrew alef are letters. `IsDigit` is `DecimalDigitNumber` **only**, so a Roman
numeral and a circled digit are not digits — .NET's distinction, and a mutation widening it to
"has a numeric value" is caught.

## 2. Simple case mapping only, and that is .NET's constraint too

`ToUpper`/`ToLower` are the **simple** mapping, one code point to one code point. `U+00DF` SHARP S
stays itself rather than becoming `"SS"`. That is not a shortcut: a full mapping can produce more
than one scalar and the return type is one `Rune`, which is why .NET's `Rune.ToUpperInvariant` has
the same limit.

The case tables are 16-bit **signed deltas** over the *same* trie as the category —
`CharUnicodeInfo` indexes all three with `GetCategoryCasingTableOffsetNoBoundsChecks` — so they
live in the same generated header. The plane is preserved by masking rather than added to, because
a 16-bit delta cannot cross a plane; a mutation dropping the mask is caught by the supplementary
Deseret rows.

## 3. The round trip is not a bijection, and the five exceptions have one explanation

Lowering every uppercase BMP scalar and raising it back lands where it started for all but
**five**, asserted as the exact set rather than a count — because the count would have hidden
this:

| | lowers to | raises back to |
|---|---|---|
| `U+03F4` GREEK CAPITAL THETA SYMBOL | `U+03B8` θ | `U+0398` GREEK CAPITAL LETTER THETA |
| `U+2126` OHM SIGN | `U+03C9` ω | `U+03A9` GREEK CAPITAL LETTER OMEGA |
| `U+212A` KELVIN SIGN | `U+006B` k | `U+004B` LATIN CAPITAL LETTER K |
| `U+212B` ANGSTROM SIGN | `U+00E5` å | `U+00C5` LATIN CAPITAL A WITH RING ABOVE |
| `U+1E9E` LATIN CAPITAL SHARP S | `U+00DF` ß | `U+00DF` — no single-scalar uppercase |

Four of the five are Unicode's own **duplicate letters**, which lowercase into the canonical letter
whose uppercase is the canonical capital rather than the duplicate. The fifth is SHARP S. None is a
defect in the table, and a repair that "fixed" the round trip would be disagreeing with the UCD.

## 4. Evidence

| Mutation | Caught |
|---|---|
| the plane-preserving mask dropped | yes |
| upper and lower tables swapped | yes |
| `IsLetter` narrowed to the cased categories | yes |
| `IsDigit` widened to any number category | yes |
| **the ASCII fast path removed** | **no — a proven equivalence** |

The last is reported rather than papered over. Measured over all 128 ASCII code points, the fast
path and the table agree on all six members — **0 disagreements**. It is kept because .NET's ASCII
branch and its category branch are two statements and only the second is a table lookup;
collapsing them would be a simplification of the reference rather than a port of it, and it is
exactly where a future divergence would slip in unnoticed. The comment at the site says so.

## 5. Downstream, measured

`cna` and `mobile-eggbert` reference `System::Text::Rune` in **zero** places. Neither was modified.
