<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Text::Json` (`modules/text-json`) namespace review — ticket #2110

Owning ticket **#2110**. This document is the durable record; it **remediates nothing by
itself**. Every claim is measured against the tree at `59a8107`.

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-04. Every statement about
.NET comes from repository-contained evidence only: the per-file audit reports, doc-comments
transcribed from .NET when the module was written, and this module's own tests. Where a repair
would need .NET's exact behaviour and no repository evidence pins it, a **deferred-verification
ticket** is created instead of a guess.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit defects
carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket #1773 stays blocked.

Primary evidence: `build-probe/2110_probe1_textjson.cpp` (log `2110_probe1_before.log`) and
`build-probe/2110_probe2_newdefects.cpp` (log `2110_probe2_newdefects.log`).

---

## 1. Why this unit was selected — re-measured, not inherited

The `modules/io` review §17 recommended `modules/text-json`. That recommendation was
**re-derived from scratch** against `audit/AUDIT_FINDINGS_INDEX.md` at `59a8107`, after
`modules/io`'s compatible queue advanced. Every unit with at least six open findings:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `modules/core` | 68 | 9 | 55 | 4 | 13% | 1 | 43 | family plans only |
| `modules/threading` | 17 | 6 | 11 | 0 | 35% | 0 | 21 | **yes** |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7% | 12 | 8 | **yes** |
| `modules/text` | 11 | 1 | 10 | 0 | 9% | 11 | 3 | **yes** |
| `modules/uri` | 10 | 0 | 10 | 0 | 0% | 10 | 4 | **yes** |
| `modules/time-zone` | 7 | 0 | 7 | 0 | 0% | 0 | 0 | none |
| **`modules/text-json`** | **7** | **1** | **6** | **0** | **14%** | **1** | **0** | **none** |
| `modules/io` | 7 | 0 | 7 | 0 | 0% | 0 | **6** | **yes** |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none |
| `modules/net-http` | 6 | 1 | 5 | 0 | 16% | 2 | 3 | **yes** (closed) |

**Unreviewed units with ≥6 open:** `core` (68), `time-zone` (7), `text-json` (7),
`globalization` (7).

### Applying the stated priorities

1. **Memory safety and lifetime.** `text-json` holds the module's one `high`, **SR-AUD-327**, a
   **CCF-019** member with an ASan-confirmed `heap-use-after-free` still live (J11). `time-zone`
   and `globalization` have no lifetime findings at all.
2. **Public-input attackability.** **Decisive.** `text-json` is the only candidate that parses
   **untrusted remote documents**. `time-zone` reads system zone data; `globalization` answers
   culture queries. This review found two remotely-triggerable defects that no audit finding
   names (§7), which is the direct payoff.
3. **Decidability without the reference tree.** `text-json` wins again. Its defects are
   self-evident contract violations — a `std::` exception must not escape a `System`-shaped API,
   a NUL must not silently truncate a document, an option that is validated must not be inert.
   `time-zone`'s seven and five of `globalization`'s seven are *"what exactly does .NET do"*
   questions (DST rule equality, collation, grapheme clusters) that with `/rv` absent produce a
   queue of deferred-verification tickets rather than compatible work.
4. **Existing family obligation.** SR-AUD-327 is CCF-019's **first-named** site. Nothing here
   closes CCF-019, but the module is where that family's evidence is densest.
5. **Coherent module boundary.** One CMake component (`Text.Json`, `TYPE STATIC`,
   `PUBLIC_DEPENDENCIES Core.Base Text`, `PRIVATE_DEPENDENCIES Collections.Core`): 34 public
   headers (2,459 lines), **3** sources (765 lines), 8 test files (2,951 lines). `modules/core`
   at 68 open is not a coherent unit and already has family plans.
6. **No existing complete review.** Correct.

**Selected: `modules/text-json`**, on priorities 1, 2 and 3. `modules/net-http-headers`
(5 open, two `high`) remains the **CCF-021 trigger** below the ≥6 threshold, and this batch was
explicitly instructed not to begin it. It is the recommended next unit (§16).

---

## 2. Scope and file inventory

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 34 | 2,459 |
| implementation | **3** | 765 |
| tests | 8 | 2,951 |

**The implementation/header ratio is the module's defining structural fact**: all but three
files are header-only, and the parsing engine is **not in this module at all** — it is
`vendor/nlohmann/json.hpp`. Almost every defect below is a **boundary** defect between a
`System`-shaped public surface and a vendored `std::`-shaped parser.

**In scope:** everything under `modules/text-json/`.

**Out of scope, and why:**

- `vendor/nlohmann/json.hpp` itself — third-party source, exempt by `CLAUDE.md`. Its *behaviour*
  is in scope wherever it reaches a public surface; its *code* is never modified.
- **`Utf8JsonReader` — absent from this port.** There is no reader type: `JsonTokenType` and
  `JsonReaderOptions` exist as declarations with no reader consuming them. The batch's
  partial-token, incremental-read and `ReadOnlySequence` checklist therefore **has no subject
  here**, and saying so is more useful than inventing one.
- **Asynchronous serialization, cancellation and stream ownership — absent.** `JsonSerializer`
  exposes no `SerializeAsync`/`DeserializeAsync`, no `Stream` overload and no
  `CancellationToken`. The async-lifetime, cancellation-ordering and `leaveOpen` checklist has
  **no subject here** either. Recorded rather than fabricated.
- **Reflection-based `Serialize<T>`/`Deserialize<T>`** — a permanent deviation per `CLAUDE.md`.
  The template overloads use nlohmann's ADL customization points instead; that is the design,
  not a gap.
- `System.Text.Encodings.Web` `JavaScriptEncoder` integration — out of scope by the same
  decision `JsonWriterOptions` already records.

---

## 3. Complete public-surface inventory

| Area | Types |
|---|---|
| Document / DOM (read) | `JsonDocument`, `JsonElement`, `JsonProperty` |
| Document options | `JsonDocumentOptions`, `JsonReaderOptions`, `JsonWriterOptions`, `JsonCommentHandling` |
| Mutable DOM (`Nodes`) | `JsonNode`, `JsonArray`, `JsonObject`, `JsonValue`, `JsonNodeOptions` |
| Writing | `Utf8JsonWriter`, `JsonEncodedText` |
| Serialization | `JsonSerializer`, `JsonSerializerOptions`, `JsonSerializerDefaults`, `JsonNamingPolicy` |
| Converters | `JsonConverter`, `JsonConverterFactory`, `JsonStringEnumConverter` |
| Serialization policy | `JsonNumberHandling`, `JsonObjectCreationHandling`, `JsonUnknownTypeHandling`, `JsonUnknownDerivedTypeHandling`, `JsonUnmappedMemberHandling`, `JsonKnownNamingPolicy`, `JsonSerializationAttributes`, `JsonOnSerializationInterfaces` |
| Reference handling | `ReferenceHandler`, `ReferenceResolver` |
| Kinds / errors | `JsonValueKind`, `JsonTokenType`, `JsonException` |

**Ownership shape worth recording up front:** `JsonElement` holds an **owning aliasing
`shared_ptr`** into the parsed `nlohmann::ordered_json`. It is *not* a borrowed view. That single
fact corrects SR-AUD-324's framing (§6.1). `JsonNode`'s `parent_`, by contrast, **is** a raw
borrowed pointer — the CCF-019 shape — and is SR-AUD-327's subject.

---

## 4. Every open finding, with its measured disposition

All seven reproduced or refuted against `59a8107`.

| Finding | Sev | Measured | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-324 | med | **confirmed, premise corrected** — §6.1 | compatible, but layout-gated | **#2117** |
| SR-AUD-325 | med | **confirmed exactly as filed** | blocked — architectural | **#2118** |
| SR-AUD-326 | med | **confirmed and halved** — §6.2 | split: pin + design | **#2115** |
| SR-AUD-327 | high | **unchanged** — design-complete, residual J11 | **blocked** (#1888/#1889/#1894) | — |
| SR-AUD-328 | med | **largely REFUTED** — §6.3 | compatible, narrowed | **#2114** |
| SR-AUD-329 | med | **confirmed and wider** — §6.4 | compatible | **#2113** |
| SR-AUD-330 | med | **confirmed** — §6.5 | compatible in part | **#2116** |

### 4.1 SR-AUD-327 is not re-opened by this review

It is `confirmed (design-complete)`. `docs/OwnedTreeLifetimeContractPlan.md` holds the selected
repair; #1886 landed the approved core; **#1888, #1889 and #1894 remain blocked** on a public
source break and an object-layout change. This review **adds no member, changes no status and
implements nothing** there. One measurement is recorded in §7.5 because it appears to differ
from that plan's residual table, and it is flagged for re-verification rather than claimed.

---

## 5. Structural root-cause families

- **TJ-A — a vendored `std::`-shaped parser's exception escapes a `System`-shaped public API.**
  The new §7.1 defect. **Identical in kind to `modules/io`'s cause I-C (SR-AUD-347, #2101) and to
  CCF-012's defect class**, in a third component. Not a CCF-012 member: CCF-012 is about
  composite-format brace grammars.
- **TJ-B — a public option is validated, stored, and never consulted.** SR-AUD-326's two inert
  flags, SR-AUD-330's discarded `opts`. **Same shape as `io`'s I-D (SR-AUD-346),
  `net-websockets`' W-E (SR-AUD-252) and `xml`'s stored-never-read properties.** A genuine
  cross-module candidate family, recorded in §8.2 and **not minted**.
- **TJ-C — a borrowed parent pointer with no liveness boundary.** SR-AUD-327. **CCF-019.**
- **TJ-D — the DOM cannot represent the source, so round-tripping is lossy.** SR-AUD-325.
- **TJ-E — sibling overloads disagree about the same input.** SR-AUD-329's two `Encode`
  overloads. **Same shape as `io`'s I-F (SR-AUD-345, #2103).**
- **TJ-F — a narrowing conversion is performed instead of rejected.** SR-AUD-328's surviving
  `JsonValue` half, and `Deserialize<int>("1.5")`.
- **TJ-G — a control byte is treated as a terminator by the vendored parser.** The new §7.2
  defect. **Same shape as `xml`'s #2085.**
- **TJ-H — disposal is recorded but not propagated to values handed out earlier.** SR-AUD-324.
  **This is CCF-022's candidate shape (X-D) in a third module** — see §8.3.

---

## 6. Corrected premises

### 6.1 SR-AUD-324 — it is **not** a use-after-free, and half of it is already correct

The finding sits among lifetime defects and the batch's checklist expects a dangling DOM view.
Measured, it is neither:

- `JsonElement` holds an **owning** aliasing `shared_ptr`. `Dispose()` drops the *document's*
  share; a previously captured element **keeps the tree alive** and reads valid memory. The
  probe's `element-after-dispose=10` is a **correct read of live storage**, not a dangling one.
- **`getRootElementProperty()` after `Dispose()` already throws** `ObjectDisposedException`. The
  door the finding's own file owns is already guarded.
- **Double `Dispose()` is already safe.**

So the defect is a **contract** divergence (.NET's `CheckUseAfterDispose` requires elements
handed out earlier to throw too), *not* a memory-safety defect. That reclassification matters:
it moves the finding out of the "fix urgently, it is a use-after-free" bucket and into
TJ-H/CCF-022, and it means the repair is *adding* a shared disposal flag — which is an
**object-layout change to `JsonElement`** (§9). Recorded, ticketed as **#2117**, **not**
implemented.

### 6.2 SR-AUD-326 — **two of the four options work**; the summary over-generalises

The finding says *"parsing flags are exposed but not applied"*. Measured:

| Option | Applied? |
|---|---|
| `CommentHandling` | **YES** — `Skip` accepts `[1] // c`, `Disallow` rejects it |
| `MaxDepth` | **YES** — `MaxDepth=3` rejects 5 levels; the default accepts exactly 64 and rejects 65; `MaxDepth=-1` throws `ArgumentOutOfRangeException` |
| `AllowTrailingCommas` | **NO** — `[1,]` is rejected with the flag set to `true` |
| `AllowDuplicateProperties` | **NO** — `{"x":1,"x":2}` is accepted with the flag set to `false`, and yields `x=2` |

