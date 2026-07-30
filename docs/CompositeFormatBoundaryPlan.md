<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Composite-format boundary family — CCF-012 plan

Design record for ticket **#1881**. Written 2026-07-30 on branch
`feature/remediation-batch-ccf012-composite-format`, from the measured output of
`build-probe/1881_family_compare_probe.cpp` (`build-probe/1881_prefix.log`,
`build-probe/1881_ubsan_prefix.log`) and from the current .NET reference source
under `/rv/tmp/runtime/src/libraries/`.

This document does two things:

1. **Section 0** records the bounded, evidence-based comparison of **CCF-012**
   against **CCF-017** that selected this family, and why the **CCF-019**
   fallback was not taken. That comparison is durable evidence, not a preamble:
   the batch prompt requires the selection to be justified by measurement rather
   than by finding count or title.
2. **Sections 1–20** are the family plan proper.

Nothing in this document marks a finding remediated. SR-AUD-015 stays
`confirmed` until an implementation ticket closes it.

---

## 0. Family selection — measured comparison of CCF-012 and CCF-017

Both candidates were reproduced against the shipped headers before either was
chosen, in one probe, in one process tree, on 2026-07-30.

### 0.1 The two candidates side by side

| Dimension | **CCF-012** (composite format) | **CCF-017** (Attribute identity) |
|---|---|---|
| Confirmed findings | 1 (SR-AUD-015, `medium`) | 1 (SR-AUD-114, `medium`) |
| Owning modules | `Core.Base` (`modules/core`) | `Core.Base` (`modules/core`) |
| Owning files | `src/System/String.cpp`, `include/System/FormattableString.hpp` | `include/System/Attribute.hpp` (+5 derived headers) |
| Public entry points | **23** (22 `String::Format` overloads + `FormattableString::ToString`, reached by 22 further `StringBuilder::AppendFormat` / `Console::Write(Line)` wrappers) | 5 virtuals on one base, inherited by 13 attribute types |
| Severity as filed | medium | medium |
| **Severity as measured** | **non-termination + UB + silent corruption + escaped `std::` exception** | value-semantics divergence only |
| Reachable non-termination | **yes** — 2 of 2 self-referential cases hang | no |
| Reachable undefined behaviour | **yes** — UBSan-confirmed at `String.cpp:149` | no |
| Silent output corruption | **yes** — 4 measured cases | no |
| Non-`System::` exception escapes a public API | **yes** — 3 measured cases | no |
| Shared root cause | **yes, one** — output-mutating sequential replacement in both files | n/a (single site) |
| Reproduction status | all 8 audit claims reproduce, plus 4 classes the audit never named | all 4 audit claims reproduce exactly |
| Original premises still applicable | yes, and *understated* | yes, and complete |
| Compatible implementation available | **yes, and it is the structural repair itself** | **no** — see 0.3 |
| Depends on a blocked ticket | no | no |
| Test cost | ~40 permanent regressions across 2 suites | ~15, but only after an approval |
| Sanitizer cost | UBSan (arithmetic) + ASan/LSan (buffer/allocation on a rewritten parser) | none applicable |
| Incrementally completable | yes — two files, two tickets, independent | no — one indivisible decision |
| One structural fix closes multiple findings | yes — one parse model closes both halves of SR-AUD-015 | n/a |

### 0.2 What CCF-012 actually does, measured

`build-probe/1881_prefix.log`, every case run in a forked child under a 3-second
watchdog so a hanging case cannot hide the ones after it:

```
  Format("{0}", "{0}")           -> *** DID NOT TERMINATE within the watchdog ***
  Format("[{0}]", "x{0}y")       -> *** DID NOT TERMINATE within the watchdog ***
  Format("{0}{1}", "{1}", "X")   -> value   "XX"          (.NET: "{1}X")
  Format("{0}|{1}", "{1}", "Y")  -> value   "Y|Y"         (.NET: "{1}|Y")
  Format("{0:D99999999999}", 42) -> *** std::exception ESCAPED *** (St12out_of_range) "stoi"
  Format("{0:DX}", 42)           -> *** std::exception ESCAPED *** (St16invalid_argument) "stoi"
  Format("{0:F99999999999}", 1.5)-> *** std::exception ESCAPED *** (St12out_of_range) "stoi"
  Format("{0:D-2147483648}", 42) -> value   "42"          (with UB, see below)
```

and under `-fsanitize=undefined` (`build-probe/1881_ubsan_prefix.log`):

```
modules/core/src/System/String.cpp:149:41: runtime error: negation of -2147483648
    cannot be represented in type 'int'
```

Four consequences the audit did not name, all reachable from a **plain public
call with an ordinary format string**:

- **`String::Format` can fail to terminate.** `replaceArg` resets its scan
  cursor to `0` after each substitution (`String.cpp:122`), so it re-scans text
  it just inserted. An *argument value* containing its own placeholder therefore
  loops forever. This needs no malformed format string and no attacker-supplied
  format: `String::Format("{0}", userText)` hangs whenever `userText` contains
  `{0}`. That is a public-input denial of service in the single most widely
  called formatting entry point in the library.
- **Reachable signed-overflow UB**, `std::abs(std::stoi(...))` on `INT_MIN`.
- **Two `std::` exception types escape a `System::`-shaped public API**, where
  the reference raises `FormatException`.
- **Silent corruption is bidirectional**: text that came *from an argument* is
  re-read as format syntax, so a later argument overwrites an earlier
  argument's literal text.

CCF-012's own words — "altering just one API preserves divergent brace rules" —
are confirmed: `String::Format("{{0}}", …)` throws while
`FormattableString("{{0}}", …)` returns `"{value}"`, on the same input shape.

### 0.3 Why CCF-017 was deferred

CCF-017 reproduces exactly as filed (`1881_prefix.log`, final block): every
comparison is identity, `CLSCompliantAttribute(true) != CLSCompliantAttribute(true)`,
hashes differ, `System::Attribute` is directly constructible. Nothing in the
finding is stale or false. It was deferred for three measured reasons:

1. **Its demanded repair is unreachable in this port.** SR-AUD-114 asks for
   .NET's contract: "first requires the same runtime type and then compares
   every instance field; its hash is derived from the first non-array field".
   Fieldwise comparison of an arbitrary derived type requires **runtime
   reflection**, which `CLAUDE.md` lists as a **permanent deviation, completely
   out of scope, stubs are the correct end state**. No base-class policy in C++
   can enumerate a subclass's fields. The finding's own preferred shape — "one
   base policy … rather than per-marker overrides" — is therefore not
   implementable, and the only shapes that are (per-type `Equals` overrides)
   are the ones it explicitly rejects.
