<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Remaining approval decisions — one packet, six groups

Written by ticket **#1924** on 2026-07-31, after #1919 closed the last approved
item in the post-audit queue. It exists so the remaining work can be approved or
rejected **in batches by shared consequence**, instead of one ticket at a time.

**Nothing in this document has been implemented.** Every group states the exact
current behaviour, the .NET behaviour, a reproducer, and — in §"Approval
wording" — a sentence that can be copied verbatim into the next batch prompt.

**Scope of this packet.** Every ticket with status `needs_user` (7), plus the
`blocked` tickets whose blocker is a user design decision rather than an
external dependency (3), plus the two follow-ups #1919 discovered (2). Ticket
**#1773** is excluded: it is blocked on CNA and mobile-eggbert deliberately
upgrading sharp-runtime, which is an external event and not a decision to be
taken here. Tickets **#1888**, **#1889** and **#1896** are listed in §E.4 as
**already declined** and are *not* re-proposed.

---

## 0. Summary and recommended batching

| Group | Tickets | Shared consequence | ABI | Layout | Recommend |
|---|---|---|---|---|---|
| **A** | #1854, #1862 | a validation .NET does by throwing cannot be done by a `noexcept` function | none | none | **Approve A(i)** — drop `noexcept`, throw |
| **B** | #1858, #1865 | invariant-culture separators and overflow taxonomy in the numeric parsers | none | none | **Approve B, split** — take #1865 whole, take only #1858's overflow half |
| **C** | #1879, #1884 | text the library **accepts** becomes strict | none | none | **Approve C** |
| **D** | #1863 | text the library **emits** changes | none | none | **Approve D** |
| **E** | #1897, #1899 | Text.Json / Xml.Linq owned-tree residuals | see rows | none | **Approve E1(B) and E2(D)** |
| **F** | #1925, #1926 | Collections comparison follow-ups from #1919 | see rows | none | **Defer both** |

**Groups A, B, C and D can safely be approved as ONE batch.** All ten of their
tickets are header-only or `.cpp`-body changes with **no public signature
change, no object-layout change, no vtable change and no mangled-name change**.
Their only cross-group interaction is the comma question, which #1858 and #1865
must answer the same way — §B exists to make that one answer.

**Group E must be approved item by item.** E1 and E2 have different
consequences from each other and from anything in A–D.

**Group F should be deferred.** Neither is a shipped-behaviour defect in the
population #1912 closed; both are follow-ups worth deciding *after* the A–D
batch, when there is evidence about whether the port's users key containers on
composite floating types at all.

---

## A. `noexcept` versus validation — #1854, #1862

### A.1 Tickets and identifiers

| Ticket | Type | Finding | Family |
|---|---|---|---|
| #1854 | `ReadOnlyMemory<T>` ctors ×3, `HashCode::AddBytes` | SR-AUD-043b | CCF-005, item CCF5-E |
| #1862 | `Single::Round(float,intcs)`, `Double::Round(double,intcs)` | SR-AUD-029 | CCF-007, item CCF7-4 |

### A.2 Root cause, shared

.NET validates an argument by **throwing**. The port's counterparts are
declared `noexcept` (and one is additionally `constexpr`), and a `throw` from a
`noexcept` function is `std::terminate`, not an exception. So the validation
cannot be added without changing the exception specification. Two independent
findings on unrelated types with the identical decision shape; #1854's own note
records that they should be decided together for one project-wide convention.

### A.3 Current behaviour vs .NET

| Site | Current | .NET |
|---|---|---|
| `ReadOnlyMemory<T>(const T*, intcs)` — `ReadOnlyMemory.hpp:49-50`, `constexpr noexcept` | a negative length is accepted and produces a span of that length | `ArgumentOutOfRangeException` |
| `ReadOnlyMemory<T>(vector&)` L58-59, `(ArraySegment)` L67-69, `noexcept` | same | same |
| `HashCode::AddBytes(const ReadOnlySpan<uint8_t>&)` — `HashCode.hpp:92`, `noexcept` | same | same |
| `Single::Round(float, intcs)` — `Single.hpp:236`, `noexcept` | computes `std::pow(10, digits)` unchecked; out-of-range `digits` returns a spurious value or NaN | `ArgumentOutOfRangeException("digits", "Rounding digits must be between 0 and 6, inclusive.")` |
| `Double::Round(double, intcs)` — `Double.hpp:282`, `noexcept` | same | same, limits 0–15 |

