<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `CharUnicodeInfo` BMP category classification (SR-AUD-174) — review record

Ticket #2314. Reviewed 2026-08-11. Reviewed in the same batch as SR-AUD-106
only because both were unowned; they share no cause and are not a family, and
no CCF is minted or extended.

---

## 1. Ownership before this review, and a correction to the premise

The inherited premise was "no ticket references SR-AUD-174 at all". That is
**not literally true**, and the reason is a search-method defect worth
recording: ticket **#1766** (the repository-wide evidence-only audit, `done`)
names it in a checkpoint note as

> "C++/managed probes confirm SR-AUD-173/174: numeric lookup covers only
> ASCII/a few Latin-1 values and C-locale categories misclassify ordinary
> assigned BMP characters."

A `LIKE '%SR-AUD-174%'` scan returns nothing, because the identifier is written
in the compressed run form `SR-AUD-173/174`. Expanding those runs is required
to inventory ownership correctly, and the same form appears elsewhere
(`SR-AUD-175/176`).

The substantive premise stands, though: #1766 is the audit that **discovered**
the finding, not an owner. After expansion, SR-AUD-174 is referenced by exactly
one ticket, and that ticket is the audit umbrella. **No review, design,
implementation, deferred or `needs_user` ticket owned it.**

---

## 2. The frozen finding, re-verified against live source

`modules/core/include/System/Globalization/CharUnicodeInfo.hpp` is 182 lines and
contains **no Unicode data at all**. `GetUnicodeCategory(intcs)` is a seven-test
POSIX ladder — `iswupper`, `iswlower`, `iswdigit`, `iswspace`, `iswpunct`,
`iswalpha`, `iswcntrl` — over a `wchar_t`, preceded by a `codePoint < 32`
control guard, falling through to `OtherNotAssigned`. It can therefore produce
at most **8 of the enum's 30** categories, and `UnicodeCategory.hpp` does
declare all 30. Every category the ladder cannot name — every mark, every
symbol, every non-`Other` punctuation class, every separator other than the one
`iswspace` finds, `PrivateUse`, `Surrogate`, `Format` — is reported as
`OtherNotAssigned`. The defect is present exactly as frozen.

Blast radius inside this repository: `Char::IsNumber` and `Char::IsSeparator`
consume the category directly (`Char.hpp:124`, `:141`), and
`Char::GetUnicodeCategory` forwards it (`:432`). Tests touching it live in
`CharTests2.cpp` and two `modules/globalization` suites; `CharTests2`'s category
coverage is `u'A'`, `u'z'`, `u'3'` and `"Ab"` — ASCII only, as the finding says.

---

## 3. Premise check: is this "a large Unicode/reference-data port"?

Mostly yes, and the repository has already said so about the same data.

**There is no Unicode character database in this repository.** A search over
the whole tree (excluding `vendor/`) finds no `UnicodeData.txt`, no
`DerivedGeneralCategory.txt`, no generated table and no generator.
`System/Text/Unicode/UnicodeRanges.hpp` is the .NET `UnicodeRanges` API — 163
*named block* ranges — which is block data, not general-category data, and
cannot answer this question.

**The decision this needs is already open, for a different type.**
`docs/SystemTextApprovalPackage.md` §7 and `docs/ConsolidatedApprovalPackage.md`
§T.8 carry **Approval F**, ticket **#2018** (`blocked`), asking to approve

> "implementing real Unicode category and simple case mapping in
> `System::Text::Rune` — including the generated table, its data source, its
> attribution, and a **stated Unicode version with a policy for updating it**".

Its recorded recommendation is **defer**, on the ground that "there is no
Unicode data source in this container, and a hand-written approximation is
worse than a declared reduction."

SR-AUD-174 is the **same root dependency in a second type**: `CharUnicodeInfo`
needs the general-category table that #2018's `Rune` members need. It is
therefore not a new decision to put to the user — it is a second consumer of an
undecided one, and it should be gated on the same answer rather than asked
again. Ticket **#2315** records it as blocked on Approval F.

---

## 4. Two clauses that do **not** need Unicode data

The finding is a conjunction, and not all of it is table-dependent.

### 4.1 The locale clause is a separate, table-independent defect

The categories returned depend on the process's ambient C locale, because
`isw*` are locale-sensitive. Two facts make this checkable here:

* This container has **no locales installed** — `CultureInvariantFormattingTests`
  probes `en_US.utf8`, `en_US.UTF-8`, `de_DE.utf8`, `de_DE.UTF-8` and
  `GTEST_SKIP()`s when none is present, and it does skip: measured in this
  batch's gate as `CultureInvariantFormattingTests.NumericAndDateFormatting_Unaffected
  ByNonInvariantGlobalLocale`, one of the two skips (the other is
  `PingTests.NoOptionsDoors_ReportAbsentOptions`, unrelated). So the C locale is
  not a choice here, it is the only option, and the finding's "the process
  starts in the C wide-character locale" is confirmed from inside the
  repository.
* The global locale is nevertheless **mutable in-process and the repository
  already mutates it**: the same suite installs a non-invariant locale through
  `std::locale::global` under an RAII guard. On a host where those locales
  exist, a caller doing the same thing changes what
  `CharUnicodeInfo::GetUnicodeCategory` returns for the same code point.

So the result is not merely incomplete, it is **not reproducible across hosts**
— and that is true whatever Unicode version is eventually chosen. It is also
undocumented: the header's only stated limitation is the non-BMP one
("non-BMP code points are mapped to OtherNotAssigned"), which the finding
explicitly notes is the documented part.

### 4.2 One misclassification the repository already contradicts internally

`Char::IsSurrogate(c)` is `c >= 0xD800u && c <= 0xDFFFu` (`Char.hpp:245`), and
`Char::IsHighSurrogate`/`IsLowSurrogate` split the same range. So this header
already asserts, authoritatively and with no external reference, that
U+D800–U+DFFF are surrogates — while `GetUnicodeCategory` on the same code
point answers `OtherNotAssigned`, i.e. *not assigned*. **The header contradicts
itself**, and the frozen finding independently records the reference answer for
this exact input: its managed probe returns `16 (Surrogate)` for a high
surrogate.

The same probe records `17 (PrivateUse)` for U+E000. That evidence is one code
point, not a range: unlike the surrogate range, no private-use range constant
exists anywhere in this repository, so extending U+E000 to a full
`PrivateUse` range would be an inference from the encoding architecture rather
than a repository fact. The distinction is deliberate and must survive into
implementation.

The two remaining probe rows — U+0301 `NonSpacingMark`, U+2014
`DashPunctuation`, and the U+1F600 surrogate-pair row — are genuinely
table-dependent and belong to §3.

---

## 5. Disposition and tickets

| Clause | Ticket | State | Why |
|---|---|---|---|
| Full 30-way BMP general-category table | **#2315** | `blocked` | Needs a UCD source, an attribution and a stated Unicode version + update policy — the identical decision already pending as Approval F / #2018. Not asked again. |
| Locale dependence + the undocumented BMP reduction (§4.1) | **#2316** | `todo` | Needs no Unicode data. Documentation of the real limitation is zero-behaviour-change; a locale-independent ASCII ladder is a bounded follow-on. |
| Surrogate range misclassification (§4.2) | **#2316** | `todo` | In-repo evidence only: the range from `Char.hpp:245`, the expected value from the frozen finding's own managed probe. |

**Not implemented in this batch, deliberately.** #2316 edits
`CharUnicodeInfo.hpp`, a header reached transitively by `Char.hpp` and
therefore by most of the repository; the edit is small but its rebuild is
near-total, and it changes the observable classification of real code points.
That is a change to make deliberately, with its own before/after sweep and its
own gate, not as a tail-end addition to a triage batch. It is ranked as the
next unit of work rather than started here.

**Do not hand-author a partial table.** Both this record and #2018 reach the
same conclusion for the same reason: a partial or remembered table is worse
than a declared reduction, because it is wrong in places no test names.

## 6. Size estimate for #2315, if Approval F is ever granted

A general-category table for U+0000–U+10FFFF compresses to roughly two to three
thousand contiguous ranges, or a two-level trie in the tens of kilobytes; the
.NET equivalent is generated, not written. The work is a generator plus its
input plus a version pin plus a per-category sample suite naming that version —
i.e. mechanical **once the data source and version policy exist**, and
impossible before. `GetUnicodeCategory` is `static` and non-`noexcept`-marked
today, and a table changes no signature, no layout and no vtable; only results.

## 7. Frozen-identifier discipline

No `SR-AUD-*` identifier is created; numbering stays frozen at 364. SR-AUD-174
stays **`confirmed`** — its table clause is blocked and its two data-free
clauses are open in #2316. SR-AUD-173, the sibling numeric-value finding in the
same report, shares §3's root dependency and is **not** claimed by this review.