2. **Every remaining part of it is approval-gated.** Making `Attribute` abstract
   removes public constructibility — a public **source-compatibility break**.
   Changing the default `Equals`/`GetHashCode` from identity to anything else is
   an **ownership-visible semantic change** to five virtuals. Both sit squarely
   inside this batch's approval boundary, and the existing green
   `AttributeTests` fixture *deliberately* pins `Attribute a, b` as unequal, so
   the change also mandates a test rewrite rather than an addition.
3. **It carries none of the preference criteria.** No memory-safety or lifetime
   impact, no undefined behaviour, no silent corruption, no public-input crash
   or hang, no missing validation before a dangerous operation. Measured: zero
   sanitizer findings, because there is nothing for a sanitizer to find.

CCF-017 is **not** closed, downgraded, or reclassified by this document.
SR-AUD-114 stays `confirmed`. Its correct next step is a *design-only* ticket
that records the permanent-deviation argument and drafts the exact approval
wording for the abstract-base decision — deliberately not started here, because
this batch has compatible work and the prompt forbids opening an approval
request while compatible work remains.

### 0.4 Why the CCF-019 fallback was not taken

The fallback is gated on CCF-012 and CCF-017 both being entirely
approval-gated, both false or stale, both lacking a coherent implementable
family, or a repository dependency ordering CCF-019 first. **None holds.**
CCF-012 is live, reproducible, coherent, and its largest and most severe portion
is compatible. CCF-019's two open members (SR-AUD-327 `JsonNode`, SR-AUD-333
`XObject`) are genuinely higher-severity in kind — ASan-confirmed
use-after-free — but the cross-cutting record already states that each "needs
its own compatibility review before repair", and both repairs are public
**ownership-model** changes, which this batch may not infer permission for.
CCF-019 remains the strongest candidate for a batch that opens with an
ownership design and an approval request.

### 0.5 Selection

**CCF-012 is the primary family for this batch.** It is the only candidate that
combines reachable undefined behaviour, a reachable public-input hang, silent
corruption, one coherent shared root cause across two files, and a compatible
repair that is *identical to* the structural repair the finding asks for.

---

## 1. Exact family scope

**In scope.** The two hand-written composite-format replacement engines and the
per-item specifier formatting they call:

- `System::String::Format` — 22 public overloads, `modules/core/src/System/String.cpp`,
  and the three file-internal helpers they share: `extractSpec`, `replaceArg`,
  `FinalizeFormat`, plus the two specifier formatters `fmtInt` and `fmtDouble`.
- `System::FormattableString::ToString()` and `ToString(const IFormatProvider*)`,
  `modules/core/include/System/FormattableString.hpp`, and the two static
  wrappers `Invariant` / `CurrentCulture` that call them.

**In scope as callers only** (not modified; used as regression surface):
`System::Text::StringBuilder::AppendFormat` (11 overloads) and
`System::Console::Write` / `WriteLine` format overloads (11), all of which
forward to `String::Format` and therefore inherit every defect and every repair.

**Out of scope for this family** — see section 18 for the full list with
reasons: culture/`IFormatProvider` behaviour, `ICustomFormatter`, custom
numeric format strings, UTF-16 vs UTF-8 text semantics, `CompositeFormat`
(not ported), and `String::LastIndexOf` (SR-AUD-016, same file, different
cause).

---

## 2. Every related SR-AUD finding

| Finding | Severity | Status entering this batch | Half |
|---|---|---|---|
| SR-AUD-015 | medium | `confirmed` | `String::Format` brace grammar (`String.cpp.audit.md`) |
| SR-AUD-015 (extended) | medium | `confirmed` | `FormattableString::ToString` reinterpretation (`FormattableString.hpp.audit.md`) |

SR-AUD-015 is a **single identifier covering two files**. CCF-012 lists three
owning reports; the third
(`modules/core/tests/System/FormattableStringTests.cpp.audit.md`) is a
test-coverage report with no separate identifier.

**Numbering stays frozen at 364.** Every defect class this plan adds beyond the
audit text (non-termination, `std::abs` UB, escaped `std::` exceptions) was
found *during remediation* of an existing finding in the files that finding
owns, so it is folded into SR-AUD-015's record by appending, and **no new
`SR-AUD-*` identifier is issued**.

---

## 3. Modules, files, types and symbols

| Module | Component | File | Symbol |
|---|---|---|---|
| `core` | `Core.Base` | `src/System/String.cpp` | `String::Format` ×22 |
| `core` | `Core.Base` | `src/System/String.cpp` | `(anon)::extractSpec`, `::replaceArg`, `::FinalizeFormat`, `::fmtInt`, `::fmtDouble` |
| `core` | `Core.Base` | `include/System/String.hpp` | `Format` declarations ×22 |
| `core` | `Core.Base` | `include/System/FormattableString.hpp` | `FormattableString::ToString` ×2, `Invariant`, `CurrentCulture` |
| `text` | `Text.Core` | `include/System/Text/StringBuilder.hpp` | `AppendFormat` ×11 (caller) |
| `console` | `Console` | `include/System/Console.hpp` | `Write`/`WriteLine` ×11 (caller) |

No module boundary changes. No new component edge. `Core.Base` already owns both
files.

---

## 4. Complete public-entry inventory

### 4.1 `String::Format` — 22 overloads (`String.hpp:632-676`)

Grouped by the argument-tuple shape they reach, because that determines which
of `fmtInt` / `fmtDouble` / raw-string paths a defect can enter:

| # | Signature tail | Specifier path |
|---|---|---|
| 1 | `(fmt, intcs)` | `fmtInt` |
| 2 | `(fmt, double)` | `fmtDouble` |
| 3 | `(fmt, const std::string&)` | none (raw) |
| 4 | `(fmt, float)` | `fmtDouble` (widened) |
| 5 | `(fmt, bool)` | none (raw `"True"`/`"False"`) |
| 6 | `(fmt, char)` | none (raw) |
| 7 | `(fmt, longcs)` | none (`std::to_string`) |
| 8 | `(fmt, intcs, intcs)` | `fmtInt` ×2 |
| 9 | `(fmt, intcs, const std::string&)` | `fmtInt`, raw |
| 10 | `(fmt, const std::string&, intcs)` | raw, `fmtInt` |
| 11 | `(fmt, const std::string&, const std::string&)` | raw ×2 |
| 12 | `(fmt, double, double)` | `fmtDouble` ×2 |
| 13 | `(fmt, double, const std::string&)` | `fmtDouble`, raw |
| 14 | `(fmt, const std::string&, double)` | raw, `fmtDouble` |
| 15 | `(fmt, longcs, intcs)` | `std::to_string`, `fmtInt` |
| 16 | `(fmt, intcs, longcs)` | `fmtInt`, `std::to_string` |
| 17 | `(fmt, intcs, double)` | `fmtInt`, `fmtDouble` |
| 18 | `(fmt, double, intcs)` | `fmtDouble`, `fmtInt` |
| 19 | `(fmt, intcs, intcs, intcs)` | `fmtInt` ×3 |
| 20 | `(fmt, string, string, string)` | raw ×3 |
| 21 | `(fmt, longcs, longcs)` | `std::to_string` ×2 |
| 22 | `(fmt, string, string, string, string)` | raw ×4 |