Two inert flags, not four. And the two that are inert are **not equally fixable**: nlohmann
offers no trailing-comma mode at all, so honouring `AllowTrailingCommas=true` would require
replacing the parser, whereas `AllowDuplicateProperties=false` is reachable through nlohmann's
parse callback. That asymmetry is why **#2115 is a design ticket, not a repair** (§9).

### 6.3 SR-AUD-328 — **largely refuted**; only the `Nodes` half survives

The finding names `JsonValue.hpp`. Measured, `JsonElement`'s accessors are **already correct**:

- `JsonElement` `1.5` → `GetInt32()` **throws** *"The JSON value could not be converted to Int32."*
- `1e100` → `GetInt32()` and `GetInt64()` both **throw**
- `99999999999999999999` → `GetInt64()` **throws**
- `7` → `GetInt32()` returns `7`; `GetString()` on it throws a kind mismatch

**What survives is exactly one door:** `JsonValue::Create(1.5)->GetInt32()` returns **`1`**. Plus
one the finding does not name: `JsonSerializer::Deserialize<int>("1.5")` also returns **`1`**.
#2114 is scoped to those, not to the whole family the summary implies.

### 6.4 SR-AUD-329 — confirmed, and the sharper framing is that two overloads disagree

`JsonEncodedText::Encode(const std::string&)` accepts **everything**: `C3 28` (invalid), `ED A0
80` (a UTF-8-encoded lone surrogate), an embedded NUL, and `C0 AF` (overlong). Meanwhile
`Encode(const std::u16string&)` **validates and throws** `ArgumentException`. Two overloads of
one function, on one type whose documented contract is *"validated UTF-8/JSON text"*, give
opposite answers. That is cause **TJ-E**, the same shape as `io`'s SR-AUD-345, and the repair
target — a validator — **already exists in the module's dependency** (`System::Text::Unicode`).

