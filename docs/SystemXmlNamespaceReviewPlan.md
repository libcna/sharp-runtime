<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Xml` namespace review — ticket #2073

Owning ticket **#2073**. This document is the durable record; it **remediates nothing by
itself**. Every claim below was measured against the tree at `071fc30` with
`build-probe/2073_probe1_xml_defects.cpp` (log `build-probe/2073_probe1_before.log`).

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-04, `/rv` does not exist.
Every statement about .NET below therefore comes from repository-contained evidence only: the
per-file audit reports, doc-comments transcribed from .NET when the module was written, and
this module's own tests and sibling implementations. Where a repair would need .NET's exact
grammar and no repository evidence pins it, a **deferred-verification ticket** is created
instead of a guess.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit
defects found by this review carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket #1773 stays blocked.

---

## 1. Why this unit was selected

Re-derived by measurement over `audit/AUDIT_FINDINGS_INDEX.md` at `071fc30`, **not** inherited
from the `System::Net::Http` review's recommendation. Every coherent unit with at least six
open findings:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `modules/core` | 72 | 9 | 59 | 4 | 12% | 1 | 47 | family plans only |
| `modules/threading` | 17 | 6 | 11 | 0 | 35% | 0 | 21 | **yes** |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7% | 12 | 8 | **yes** |
| `modules/io` | 11 | 0 | 11 | 0 | 0% | 0 | 2 | none |
| `modules/text` | 11 | 1 | 10 | 0 | 9% | 11 | 3 | **yes** |
| `modules/uri` | 10 | 0 | 10 | 0 | 0% | 10 | 4 | **yes** |
| **`modules/xml`** | **8** | **2** | **6** | **0** | **25%** | **0** | **0** | **none** |
| `modules/time-zone` | 7 | 0 | 7 | 0 | 0% | 0 | 0 | none |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none |
| `modules/text-json` | 7 | 1 | 6 | 0 | 14% | 1 | 0 | none |
| `modules/net-websockets` | 6 | 2 | 4 | 0 | 33% | 0 | 0 | none |
| `modules/net-http` | 6 | 1 | 5 | 0 | 17% | 2 | 3 | **yes** (closed for compatible work) |

`modules/net-http-headers` has **5** open findings — below the ≥6 threshold — with **two**
highs, both CR/LF injection. It is not a candidate under the stated rule but is load-bearing
for §17's promotion recommendation.

### Applying the stated selection priorities, in order

1. **High-severity memory or lifetime risk.** On the letter, **`net-websockets` wins this
   priority**: SR-AUD-247 is an ASan-confirmed use-after-free. It is nonetheless **not**
   selected, for a reason that is now measured rather than predicted — that finding is
   **CCF-019 verbatim**, the same shape as SR-AUD-310, and #2066's disposition is now known:
   **blocked, unapproved, two competing options and no selection** (`docs/SystemNetHttpNamespaceReviewPlan.md`
   §20.7). Reviewing `net-websockets` today produces a review whose highest-value finding is
   blocked on arrival. `xml`'s two highs are **both actionable**: SR-AUD-350 is **silent
   document content loss** on public input (a destructive partial-state publication) and
   SR-AUD-351 is an **ownership defect that mutates an unrelated subtree** — and this review
   measured that one of its four affected mutators **drops a node entirely**, which is a
   lifetime question, not only a correctness one (§4.2).
   `globalization`'s SR-AUD-280 is a TSan-confirmed race on a mutable process-global, real but
   almost certainly approval-sensitive (a thread-static `CurrentCulture` changes a public
   static property's semantics). `text-json`'s high, SR-AUD-327, is **already**
   `confirmed (design-complete)` and blocked as #1888/#1889/#1894.
2. **Public-input attackability.** `xml` parses documents from files, strings and URIs; XML is
   a classic hostile-input surface (entity expansion, malformed markup, depth). This review
   measured that surface directly (§7) and found **two genuine positives worth recording** —
   internal entities are never expanded, and nesting depth is bounded — plus **two
   post-audit acceptance defects**. `text-json` is comparable; `time-zone`, `io` and
   `globalization` are weaker.
3. **Useful compatible queue.** `xml` yields **six** compatible tickets and two deferred
   (§13). `net-websockets` yields roughly three of six, with its highest-value one blocked.
   `globalization`'s queue is ICU-scale work behind a blocked concurrency change.
   `io` has eleven findings but **zero** highs and is dominated by one dispose/close-lifecycle
   family that would be better worked as a family than as a namespace.
4. **Coherent module boundary.** `modules/xml` is one CMake component (`Xml`, with `Xml.XPath`
   an alias of the same archive): 78 headers, 30 bodies, ~8,000 lines, plus one vendored
   dependency (`tinyxml2`). `modules/xml-linq` is a **separate** component with its own four
   findings and is excluded, exactly as `Net.Http.Headers` was excluded from the `Net.Http`
   review.
5. **No existing complete review.** `xml` has none.

**Selected: `modules/xml`.** Priority 1 favours `net-websockets` on the letter and `xml` on
the actionable evidence; priorities 2, 3 and 4 favour `xml`. **`modules/net-websockets` is the
recommended next unit**, and §17 records why reviewing it is now *more* valuable than it was:
it is the third module of the CR/LF-injection family and would let **CCF-021** be minted with
cross-module evidence.

---

## 2. Scope and file inventory

Component `Xml` (`modules/xml/CMakeLists.txt`): `TYPE STATIC`,
`PUBLIC_DEPENDENCIES Core.Base Uri`, `PRIVATE_DEPENDENCIES Diagnostics`,
`TEST_DEPENDENCIES Xml.Linq`, plus the vendored `tinyxml2` archive. `Xml.XPath` is a
**component alias of the same physical target** — the two have mutual binary dependencies —
so an XPath change is an `Xml` change.

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 78 | 3,820 |
| implementation | 30 | 4,254 |
| tests | 8 files | — |

**In scope:** everything under `modules/xml/`, including `System/Xml/XPath/` and
`System/Xml/Schema/`.

**Out of scope, and why:**

- `modules/xml-linq` — a separate component with its own four findings (SR-AUD-333 high and
  design-complete under CCF-019, plus 334/335/336). §17 records how its SR-AUD-335 relates to
  this review's SR-AUD-349.
- `vendor/tinyxml2` — third-party source, exempt from this project's rules by `CLAUDE.md`.
  This review treats it as the **parser substrate** whose behaviour is measured, never edited.
- XSD schema **validation** (`XmlValidatingReader`, `Schema/`) — the port carries the exception
  types only; validation itself is absent by design and is not a finding.

---

## 3. Complete public-surface inventory

| Area | Types | Notes |
|---|---|---|
| DOM core | `XmlNode`, `XmlLinkedNode`, `XmlDocument`, `XmlElement`, `XmlAttribute`, `XmlText`, `XmlCDataSection`, `XmlComment`, `XmlProcessingInstruction`, `XmlDeclaration`, `XmlDocumentFragment`, `XmlDocumentType`, `XmlEntity`, `XmlEntityReference`, `XmlNotation`, `XmlSignificantWhitespace`, `XmlWhitespace`, `XmlCharacterData` | wrappers over a `tinyxml2::XMLDocument` owned by `XmlDocument`; `XmlNode*` is a **borrowed raw pointer into a document-owned cache** |
| DOM collections | `XmlNodeList`, `XmlNamedNodeMap`, `XmlAttributeCollection` | `getChildNodesProperty()` returns a pointer to a snapshot cached **on the node** |
| Reader | `XmlReader`, `XmlTextReader`, `XmlValidatingReader`, `XmlReaderSettings`, `XmlParserContext`, `ReadState`, `ConformanceLevel`, `DtdProcessing`, `EntityHandling`, `ValidationType`, `WhitespaceHandling`, `IXmlLineInfo` | `XmlReader::Create`/`CreateFromString` **flatten the whole document into an event vector** up front; there is no streaming |
| Writer | `XmlWriter`, `XmlTextWriter`, `XmlWriterSettings`, `WriteState`, `Formatting`, `NamespaceHandling`, `NewLineHandling`, `XmlOutputMethod` | builds a `tinyxml2` tree, `ToString()`/file on `Close()` |
| Names and namespaces | `XmlQualifiedName`, `XmlNameTable`, `NameTable`, `XmlNamespaceManager`, `XmlNamespaceScope`, `IXmlNamespaceResolver` | |
| Conversion | `XmlConvert`, `XmlDateTimeSerializationMode`, `XmlTokenizedType`, `XmlSpace` | `VerifyName`/`VerifyNCName`/`VerifyNMTOKEN` **already exist and throw `XmlException`** |
| Resolvers | `XmlResolver`, `XmlUrlResolver`, `XmlSecureResolver` | external-resource policy |
| XPath | `XPathNavigator`, `XPathNodeIterator`, `XPathExpression`, `XPathDocument`, `XPathItem`, `XmlDocumentNavigator`, `IXPathNavigable`, result/type/order enums | |
| Events | `XmlNodeChangedEventArgs`, `XmlNodeChangedAction`, and six **public data members** on `XmlDocument` (`NodeInserting`/`NodeInserted`/`NodeRemoving`/`NodeRemoved`/`NodeChanging`/`NodeChanged`) | the handlers are `std::function` fields, not accessors |
| Exceptions | `XmlException`, `XPathException`, `Schema::XmlSchemaException`, `Schema::XmlSchemaValidationException` | |

---

## 4. Every open finding, with its measured disposition

| Finding | Severity | Reproduced? | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-348 | medium | yes, and **wider than filed** | compatible | **#2078** |
| SR-AUD-349 | medium | yes, and **much wider than filed** | compatible | **#2076** |
| SR-AUD-350 | high | yes | compatible | **#2074** |
| SR-AUD-351 | high | yes, and **four mutators, not one** | compatible | **#2075** |
| SR-AUD-352 | medium | yes | compatible | **#2079** |
| SR-AUD-353 | medium | yes, and **wider than filed** | compatible | **#2077** |
| SR-AUD-354 | medium | yes | **deferred verification** | #2080 |
| SR-AUD-355 | medium | yes | compatible, larger | **#2081** |

### 4.1 SR-AUD-350 — invalid `InnerXml` destroys the existing children (high) → **#2074, compatible**

`XmlNode::setInnerXmlProperty` calls `RemoveAllChildren()` **first**, then
`fragmentDoc.Parse(wrapped.c_str())` whose **return status is ignored**, then
`if (!root) return;`. Measured:

```
before:        innerXml='<keep/>'
setInnerXml("<bad>")
after invalid: innerXml=''  hasChildren=0     <- silent, total content loss
setInnerXml("<a/><b/>") -> innerXml='<a/><b/>'  (valid input unaffected)
```

**Corrected premise — the module already does the right thing one call away.**
`XmlDocument::LoadXml` parses the *same* text through the *same* substrate and **throws**:

```
LoadXml("<bad>") -> XmlException:
  XmlDocument::LoadXml: parse error: Error=XML_ERROR_MISMATCHED_ELEMENT ErrorID=14 Line number=1