### A.4 Reproducer

```cpp
// #1862 -- returns a spurious value instead of throwing
float  a = System::Single::Round(1.2345f, 99);
double b = System::Double::Round(1.2345,  -3);

// #1854 -- accepted; .NET throws
std::vector<std::uint8_t> v{1,2,3};
System::ReadOnlyMemory<std::uint8_t> m(v.data(), -1);
```

### A.5 Severity

**#1862 is a live wrong-answer defect** with no diagnostic. **#1854 is now pure
defence in depth**: ticket #1852 (SR-AUD-043a, landed 2026-07-30) validates
`Span`/`ReadOnlySpan` construction, so a negative-length span can no longer
reach `HashCode::AddBytes` through the public surface. The `ReadOnlyMemory`
constructors themselves remain directly reachable.

### A.6 Options

- **A(i) — drop `noexcept` (and the one `constexpr`), throw.** Full .NET parity.
- **A(ii) — keep `noexcept`, clamp.** `ReadOnlyMemory` clamps to empty; `Round`
  clamps `digits` to its valid range. A documented, permanent deviation.
- **A(iii) — split.** Take A(i) for `Round` (a wrong answer today) and A(ii)
  for `ReadOnlyMemory` (defence in depth). Rejected as a recommendation: it
  leaves the project with two conventions for one question.

**Recommended: A(i).** It is what .NET does, and `noexcept` on a function whose
whole job is to reject bad input is a promise the port should not be making.

### A.7 Exactly what changes

Five exception specifications and one `constexpr`. **No parameter list, no
return type, no object layout, no vtable, no mangled name.** An Itanium mangled
name does not encode `noexcept` for a non-function-pointer parameter, so
**there is no ABI symbol break** — the effect is source-level only.

### A.8 Source compatibility

Breaks exactly three spellings, all rare: `static_assert(noexcept(expr))` over
one of the five; taking a pointer-to-function with an explicit `noexcept` type;
and using `Round` in a `constexpr` context (only `ReadOnlyMemory`'s L49 ctor is
`constexpr` today, and it stays constructible — it just stops being usable in a
constant expression). No iterator or template effect.

### A.9 Test, sanitizer, performance, rollback

Add-only tests per site: valid input unchanged, each invalid input throws with
the exact .NET message, and `TryParse`-style siblings unaffected. Sanitizers are
irrelevant — this is an argument-validation contract, and ASan/UBSan/LSan cannot
see a missing throw. Performance: one comparison on a path that already calls
`std::pow`; unmeasurable. Rollback: restore the five specifiers.

### A.10 Approval wording

> Approve dropping `noexcept` — and the one `constexpr` on
> `ReadOnlyMemory.hpp:49` — from `ReadOnlyMemory<T>`'s three constructors,
> `HashCode::AddBytes`, `Single::Round(float,intcs)` and
> `Double::Round(double,intcs)`, so that each throws
> `ArgumentOutOfRangeException` with .NET's message and parameter name on an
> invalid argument. No parameter list, return type, object layout, vtable or
> mangled name changes. Tickets #1854 and #1862.

---

## B. Numeric text grammar — #1858, #1865

### B.1 Tickets and identifiers

| Ticket | Type | Finding | Family |
|---|---|---|---|
| #1858 | `Decimal::Parse`/`TryParse` | SR-AUD-035 tail | CCF-005 Decimal slice |
| #1865 | `Single`/`Double` `Parse`/`TryParse` | SR-AUD-033 parse tail | CCF-007, item CCF7-6 |

Each has a **separator** half and an **overflow** half. The separator halves
must be answered the same way; the overflow halves are independent.

### B.2 Root cause, shared

Both parsers implement a C++ subset of .NET's default `NumberStyles`. .NET's
`NumberStyles.Number` (Decimal) and `NumberStyles.Float | AllowThousands`
(Single/Double) both accept invariant-culture **group separators**; neither
port does, and `Decimal` additionally reuses `,` for something else entirely.