**Every one of the 22 reaches `replaceArg` and `FinalizeFormat`.** The
non-termination defect is therefore in all 22; the `fmtInt`/`fmtDouble` UB and
`std::stoi` escape are in the 13 that take a numeric argument (1, 2, 4, 8, 9,
10, 12, 13, 14, 15, 16, 17, 18, 19 — 14 rows, counting each numeric position
once per overload).

### 4.2 `FormattableString` — 4 public entries

| Entry | Reaches |
|---|---|
| `ToString()` | the replacement loop |
| `ToString(const IFormatProvider*)` | delegates to `ToString()` |
| `static Invariant(const FormattableString&)` | delegates to `ToString()` |
| `static CurrentCulture(const FormattableString&)` | delegates to `ToString()` |

`ToString` is `virtual`, so a subclass may override it; the two statics call
through the vtable and therefore honour an override. Neither the repair nor the
tests may assume the base body runs.

### 4.3 Wrappers (regression surface, unmodified)

11 `StringBuilder::AppendFormat` overloads and 11
`Console::Write`/`WriteLine` overloads. `Console` is not exercised by the
permanent tests (it writes to the real stream); `StringBuilder::AppendFormat`
is, because a hang or a corrupted substitution reaches it unchanged.

---

## 5. Current behaviour matrix (measured 2026-07-30)

From `build-probe/1881_prefix.log`. `.NET` column is the current reference
behaviour established in section 6.

### 5.1 `String::Format`

| # | Input | Current | .NET | Class |
|---|---|---|---|---|
| A1 | `Format("{{0}}", 42)` | `FormatException` "Index (zero based) must be…" | `"{0}"` | grammar (escape) |
| A2 | `Format("{{}}", 42)` | `FormatException` "Input string was not in a correct format." | `"{}"` | grammar (escape) |
| A3 | `Format("{{{0}}}", 42)` | `FormatException` "Input string was not in a correct format." | `"{42}"` | grammar (escape) |
| B1 | `Format("value}", 42)` | `"value}"` | `FormatException` | grammar (tighten) |
| B2 | `Format("a}b{0}", 42)` | `"a}b42"` | `FormatException` | grammar (tighten) |
| C1 | `Format("{0}{1}", "{1}", "X")` | `"XX"` | `"{1}X"` | **silent corruption** |
| C2 | `Format("{0}|{1}", "{1}", "Y")` | `"Y|Y"` | `"{1}|Y"` | **silent corruption** |
| D1 | `Format("{0}", "{0}")` | **hangs** | `"{0}"` | **non-termination** |
| D2 | `Format("[{0}]", "x{0}y")` | **hangs** | `"[x{0}y]"` | **non-termination** |
| E1 | `Format("{0:D99999999999}", 42)` | `std::out_of_range` escapes | `FormatException` "Format specifier was invalid." | **escaped `std::`** |
| E2 | `Format("{0:DX}", 42)` | `std::invalid_argument` escapes | `"DX"` (custom format) | **escaped `std::`** |
| E3 | `Format("{0:D-2147483648}", 42)` | `"42"` **with UBSan-confirmed UB** | `"D-2147483648"` (custom format) | **undefined behaviour** |
| E4 | `Format("{0:F99999999999}", 1.5)` | `std::out_of_range` escapes | `FormatException` | **escaped `std::`** |
| E5 | `Format("{0:X-2147483648}", 42)` | `"2A"` **with UB** | `"X-2147483648"` (custom format) | **undefined behaviour** |
| F1 | `Format("{0,6}|", 42)` | `"42\|"` | `"    42\|"` | grammar (alignment) |
| G1 | `Format("{1}", 42)` | `FormatException` index msg | `FormatException` | correct |
| G2 | `Format("{0", 42)` | `FormatException` **index msg** | `FormatException` (unclosed item) | correct type, wrong message |
| G3 | `Format("abc", 42)` | `"abc"` | `"abc"` | correct |
| H1 | `Format("{1}{0}{1}", "A", "B")` | `"BAB"` | `"BAB"` | correct |
| H2 | `Format("{0:X4}", 255)` | `"00FF"` | `"00FF"` | correct |

Three further rows, measured by the supplementary probe
`build-probe/1881_extra_probe.cpp` (`build-probe/1881_extra_prefix.log`) after
the matrix above was written. All three are the same two causes; none needs a
new identifier:

| # | Input | Current | .NET | Class |
|---|---|---|---|---|
| K1 | `Format("{0:X}/{0:D3}", 255)` | `"FF/FF"` | `"FF/255"` | **silent corruption** (`extractSpec` returns the *first* item's specifier for every occurrence) |
| K2 | `Format("{0:D3}/{0:X}", 255)` | `"255/255"` | `"255/FF"` | same |
| K3 | `Format("{0:D999999999}", 7)` | **does not terminate** | `OutOfMemoryException` | **third non-termination class** |
| K4 | `Format("{0}{1}", "{1}", "{0}")` | throws `FormatException` (index) | `"{1}{0}"` | **spurious failure** on a valid call |
| K5 | `Format("a}}b", 42)` | `"a}}b"` | `"a}b"` | grammar (escape) |
| K6 | `Format("{0:D9}", 7)` | `"000000007"` | `"000000007"` | correct |
| K7 | `FS("{10}", 11 args)` | `"a10"` | `"a10"` | correct (multi-digit index works) |

**K3 is a distinct non-termination mechanism from D1/D2** and must not be
confused with it. `fmtInt`'s decimal-padding loop is
`while ((int)s.size() < width) s = "0" + s;` (`String.cpp:162`) — one full
string copy per character, so a width of 999 999 999 is ~10<sup>18</sup> byte
copies. It is not an infinite loop; it is unbounded quadratic work plus a
~1 GB allocation reached from a single public format string. The repair must
make the padding a single O(n) construction *and* keep the reference's own
digit bound.

**K4 shows the corruption is not always silent.** Two arguments that each
contain the other's placeholder rewrite each other into an index the call never
supplied, and the result is an exception on input the reference formats
successfully. A repair that only stopped the hang would leave K4 throwing.

### 5.2 `FormattableString::ToString`

| # | Input | Current | .NET-shaped | Class |
|---|---|---|---|---|
| J1 | `FS("{0}", {"{1}","second"})` | `"second"` | `"{1}"` | **silent corruption** |
| J2 | `FS("{{0}}", {"value"})` | `"{value}"` | `"{0}"` | grammar (escape) |
| J3 | `FS("{1}", {"only"})` | `"{1}"` | `FormatException` | grammar (tighten) |
| J4 | `FS("value}", {"a"})` | `"value}"` | `FormatException` | grammar (tighten) |
| J5 | `FS("{0}", {"{0}"})` | `"{0}"` | `"{0}"` | correct (by accident — see 8.2) |
| J6 | `FS("{0,6}", {"a"})` | `"{0,6}"` | `"     a"` | grammar (alignment) |
| J7 | `FS("{0:D4}", {"a"})` | `"{0:D4}"` | `"a"` | grammar (specifier) |
| J8 | `FS("{1}{0}", {"A","B"})` | `"BA"` | `"BA"` | correct |

**J1 versus J5 is the whole shape of the bug in one pair.** Both insert text
containing a placeholder; J5 is correct only because the inner `while` advances
`pos` past the inserted text, and J1 is wrong because the *outer* `for i` loop
restarts at `pos = 0` for the next index and re-reads what index 0 inserted.

### 5.3 Divergence between the two engines on identical input

| Input shape | `String::Format` | `FormattableString` |
|---|---|---|
| `{{0}}` | throws | returns `"{value}"` |
| stray `}` | returns literally | returns literally |
| missing index | throws | returns literally |
| `{0,6}` | substitutes, drops alignment | leaves the whole item literal |
| `{0:D4}` | substitutes with the specifier applied | leaves the whole item literal |
| self-referential argument | **hangs** | terminates |

Six rows, four disagreements. This is exactly the "divergent brace rules"
CCF-012 warns a one-API repair would preserve.

---

## 6. Actual current .NET / reference behaviour

Read 2026-07-30 from
`System.Private.CoreLib/src/System/Text/ValueStringBuilder.AppendFormat.cs`
(the shared `AppendFormatHelper`, which `string.Format`, `StringBuilder.AppendFormat`
and `FormattableString.ToString` all reach),
`Common/src/System/Number.Formatting.Common.cs` (`ParseFormatSpecifier`), and
`System.Private.CoreLib/src/Resources/Strings.resx`.

### 6.1 The grammar

```
format      := ( text | "{{" | "}}" | item )*
item        := "{" index [ spaces ] [ "," [ spaces ] [ "-" ] width [ spaces ] ] [ ":" itemFormat ] "}"
index       := digit+            ; 0 <= index < 1_000_000
width       := digit+            ; -1_000_000 < width < 1_000_000
itemFormat  := any char except "{"    ; terminated by the first "}"
```

- `{{` and `}}` are escapes producing one literal brace
  (`AppendFormat.cs:57-62`: "if (brace == ch) { Append(ch); pos++; continue; }").
- An unescaped `}` outside an item throws `FormatException` with
  `Format_UnexpectedClosingBrace`, "Unexpected closing brace without a
  corresponding opening brace." (`AppendFormat.cs:64-68`).
- A `{` not followed by an ASCII digit throws with `Format_ExpectedAsciiDigit`
  (`:86-89`).
- Running off the end of the format inside an item throws with
  `Format_UnclosedFormatItem`, "Format item ends prematurely."
- A `{` **inside** the specifier throws `Format_UnclosedFormatItem`
  (`:176-180`) — braces inside an argument hole are not supported.
- An index `>= args.Length` throws `Format_IndexOutOfRange`, "Index (zero based)
  must be greater than or equal to zero and less than the size of the argument
  list." (`:194-197`).
- All of the above reach `ThrowFormatInvalidString(pos, reason)`, whose message
  is `Format_InvalidStringWithOffsetAndReason`: "Input string was not in a
  correct format. Failure to parse near offset {0}. {1}".
- **The parse walks the format string exactly once and appends to a separate
  output buffer.** Inserted argument text is never re-examined. That single
  property is what makes .NET immune to C1/C2/D1/D2/J1 by construction.

### 6.2 The numeric specifier

`ParseFormatSpecifier(format, out digits)`
(`Number.Formatting.Common.cs:55-113`):

- letter + nothing → standard format, `digits = -1`;
- letter + 1 or 2 ASCII digits → fast paths;
- letter + more digits → accumulate, and **`if (n >= 100_000_000)
  ThrowHelper.ThrowFormatException_BadFormatSpecifier()`** → `FormatException`
  with `Argument_BadFormatSpecifier`, **"Format specifier was invalid."**;
- letter followed by anything that is not an ASCII digit (including `-`) →
  **not** a standard format; the whole string is treated as a *custom* numeric
  format string, in which unrecognised characters are emitted literally.

So the reference answers E1/E4 with a `FormatException`, and E2/E3/E5 with a
custom-format literal. It never raises a non-`System` exception and never
performs `abs` on a parsed value.

---

## 7. Corrected audit premises

Historical audit text is preserved; these are appended corrections, following
the SR-AUD-081 / SR-AUD-362 convention.

1. **SR-AUD-015 is understated, not overstated.** The finding describes a
   grammar-fidelity problem ("rejects valid escaped braces and accepts
   malformed closing braces"). Measurement shows the same root cause also
   produces a **non-terminating loop**, **UBSan-confirmed signed-overflow UB**,
   and **two escaping `std::` exception types**. A repair scoped to the audit's
   literal wording — brace handling only — would leave all four in place. The
   severity as filed (`medium`) understates a reachable public-input hang.

2. **`String::Format`'s scope is 22 overloads plus 22 wrappers, not "the String
   path".** Every overload reaches the same three helpers, so any per-overload
   reasoning is unnecessary and any per-overload repair would be wrong.

3. **`FormattableString`'s "escaped `{{0}}` becomes `{value}`" is correct but
   its cause is not brace escaping.** There is no escape handling in that engine
   at all; `{{0}}` contains a literal `{0}` which is substituted, leaving the
   outer braces. The distinct, more serious sibling in the same loop is J1,
   where the *inserted argument text* is re-read.

4. **The audit's "FormattableString … leaves a missing index literal" is
   accurate, and so is the implied asymmetry**: `String::Format` throws on a
   missing index and `FormattableString` does not. Confirmed (G1 vs J3).

5. **`Format("{0", 42)` throws the wrong message, which the audit did not
   note.** It reports the *index-out-of-range* text for an *unclosed item*,
   because `FinalizeFormat` classifies solely on "is the next character a
   digit". The exception **type** is right.

6. **Not every self-referential argument hangs.** `FormattableString` J5 does
   not, and `String::Format` would not either if `replaceArg` advanced its
   cursor instead of resetting it. The defect is `searchFrom = 0` at
   `String.cpp:122`, one statement.

7. **There are three non-termination mechanisms, not one.** D1/D2 (cursor
   reset), K3 (quadratic pad), and — as K4 shows — a fourth outcome where the
   same corruption terminates but throws on valid input. Any claim that "the
   hang" is fixed must name which of the three it means.

8. **The family has three root causes, not the one the cross-cutting record
   implies.** CCF-012 describes only the parse model. `fmtInt`/`fmtDouble`'s
   throwing `std::stoi` + `std::abs` and its quadratic padding are independent
   of the parse model and would survive a parser rewrite that left those two
   functions alone.

---

## 8. Common root cause

### 8.1 One cause, stated once

> **Both engines produce output by repeatedly mutating a buffer that already
> contains substituted argument text, instead of parsing the format string once
> and appending to a separate output.**

Every measured defect follows from that one property:

| Defect | Why the cause produces it |
|---|---|
| D1/D2 non-termination | the scan cursor is reset to `0` after a substitution, so inserted text is re-scanned forever if it contains its own placeholder |
| C1/C2/J1 corruption | a later pass over the buffer cannot distinguish format text from argument text |
| A1–A3, J2 escapes | escape handling would have to run before substitution, but substitution runs first |
| B1/B2, J3/J4 | validation runs *after* substitution (`FinalizeFormat`), so it can only inspect what survived, and never sees a `}` |
| F1, J6/J7 divergence | each engine ad-hoc-recognises a different subset of item syntax because neither has a grammar |
| G2 wrong message | the classification is a one-character lookahead, not a parse state |

The two `stoi` defects (E1–E5) are a **second, smaller cause** in the same
family: `fmtInt`/`fmtDouble` parse the specifier's numeric tail with a throwing,
unbounded `std::stoi` and then apply `std::abs` to a possibly-`INT_MIN` result.
This is separable from the parse model but lives in the same call chain and the
same repair ticket, because a correct parser must hand the specifier to these
functions and must not have its own contract broken by them.

### 8.2 Structurally related surfaces the audit did not name

- `extractSpec` finds the **first** `{N`, ignoring that a format may contain
  `{N}` twice with different specifiers; the second occurrence silently gets the
  first's specifier. **Measured** (K1/K2): `Format("{0:X}/{0:D3}", 255)` returns
  `"FF/FF"` and `Format("{0:D3}/{0:X}", 255)` returns `"255/255"`, where the
  reference gives `"FF/255"` and `"255/FF"`. A single-pass parser removes it
  without a separate fix.
- `fmtInt`'s decimal padding is quadratic and unbounded (K3). This is a third
  root cause in the family — neither the parse model nor the `stoi` call — and
  it is repaired in the same ticket because it lives in the same function as
  E1–E5.
- `StringBuilder::AppendFormat` and `Console::Write`/`WriteLine` inherit every
  defect verbatim and are silent in the audit; they need regression coverage,
  not their own repair.
- `fmtInt`'s hex path casts to `unsigned int` regardless of the source width, so
  `Format("{0:X}", longcs)` truncates. **Deliberately excluded** (section 18) —
  it is a numeric-formatting fidelity question, not a composite-format one, and
  changing it alters successful output.

---

## 9. Compatible versus approval-gated

This is the classification that governs the ticket breakdown. The rule applied:
a change is **compatible** when the current behaviour is undefined,
non-terminating, a leak of a non-`System` exception, or a silently wrong value;
it is **approval-gated** when a currently-successful call with a defined,
plausibly-intended result would start failing, or would start producing
different successful text.

### 9.1 Compatible (this batch)

| Row | Change | Why compatible |
|---|---|---|
| D1, D2 | terminate | no defined behaviour exists to preserve; a hang has no observable result |
| C1, C2 | `"XX"` → `"{1}X"`, `"Y\|Y"` → `"{1}\|Y"` | silent corruption of a documented substitution; precedent CCF-013/#1816 shipped exactly this class |
| J1 | `"second"` → `"{1}"` | same class, same reasoning |
| E1, E4 | `std::out_of_range` → `System::FormatException` | replaces an *undocumented leak of a foreign exception type* with the reference's own outcome; not a taxonomy change between two documented `System` exceptions |
| E2 | `std::invalid_argument` → defined value | same |
| E3, E5 | UB → defined value | UB has no behaviour to preserve |
| G2 | index message → "Input string was not in a correct format." | the type, HResult and throw/no-throw decision are unchanged; only a factually wrong message is corrected |
| K1, K2 | second `{N}` gets its own specifier | falls out of the parse; currently silently wrong |
| K3 | quadratic unbounded padding → O(n) padding + the reference's digit bound | no defined behaviour to preserve |
| K4 | spurious `FormatException` → `"{1}{0}"` | a valid call currently fails; the failure is an artefact of the corruption, not a contract |

### 9.2 Approval-gated (deferred, `needs_user`)

| Row | Change | Why gated |
|---|---|---|
| A1–A3 | `{{`/`}}` escaping in `String::Format` | changes the **accepted grammar**; `Format("{{0}}", x)` goes from throw to a value, and `"}}"` goes from `"}}"` to `"}"` — the second is a **different successful output** |
| J2 | `{{`/`}}` escaping in `FormattableString` | same, and `{{0}}` goes from `"{value}"` to `"{0}"` |
| B1, B2, J4 | stray `}` rejected | **success → failure**; every currently-working call containing a literal `}` starts throwing |
| J3 | missing index rejected | **success → failure** |
| F1, J6 | alignment applied | **different successful output** (`"42\|"` → `"    42\|"`) |
| J7 | `{N:spec}` recognised by `FormattableString` | **different successful output** (`"{0:D4}"` → `"a"`) |

The gated rows are one coherent decision — "adopt .NET's composite-format
grammar" — and belong in **one** `needs_user` ticket, not six.

### 9.3 The invariant the compatible tickets must hold

> **After the compatible repair, every input that succeeds today must still
> succeed and produce byte-identical output, except C1/C2/J1 (corruption) and
> E3/E5 (undefined), and every input that throws today must still throw the same
> exception type.**

This is testable directly and is the acceptance criterion of both
implementation tickets.

---

## 10. Ownership and lifetime model

Not applicable in the CCF-019 sense: no handle, no borrowed pointer, no
reference-counted state, no iterator escapes either engine. Both operate on
`std::string` values and return by value.

Two lifetime obligations the repair must not break:

1. `FormattableString::ToString` returns `std::string` by value and its
   `GetArgument` returns `const std::string&` into `args_`. The repair must read
   `args_` in place and must not return a reference into a temporary.
2. `String::Format`'s helpers currently take and return `std::string` by value.
   The repair may use `std::string_view` **only** over the caller's `format`
   parameter, whose lifetime spans the call; it must not create a view over a
   temporary built inside the function.

---

## 11. Validation and exception ordering

Binding for the implementation:

1. **Parse before formatting.** The grammar of an item (index, optional
   alignment, optional specifier, closing brace) is decided before any argument
   is formatted, so a malformed item cannot be reported *after* a partial
   substitution has been published.
2. **Index range before argument access.** An index `>= argument count` throws
   `System::FormatException` with the existing index message, before the
   argument is touched.
3. **Specifier validation before use.** `fmtInt`/`fmtDouble` decide whether the
   numeric tail is a valid bounded digit run before computing a width or
   precision from it. No `std::stoi`, no `std::abs` on a parsed value. The rule
   is the reference's own (`Number.Formatting.Common.cs:93-105`), stated here so
   the implementation has no latitude:

   - the tail must be ASCII digits only;
   - accumulate `n = n * 10 + d`, and **before** each multiply, if
     `n >= 100'000'000` throw `System::FormatException("Format specifier was
     invalid.")` — the reference's `Argument_BadFormatSpecifier`;
   - a tail containing any non-digit (including `-`) means the specifier is not
     a standard one; this port does not implement custom numeric format strings,
     so the value is emitted with no specifier applied. **No exception, no UB**;
   - padding is produced by **one** `std::string(count, '0')`-shaped
     construction, never by repeated single-character prepends.
4. **No partial output on throw.** `String::Format` builds into a local and
   returns it only on success; a throw from any position leaves the caller's
   world unchanged. (It already has this property because it returns by value;
   the repair must keep it.)
5. **Exception type is preserved exactly.** Every path that throws
   `System::FormatException` today still throws `System::FormatException`. No
   new exception type is introduced by the compatible tickets.

---

## 12. Source / ABI / `noexcept` / layout matrix

| Aspect | Consequence |
|---|---|
| Public signatures | **unchanged** — all 22 `Format` declarations and all 4 `FormattableString` entries keep their exact signatures |
| `noexcept` | **unchanged** — none of the affected entries is `noexcept` today and none becomes so |
| Virtual functions / vtable | **`FormattableString`'s vtable is unchanged**: `ToString()` and `ToString(const IFormatProvider*)` keep their slots, order and signatures. No virtual is added or removed |
| Data members / layout | **unchanged** — `String` is a static-only utility class with no members; `FormattableString` keeps `format_` and `args_` in order |
| Calling convention | unchanged |
| Header changes | `String.cpp` is a `.cpp`-only change. `FormattableString.hpp` is a public header, but the change is confined to **one inline function body**; no declaration moves |
| Mandatory downstream migration | **none** |
| Component graph | unchanged (41 modules / 91 edges) |
| Observable semantics | limited to section 9.1's rows |

`FormattableString::ToString` is an inline body in a public header, so a
downstream consumer must **recompile** to pick up the fix — the ordinary
consequence of any inline change, not an ABI break, and identical in kind to
#1867/#1868/#1870, which shipped header-inline repairs.

---

## 13. Implementation dependency order

```
#1881  design (this document)                        [no code]
   |
   +-- #1882  String::Format single-pass parse       [independent]
   |            + bounded specifier parsing
   |
   +-- #1883  FormattableString::ToString            [independent of #1882]
   |            single-pass substitution
   |
   +-- #1884  needs_user: adopt .NET composite grammar (both engines)
                (created, NOT implemented)
```

#1882 and #1883 touch disjoint files and share no helper, so either may land
first. They are ordered #1882 → #1883 only because #1882 carries the UB and the
hang, and because #1883's test matrix reuses #1882's shared-grammar vocabulary.
**#1884 depends on both** — the gated grammar is defined against the repaired
parse model, not the current one.

---

## 14. Permanent test matrix

All tests are **add-only**. No existing test is weakened, deleted or
recategorised. The 8 currently-green `String::Format` validation tests
(`StringTests.cpp:436-461`) must continue to pass **unmodified**, including
`Format_TwoDigitIndexDoesNotAliasSingleDigitIndex` and
`Format_UnclosedBrace_Throws`.

### 14.1 `String::Format` (#1882) — target ≥ 24 regressions

| Group | Cases |
|---|---|
| Termination | `Format("{0}", "{0}")`, `Format("[{0}]", "x{0}y")`, `Format("{0}{1}", "{1}", "{0}")`, argument equal to the whole format |
| Argument isolation | C1, C2, argument containing `{`, `}`, `{{`, `:`, `,`, and a full item `{0:X4}` |
| Specifier bounds | E1, E2, E3, E4, E5, plus `{0:D0}`, `{0:D9}`, `{0:D20}`, and the rejection boundary `{0:D100000000}` / `{0:D2147483648}` / `{0:D99999999999}` |
| Specifier reuse | K1, K2 — both occurrences get their own specifier |
| Termination (K3) | `{0:D100000000}` returns promptly by throwing; the quadratic path is gone |
| Spurious failure (K4) | `Format("{0}{1}", "{1}", "{0}")` returns `"{1}{0}"` instead of throwing |
| Preserved success | every currently-green case re-asserted through the new parser: plain, `X`, `x`, `X4`, `D3`, `D3` negative, `F2`, `F0`, plain double, 2-arg, 3-arg, 4-arg, `bool`, `char`, `longcs` |
| Preserved failure | G1, G2 (type), `{abc}`, `{1} and {10}`, `{5}` |
| Preserved grammar (gated rows pinned as *current* behaviour) | B1, B2, F1, A1–A3 — pinned so #1884 cannot land silently |
| Wrappers | `StringBuilder::AppendFormat` with a self-referential argument; `AppendFormat` with C1's shape |
| Message | G2's corrected message asserted verbatim |

### 14.2 `FormattableString::ToString` (#1883) — target ≥ 14 regressions

| Group | Cases |
|---|---|
| Argument isolation | J1, plus an argument containing `{0}`, `{2}`, `{{`, and the exact format text |
| Multi-digit | `FS("{10}", 11 args)`, `FS("{1}{10}", 11 args)` |
| Preserved | J2, J3, J4, J5, J6, J7, J8 pinned as current behaviour |
| Virtual dispatch | a subclass overriding `ToString()`; `Invariant`/`CurrentCulture` honour the override |
| Provider | `ToString(nullptr)` and `ToString(&provider)` equal `ToString()` |
| Bounds | `GetArgument(-1)`, `GetArgument(count)` still throw `IndexOutOfRangeException` |

### 14.3 Mutation checks (required)

For each implementation ticket, at least one mutation must be shown to fail the
new permanent tests, with proof the mutated binary was rebuilt and executed:

- #1882: restore `searchFrom = 0` → the termination tests must hang or fail;
  restore `std::stoi` → the specifier-bounds tests must fail.
- #1883: restore the outer `for i` / inner `find` loop → J1's test must fail.

---

## 15. Sanitizer matrix

| Sanitizer | Applicability | What it must show |
|---|---|---|
| **UBSan** | **required** — the family contains UBSan-confirmed signed-overflow UB | `1881_ubsan_prefix.log` records `String.cpp:149` before; the post-fix run over the same inputs must be silent. Operands must be **runtime** values (read from `argv`/a table) so constant folding cannot suppress the diagnostic |
| **ASan** | **required** — both repairs rewrite index arithmetic over `std::string` | zero heap/stack-buffer diagnostics over the whole case matrix, including 0-length formats, formats ending mid-item, and a 100 000-character format |
| **LSan** | **required** — the parser allocates | zero leaks across the matrix, including every throwing path (a throw must not leak a partially built buffer) |
| **TSan** | **not applicable** — recorded, not skipped: neither engine has shared mutable state, an atomic, a lock, or a cache; both are pure functions of their arguments |

Freshness obligation: `String.cpp` is compiled into `libsharp_runtime_core.a`,
so a sanitizer conclusion about it **must** prove the archive and the changed
object are newer than the source. `FormattableString.hpp` is header-only, so
instrumenting a probe recompiles it and no stale-archive risk exists — that
distinction must be stated explicitly in each ticket's evidence, not assumed.

---

## 16. Performance implications

The current `String::Format` is **O(n · m · k)** at best and **unbounded** at
worst: `replaceArg` copies the whole buffer per argument, calls `find` from
position 0 after every substitution, and `extractSpec` re-scans the format once
per argument. A single-pass parser is **O(n + total argument length)** with one
`reserve`. Every 2-argument call today performs at least four full scans of the
format and two full string copies; after the repair it performs one scan and one
append per segment.

`FormattableString::ToString` is currently **O(args · n)** with a full `find`
sweep per argument; it becomes **O(n)**.

No allocation-count regression is possible: the repair replaces N intermediate
`std::string` copies with one output buffer. The measurement is not the point of
the family, but it must not regress, and a 100 000-character format case in the
ASan matrix doubles as a smoke test that it does not.

---

## 17. Explicit exclusions

Named so a later reader does not mistake absence for oversight:

1. **Culture and `IFormatProvider`.** Both engines ignore the provider by
   documented design. Unchanged.
2. **`ICustomFormatter`.** Not ported; the reference consults it first. Out of
   scope.
3. **Custom numeric format strings** (`"#,##0.00"`, and the `"DX"` /
   `"D-2147483648"` rows E2/E3/E5). The reference treats a non-standard
   specifier as a custom format and emits it literally; this port has never
   implemented custom formats and this family does not add them. The repair's
   obligation is only that these inputs become **defined** — no UB, no escaping
   `std::` exception.
4. **`System.Text.CompositeFormat`.** Not ported. Not added.
5. **UTF-16 vs UTF-8 text semantics** and alignment measured in characters
   rather than bytes. That is CCF-015's subject (SR-AUD-048), a different cause.
6. **`fmtInt`'s `unsigned int` hex truncation for `longcs`** (8.2). A numeric
   fidelity defect that changes successful output; not composite formatting.
7. **`SR-AUD-016`** (`String::LastIndexOf` range spill). Same file, unrelated
   cause, not a CCF-012 member.
8. **`Concat`/`Join` size-overflow diagnostics.** Named in the same per-file
   report under "Other missing assertions"; not this family.
9. **`String::Format`'s overload set.** No overload is added, removed or
   re-signatured; the port's 22 fixed-arity overloads stand in for C#'s
   `params object?[]` and that design is not revisited here.
10. **Accepted-but-enormous pad widths.** After the repair, a width the
    reference *accepts* — anything below 100 000 000 — still materialises a
    string of that length in one allocation, exactly as .NET does before it
    raises `OutOfMemoryException`. The repair's obligation is to remove the
    **quadratic** cost and the **unbounded** parse (K3), not to invent a
    stricter bound than the reference has. No permanent test materialises a pad
    above a few dozen characters; the acceptance side is exercised at small
    widths and the rejection side at the reference's own boundary. Stated so a
    later reader does not read the absence of a 100 MB test as missing coverage.
10. **`CCF-017` and `CCF-019`.** Compared in section 0 and deliberately deferred;
    neither is closed, downgraded or reclassified by this document.

---

## 18. Completion criteria

CCF-012 may be recorded **CLOSED** only when all of the following hold:

1. `String::Format` and `FormattableString::ToString` both derive their output
   from **one left-to-right pass over the format string**, with no re-examination
   of inserted argument text, in a shared documented grammar.
2. No input to any of the 23 public entries fails to terminate.
3. No `std::` exception escapes any of the 23 public entries.
4. UBSan is silent over the whole case matrix.
5. The section 9.3 invariant holds, demonstrated case by case.
6. The two engines agree on every row of the section 5.3 divergence table.
7. Sections 14.1 and 14.2's permanent tests exist and pass, and 14.3's mutation
   checks are recorded.
8. SR-AUD-015's record carries the correction from section 7.

**Criterion 6 cannot be met by the compatible tickets alone** — four of its six
rows are approval-gated (9.2). Therefore **CCF-012 will be PARTIALLY
REMEDIATED** after #1882 and #1883, and closes only when #1884 is approved and
implemented. This is stated in advance so the family's status is not overclaimed
later.

---

## 19. Ticket breakdown

| # | Kind | Scope | Status |
|---|---|---|---|
| **#1881** | design | this document, including the CCF-012 vs CCF-017 comparison | **done** (2026-07-30) |
| **#1882** | implementation, compatible | `String::Format`: single-pass parse; bounded specifier parsing; termination; UB removal; `std::` exception containment | **done** (2026-07-30, +34 tests) |
| **#1883** | implementation, compatible | `FormattableString::ToString`: single-pass substitution; argument isolation | **done** (2026-07-30, +20 tests) |
| **#1884** | **`needs_user`** | adopt .NET's composite-format grammar in both engines: `{{`/`}}` escaping, stray-`}` rejection, missing-index rejection, alignment, and `{N:spec}` in `FormattableString` | **created, NOT implemented** — approval wording in §20.7 |

### 19.1 Implementation outcome (2026-07-30)

Both compatible tickets landed. Measured against the section 5 matrices, with
`build-probe/1881_prefix.log` as the before and `build-probe/1882_postfix.log` /
`build-probe/1883_postfix.log` as the after:

| Row | Before | After | Status |
|---|---|---|---|
| D1, D2 | did not terminate | `"{0}"`, `"[x{0}y]"` | fixed |
| K3 | did not terminate | `FormatException` at the reference bound | fixed |
| C1, C2 | `"XX"`, `"Y\|Y"` | `"{1}X"`, `"{1}\|Y"` | fixed |
| K1, K2 | `"FF/FF"`, `"255/255"` | `"FF/255"`, `"255/FF"` | fixed |
| K4 | spurious `FormatException` | `"{1}{0}"` | fixed |
| E1, E4 | `std::out_of_range` escaped | `FormatException("Format specifier was invalid.")` | fixed |
| E2 | `std::invalid_argument` escaped | `"42"` | fixed |
| E3, E5 | UBSan-confirmed UB | `FormatException` | fixed |
| J1 | `"second"` | `"{1}"` | fixed |
| G2 | index message | `"Input string was not in a correct format."` | fixed |
| A1–A3, B1, B2, F1, J2–J4, J6, J7 | — | **unchanged, pinned by test** | **#1884** |

The section 9.3 invariant holds: of the 28 measured rows across both engines,
the only ones whose result changed are the twelve above, every one of which was
undefined, non-terminating, a leaked `std::` exception, a silently wrong value,
or a spurious failure. Seven of `FormattableString`'s eight rows and thirteen of
`String::Format`'s twenty are byte-identical before and after.

Two implementation decisions departed from the letter of this plan and are
recorded rather than silently absorbed:

1. **Section 11's "a tail containing any non-digit (including `-`) means the
   specifier is not a standard one" was too coarse.** It would have changed
   `"{0:D-3}"` from `"007"` to `"7"` — a currently-succeeding call. The
   implementation instead reproduces `std::stoi`'s **prefix** semantics (optional
   sign, digits, trailing junk ignored) so every successful specifier keeps its
   exact text, and applies the reference's digit bound to the magnitude. A sign
   whose magnitude exceeds the bound, such as `"{0:D-2147483648}"`, is therefore
   rejected with `FormatException` rather than emitted plainly — an outcome that
   replaces undefined behaviour, so it is compatible either way.
2. **`std::bad_alloc` needed containing too.** A width the reference *accepts*
   can still fail to allocate. Letting `std::bad_alloc` escape would reintroduce
   exactly the defect class E1/E2/E4 represent, so an allocation failure inside a
   specifier now raises `System::OutOfMemoryException` — .NET's own outcome. This
   was not in the plan as written.

**#1884 is not implemented by this batch under any circumstance.** Section 20
holds its exact approval wording.

---

## 20. Approval decision record — #1884

### 20.1 Exact before/after behaviour

| # | Call | Today | After approval | .NET |
|---|---|---|---|---|
| 1 | `String::Format("{{0}}", 42)` | throws `FormatException` | `"{0}"` | `"{0}"` |
| 2 | `String::Format("{{}}", 42)` | throws `FormatException` | `"{}"` | `"{}"` |
| 3 | `String::Format("{{{0}}}", 42)` | throws `FormatException` | `"{42}"` | `"{42}"` |
| 4 | `String::Format("a}}b", 42)` | `"a}}b"` | `"a}b"` | `"a}b"` |
| 5 | `String::Format("value}", 42)` | `"value}"` | throws `FormatException` | throws |
| 6 | `String::Format("a}b{0}", 42)` | `"a}b42"` | throws `FormatException` | throws |
| 7 | `String::Format("{0,6}\|", 42)` | `"42\|"` | `"    42\|"` | `"    42\|"` |
| 8 | `String::Format("{0,-6}\|", 42)` | `"42\|"` | `"42    \|"` | `"42    \|"` |
| 9 | `FormattableString("{{0}}", {"v"}).ToString()` | `"{value}"`-shape (`"{v}"`) | `"{0}"` | `"{0}"` |
| 10 | `FormattableString("{1}", {"only"}).ToString()` | `"{1}"` | throws `FormatException` | throws |
| 11 | `FormattableString("value}", {"a"}).ToString()` | `"value}"` | throws `FormatException` | throws |
| 12 | `FormattableString("{0,6}", {"a"}).ToString()` | `"{0,6}"` | `"     a"` | `"     a"` |
| 13 | `FormattableString("{0:D4}", {"a"}).ToString()` | `"{0:D4}"` | `"a"` | `"a"` |
| 14 | `String::Format("{0", 42)` message | (corrected by #1882) | + "Failure to parse near offset N." | offset+reason form |

### 20.2 Affected methods

All 22 `String::Format` overloads, both `FormattableString::ToString` overloads,
`FormattableString::Invariant`/`CurrentCulture`, and transitively the 11
`StringBuilder::AppendFormat` and 11 `Console::Write`/`WriteLine` format
overloads. **46 public entries.**

### 20.3 Source and ABI consequences

**None.** No signature, `noexcept` specification, virtual, vtable slot, data
member or layout changes. This is a pure observable-behaviour change:
`FormattableString.hpp` requires a downstream recompile because the body is
inline.

### 20.4 Migration impact

Rows 5, 6, 10 and 11 turn currently-succeeding calls into `FormatException`.
Any caller that formats a literal `}` — a JSON-ish or brace-heavy template — is
affected and must double it to `}}`. Rows 4, 7, 8, 12 and 13 change successful
output, so any caller asserting on the exact string is affected. This is the
migration cost the approval buys .NET parity with; it is why the change is
gated and not folded into #1882/#1883.

### 20.5 Test vectors

The 14 rows of 20.1 verbatim, plus: `"{{"` alone, `"}}"` alone, `"{0}}}"`,
`"{0,}"`, `"{0,-}"`, `"{0:{1}}"` (brace inside a specifier), `"{1000000}"`
(index limit), `"{0,1000000}"` (width limit), and every row of section 14.1's
"Preserved grammar" group inverted.

### 20.6 Rollback

Single-commit revert. The gated grammar lands behind no flag and shares no state
with #1882/#1883, both of which remain correct without it.

### 20.7 Exact approval wording requested

> **Approve changing `System::String::Format` and
> `System::FormattableString::ToString` — and therefore
> `System::Text::StringBuilder::AppendFormat` and `System::Console::Write` /
> `WriteLine` — to .NET's composite-format grammar.**
>
> This makes `{{` and `}}` produce single literal braces, makes an unescaped
> `}` outside a format item a `System::FormatException`, makes a format item
> whose index exceeds the argument count a `System::FormatException` in
> `FormattableString` (it already is one in `String::Format`), and makes the
> `,alignment` component pad the substituted text.
>
> **It is a behaviour change, not an API change:** no signature, `noexcept`
> specification, virtual, vtable slot or data member changes, and no consumer
> source edit is required to keep compiling.
>
> **It will break callers at run time** in two ways: a format string containing
> a literal unescaped `}` starts throwing instead of returning that `}`
> (fourteen exact rows in §20.1), and a format string using `{{`, `}}` or
> `{N,width}` starts returning different text.
>
> Approving this authorises **only** the grammar rows in §20.1. It does not
> authorise culture behaviour, `ICustomFormatter`, custom numeric format
> strings, or any change to `String::Format`'s overload set.
