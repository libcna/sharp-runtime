<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# SR-AUD-133 — `TryWriteInterpolatedStringHandler` formatting semantics

Design record for review ticket **#2303** and implementation tickets **#2304**
(SR-AUD-133) and **#2305** (a separate defect the review found in the same
member, carrying an ordinary ticket number and **no** `SR-AUD-*` identifier —
the audit numbering stays frozen at 364).

## 0. Where the evidence lives

`build-probe/` is covered by `.gitignore`, so **no probe source or log in it is
tracked**, and a later reader cannot open one. Ticket #2284's closing commit
`c1e2c78` made exactly this correction for SR-AUD-018. Every measurement this
document relies on is therefore **transcribed into it** — the before/after
tables in §3, §6, §7 and §11 are the durable record, and the probe files are
disposable working artefacts, named below only so this batch's own run is
traceable.

The measurements are reproducible without those files. Five probes were built
with `g++ -std=c++23 -Imodules/core/include <probe>.cpp build/libsharp_runtime_core.a`
(two of them additionally with `-g -fsanitize=address,undefined`):

| Probe | What it measures |
|---|---|
| `2303_probe1_before` | the handler's output beside each sibling `ToString` for the same input |
| `2303_probe2_matrix` | the 75-cell before/after matrix of §6, printed as `KEY = VALUE` so the two runs diff mechanically |
| `2303_probe3_nullptr` | `AppendFormatted((const char*)nullptr)`, under ASan + UBSan (§7) |
| `2304_probe4_emptyspec` | `ToString(v)` against `ToString(v, "")` for all eleven wrappers (§11, M10) |
| `2304_probe5_arraybounds` | a non-NUL-terminated `char[3]` through the array arm, under ASan (§11, M5) |

---

## 1. The frozen finding, and what it did and did not say

> **SR-AUD-133 — medium — AppendFormatted ignores format and replaces .NET
> formatting with hardcoded C++ spellings.** The explicit format overload
> discards its format string, while the unformatted route routes all arithmetic
> through `std::to_string` and every unsupported type to `"[?]"`; it never
> performs the documented IFormattable fallback. The standalone probe therefore
> emits `1` for `true`, `255` for `255` with `"X2"`, and `3.140000` for `3.14`.

The finding is **one root cause with several observable subcases**, not a
conjunction of independent defects. The single cause is that `formatValue<T>`
was a hand-written type→text map that did not consult this repository's own
formatters and had no parameter for the specifier at all. Everything below
follows from that one fact, and one delegation change closes all of it.

Its three named examples are all real and all reproduced. They are **not**
the whole surface. The bounded matrix in `build-probe/2303_probe2_matrix.cpp`
(75 cells over ten categories) found four further consequences of the same
cause that the finding does not mention, listed in §3.

## 2. Production doors

`TryWriteInterpolatedStringHandler` has exactly two formatting entry points,
both function templates, and both funnel into one private helper:

| Door | Before | After |
|---|---|---|
| `AppendFormatted(const T&)` | `AppendLiteral(formatValue(value))` | `AppendLiteral(formatValue(value, {}))` |
| `AppendFormatted(const T&, const std::string& format)` | discarded `format`, called the overload above | `AppendLiteral(formatValue(value, format))` |
| `formatValue<T>` (private) | five-arm `if constexpr` map, no specifier parameter | delegating map, specifier threaded through |

`AppendLiteral`'s two overloads are **not** formatting doors and are untouched;
they were SR-AUD-132's subject and were closed by #1810.

**Consumer surface: there is none.** Searched repository-wide, the only files
that name the type are the header itself, its own test file, and three planning
documents (`NEXT.md`, `plan.md`, `build-tmp/*.md`). No production translation
unit in any module includes it. The incremental build confirms this
independently: changing the header recompiled exactly one object file, its own
test. Every output transition below therefore has **zero first-party
production consumers** to break.

## 3. Before-state, measured

`build-probe/2303_probe2_BEFORE.log`, against the unchanged tree.

### 3.1 The finding's own three examples — all confirmed