### B.3 Current behaviour vs .NET — the four halves

| # | Input | Current | .NET | Change class |
|---|---|---|---|---|
| B-1 | `Decimal.Parse("1,5")` | **`1.5m`** — `,` is treated as a decimal point | `15m` — `,` is a group separator | **value of an accepted input silently changes (10×)** |
| B-1 | `Decimal.Parse(" 1,234.5 ")` | `FormatException` | `1234.5m` | rejected → accepted |
| B-2 | `Decimal.Parse("79228162514264337593543950336")` | `FormatException` | `OverflowException("Value was either too large or too small for a Decimal.")` | exception type changes |
| B-3 | `Double.Parse("1,234.5")` | `FormatException` | `1234.5` | rejected → accepted |
| B-4 | `Double.Parse("1e999")` | `FormatException` | `+∞`, no throw | **throw → value** |

### B.4 Severity

**B-1 is the only genuinely dangerous row in groups A–D.** It changes the
*value* of input that already parses successfully: a program that reads
`"1,5"` from a config file gets `1.5` today and would get `15`. Nothing in this
repository feeds comma input, so nothing here breaks — the risk is entirely
downstream and entirely silent. B-2, B-3 and B-4 are safe: each either widens
what is accepted or changes an exception type on input that fails today.

### B.5 Options

- **B(i) — adopt .NET fully** (all four rows). Maximum parity, accepts B-1's
  silent value change.
- **B(ii) — adopt everything except B-1's re-interpretation of `,`**: take
  B-2, B-3, B-4, and leave `Decimal`'s comma as a decimal point as a
  documented deviation. Loses parity on one row; loses **consistency**, since
  `,` would then mean "decimal point" to `Decimal` and "group separator" to
  `Double`.
- **B(iii) — status quo**, all four documented as deviations.

**Recommended: B(i), staged.** Land B-2, B-3 and B-4 first (they are pure
widenings and an exception-type change on already-failing input), then B-1 as
its own commit with the value change stated in the commit message and a
migration note, so it can be reverted alone. Approving B(i) as a whole is
reasonable; approving it *as one commit* is not.

### B.6 Exactly what changes

Internal parse logic only. `Decimal` gains a private
`enum {OK, Malformed, Overflow}` returned by a helper so `Parse` can map
overflow to `OverflowException` while `TryParse` still returns `bool`.
`Single`/`Double` gain a group-aware pre-pass (`std::from_chars` has no
grouping mode) and a `result_out_of_range`-with-all-chars-consumed branch.
**No public signature, no object layout, no ABI.**

### B.7 Source compatibility, tests, rollback

Source-compatible. Behaviour-compatible except B-1. Downstream migration: a
caller that relied on `,` as a decimal point must switch to `.`; there is no
compiler diagnostic for this, which is why B-1 needs its own commit and its
own migration note. Two existing tests, `DecimalTests2.Parse_*_PendingApproval`,
pin today's behaviour and must be inverted when B-1 lands. Sanitizers cannot
see any of this. Performance: one extra pass over the input string. Rollback:
per-commit revert.

### B.8 Approval wording

> Approve, as separate commits: (1) `Single`/`Double` `Parse`/`TryParse`
> accepting invariant-culture `,` group separators per
> `NumberStyles.Float | AllowThousands`, and returning ±Infinity rather than
> throwing on a finite-overflow magnitude; (2) `Decimal::Parse` throwing
> `OverflowException` rather than `FormatException` on an out-of-range
> magnitude; and (3) — **as its own commit, with a migration note** —
> `Decimal::Parse` treating `,` as a group separator per `NumberStyles.Number`,
> accepting that `Decimal.Parse("1,5")` changes from `1.5` to `15`. No public
> signature or object-layout change. Tickets #1858 and #1865.

---

## C. Text the library **accepts** becomes strict — #1879, #1884

### C.1 Tickets and identifiers

| Ticket | Type | Finding | Family |
|---|---|---|---|
| #1879 | `DateTime`, `DateTimeOffset`, `TimeOnly`, `DateOnly` `TryParse`/`Parse` | SR-AUD-007b, SR-AUD-009, SR-AUD-061 | CCF-002, class CCF2-D |
| #1884 | `String::Format`, `FormattableString` | SR-AUD-015 tail | CCF-012 |

