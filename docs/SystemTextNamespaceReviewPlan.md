<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Text` namespace review and remediation plan

Ticket **#2006**, written 2026-08-03 on branch
`feature/remediation-batch-system-text-review`.

This is the fifth namespace review in the post-audit remediation programme, after
`System::Threading` (#1950), `System::Threading::Tasks`/`Channels` (#1964),
`System::Runtime` (#1972) and `System::Uri` (#1987). It follows the same
contract: **every confirmed finding in the namespace gets exactly one
disposition, no finding disappears between the audit index and this plan, and
every premise is re-measured against the shipped library before it is relied
upon.**

**Nothing in §§1–19 is implemented by writing them.** The measured
before-matrix is `build-probe/2006_probe1_before.log`, reproducible with
`build-probe/2006_probe1_text_boundaries.cpp`. Sections numbered 20 and above
record what each implementation ticket actually did.

**No `SR-AUD-*` identifier is issued by this review.** Audit numbering stays
frozen at **364**. Defects this review found that the audit does not name are
recorded in §16 and are carried by ordinary ticket numbers only.

---

## 1. Why `System::Text` is next

Selected from tracked state, not alphabetically and not by raw count:

| Source | What it says |
|---|---|
| `NEXT.md` §10 "Next recommended work" (the `System::Uri` follow-up handoff) | **1. `System::Text` — 14 open findings … now the largest un-reviewed namespace queue. A fresh context should start with its namespace review, in the style of #1950/#1964/#1972/#1987.** |
| `audit/AUDIT_FINDINGS_INDEX.md` | 14 rows whose owning report lives under `modules/text/`: **SR-AUD-286 … SR-AUD-299**, contiguous, **all `confirmed`**, none `remediated`, none carrying the `confirmed (design-complete)` qualifier |
| `plan.sqlite3` | no open ticket references any of the fourteen; the four `todo` rows are #1963, #1985, #1986 and #2005, none of them `System::Text` |
| `docs/` | **no** `System::Text` design or review document exists. `docs/TextSubsetCompatibilityDecision.md` is **not** one — despite its name it is the #1927/#1928/#1929 *numeric and date/time* text-subset packet and explicitly puts `Encoding` **out of scope** (its §1, "Deliberately out of scope … `Convert::To*`, `Uri` escaping, `Encoding`, and every non-numeric, non-date/time text API") |

Verified against the index on 2026-08-03: **364 findings, 104 `remediated`, 236
`confirmed`, 24 `confirmed (design-complete)`** — i.e. **260 confirmed** in the
sense the programme counts, exactly the totals the previous handoff records. The
handoff is therefore not stale and `System::Text` is the authoritative next
namespace.

### 1.1 What "the `System::Text` namespace" means here

The C++ namespace `System::Text` is spread across **three** CMake components.
This review owns **one** of them:

| Component | Contents | In this review? |
|---|---|---|
| **`Text`** (`modules/text/`) | `Encoding` and the six concrete encodings, `Encoder`/`Decoder`, the fallback classes, `Rune`, the rune enumerators, `StringBuilder`, `CompositeFormat`, `Ascii`, `Unicode::Utf8`/`Utf16`/`UnicodeRange(s)`, `Encodings::Web` | **yes** — all 14 findings live here |
| **`Text.Json`** (`modules/text-json/`) | `System::Text::Json` | **no** — a distinct namespace with its own open family (#1888/#1889/#1894/#1896, CCF-019), already tracked |
| **`Text.RegularExpressions`** (`modules/text-regular-expressions/`) | `System::Text::RegularExpressions` | **no** — no confirmed finding maps to it |

`System::Text::NormalizationForm` is declared in **`modules/core`**
(`modules/core/include/System/Text/NormalizationForm.hpp`), not in `modules/text`.
It is inventoried in §2 and dispositioned in §6.

---

## 2. Scope and file inventory

Measured from the repository, not assumed. `modules/text` is a `STATIC` target
with `PUBLIC_DEPENDENCIES Buffers Core.Base` and no private or test dependency.

### 2.1 Public headers (27 files, 3,179 lines)

| File | Lines | Public surface | Findings |
|---|---|---|---|
| `System/Text/Encoding.hpp` | 124 | `Encoding`: 7 factories + `Default()`, `GetBytes`, 2 × `GetString`, `GetByteCount`, `GetCharCount`, 4 name/code-page/single-byte properties, 2 fallback getters, **2 fallback setters**, `Equals`/`==`/`!=`/`GetHashCode` | 286, 287, 288, 290, 299 |
| `System/Text/ASCIIEncoding.hpp` | 38 | `GetBytes`, `GetString`, 3 property overrides | 286, 292 |
| `System/Text/UTF8Encoding.hpp` | 39 | ctor, `GetBytes`, `GetString`, name | 286, 287, 288 |
| `System/Text/Latin1Encoding.hpp` | 42 | `GetBytes`, `GetString`, 3 properties — **all inline** | 286, 289 |
| `System/Text/UnicodeEncoding.hpp` | 205 | 2 ctors, `GetBytes`, `GetString`, name/code page, `getIsBigEndianProperty`, `getByteOrderMarkProperty` — **all inline**; private `readUnit`/`decodeUtf8`/`encodeUtf8` | 286, 291, 292, 293 |
| `System/Text/UTF32Encoding.hpp` | 194 | same shape as UTF-16 — **all inline** | 286, 291, 292, 293 |
| `System/Text/UTF7Encoding.hpp` | 299 | 2 ctors, `GetBytes`, `GetString`, properties — **all inline**; **the only encoding that validates its raw range** | 292 |
| `System/Text/Encoder.hpp` | 41 | ctor, `GetBytes`, `GetByteCount`, `Reset` | — (forwards) |
| `System/Text/Decoder.hpp` | 43 | ctor, 2 × `GetString`, `Reset` | 286 (forwards) |
| `System/Text/EncoderFallback.hpp` | 198 | `EncoderFallbackException`, `EncoderFallback` (+2 statics), `EncoderFallbackBuffer`, replacement/exception pairs | 292 |
| `System/Text/DecoderFallback.hpp` | 202 | `DecoderFallbackException`, `DecoderFallback` (+2 statics), `DecoderFallbackBuffer`, replacement/exception pairs; **`GetFallbackString(const bytecs*, intcs)` is a raw-pointer public virtual** | 286 |
| `System/Text/EncodingInfo.hpp` | 46 | ctor, 3 properties, `GetEncoding` | 299 |
| `System/Text/EncodingProvider.hpp` | 38 | 3 pure/virtual members | 299 (through consumers) |
| `System/Text/Rune.hpp` | 186 | 3 ctors, 6 properties, `IsValid`, `TryGetRuneAt`, 7 static classifiers, `ToUpper`/`ToLower`, `ToString`, 6 comparisons, `ReplacementChar` | 294 |
| `System/Text/RunePosition.hpp` | 145 | enumerator over UTF-8 byte offsets | 294 (delegates) |
| `System/Text/StringRuneEnumerator.hpp` | 84 | owning rune enumerator over a string | — |
| `System/Text/StringBuilderRuneEnumerator.hpp` | 72 | snapshotting rune enumerator | 296 (delegates) |
| `System/Text/StringBuilder.hpp` | 323 | 2 ctors, 10 × `Append`, 2 × `AppendLine`, 11 × `AppendFormat`, `Insert`, `Remove`, `Replace`, `Clear`, `ToString`, `Length` get/set, `Empty`, **`CopyTo`** | 295, 296 |
| `System/Text/CompositeFormat.hpp` | 74 | `Parse`, `getFormatProperty`, `getMinimumArgumentCountProperty`; private `countPlaceholders` | 298 |
| `System/Text/Ascii.hpp` | 193 | byte/UTF-16 validation, case conversion, bounded-destination results, trimming | — |
| `System/Text/Unicode/Utf8.hpp` | 292 | validation and UTF-8↔UTF-16 transcoders with `OperationStatus` | — |
| `System/Text/Unicode/Utf16.hpp` | 50 | linear UTF-16 validity scanner | — |
| `System/Text/Unicode/UnicodeRange.hpp` | 57 | BMP range value type | — |
| `System/Text/Unicode/UnicodeRanges.hpp` | 348 | generated BMP block accessors | — |
| `System/Text/Encodings/Web/HtmlEncoder.hpp` | 44 | 2 × `Encode`, `Default` | 297 |
| `System/Text/Encodings/Web/JavaScriptEncoder.hpp` | 41 | `Encode`, `Default` | 297 |
| `System/Text/Encodings/Web/UrlEncoder.hpp` | 58 | `Encode`, **`Decode`** (no .NET counterpart), `Default` | 297 |

Declared in **`modules/core`**, not here:
`System/Text/NormalizationForm.hpp` (enum only; `String::Normalize` is absent —
§6).

### 2.2 Implementation files (4 files, 483 lines)

| File | Lines | Findings |
|---|---|---|
| `src/System/Text/Encoding.cpp` | 74 | 286, 288 |
| `src/System/Text/ASCIIEncoding.cpp` | 101 | 286, 292 |
| `src/System/Text/UTF8Encoding.cpp` | 103 | 286, 287 |
| `src/System/Text/StringBuilder.cpp` | 205 | 295, 296 |

Everything else in the component is **header-inline**, which is the single most
important ABI fact in §9: seven of the eleven bodies a repair must touch are
inline, so a consumer must **recompile**, never merely relink.

### 2.3 Tests (6 files, 1,840 lines, one executable)

`SharpRuntimeTests_Text`: `EncodingTests.cpp` (100), `EncodingWebTests.cpp`
(185), `StringBuilderTests.cpp` (631), `TextNamespaceTests.cpp` (442),
`UTF7EncodingTests.cpp` (77), `UnicodeTests.cpp` (405). Plus the integration
suite `tests/integration/System/Text/TextRemainingTests.cpp`.

### 2.4 Cross-module callers

`modules/text` is consumed by `Text.Json`, `Net.Http`, `Xml`, `IO` and `Console`
(through `StringBuilder` and `Encoding::UTF8()`); no first-party caller
constructs an `EncodingInfo`, an `EncodingProvider`, a `Latin1Encoding`, a
`UTF32Encoding` with a non-default BOM setting, or calls
`Encoding::GetCharCount` — measured by grep over `modules/` and `tests/`. That
absence is what makes §8's compatible/gated split defensible: the gated repairs
are gated on **external** consumers only.

---

## 3. Confirmed finding inventory — all 14, with the measured current behaviour

Every row was re-measured on 2026-08-03 against the shipped
`libsharp_runtime_text.a`. "Measured" quotes `2006_probe1_before.log`.

| ID | Sev | Cause | Measured now | Disposition |
|---|---|---|---|---|
| **286** | high | **T-A** | `ASCIIEncoding::GetString(p,-1,1)` reads the byte before the buffer and returns `"?"`; `Latin1Encoding::GetString(p,-4,4)` returns the four guard bytes `aaabacad`; base and UTF-16/UTF-32 likewise. Null data + positive count is **SIGSEGV** in Latin-1/UTF-16/UTF-32. Negative count throws **`std::length_error`** out of Latin-1. `index+count` overflow is **SIGSEGV** in UTF-16. `Decoder::GetString(vec,-1)` reads **five** bytes from a four-byte vector | **#2007**, compatible |
| **287** | high | **T-B** | `setDecoderFallbackProperty(nullptr)` then decoding `0xFF` is **SIGSEGV**; so is the **encoder** direction, which the finding does not name | **#2008**, compatible |
| **288** | high | **T-G** | `Encoding::UTF8()` returns the same object every call; mutating its decoder fallback changes what every other caller decodes | **#2013**, DESIGN, blocked |
| **289** | med | **T-H** | `Latin1.GetBytes(u8"é")` → `c3a9` (want `e9`); `Latin1.GetString({e9})` → `e9` (want `c3a9`) | **#2014**, DESIGN, blocked |
| **290** | med | **T-I** | `UTF8.GetCharCount(U+1F600)` → `4`; the managed answer is `2` | doc half **#2012** compatible; semantic half **#2015**, DESIGN, blocked |
| **291** | med | **T-J** | `Encoding::UTF32()->GetBytes("A")` → `fffe000041000000` — the **default factory** emits a BOM as payload, while `Encoding::Unicode()` does not | **#2016**, DESIGN, blocked |
| **292** | med | **T-K** | a configured **exception** fallback never throws in UTF-16/UTF-32/ASCII (it does in UTF-8); a configured `"!"` encoder replacement is ignored by ASCII, which hard-codes `'?'` | **#2017**, DESIGN, blocked |
| **293** | med | **T-K** | `UTF16LE.GetString(3 bytes)` → `41`, trailing byte dropped; `UTF32LE.GetString(6 bytes)` → `41`, two bytes dropped | **#2017**, DESIGN, blocked |
| **294** | med | **T-L** | `Rune(U+00E9).IsLetter` → `false`, `ToUpper` unchanged, `IsDigit(U+0660)` → `false` — while `IsWhiteSpace(U+00A0)` → **`true`** | **#2018**, DESIGN, blocked |
| **295** | high | **T-C** | `CopyTo(0, dest, INTCS_MIN, 0, 1)` **returns normally and writes** — the signed overflow does not merely happen, it **defeats the bounds check** | **#2009**, compatible |
| **296** | med | **T-I** | `StringBuilder(u8"éA").Length` → `3`; `Remove(1,1)` yields `c341`, ill-formed UTF-8 | doc half **#2012** compatible; semantic half **#2015**, DESIGN, blocked |
| **297** | med | **T-E**/**T-M** | `UrlEncoder::Decode("%zz")` throws **`std::invalid_argument`**; `"%-1"` → byte `ff`, `"% 1"` → `01`, `"%+f"` → `0f`; `HtmlEncoder::Encode("abc",-1,2)` throws **`std::out_of_range`**. Default HTML/JS encoders pass `é` through unchanged | diagnostics half **#2011** compatible; policy half **#2019**, DESIGN, blocked |
| **298** | med | **T-D**/**T-N** | `Parse("{2147483648}")` throws **`std::out_of_range`**; `Parse("{2147483647}")` returns **`minArgCount = -2147483648`**; `{0,not-a-width}` and `{0,-}` are accepted | diagnostics half **#2010** compatible; grammar half **#2020**, DESIGN, blocked |
| **299** | med | **T-F** | `EncodingInfo(20127,"us-ascii").GetEncoding()->getCodePageProperty()` → `65001`, and it encodes `é` as UTF-8 | **#2021**, DESIGN, blocked |

**Fourteen findings in, fourteen out.** None is a duplicate, none is a
false premise in full, none is already remediated, and none receives a
"no action" disposition.

---

## 4. Corrections to the audit record

The historical audit text is preserved everywhere. These are appended
corrections, not rewrites.

### 4.1 SR-AUD-286's site list is incomplete, and one named member is not a member

The finding's summary reads *"Raw `GetString` paths do not consistently reject
signed index/count before pointer arithmetic; ASan confirms
`ASCIIEncoding::GetString(data, -1, 1)` stack-buffer-underflow."* Measured:

1. **`UTF8Encoding::GetString(p, -1, 1)` returns `""`, not an out-of-bounds
   read** — the per-file report `src/System/Text/UTF8Encoding.cpp.audit.md`
   states *"a negative index is converted to `size_t`"*, which is true, but for
   `index = -1, count = 1` the derived `end = i + count` wraps to `0`, so the
   loop never runs. UTF-8 **is** reachable for `count < |index|`
   (e.g. `index = -4, count = 2`), so it stays a member — but its named
   reproduction does not reproduce.
2. **Three failure modes the finding does not name, all reachable from an
   ordinary public call:**
   - **null `data` with a positive `count` is a segmentation fault** in
     `Latin1Encoding`, `UnicodeEncoding` and `UTF32Encoding`, none of which has
     a null check at all (base, ASCII and UTF-8 do);
   - **a negative `count` throws `std::length_error`** out of
     `Latin1Encoding::GetString` — a `std::` exception escaping a
     `System`-shaped public API, the same class CCF-012 removed from
     `String::Format`;
   - **`index + count` is a signed `intcs` addition** in `UnicodeEncoding`
     (`intcs end = index + count`), so `GetString(p, INTCS_MAX, 4)` is
     undefined behaviour and was measured as a segmentation fault — a CCF-004
     shape the finding does not mention.
3. **Two entry points beyond the six `GetString` overrides:**
   `Decoder::GetString(const std::vector<bytecs>&, index, count = -1)` derives
   `len = size - index`, so a negative index makes the length **longer than the
   vector** — measured, `Decoder(Latin1).GetString(vec, -1)` returned five bytes
   from a four-element vector; and `Encoding::GetCharCount` inherits the whole
   defect by delegating to `GetString`, measured returning `1` for
   `(p, -1, 1)`.
   `DecoderExceptionFallback::GetFallbackString(nullptr, -1)` — a raw-pointer
   **public virtual** — throws `std::length_error` from
   `std::vector(ptr, ptr + count)`.
4. **`UTF7Encoding` already implements the correct policy** and is therefore
   *not* a defect site, only a member of the inconsistency: it throws
   `ArgumentOutOfRangeException("index")`, `ArgumentOutOfRangeException("count")`
   and `ArgumentNullException("data")`, and returns empty for `count == 0`. The
   per-file report says its checks are *"much stronger than other encodings"*;
   measured, they are exactly the policy the other six need, which is why #2007
   adopts UTF-7's own shape rather than inventing one.

### 4.2 SR-AUD-287 names one direction of a two-direction defect

The index row says *"Public fallback setters accept null; decoding one malformed
UTF-8 byte afterwards reaches an ASan/UBSan-confirmed null virtual call"*, and
`UTF8Encoding.cpp.audit.md` names only `setDecoderFallbackProperty`. Measured,
**`setEncoderFallbackProperty(nullptr)` followed by
`GetBytes("\xFF")` is an identical segmentation fault.** Repairing one setter
would leave the other.

### 4.3 SR-AUD-295 understates its own consequence

`StringBuilder.cpp.audit.md` says *"`CopyTo` calculates `destinationLength -
count` before validating a negative capacity. The UBSan probe with `INT_MIN`
capacity and count 1 reports signed overflow at line 146."* That is accurate but
stops one step short. Measured: because `INT_MIN - 1` wraps to `INT_MAX`, the
comparison `destinationIndex > destinationLength - count` evaluates
`0 > INT_MAX`, which is **false**, so the guard **passes** and `std::copy`
executes. The undefined arithmetic is not a side effect of the bounds check — it
**defeats** the bounds check, turning an invalid capacity into a silent write
into caller memory. That reads the severity **up**, not down; it is already
filed `high`.

### 4.4 SR-AUD-298 names the smaller of two defects in the same expression

The finding names `std::out_of_range` for `{2147483648}` and the acceptance of
`{0,not-a-width}`. Measured, the same `std::stoi(idxStr)` call also produces a
**silent wrong result** one value earlier: `Parse("{2147483647}")` succeeds and
returns `getMinimumArgumentCountProperty() == -2147483648`, because
`countPlaceholders` finishes with `return maxIdx + 1` on a signed `intcs` that
is already `INT32_MAX`. A **negative minimum argument count** is worse than a
leaked native exception, and it is undefined behaviour reaching a public API
from public input. It is inseparable from the repair (the same expression) and
is therefore folded into #2010 rather than filed separately — the #1931/#1929a
precedent.

### 4.5 SR-AUD-297's `Decode` defect is three defects

`UrlEncoder.hpp.audit.md` names `std::invalid_argument` for `%zz`. Measured, the
`std::stoi(value.substr(i+1,2), nullptr, 16)` call also **accepts text that is
not two hexadecimal digits and silently produces a wrong byte**:
`"%-1"` → `0xFF` (a negative value cast to `char`), `"% 1"` → `0x01` (`stoi`
skips leading whitespace), `"%+f"` → `0x0F`. And the finding does not mention
`HtmlEncoder::Encode(value, startIndex, characterCount)` at all in the index
row, although the per-file report does: a negative `startIndex` becomes
`18446744073709551615` and `std::substr` throws `std::out_of_range`, while an
over-large `characterCount` is **silently clamped** instead of rejected.

### 4.6 CCF-012's exclusion list contains a false premise

`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`, CCF-012, closes with a list of things
*"deliberately not members and not closed by this work"*, whose last entry is:

> *"and `System.Text.CompositeFormat`, which is not ported."*

**It is ported.** `modules/text/include/System/Text/CompositeFormat.hpp` has
existed throughout, it has its own audit report and its own confirmed finding
(SR-AUD-298), and it contains a **third** hand-written composite-format grammar
that #1882/#1883/#1884 did not touch. CCF-012's own warning — *"altering just
one API preserves divergent brace rules"* — therefore still applies to this
namespace: after #1884, `String::Format` and `FormattableString::ToString` share
`System::detail::runCompositeFormat`, and `CompositeFormat::Parse` does not.
This correction is what makes #2020 a *family* ticket rather than a local one.

### 4.7 Two audit statements that are correct and are **not** corrected

Recorded so a later reader does not re-open them:

- `Rune.hpp.audit.md` says the ASCII-only classifiers are *"deliberate"*. The
  header agrees: seven of the eight doc-comments already say "ASCII range only".
  The finding is still real (the members carry .NET's names and .NET's
  Unicode-category contract), but the port is **not** silently wrong, and the
  disposition in §8 reflects that.
- `EncodingInfo.hpp`'s `GetEncoding` doc-comment already states the reduction
  verbatim, including that *"no `EncodingInfo` instances are actually constructed
  anywhere in this codebase today"* — re-verified true on 2026-08-03.

---

## 5. Root causes

Grouped by structural cause, not by file or line.

### T-A — the raw `(data, index, count)` decode boundary has no single policy

**Members: SR-AUD-286.** Seven public decode entries take a bare pointer plus two
signed lengths. One of them (`UTF7Encoding`) validates all three; six do not,
each in a different way, and two further entry points (`Decoder`'s vector
overload, `Encoding::GetCharCount`) inherit whatever the encoding does. The
failure modes are four distinct ones — out-of-bounds read, null dereference,
`std::` exception escape, signed overflow — because there is no policy, not
because six authors each made the same mistake. **Repair: one shared validator,
adopted by all seven, with UTF-7's exact exception shape.** Compatible.
Shares CCF-004's arithmetic shape and CCF-005's boundary-validation shape; it
is a **new occurrence of both, not a new family**.

### T-B — a nullable public setter with a non-nullable use site

**Members: SR-AUD-287.** `setDecoderFallbackProperty`/`setEncoderFallbackProperty`
store a `shared_ptr` that every conversion path dereferences without checking.
This is CCF-011's shape one level up: CCF-011 is an *empty callable* crossing a
public boundary; this is an *empty owner*. **Repair: reject null at the setter,
as .NET's `Encoding.DecoderFallback` setter does.** Compatible.

### T-C — signed capacity arithmetic performed before its own validation

**Members: SR-AUD-295.** `destinationLength - count` on `intcs`. **CCF-004,
fourth module.** Repair: validate, then compare in a width that cannot overflow.
Compatible.

### T-D / T-N — a fourth hand-written composite-format grammar

**Members: SR-AUD-298.** `CompositeFormat::countPlaceholders` is an independent
partial re-implementation of the grammar `System::detail::runCompositeFormat`
already states once. **CCF-012, and the reason CCF-012 cannot be closed by
#1884 alone (§4.6).** Splits into a diagnostics half (T-D, compatible: no
`std::` exception may escape, and no signed overflow may reach the returned
count) and a grammar half (T-N, gated: adopt the shared scanner and its index,
alignment and specifier rules).

### T-E / T-M — Web helpers that delegate validation to `std::` primitives

**Members: SR-AUD-297.** `std::stoi` and `std::string::substr` are used as
parsers on public input. Same cause as CCF-012's `std::stoi` bullet, in a
different component. Splits into a diagnostics half (T-E, compatible: strict
two-hex-digit parsing and managed range diagnostics) and a **policy** half
(T-M, gated: the default encoders' allow-list).

### T-F — metadata and behaviour are allowed to disagree

**Members: SR-AUD-299.** `EncodingInfo` carries a code page and returns an
encoding that ignores it. Gated, because every repair changes what the returned
object does.

### T-G — one process-wide mutable object behind a factory

**Members: SR-AUD-288.** Structurally **CCF-009** (`Random::Shared`,
`Guid::NewGuid`): a shared singleton with publicly mutable state and no
ownership boundary. Reusing CCF-009's policy rather than opening a new family is
explicit here. Gated, because .NET's answer (read-only factory encodings) makes
a currently-succeeding setter throw — and this repository's **own test suite**
mutates `Encoding::UTF8()`'s fallback (`TextNamespaceTests.cpp`).

### T-H — a code-page encoding implemented over storage bytes

**Members: SR-AUD-289.** `Latin1Encoding` is the only encoding that does **not**
decode the UTF-8 storage representation into scalars first; `ASCIIEncoding`,
`UnicodeEncoding` and `UTF32Encoding` all do. Gated: it changes produced bytes.

### T-I — "character" positions are storage-byte positions

**Members: SR-AUD-290, SR-AUD-296.** One cause, two surfaces: `GetCharCount`
counts UTF-8 bytes, and `StringBuilder`'s `Length`/`Insert`/`Remove`/`CopyTo`
index UTF-8 bytes, so a mutation can split a character. Disclosure is
compatible; changing the unit is not.

### T-J — the preamble is serialized as payload

**Members: SR-AUD-291.** `GetBytes` prepends the configured BOM; .NET states
that it does not, and exposes `GetPreamble`/`Preamble` instead. Gated twice
over: it changes emitted bytes **and** the natural repair adds a virtual to
`Encoding`, which moves the vtable.

### T-K — the fallback objects are configuration without effect

**Members: SR-AUD-292, SR-AUD-293.** One cause: only `UTF8Encoding` routes
ill-formed input through the configured fallback. The others substitute a
hard-coded `'?'` or `U+FFFD`, and the fixed-width decoders do not even reach a
substitution for a truncated trailing unit — they discard it. Gated: it is a
fallback-policy change, and it makes a configured exception fallback start
throwing.

### T-L — Unicode classification reduced to ASCII, inconsistently

**Members: SR-AUD-294.** `Rune::IsLetter`/`IsDigit`/`IsUpper`/`IsLower`/
`ToUpper`/`ToLower` are ASCII-only and say so; `Rune::IsWhiteSpace` is
Unicode-aware. Gated and large (it needs Unicode tables); shares CCF-015's
subject ("UTF-8 public text cannot use C-locale byte classification") without
being a CCF-015 member, because `Rune` classifies **scalars**, not bytes.

---

## 6. Findings and surfaces that are *not* in this namespace's queue

| Item | Why not |
|---|---|
| SR-AUD-015 / CCF-012 in `modules/core` | already `remediated` by #1882/#1883/#1884; only the CompositeFormat gap (§4.6) belongs here |
| SR-AUD-048 / CCF-015 | `MemoryExtensions` and `ArgumentException` in `modules/core`; `Ascii.hpp`'s six-character trim is a *deliberately* byte-oriented ASCII API and no finding maps to it |
| `System::Text::Json` (#1888/#1889/#1894/#1896) | a different namespace with its own tracked family |
| `System::Text::RegularExpressions` | no confirmed finding |
| `System::Text::NormalizationForm` | the enum exists in `modules/core`; no `Normalize`/`IsNormalized` surface exists to be wrong, so there is nothing to remediate — normalization is an **absent** feature, recorded in §15 |
| `Buffers::Text::Base64`/`Base64Url`/`Utf8Formatter`/`Utf8Parser` | `modules/buffers`, covered by CCF-013 and `docs/Base64FamilyPlan.md` |
| `IO::TextReader`/`TextWriter` | `modules/io`; #1809 and `docs/TextWrapperInputContractPlan.md` |
| `Globalization::TextInfo`/`TextElementEnumerator` | `modules/globalization` |
| `Ascii.hpp`, `Unicode::Utf8/Utf16/UnicodeRange(s)`, `StringRuneEnumerator` | audited, **no evidence-backed finding confirmed** |

---

## 7. Reference evidence actually available, per repair

`/rv/tmp/runtime/src/libraries/` was **re-verified absent** on 2026-08-03 (so is
`/rv`), and **no .NET runtime is installed** in this container. Every repair
below therefore states what evidence it rests on inside the repository.

| Cause | Evidence available here | Sufficient? |
|---|---|---|
| **T-A** | `UTF7Encoding.hpp:227-241` — this repository's **own** validated raw decode entry, with its exception types, parameter names and `count == 0` ordering. `StringBuilder.cpp`, `Span.hpp`'s `detail::checkedSpanLength` and `BitConverter`'s `validateDecodeRange` (#1851) give three more in-repo statements of the same policy | **yes** — the policy is transcribed from the port, not recalled from .NET |
| **T-B** | the `Encoding.hpp` doc-comments promise a fallback object is used; #1953's null-argument precedent gives the exception shape | **yes** |
| **T-C** | the four sibling guards in the same function already use `ArgumentOutOfRangeException(name, "Non-negative number required.")` | **yes** |
| **T-D** | `System/detail/CompositeFormat.hpp` (#1884, approved) states the index limit and the diagnostic text; **T-D deliberately does not adopt the limit** (§8.2) | **yes for the diagnostics half** |
| **T-E** | RFC 3986 §2.1 defines a percent-encoded octet as `%` + two `HEXDIG`; `UrlEncoder::Encode` in the same file emits exactly that | **yes** |
| **T-F, T-G, T-H, T-I, T-J, T-K, T-L, T-M, T-N** | the .NET contract each needs — read-only factory encodings, ISO-8859-1 mapping, UTF-16 char counts, `GetPreamble`, fallback dispatch order and `bytesUsed`/`charsUsed` semantics, Unicode category tables, the Basic-Latin allow-list — **cannot be established from this repository** | **no** — every one is gated on approval *and* on evidence, and §14 says so per ticket |

**No repair in §8's compatible column depends on a .NET behaviour that could not
be re-measured here.** That is the criterion that separates the two columns.

---

## 8. Compatible versus approval-sensitive classification

### 8.1 Compatible — implemented by this batch

| Ticket | Cause | Findings | What changes observably |
|---|---|---|---|
| **#2007** | T-A | 286 | invalid raw decode metadata throws instead of reading out of bounds, faulting, leaking `std::length_error`, or returning a meaningless empty string. **Every valid `(data, index, count)` triple returns byte-identical output.** |
| **#2008** | T-B | 287 | `setDecoderFallbackProperty(nullptr)`/`setEncoderFallbackProperty(nullptr)` throw `ArgumentNullException("value")` instead of arming a later segmentation fault |
| **#2009** | T-C | 295 | a negative `destinationLength` throws instead of silently passing the bounds check and writing |
| **#2010** | T-D | 298 (half) | `CompositeFormat::Parse` raises `System::FormatException` instead of `std::out_of_range`, and can no longer return a negative minimum argument count |
| **#2011** | T-E | 297 (half) | `UrlEncoder::Decode` accepts exactly two hexadecimal digits after `%` and raises `System::FormatException` otherwise; `HtmlEncoder::Encode(value, start, count)` raises `System::ArgumentOutOfRangeException` instead of `std::out_of_range`, and rejects an over-long count instead of clamping |
| **#2012** | T-I | 290, 296 (doc half) | **nothing executable** — the `Encoding`, `StringBuilder` and `Latin1Encoding` doc-comments stop promising managed character units they do not deliver |

Why each is compatible rather than gated, in one line: **#2007/#2009 change only
inputs that are currently undefined, faulting, or defined to a value with no
possible use; #2008 changes only an input that arms a later crash; #2010/#2011
replace a `std::` exception escaping a `System`-shaped API with the `System`
exception the API's own doc-comment already names, exactly as #1882 did;
#2012 changes no code.**

The three rows in #2007 that are *not* previously-undefined — a negative
`count`, a negative `index` with `count == 0`, and a null `data` with
`count == 0` — are tabulated explicitly in §9.3 rather than hidden, and the
first of the three is the one that makes the six encodings agree with the
seventh.

### 8.2 Approval-sensitive — designed here, not implemented

| Ticket | Cause | Findings | Gate |
|---|---|---|---|
| **#2013** | T-G | 288 | thread-safety guarantee + a currently-succeeding setter starts throwing |
| **#2014** | T-H | 289 | produced bytes change for all non-ASCII input |
| **#2015** | T-I | 290, 296 | the meaning of every public index/length in the namespace |
| **#2016** | T-J | 291 | emitted bytes change **and** `Encoding` gains a virtual (vtable) |
| **#2017** | T-K | 292, 293 | fallback policy; a configured exception fallback starts throwing |
| **#2018** | T-L | 294 | Unicode classification/casing policy + a large data table |
| **#2019** | T-M | 297 | the default Web encoders' allow-list; emitted text changes |
| **#2020** | T-N | 298 | accepted composite-format grammar narrows |
| **#2021** | T-F | 299 | `EncodingInfo::GetEncoding` returns a different object |

**#2010 deliberately does not adopt `kCompositeIndexLimit`.** Doing so would
reject `{1000000}`…`{2147483646}`, which `CompositeFormat::Parse` accepts today
with a defined, positive answer. That narrowing is #2020's, and #2010 stops
exactly one value short of it — it rejects only `{2147483647}` (currently
undefined behaviour producing a negative count) and anything larger (currently
a leaked `std::out_of_range`).

---

## 9. Compatibility proofs and the source / ABI / layout consequence matrix

### 9.1 Declarations

| Ticket | Signature | `noexcept` | virtual / vtable | data members | mangled names |
|---|---|---|---|---|---|
| #2007 | unchanged | none present, none added | unchanged | unchanged | one **new** free function in `System::Text::detail`; no existing name changes |
| #2008 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2009 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2010 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2011 | unchanged | unchanged | unchanged | unchanged | unchanged |
| #2012 | unchanged | unchanged | unchanged | unchanged | unchanged |

No member of `Encoding`, `StringBuilder`, `Rune`, `CompositeFormat` or any
encoding is added, removed, reordered or re-qualified by the compatible batch.
`sizeof`/`alignof` of every public type is pinned by permanent tests
(§11).

### 9.2 Recompilation

Seven of the eleven touched bodies are **inline in headers**
(`Latin1Encoding`, `UnicodeEncoding`, `UTF32Encoding`, `UTF7Encoding`,
`Decoder`, `DecoderFallback`, `CompositeFormat`, plus both Web encoders), so a
consumer of `modules/text` must **recompile**. That is the ordinary consequence
of an inline change, as with #1867/#1868/#1870/#1882. Relinking alone is
sufficient only for the four `.cpp` bodies. No consumer **source** edit is
required by any compatible ticket.

### 9.3 The complete observable-change table for #2007

The only compatible ticket that changes a currently-defined result. `p` is a
valid four-byte buffer.

| Call | Before | After |
|---|---|---|
| `GetString(p, 0, 4)` and every valid triple | correct text | **identical** |
| `GetString(p, 0, 0)` | `""` | `""` |
| `GetString(nullptr, 0, 0)` | `""` | `""` |
| `GetString(p, -1, 1)` | **out-of-bounds read** (ASCII/Latin-1/UTF-16/UTF-32/base) | `ArgumentOutOfRangeException("index")` |
| `GetString(nullptr, 0, 4)` | **SIGSEGV** (Latin-1/UTF-16/UTF-32) | `ArgumentNullException("data")` |
| `GetString(p, INTCS_MAX, 4)` | **SIGSEGV** (UTF-16), preceded by UBSan `signed integer overflow: 2147483647 + 4` at `UnicodeEncoding.hpp:102` | **still faults** — see the correction below; what changes is that the addition is now done in `std::size_t`, so UBSan is silent |
| `Latin1.GetString(p, 0, -1)` | **`std::length_error`** | `ArgumentOutOfRangeException("count")` |
| `GetString(p, 0, -1)` (base/ASCII/UTF-16/UTF-32) | `""` | `ArgumentOutOfRangeException("count")` — **the one row that narrows a defined result**, and the row that makes the six agree with `UTF7Encoding`, which already throws |
| `GetString(p, -1, 0)` | `""` | `ArgumentOutOfRangeException("index")` — second narrowed row |
| `UTF7Encoding::GetString(...)` | already throws | **identical**, now through the shared validator |
| `Decoder::GetString(vec, -1)` | reads past the vector | `ArgumentOutOfRangeException("index")` |
| `GetCharCount(p, -1, 1)` | `1`, from an out-of-bounds read | `ArgumentOutOfRangeException("index")` |

**Correction to this table, made by #2007's own measurement (2026-08-03).** The
`INTCS_MAX` row above was written before the after-probe ran, and predicted a
throw. **It does not throw, and it must not.** `INTCS_MAX` is a valid
*non-negative* index; a raw pointer carries no length, so no validator can know
the caller's buffer ends sooner. `build-probe/2007_asan_after.log` records the
case still faulting under ASan, deliberately. What #2007 removes is the
**undefined addition** — `build-probe/2007_asan_before.log` shows
`UnicodeEncoding.hpp:102:33: runtime error: signed integer overflow: 2147483647
+ 4 cannot be represented in type 'int'`, and the after run reports nothing at
that site. The residual out-of-range read is inherent to the signature, is
stated in every entry's doc-comment, and is **not** closed by this ticket. It is
recorded here rather than quietly dropped.

### 9.4 What no compatible ticket touches

Accepted text, produced bytes, decoded characters, fallback selection, fallback
output, BOM emission, code-page identity, `Equals`/`GetHashCode`, thread-safety
guarantees, Unicode classification, normalization, and the composite-format
grammar. Each of those is a §8.2 gate.

---

## 10. Downstream consumer impact

**Not estimated, by instruction.** `CNA` and `mobile-eggbert` were not read,
searched, built, tested or modified, and no filesystem search left this
repository. #1773 stays `blocked`. In-repository callers were measured (§2.4):
no first-party caller passes a negative index or count to any decode entry, sets
a fallback to null, calls `CompositeFormat::Parse`, calls `UrlEncoder::Decode`,
or calls `HtmlEncoder`'s three-argument overload — so **no in-repository call
site changes behaviour** under the compatible batch, which is what the whole
37-executable gate then confirms.

---

## 11. Test matrix

Permanent, add-only, in `modules/text/tests/System/Text/`. Every row is a
requirement on the owning ticket, not a suggestion.

| Area | Cases required |
|---|---|
| **#2007 raw range** | for **each** of the seven decode entries: empty (`count == 0`), one byte, valid full range, `index` at the last valid unit, negative `index` (−1 and `INTCS_MIN`), negative `count` (−1 and `INTCS_MIN`), null `data` with `count > 0`, null `data` with `count == 0`, `index == INTCS_MAX` with `count > 0`, exact exception type, exact `paramName`, exact message; plus `Decoder`'s two overloads and `GetCharCount`; plus `DecoderExceptionFallback::GetFallbackString(nullptr, -1)` |
| **#2007 no-corruption** | ASCII, multibyte UTF-8, BMP non-ASCII, supplementary scalars, embedded NUL, isolated high surrogate, isolated low surrogate, valid surrogate pair, truncated sequence, invalid continuation byte, overlong encoding, invalid scalar — each round-tripped through every encoding **before and after**, asserting byte-identical output |
| **#2008 fallbacks** | null decoder setter, null encoder setter, exact exception and `paramName`; a non-null setter still installs; the installed object is still the one used; repeated calls after the failure leave the previous fallback in place |
| **#2009 CopyTo** | `destinationLength` of `INTCS_MIN`, −1, 0; `count` 0 and 1; destination exactly large enough; destination one byte too small; valid copy unchanged; exact exception types and `paramName`s; a UBSan case |
| **#2010 CompositeFormat** | `{2147483647}`, `{2147483648}`, `{99999999999999999999}`, `{1500000}` (**still accepted**), `{0}`, `{{0}}`, `Hello {`, `a } b`, `{ 0 }`, `{-1}`, `{0,not-a-width}` (**still accepted**, pinning what #2020 would change), exact exception type and message |
| **#2011 Web** | `%41`, `%7A`, `%zz`, `%-1`, `% 1`, `%+f`, `%4` at end, `%` at end, `a+b`, empty, embedded NUL; `Encode(v,-1,2)`, `Encode(v,0,99)`, `Encode(v,1,2)`, `Encode(v,3,0)`, `Encode(v,0,0)`; exact exception types and `paramName`s |
| **#2012 docs** | none executable; the ticket instead pins the **current** measured units (`GetCharCount` = bytes, `StringBuilder(u8"éA").Length == 3`) so a later semantic change under #2015 cannot land silently |
| **layout pins** | `sizeof`/`alignof` of `Encoding`, `UTF8Encoding`, `UnicodeEncoding`, `UTF32Encoding`, `StringBuilder`, `Rune`, `CompositeFormat` |

Systematic generation is required where the matrix is a cross product: the
#2007 no-corruption block is generated over {7 encodings} × {12 input classes},
not hand-written per example.

---

## 12. Sanitizer matrix

| Sanitizer | Applies to | What it must show |
|---|---|---|
| **ASan** | #2007, #2009 | the out-of-bounds decode read and the `CopyTo` write are **present before** and **absent after**, with the changed production body compiled **into** the probe so the instrumented code is the production code |
| **UBSan** | #2007, #2009, #2010 | `signed-integer-overflow` at `UnicodeEncoding`'s `index + count`, at `StringBuilder.cpp`'s `destinationLength - count`, and at `CompositeFormat`'s `maxIdx + 1`, present before and absent after |
| **LSan** | #2007, #2008, #2011 | no leak when a conversion throws part-way, when a fallback setter throws, or when `Decode` throws mid-string |
| **TSan** | **not applicable to the compatible batch** — and this must be stated, not omitted: none of #2007–#2012 introduces shared state, an atomic, a lock or a cache. TSan is the **gate** for #2013 (T-G), where the existing race lives |

A sanitizer-clean result proves memory and arithmetic safety. It proves
**nothing** about Unicode or formatting parity; §11's before/after byte-identity
block is what carries that.

---

## 13. Recommended execution order

1. **#2006** — this plan (no code).
2. **#2007** (T-A) — the highest-severity cause, the widest blast radius, and
   the one every other decode-side repair must sit on top of.
3. **#2008** (T-B) — same file as #2007's base change, same review context.
4. **#2009** (T-C) — independent, self-contained, CCF-004.
5. **#2010** (T-D) — independent.
6. **#2011** (T-E) — independent.
7. **#2012** — last, because it must describe what #2007–#2011 left true.
8. **#2013 … #2021** — design records only; none is implemented without its
   §14 approval sentence.

#2007 and #2008 may share one commit (they edit the same two regions of
`Encoding.hpp`/`Encoding.cpp` in one run — the #1991/#1992 and #2000/#2001/#2002
precedent). Every other ticket takes its own commit.

---

## 14. Approval package — the nine gated causes

Each entry gives the exact current behaviour, the proposed behaviour, the
consequences, the alternatives, and a copyable approval sentence. **None is
requested in this batch; none is implemented.**

### 14.1 #2013 — T-G, the shared mutable factory encodings (SR-AUD-288)

**Now:** `Encoding::UTF8()` and the six siblings each return one process-wide
`shared_ptr` to a mutable object. Measured: mutating its decoder fallback
changes what every other caller decodes; `Encoding.cpp.audit.md` records a TSan
read/write race between the setter and a concurrent decode.
**.NET:** the factory encodings are **read-only**; `IsReadOnly` is true and both
fallback setters throw `InvalidOperationException`.
**Consequence of adopting it:** a currently-succeeding setter starts throwing —
including in this repository's own `TextNamespaceTests.cpp`, which mutates
`Encoding::UTF8()`'s fallback and restores it. **Alternatives:** (A) .NET's
read-only contract plus an `IsReadOnly` property and a `Clone()`; (B) return a
fresh instance from every factory call (breaks the identity
`UTF8() == UTF8()`); (C) a mutex inside `Encoding`, which fixes the race but not
the aliasing; (D) document the aliasing and leave it. Recommended minimum:
**A**, with `Clone()` so the existing test can migrate.

> Approve making `System::Text::Encoding`'s seven factory instances read-only,
> so `setDecoderFallbackProperty`/`setEncoderFallbackProperty` throw
> `System::InvalidOperationException` on them, and adding an `IsReadOnly`
> property and a `Clone()` that returns a mutable copy — accepting that this
> repository's own `TextNamespaceTests` must migrate to `Clone()`. Ticket
> **#2013**.

### 14.2 #2014 — T-H, Latin-1 over scalars (SR-AUD-289)

**Now:** `Latin1.GetBytes(u8"é")` → `c3a9`; `GetString({e9})` → `e9`.
**Proposed:** decode the UTF-8 storage to scalars, emit one byte per scalar
≤ U+00FF and the encoder fallback otherwise; decode each byte as the scalar of
the same value and re-encode as UTF-8 — i.e. exactly what `ASCIIEncoding`
already does one range wider.
**Consequence:** produced bytes change for every non-ASCII input; ASCII is
unaffected. **Alternative:** rename/redocument the class as a byte pass-through,
which contradicts its `iso-8859-1` name and code page 28591.

> Approve making `System::Text::Latin1Encoding` convert Unicode scalar values
> rather than UTF-8 storage bytes, so `GetBytes` emits one ISO-8859-1 byte per
> scalar U+0000–U+00FF and `GetString` decodes each byte to that scalar,
> accepting that every non-ASCII conversion produces different bytes than
> today. Ticket **#2014**.

### 14.3 #2015 — T-I, the unit of a public index or length (SR-AUD-290, 296)

**Now:** `GetCharCount(U+1F600)` → 4; `StringBuilder(u8"éA").Length` → 3, and
`Remove(1,1)` yields ill-formed `c341`.
**Proposed:** either (A) make every public index/length a UTF-16 code-unit
count as .NET's is, or (B) make them scalar counts, or (C) keep bytes and
declare it. A and B both change every index-taking member in
`Encoding`, `StringBuilder`, `RunePosition` and the rune enumerators; C is what
#2012 does for the doc-comments only. **This is the largest gate in the
namespace** and should not be taken without deciding it for `System::String`
too, which has the same adaptation.

> Approve changing the unit of every public index, length and count in
> `System::Text` from UTF-8 storage bytes to UTF-16 code units, accepting that
> `Encoding::GetCharCount`, `StringBuilder::Length`/`Insert`/`Remove`/`CopyTo`
> and every caller's arithmetic change meaning. Ticket **#2015**.

### 14.4 #2016 — T-J, preamble versus payload (SR-AUD-291)

**Now:** `Encoding::UTF32()->GetBytes("A")` → `fffe000041000000`.
**Proposed:** `GetBytes` never prepends; a new `GetPreamble()` exposes the
bytes. **Two gates, not one:** emitted bytes change for the default UTF-32
factory, **and** making `GetPreamble` virtual on `Encoding` appends a vtable
slot. A non-virtual per-class `GetPreamble()` avoids the vtable gate but cannot
be reached through an `Encoding&`, which is how every consumer holds one.

> Approve (a) removing the byte-order mark from `UnicodeEncoding::GetBytes` and
> `UTF32Encoding::GetBytes`, changing the bytes `Encoding::UTF32()` produces for
> every input, and (b) adding a virtual `GetPreamble()` to
> `System::Text::Encoding`, accepting the resulting vtable layout change and the
> full recompilation it requires. Ticket **#2016**.

### 14.5 #2017 — T-K, the fallback policy (SR-AUD-292, 293)

**Now:** only `UTF8Encoding` consults the configured fallback; ASCII hard-codes
`'?'`, UTF-16/UTF-32 hard-code `U+FFFD`, and a truncated trailing unit is
discarded without reaching a fallback at all.
**Proposed:** every encoding routes ill-formed input and every unencodable
scalar through its configured encoder/decoder fallback, and a truncated trailing
unit is an ill-formed sequence like any other. **Consequence:** a configured
**exception** fallback starts throwing where it never did; and unless each
encoding's constructor installs the same default its loop hard-codes today
(`U+FFFD` for UTF-16/UTF-32, `'?'` for ASCII, as `UTF8Encoding`'s constructor
already does), default output changes too.

> Approve making every `System::Text` encoding route unencodable characters and
> ill-formed byte sequences — including an incomplete trailing UTF-16 or UTF-32
> unit — through its configured `EncoderFallback`/`DecoderFallback`, so a
> configured exception fallback throws, accepting that each concrete encoding
> must install the replacement text its loop currently hard-codes in order to
> leave default output unchanged. Ticket **#2017**.

### 14.6 #2018 — T-L, Unicode classification and casing (SR-AUD-294)

**Now:** `Rune::IsLetter(U+00E9)` is false and `ToUpper` is a no-op, while
`IsWhiteSpace(U+00A0)` is true.
**Proposed:** real Unicode category and simple-case-mapping tables.
**Consequence:** a large generated data table, a new data-versioning question
(which Unicode version), and changed results for every non-ASCII scalar.
**Alternative:** declare the ASCII scope in the type name or class doc-comment
and leave the behaviour — which is close to what the member doc-comments already
do.

> Approve implementing real Unicode category and simple case mapping in
> `System::Text::Rune`, including the generated table and a stated Unicode
> version, accepting that every non-ASCII classification and casing result
> changes. Ticket **#2018**.

### 14.7 #2019 — T-M, the default Web encoders' policy (SR-AUD-297)

**Now:** `HtmlEncoder::Encode(u8"é")` and `JavaScriptEncoder::Encode(u8"é")`
return the input unchanged.
**Proposed:** .NET's default encoders allow Basic Latin only and escape
everything else. **Consequence:** emitted text changes for all non-ASCII input,
and a relaxed-encoder opt-in (`Create(UnicodeRanges…)`) must be added for
callers who want today's behaviour.

> Approve giving `System::Text::Encodings::Web`'s default HTML and JavaScript
> encoders .NET's Basic-Latin allow-list, escaping every non-Basic-Latin scalar,
> and adding a `Create(UnicodeRange…)` opt-in for the current pass-through
> behaviour. Ticket **#2019**.

### 14.8 #2020 — T-N, one composite-format grammar (SR-AUD-298, CCF-012)

**Now:** `CompositeFormat::Parse("{0,not-a-width}")` and `("{0,-}")` succeed;
`{1500000}` succeeds with `minArgCount = 1500001`.
**Proposed:** `CompositeFormat::Parse` validates with
`System::detail::runCompositeFormat`'s grammar — the one #1884 approved for
`String::Format` and `FormattableString::ToString` — including
`kCompositeIndexLimit`. **Consequence:** the three shapes above start throwing
`FormatException`; **CCF-012 can then be closed**, which it cannot be while a
third engine disagrees (§4.6).

> Approve making `System::Text::CompositeFormat::Parse` validate against the
> same composite-format grammar `String::Format` and
> `FormattableString::ToString` adopted in #1884, including its one-million
> index limit and its alignment and specifier rules, accepting that
> `{0,not-a-width}`, `{0,-}` and any index at or above 1,000,000 begin to throw
> `System::FormatException`. Ticket **#2020**.

### 14.9 #2021 — T-F, `EncodingInfo::GetEncoding` (SR-AUD-299)

**Now:** always UTF-8, whatever the code page says.
**Proposed:** resolve 65001/20127/1200/1201/12000/12001/28591/65000 to the
matching encoding, and raise a diagnostic for anything else.
**Consequence:** the returned object changes for seven of the eight code pages
this port implements, and unknown pages start throwing where they used to
return UTF-8. No first-party caller exists (§2.4), so the risk is external only.

> Approve making `System::Text::EncodingInfo::GetEncoding` return the encoding
> matching its own code page, and throw for a code page this port does not
> implement, accepting that it no longer returns UTF-8 unconditionally. Ticket
> **#2021**.

---

## 15. Explicit exclusions

Out of this namespace's scope, permanently or by instruction:

1. **Normalization.** `System::Text::NormalizationForm` exists as an enum in
   `modules/core`; `String::Normalize`/`IsNormalized` do not exist. Implementing
   Unicode normalization needs the full UCD and is not proposed. No finding
   maps to it and none is invented.
2. **Streaming/stateful conversion.** `Encoder` and `Decoder` are documented
   stateless wrappers with no `Convert(…, out bytesUsed, out charsUsed, out
   completed)` surface at all. There is therefore **no** streaming state to be
   wrong, no split-sequence contract to violate, and no `flush` parameter — the
   audit filed no finding against either, and this review adds none. Adding the
   incremental surface is new API, not remediation.
3. **`Encoding.RegisterProvider`** and the global code-page registry —
   documented as omitted; only its consequence (SR-AUD-299) is in scope.
4. **`CodePagesEncodingProvider`**, `UTF7Encoding`'s obsolescence, and every
   non-implemented code page.
5. **Downstream migration.** CNA and mobile-eggbert (§10).
6. **`Text.Json`, `Text.RegularExpressions`, `Buffers::Text`, `IO::Text*`,
   `Globalization::Text*`** (§6).
7. **`Ascii.hpp`'s byte orientation.** Deliberate and audited clean.

---

## 16. Post-audit defects found by this review (no `SR-AUD-*` identifier)

Audit numbering stays frozen at 364. These were found by #2006's own
measurement, inside files the existing findings already own, and are folded into
the owning compatible ticket because each is the same expression as the named
defect.

| # | Defect | Measured | Folded into |
|---|---|---|---|
| 1 | null `data` with a positive `count` is a segmentation fault in `Latin1Encoding`, `UnicodeEncoding`, `UTF32Encoding` | `KILLED by signal 11` ×3 | #2007 |
| 2 | a negative `count` throws `std::length_error` out of `Latin1Encoding::GetString` | `basic_string::_M_create` | #2007 |
| 3 | `UnicodeEncoding`'s `intcs end = index + count` is signed overflow | `KILLED by signal 11` | #2007 |
| 4 | `Decoder::GetString(vector, negativeIndex)` inflates the derived length past the vector | five bytes from a four-element vector | #2007 |
| 5 | `DecoderExceptionFallback::GetFallbackString(nullptr, -1)` throws `std::length_error` | `cannot create std::vector larger than max_size()` | #2007 |
| 6 | `setEncoderFallbackProperty(nullptr)` is the same crash as the decoder setter | `KILLED by signal 11` | #2008 |
| 7 | `CopyTo`'s signed overflow **defeats** the bounds check and writes | `returned normally` | #2009 |
| 8 | `CompositeFormat::Parse("{2147483647}")` returns `minArgCount = -2147483648` | measured | #2010 |
| 9 | `UrlEncoder::Decode` accepts non-hexadecimal text and produces a wrong byte | `%-1`→`ff`, `% 1`→`01`, `%+f`→`0f` | #2011 |
| 10 | `HtmlEncoder::Encode(value, start, count)` throws `std::out_of_range` for a negative start and silently clamps an over-long count | measured | #2011 |

---

## 17. Deferred verification

| Item | What is missing | Ticket |
|---|---|---|
| Whether .NET's `Encoding.GetString(byte*, int)` uses `ArgumentNullException` or `ArgumentOutOfRangeException` **first** for `(nullptr, -1)` | the reference tree | none — #2007 adopts **this repository's** `UTF7Encoding` order instead, which needs no external evidence (§7) |
| The exact .NET message text for each raw-range diagnostic | the reference tree | none — #2007 reuses the messages `UTF7Encoding` and `BitConverter` already carry |
| Whether .NET's default `UrlEncoder` has a `Decode` at all | it does not; the port's `Decode` is an addition | folded into #2011's design note |
| Every §14 gate marked "no" in §7 | the reference tree, and a decision | the gate's own ticket |

**No compatible ticket in this batch is blocked on absent evidence.**

---

## 18. Namespace completion criteria

`System::Text` is complete when **all fourteen** findings are `remediated` or
carry the `confirmed (design-complete)` qualifier with a blocked implementation
ticket, and:

1. #2007–#2012 are `done` and their tests are permanent;
2. #2013–#2021 each carry a durable design in this document **and** a blocked
   ticket whose notes name the approval sentence;
3. the whole 37-executable gate is green apart from the six known
   environment/#1962 failures;
4. `SharpRuntimeTests_Text` has grown by the §11 matrix, add-only;
5. CCF-012's exclusion-list correction (§4.6) is recorded in the cross-cutting
   file;
6. no `SR-AUD-*` identifier was created — numbering stays at 364.

---

## 19. Status

| Ticket | Cause | Findings | State |
|---|---|---|---|
| #2006 | — | maps all 14 | this document |
| #2007 | T-A | 286 | see §20 |
| #2008 | T-B | 287 | see §20 |
| #2009 | T-C | 295 | see §21 |
| #2010 | T-D | 298 (half) | see §22 |
| #2011 | T-E | 297 (half) | see §23 |
| #2012 | T-I | 290, 296 (doc half) | see §24 |
| #2013–#2021 | T-G/H/I/J/K/L/M/N/F | 288, 289, 290, 291, 292, 293, 294, 296, 297, 298, 299 | **blocked**, design complete (§14) |

---

## 20–24. Where the implementation records actually live

**Appended by ticket #2022, 2026-08-03.** §19's table points at sections 20–24
that were never written: the #2007–#2012 implementation records were kept in
`ticket.notes`, in the `audit/AUDIT_FINDINGS_INDEX.md` rows for SR-AUD-286,
287, 289, 290, 295, 296, 297 and 298, and in `NEXT.md`'s 2026-08-03 handoff,
rather than here. The cross-references are corrected rather than the records
duplicated:

| §19 says | The record is in |
|---|---|
| §20 (#2007, #2008) | tickets #2007/#2008 `notes`; index rows SR-AUD-286, SR-AUD-287; commit `9d2600a`; probes `2007_asan_before.log`, `2007_asan_after.log` |
| §21 (#2009) | ticket #2009 `notes`; index row SR-AUD-295; commit `82edb12` |
| §22 (#2010) | ticket #2010 `notes`; index row SR-AUD-298; commit `a4a2b11`; probe `2010_sanitizer_*.log` |
| §23 (#2011) | ticket #2011 `notes`; index row SR-AUD-297; commit `a4a2b11` |
| §24 (#2012) | ticket #2012 `notes`; index rows SR-AUD-290, SR-AUD-296, SR-AUD-289; commit `5a225ea` |

---

## 25. Verification of §14 — ticket #2022 (2026-08-03)

§§1–19 are preserved exactly as written. This section records what re-measuring
§14 against the shipped library found, and where the **request** now lives.

**The approval request has moved.** `docs/SystemTextApprovalPackage.md` is the
single place a decision is asked for on #2013–#2021. It groups the nine tickets
into six families, gives one exact approval sentence per family, and carries the
corrections below. §14 remains the historical design record and is **not**
rewritten.

**Ten corrections, each measured** (`build-probe/2022_probe1_approval_verify.cpp`
→ `build-probe/2022_probe1_verify.log`); the full table is
`docs/SystemTextApprovalPackage.md` §10. In brief:

1. **§14.1** — the recommended `IsReadOnly` + `Clone()` spelling costs
   `sizeof(Encoding)` **40 → 48** and, with a virtual `Clone()`, a **vtable
   slot**; §14.1 states neither. An identity-based `IsReadOnly` with a
   code-page-dispatching non-virtual `Clone()` has **neither** cost.
2. **§14.4** — **two** default factories emit a byte-order mark as payload, not
   one: `Encoding::BigEndianUnicode()` → `feff0041` as well as
   `Encoding::UTF32()` → `fffe000041000000`.
3. **§14.4** — the **decode** direction *consumes* a leading U+FEFF in both
   UTF-16 and UTF-32, so a real ZERO WIDTH NO-BREAK SPACE is discarded. Neither
   the finding nor §14.4 mentions it; a round trip cancels both halves exactly,
   which is why no test ever saw them.
4. **§14.5** — the fallback surface takes a **`char`**
   (`EncoderFallbackBuffer::Fallback`, `EncoderFallback::GetFallbackBytes`,
   `EncoderFallbackException::getCharUnknownProperty`), which cannot carry a
   non-ASCII scalar. Routing *scalars* through it requires a **public virtual
   signature change**; §14.5 does not ask that question.
5. **§14.6** — `Rune::IsWhiteSpace` is Unicode-aware **and divergent**: its table
   contains U+FEFF, which .NET excludes. The repair is not "make six members
   match the seventh".
6. **§14.8** — *"any index at or above 1,000,000 begin to throw"* is **false**.
   `kCompositeIndexLimit` stops digit consumption rather than rejecting, so the
   shared grammar **accepts** `{1000000}` … `{9999999}`; the first rejected
   shapes are eight-digit indices and `{2147483646}`.
7. **§14.8** — adoption is **not purely a narrowing**: `{0 }` and `{0  ,5}` are
   `FormatException` here today and are **accepted** by the shared grammar.
8. **§14.8** — `runCompositeFormat` is a **formatting** engine (it needs an
   `argCount` and a `render`, and it **pads while validating** — measured
   `pad=1000000` for `{0,1000000}`). #2020 must extract a non-rendering scanner
   into `modules/core`, a far wider blast radius than §14.8 implies.
9. **§14.9** — `EncodingInfo::GetEncoding` returns `Encoding::UTF8()` **itself**,
   the shared mutable singleton, so #2021 and #2013 are one decision.
10. **§5 / §4.6** — measured today there are exactly **two** composite-format
    grammar implementations in the repository; `CompositeFormat::Parse` is the
    **second** and the only divergent one, not the third or fourth.

**One completion criterion in §18 was not met, and is now.** The batch claimed
every gated behaviour was pinned "so none can land silently". **SR-AUD-294
(#2018) and SR-AUD-299 (#2021) had no pin at all** — every `Rune` test in the
repository uses ASCII exclusively, and `EncodingInfo` had no test anywhere — and
SR-AUD-291, SR-AUD-292 and SR-AUD-298 were pinned only in part. Ticket **#2022**
adds `modules/text/tests/System/Text/TextGatedBehaviourPinTests.cpp` (+8,
add-only, `SharpRuntimeTests_Text` 288 → 296), mutation-checked three ways: a
Latin-1-widened `Rune::IsLetter`, U+FEFF removed from `Rune::IsWhiteSpace`, and
`GetEncoding` resolving code page 20127 to ASCII each **fail** the new pins and
each **pass** the pre-existing `TextUnitContractTests`.

**CCF-012 is not closed and is not marked closed.** Verified: its member
SR-AUD-015 is `remediated`, the population is exactly the two implementations
named above, and #2020 closes the family **only** under the shared-scanner
option — a hand-aligned second grammar would preserve exactly the divergence
CCF-012 warns about (`docs/SystemTextApprovalPackage.md` §5.5).

**No `SR-AUD-*` identifier was issued by this verification.** Numbering stays
frozen at **364**. The two post-audit observations it measured — the decode-side
BOM consumption and `IsWhiteSpace`'s U+FEFF — are folded into #2016 and #2018
under ordinary ticket numbers, on the §16 precedent.
