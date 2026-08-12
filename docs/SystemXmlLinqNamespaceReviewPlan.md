<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/xml-linq` (`System::Xml::Linq`) — namespace review plan

*Ticket #2195. Written 2026-08-10 on branch `claude/remediation-batch-1804-namespace-b1yjh5`.*

Every claim below was measured in **this** container from the **shipped** bodies, not inherited
from the audit text or from the previous handoff. The reproduction probes are
`build-probe/2195_probe1_surface.cpp` (log `…_surface.log`),
`build-probe/2195_probe2_malformed.cpp` (log `…_malformed.log`) and
`build-probe/2195_probe3_events.cpp` (log `…_events.log`), all compiled with
`build-probe/2195_compile.sh`, one translation unit at a time, at most two jobs.

`/rv/tmp/runtime/` is **absent** (re-verified 2026-08-10), so no .NET behaviour is assumed from
the reference source. Where a .NET rule is relied on, it comes from something this repository
already contains — most often `modules/xml`, which ships the resolver or sanitiser the Linq
layer bypasses. Where nothing in the repository settles a question, it is recorded as deferred
rather than guessed.

**No `SR-AUD-*` identifier is issued by this review. Audit numbering stays frozen at 364.**

---

## 1. Work unit 1 — why `modules/xml-linq`, verified rather than inherited

### 1.1 The audit decomposition, recounted from scratch

`audit/AUDIT_FINDINGS_INDEX.md` was re-parsed row by row at the start of this batch:

```
364 rows, 364 unique SR-AUD identifiers, none missing in 1..364
170 remediated
140 confirmed (plain)
 54 confirmed (design-complete)