```

So the repair is not a new policy and needs no new evidence: parse **first**, throw the same
shaped `XmlException` on failure, and only then replace the children. That makes the operation
atomic, which is what .NET's `InnerXml` setter is.

**Evaluated and excluded:** `setInnerTextProperty` also clears first, but there is no parse to
fail — it cannot lose content on invalid input, and it is **not** a member of this defect.

### 4.2 SR-AUD-351 — DOM mutators accept a node owned by another parent (high) → **#2075, compatible**

The finding names `RemoveChild`. **Measured, four public mutators are affected, and they fail
in three different ways.** Setup: `root` has children `a` and `b`; `b` has child `childOfB`.

| Call | Measured result | Correct behaviour |
|---|---|---|
| `a->RemoveChild(childOfB)` | `<a/><b/>` — **B's child was detached** | `ArgumentException` |
| `a->InsertBefore(n, childOfB)` | `<a><n/></a><b><childOfB/></b>` — `refChild` **silently ignored**, `n` landed inside `a` | `ArgumentException` |
| `a->InsertAfter(n, childOfB)` | `<a/><b><childOfB/></b>` — **`n` is nowhere**; it was neither inserted nor destroyed | `ArgumentException` |
| `a->ReplaceChild(n, childOfB)` | `<a><n/></a><b/>` — **both** wrongs at once | `ArgumentException` |
| `root->RemoveChild(orphan)` (no parent) | accepted silently | `ArgumentException` |

`RemoveChild` calls `doc->DetachNode(oldChild->native_)` after checking only that the pointers
are non-null; `InsertBefore`/`InsertAfter` pass a foreign `refChild` straight to
`tinyxml2::XMLNode::InsertAfterChild`, which refuses when the reference node's parent is not
the receiver, and the port ignores the refusal.

**The `InsertAfter` row is the one with a lifetime consequence**, and it is why this finding is
worth a `high`: the created node `n` is owned by the document's node pool but is reachable from
no tree, and the wrapper `XmlNode*` the caller holds still points at it. Whether that is a leak,
a dangling wrapper after a later `RemoveAllChildren`, or merely an orphan is **an ASan question,
not a reading question** — §12 requires it to be answered by measurement before #2075 closes.

### 4.3 SR-AUD-349 — the writer emits documents its own reader rejects (medium) → **#2076, compatible**

Measured; every row accepted with no diagnostic:

| Call | Emitted |
|---|---|
| `WriteStartElement("1bad")` | `<1bad/>` — **the module's own reader then throws `XML_ERROR_PARSING`** |
| `WriteStartElement("a b")` | `<a b/>` — reparses as an element with an attribute |
| `WriteStartElement("")` | `</>` |
| `WriteStartElement("a<b")` | `<a<b/>` |
| `WriteAttributeString("1bad", "v")` | `<e 1bad="v"/>` |
| `WriteEndElement()` with nothing open | accepted, result `''` |
| any write **after `Close()`** | accepted, silently discarded (`'<e/>'` unchanged) |

**Corrected premise — the validator already exists, and one sibling already uses it.**
`XmlConvert::VerifyName` throws `XmlException("Invalid XML name: '…'.")`, and
`XmlDocument::CreateElement("1bad")` **already throws it**. Measured side by side in the same
program: the DOM rejects the name and the writer emits it. The repair is to route the writer's
names through the validator the module already ships, not to invent a name grammar — which is
what keeps it compatible and evidence-backed.

Writer **state** validation (unbalanced `WriteEndElement`, writes after `Close`) is the
finding's second clause and belongs to the same ticket; `WriteState` is already a public enum
the writer never exposes a value from.

### 4.4 SR-AUD-348 — `XmlReader::Close` is unobservable (medium) → **#2078, compatible**

`Close()` assigns `readState = ReadState::Closed` and nothing else. `Read()` has no guard and
**assigns `ReadState::Interactive` on its way out**, so the closed state does not even survive
the next call. Measured after `Read(); Close(); Read();`:

```
Read()=1  readState=1 (Interactive)  Name='x'
```

So a closed public reader keeps traversing input **and** reports itself as interactive. Note
the port's reader is not streaming — `Create` flattens the document into an event vector — so
"consuming input" here means advancing a cursor, not reading a stream. That does not make the
lifecycle boundary less public.

**The repair is a choice this port must record, not a match it can claim.** .NET's
`XmlReader.Read()` after `Close()` returns `false` with `ReadState.Closed`; that is the
behaviour selected, and it is chosen because it is **self-consistent with this port's own
`getReadStateProperty()` contract**, not because the reference was consulted (it is absent).

### 4.5 SR-AUD-353 — `HasNamespace` searches one scope (medium) → **#2077, compatible**

One line: `return scopes_.back().find(prefix) != scopes_.back().end();`, while its sibling
`LookupNamespace` walks `scopes_` in reverse. Measured after declaring `p` and pushing a scope:

```
HasNamespace("p")   = 0
LookupNamespace("p") = urn:outer     <- the same manager answers both
HasNamespace("xml") = 0              <- WIDER THAN FILED
```

**Corrected premise:** the finding names user-declared outer prefixes. Measured, the **built-in
`xml` prefix** — a permanent declaration this manager itself installs in scope 0 and documents
as always present — is also invisible to `HasNamespace` from any inner scope.

`GetNamespacesInScope(XmlNamespaceScope::Local)` is **not** a member of this defect: it is
documented and implemented as local-only, and `Local` is what it means.

### 4.6 SR-AUD-352 — the node-change events are inert (medium) → **#2079, compatible**

Six public `std::function` data members on `XmlDocument`; measured, an inserted element, a
removed element and a text change dispatch **zero** of them
(`inserted=0 removed=0 changing=0 changed=0`). The header's own comment says *"not yet wired to
call sites"*, so the **documentation is honest** — but the surface is public, `.NET` raises
these, and a consumer cannot tell the difference between "no mutation happened" and "no event
was ever going to arrive".

Compatible: the handlers already exist as public members, so wiring them changes no type, no
signature and no layout. It **does** begin invoking a callback that was previously never
invoked, which is a real behaviour change and needs the migration note §11 records.

### 4.7 SR-AUD-355 — XPath does not collapse adjacent text-like nodes (medium) → **#2081, compatible, larger**

Measured on `<r>left<![CDATA[right]]></r>`:

```
MoveToFirstChild=1 value='left'
MoveToNext=1       value='right'
```

The XPath data model has **one** logical text node per adjacent text-like run, whose string
value is the concatenation. `XmlDocumentNavigator` returns the native node directly, so
position, value, `MoveToNext` and any count-based expression diverge for mixed CDATA/text
content.

Compatible in the signature sense and larger in the implementation sense: collapsing must be
consistent across `MoveToFirstChild`, `MoveToNext`, `MoveToPrevious`, `MoveToParent`,
`getValueProperty` and the iterator, or the navigator becomes internally inconsistent — which
is worse than the defect. Sequenced **last** for that reason.

### 4.8 SR-AUD-354 — `XmlConvert` `TimeSpan` is not an XSD duration (medium) → **#2080, deferred verification**

Measured:

```
ToTimeSpan("P1D")      -> throws "String was not recognized as a valid TimeSpan: P1D"
ToTimeSpan("PT1H30M")  -> throws
ToString(1 day)        -> '1.00:00:00.0000000'   (should be 'P1D')
ToTimeSpan("1.00:00:00") -> accepted            (the native form)
```

The finding is correct. **The repair is not decidable here.** `xs:duration` carries **years and
months**, which have no fixed length in ticks; .NET's `XmlConvert.ToTimeSpan` resolves that with
a specific documented approximation, and choosing a different one silently produces wrong
durations rather than a visible error. Three sub-questions have no repository-contained answer:
the year/month conversion factors, whether the native `1.00:00:00` form stays accepted
(a widening question), and the exact exception identity. **Deferred, #2080**, with all four
measured behaviours pinned so no answer can land silently.

---

## 5. Structural root-cause families

### 5.1 X-A — a destructive step runs before the step that can fail

Members: SR-AUD-350. Root cause: `RemoveAllChildren()` precedes the parse, and the parse's
status is discarded. This is the **partial-state-publication** shape: on the failure path the
object is left in a state neither the old nor the new one describes. Module-local, one site,
**#2074**. Not minted as a CCF: one site, and the sibling `LoadXml` already demonstrates the
correct ordering inside the same file.

### 5.2 X-B — a mutator trusts a caller-supplied node without checking ownership

Members: SR-AUD-351 (four mutators). Root cause: the port validates *document* identity
(`ThrowIfDifferentDocument`) and *ancestry* (`ThrowIfSelfOrAncestor`) but never
**parenthood**, so a same-document node belonging to a different parent passes every check.
The three existing guards prove the shape is understood; one case was missed. **#2075.**

### 5.3 X-C — a public door bypasses a validator the module already ships

Members: SR-AUD-349. Root cause: `XmlConvert::VerifyName` exists and `XmlDocument::CreateElement`
uses it; `XmlWriter` does not. This is the same shape as `System::Net::Http`'s NH-B repair
(one shared helper, every door routed through it) and is why #2076 is bounded. **Not** minted
as a CCF — it needs a second module's evidence, and §17 records where to look.

### 5.4 X-D — a public lifecycle state is recorded but never enforced

Members: SR-AUD-348 (`XmlReader::Close`), SR-AUD-349's second clause (`XmlWriter` after
`Close`). Root cause: the state is a field, not a precondition. **This is the shape
`modules/io` is saturated with** — SR-AUD-337, 343, 344 are the same sentence about
`StreamReader`/`StringWriter`/`UnmanagedMemoryStream`. §17 records the promotion rule: mint
**CCF-022** when `modules/io` is reviewed, citing all five, and not before.

### 5.5 X-E — a public surface exists and is inert

Members: SR-AUD-352. Root cause: the notification API was ported as storage without call-site
dispatch. **#2079.**

### 5.6 X-F — the port's lexical space is the framework's, not XML's

Members: SR-AUD-354 (XSD duration), and adjacent to SR-AUD-355 (the XPath data model's logical
text node vs the parser's native node). Root cause: a conversion or navigation delegates to a
general-purpose implementation whose grammar/data model is not XML's. #2080 (deferred), #2081.

---

## 6. Corrected premises

| # | The record said | Measured |
|---|---|---|
| 6.1 | SR-AUD-351 is about `RemoveChild` | **Four** public mutators: `RemoveChild`, `InsertBefore`, `InsertAfter`, `ReplaceChild`. They fail in **three different ways**, and `InsertAfter` **drops the new node entirely** — a lifetime question, not only a correctness one. |
| 6.2 | SR-AUD-349 is about element names and writer state | Confirmed, and the module **already ships the validator** (`XmlConvert::VerifyName`) and **already uses it** in `XmlDocument::CreateElement`. The repair routes the writer through it rather than inventing a grammar. Six distinct writer inputs are open, not one. |
| 6.3 | SR-AUD-350 needs "atomic replacement semantics" | Confirmed, and `XmlDocument::LoadXml` in the **same module** already parses first and throws a shaped `XmlException`. The repair is an ordering change plus an existing error path, not a new policy. |
| 6.4 | SR-AUD-353 is about user-declared outer prefixes | Confirmed, **plus** the built-in `xml` prefix — which this manager installs itself and documents as permanent — is invisible to `HasNamespace` from any inner scope. |
| 6.5 | SR-AUD-348 says a closed reader "can consume input" | Confirmed, **and worse**: `Read()` assigns `ReadState::Interactive`, so the closed state does not survive one call. Note the reader is **not streaming** — `Create` flattens the document into an event vector — so no I/O is consumed; the defect is the lifecycle, not the I/O. |
| 6.6 | — | **`XmlDocument` node-change handlers are public *data members*, not accessors.** Wiring them needs no layout or signature change, which is what makes #2079 compatible. |
| 6.7 | — | XPath is **not a separable review unit**: `Xml.XPath` is a CMake **alias of the same archive** as `Xml`. |

---

## 7. Public-input and security surface, measured

Requested explicitly by the review brief; measured in `2073_probe1_before.log`. **Two genuine
positives worth recording as non-findings, and two post-audit acceptance defects.**

| Input | Measured | Assessment |
|---|---|---|
| internal entity chained ten-fold (`billion laughs` shape) | `&b;` is **not expanded** — `innerText` is the literal three characters | **Not a vulnerability.** The substrate does not expand internal entities at all. It is a *parity gap* (DTD entities unsupported), not an exposure. Recorded, no ticket. |
| 2,000-deep nesting | `XML_ELEMENT_DEPTH_EXCEEDED` | **A depth bound already exists.** No stack-overflow exposure through `LoadXml`. Recorded, no ticket. |
| duplicate attribute `<r a='1' a='2'/>` | rejected, `XML_ERROR_PARSING_ATTRIBUTE` | correct |
| embedded NUL in the document | rejected, `XML_ERROR_PARSING_TEXT` | correct |
| **undeclared entity `<r>&nope;</r>`** | **accepted**, and round-trips as `<r>&amp;nope;</r>` | **Post-audit defect.** An undeclared entity reference is silently reinterpreted as literal text *and re-escaped*, so the document's own text changes. .NET throws. **Deferred #2082** — narrowing parser acceptance needs evidence. |
| **undeclared namespace prefix `<p:r/>`** | **accepted**, no resolution, round-trips unchanged | **Post-audit defect.** .NET's `XmlDocument` rejects an undeclared prefix. **Deferred #2083** — same reason. |
| external resources | `XmlResolver`/`XmlUrlResolver`/`XmlSecureResolver` exist; the substrate resolves **no** external entity or DTD | no external-fetch exposure through `LoadXml`; recorded, no ticket |

**No `SR-AUD-*` identifier is issued for the two post-audit defects.** They carry ordinary
ticket numbers, exactly as #2051/#2055 and #2072 did.

---

## 8. Dependency graph of the tickets

```
#2074  InnerXml atomicity            (P1) ── independent
#2075  DOM mutator ownership         (P1) ── independent of #2074, but SHARES XmlNode.cpp:
                                              land #2074 first, #2075 second
