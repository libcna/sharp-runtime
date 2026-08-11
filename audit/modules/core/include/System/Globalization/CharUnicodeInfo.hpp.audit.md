# Audit: `modules/core/include/System/Globalization/CharUnicodeInfo.hpp`

## Metadata

- AUDITED: 182-line inline Unicode-information implementation, fully read.
- Validation: `CharTests2.*` passed 63/63 on 2026-07-27.
- Reference/probe: local current-.NET 542-line `CharUnicodeInfo.cs` Unicode
  table implementation; C++20/managed probes compare Arabic digit U+0665,
  Roman numeral U+216B, fraction U+2153, combining acute U+0301, em dash
  U+2014, private-use U+E000, a high surrogate, and U+1F600 string input.

## SR-AUD-173 — medium — Unicode numeric APIs recognize only ASCII plus a few hard-coded Latin-1 characters

`GetDecimalDigitValue` accepts only ASCII `0` through `9`; `GetDigitValue`
adds only superscript 1/2/3; and `GetNumericValue` adds just those three plus
three Latin-1 vulgar fractions.  They otherwise return `-1` despite claiming
the corresponding Unicode numeric properties.

The C++ probe prints `decimal=-1 digit=-1 numeric=-1` for Arabic-Indic five
U+0665, Roman numeral twelve U+216B, and fraction one-third U+2153.  The
managed probe prints `5/5/5`, `-1/-1/12`, and `-1/-1/0.333333333`
respectively.  Current .NET derives all three operations from generated
Unicode data, including non-ASCII decimal, digit, and Numeric_Type values.

This is not documented as a limited character set and directly affects
`Char::GetNumericValue` and `Char::IsNumber` consumers.

## SR-AUD-174 — medium — BMP category classification depends on the C locale and labels common assigned code points as OtherNotAssigned

For code points outside the handful matched by `iswupper`, `iswlower`,
`iswdigit`, `iswspace`, `iswpunct`, `iswalpha`, or `iswcntrl`, the header
returns `OtherNotAssigned`.  No locale is installed, so the process starts in
the C wide-character locale; even where a host changes it, POSIX categories
are locale/version dependent and cannot supply the full 30-way Unicode
general-category table.

The C++ probe returns category 29 (`OtherNotAssigned`) for combining acute
U+0301, em dash U+2014, private-use U+E000, and a high surrogate.  The managed
probe returns 5 (`NonSpacingMark`), 19 (`DashPunctuation`), 17
(`PrivateUse`), and 16 (`Surrogate`).  It also returns 29 for U+1F600 in a
UTF-16 string where current .NET decodes the pair and returns 28
(`OtherSymbol`).

The header explicitly documents the non-BMP mapping as a native limitation;
that limited part is not separately classified.  The undocumented BMP
misclassification and locale-dependent result are enough to break the stated
`CharUnicodeInfo` counterpart contract and Char's category predicates.

## Assessment

ASCII category/control behavior, bound checks for the UTF-16 overloads, and
the enum ordinal mapping are coherent.  They do not provide the Unicode data,
surrogate decoding, or culture-independent classification that the public
numeric/category methods describe.

## Other missing assertions and diagnostics

- Direct tests contain only ASCII numeric/category cases; they omit all
  non-ASCII decimal/digit/numeric values, fractions, marks, private-use,
  punctuation subclasses, symbols, separators, surrogates, and invalid
  code-point bounds.
- No fixture fixes the C locale or repeats classification under a changed
  process locale, so its current output's dependency on ambient global state
  is invisible.
- The UTF-16 string overloads lack valid supplementary-pair, lone-surrogate,
  null/empty, and end-of-pair index diagnostics.

## Final assessment

The header has two confirmed Unicode-data/category defects (SR-AUD-173 and
SR-AUD-174).  No source or test was modified.


## SR-AUD-174 review, 2026-08-11 (#2314) — still confirmed, split by data dependency

`docs/CoreUnicodeCategoryTablePlan.md`. Re-verified against live source: the
header is 182 lines and contains **no Unicode data**; the `isw*` ladder can name
at most **8 of `UnicodeCategory`'s 30** values, so every mark, symbol,
non-`Other` punctuation class, `PrivateUse`, `Surrogate` and `Format` code point
is reported `OtherNotAssigned`.

**Ownership premise corrected.** SR-AUD-174 was believed to have no ticket
reference at all. It has one — #1766 names it as the compressed run
`SR-AUD-173/174`, which a `LIKE '%SR-AUD-174%'` scan cannot see. #1766 is the
audit that discovered it, not an owner, so the finding was indeed unowned; but
the same search defect was hiding others, and expanding those runs is what
produced #2317.

**Not wholly a table port.** Two clauses need no Unicode data and are #2316:

* **Locale dependence.** `isw*` are locale-sensitive, and this repository itself
  installs a non-invariant global locale under an RAII guard in
  `CultureInvariantFormattingTests`. On a host where those locales exist, the
  same source returns different categories for the same code point — a
  reproducibility defect independent of any Unicode version, and undocumented:
  the header states only the non-BMP limitation. That the suite *skips* here
  (one of the gate's two skips) confirms this container has no locales
  installed, exactly as the finding assumes.
* **Surrogates.** `Char::IsSurrogate` is `c >= 0xD800u && c <= 0xDFFFu`
  (`Char.hpp:245`), so this port already asserts in-repo that those code points
  are surrogates, while `GetUnicodeCategory` answers `OtherNotAssigned` — *not
  assigned* — for the same input. **The header contradicts itself**, provably,
  with no external reference; and this report's own managed probe already
  records the expected `16 (Surrogate)`.

**Evidence boundary, carried into #2316.** The same probe records
`17 (PrivateUse)` for U+E000, but that is **one code point**, and no private-use
range constant exists anywhere in this repository — unlike the surrogate range,
which `Char.hpp` fixes. Extending U+E000 to a range would be an inference from
the encoding architecture, not a repository fact, and must be treated as one.

**The table clause is #2315 (`blocked`), and its decision is already open.**
There is no Unicode character database here — no `UnicodeData.txt`, no
`DerivedGeneralCategory.txt`, no generated table, no generator;
`System/Text/Unicode/UnicodeRanges.hpp` is named **block** data and cannot
answer category questions. The approval required is already worded as
**Approval F / ticket #2018** (`blocked`) for `System::Text::Rune`: a generated
Unicode category table *including its data source, its attribution, and a
stated Unicode version with a policy for updating it*. `CharUnicodeInfo` is a
second consumer of that one decision, so it is gated on it rather than asked
again. **No Unicode data was hand-authored**, for the reason #2018 records: a
partial or remembered table is worse than a declared reduction, because it is
wrong where no test looks. SR-AUD-173, the sibling numeric finding in this
report, shares the same dependency and is claimed by #2317, not by this review.