```

That is exactly the triple the inherited handoff stated, and exactly the triple this batch's
brief predicted. (One row — SR-AUD-029 — carries a seventh column and needs a looser parse than
the other 363; a strict six-column regex silently reports 363/169 and misses it. Recorded so the
next recount does not "discover" a different total.)

### 1.2 The inherited claims about `xml-linq`, checked one by one

| Claim | Verdict |
|---|---|
| 4 open findings | **True** — SR-AUD-333, 334, 335, 336; no remediated finding in this module. |
| 1 high, and that high is blocked | **True** — SR-AUD-333, `confirmed (design-complete)`, owned by CCF-019; its remaining tickets are #1899 (blocked on one approval question), #1894 and #1896 (both blocked), and #1892/#1893 (`wontfix`). |
| 3 compatible-actionable findings | **Confirmed for two outright, corrected for the third.** SR-AUD-334 and SR-AUD-335 are compatible. **SR-AUD-336 is not** — §12.3 measures two independent blockers, one of which is the *same* `sizeof(XObject)` growth the user explicitly declined on 2026-07-31 for #1896. It splits into a compatible pin ticket (#2198) and a blocked implementation ticket (#2199). |
| no `/rv` dependency for those three | **True.** Every rule the repairs rely on is already implemented in `modules/xml` — see §5. |
| a small coherent namespace | **True** — 22 headers, 12 sources, 6 test files, one CMake target, one component (`Xml.Linq`), 5,533 lines total. |

### 1.3 Candidate scoring against the strongest alternatives

Recomputed from the index, restricted to units with open findings and no review plan:

| Candidate | Open | high | Compatible-actionable | Blocked | Approval-gated | Memory/lifetime risk | Public-input exposure | Security consequence | `/rv` needed | Cohesion | Can close compatible work |
|---|---:|---:|---:|---:|---:|---|---|---|---|---|---|
| **`xml-linq`** | **4** | 1 | **2 outright + 1 split** | 1 (its high **is** CCF-019) | 1 (SR-AUD-336's implementation half) | high — but that half is the blocked half | **high** — parse and serialise are the doors | **medium** — emits unparseable text; no code execution, no resource fetch | **no** | **high**, one namespace | **yes — two findings fully** |
| `io-isolated-storage` | 1 | **1** | 1 | 0 | 0 | medium (path escape) | **high** | **high** — an absolute caller path escapes the store | no | tiny | yes — the whole unit |
| `globalization` | 7 | 1 | ~2 | 0 | 1 (`Calendar` shape, 82 tests pin it) | high (TSan-confirmed culture race) | high | low | **yes, absent** (ICU collation/casing) | medium | partly |
| `text-regular-expressions` | 1 | 1 | ~0 | likely (stateful raw-`this`) | 0 | high | medium | low | no | tiny | probably not |
| `collections-object-model` | 1 | 1 | ~0 | likely (stateful raw-`this`) | 0 | high | low | low | no | tiny | probably not |

**Selection: `modules/xml-linq`, and the reason is not raw severity.** `io-isolated-storage`
carries the single highest-consequence *unblocked* defect in the corpus, and this review does
**not** claim it outranks nothing — §18.2 recommends it as the next unit and §19 of this batch's
work carries a scoped review of it. `xml-linq` is taken first because:

1. it is the only candidate where **two findings can be closed outright** in one context, and
   both close a *shared* root cause (§5, family X-C) rather than two unrelated ones;
2. its high-severity finding is **already design-complete and already partially repaired**
   (#1890/#1891/#1895/#1898 landed), so the compatible remainder is genuinely separable — the
   same separation the previous batch executed against blocked #1962;
3. **its defect is not merely a fidelity gap.** §4.2 measures that a legal, `.NET`-shaped tree
   serialises to text that **this module's own parser rejects** (`XML_ERROR_PARSING_ATTRIBUTE`).
   Silent data corruption on a round trip through a public door outranks a namespace-loss
   parity gap, and it was not visible from the index summary;
4. `xml-linq`'s repairs need no new component edge and no reference data.

`io-isolated-storage` is **not** deferred for being unimportant. It is deferred because it is
one finding, and taking it first would leave two closable findings and a shared root cause
untouched in a namespace nobody had scoped.

---

## 2. Namespace scope and file inventory

Component `Xml.Linq`, target `sharp_runtime_xml_linq`, `PUBLIC_DEPENDENCIES Core.Base Xml`.
No component edge is added, changed or removed by any ticket in this review.

**Public headers (22)** — `modules/xml-linq/include/System/Xml/Linq/`:

`Extensions.hpp`, `LoadOptions.hpp`, `ReaderOptions.hpp`, `SaveOptions.hpp`, `XAttribute.hpp`,
`XCData.hpp`, `XComment.hpp`, `XContainer.hpp`, `XDocument.hpp`, `XDocumentType.hpp`,
`XElement.hpp`, `XName.hpp`, `XNamespace.hpp`, `XNode.hpp`, `XNodeDocumentOrderComparer.hpp`,
`XNodeEqualityComparer.hpp`, `XObject.hpp`, `XObjectChange.hpp`, `XObjectChangeEventArgs.hpp`,
`XProcessingInstruction.hpp`, `XStreamingElement.hpp`, `XText.hpp`.

**Sources (12)** — `XAttribute.cpp`, `XCData.cpp`, `XComment.cpp`, `XContainer.cpp`,
`XDocument.cpp`, `XDocumentType.cpp`, `XElement.cpp`, `XNode.cpp`, `XObject.cpp`,
`XProcessingInstruction.cpp`, `XStreamingElement.cpp`, `XText.cpp`.

**Tests (6, 184 tests, all passing at review start)** — `XLinqBorrowedViewTests.cpp` (14),
`XLinqLifetimeTests.cpp` (32), `XLinqMutationConsistencyTests.cpp` (24), `XLinqNodeTests.cpp`
(77), `XLinqSupportTests.cpp` (20), `XLinqTeardownTests.cpp` (17).

**The bridge to `System::Xml`.** `XDocument::Parse`/`Load` build a `System::Xml::XmlDocument`
(tinyxml2-backed) and convert it; `WriteTo` writes through `System::Xml::XmlWriter`;
`SerializeTo` bypasses both and writes to a `std::ostream` directly. **That asymmetry is the
whole of §5's root-cause family.**

---

## 3. Complete public-surface inventory

### 3.1 Present

| Area | Surface |
|---|---|
| Object base | `XObject` — `getNodeTypeProperty`, `getParentProperty`, `getDocumentProperty`, `add_Changed`, `remove_Changed`, `add_Changing`, `remove_Changing`; copy/move all deleted |
| Node base | `XNode` — `SerializeTo`, `getNextNodeProperty`, `getPreviousNodeProperty`, `Remove`, `ReplaceWith`×2, `NodesBeforeSelf`, `NodesAfterSelf`, `CompareDocumentOrder`, `IsAfter`, `IsBefore`, `DeepEquals`, `GetDeepHashCode`, `ToString`×2, `WriteTo` |
| Container | `XContainer` — `getFirstNodeProperty`, `getLastNodeProperty`, `Add`×2, `AddFirst`×2, `Nodes`, `DescendantNodes`, `Element`, `Elements`×2, `Descendants`×2, `RemoveNodes`; protected `ValidateNode`, `RemoveNode`, `InsertNodeAt`, `AdoptObject` |
| Element | `XElement` — 2 constructors, `getNameProperty`/`setNameProperty`, `getValueProperty`/`setValueProperty`, `Add(attr)`, `Add(text)`, `Attribute`, `getAttributesProperty`, `Attributes`, `getFirstAttributeProperty`, `getLastAttributeProperty`, `getHasAttributesProperty`, `getHasElementsProperty`, `getIsEmptyProperty`, `RemoveAttributes`, `RemoveAttribute`, `RemoveAll`, `getAttributeValue`, `WriteTo`, `Save`×2, `Parse`, `Load`, `GetDeepHashCode` |
| Document | `XDocument` — `getRootProperty`/`setRootProperty`, `getDocumentTypeProperty`, `getDeclarationProperty`/`setDeclarationProperty`, `WriteTo`, `Save`×2, `Parse`, `Load`; `XDeclaration` |
| Attribute | `XAttribute` — 2 constructors, `getNameProperty`, `getValueProperty`/`setValueProperty`, `getIsNamespaceDeclarationProperty`, `getNextAttributeProperty`/`setNextAttributeProperty`, `getPreviousAttributeProperty`, `Remove`, `ToString` |
| Leaf nodes | `XText`, `XCData`, `XComment`, `XProcessingInstruction`, `XDocumentType` |
| Names | `XName` (4 constructors, `getLocalNameProperty`, `getNamespaceNameProperty`, `getNamespaceProperty`, `ToString`, `Equals`, `==`/`!=`, `GetHashCode`, `Get`×2, `std::hash` specialisation); `XNamespace` (`getNamespaceNameProperty`, `ToString`, `Equals`, `==`/`!=`, `GetName`, `operator+`, `Get`, `None`/`Xml`/`Xmlns`) |
| Comparers | `XNodeDocumentOrderComparer`, `XNodeEqualityComparer` |
| Options | `LoadOptions`, `ReaderOptions`, `SaveOptions` (flag enums with `|`/`&`) |
| Events | `XObjectChange`, `XObjectChangeEventArgs` (+4 static instances), `XObjectChangeEventHandler` |
| Streaming | `XStreamingElement` — `getNameProperty`/`setNameProperty`, `Add`×4, `WriteTo`, `Save`×2, `ToString`×2 |
| Extensions | `Extensions::Elements`, `Attributes`, `Descendants`, `DescendantNodes`, `Ancestors`, `AncestorsAndSelf`, `Remove`, `InDocumentOrder` (each with plain and `XName`-filtered forms) |

### 3.2 Absent — recorded so the review is not read as claiming coverage it does not have

- **Annotations** (`AddAnnotation`/`Annotation`/`Annotations`/`RemoveAnnotations`) — documented
  out of scope on `XObject`. This is not incidental: in .NET the `Changed`/`Changing` handler
  storage *is* an annotation, which is precisely why SR-AUD-336 has nowhere to put a handler
  (§12.3).
- **`BaseUri`, `IXmlLineInfo`** — depend on annotations; `LoadOptions::SetBaseUri`/`SetLineInfo`
  already document themselves as no-ops.
- **`XNode::CreateReader`/`XContainer::CreateWriter`** — documented out of scope.
- **`XElement::GetPrefixOfNamespace` / `GetDefaultNamespace`** — absent today. #2197 adds both
  (purely additive; they fall out of the scope resolver it has to build anyway).
- **`XElement`'s value-conversion `explicit operator T`** family (`(int)element` etc.) — absent;
  `getValueProperty()`/`getAttributeValue()` are the ported spelling. No open finding names it,
  and none is invented here.
- **`XNamespace` interning / reference equality** — deliberately value-based; documented on both
  `XName` and `XNamespace`.
- **`SaveOptions::OmitDuplicateNamespaces`** — accepted and inert. #2197 does **not** implement
  it and says so in the doc-comment; §18 records it as an exclusion.

---

## 4. The four findings, dispositions, and the tickets that carry them

| Finding | Severity | Status at review start | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-333 | high | `confirmed (design-complete)` | **blocked** — CCF-019; not touched by this batch beyond re-measurement | #1899 (blocked), #1894 (blocked), #1896 (blocked); #1892/#1893 `wontfix` |
| SR-AUD-334 | medium | `confirmed` | **compatible implementation** | **#2197** |
| SR-AUD-335 | medium | `confirmed` | **compatible implementation** | **#2196** |
| SR-AUD-336 | medium | `confirmed` | **split**: compatible pin + blocked implementation | **#2198** (compatible) + **#2199** (blocked) |

Two post-audit defects were found while inventorying, both **structurally equivalent to an
already-open `modules/xml` ticket** and both deliberately left to settle with their twin:
**#2200** (`XDocumentType::SerializeTo`, twin of #2084) and **#2201** (embedded NUL through the
direct serialisers, twin of #2085). **No `SR-AUD-*` identifier is issued for either**, exactly
as #2051/#2055/#2072/#2082/#2083 did.

### 4.1 SR-AUD-335 — the direct serialisers bypass sanitisers the module already ships → **#2196, compatible**

**Reproduced exactly as written** (`2195_probe1_surface.log`, block `L*`), and the audit's own
sentence — "the local parser accepts the comment but does not make the output valid" — is
confirmed by probe 2 (`D03`, `D05`: tinyxml2 accepts `<!--left--right-->` on re-read).

The premise is right and the surface is larger than the three types named. **Measured:**

| Door | `SerializeTo` (ToString / Save) | `WriteTo` (XmlWriter) |
|---|---|---|
| `XCData("left]]>right")` | `<![CDATA[left]]>right]]>` — **broken** | `<![CDATA[left]]]]><![CDATA[>right]]>` — **already correct** |
| `XComment("left--right")` | `<!--left--right-->` — **broken** | `<!--left- -right-->` — **already correct** |
| `XComment("trailing-")` | `<!--trailing--->` — **broken** | `<!--trailing- -->` — **already correct** |
| `XProcessingInstruction("p","left?>right")` | `<?p left?>right?>` — **broken** | `<?p left? >right?>` — **already correct** |
| `XProcessingInstruction("a?>b","d")` — the **target** | `<?a?>b d?>` — **broken** | **throws** `Invalid XML name: 'a?>b'.` — already correct |

**Five doors, not three**, and the fifth (the PI *target*) fails differently from the other
four: the writer *rejects* it where the direct serialiser emits it. That asymmetry is the
finding's real shape.

**Consequences, measured rather than asserted:**

- `L02` — a CDATA round trip is **lossy**: `left]]>right` comes back as `leftright]]>`
  (`roundtrip-lossless=0`). Probe 2 `D04` shows *why*: the one CDATA node re-reads as a CDATA
  node `left` **plus a text node** `right]]>`.
- `L08` — the PI round trip does not merely corrupt, it **throws**:
  `XML_ERROR_PARSING_DECLARATION`. The emitted document is not parseable by the module's own
  parser.
- `L07`/`D03` — the comment round trip is **silent**: this parser accepts the invalid comment,
  so nothing signals the corruption. A conforming parser would reject it.

**Repair (#2196).** Route the four direct serialisers through the behaviour
`modules/xml/src/System/Xml/XmlWriter.cpp` already implements — `sanitizeCDataText`,
`sanitizeCommentText`, `sanitizeProcessingInstructionText` — and the PI target through
`XmlConvert::VerifyName`, which the writer already calls. This is family **X-C** (§5.1): the
repair reuses a shipped validator instead of inventing a grammar, exactly as #2076 did in the
sibling namespace.

### 4.2 SR-AUD-334 — namespace URIs discarded on parse and on serialisation → **#2197, compatible**

**Reproduced, and materially understated.** The index calls this a case where "a namespaced
programmatic tree serializes/reparses as unqualified XML". It is that, and three further things
the audit does not say:

1. **It emits XML that this module's own parser rejects.** Two attributes differing only by
   namespace serialise as `<r x="1" x="2"/>`; re-reading that throws
   `XML_ERROR_PARSING_ATTRIBUTE` (probe 2 `D01`). Duplicate attribute names are not
   well-formed. This is **silent corruption at a public door**, not a fidelity gap.
2. **It destroys namespace declarations built the .NET way.** `XAttribute(XNamespace::Xmlns + "p", "urn:x")`
   — the exact spelling `XAttribute.cpp`'s own `ValidateAttribute` exists to validate — serialises as
   `<e p="urn:x"/>`: the declaration becomes an **ordinary attribute named `p`** (probe 1 `N10`,
   probe 2 `D02`). A round trip therefore silently unbinds every prefix in the document.
3. **`getIsNamespaceDeclarationProperty()` is wrong for parsed input, inconsistently.** For
   `<p:root xmlns:p="…" xmlns="…"/>` it answers **0** for `xmlns:p` and **1** for `xmlns`
   (probe 2 `D06`). The default form works only by accident — its literal local name *is*
   `"xmlns"` — while the prefixed form is misclassified because its name was never split.

**The root cause is one line, and it is family X-C again.** `XDocument.cpp`'s converter builds
every name from the raw qualified tag text:

```cpp
auto el = std::make_shared<XElement>(XName(srcEl->getNameProperty()));           // "p:root"
el->Add(std::make_shared<XAttribute>(XName(a->getNameProperty()), a->getValueProperty()));
```

while **the DOM layer directly underneath already resolves all three parts** —
`XmlNode::getLocalNameProperty()`, `getPrefixProperty()`, `getNamespaceURIProperty()` (which
walks ancestors for `xmlns`/`xmlns:prefix`), and `XmlAttribute::getNamespaceURIProperty()`,
which already implements the two rules that are easy to get wrong: an **unprefixed attribute has
no namespace** (an ancestor's default `xmlns` does not apply to it), and the **`xml` prefix is
built in**. The Linq converter calls none of them.

The serialisation half is symmetric: `XElement::SerializeTo`, `XElement::WriteTo`,
`XAttribute::ToString` and `XStreamingElement::WriteTo` all write
`name.getLocalNameProperty()`, with in-code comments recording that this was a deliberate choice
to avoid emitting Clark notation as an XML Name. **That choice was correct about Clark notation
and wrong about the alternative** — the alternative is a prefix, not a bare local name.

**Repair (#2197).**

- **Parse**: build `XName` from the DOM's resolved `(namespaceURI, localName)`, special-casing
  the `xmlns` prefix to `XNamespace::Xmlns` and the bare `xmlns` attribute to the unqualified
  name `"xmlns"`, so declarations survive as declarations.
- **Serialise**: give the element serialisers a namespace scope built from the declarations
  already carried as attributes, plus a prefix allocator for namespaces used but not declared.
  Emit qualified names and declarations.
- **Additive**: `XElement::GetDefaultNamespace()` and `GetPrefixOfNamespace()` — both fall out
  of the scope resolver, both are real .NET surface, neither changes layout.

**Undeclared prefixes are deliberately left alone** (`<p:r/>` with no `xmlns:p` in scope keeps
the local name `p:r` and an empty URI). Narrowing accepted input is the open question #2083
already owns at the DOM layer; this review does not answer it from the Linq side. §18 records
it as an exclusion and #2197 pins the current behaviour.

### 4.3 SR-AUD-336 — `Changed`/`Changing` accept handlers and never notify → **#2198 compatible + #2199 blocked**

**Reproduced across nine doors** (`2195_probe1_surface.log`, block `E*`): `setValueProperty`,
`Add(node)`, `Add(attribute)`, `setNameProperty`, `RemoveAll`, and an ancestor observing a
descendant's change — **every one reports 0**. The audit's second sentence is also confirmed:
`XLinqNodeTests.cpp:87` (`ChangedChangingEventAccessors_DoNotThrow`) asserts only that
registration does not throw, so it *preserves* the inert behaviour rather than pinning it.

**The premise is right. The classification "compatible-actionable" is not**, and §12.3 records
why with structural evidence rather than opinion. Two independent blockers:

1. **Per-object handler storage is an object-layout change.** `sizeof(XObject)` is **16** —
   vptr plus `XContainer* parent_`, with **no padding** (`alignof` 8). Adding one pointer takes
   it to **24** (measured directly, `2195_probe3_events.log`: `growth-per-object=8`), and every
   derived type grows with it: `XNode` 16, `XContainer` 40, `XElement` 128, `XAttribute` 120,
   `XText`/`XCData`/`XComment` 48, `XProcessingInstruction` 80, `XDocument` 56. **The user
   explicitly declined exactly this growth — `XObject` 16 → 24 — on 2026-07-31**, recorded in
   #1896's notes for the CCF-019 depth cache. A different motive does not make it a different
   approval.
2. **The handler type cannot be compared, so `remove_Changed` cannot name a registration.**
   `XObjectChangeEventHandler` is a bare `std::function` alias, and `std::function` has no
   `operator==` against another `std::function`. Proved at compile time — the probe carries a
   `static_assert(!HasEquality<XObjectChangeEventHandler>::value)` that would fail if this ever
   stopped holding. **The removal half is therefore not implementable at any layout cost**
   without either a public API shape change (return a registration token) or a documented
   deviation about which handler `remove_Changed` drops.

The third avenue — a process-wide side table keyed by `const XObject*` — avoids blocker 1 but
not blocker 2, and #1896's notes already record "memoising outside the object needs a side table
on every mutation" among the alternatives judged **worse**. It is not adopted here on this
review's own authority.

**Split, not deferred.** #2198 (compatible) makes the inert contract explicit and
**discriminating** across every mutation door and corrects the class doc-comment to name both
blockers — the same "state and pin the contract" move #1898 made for CCF-019's borrowed views.
#2199 (blocked) carries the implementation and the exact approval sentence (§12.3).

### 4.4 SR-AUD-333 — the CCF-019-owned high finding → **blocked, re-measured only**

Re-measured, not repaired. Probe 1's `X*` block confirms the residual set is exactly what
`XObject.hpp.audit.md`'s #1890 correction records:

- **X15** — `Extensions::Ancestors`/`AncestorsAndSelf` return `std::vector<XElement*>`; the raw
  handles outlive the tree (`ancestor-count-while-alive=2`, then dangling). Four overloads.
- **X17** — `XElement::getAttributesProperty()` returns a reference to the element's own vector,
  which outlives the element.
- **X21** — `Add` still **moves** an already-attached node (`donor-child-count=0`,
  `receiver-child-count=1`) where .NET clones. Authorised documented deviation, unchanged.

**Neither X15 nor X17 was dereferenced by this probe**, deliberately: their use-after-free is
already ASan-confirmed and recorded, and re-triggering it proves nothing new while making every
other measurement in the run unreadable. §11 states this as a non-result rather than implying
coverage.

**Why it stays blocked.** #1899 is blocked on **one** approval question with three named options
(B / D / E) and a recorded recommendation; #1894 cannot start because no CCF-019 repair has yet
outlawed a spelling for a negative fixture to reject; #1896 is blocked on the declined layout
approval. **This review chooses no ownership policy, adds no retain/detach/copy/share
behaviour, and marks nothing remediated.** §7 records what it *did* contribute: one structurally
equivalent borrowed edge inventory and the classification of each.

---

## 5. Structural root-cause families

### 5.1 X-C — a public door bypasses a validator or resolver the module already ships

**Members: SR-AUD-334 and SR-AUD-335 — both of them, which is why this unit closes two findings
with one shape of repair.**

- SR-AUD-335: `SerializeTo` bypasses `sanitizeCDataText`/`sanitizeCommentText`/
  `sanitizeProcessingInstructionText`/`XmlConvert::VerifyName`, all shipped in `modules/xml`
  and all already used by the `WriteTo` door of the *same node objects*.
- SR-AUD-334: the parse converter bypasses `XmlNode::getLocalNameProperty`/`getPrefixProperty`/
  `getNamespaceURIProperty` and `XmlAttribute::getNamespaceURIProperty`, all shipped in
  `modules/xml`, all already correct including the two subtle attribute rules.

This is the family `docs/SystemXmlNamespaceReviewPlan.md` §17 predicted in writing: *"Watch for
it in `modules/xml-linq`, whose SR-AUD-335 is the mirror image."* The prediction was right and
**under**-stated: SR-AUD-334 is a second member of the same family in the same module.

**Promotion: not minted.** X-C now has three members across two modules (SR-AUD-349 in
`modules/xml`, SR-AUD-334 and SR-AUD-335 here). The sibling review declined to mint from one
module's evidence; minting from two modules is a maintainer act, and #2109 records that every
promotion sentence in the corpus is passive and names no agent. **Recorded as evidence, not
minted.** No CCF number is claimed or reserved for it.

### 5.2 X-E — a public surface exists and is inert

Member: SR-AUD-336. The sibling namespace's SR-AUD-352 was the same shape and was **compatible**
there because `XmlDocument`'s handlers are public *data members*. Here they are *accessors that
discard*, and there is no member to write into. **Same family, opposite cost** — recorded
because it is exactly the asymmetry #2109 flags as making a family non-homogeneous.

### 5.3 CCF-019 — a borrowed edge with no liveness bound

Member: SR-AUD-333, already owned. §7 is this review's contribution to it.

---

## 6. Corrected premises

Every row is additive; no historical text is rewritten.

| # | The record said | Measured 2026-08-10 |
|---|---|---|
| 6.1 | SR-AUD-335 covers `XCData`, `XComment`, `XProcessingInstruction` | **Five doors, not three.** The PI **target** is a fifth and behaves differently from the other four — the writer door **throws** `Invalid XML name` for it while the direct serialiser emits it. |
| 6.2 | SR-AUD-335's writer paths share the defect | **They do not.** `WriteTo` was **already correct** at all five doors before this batch. The defect is confined to `SerializeTo`, i.e. `ToString()`, `ToString(SaveOptions)` and `Save(fileName)` — and to every containing element/document, which recurse into them. |
| 6.3 | SR-AUD-335 "a CDATA round trip loses data" | Confirmed, **with the mechanism**: the single CDATA node re-reads as a CDATA node (`left`) **plus a text node** (`right]]>`), so the concatenated value becomes `leftright]]>`. The PI round trip does not corrupt — it **throws**. |
| 6.4 | SR-AUD-334 is a namespace fidelity/round-trip gap | **It also emits malformed XML.** Two attributes differing only by namespace serialise to a duplicate attribute name that this module's own parser rejects (`XML_ERROR_PARSING_ATTRIBUTE`). |
| 6.5 | — | **An `xmlns:p` declaration built the .NET way degrades into an ordinary attribute `p`** on serialisation, silently unbinding the prefix for the whole subtree. |
| 6.6 | — | **`getIsNamespaceDeclarationProperty()` is wrong for parsed input** — `0` for `xmlns:p`, `1` for `xmlns`. The correct answer for the default form is reached by accident, not by resolution. |
| 6.7 | — | **The DOM layer already resolves namespaces correctly**, including the two rules easiest to get wrong (unprefixed attributes take no default namespace; the `xml` prefix is built in). SR-AUD-334's repair *reuses* that resolver; it does not invent one. |
| 6.8 | SR-AUD-334 names five files | **Six**, and the sixth is a separate class: `XStreamingElement::WriteTo` writes local names only, by the same reasoning and with the same in-code comment. |
| 6.9 | SR-AUD-336 is compatible-actionable | **It is not.** §12.3: per-object storage is the `XObject` 16 → 24 growth the user declined for #1896, and `std::function` has no equality so `remove_Changed` cannot identify a registration. Split into compatible #2198 and blocked #2199. |
| 6.10 | SR-AUD-336's test "only asserts that registration does not throw" | Confirmed verbatim (`XLinqNodeTests.cpp:87`) — **and that is the whole of the module's event coverage**: 184 tests, none of which would fail if notification were half-implemented. |
| 6.11 | — | Two post-audit defects, each the exact twin of an already-open `modules/xml` ticket: `XDocumentType::SerializeTo` (#2200 ↔ #2084) and NUL through the direct serialisers (#2201 ↔ #2085). The NUL pair is a **mirror image**: the Linq door *emits* the NUL, the writer door *truncates* at it. |

---

## 7. CCF-019 mapping — the borrowed-edge inventory this review contributes

The brief requires the blocked finding's structurally equivalent edges to be inventoried and
classified, without choosing a policy. Every borrowed edge reachable from this module's public
surface, classified by kind:

| # | Edge | Kind | Owner | Guarded? | Status |
|---|---|---|---|---|---|
| 1 | `XObject::parent_` | raw back-pointer | child object | **yes** — cleared by `~XContainer` (#1890) | closed by #1890 |
| 2 | `XAttribute::next_` | intrusive raw sibling link | attribute | **yes** — cleared by `~XElement` (#1890) | closed by #1890 |
| 3 | `XElement::getAttributesProperty()` → `const std::vector<…>&` | borrowed **reference into owner storage** | element | no | **X17, open, #1899** |
| 4 | `Extensions::Ancestors` → `std::vector<XElement*>` | borrowed **raw handles** (4 overloads) | tree | no | **X15, open, #1899** |
| 5 | `XAttribute::getPreviousAttributeProperty()` → `XAttribute*` | borrowed raw pointer | element | reaches #1 and #2, both now guarded | derived; no separate exposure |
| 6 | `XAttribute::getNextAttributeProperty()` → `XAttribute*` | borrowed raw pointer | element | guarded by #2 | closed by #1890 |
| 7 | `XObject::getParentProperty()` / `getDocumentProperty()` → raw | borrowed raw pointer | tree | guarded by #1 | closed by #1890 |
| 8 | `XNode::CompareDocumentOrder(const XNode*, const XNode*)` | borrowed **parameters** | caller | n/a — never retained | not an edge |
| 9 | `XContainer::Nodes()`, `XElement::Attributes()` | **owning snapshots**, by value | caller | n/a | measured safe, recorded so no repair breaks them |
| 10 | `XObjectChangeEventHandler` registrations | **callback capture** | would-be registry | n/a — discarded | **inert; see SR-AUD-336 / #2199** |
| 11 | `XStreamingElement::content_` `std::any` items | owning `shared_ptr` / value | streaming element | n/a | owning, not borrowed |
| 12 | `XContainer::children_` / `XElement::attributes_` | **owning** `shared_ptr` | container | n/a | owning |

**Two open borrowed edges, both already owned by #1899, both raw-handle/reference kinds, neither
reachable through `parent_`** — which is why #1890's repair could not close them and why they are
still the finding's residual. **No policy is chosen here.** Edge 10 is added to the CCF-019
inventory as a *callback-capture* edge for the first time — it is currently inert, so it carries
no live exposure, and #2199 must not be implemented without deciding whether a registered
handler may outlive its `XObject` (a question CCF-019 owns, and a second reason #2199 is
blocked).

---

## 8. SR-AUD-335 and candidate CCF-021 — the adjacency, answered

The brief warns that SR-AUD-335 is adjacent to the unminted CCF-021 (#2131) and forbids minting.
**It is not a member, and the reason is structural rather than a judgement call.**

| Test | CCF-021's five members | SR-AUD-335 |
|---|---|---|
| What crosses the door | a **control character** (CR, LF, NUL) | a **multi-character markup delimiter** (`]]>`, `--`, `?>`) |
| What it terminates | a **protocol field** in a header/frame the peer parses | a **document lexical construct** in text the same process re-reads |
| Boundary the guarantee is stated at | "no byte reaches the wire" / "no field terminator in the serialized text" | "the emitted document is well-formed" |
| Correct repair | **rejection** at the door | **self-healing** — split the CDATA section, insert a protective space |
| Does the module already ship the fix | no | **yes**, on the sibling door of the same object |

The repair policies are **opposite**: CCF-021's members reject; SR-AUD-335's correct behaviour —
already implemented in `modules/xml` and matching .NET's `XmlEncodedRawTextWriter` — preserves
the content and repairs the markup. A family whose members need opposite repairs is not one
family. Independently, `docs/SystemXmlNamespaceReviewPlan.md` §17 already assigned SR-AUD-335 to
**X-C**, not to CCF-021, and §5.1 confirms that assignment with measurement.

**Action taken: append this determination to #2131's evidence as a re-verified NON-member. CCF-021
is not minted. No finding's status is changed. No CCF number is reserved.**

---

## 9. Dependency graph

```
#2195 (review, this document)
  ├── #2196  SR-AUD-335 — direct serialisers            [compatible, independent]
  ├── #2197  SR-AUD-334 — namespaces                    [compatible, independent of #2196]
  ├── #2198  SR-AUD-336 — pin the inert contract        [compatible, independent]
  │     └── #2199  SR-AUD-336 implementation            [BLOCKED: 2 approvals]
  ├── #2200  XDocumentType quoted literals              [waits on #2084's decision]
  └── #2201  NUL through the direct serialisers         [waits on #2085's decision]

