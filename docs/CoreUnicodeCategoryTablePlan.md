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

---

# Part II — #2316 implementation record (the two data-free clauses)

Ticket **#2316**, implemented 2026-08-11. SR-AUD-174 stays **`confirmed`**: this closes
its locale clause and its surrogate clause, and leaves its table clause open in **#2315**,
blocked on Approval F / **#2018**. Nothing below uses, downloads, generates, infers or
hand-writes Unicode character-database content.

## 8. Before-state, measured

Two probes over the **whole** code space U+0000–U+10FFFF (1,114,112 code points), linked
against the live tree (`build-probe/2316_probe1_locale_surrogate.cpp`,
`build-probe/2316_probe2_points.cpp`).

### 8.1 The locale clause reproduces *in this container* — a premise correction

Part I §4.1 and #2316's own description say this container has **no locales installed**, so
the locale dependence could only be demonstrated "on a host where those locales exist".
**That is wrong.** `locale -a` lists three: `C`, `POSIX` and **`C.utf8`**. `C.utf8` is a
non-invariant locale for wide-character classification, and installing it through the
repository's own mechanism — `std::locale::global(std::locale("C.utf8"))`, exactly what
`ScopedGlobalLocale` does — changed the category of

> **287,218 of 1,114,112 code points (25.8%)**, first at U+0080.

`CultureInvariantFormattingTests` skips here not because no locale exists but because it
probes exactly four names — `en_US.utf8`, `en_US.UTF-8`, `de_DE.utf8`, `de_DE.UTF-8` — and
never probes `C.utf8`. The inference "it skips, therefore nothing is installed" does not
hold. The locale clause was therefore directly measurable, not hypothetical.

Every one of the 287,218 moves **out of** `OtherNotAssigned` into a positive category (the
`OtherNotAssigned` census falls by exactly 287,218 and no code point moves the other way):
148,166 → `OtherPunctuation`, 134,531 → `OtherLetter`, 2,518 → `LowercaseLetter`, 1,956 →
`UppercaseLetter`, 32 → `Control`, 15 → `SpaceSeparator`.

### 8.2 The ladder produced 7 categories here, not the 8 Part I estimated

Part I says the ladder "can name at most 8 of the enum's 30". Measured, the ceiling is 8 but
the **actual** count is locale-dependent too: in the `C` locale it produces **7** —
`OtherLetter` is unreachable, because every `iswalpha` character is already caught by
`iswupper` or `iswlower` — and under `C.utf8` it produces 8.

### 8.3 The C-locale answer is exactly reproducible without any locale facet

The `C` locale's `isw*` classification is fixed by the C standard over the basic execution
character set. Restating it as explicit ASCII ranges and comparing against the live
pre-change implementation over all 1,114,112 code points gives **0 mismatches**. This is
what makes the repair provably behaviour-preserving in the invariant locale rather than
merely intended to be.

### 8.4 The surrogate clause is locale-invariant

Under **both** `C` and `C.utf8`, all 2,048 code points U+D800–U+DFFF answered
`OtherNotAssigned`: 0/2048 non-default under `C.utf8`. So the two clauses do not interact,
and `Char::IsSurrogate` disagreed with `category == Surrogate` at exactly **2,048** code
points.

### 8.5 The documented non-BMP limitation was untrue on two axes — a second premise correction

The header's only stated limitation was "non-BMP code points are mapped to
OtherNotAssigned". Neither half held:

* **On a 32-bit-`wchar_t` target** (Linux; `WCHAR_MAX` = 2,147,483,647) supplementary code
  points were *not* substituted at all — they went straight into the `isw*` ladder, and
  under `C.utf8` most of them received a positive category.
* **On a 16-bit-`wchar_t` target** (Windows) `wc` became `L'\0'`, which is not `< 32` (the
  guard tests `codePoint`, not `wc`), fails every classification test, and then satisfies
  `iswcntrl(0)` — so every supplementary code point returned **`Control`**, not
  `OtherNotAssigned`. Checked by replaying the exact expression with `WCHAR_MAX` forced to
  `0xFFFF` (`build-tmp/2316_win_wchar.cpp`): U+10000, U+1F600 and U+10FFFF all → 14.

After the repair the documented behaviour is true for the first time, on every target.

## 9. What was implemented

One header, `modules/core/include/System/Globalization/CharUnicodeInfo.hpp`, inside
`GetUnicodeCategory(intcs)` and its doc-comments only.

1. **The `isw*` ladder is replaced by explicit ASCII ranges** — Control (U+0000–U+001F and
   U+007F), `A`–`Z`, `a`–`z`, `0`–`9`, U+0020, and the four punctuation runs — reproducing
   the `C`-locale answer exactly (§8.3) with no locale facet consulted.
2. **U+D800–U+DFFF returns `UnicodeCategory::Surrogate`**, the range taken from this port's
   own `Char::IsSurrogate` (`Char.hpp:245`) and the value from the frozen finding's own
   managed probe.