### 6.5 SR-AUD-330 — confirmed, and it is the same cause as §6.2

Both `Deserialize` overloads take `JsonSerializerOptions` and discard it. Measured, `[1,]` is
rejected with serializer options that request trailing commas. But **the options it would
forward are the inert ones** — so forwarding alone changes nothing observable until #2115 is
decided. #2116 is therefore scoped to the *forwarding* (compatible, and it makes `MaxDepth` and
`CommentHandling` reachable through the serializer, which today they are not) and explicitly
depends on #2115 for the rest.

---

## 7. Post-audit defects (no `SR-AUD-*` identifier)

### 7.1 A `std::` exception escapes **four** public doors on an 8-character input — the sharpest defect in the module

`JsonDocument::Parse` catches only `nlohmann::ordered_json::parse_error`. A number literal that
overflows a `double` raises `nlohmann::detail::out_of_range` (error 406), which is **not** a
`parse_error`. Measured:

| Door | `{"a":1e999999}` |
|---|---|
| `JsonDocument::Parse` | **`std::` exception escapes** |
| `JsonNode::Parse` | **`std::` exception escapes** |
| `JsonSerializer::Deserialize` (`JsonDocument` overload) | **`std::` exception escapes** |
| `JsonSerializer::Deserialize<double>` | `JsonException` — **already correct** |