### C.2 Root cause, shared

Both accept malformed text and produce a plausible-looking answer instead of
rejecting it. The date/time parsers verify separator *positions* and then run
one `std::sscanf` **prefix** conversion, never checking that the conversion
consumed the whole string; the composite formatter rewrites its output as it
goes instead of parsing the format string once.

### C.3 Current behaviour vs .NET — reproducers

```cpp
DateTime  d;  DateTime::TryParse("2024-06-15junk", d);        // true  -> must be false
DateTime::TryParse("2024-06-15 10:xx:00", d);                 // true, MIDNIGHT fabricated
DateTime::TryParse("2024-06-15T10:20:30.1234", d);            // true  -> must be false
DateTimeOffset::TryParse("2024-06-15T10:20:30+2:5", o);       // true, offset +125 MINUTES
TimeOnly::TryParse("10:20:30.abc", t);                        // true
DateOnly::TryParse("2024-06-15 10:20:30", dt);                // true
```

The full before/after list is `docs/DateTimeValidationBoundaryPlan.md` §8.2
(13 inputs) and `docs/CompositeFormatBoundaryPlan.md` §20.1 (14 rows).

### C.4 Severity

High and silent. `"2024-06-15 10:xx:00"` parsing to **midnight** is a wrong
answer a caller cannot detect; `+2:5` becoming a 125-minute offset is a wrong
answer that survives round-tripping. #1879 additionally removes `std::sscanf`'s
formally undefined behaviour on an out-of-`int` numeral.

### C.5 Options

- **C(i) — adopt .NET's grammar** in both. Every listed input starts returning
  `false` from `TryParse` and throwing `FormatException` from `Parse`.
- **C(ii) — status quo**, documented.
- **C(iii) — #1879 only.** Reasonable if the composite-format output change is
  unwanted; #1884 is the one that also alters *emitted* text (alignment).

**Recommended: C(i).** Accepting `"2024-06-15junk"` as a valid date is not a
compatible convenience; it is a wrong answer with no diagnostic.

### C.6 Exactly what changes

`sscanf` is replaced by a full-consumption scanner in all four parsers;
`String::Format` parses the format string once. **No public signature, no
object layout, no ABI, no iterator or template effect.** What changes is the
accepted **language** and, for #1884, the emitted text for alignment.

### C.7 Compatibility, tests, rollback

Source-compatible; **behaviour-incompatible by design**. A caller feeding text
that was silently accepted starts getting `false`/`FormatException`, which is
the point — and unlike group B, the caller *finds out*. Every currently-valid
shape must still parse to the identical value, which is the larger half of the
test plan. Sanitizers cannot see it. Performance: a hand-written scanner
replacing `sscanf` is expected to be neutral-to-faster; measure and report.
Rollback: per-ticket revert.

### C.8 Approval wording

> Approve making the accepted textual grammar strict: (1) `DateTime`,
> `DateTimeOffset`, `TimeOnly` and `DateOnly` `TryParse` return `false` — and
> `Parse` throws `FormatException` — for every input listed in
> `docs/DateTimeValidationBoundaryPlan.md` §8.2, including trailing text and
> unparseable time text that today yields midnight, with `std::sscanf` replaced
> by a full-consumption scanner; and (2) `String::Format` and
> `FormattableString` adopt .NET's composite-format grammar exactly as written
> in `docs/CompositeFormatBoundaryPlan.md` §20.7. Every currently-valid input
> must still produce the identical value. No public signature or object-layout
> change. Tickets #1879 and #1884.

---

## D. Text the library **emits** changes — #1863

### D.1 Ticket

#1863 — `Single::ToString(value, format)` (`Single.hpp:602`),
`Double::ToString(value, format)` (`Double.hpp:685`). SR-AUD-033 format slice,
CCF-007 item CCF7-5. `Half` inherits by delegation.

### D.2 Root cause