3. **`<climits>` and `<cwctype>` are dropped** — they existed only for `WCHAR_MAX` and the
   `isw*` calls. (`Char.hpp` includes `<cwctype>` itself, so its consumers are unaffected.)
4. **The doc-comments state the real contract**: the classification is locale-independent
   and is a *declared reduction* to ASCII plus the surrogate range, `OtherNotAssigned` is a
   statement about what this port knows rather than a claim about Unicode, and the rest
   waits on a database, an attribution and a stated version.

### 9.1 Deliberately not done

* **ASCII punctuation subcategories.** `$` is `CurrencySymbol`, `+`/`<`/`=`/`>`/`|`/`~` are
  `MathSymbol`, `(`/`[`/`{` are `OpenPunctuation`, `-` is `DashPunctuation`, `_` is
  `ConnectorPunctuation` and `^`/`` ` `` are `ModifierSymbol` in .NET; all 32 stay
  `OtherPunctuation` here. Correcting them is table work with a behaviour change in the
  default locale, i.e. #2315's clause, not #2316's — and the reduction is now documented
  rather than silent.
* **`PrivateUse` for U+E000.** The probe evidence is one code point; no private-use *range*
  is fixed anywhere in this repository, unlike the surrogate range. Part I §4.2's evidence
  boundary is carried into a test that asserts U+E000 is *not* `PrivateUse`.
* **SR-AUD-173.** The numeric methods are untouched.

## 10. After-state, measured

Genuine before/after: the pre-change header was extracted with
`git show HEAD:…/CharUnicodeInfo.hpp` into `build-probe/2316_before/`, one probe
(`build-probe/2316_probe3_sweep.cpp`) was compiled against each tree, and both dumps —
1,114,112 lines each — were diffed, under the `C` locale and under `C.utf8`.

| Measurement | Before | After |
|---|---|---|
| Code points whose category changes when the global locale goes `C` → `C.utf8` | **287,218** | **0** |
| Code points where `Char::IsSurrogate` ≠ (`category == Surrogate`) | **2,048** | **0** |
| Category changes in the invariant locale, before → after | — | **2,048**, all `OtherNotAssigned` → `Surrogate`, exactly U+D800–U+DFFF |
| `Char::IsNumber`/`IsSeparator`/`IsControl`/`IsSurrogate` answers changed over U+0000–U+FFFF, invariant locale | — | **0** |

**In the invariant locale — the container default, and tracked CI — the only observable
change in the entire code space is the 2,048 surrogates.** Nothing else moved.

### 10.1 The price, stated exactly

On a host that installs a UTF-8 global locale, 287,218 categories are **withdrawn** to
`OtherNotAssigned` (289,266 total changes = 287,218 withdrawn + 2,048 surrogates). Those
answers were unreproducible across hosts and mostly wrong: of the frozen finding's own
non-ASCII probe rows, `C.utf8` got U+0301 as `OtherPunctuation` (Unicode: `NonSpacingMark`),
U+2014 as `OtherPunctuation` (`DashPunctuation`), U+E000 as `OtherPunctuation`
(`PrivateUse`) and U+1F600 as `OtherPunctuation` (`OtherSymbol`) — four confident, wrong,
positive claims where `OtherNotAssigned` is at least an honest one. Only U+00C9 was right
(`UppercaseLetter`), and it is among the 4,474 letter classifications withdrawn.

**One first-party consequence, on such a host only:** `Char::IsSeparator` returns `false`
where it returned `true` for 15 code points — U+1680, U+2000–U+2006, U+2008–U+200A, U+2028,
U+2029, U+205F, U+3000. (`Char::IsSeparator` short-circuits below U+0100, so ASCII and
Latin-1 are untouched.) Those are genuinely separators in Unicode; answering them correctly
and *reproducibly* is #2315's table, not a locale accident. No other `Char` predicate moves
on any host.

## 11. Compatibility

No public signature, no object layout, no vtable, no `noexcept` specification and no
exported symbol changes: `CharUnicodeInfo` is a `static`-only class with a deleted
constructor and every member is an inline function template-free body in the header. The
out-of-range `ArgumentOutOfRangeException("codePoint")` guard, its `paramName` and its
position (first) are untouched, as are all three other overloads' bodies and both numeric
method families. The rebuild is broad — the header is reached transitively through
`Char.hpp` — but the change is confined to one function body and its comments.

## 12. Why SR-AUD-174 stays open

Its table clause is untouched and unblocked only by Approval F / #2018 (#2315, `blocked`).
The finding's own probe rows for U+0301, U+2014, U+E000 and U+1F600 still diverge from .NET
and are now *pinned* as the declared reduction, so the gap is permanently visible instead of
resting on ambient locale state. The audit index records partial closure; the status stays
`confirmed`.