#2077  HasNamespace across scopes    (P2) ── independent, one line
#2078  reader Close enforcement      (P2) ── independent
#2076  writer name + state validation(P2) ── independent; routes through XmlConvert::VerifyName
#2079  node-change event dispatch    (P2) ── AFTER #2074 and #2075: the events must fire from
                                              the repaired mutators, not the broken ones
#2081  XPath text-node collapsing    (P3) ── last; touches the navigator's whole cursor model
#2080  XSD duration                       ── deferred verification, evidence absent
#2082  undeclared entity                  ── deferred verification, evidence absent
#2083  undeclared namespace prefix        ── deferred verification, evidence absent
```

---

## 9. Compatible versus approval-sensitive

| Ticket | Compatible? | Why |
|---|---|---|
| #2074 | yes, **with a documented narrowing** | invalid input that silently destroyed content now throws `XmlException`; no type, member or signature change |
| #2075 | yes, **with a documented narrowing** | four mutators reject a foreign node; .NET already throws for all four |
| #2076 | yes, **with a documented narrowing** | routes through an existing validator; no new type |
| #2077 | yes, fully | one predicate becomes consistent with its own sibling; nothing that was `true` becomes `false` |
| #2078 | yes, **with a documented narrowing** | a closed reader stops advancing; the state field already exists |
| #2079 | yes, **with a behaviour addition** | the handlers are public data members already; a previously inert callback begins firing |
| #2081 | yes, **with value changes** | navigator values and positions change for mixed text/CDATA runs |
| #2080, #2082, #2083 | **no** — evidence absent | each would widen or narrow accepted input on an unverified premise |

**Nothing in this namespace requires an object-layout, vtable, base-class or public-type
change.** That is unusual for this repository and is the main reason the compatible queue is
six deep.

---

## 10. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` |
|---|---|---|---|---|
| #2074 | narrows accepted input on one public setter | none | none | none |
| #2075 | narrows accepted input on four public mutators | none | none | none |
| #2076 | narrows accepted input on the writer's name and state doors | none | none | none |
| #2077 | one predicate returns `true` in more cases | none | none | none |
| #2078 | a closed reader stops advancing | none | none | none |
| #2079 | a public callback begins firing | none | none | none |
| #2081 | navigator values change | none | none | none |