| Input | Before | This repository's own answer |
|---|---|---|
| `true` | `'1'` | `Boolean::ToString(true)` = `'True'` |
| `255` with `"X2"` | `'255'` | `Int32::ToString(255,"X2")` = `'FF'` |
| `3.14` | `'3.140000'` | `Double::ToString(3.14)` = `'3.14'` |

### 3.2 Four consequences of the same cause that the finding does not name

1. **A string literal lost its value entirely.** `AppendFormatted("lit")`
   emitted `'[?]'`. `T` deduces to `char[4]` for a literal, which never matched
   the `const char*` arm, so the most ordinary interpolation argument imaginable
   fell into the placeholder branch. This is the most severe cell in the matrix:
   the other divergences render the value *differently*, this one **discards
   it**. `std::string_view` and any `char[N]` did the same.
2. **`float` is affected as well as `double`.** `2.5f` emitted `'2.500000'`.
   The finding names only `3.14`.
3. **Every unrecognised or malformed specifier was silently accepted.** `"Q"`,
   `"Dz"` and `"Fz"` all produced the unformatted text, where the sibling
   wrappers raise `FormatException("Format specifier was invalid.")`. That is
   the exact fallback CCF-006 (#1847/#1849) removed from all twelve numeric
   wrappers; this handler still had it.
4. **The advertised `IFormattable` fallback did not merely fail to honour the
   format — it produced `"[?]"`.** A real `System::IFormattable` implementer,
   whose whole purpose is to state its own text, was rendered as the
   placeholder. The class doc-comment said "falls back to the value's
   `ToString()` if it inherits from `IFormattable`", and that sentence was
   false.

### 3.3 What was already correct, and stays correct

`int`, `long long`, all other integral widths, `char`, `std::string` and a
`const char*` **lvalue** all produced correct text before and produce
byte-identical text after (§6, section A/B of the diff).

## 4. Formatting authority — nothing here is invented

`/rv` is absent, so .NET's own source cannot be consulted. It is not needed:
**this repository already ships a public, documented, tested formatter for every
category the handler supports**, in the same module and the same component
(`Core.Base`), and several of them were themselves brought to their current
behaviour by approved remediation.

| Category | Authority | Provenance |
|---|---|---|
| `bool` | `System::Boolean::ToString(bool)` | pre-existing, `"True"`/`"False"` |
| signed integral by width | `SByte`/`Int16`/`Int32`/`Int64::ToString` | `B`/`b` branch added by #1845 (SR-AUD-023); unknown-specifier rejection by #1847 (SR-AUD-021, CCF-006) |
| unsigned integral by width | `Byte`/`UInt16`/`UInt32`/`UInt64::ToString` | same |
| `float` / `double` | `Single`/`Double::ToString` | #1849 (SR-AUD-021, CCF-006); `E`/`N`/`G` text set under the approved #1854/#1865 batch recorded in `CLAUDE.md` rule 2 |
| a type with `ToString(format)` | the value itself | `System::IFormattable`'s sole pure virtual |
| a type with `ToString()` | the value itself | `System::Object::ToString` |
| text and `char` | the characters themselves | — |

Because every arm delegates, **no format grammar is written in this header and
no text is chosen here**. Each emitted string is, by construction, a string some
already-tested sibling API answers for the same input. The repair cannot invent
a spelling, and it cannot drift from the sibling API later either.

**No new formatting machinery was added.** `System::detail::runCompositeFormat`
was evaluated and correctly rejected: it parses a *composite format string*
(`"{0:X2}"`) into items, which is a problem this handler does not have — the
handler receives the value and the specifier as separate arguments, already
split. Its per-argument renderers (`fmtInt`/`fmtDouble` in `String.cpp`) live in
an anonymous namespace inside a `.cpp` and are not reachable from a header;
they are themselves near-duplicates of the wrapper `ToString` overloads this
change uses, so delegating to the wrappers reaches the same authority without
promoting a private helper to public API.

## 5. Two boundaries deliberately left unchanged rather than guessed

Both are documented in the header at the point of the decision, and both are
pinned by the matrix as **unchanged** cells.

- **`long double`** keeps `std::to_string`, so it still emits `'3.140000'`.
  This repository has no extended-precision formatter; routing it through
  `Double::ToString` would silently narrow the value. It is the one arithmetic
  arm whose text did not change.
- **`char16_t`** stays on the integral path and keeps emitting a number, even
  though `SharpRuntime::charcs` is `char16_t`. Whether that door means "a
  character" or "a 16-bit integer" is not answerable from this type's own
  contract, and either answer would be a guess.

A type with no `ToString` at all still yields `"[?]"` — narrowed in reach, not
removed.

These are an explicit, coherent boundary, not a patchwork: every arithmetic and
text category the repository has a formatter for is delegated; the two it has no
answer for keep exactly the behaviour they had, and say so.

## 6. Output transitions, classified

From `diff build-probe/2303_probe2_BEFORE.log build-probe/2303_probe2_AFTER.log`.
Of 75 matrix cells, **35 changed and 40 are byte-identical**.

| Class | Cells | Examples |
|---|---|---|
| **previously wrong → corrected** | 10 | `true`→`True`, `3.14`→`3.14`, `2.5f`→`2.5`, `1.0`→`1` |
| **value was discarded → value now emitted** | 6 | `"lit"`, `""`, a `char[N]`, a `string_view`, all from `'[?]'` |
| **format previously ignored → now honoured** | 14 | `255:"X2"`→`FF`, `42:"B8"`→`00101010`, `1234.5:"N2"`→`1,234.50` |
| **newly rejected input** | 5 | `42:"Q"`, `42:"Dz"`, `3.14:"Fz"`, `3.14:"Q"`, `2.5f:"Q"` → `FormatException` |
| **previously aborted the process → now throws** | 1 | `AppendFormatted((const char*)nullptr)` (#2305, §7) |
| **unchanged** | 40 | every integer, `char`, `std::string`, a `const char*` lvalue, `long double`, `char16_t`, `"[?]"` for a type with nothing to ask, **all capacity/failure behaviour, and all layout/trait facts** |

**No error message changed** — the two exception texts are the sibling APIs'
own, unmodified. **No accepted input became unaccepted at compile time**: §9.

The five newly-rejected specifiers are the only transition that turns a
succeeding call into a throw. It is inherited rather than invented: delegating
to the numeric wrappers necessarily delegates CCF-006's settled validation
policy with them. It is safe to add to a type whose `bool` result means "did it
fit" for the same reason #1810's `ArgumentNullException` was: a malformed
specifier is a caller bug, not a capacity outcome, and the two must not share
one channel. The throw happens while producing the text, **before** any byte
reaches `appendRaw`, so a rejected specifier leaves `pos_` and `success_`
exactly as they were and the handler stays usable — pinned by
`RejectedSpecifierLeavesTheHandlerUntouched`.

### 6.1 First-party dependence on the old text

One test pinned a defect and one was named for it:

- `AppendFormatted_Bool` asserted `"1"`. Replaced by
  `AppendFormatted_Bool_UsesBooleanToString`, asserting `"True"`.
- `AppendFormatted_WithFormat_Ignored` asserted only that the result was
  non-empty, which held equally well whether or not the format was honoured, so
  it could never have detected this repair. Replaced by
  `AppendFormatted_WithFormat_IsHonoured`, asserting `"FF"`.

Nothing else in the repository depended on any of the changed text.

## 7. #2305 — a separate defect in the same member

Found while rewriting the `const char*` arm, disclosed rather than filed under
noise, and given an ordinary ticket number.

`AppendLiteral(const char*)` has rejected a null since #1810.
`AppendFormatted(const char*)` reached `return std::string(v);` instead, and
`std::string(const char*)` has a NUL-terminated-array precondition that a null
violates. Measured on the unchanged tree
(`build-probe/2303_probe3_nullptr_BEFORE.log`): libstdc++ diagnoses the
precondition itself and throws `std::logic_error` — *"basic_string: construction
from null is not valid"* — which escapes this `System`-shaped public API with
nothing to catch it, so **the process aborts, exit 134**.

Worth recording precisely: **ASan and UBSan report nothing** here, because the
standard library turns the undefined behaviour into a throw before any bad
access happens. The sanitizers were still the discriminating evidence — they
established that the mechanism is *not* a memory error but an escaping
non-`System` exception, which is the shape CCF-012 already named as a defect
class in this repository.

The fix is the policy #1810 settled for the sibling door, with the same
exception type and the same parameter name: `ArgumentNullException("value")`.
After: exit 0, *"Value cannot be null. (Parameter 'value')"*.

## 8. Cross-cutting disposition — adjacency, not membership

**Not a CCF-012 member.** CCF-012 is "hand-written composite-format replacement
is not a format parser", and its independent re-enumeration on 2026-08-04
concluded that **exactly two composite-format grammar implementations exist**,
`System::detail::runCompositeFormat` and
`System::Text::CompositeFormat::countPlaceholders`. This handler contains no
brace grammar at all — it never sees a composite format string, because the
value and the specifier arrive as separate arguments. It is therefore not a
third implementation of that grammar and does not join the family. CCF-012
**remains open**, closable only by #2020, and is unchanged by this work.

**Not a CCF-006 member.** CCF-006 — "numeric format validation is not normalized
at the public API boundary" — is closed, and its enumerated membership is the
twelve numeric wrappers. This handler is not a numeric wrapper. What it shared
was CCF-006's *shape*: an unrecognised specifier silently treated as a default
request. That shape is now gone here too, but by delegation to the closed
cause's own remediated members rather than by re-opening or extending it.

**Adjacent to both; a member of neither. No `CCF-*` identifier is minted or
extended**, and none is needed: the repair required no new policy, only the
application of two policies that already exist. CCF-011 stays closed, CCF-019
stays open and unextended, CCF-021 and CCF-022 stay unminted.

## 9. API, ABI and dependency consequences

| Property | Effect |
|---|---|
| Public signatures | **unchanged** — both `AppendFormatted` templates keep their exact parameter lists; the second's `format` parameter simply stops being ignored |
| Overload set | **unchanged** — no overload added or removed |
| Template constraints | **unchanged** — neither template gained a constraint, so the set of types that compile is the same |
| `sizeof` / `alignof` | **32 / 8, unchanged** (matrix section J) |
| Member layout, vtable | **unchanged** — no data member touched; the class is not polymorphic, before or after |
| `noexcept` | **unchanged** — no member was or is `noexcept` |
| Exported symbols | **unchanged** — header-only, all templates and `static` helpers; nothing gains linkage |
| Module dependencies | **unchanged** — every header now included is `modules/core/include/System/`, i.e. the same `Core.Base` component. No `PUBLIC_`/`PRIVATE_`/`TEST_DEPENDENCIES` edge, no module-graph edge and no catalogue entry changes |
| Header dependency exposure | **changed, and disclosed.** The header now pulls in eleven sibling wrappers, and through `Int64.hpp` and `Double.hpp` transitively `Int128.hpp`/`Math.hpp`, which use `__int128`. That makes this header MSVC-unsupported on the same terms `CLAUDE.md` already records for `Decimal`/`Int128`/`UInt128` — a compiler-extension dependency, not a platform bug, in a repository that does not target MSVC as a first-class compiler. Any consumer of `Int64.hpp` or `Double.hpp` already had this property, and this header has no production consumers at all |

`formatValue` is `static` and private; `viaWrapper` is a new `static` private
helper. The two new concepts live in `System::detail` under
deliberately-prefixed names (`InterpolatedFormattableWithSpec`,
`InterpolatedHasToString`) so they cannot collide with that namespace's existing
inhabitants.

**Compile-domain behaviour is unchanged.** The concepts are used only inside
`if constexpr`, never as constraints, so no call that compiled before fails to
compile now. An integral type of a width other than 1/2/4/8 — `__int128` is the
only one GCC offers — reaches the same `std::to_string` fallback it reached
before, and is ill-formed for it now exactly as it was then.

## 10. Tests

`modules/core/tests/System/TryWriteInterpolatedStringHandlerTests.cpp`:
**25 → 56 tests**, **+31 net** (31 added, and 2 of the pre-existing 25 replaced in
place — §6.1 — which is why the count rises by the number added). The repository
gate moves by exactly the same +31, from 16,941 to 16,972, so no other suite's
count changed.

One test per production door and per boundary: `Boolean::ToString` for both
values; the format ignored for `bool`; `Double`/`Single` unformatted and
formatted, including the `N` group separator; every integer format the wrappers
accept, including `B`; **one assertion per integral width**; unformatted integer
text explicitly asserted unchanged; the string-literal, non-NUL-terminated
array, embedded-NUL array and `string_view` doors; the format ignored for text;
#2305's null pointer, its parameter name, and a non-null control; `IFormattable`
formatted and unformatted; a `System::Object` descendant; a type with no
`ToString` still reaching `"[?]"`; empty format equal to the unformatted
overload; four unrecognised-specifier rejections; the untouched-handler
guarantee after a rejection; and capacity behaviour re-measured against the new,
longer text.

Assertions state literal expected text as the primary claim. Several *also*
cross-check against the sibling formatter, but never as the only assertion, so a
defect shared by handler and sibling cannot pass unseen.

## 11. Mutations

Nine mutations, each editing real source, rebuilding, and re-running the suite.
Eight valid, **eight caught**; one equivalent, proved so rather than assumed.
None discarded as non-compiling.

| # | Mutation | Result |
|---|---|---|
| M1 | `bool` arm back to `std::to_string` | **caught** — 4 tests |
| M2 | `viaWrapper` discards the specifier (the original defect) | **caught** — 12 tests |
| M3 | `float` routed to `Double::ToString` | **caught** — 1 test |
| M4 | char-array arm removed (literals fall back to `"[?]"`) | **caught** — 4 tests |
| M5 | char-array arm rewritten as `std::string(v)` | **caught by ASan** — `stack-buffer-overflow` READ in `strlen`; the shipped form is clean on the same input and build |
| M6 | null guard removed from the char-pointer arm | **caught** — 2 tests |
| M7 | `IFormattable` arm removed | **caught** — 2 tests |
| M9 | 32-bit signed routed to the `Int64` wrapper | **caught** — 1 test |
| M10 | empty-specifier short circuit dropped in `viaWrapper` | **equivalent** |

M3 and M9 initially survived and are the reason two tests are stronger than they
would otherwise have been: `2.5` is exact in both binary precisions and reads
identically either way, so the float test now uses `0.1f`; and every unformatted
integer reads the same at any width, so the width test now pins `-1` in
hexadecimal, which is eight nibbles through `Int32` and sixteen through `Int64`.

M10 is labelled equivalent on evidence, not assumption:
`build-probe/2304_probe4_emptyspec.log` shows `ToString(v) == ToString(v, "")`
for all **eleven** wrappers over 16 subjects. The short circuit is defence
against a future wrapper reading an empty specifier differently, and the header
says so at the point it is written.

## 12. Sanitizers

Run only where they discriminate, not ceremonially.

- **M5** (§11): ASan is the *only* thing that separates the bounded array read
  from `std::string(v)`, since both answer `"abc"` for a NUL-terminated array.
  It reported `stack-buffer-overflow` on the mutated form and nothing on the
  shipped one.
- **#2305** (§7): ASan + UBSan were run on the null-pointer door before and
  after. Their **silence** was the informative result — it established that the
  abort is an escaping `std::logic_error`, not a memory error.

No sanitizer was run over the formatting arms themselves. They allocate and
copy nothing the pre-existing `AppendLiteral` path did not already allocate and
copy, and the capacity arithmetic they feed is unchanged and was closed by
#1810.

## 13. Status

SR-AUD-133 → **remediated** (#2304). Every category the finding names, and the
four further consequences of the same cause that it does not, are closed against
this repository's own formatters, with the two boundaries of §5 documented as
deliberately unchanged rather than guessed. No part of the finding is left
evidence-blocked, so no deferred ticket is opened for it.

#2305 → **done**, no `SR-AUD-*` identifier, numbering still frozen at 364.