SR-AUD-333 / CCF-019 — untouched
  ├── #1899 blocked (one approval question, options B/D/E)
  ├── #1894 blocked (nothing to pin until #1888 or #1899 lands)
  └── #1896 blocked (layout approval declined 2026-07-31)
```

`#2196` and `#2197` touch disjoint functions (`XCData/XComment/XProcessingInstruction::SerializeTo`
versus `XElement`/`XAttribute`/`XDocument`/`XStreamingElement`), so either order works. #2196 is
taken first because it is the smaller of the two and its tests are the ones #2197's round-trip
assertions build on.

---

## 10. Severity

| Finding | Index | This review | Why |
|---|---|---|---|
| SR-AUD-333 | high | **high, unchanged** | Two ASan-confirmed use-after-free edges remain. |
| SR-AUD-334 | medium | **medium, at the top of the band** | Silent corruption on a public round trip, and output the module's own parser rejects. Not raised to high: no memory unsafety, no code execution, no resource fetch, and the corruption is confined to text the caller asked to be produced. |
| SR-AUD-335 | medium | **medium, unchanged** | Same reasoning; the PI round trip at least *throws* rather than corrupting silently. |
| SR-AUD-336 | medium | **medium, unchanged** | Inert surface; no unsafety. The severity is unaffected by the finding being harder to repair than the index implies. |