`1e400` and `-1e999999` do it too; `1e308` parses fine, so the boundary is real.

**Why this is priority one.** It is reachable from **eight characters of untrusted document
text**; it crosses a `System`-shaped API as a `std::` type, so a caller writing
`catch (const System::Exception&)` or `catch (const JsonException&)` **does not catch it** and
the process terminates; and it hits every parse entry point in the module at once. It is cause
**TJ-A**, the same class as `io`'s SR-AUD-347.

**The repair target already exists one overload away.** `Deserialize<double>` catches
`nlohmann::ordered_json::exception` — the **base** — and is therefore already correct. Catching
the base at the other three doors is the whole fix, and it is the same
"route the broken door through the sibling that was already right" shape as #2103 and #2101.
Ticket **#2111**.

### 7.2 An embedded NUL silently truncates the document, and a control proves it

| Input | Result |
|---|---|
| `{"a":1}` `NUL` `{"b":2}` | **accepted**, parsed as `{"a":1}` — the second object is **silently discarded** |
| `{"a":1}` `SPACE` `{"b":2}` | **rejected** — *"syntax error"* — **the control** |
| `[1,2]` `NUL` `[9]` | **accepted**, parsed as `[1,2]` |
| `{"a":12` `NUL` ` 34}` | rejected — a NUL *inside* a token is still an error |
| `JsonNode::Parse` on the first input | **accepted**, same truncation |

The control is what makes this a finding rather than an observation: the identical document with
a space instead of the NUL **is** rejected as trailing junk, so the NUL is specifically being
treated as end-of-input. Two parties parsing the same bytes can therefore disagree about what
the document says — the classic parser-differential shape. Cause **TJ-G**, the same shape as
`xml`'s #2085. Ticket **#2112**.