`sizeof`/`alignof` of every public type in the module must be unchanged and are to be pinned by
the first implementation ticket that lands, using the **probe-struct** technique
`docs/SystemNetHttpNamespaceReviewPlan.md` §20.4 established — **not** literal byte counts,
because these types hold `std::string`/`std::map` whose sizes differ between libstdc++ and
libc++ and the platform policy requires the MinGW/Emscripten/Apple-Clang builds to keep
compiling.

---

## 11. Observable semantic consequences

- **#2074** — `node.InnerXml = "<bad>"` throws `XmlException` instead of emptying the node.
  A caller who *relied* on the clearing side effect must call `RemoveAll()`.
- **#2075** — the four mutators throw `System::ArgumentException` for a node that is not a
  child of the receiver. Code that was accidentally mutating the wrong subtree stops.
- **#2076** — an invalid element/attribute name, an unbalanced `WriteEndElement`, and any write
  after `Close()` throw instead of producing unreadable markup or being discarded.
- **#2077** — `HasNamespace(prefix)` returns `true` for an inherited or built-in prefix.
  Nothing that returned `true` returns `false`.
- **#2078** — `Read()` after `Close()` returns `false` and `ReadState` stays `Closed`.
- **#2079** — a previously inert handler begins being invoked. **This is the only ticket in the
  namespace that can run caller code that never ran before**, and it needs the migration note.