---

## 11. Compatible / blocked / deferred matrix

| Ticket | Compatible | Behaviour change | Approval needed | Deferred on |
|---|---|---|---|---|
| #2196 | **yes** | **yes** — four direct-serialiser doors change their output; one previously-accepted PI target is now rejected | no | — |
| #2197 | **yes** | **yes** — parsed names, query semantics and serialised text all change for namespaced input | no | — |
| #2198 | **yes** | **no** | no | — |
| #2199 | no | yes | **two** (§12.3) | — |
| #2200 | no | yes | — | #2084's delimiter/escaping decision |
| #2201 | no | yes | — | #2085's `CheckCharacters` decision |

**Neither #2196 nor #2197 is source-, ABI-, layout-, vtable- or `noexcept`-breaking.** Both are
*behaviour*-changing, which this repository handles with a migration note rather than an
approval — the same treatment `Migration-XmlStrictnessAndLifecycle.md` and
`Migration-WebSocketProtocolStrictness.md` gave to their equivalents.

---

## 12. Source / ABI / layout / vtable / `noexcept` consequences

### 12.1 #2196

- **Source:** none. No signature, no new public member, no new type.
- **Layout / vtable:** none — `.cpp` bodies plus file-local helpers only.
- **`noexcept`:** none. `SerializeTo` is not `noexcept` today and is not made so; the PI target
  door gains a `throw`, which is only reachable from an already-throwing-capable function.