The formatter is an `std::ostringstream` back end, which cannot emit a
three-digit exponent or insert group separators. `N` is therefore formatted
identically to `F`, `E` uses `std::scientific`'s two-digit exponent, and
`G`/`G9`/`G17` go through `setprecision` rather than shortest-round-trip.

### D.3 Current vs .NET

| Call | Current | .NET |
|---|---|---|
| `ToString(1234.5, "N2")` | `1234.50` | `1,234.50` |
| `ToString(1.25, "E2")` | `1.25E+00` | `1.25E+000` |
| `ToString(x, "G17")` / `"G9"` | `setprecision` width | round-trip width |
| `ToString(x, "G")` / `"R"` / no format | already shortest-round-trip via `to_chars` | same — **must be preserved** |

### D.4 Severity, options, consequences

Medium: wrong output text, always visible, never silent. Options are
**D(i)** adopt .NET (needs a custom formatter or post-processing over the
`ostringstream` result) or **D(ii)** document the subset permanently.
**Recommended: D(i).** No signature, layout or ABI change. The real cost is
downstream: **any golden file, snapshot test or serialized text that captured
`1234.50` or `1.25E+00` changes**, and there is no compiler diagnostic. The
`to_chars` default/`R` fast path must be preserved unchanged — that is the one
non-negotiable constraint, recorded in
`docs/FloatingValueFidelityPlan.md` §19.2. Tests: exact-text assertions per
format specifier plus round-trip tests. Rollback: revert.

### D.5 Approval wording

> Approve changing the text `Single::ToString(value, format)` and
> `Double::ToString(value, format)` emit: `E`/`e` gain a sign and at least
> three exponent digits, `N`/`n` gain invariant-culture group separators with
> two default decimals, and `G`/`G9`/`G17`/`R` become shortest-round-trip and
> round-trip widths, with the existing `to_chars` default/`R` fast path
> preserved. Accepting that captured output text changes. No public signature
> or object-layout change. Ticket #1863.

---

## E. Text.Json and Xml.Linq owned-tree residuals — #1897, #1899

These are **not** a batch. Each needs its own answer.

### E.1 #1897 — `JsonNode::Parse` overflows the stack on deep untrusted text

**Identifiers:** CCF-019, probe X28c. No `SR-AUD-*`.