- **#2081** — `XPathNavigator` reports one logical text node with a concatenated value where it
  used to report several.

A migration note (`docs/Migration-XmlStrictnessAndLifecycle.md`) covers #2074–#2079 together
when the first of them lands, on the `Migration-HttpControlCharacterRejection` model.

---

## 12. Test matrix

| Ticket | Required cases |
|---|---|
| **#2074** | invalid fragment with existing children (children **survive**, `XmlException` thrown, message names the parse error); invalid fragment on an empty node; valid fragment unchanged; empty string clears; a fragment that parses but has no root element; the document is otherwise untouched after the throw |
| **#2075** | each of `RemoveChild`/`InsertBefore`/`InsertAfter`/`ReplaceChild` with a foreign `refChild`/`oldChild`; the same four with a **correct** child (unchanged); a node with **no** parent; a node from **another document** (already throws — unchanged); `ReplaceChild` leaves both trees intact after the throw; **and an ASan run over the `InsertAfter` drop path**, before and after |
| **#2076** | `1bad`, `a b`, `""`, `a<b`, `a:b:c` as element and attribute names; a **valid** name with a colon, a dot, a hyphen and an underscore (accepted); unbalanced `WriteEndElement`; write after `Close`; every existing writer test still green; the emitted document is re-readable by this module's own reader |
| **#2077** | inherited prefix from an outer scope; the built-in `xml` prefix; a prefix declared in the current scope; a prefix that does not exist (still `false`); after `PopScope`, the popped prefix is gone; agreement with `LookupNamespace` asserted **as a property** over a scope stack |
| **#2078** | `Read()` after `Close()` returns `false`; `ReadState` stays `Closed` across repeated `Read()`; `Close()` twice; property accessors after `Close()`; a reader that was never read |
| **#2079** | insert, remove and value change each dispatch the matching pair; the `*ing` handler fires **before** and the `*ed` handler **after**; the event args carry action, node, old and new parent; **no** handler installed is still a no-op; a handler that throws does not corrupt the tree |
| **#2081** | text+CDATA, CDATA+text, text+text, three-node runs; `MoveToNext` skips the rest of the run; `getValueProperty` returns the concatenation; a run bounded by elements on both sides; `MoveToPrevious` symmetric |
| **pins** | #2080's four measured behaviours; #2082's `&amp;nope;` round-trip; #2083's `<p:r/>` acceptance; the `sizeof` probe structs |

---

## 13. Sanitizer matrix

| Ticket | ASan | UBSan | LSan | TSan |
|---|---|---|---|---|
| #2074 | **yes, discriminating** — the failure path must not leave a wrapper pointing at a deleted `tinyxml2` node; `RemoveAllChildren` calls `PurgeCache` then `DeleteChildren` | no | possibly — a fragment parsed and then discarded | no |
| #2075 | **yes, discriminating** — `InsertAfter`'s dropped node (§4.2) is an open ownership question and **must** be answered by measurement | no | **yes** — a node owned by the document pool but reachable from no tree | no |
| #2076 | no | no | no | no |
| #2077 | no | no | no | no |
| #2078 | no | no | no | no |
| #2079 | possibly — a handler that mutates during dispatch | no | no | no |
| #2081 | **yes** — run collapsing keeps references across native siblings | no | no | no |

`Xml` is a **STATIC** component, so a probe that links `build/libsharp_runtime_xml.a` measures
an **uninstrumented** body. Either rebuild into the sanitizer tree or compile the `.cpp` into
the probe directly, and say which — the rule `docs/SystemNetHttpNamespaceReviewPlan.md` §12
established and #2063/#2065 followed. `vendor/tinyxml2` must be compiled from source into the
probe too, since the ownership questions live at the boundary between the port and it.

---

## 14. Ticket split

### 14.1 Compatible, ready