### 7.3 `JsonDocumentOptions::Validate()` does not validate two of its own fields

`Validate()` checks `CommentHandling` and `MaxDepth` and says nothing about the two flags §6.2
found inert. A caller cannot discover from the API that setting them has no effect. Folded into
**#2115**.

### 7.4 Measured positives, recorded so they are not re-investigated

- **`JsonDocument::Parse` has no stack-overflow exposure at 100,000 nesting levels.** It rejects
  at the configured depth and returns cleanly; nlohmann's parser and its tree teardown are both
  iterative enough to survive it. The batch's leading structural suspicion for a JSON parser was
  a recursive-descent stack overflow; **measured, there is none at this door.**
- `checkMaxDepth` recursion is **bounded by `MaxDepth`**, because it throws as soon as the depth
  is exceeded — it cannot itself overflow.
- Property lookup is **case-sensitive**, and a missing key, a wrong-kind access and a
  non-object `GetProperty` all raise `System` exceptions with useful messages.
- ` ` inside a string value is accepted and produces a 3-character string — the escape form
  is handled correctly, unlike the raw byte in §7.2.
- Lone surrogate **escapes** (`\ud800`, `\udc00`) are **rejected**; a valid pair is accepted.
- Raw invalid UTF-8 inside a string value is **rejected** by the parser. (`JsonEncodedText` is a
  different door and is not — §6.4.)

### 7.5 One measurement that appears to differ from `OwnedTreeLifetimeContractPlan.md`, flagged not claimed

That plan's residual table records **J19c/J19d/X28c** — deep-nesting teardown and
`JsonNode::Parse` overflowing the stack at 20,000 levels and timing out at 100,000. Measured at
this tip, `JsonNode::Parse` of **100,000** nested arrays **and its teardown** complete with exit
status 0 in well under a second. Ticket **#1897** made `JsonNode::Parse` iterative *after* that
table was written, which is the plausible explanation.

**This review does not claim #1893 is fixed.** It did not re-run that plan's harness, the two
measurements are not like-for-like, and #1893 is blocked on an accepted-input change that this
review has no authority over. It is recorded as **#2119, a deferred verification**, so the
discrepancy is not lost.

---

## 8. CCF mapping

### 8.1 CCF-019 — `text-json` is the family's first-named site, and nothing here changes it

SR-AUD-327 is CCF-019's original member. #2066 is the family's open design question with two
competing options and no selection. **CCF-019 remains open**; nothing here closes it and no
seventh local answer is added.

### 8.2 TJ-B is a genuine cross-module candidate, and it is **NOT** minted