**Current:** `fromNlohmann` recurses once per nesting level while turning the
parsed document into `JsonNode` objects, so `JsonNode::Parse` of 20,000 nested
arrays crashes with a stack overflow (ASan frames are in the port's own
recursion, not nlohmann's parser). **This is the only CCF-019 case reachable
from untrusted input.**

**Corrected premise, already recorded** (`docs/OwnedTreeLifetimeContractPlan.md`
§40.2): the plan once presented "bound the nesting depth" as a *new* grammar
decision. It is not new — `JsonDocumentOptions::DefaultMaxDepth = 64` already
exists in this module, `JsonDocument::Parse` already applies it and throws
`JsonException("The maximum configured depth of N has been exceeded.")`, and
`JsonTests.ParseExceedingDefaultMaxDepth_Throws` already pins it. **The
module's two JSON parse entry points disagree about the same untrusted text.**

**The question:** should `JsonNode::Parse`
**(A)** reject text nested deeper than the existing `DefaultMaxDepth = 64`,
matching `JsonDocument::Parse` and .NET's own `JsonReaderOptions.MaxDepth`
default — an **accepted-input change**, because text that crashes today starts
being rejected; or
**(B)** build the tree **iteratively** with an explicit worklist, the same shape
#1895 used for teardown — **fully compatible**, no accepted-input change, no
layout or signature change, and it removes the crash without rejecting
anything?

**Recommendation: (B), then (A).** The plan's own earlier recommendation was
(A) alone, on the grounds that (B) leaves the two entry points disagreeing.
That reasoning is sound but incomplete: (B) is *compatible* and removes a
crash on untrusted input, so it can land immediately, whereas (A) is an
accepted-input change that should be decided on its own merits. Landing (B)
first removes the security-relevant crash at zero compatibility cost; (A) can
then follow to make the module self-consistent. Doing (A) alone also removes
the crash, so either order is defensible — but (B) first is strictly safer.

**No layout, signature or ABI change in either option.** Rollback: revert.

> **Approval wording:** Approve making `System::Text::Json::Nodes::JsonNode::Parse`
> build its tree iteratively with an explicit worklist instead of recursively,
> removing the stack overflow on deeply nested untrusted text with no change to
> the accepted input, no signature change and no layout change. Ticket #1897.
> *(Optionally add: and then apply the existing `DefaultMaxDepth = 64` to it, so
> it rejects the same deeply nested text `JsonDocument::Parse` already rejects.)*

### E.2 #1899 — the Xml.Linq borrowed views are documented but not safe

**Identifiers:** SR-AUD-333, CCF-019, probes X15 and X17 — the last two
ASan-confirmed use-after-free cases in CCF-019. #1898 (done) made the contract
explicit and testable; #1899 would make violating it impossible.

**The impossible requirement, disposed of** (`docs/OwnedTreeLifetimeContractPlan.md`
§42.2): the original wording asked `Extensions::Ancestors`/`AncestorsAndSelf`
to "return owning handles". That cannot be implemented **at any layout cost**,
for two independent reasons: the topmost ancestor has no parent to own it, and
`XElement`/`XDocument`/`XContainer` are routinely automatic-storage (51 such
declarations in this repository's own tests) with no control block at all —
so adding `enable_shared_from_this` does not rescue it either, because
`shared_from_this()` on an automatic-storage element throws `bad_weak_ptr`.

**The question:** take
**(B)** `XElement::getAttributesProperty()` returns **by value** — every
ordinary call site keeps compiling, `&el->getAttributesProperty()` stops
compiling, the return calling convention changes, and each call gains one
vector copy;
**(D)** add a visitor spelling
`template <class F> void Extensions::ForEachAncestor(const shared_ptr<XNode>&, F&&)`
whose borrowed pointers cannot outlive the call, **beside** the existing
`Ancestors`/`AncestorsAndSelf`, which keep their signatures and their
now-documented contract; or
**(E)** both, plus removal of the borrowed spellings, in a coordinated
ABI-breaking release together with #1889?

**Recommendation: (D) now, (B) and (E) only in a coordinated ABI-breaking
release.** (D) is **purely additive** — no break at all, no layout change, no
existing signature touched — and gives callers a spelling that is safe by
construction. (B) is a real ABI change (return calling convention) for one
accessor. (E) depends on #1889, which is declined (§E.4).

> **Approval wording:** Approve adding
> `template <class F> void System::Xml::Linq::Extensions::ForEachAncestor(const std::shared_ptr<XNode>&, F&&)`
> — a purely additive visitor whose borrowed pointers cannot outlive the call —
> beside the existing `Ancestors`/`AncestorsAndSelf`, which keep their
> signatures and their documented borrowed-view contract. No existing signature,
> object layout or ABI changes. Ticket #1899, option D.

### E.3 #1894 — blocked on a dependency, not on a decision

#1894 (negative consumer fixtures + sanitizer closure for CCF-019) is `blocked`
and **needs no answer**. Its purpose is one negative fixture site per spelling a
CCF-019 repair outlawed, and **no landed CCF-019 repair has outlawed any
spelling** — every one is source-compatible by construction. Until #1888 or
#1899 lands there is literally nothing for a fixture to reject. Its other half,
permanent sanitizer closure, is already delivered (ASan+UBSan+LSan clean over
218/218 `Text_Json` and 184/184 `Xml_Linq`). Approving §E.2 option D does **not**
unblock it either — D outlaws nothing. It is listed here only so it is not
mistaken for a pending question.

### E.4 Declined, preserved, and **not** re-proposed

Per the standing rule that a declined ticket is not reopened without new
evidence that materially invalidates the decision — and there is none — these
are recorded, not proposed:

| Ticket | Declined | Preserved design |
|---|---|---|
| #1888 | delete `JsonNode` copy/move, make `DetachParent` protected — public source break | plan §37; measured in-repo cost is **one** test edit and zero copy sites |
| #1889 | version-guard the `JsonArray`/`JsonObject` enumerators — object-layout **and silent ABI** break (a return type is not part of an Itanium mangled name) | plan §39, approval wording §39.12 |
| #1896 | one cached ancestor/depth pointer to make the O(d²) attach guard O(1) — layout change | exact future wording in the ticket's notes |

All three should be revisited **together**, in a future coordinated
source-and-ABI-breaking release, not individually.

---

## F. Collections comparison follow-ups from #1919 — #1925, #1926

Both were discovered and measured **while implementing #1919** and are recorded
in `docs/CollectionsComparisonContractPlan.md` §17 and §19.1. Neither is a
member of the #1912 population, which is closed.

### F.1 #1925 — a nullable or composite floating key keeps raw IEEE equality

`System::detail::DefaultKeyHash`/`DefaultKeyEqual` select on
`std::is_floating_point_v`, so they select for `double` but **not** for
`std::optional<double>`, `std::pair<int,double>`, `std::tuple<double>` or any
user type holding a floating member.

```cpp
Dictionary<std::optional<double>, int> d;
d.Add(std::optional<double>(NaN), 1);
d.TryGetValue(std::optional<double>(NaN), v);   // FALSE
```

The key is unfindable **by the very object that was inserted**. .NET's
`Dictionary<double?,V>` finds it. Today's behaviour is pinned by
`CollectionsComparisonContract.NullableFloatingKeysKeepRawIeeeEqualityForNow`
so it cannot change silently.

**Consequence of repairing it:** the policy must recurse into
`optional`/`pair`/`tuple`, which moves the backing type — and therefore the
public `SetType`/`MapType` — of a **further family of instantiations**, exactly
the class of change #1919 needed approval for. **Recommend: defer.** It is a
real divergence, but the affected shape (a nullable or tuple floating key in a
hashed container) is unusual in ported game code, and the decision is better
taken with evidence that it occurs.

> **Approval wording (if taken):** Approve extending
> `System::detail::DefaultKeyHash`, `DefaultKeyEqual` and `DefaultKeyLess` to
> recurse into `std::optional`, `std::pair` and `std::tuple`, changing the
> backing container type — and the public `SetType`/`MapType` alias — of every
> Collections instantiation keyed on such a composite containing a
> floating-point member, on the same terms as #1919. Ticket #1925.

### F.2 #1926 — `long double` hashed insertion is 1.300× slower

libstdc++ specialises `__is_fast_hash<std::hash<long double>>` to `false`, so
an `unordered_map<long double, …>` node **cached** its hash code.
`DefaultHash<long double>` is a new type for which the primary template says
`true`, so the cache is switched off. Measured over 7 alternating rounds,
`Dictionary<long double,int>` insert of 200k keys went **52.41 → 68.12 ms
(1.300×, noise 1.206)** — the only genuine regression in #1919's 19-workload
benchmark. The same container's **lookup** got *faster* (0.791×). `float` and
`double` are unaffected in both directions.

**The fix** is a specialisation of `std::__is_fast_hash<DefaultHash<long double>>`
to `false` behind `#ifdef __GLIBCXX__` — a **reserved libstdc++ internal this
project does not own**, and specialising a `std::` template not designated for
it is undefined by the letter of the standard. **Recommend: defer, leaning
wontfix.** It buys 1.3× on one workload on the rarest of the three floating
types, at the price of a non-portable specialisation. It would additionally
remove the only iterator-type movement #1919 caused, which is the one argument
in its favour.

> **Approval wording (if taken):** Approve specialising
> `std::__is_fast_hash<System::detail::DefaultHash<long double>>` to `false`
> behind `#ifdef __GLIBCXX__`, restoring libstdc++'s per-node hash-code caching
> for `long double` hashed containers and with it their pre-#1919 node and
> iterator types. Ticket #1926.

---

## G. What this packet deliberately does not do

- It does **not** estimate downstream migration from CNA or mobile-eggbert.
  Neither was inspected. Every compatibility statement above is derived from
  sharp-runtime's own public surface and its tracked consumer fixtures
  (`test/consumer/`, 10 fixtures / 74 negative sites).
- It does **not** re-propose #1888, #1889 or #1896 (§E.4).
- It does **not** touch #1773, which waits on an external event.
- It issues **no** new `SR-AUD-*` identifier. Audit numbering stays frozen at
  **364**.