| # | P | Size | Scope | Findings | Family |
|---|---|---|---|---|---|
| **#2074** | P1 | S | parse before destroying, so `InnerXml` replacement is atomic | SR-AUD-350 | X-A |
| **#2075** | P1 | M | four DOM mutators reject a node that is not a child of the receiver | SR-AUD-351 | X-B |
| **#2077** | P2 | XS | `HasNamespace` searches every active scope, as `LookupNamespace` already does | SR-AUD-353 | — |
| **#2076** | P2 | M | route writer names through `XmlConvert::VerifyName`; enforce writer state | SR-AUD-349 | X-C, X-D |
| **#2078** | P2 | S | a closed reader stops advancing and stays closed | SR-AUD-348 | X-D |
| **#2079** | P2 | M | dispatch the six node-change events from the repaired mutators | SR-AUD-352 | X-E |
| **#2081** | P3 | L | collapse adjacent text-like runs into one XPath logical node | SR-AUD-355 | X-F |

### 14.2 Deferred verification

| # | P | Scope | Findings |
|---|---|---|---|
| **#2080** | P3 | does .NET's `XmlConvert` accept the native `1.00:00:00` form alongside `xs:duration`, and with what year/month factors? | SR-AUD-354 |
| **#2082** | P3 | does .NET reject an undeclared entity reference, or reinterpret it? | post-audit, no `SR-AUD-*` |
| **#2083** | P3 | does .NET's `XmlDocument` reject an undeclared namespace prefix? | post-audit, no `SR-AUD-*` |

### 14.3 Blocked on approval

**None.** No finding in this namespace needs a layout, vtable, base-class or public-type
change.

---

## 15. Recommended implementation order

1. **#2074** — the highest-consequence defect (silent content loss), smallest blast radius, and
   the module already contains the correct pattern one call away.
2. **#2075** — same file; must land after #2074 so the two `XmlNode.cpp` changes are separable
   in history, and it carries the ASan question §13 requires answering.
3. **#2077** — one line, no risk, closes a finding outright.
4. **#2078**, then **#2076** — the two lifecycle/validation tickets.
5. **#2079** — after #2074 and #2075, so the events fire from repaired mutators.
6. **#2081** — last; the largest and the only one that can make the navigator internally
   inconsistent if done partially.
7. Stop. #2080, #2082 and #2083 need evidence that does not exist in this container.

---

## 16. Deferred evidence

`/rv/tmp/runtime/src/libraries/` is absent, re-verified 2026-08-04. The following are **not**
decided by this review and must not be guessed:

- .NET's `XmlConvert.ToTimeSpan`/`ToString(TimeSpan)` `xs:duration` grammar, its year/month
  conversion factors, and whether the native `1.00:00:00` form stays accepted (#2080);
- whether .NET rejects an undeclared entity reference or reinterprets it as text (#2082);
- whether .NET's `XmlDocument` rejects an undeclared namespace prefix (#2083);
- the exact exception type and message .NET raises for each of #2074–#2078. This port uses
  `XmlException` where the module already does and `System::ArgumentException` where the
  parameter is the defect, and **records those as this port's choices**.

---

## 17. Promotion rules for the families this review declines to mint

- **X-D (a public lifecycle state recorded but not enforced).** Open here as SR-AUD-348 and
  SR-AUD-349's second clause, and open in `modules/io` as SR-AUD-337, SR-AUD-343 and
  SR-AUD-344 — the same sentence about `StreamReader`, `StringWriter`/`StringReader` and
  `UnmanagedMemoryStream`. Five sites across two modules is a pattern, but `modules/io` is
  unreviewed. **Mint CCF-022 when `modules/io` is reviewed, citing all five.** Do not mint it
  from this module's evidence alone — the discipline the Buffers review's §5.3 and the Net.Http
  review's §18 both applied.
- **X-C (a public door bypassing a validator the module already ships).** One module so far.
  Watch for it in `modules/xml-linq`, whose SR-AUD-335 is the mirror image — that module's
  writer *does* sanitise CDATA/comment/PI delimiters while this one does not validate names.
- **CCF-021 (a control character crossing a public door into a protocol frame)** is still
  unminted. `System::Net::Http` §18 says to mint it when `Net.Http.Headers` (SR-AUD-319, 322)
  or `Net.WebSockets` (SR-AUD-248) is reviewed. **That recommendation is now stronger**: with
  SR-AUD-313 remediated, `net-websockets` is both the CCF-021 site and the third CCF-019 site,
  which is the measured reason it is the recommended unit after `modules/xml`.

---

## 18. Exclusions

- `modules/xml-linq` — a separate component with its own four findings.
- `vendor/tinyxml2` — third-party source; measured, never edited.
- XSD **validation** — absent by design; the port ships the exception types only.
- Streaming/pull parsing — `XmlReader` flattens the document up front by design; that is a
  documented simplification, not a finding, and no open finding names it.
- CNA and mobile-eggbert — not inspected; #1773 stays blocked.

---

## 19. Completion criteria

This review (#2073) is complete when this document exists, each of the eight open findings has
exactly one disposition in §4, and §14's tickets are in `plan.sqlite3`. **It is complete on
those terms and remediates nothing by itself.**

`System::Xml` is closed for *compatible* work when:

1. #2074, #2075, #2076, #2077, #2078, #2079 and #2081 are `done`;
2. SR-AUD-348, 349, 350, 351, 352, 353 and 355 are `remediated`;
3. SR-AUD-354 carries a deferred-verification ticket and a pin;
4. the two post-audit defects of §7 carry deferred tickets and pins;
5. the repository gate shows no new failure and `SharpRuntimeTests_Xml` has grown, add-only;
6. §13's ASan question about `InsertAfter`'s dropped node is **answered by measurement**, not
   by reading.

---

## 20. Implementation record — corrections made while implementing

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.

### 20.1 #2074 landed exactly as §4.1 specified

The repair is an ordering change plus the error path `XmlDocument::LoadXml` already used:
parse into the scratch document **first**, throw a shaped `XmlException` on failure, clone
every child **before** touching this node, and only then remove the old children and insert.
The extra clone-first step is not in §4.1 and was decided while implementing: it means a
failure inside `DeepClone` cannot leave the node stripped of its old content and missing its
new content too.

Measured before → after (`2073_probe1_before.log` → the same probe re-run):

| Input | Before | After |
|---|---|---|
| `<keep/>` then `setInnerXml("<bad>")` | `innerXml ""`, **no exception** | `XmlException`, `innerXml "<keep/>"` |
| `setInnerXml("<a/><b/>")` | `<a/><b/>` | **unchanged** |
| `setInnerXml("")` | clears | **unchanged** |
| `setInnerXml("plain text")`, `"a<b/>c"` | accepted | **unchanged** |

**Mutation-checked.** Restoring the original ordering — `RemoveAllChildren()` before the parse
— fails **exactly** `InvalidFragment_ThrowsAndKeepsTheExistingChildren` and
`RepeatedFailuresLeaveTheNodeUsable`, and nothing else in the 394-test suite. A first attempt
at the mutation (`if (false && Parse(...))`) was **discarded rather than reported**: `&&`
short-circuits, so `Parse` was never called at all and three unrelated tests failed instead.
A mutation that changes more than the guard proves nothing about the guard.

### 20.2 #2075 — the ASan question §13 required is **answered, and the answer is a non-result**

§4.2 flagged `a->InsertAfter(n, foreignRef)` dropping `n` as *"an ASan question, not a reading
question"* and §13 made answering it a closure condition. Answered, with `XmlNode.cpp`,
`XmlDocument.cpp`, `XmlElement.cpp` **and `vendor/tinyxml2/tinyxml2.cpp` compiled from source**
into the probe (`build-probe/2075_probe1_asan.cpp`) — `Xml` is a `STATIC` component and the
question lives exactly at the port/substrate boundary:

**ASan, UBSan and LSan are all clean, before the repair, including over 200 consecutive drops**
(`2075_probe1_before.log`). The dropped node is **allocated from the document's own node pool
and freed with the document**, so it is an *orphan* — neither a leak, nor a dangling wrapper,
nor a double free. The caller's `XmlNode*` stays valid and readable.

That is a **measured non-result and it corrects this review's own suspicion.** The defect is
therefore precisely the *silent acceptance*, not a memory-safety hazard, and the closure
evidence is the behavioural test, not a sanitizer run. The instrumentation is proven live by a
control heap-use-after-free in the same binary.

**Four mutators, one guard.** `ThrowIfNotChildOf` joins the two guards the file already had.
`ReplaceChild` checks **before** delegating to `InsertBefore`, because otherwise a foreign
`oldChild` would leave `newChild` already spliced in when the removal threw — the very partial
state #2074 exists to prevent, reproduced one function away.

Measured after: all five rows of §4.2's table throw and both trees stay intact; the same four
calls with a **correct** child are unchanged; a null `refChild` still means append/prepend; and
the pre-existing cross-document and ancestry guards still fire.

**Mutation-checked.** Emptying `ThrowIfNotChildOf` fails **exactly** the five ownership tests
and nothing else in the 394-test suite.

**Exception wording is this port's choice.** `"The node to be removed is not a child of this
node."`, `"The reference node is not a child of this node."` and `"The node to be replaced is
not a child of this node."` follow this file's existing `ArgumentException` style; .NET's exact
text is not verifiable here and §16 already says so.

### 20.3 #2077 — the defect was **pinned by a pre-existing test**, which is why it survived

The repair is one expression: `HasNamespace` is now `LookupNamespace(prefix).has_value()`,
expressed in terms of its sibling rather than duplicating its loop, so the two can no longer
disagree.

**The correction the review did not predict.** `XmlNamespaceManagerTests` contained
`HasNamespace_ChecksCurrentScopeOnly`, asserting `EXPECT_FALSE(mgr.HasNamespace("foo"))` after
`PushScope()`. That test **locked in the defect SR-AUD-353 reports**, against the type's own
documented contract (*"true if @p prefix is declared in any scope"*) and against
`LookupNamespace`, which has always searched outward. §14's ticket allowed updating an existing
test "with a recorded reason"; this is that reason, and the test was **corrected in place and
renamed rather than deleted**, with a comment explaining the history, so the claim's provenance
stays visible.

Measured after: an inherited prefix, a prefix two scopes deep, and the built-in `xml`, `xmlns`
and default declarations are all visible; an undeclared prefix and a popped prefix are still
`false`; prefixes stay case-sensitive. The suite additionally asserts the **property** that
made the desynchronisation possible — over a four-deep push/pop cycle, `HasNamespace` and
`LookupNamespace` agree for every probe prefix, on the way down and on the way back up.

**Mutation-checked.** Restoring the single-scope search fails **exactly** the three new
scope-search tests, the agreement property and the corrected pre-existing test — five
assertions across four tests — and nothing else in the 400-test suite.

### 20.4 #2076 — four name doors, not two, and the PI target was the sharpest

§4.3 scoped this ticket to element and attribute names plus writer state. Measured against
`1b65f0f` (`build-probe/2076_probe1_writer_doors.cpp`, log `2076_probe1_before.log`), **four**
public doors take an XML name and validated none of them, and the two §4.3 does not name are
not decorative:

| Door | Before | After |
|---|---|---|
| `WriteStartElement("1bad")` | `<1bad/>`, rejected by this module's own reader | `XmlException` |
| `WriteAttributeString("1bad","v")` | `<e 1bad="v"/>`, rejected by the same reader | `XmlException` |
| `WriteProcessingInstruction("a?>b","d")` | `<?a?>b d?>` — the `?>` **closed the instruction early** and spilled `b d?>` into document-level text | `XmlException` |
| `WriteDocType("1bad",…)` | `<!DOCTYPE 1bad>` | `XmlException` |

The PI row is the one worth reading twice: the writer **already** sanitized the PI *data*
against exactly this hazard (`sanitizeProcessingInstructionText`), so the module demonstrably
understood the shape — and the *target* had no door at all. That is X-C's sentence applied
inside a single function.

**The state clause was wider too.** All **twelve** `Write*` members stayed callable after
`Close()` and silently discarded their argument; an unbalanced `WriteEndElement()` and an
attribute written with no element open were discarded as well. All now throw
`System::InvalidOperationException` — this port's choice, recorded as such per §16.
`Close()` marks the writer closed **before** flushing, so a writer whose save failed is
terminally closed rather than accepting writes into a document it could not persist; it stays
idempotent because `~XmlWriter()` calls it unconditionally, and `ToString()`/`Flush()` stay
usable because `ToString()` is the only way to read an in-memory writer's result back.

**The corrected premise of §6.2 held exactly.** All four doors route through
`XmlConvert::VerifyName`, so the writer door and `XmlDocument::CreateElement` now report an
**identical** diagnostic for identical input — including `ArgumentException` for an empty name,
the validator's own pre-existing choice, deliberately not re-mapped. No name grammar was
invented.

**One narrowing beyond §11's list, recorded rather than glossed:** a name with a **leading
colon** (`:lead`) was accepted before and produced re-readable markup; `VerifyName` rejects it,
because `IsStartNCNameChar` does not admit `:`. That is the module's own validator's rule and
the DOM door has always applied it, so the alternative would have been to make the writer
*more* permissive than the DOM — which is the defect, reversed.

**+27 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Xml` 400 → 427; `SharpRuntimeTests_Xml_Linq` stays 184 green even though
`modules/xml-linq` calls every repaired door through `WriteTo`/`Save`.

**Nine mutations, each reverted from an exact backup.** M1 (`ThrowIfClosed` emptied) → 3 tests;
M2 (unbalanced `WriteEndElement` silent again) → 3; M3 (element name) → 7; M4 (attribute name)
→ 4; M5 (PI target) → 2; M6 (DOCTYPE name) → 1; M7 (no-open-element guard reverted to the
original silent-drop shape) → 1; M8 (`Close()` idempotency) → 1; M9 (`closed` set after the
flush) → 1. **A false pass is recorded rather than hidden:** M7's first attempt left the helper
unused, the build aborted on `-Werror=unused-function`, and the run reported the **stale**
binary's `427 PASSED`. Re-run with the build output unsuppressed, it discriminated. The lesson
generalises — a mutation run whose build output is suppressed measures the previous binary.

**Sanitizers — §13's "no" is now measured, not assumed.** ASan/UBSan/LSan clean over 3,700
rejections and the abandoned-mid-document destructor path, with `XmlWriter.cpp`,
`XmlConvert.cpp` and `vendor/tinyxml2/tinyxml2.cpp` compiled **from source** (`Xml` is
`STATIC`), instrumentation proven by a control heap-use-after-free
(`build-probe/2076_probe4_asan.log`).

**Two post-audit defects found while reproducing, deliberately NOT absorbed** — both carry
ordinary ticket numbers and **no `SR-AUD-*` identifier; numbering stays frozen at 364**:

- **#2084** — `WriteDocType`'s `publicId`/`systemId`/`internalSubset` are **quoted-literal**
  doors with no validation. `systemId = "s\">x<!--"` emitted
  `<!DOCTYPE r PUBLIC "p" "s">x<!--">`, and an `internalSubset` containing `]` emitted
  `<!DOCTYPE r []><evil/><!--]>`. **This module's own reader rejects both**, so it is
  SR-AUD-349's closure sentence on a *literal* door rather than a *name* door — the same X-C
  family, a different input class. Not absorbed because the minimal repair for `systemId` and
  `internalSubset` is a delimiter/escaping decision this review has no evidence to make,
  whereas `publicId` already has `XmlConvert::VerifyPublicId`. Splitting it keeps #2076's
  claim exact (`build-probe/2076_probe3_injection.log`).
- **#2085** — an embedded NUL **silently truncates content** at three doors:
  `WriteString("a\0b")` → `<e>a</e>`, an attribute value `"x\0y"` → `a="x"`,
  `WriteCData("c\0d")` → `<![CDATA[c]]>`. Every writer body passes `std::string::c_str()` to
  tinyxml2, so the byte count is lost at the boundary. This is X-A's shape (silent loss on a
  public door), not X-C's, and it interacts with `XmlWriterSettings::CheckCharacters`, which
  the header already documents as *"Not currently enforced"*
  (`build-probe/2076_probe2_content.log`).

**Measured and recorded with no ticket**, so a later reader does not re-derive them: two roots
(`<a/><b/>`) and document-level text are accepted, but both are governed by
`XmlWriterSettings::ConformanceLevel`, which the settings header documents as *"Not currently
enforced"* — narrowing them is a settings-semantics decision, not this finding. A PI target of
`xml` is accepted, and `XmlReader.cpp` documents a **deliberate** handling for that exact
target, so rejecting it would contradict a stated in-module contract.

### 20.5 #2078 — the class already had a correct terminal state one function away

§4.4 predicted the shape and the repair; measurement widened both.
`build-probe/2078_probe1_reader_close.cpp` (log `2078_probe1_before.log`) shows the closed
state was destroyed by **three** members, not one — `Read()`, and
`ReadStartElement()`/`ReadElementContentAsString()`, which advance through it — while
`MoveToElement()`, `MoveToNextAttribute()` and `GetAttribute()` all kept answering from a
closed reader, and the four value accessors kept reporting the last node.

| Member, after `Close()` | Before | After |
|---|---|---|
| `Read()` | `true`, advanced, state → `Interactive` | `false`, parked, state stays `Closed` |
| `ReadStartElement()` / `ReadEndElement()` | advanced, state → `Interactive` | `XmlException`, state stays `Closed` |
| `ReadElementContentAsString()` | `"t"`, advanced, state → `Interactive` | `""`, state stays `Closed` |
| `MoveToElement()` / `MoveToNextAttribute()` | `true` | `false` |
| `GetAttribute("a")` | `"1"` | `""` |
| `getNodeType` / `Name` / `Value` / `IsEmptyElement` | the last node | `None` / `""` / `""` / `false` |

**The corrected premise this ticket adds, and the reason it is small.** §4.4 justified the
selected behaviour by self-consistency with `getReadStateProperty()`. Measurement supplies
something stronger: **this class already implements one terminal read state correctly.** A
reader driven past its last event reports `EndOfFile`, returns `false` from every further
`Read()`, and never leaves that state. `Closed` is now literally the same shape. That is the
same discipline as #2074 (`LoadXml` already parsed first) and #2076 (`VerifyName` already
existed) — the third ticket in this namespace whose repair was already present in the module
and simply not reached from the defective door.

Three consequences follow, and each is what keeps the ticket compatible:

- **No new state field.** `ReadState::Closed` *is* the flag, and `Close()` is its only
  writer — so nothing was added even to the opaque `XmlReaderState`.
- **No invented accessor values.** Every accessor routes the closed case into the early
  return it already had for "there is no current node".
- **No new exception identity.** `ReadStartElement`/`ReadEndElement` throw the same
  `XmlException` they already threw for any other wrong position.

**+12 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Xml` 427 → 439.

**Three mutations, each reverted from an exact backup**, with the build output unsuppressed
after §20.4's stale-binary lesson: R1 (`Read()` unguarded again) → 5 tests; R2
(accessors/navigation ignore the closed state) → 7; R3 (`isClosed` always false) → 10. The two
**control** tests — `EndOfFile` unchanged, and reading unaffected until `Close()` — fail under
**none** of the three, which is what proves the narrowing is confined to the closed state
rather than merely correlated with it.

**Sanitizers: §13's "no" holds, with the reason stated.** The repair adds no allocation, no
arithmetic and no ownership change, and every guard strictly *shrinks* the set of reachable
event indices.

**CCF-022 is still not minted.** Cause X-D now has two remediated members in this module
(SR-AUD-348 here, SR-AUD-349's second clause in #2076) and three open in `modules/io`
(SR-AUD-337, 343, 344). §17's rule is unchanged: mint it when `modules/io` is reviewed, citing
all five, and not from this module's evidence alone.