*A public option is validated, stored, and never consulted* now has members in **four** modules:
`text-json` (SR-AUD-326's two flags, SR-AUD-330), `io` (SR-AUD-346, `NotifyFilter`),
`net-websockets` (SR-AUD-252, `KeepAliveInterval`) and `xml`. Under the numbering reconciliation
this batch recorded (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`, "Numbering-policy reconciliation"),
the policy no longer forbids minting — but **#2109 is the open decision about who may mint and
whether a family with blocked members should be minted at all**, and creating a *second*
unminted candidate while that is unresolved would be exactly the drift #2109 exists to end.
**Recorded here as a candidate family with complete membership, deliberately unnumbered.**

### 8.3 CCF-022 — `text-json` contributes a candidate member and it is **not** added

SR-AUD-324 (TJ-H) is *a public lifecycle state recorded but not enforced* — X-D's shape — in a
third module. It is **not** added to CCF-022's membership table, because CCF-022 **is not
minted** and #2109 is the open decision. Recorded so that whoever mints can consider it.

### 8.4 CCF-012

Not a member. TJ-A shares CCF-012's *defect class* (a `std::` primitive's exception escaping a
`System`-shaped API) but CCF-012 is about **composite-format brace grammars**. Recorded so a
future reader does not merge them. **CCF-012 is not marked closed.**

---

## 9. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` |
|---|---|---|---|---|
| **#2111** | an escaping exception's type changes `std::` → `JsonException` | none | none | none |
| **#2112** | narrows: a NUL-truncated document starts being rejected | none | none | none |
| **#2113** | narrows: malformed UTF-8 stops being accepted by one overload | none | none | none |
| **#2114** | narrows: a non-integral `JsonValue` stops converting | none | none | none |
| **#2115** | **DESIGN** — either two options start being honoured or they start being rejected | none expected | none | none |
| **#2116** | widens: `MaxDepth`/`CommentHandling` become reachable through the serializer | none | none | none |
| **#2117** | narrows | **OBJECT LAYOUT — `JsonElement` needs shared disposal state** | none | none |
| **#2118** | **blocked** — needs source spans retained | **OBJECT LAYOUT — `JsonDocument` and `JsonElement`** | none | none |

**#2111–#2114 and #2116 are layout-neutral and need no approval.** #2117 and #2118 both require
an object-layout decision and are gated exactly as `modules/io`'s #2098 is; neither is
implemented, and neither gets a guessed design here.

---

## 10. Ownership and lifetime consequences

- `JsonElement`'s **owning** aliasing `shared_ptr` means the DOM cannot dangle through
  `JsonElement` (§6.1). It also means `Dispose()` does not free memory while an element survives
  — which is a *retention* property, not a leak, and is the direct cost of the safety.
- `JsonNode::parent_` is a raw borrowed pointer. #1886 landed the approved destructor-side repair;
  **J11 (an iterator held across a reallocating `Add`) is still an ASan-confirmed
  use-after-free** and is blocked as #1889.
- Nothing in this module is thread-safe, and `JsonSerializerOptions::Default()` returns a shared
  object — see §12.

---

## 11. Parsing acceptance and rejection consequences

| Input class | Today | After the compatible queue |
|---|---|---|
| number overflowing `double` | **process terminates** for a `System`-catching caller | `JsonException` |
| document with an embedded NUL | silently truncated | rejected |
| trailing comma with the flag set | rejected | **unchanged pending #2115** |
| duplicate properties with the flag clear | accepted, last wins | **unchanged pending #2115** |
| malformed UTF-8 into `JsonEncodedText::Encode(string)` | accepted | rejected |
| `1.5` → `JsonValue::GetInt32()` | `1` | throws |
| nesting beyond `MaxDepth` | rejected (correct) | unchanged |
| nesting to 100,000 in `JsonDocument::Parse` | rejected cleanly, no overflow | unchanged |

---

## 12. Serializer, converter and concurrency consequences

- **Converters are declaration-only.** `JsonConverter` and `JsonConverterFactory` are abstract
  shapes that `JsonSerializer` never consults; there is no converter dispatch, so the batch's
  converter-recursion and null-callback checklist **has no subject here**. Recorded, not invented.
- **`ReferenceHandler`/`ReferenceResolver` are likewise not consulted** by `JsonSerializer`.
  Both are instances of cause TJ-B at type scale rather than option scale; neither is ticketed
  by this review, because the honest repair is "implement the feature", not "fix a defect".
- **Concurrency:** `JsonSerializerOptions::Default()` hands out a shared object. No public
  member of this module is documented thread-safe and none is. No finding names it and this
  review does not open one; recorded so a future reader does not assume it was checked and found
  safe. It was checked and found **absent**, which is different.

---

## 13. Test matrix

| Ticket | Required cases |
|---|---|
| **#2111** | `1e999999`, `-1e999999`, `1e400` at **all four** doors; `1e308` still parses; the exception is a `JsonException` and **no `std::` exception escapes**, asserted by catching `System::Exception` first and `std::exception` second; the message retains nlohmann's text |
| **#2112** | NUL between two values, NUL inside a token, NUL at the very end, NUL only; the **space control** stays rejected; a document with no NUL is byte-identical; both `JsonDocument::Parse` and `JsonNode::Parse` |
| **#2113** | `C3 28`, lone-surrogate UTF-8, overlong `C0 AF`, embedded NUL, valid ASCII, valid multi-byte; both overloads **agree**; `Utf8JsonWriter` still writes what it wrote |
| **#2114** | `1.5`, `-0.5`, `1e100`, exact integers, `2147483648` at `GetInt32`; `JsonElement`'s already-correct behaviour **pinned** so it cannot regress |
| **#2116** | `MaxDepth` and `CommentHandling` reaching `JsonDocument::Parse` through `Deserialize`; the default path unchanged |
| **pins** | §7.4's six measured positives; §6.2's two working options; §6.3's already-correct `JsonElement`; the 100,000-level result |

---

## 14. Sanitizer and direct-resource matrix

| Tool | Applicable here? |
|---|---|
| **ASan** | **yes, primarily** — SR-AUD-327's J11 is an ASan-confirmed use-after-free, and the DOM is pointer-dense |
| **UBSan** | yes — number conversion, depth arithmetic, `intcs` narrowing in the accessors |
| **LSan** | yes — the DOM is heap, and `JsonElement`'s retention (§10) is exactly the shape that looks like a leak |
| **TSan** | **no subject** — nothing in the module is concurrent, and §12 records that rather than asserting safety |
| **`/proc/self/fd`** | **not applicable** — this module opens no descriptor |

Note the contrast with `modules/io`: there the descriptor count was the primary instrument and
LSan was explicitly not a substitute. Here the reverse holds.

---

## 15. Bounded tickets and recommended order

```
#2111  a std:: exception escapes four parse doors on a number literal   (P1, S) ── FIRST
#2112  an embedded NUL silently truncates the document                  (P2, S) ── independent
#2113  JsonEncodedText's two Encode overloads disagree (SR-AUD-329)     (P2, S) ── independent
#2114  JsonValue's integer accessors truncate (SR-AUD-328, narrowed)    (P2, S) ── independent
#2115  DESIGN: two JsonDocumentOptions flags are inert (SR-AUD-326)     (P2, M) ── needs_user
#2116  Deserialize discards its options (SR-AUD-330)                    (P2, S) ── AFTER #2115
#2117  BLOCKED: JsonElement disposal state (SR-AUD-324) — OBJECT LAYOUT (P2, M)
#2118  BLOCKED: GetRawText loses the source (SR-AUD-325) — ARCHITECTURAL(P2, L)
#2119  DEFERRED VERIFICATION: does #1893's deep-nesting residual survive? (P3)
#2120  documentation and gated-behaviour pins                           (P3, S) ── LAST
```

**Recommended order: #2111, then #2112, then #2113/#2114.** #2111 first because it is the only
one reachable from eight characters of untrusted text that terminates the process.

---

## 16. Compatible versus blocked or deferred

| Ticket | Compatible? | Why |
|---|---|---|
| #2111 | **yes** | an escaping exception's type changes; nothing valid is affected |
| #2112 | **yes, with a documented narrowing** | a truncated document starts being rejected |
| #2113 | **yes, with a documented narrowing** | one overload stops accepting malformed bytes |
| #2114 | **yes, with a documented narrowing** | a non-integral value stops silently truncating |
| #2115 | **no** — design; the two flags are not equally implementable (§6.2) |
| #2116 | **yes** — but its observable payoff depends on #2115 |
| #2117, #2118 | **no** — object-layout / architectural |
| #2119 | **no** — deferred verification |

**Next unit: `modules/net-http-headers`** — 5 open findings, **two `high`**, below the ≥6
threshold but carrying the recorded **CCF-021 promotion obligation** with all five members
present. This batch was instructed not to begin it.

---

## 17. Deferred evidence

`/rv/tmp/runtime/src/libraries/` is absent. These are **not** decided by this review:

- whether .NET rejects or accepts a document with an embedded NUL, and with what exception —
  #2112 chooses **rejection with `JsonException`** and records it as this port's choice, on the
  strength of the space-control differential rather than on a reference;
- .NET's exact exception and message for a `double`-overflowing number literal — #2111 preserves
  nlohmann's text inside a `JsonException` rather than inventing a .NET-shaped message;
- whether `AllowTrailingCommas` should be honoured or rejected when the parser cannot honour it
  (#2115);
- #1893's deep-nesting residual (#2119).

---

## 18. Exclusions

- `vendor/nlohmann/json.hpp` — third-party source, never modified.
- `Utf8JsonReader`, async serialization, cancellation and stream ownership — **absent from this
  port** (§2), so those checklists have no subject.
- Reflection-based serialization — a permanent deviation.
- SR-AUD-327's implementation — `docs/OwnedTreeLifetimeContractPlan.md` owns it; #1888/#1889/#1894
  stay blocked.
- CNA and mobile-eggbert — not inspected; #1773 stays blocked.

---

## 19. Completion criteria

This review (#2110) is complete when this document exists, each of the seven open findings has
exactly one disposition in §4, each post-audit defect carries a ticket or an explicit
"recorded, not ticketed", and §15's tickets are in `plan.sqlite3`. **It is complete on those
terms and remediates nothing by itself.**

`modules/text-json` is closed for *compatible* work when #2111–#2114, #2116 and #2120 are `done`,
SR-AUD-328, 329 and 330 are `remediated`, and SR-AUD-324/325/326/327 each carry a blocked or
design ticket and a behaviour pin.

---

## 20. Implementation record

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.

### 20.1 #2111 and #2112 landed together — four doors each, and a fourth door found by reading

Both are post-audit defects with **no `SR-AUD-*` identifier**, so the audit index is unchanged at
**138 remediated / 226 confirmed / 364 total**.

**#2111 had a door the probe never reached.** §7.1 measured three leaking doors —
`JsonDocument::Parse`, `JsonNode::Parse` and `JsonSerializer::Deserialize`. A grep for
`parse_error` while implementing found a **fourth**: `Utf8JsonWriter::WriteRawValue`, which
validates through the same parser with the same `parse_error`-only catch. Ticketed scope came
from the probe; **implementation scope came from the grep**, and the difference is one whole
public door.

The repair is what §7.1 predicted: catch the **base** nlohmann exception instead of only
`parse_error`, at each of the three code sites (the fourth caller,
`JsonSerializer::Deserialize`'s `JsonDocument` overload, delegates to `JsonDocument::Parse` and
was fixed by that one edit). Each site keeps its **own message wording**, so nothing but the
exception's type changes.

**#2112's guard is one function, not three copies.** `System::Text::Json::detail::RejectEmbeddedNul`
lives in a new internal header `System/Text/Json/detail/JsonParseGuard.hpp` and every entry point
that hands caller text to the parser goes through it. **No new component edge** — the header is
inside `Text.Json`. The graph stays at **41 modules / 91 edges**.

`Utf8JsonWriter::WriteRawValue` is the **sharper** of #2112's doors: validation stopped at the NUL
and *passed*, while the **full** text — everything after the NUL included — was appended to the
buffer, so the written document would have contained text that was never validated.

**A deliberate non-narrowing, pinned:** an escaped NUL inside a *string value* is legal JSON and
stays accepted, producing a 3-character string. #2112 rejects the **raw byte in the document
text**, never the escaped character in a value.

### 20.2 #2111/#2112 evidence

**+18 permanent regressions** — `SharpRuntimeTests_Text_Json` 244 → **262**, in a new
`JsonNamespaceReviewTests.cpp`. Eight of the eighteen are **pins of behaviour this review measured
and deliberately did not change**: the 100,000-level no-overflow result, the two document options
that *do* work, the two that are *inert* (a pin of a known defect, not an endorsement —
#2115 owns it), `JsonElement`'s already-correct integer accessors (#2114 owns the surviving half),
the disposal guards that already work plus the one that does not (#2117), and the malformed text
the parser already rejects.

**Two mutations**, both against the final source, control clean, all four files restored
**byte-identical**:

| Mutation | Restores | Tests failed |
|---|---|---:|
| T1 | the three catch sites narrowed back to `parse_error` | **4** |
| T2 | the NUL guard disabled | **4** |

The load-bearing assertion for T1 is `ExpectNoStdExceptionEscapes`, which catches
`System::Exception` first and `std::exception` second. A bare
`EXPECT_THROW(..., JsonException)` would **not** have discriminated: before #2111 the escaping
type was `nlohmann::detail::out_of_range`, which is a `std::exception` and **not** a
`System::Exception`, so the process died at the call site rather than throwing something the test
could observe.

**The control that keeps #2112 honest is itself a test.**
`THECONTROLTheSameDocumentWithASpaceWasAlreadyRejected` asserts that the identical document with
a space instead of the NUL was *always* rejected. If that control ever starts passing, the
finding's premise is gone and #2112's guard is measuring nothing.

**ASan + UBSan + LSan clean over 30,000 rejections and 18,000 acceptances**
(`build-probe/2111_probe1_san.log`), control proven live, with both repaired `.cpp` bodies
compiled *into* the instrumented translation unit because `Text.Json` is a `STATIC` component.

**No public signature, member, base-class, virtual, vtable, object-layout or
exception-specification change.** One new internal header; no public type gained or lost a member.