- **Behaviour:** four doors emit different text; the PI target door rejects input it accepted.

### 12.2 #2197

- **Source:** **additive only** — `XElement::GetDefaultNamespace()` and
  `GetPrefixOfNamespace(const XNamespace&)`. No existing signature changes.
- **Layout / vtable:** none. The namespace scope is computed from the tree at serialisation
  time and from the DOM at parse time; **nothing is cached in any object**. This is the
  deliberate reason the repair is compatible where SR-AUD-336's is not.
- **Cost:** `XElement::SerializeTo`/`WriteTo` called on a *non-root* element walks `parent_` once
  to collect inherited declarations — O(depth) for the entry call only, not per descendant.
- **`noexcept`:** none.

### 12.3 #2199 — the two approvals, stated exactly

> **Approval XL-1 (object layout).** *"Approve adding one pointer-sized handler-storage field to
> `System::Xml::Linq::XObject`, growing `sizeof(XObject)` from 16 to 24 and growing every
> derived node type with it — `XNode` 16→24, `XContainer` 40→48, `XElement` 128→136,
> `XAttribute` 120→128, `XText`/`XCData`/`XComment` 48→56, `XProcessingInstruction` 80→88,
> `XDocument` 56→64 — in exchange for working `Changed`/`Changing` notification. The break is
> binary-only and silent; every consumer must rebuild completely."*
>
> This is the **same growth (`XObject` 16 → 24) the user declined on 2026-07-31** for #1896's
> O(1) cycle guard. A different motive does not carry the earlier refusal over, and does not
> reverse it either — it has to be asked again, for this purpose.

> **Approval XL-2 (handler identity).** *"`XObjectChangeEventHandler` is `std::function`, which
> is not equality-comparable, so `remove_Changed(handler)` cannot identify which registration to
> drop. Choose: (a) change the public shape so registration returns a token that removal
> consumes; (b) document that `remove_Changed` removes **all** handlers; or (c) document that it
> removes the **most recently added** handler."*
>
> Option (a) is a public source addition; (b) and (c) are documented deviations from .NET's
> delegate semantics. **This review does not choose.**

A third question CCF-019 owns must be answered with them: **may a registered handler outlive the
`XObject` it was registered on?** (§7 edge 10.) `#2199` must not be implemented before it is.

---

## 13. Ownership / lifetime consequences

- **#2196 and #2197 add no owning or borrowed edge.** #2196 rewrites four function bodies that
  hold no pointers. #2197's scope objects are automatic-storage `std::vector<std::pair<std::string,
  std::string>>` values that never outlive the serialisation call, and its parse path holds only
  `shared_ptr` handles the converter already held.
- **Nothing in this batch touches `XObject::parent_`, `XAttribute::next_`, `~XContainer` or
  `~XElement`** — the four sites #1890 repaired. Their tests must pass unmodified, and they do.
- **#2197 walks `parent_` upward** in `CollectInheritedScope`. That walk **only compares and
  reads**, on a live tree, from a public entry point that already requires the tree to be alive;
  it introduces no new lifetime assumption beyond the one `getDocumentProperty()` already makes
  and #1890 already made safe for detached nodes.

---

## 14. Mutation / serialisation consequences

The brief's mutation matrix applies to #2197 only through *serialisation* (neither ticket adds a
mutator). The insertion/removal/replacement matrix is already covered by
`XLinqMutationConsistencyTests.cpp` (24 tests) and `XLinqLifetimeTests.cpp` (32 tests), all of
which must keep passing unchanged — they are this review's regression floor for the mutation
half, and no ticket here is permitted to modify them.

Serialisation matrix — required for #2196 and #2197 (§16).

---

## 15. Test matrix

### 15.1 #2196 — direct serialisers

Empty value; value that is exactly the delimiter; delimiter at the start / middle / end; two
adjacent delimiters; a comment of `-`, `--`, `---`; trailing hyphen; PI with empty data; PI whose
data contains `?>` once and twice; PI target rejected; `ToString()`, `ToString(DisableFormatting)`
and `Save(file)` each producing the same text; the same node inside an element and inside a
document; **`SerializeTo` and `WriteTo` agree** for every shape; **round trip is lossless** for
CDATA and parseable for PI and comment.

### 15.2 #2197 — namespaces

Programmatic namespaced element; namespaced attribute; default namespace; prefixed input; prefix
rebinding in a nested scope; default-namespace undeclaration (`xmlns=""`); `xml:lang`; `xmlns:p`
and `xmlns` round-tripping as declarations with `IsNamespaceDeclaration` true; two namespaces
with the same local name under one parent; two attributes differing only by namespace;
namespace-aware `Element`/`Elements`/`Descendants`/`Attribute` query; `DeepEquals` across a round
trip; `GetDefaultNamespace`/`GetPrefixOfNamespace`; `XStreamingElement`; document-level round
trip; **undeclared prefix pinned unchanged**; generated prefixes do not collide with declared
ones; `SerializeTo` and `WriteTo` agree.

### 15.3 #2198 — the inert event contract

Every mutation door (`setValueProperty`, `setNameProperty`, `Add(node)`, `Add(attribute)`,
`Add(text)`, `AddFirst`, `Remove`, `ReplaceWith`, `RemoveAll`, `RemoveNodes`,
`RemoveAttributes`, attribute `setValueProperty`) with a handler registered on the object and on
an ancestor; registration/removal of an unregistered handler; and a `static_assert` pinning
blocker 2 so the contract cannot silently stop being true.

### 15.4 Parsing / serialisation matrix required by the brief

Empty document, single root, text, CDATA, comment, PI, DOCTYPE, namespaces, default namespace,
prefix rebinding, malformed names, CR/LF/NUL, Unicode, supplementary scalar, invalid XML
character, escaping, round-trip. Rows that this batch **does not** close are recorded in §17 with
the ticket that owns them (NUL/invalid characters → #2201; DOCTYPE literals → #2200).

---

## 16. Sanitizer matrix

| Sanitizer | Target | Why |
|---|---|---|
| **ASan** | the changed production bodies, over the whole `#2195` probe workload plus the new tests | #2197 walks the ancestor chain and builds scope vectors; #2196 rewrites string-building loops with index arithmetic. Buffer and lifetime coverage. |
| **UBSan** | same | index and `size_t` arithmetic in the sanitiser loops and the prefix allocator. |
| **LSan** | same | the parse path allocates every node; a rejected parse must not leak. |
| **TSan** | **not run, and the reason is stated** | Neither ticket introduces shared mutable state. The one `thread_local` in this module (`pendingRelease`, #1895) is untouched. Running TSan over single-threaded serialisation would produce a clean result that proves nothing. |

Every sanitizer conclusion must prove the production body was instrumented and the target
rebuilt, use a discriminating control, and record present-before/absent-after or state an honest
non-result.

---

## 17. Exclusions — what this review deliberately does not do

- **SR-AUD-333 / CCF-019.** No ownership policy chosen, no borrowed view changed, nothing marked
  remediated. #1899/#1894/#1896 stay blocked.
- **CCF-021 is not minted** (§8), **CCF-022 is not minted**, and **no new CCF number is minted or
  reserved** for family X-C (§5.1).
- **`SaveOptions::OmitDuplicateNamespaces`** stays inert. #2197 emits a declaration only where
  one is needed, which is *close* to the flag's effect, but the flag itself is not consulted and
  the doc-comment says so.
- **Undeclared namespace prefixes** stay accepted and unresolved (#2083 owns the question).
- **Undeclared entity references** — #2082's question; not reached from this module.
- **NUL and non-XML characters** — #2201, waiting on #2085.
- **DOCTYPE quoted literals** — #2200, waiting on #2084.
- **`XName` interning / reference equality** — permanent documented deviation.
- **Annotations, `BaseUri`, `IXmlLineInfo`, `CreateReader`** — documented out of scope; not
  invented here.
- **Entity expansion and external-resource loading are not introduced.** The substrate expands
  no internal entity and resolves no external one (measured in the sibling review, §7); nothing
  in #2196/#2197 changes the parser, only the converter above it and the writers beside it.
- **CNA, mobile-eggbert and every repository outside `sharp-runtime` were not inspected.** #1773
  stays blocked.

---

## 18. Implementation order and the next unit

### 18.1 Order

1. **#2195** — this document.
2. **#2196** — SR-AUD-335. Smallest, and its round-trip helpers are reused by #2197.
3. **#2197** — SR-AUD-334. Largest.
4. **#2198** — SR-AUD-336's compatible half.
5. Reconciliation: recount all four findings, give each exactly one disposition, and state that
   `modules/xml-linq` is **closed except for exactly the blocked and deferred work named in §9**.
   **It must not be called fully closed while CCF-019 is unresolved.**

### 18.2 The next unit

`modules/io-isolated-storage` / SR-AUD-241, scored in §1.3. Reviewed separately in this batch's
work unit 3.

---

## 19. Completion criteria

This review (#2195) is complete when:

1. this document exists and covers all nineteen required sections; ✅
2. every one of the four findings has exactly **one** disposition and none has disappeared; ✅
3. every premise correction is additive and measured, not inherited; ✅
4. the CCF-019 borrowed-edge inventory exists and chooses no policy; ✅
5. the CCF-021 adjacency is answered with evidence and the family is **not** minted; ✅
6. bounded tickets exist in `plan.sqlite3` for every disposition; ✅
7. **no `SR-AUD-*` identifier was issued** and numbering stays frozen at 364. ✅

#2196, #2197 and #2198 are complete when their own acceptance criteria in `plan.sqlite3` pass,
the full gate shows no new failure, and §20 records what implementing them corrected in this
plan.

---

## 20. Implementation record — corrections made while implementing

### 20.1 #2196 (SR-AUD-335) — landed; the round-trip premise needed one correction

Landed as §4.1 specified. Three things the plan had right and one it had wrong.

**Right, and re-confirmed by the repair:** five doors not three; the writer doors already
correct; family X-C; one shared definition rather than a copy. The transforms moved to
`modules/xml/include/System/Xml/detail/XmlLexicalSanitizer.hpp` — a `detail` namespace in a
public header, the same placement `System::Collections::detail::MutationCounter` already uses —
so `System::Xml::XmlWriter` and the three Linq serializers now cannot drift apart. `XmlWriter`'s
behaviour is unchanged character for character: 483/483 `SharpRuntimeTests_Xml` pass unmodified.

**Wrong, and corrected by measurement.** §4.1 said the processing-instruction round trip
"does not merely corrupt, it **throws**". A regression written on that premise **failed after a
correct repair**, which is what exposed it. Probe `build-probe/2196_probe4_pi.log` separates the
two causes with the emitted text held constant:

| Case | Input | Result |
|---|---|---|
| P03 | `<root><?p d?></root>` — **no special character anywhere** | **throws** `XML_ERROR_PARSING_DECLARATION` |
| P02 | `<root/><?p d?>` | throws |
| P05 | `<!--c--><?p d?><root/>` | throws |
| P01 | `<?p d?><root/>` | parses |
| **P09** | `<?p left?>right?><root/>` — the **pre-repair** text | **parses, `data == "left"`** — `right` silently **gone** |
| **P10** | `<?p left? >right?><root/>` — the **post-repair** text | parses, `data == "left? >right"` |

So the throw §4.1 attributed to SR-AUD-335 is a **separate substrate limitation**: the
tinyxml2 backend parses every `<?` as an XML *declaration*, and `tinyxml2.cpp:1126` rejects a
declaration that is not before every other node. **SR-AUD-335's own consequence for the
processing instruction is silent data loss, not a throw** — which makes it the same shape as the
CDATA case, not a different one. The limitation is now ticket **#2202** (post-audit, no
`SR-AUD-*` identifier), pinned by
`ProcessingInstruction_ParserPositionLimitIsSubstrateNotSerialization` so it cannot be lost, and
it is why #2196's PI round-trip regression asserts at document level.

**Evidence.** +28 permanent regressions (`XLinqLexicalSerializationTests.cpp`); module 184 → 212.
**Four mutations, every one discriminating** — removing the CDATA split fails 8 tests, the
comment protection 6, the PI target validation 2, the PI data protection 2; each was built,
executed and restored, none was a build failure, hang or short-circuit. ASan+UBSan+LSan over the
four changed production bodies plus a 28-value boundary workload: **exit 0, zero reports**;
non-recovering UBSan: **exit 0**. The result is discriminating because a deliberate
out-of-bounds read compiled into the *same* build configuration **did** report
(`build-probe/2196_control.log`). TSan was not run, for the reason §16 gives.

**Not repaired here, deliberately:** #2200 (`XDocumentType`'s quoted literals — the same
delimiter decision #2084 records as unsettled), #2201 (embedded NUL — the same
`CheckCharacters` decision #2085 records as unsettled), #2202 (a parser change bounded by a
vendored substrate that is never edited).

### 20.2 #2197 (SR-AUD-334) — landed; one self-contradictory tree and one non-discriminating mutation

Landed as §4.2 specified: parse resolves through the DOM's shipped resolvers, both serialization
doors carry a scope rebuilt from the tree, and `GetDefaultNamespace`/`GetPrefixOfNamespace` were
added. Every measured door in §4.2 flipped (`build-probe/2195_probe1_after.log`):

| Door | Before | After |
|---|---|---|
| `{urn:audit}root` + `{urn:audit}attribute` | `<root attribute="value"/>` | `<p1:root xmlns:p1="urn:audit" p1:attribute="value"/>` |
| `<p:root xmlns:p="urn:audit" p:a="1"/>` | local `p:root`, URI `''`, `isNsDecl=0`/`0` | `{urn:audit}root`, `{xmlns}p` `isNsDecl=1`, `{urn:audit}a` |
| `<root xmlns="urn:d"><child/></root>` | both URIs `''` | both `urn:d` |
| query by `{urn:d}child` / by `child` | MISSING / found | **found / MISSING** |
| `XAttribute(Xmlns + "p", "urn:x")` | `<e p="urn:x"/>` | `<e xmlns:p="urn:x"/>` |
| two attributes differing only by namespace | `<r x="1" x="2"/>` (unparseable) | `<r xmlns:p1="urn:a" xmlns:p2="urn:b" p1:x="1" p2:x="2"/>` |
| `xml:lang` | local `xml:lang`, URI `''` | `{http://www.w3.org/XML/1998/namespace}lang` |

**Two things the plan did not anticipate.**

1. **One tree XML cannot express.** An *unqualified* element carrying its own non-empty
   `xmlns="urn:y"` declaration asks for two contradictory things: the declaration governs the
   element's own name, but the name says it has none. The first implementation emitted **both**
   an undeclaration and the declaration — `<e xmlns="" xmlns="urn:y"/>`, two `xmlns` attributes
   on one start tag, not well-formed. Resolved by letting the explicit declaration win and
   suppressing the generated undeclaration; pinned by
   `Serialize_DefaultDeclarationAttribute_StaysADeclaration`.

2. **A programmatically built tree is deliberately not `DeepEquals` to its own round trip.** It
   carries no declaration attribute; serialization must add one; declarations *are* attributes
   and `DeepEquals` compares attributes. .NET has the same asymmetry. The property that matters
   — and that is now pinned — is that the **second** round trip is stable, and that a *parsed*
   tree is `DeepEquals` to its round trip on the first pass.

**The mutation that did not discriminate, recorded rather than hidden.** Five mutations were
run. Four failed tests immediately (parse resolution removed → 9; element prefix dropped → 10;
declarations rendered as ordinary attributes → 8; prefix-shadowing check removed → 1). The
fifth — allowing an attribute name to take the **default** namespace's prefix, the XML
Namespaces rule most often got wrong — **passed the entire 47-test suite**. A test that
discriminates it (`Serialize_AnAttributeNeverTakesTheDefaultNamespacesPrefix`) was added and the
mutation re-run against it: it now fails exactly that one test. Final count 48 tests.

**Two pre-existing tests asserted the defect** and were updated, keeping both layers of their
history in the comment: `XElementTests::ToString_NamespacedAttribute_ProducesValidXml_RoundTrips`
required a *local-name* lookup to find a namespaced attribute (it now correctly misses, and the
qualified lookup finds it), and `XAttributeTests::ToString_NamespacedAttribute_DoesNotEmitClarkNotation`
required the bare local name. The Clark-notation prohibition both were written for is unchanged
and still asserted.

**Evidence.** Module 212 → 260 tests. ASan+UBSan+LSan over the four changed production bodies
with a 60-deep rebinding chain, a 200-attribute allocator stress with `p1`–`p50` pre-taken,
undeclaration, reserved prefixes and every rejecting door: **exit 0, zero reports**;
non-recovering UBSan **exit 0**; a deliberate out-of-bounds control in the same build **did**
report (`build-probe/2197_control.log`). No source, ABI, layout, vtable or `noexcept` change;
the behaviour change is documented in `docs/Migration-XmlLinqNamespaces.md`.

### 20.3 #2198 (SR-AUD-336) — the compatible half landed; the implementation stays blocked

Landed as §4.3 specified, and nothing more: **no behaviour changed and nothing was
implemented.** `XLinqChangeNotificationTests.cpp` adds **23 permanent regressions** covering
every mutation door (`setValueProperty`, `setNameProperty`, `Add(node)`, `Add(attribute)`,
`Add(text)`, `AddFirst`, `Remove`, `ReplaceWith`, `RemoveAll`, `RemoveNodes`,
`RemoveAttributes`, an attribute's own `setValueProperty` and `Remove`, an `XText`/`XComment`
value change), a handler registered on an **ancestor** and on the owning **document**, repeated
registration, removal of a never-registered handler, and an empty `std::function`.

**The mutation that matters here is the inverse of the usual one.** The audit's complaint was
that the existing test *preserved* the inert behaviour rather than describing it, so the thing to
prove is that a **half**-implementation now fails. A deliberate one was built — handlers stored in
a process-wide list, only `XElement::setValueProperty` notifying — compiled, executed, and it
**fails `SetValue_RaisesNothing`**. The pre-existing
`XObjectTests.ChangedChangingEventAccessors_DoNotThrow` passes that same half-implementation
unchanged, which is exactly the gap the audit named. It is kept, unmodified, beside the new suite.

**Both blockers are pinned so they cannot silently stop being true.** Two `static_assert`s (the
handler is still a bare `std::function`; it is still not equality-comparable) and a runtime
layout pin (`sizeof(XObject) == 2 * sizeof(void*)`, no padding) fail the build or the suite if
approval XL-1's or XL-2's premise goes stale.

**Status change.** SR-AUD-336 stays **open** and counted as `confirmed`, and now carries the
`design-complete` qualifier — the selected repair and its blocked implementation ticket (#2199)
are recorded. The decomposition moves from 172/138/54 to **172 remediated / 137 confirmed /
55 confirmed (design-complete) = 364**.

---

## 21. Completion reconciliation — `modules/xml-linq`'s compatible queue is exhausted

### 21.1 All four findings, one disposition each, none lost

| Finding | Severity | Status at review start | Status now | Disposition |
|---|---|---|---|---|
| **SR-AUD-333** | high | `confirmed (design-complete)` | **`confirmed (design-complete)` — unchanged** | **Blocked, CCF-019.** Re-measured only (§4.4, §7). X15/X17 still ASan-confirmed use-after-free, X21 still the authorised deviation. No ownership policy chosen; nothing implemented; **not** marked remediated. #1899, #1894, #1896 all stay blocked; #1892/#1893 stay `wontfix`. |
| **SR-AUD-334** | medium | `confirmed` | **`remediated`** | Compatible implementation, #2197. |
| **SR-AUD-335** | medium | `confirmed` | **`remediated`** | Compatible implementation, #2196. |
| **SR-AUD-336** | medium | `confirmed` | **`confirmed (design-complete)`** | Split: compatible pin landed (#2198); implementation **blocked** on two approvals (#2199). |

**Audit decomposition: 170/140/54 at review start → 172 remediated / 137 confirmed /
55 confirmed (design-complete) = 364.** Two findings moved `confirmed → remediated`; one moved
`confirmed → confirmed (design-complete)`, which is a qualifier, not a status change — it is
still counted as `confirmed` and still open. **No `SR-AUD-*` identifier was issued; numbering
stays frozen at 364.**

### 21.2 Is the compatible queue exhausted? Yes — and here is everything left

| Remainder | Kind | Blocked on |
|---|---|---|
| #1899 | SR-AUD-333 borrowed views | one approval question (options B/D/E), recommendation recorded |
| #1894 | SR-AUD-333 negative fixtures | nothing to pin until #1888 or #1899 lands |
| #1896 | CCF-019 quadratic attach | layout approval **declined** 2026-07-31 |
| #2199 | SR-AUD-336 implementation | approvals **XL-1** (layout) and **XL-2** (handler identity), plus one CCF-019 lifetime question |
| #2200 | `XDocumentType` quoted literals | #2084's delimiter/escaping decision |
| #2201 | NUL through the direct serializers | #2085's `CheckCharacters` decision |
| #2202 | parser accepts a PI only before everything else | a parser change bounded by a vendored substrate that is never edited |

**Nothing in that list is compatible-and-ready.** Every entry is waiting on a decision, an
approval, or another module's ticket. There is no further compatible work in `modules/xml-linq`.

### 21.3 What the pins hold, and that they discriminate

Module coverage **184 → 283 tests** (+99: 28 from #2196, 48 from #2197, 23 from #2198). **Ten
mutations were built, executed and restored.** Nine discriminated on the first attempt; the
tenth — allowing an attribute name to take the default namespace's prefix — did **not**, so a
test for it was added and the mutation re-run against it. That is recorded in §20.2 rather than
quietly fixed. The #2198 pins were checked with the **inverse** mutation: a deliberate
half-implementation of change notification, which the new suite fails and the pre-existing test
passes unchanged.

### 21.4 The status of `modules/xml-linq`

**Closed for compatible work; not fully closed.** Two of four findings are remediated, one is
pinned and design-complete, and one is the CCF-019 high. **It must not be called fully closed
while CCF-019 is unresolved**, and this review does not.

### 21.5 No family was minted

CCF-021 is **not** minted, and §8 records SR-AUD-335 as a re-verified **non-member** with the
reason. CCF-022 is **not** minted. Family **X-C** now has three members across two modules and is
**not** minted either — a namespace review does not mint on its own authority (#2109). #2131's
notes carry this review's evidence; its status is unchanged.


---

## 22. #2200 — the twin was a *delegator and a duplicate*, and the finding named three fields of four

`XDocumentType` has two serialisation doors. §21.2 recorded #2200 as blocked on "#2084's
delimiter/escaping decision"; that blocker is **discharged** — #2084 settled the decision and
put it in the one place both components can reach — and the ticket landed as a one-path reuse.

### 22.1 The two doors were never twins

| Door | Reached by | Before #2200 |
|---|---|---|
| `WriteTo(XmlWriter&)` | `Save(XmlWriter&)` | **delegates** to `XmlWriter::WriteDocType` — inherited #2084's repair with **no edit in this component** (the suite stayed 283/283 green after #2084 landed) |
| `SerializeTo(ostream&)` | `ToString()`, `ToString(SaveOptions)`, `Save(fileName)`, and every containing `XDocument`/`XElement` serialisation | **duplicated** the old three-literal concatenation and did not |

So #2200 was never "apply #2084 twice". It was "delete the one remaining copy of the pre-#2084
concatenation", which is why the repair adds no new definition: `detail::SelectExternalIdDelimiter`
and `detail::ExternalIdLiteralTerminatesDeclaration` already live in
`System/Xml/detail/XmlLexicalSanitizer.hpp` — the one-definition home #2196 created for exactly
this `XmlWriter`/`Xml::Linq` duplication — and `modules/xml-linq` already includes that header in
three other files. No component edge changed (`Xml.Linq` already declares `Xml` public).

### 22.2 Measured before/after (`build-probe/2200_probe1_before.log`)

Eighteen declarations were serialised through **both** doors. They disagreed on **eleven**:

| Input | direct door, before | writer door (already correct) |
|---|---|---|
| systemId `sys"tem` | `… "sys"tem">` — reader recovers `sys` | re-delimited `'sys"tem'` |
| systemId `sys"te'm` | emitted anyway | **throws**, unrepresentable |
| systemId `sys>tem` | `… SYSTEM "sys>tem">` — declaration ends early | **throws** |
| publicId `pub"lic` | `… PUBLIC "pub"lic" …` | **throws** (not a `PubidChar`) |
| publicId `pub>lic` | emitted | **throws** |
| name `ro ot` | `<!DOCTYPE ro ot>` | **throws** (not an XML name) |
| NUL in name / publicId / systemId | emitted raw | **throws** |
| the ticket's own case | `<!DOCTYPE root PUBLIC "pub"lic" "sys"tem" []>]>` | **throws** |

The seven that already agreed — including `sys'tem`, `pub'lic` and both internal-subset rows —
are **byte-identical** after the repair. `"` stays the preferred delimiter precisely so that
holds.

### 22.3 Premise correction: **four** fields, not the three the ticket names

#2200's title says "three unvalidated quoted literals". The DOCTYPE **root-element name** is a
fourth unvalidated field at the same door, and it fails the same way: the direct door emitted
`<!DOCTYPE ro ot>` while `WriteTo` has rejected that name since #2076. It is repaired here, in
the same statement sequence, for the reason #2196 gives for the processing-instruction *target*:
validation belongs at the **serialisation** doors, not at construction, because narrowing
construction is a wider accepted-input change than the finding calls for. `#2084` made the same
kind of correction in the other direction (it found a *second producer*, `XmlDocument::CreateDocumentType`,
that the finding never named); this is that pattern applied to the field list.

`ConstructionAndSettersStillDoNotValidate` pins the boundary that was **not** moved.

### 22.4 Not repaired here: the internal subset

Deliberately untouched and **not** absorbed. Its `>` problem is this runtime's `>`-terminated
DOCTYPE representation, not a delimiter choice — an ordinary `<!ENTITY a "b">` contains a `>`
that XML *requires*, so no re-delimiting exists — and it was already lossy before this ticket.
`InternalSubset_IsEmittedExactlyAsBefore_StillOutsideThisTicket` pins that the emitted bytes are
unchanged and still match the writer door character for character, so a later reader cannot
mistake the pin for coverage. That half is **#2348**, and closing it needs a real DTD
internal-subset scanner this port deliberately does not have.

The embedded NUL in the **internal subset** is likewise not #2200's: `VerifyName`,
`VerifyPublicId` and `VerifyXmlChars` reject a NUL in the other three fields as a side effect of
the ExternalID repair (exactly as they do at the writer door), and the fourth field is **#2201**.

### 22.5 Compatibility

Implementation-only. No public signature, layout, vtable, `noexcept` specification, exported
symbol or component dependency changed — the diff is one `.cpp` body plus four `#include`s and a
header doc-comment. It **is** a behaviour narrowing for input that previously produced
unparseable or silently-truncated output, which is the point of the ticket, and it narrows to
**exactly** what the sibling door already rejected; `BothDoorsAcceptOrRejectEveryProbedDeclaration`
is the property that keeps them from drifting apart again.

### 22.6 Pins and mutations

**18 permanent tests** (`XLinqDocTypeSerializationTests`, module 283 → 301). **Six mutations
were built, executed and restored; all six were caught**, each by a test that names its own
concern:

| Mutation | Killed by |
|---|---|
| M1 restore the pre-#2200 body verbatim | 11 tests |
| M2 force `"` regardless of content | 6 |
| M3 drop `VerifyPublicId` | 3 |
| M4 attribute-style `&quot;` escaping instead of re-delimiting | 8 |
| M5 drop the `>`-terminates-the-declaration check | 4 |
| M6 drop `VerifyName` | 2 |

M4 is the mutation that matters most: it *looks* like a repair and passes a naive
"no bare quote in the output" assertion, but `ExpectDirectDocTypeRoundTrip` catches it, because
this runtime's DOCTYPE reader never un-escapes a literal and hands back the six characters
`&quot;`.

Sanitizers were **not** run, for the reason #2085 gives: the defect is wrong serialised text
produced by ordinary `std::string` concatenation, with no allocation, indexing or lifetime
mechanism for ASan/UBSan to observe. The probe was rebuilt and re-run instead.

---

## 23. #2201 — the mirror image of #2085, and **nine** doors where the ticket named two

`XText`/`XCData` are what #2201's description names. Measured across every public Xml.Linq
serialisation door (`build-probe/2201_probe1_doors.log`), **nine** emitted an embedded NUL.

### 23.1 Why this is a mirror image, not a copy

| | writer door (`WriteTo` → `XmlWriter`) | direct door (`SerializeTo`/`ToString`) |
|---|---|---|
| boundary | `std::string::c_str()` into tinyxml2's `const char*` API | none — bytes go straight to the stream |
| before | **truncated** at the NUL, silently | **emitted** the NUL |
| repaired by | #2085 | #2201 |

Lost one way, unreadable the other. Measured, this runtime's own reader rejects all four shapes:
`XML_ERROR_PARSING_TEXT`, `_ATTRIBUTE`, `_CDATA`, `_COMMENT`. One policy now answers both doors,
and it is #2085's: **NUL is rejected unconditionally.** That is not a `CheckCharacters`
preference — the XML `Char` production excludes U+0000 and a character reference must itself
match `Char`, so "emit it in full" is not an implementable branch. A flag can only govern a
choice whose branches both exist.

### 23.2 The nine doors, and the three that already rejected

| Door | Before | Now |
|---|---|---|
| `XText::SerializeTo` (text) | emitted | rejected |
| `XCData::SerializeTo` (value) — overrides `XText`'s, so it needs its own guard | emitted | rejected |
| `XComment::SerializeTo` (value) | emitted | rejected |
| `XProcessingInstruction::SerializeTo` (**data**) | emitted | rejected |
| `XDocumentType::SerializeTo` (**internal subset**) | emitted | rejected |
| `XAttribute::ToString` (name, namespace name, value) | emitted | rejected |
| `XElement::SerializeElementTo` (element name) | emitted | rejected |
| `XElement::SerializeElementTo` (attribute name, attribute value) | emitted | rejected |
| `XDeclaration::ToString` (version, encoding, standalone) | emitted | rejected |
| `XProcessingInstruction` **target** | already rejected — `XmlConvert::VerifyName` (#2196) | unchanged, pinned |
| `XDocumentType` name / publicId / systemId | already rejected — #2200's three validators | unchanged, pinned |
| `XStreamingElement` | already rejected — routes through `XmlWriter` | unchanged, pinned |

`XCData` is the trap worth naming: it derives from `XText` but **overrides** `SerializeTo`, so a
guard placed only in the base class never runs for a CDATA node.

### 23.3 One detector, one wrapper, and where validation lives

`System::Xml::detail::ContainsNul` — the single detector #2085 put in the shared header — is
reused unchanged; no second scanner was written. The only new code is the throwing wrapper
`System::Xml::Linq::detail::ThrowIfContainsNul`, in the new header
`System/Xml/Linq/detail/XLinqSerializationGuards.hpp`, so the eight source files that need it
share one definition and one diagnostic shape rather than eight file-local statics. It
deliberately mirrors `XmlWriter.cpp`'s file-local `ThrowIfContainsNul`, and `modules/xml`'s copy
was **not** touched — #2085's message pins stay valid.

**Validation happens at serialisation, not at construction**, which answers the review question
directly: the Xml.Linq object model deliberately permits states that only fail when written. That
is the boundary #2196 set for the processing-instruction target and #2200 kept for the DOCTYPE
fields, and `ConstructionAndMutationStillAcceptANul` pins it — an `XText` still *holds* a
three-byte value containing a NUL, and `getValueProperty().size()` still returns 3.

`XDeclaration::ToString`'s body moved from the header to `XDocument.cpp` so a public header does
not have to reach the guard. Same signature, same output, same `[[nodiscard]]`.

### 23.4 What #2201 does NOT close

- **The non-`Char` policy (#2349).** 0x01, 0x0C and 0x1F are still emitted, byte for byte.
  `NonNulControlCharacters_StillEmittedByTheDirectDoor` pins that — the mirror of the pin #2085
  left at the writer door — and mutation **N5**, which widens the guard to the whole `Char`
  production, is killed by exactly that test. So #2349's decision cannot be made here by
  accident.
- **The XML name grammar at the direct door.** `XElement`/`XAttribute` names are checked for a
  NUL and nothing more; the direct door still accepts names the writer door rejects. That is a
  separate, much wider accepted-input question and is **#2350**, filed rather than absorbed.
- **The internal subset's `>` (#2348)** and everything else #2200 left alone.

### 23.5 Compatibility, pins and mutations

Implementation-only: no public signature, layout, vtable, `noexcept` specification, exported
symbol or component dependency changed (`Xml.Linq` already declares `Xml` public, and the new
header is inside `Xml.Linq`'s own include root). Every value without a NUL keeps its bytes —
tab/CR/LF, multi-byte UTF-8, `]]>`, `--`, `?>` and the escapes are all pinned identical.

**18 permanent tests** (`XLinqNulRejectionTests`, module 301 → 319). **Eight mutations were
built, executed and restored; all eight were caught.** N2 is the sharpest of the removals — a
detector that only sees an *interior* NUL — which is why every guard is tested at four positions
(leading, interior, trailing, and a value that is nothing but a NUL) rather than one.

Sanitizers were **not** run, and for a reason rather than by omission: a NUL crossing a
`std::ostream <<` is a *content* bug, not a memory bug. Nothing is read out of bounds, nothing is
freed early, no `const char*` boundary is involved on this path at all — ASan and UBSan have
nothing to observe here, exactly as #2085 recorded for the truncation half.
