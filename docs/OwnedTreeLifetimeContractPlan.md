<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Owned-tree lifetime contract plan — CCF-019

*Design record for ticket **#1885**, audit findings **SR-AUD-327**
(`System::Text::Json::Nodes::JsonNode`) and **SR-AUD-333**
(`System::Xml::Linq::XObject`). Recorded 2026-07-30, **before any production
change**. No CCF-019 implementation is approved; this document ends with the
exact approval request.*

Everything in sections 4, 5, 7, 8, 15–17 and 20–21 marked **measured** comes from
retained probe evidence in `build-probe/1885_*`. Everything marked **proposed**
is a design choice this document asks the user to accept or reject.

---

## 1. Executive summary

Both open CCF-019 members have the same one-line cause and **different**
surfaces, different public contracts, and different repair scopes.

The cause: **a child stores its parent as a raw, non-owning pointer, and no
destruction path ever clears it.** Children are owned by `shared_ptr`, so a
child can outlive its parent; the moment it does, every parent-walking accessor
reads freed storage.

**Measured this batch, against the shipped bodies, one process per case under a
watchdog:**

| | JsonNode (SR-AUD-327) | XObject (SR-AUD-333) |
|---|---|---|
| Cases run | 21 | 26 |
| ASan `heap-use-after-free` | **8** | **21** |
| ASan `stack-overflow` | 2 | 1 |
| Reads-after-free with `-fsanitize-recover=address` | 8 | 49 |
| **Writes**-after-free | **0** | **0** |
| Silent (no diagnostic) wrong-value cases | 6 | 6 |
| Public entry points that reach a UAF | 4 | **14** |
| Copy/move on the public node type | **implicitly generated** | already `= delete` |

The audit's own severity is **understated for both**, and understated
*differently* — section 6 lists nine premises corrected by measurement, none of
which receives a new `SR-AUD-*` identifier (numbering stays frozen at **364**).

**Selected design, both families: the owner detaches its children in its own
destructor.** It is the contract ticket #1769 already selected for
`LinkedListNode<T>` under this same cross-cutting cause, it closes 27 of the 29
measured use-after-free accesses, and it costs **zero bytes of object layout,
zero vtable slots, zero allocations and zero per-access work** (section 13/14).
Every other candidate was rejected with measured evidence (section 12), including
the one that reproduces .NET's contract exactly — a strong parent link — which
**leaks by construction**: 2 objects constructed, **0 destroyed**, 272 bytes in 3
allocations, LeakSanitizer-confirmed.

What the selected design does **not** fix, and what therefore needs its own
approval, is the *borrowed-view* half of the surface: a raw `JsonNode*` /
`XElement*` a consumer already holds, `XElement::getAttributesProperty()`'s
reference to an internal vector, `Extensions::Ancestors`'s
`std::vector<XElement*>`, the two container iterator pairs, and `JsonNode`'s
implicitly generated copy operations. Those are separate, individually
approvable tickets (section 25).

**Nothing in this document has been implemented.** SR-AUD-327 and SR-AUD-333
remain `confirmed`.

---

## 2. Exact scope

### 2.1 In scope

- The ownership and lifetime of every public object in
  `System::Text::Json::Nodes` and `System::Xml::Linq`.
- Every public entry point of those namespaces that reads, writes, or depends on
  a parent link, a child store, an attribute store, or an iterator into either.
- The .NET lifetime contracts those namespaces port, read from
  `/rv/tmp/runtime/src/libraries/` on 2026-07-30 (not from memory).
- The candidate representations, their measured cost, and the destruction and
  cycle proofs each requires.

### 2.2 Out of scope for this ticket

- Any production change. This ticket writes a design record, planning rows, and
  probes; it changes no header, no body, no test.
- `SR-AUD-357` / `LinkedList<T>` — already `remediated` (#1768/#1769).
- `SR-AUD-361` / `SortedSet<T>` — never a CCF-019 member, already `remediated`
  (#1782/#1783).
- Downstream consumers. **CNA and mobile-eggbert were not inspected, searched,
  built or modified**, and #1773 stays `blocked`.
- `SR-AUD-328` (JsonValue integer truncation) and `SR-AUD-336` (inert
  `Changed`/`Changing` events) — same files, different causes, still `confirmed`
  and untouched. §16.7 records what the events mean for the ownership graph, and
  nothing more.

---

## 3. Complete file / type / public-surface inventory

### 3.1 `System::Text::Json::Nodes` — 5 files, 4 types, 41 public entries

| File | Lines | Types |
|---|---|---|
| `modules/text-json/include/System/Text/Json/Nodes/JsonNode.hpp` | 96 | `JsonNode` (abstract base) |
| `modules/text-json/include/System/Text/Json/Nodes/JsonArray.hpp` | 110 | `JsonArray` |
| `modules/text-json/include/System/Text/Json/Nodes/JsonObject.hpp` | 129 | `JsonObject` |
| `modules/text-json/include/System/Text/Json/Nodes/JsonValue.hpp` | 114 | `JsonValue` |
| `modules/text-json/include/System/Text/Json/Nodes/JsonNodeOptions.hpp` | 18 | `JsonNodeOptions` |
| `modules/text-json/src/System/Text/Json/Nodes/JsonNode.cpp` | 86 | out-of-line bodies |

`JsonNode`: `getOptionsProperty`, `getParentProperty`, `getRootProperty`,
`GetValueKind`, `toNlohmann`, `AssignParent`, `DetachParent`, `AsArray`,
`AsObject`, `AsValue`, `DeepClone`, `ToJsonString`, `ToString`, `DeepEquals`,
`Parse` — **15**.
`JsonArray`: ctor, `getCountProperty`, `operator[]`, `SetItem`, `Add`, `Insert`,
`RemoveAt`, `Remove`, `Clear`, `IndexOf`, `begin`, `end`, plus 3 overrides — **15**.
`JsonObject`: ctor, `getCountProperty`, `ContainsKey`, `TryGetPropertyValue`,
`Add`, `Remove`, `operator[]`, `SetItem`, `Clear`, `begin`, `end`, plus 3
overrides — **14**.
`JsonValue`: 3 predicates, 5 getters, 5 `Create` overloads, `FromNlohmann`,
plus 3 overrides — **17**.

Plus the **implicitly generated** copy constructor and copy assignment operator
on all four types, which the headers never mention (§7.4).

### 3.2 `System::Xml::Linq` — 22 headers, 12 bodies, 18 types

| File | Type(s) | Owns |
|---|---|---|
| `XObject.hpp` / `.cpp` | `XObject` | `XContainer* parent_` (protected, non-owning) |
| `XNode.hpp` / `.cpp` | `XNode` | — |
| `XContainer.hpp` / `.cpp` | `XContainer` | `std::vector<std::shared_ptr<XNode>> children_` |
| `XElement.hpp` / `.cpp` | `XElement` | `XName name_`, `std::vector<std::shared_ptr<XAttribute>> attributes_` |
| `XDocument.hpp` / `.cpp` | `XDocument`, `XDeclaration` | `std::shared_ptr<XDeclaration> declaration_` |
| `XAttribute.hpp` / `.cpp` | `XAttribute` | `XName name_`, `std::string value_`, **`XAttribute* next_`** |
| `XText.hpp`, `XCData.hpp`, `XComment.hpp`, `XProcessingInstruction.hpp`, `XDocumentType.hpp` | leaf nodes | value strings only |
| `XStreamingElement.hpp` / `.cpp` | `XStreamingElement` | `std::vector<std::any>` — **not** in the node hierarchy |
| `Extensions.hpp` | 15 free templates | returns `std::vector<XElement*>` for `Ancestors`/`AncestorsAndSelf` |
| `XNodeDocumentOrderComparer.hpp`, `XNodeEqualityComparer.hpp` | comparers | borrow `const XNode*` |
| `XName.hpp`, `XNamespace.hpp`, `XObjectChange*.hpp`, `LoadOptions/SaveOptions/ReaderOptions.hpp` | value types / enums | no ownership |

**Public entries that read a parent link, a child store or an attribute store:**
`XObject::getParentProperty`, `XObject::getDocumentProperty`,
`XNode::getNextNodeProperty`, `XNode::getPreviousNodeProperty`, `XNode::Remove`,
`XNode::ReplaceWith` ×2, `XNode::NodesBeforeSelf`, `XNode::NodesAfterSelf`,
`XNode::CompareDocumentOrder`, `XNode::IsAfter`, `XNode::IsBefore`,
`XAttribute::getPreviousAttributeProperty`, `XAttribute::getNextAttributeProperty`,
`XAttribute::Remove`, `XContainer::Add` ×2, `XContainer::AddFirst` ×2,
`XContainer::RemoveNodes`, `XElement::Add` (attribute), `XElement::RemoveAttribute`,
`XElement::RemoveAttributes`, `XElement::RemoveAll`,
`XElement::getAttributesProperty`, `XDocument::setRootProperty`,
`Extensions::Ancestors` ×2, `Extensions::AncestorsAndSelf` ×2,
`Extensions::Remove` ×2, `Extensions::InDocumentOrder`,
`XNodeDocumentOrderComparer::Compare` ×2 and its two `operator()`s — **35**.

### 3.3 What the audit named

The cross-cutting record (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-019)
names **three files** for SR-AUD-327 and **six** for SR-AUD-333, and describes
the surface as "getRootProperty" and "getParentProperty". The measured surface is
**41 + 35 = 76 public entries across 27 headers and 13 bodies** (§6.1).

---

## 4. SR-AUD-327 reproduction (measured)

Probe: `build-probe/1885_ccf019_lifetime_probe.cpp`, built by
`build-probe/1885_build.sh`.

**Freshness.** The probe binary is produced by **one** `g++` invocation that
compiles `modules/text-json/src/System/Text/Json/Nodes/JsonNode.cpp` and the
whole of `modules/xml-linq/src` **from source into the probe**, with the same
sanitizer flags. **No prebuilt archive is linked at all**, so a stale or
non-instrumented object is structurally impossible. The exact 60-file command
line, source and binary timestamps, toolchain versions and the LeakSanitizer
activation self-test are in `build-probe/1885_env_freshness.log`.

**Harness.** Each case runs in its own `fork()`ed child under `alarm(5)`, so a
crash cannot hide later cases, a hang cannot stall the survey, and each case gets
an independent allocator state. The parent reports the exact exit status
(signal / timeout / sanitizer exit 1).

**Two builds from one source.** `asan` = `-fsanitize=address,undefined`
(detection). `none` = no sanitizer (the *silent* shapes, where ASan's quarantine
would hide allocator reuse). A third build, `asanr` =
`-fsanitize=address -fsanitize-recover=address` run with `halt_on_error=0`,
counts every faulting access rather than stopping at the first.

### 4.1 The headline case — J01

```
J01  JsonArray freed, retained child -> getRootProperty()
```

Source operation sequence:

```cpp
std::shared_ptr<JsonNode> child;
{
    auto arr = std::make_shared<JsonArray>();
    arr->Add(JsonValue::Create(std::string("v")));
    child = (*arr)[0];          // a public, supported way to retain a child
}                               // the sole owner of `arr` dies here
child->getRootProperty();       // public accessor
```

| Fact | Value |
|---|---|
| Sanitizer diagnostic | `AddressSanitizer: heap-use-after-free` |
| Access | `READ of size 8` |
| Invalid address | `0x506000000030` — 16 bytes into the 64-byte region `[0x506000000020, 0x506000000060)` |
| Faulting frame | `System::Text::Json::Nodes::JsonNode::getRootProperty()` |
| Allocation site | `operator new` ← `make_shared<JsonArray>` ← probe line 125 |
| Deallocation site | `operator delete` ← `_Sp_counted_ptr_inplace<JsonArray>::_M_destroy()` ← `~shared_ptr<JsonArray>` ← probe line 128 |
| Final access site | `JsonNode::getRootProperty()` ← probe line 129 |
| Process exit status | 1 (`==337115==ABORTING`) |
| Timeout | no |
| Without a sanitizer | **no crash** — returns `0x55c5e3aef330`, a plausible-looking pointer into freed storage |

Offset 16 is `JsonNode::parent_` inside the control-block-plus-object allocation
`make_shared` produces; the read is `n->parent_` in `getRootProperty`'s `while`.

### 4.2 The full JsonNode matrix

`ASan` = the standard `-fsanitize=address,undefined` build. `plain` = the same
source with no sanitizer. Empty ASan cell = **no diagnostic at all**.

| Case | Operation | ASan | plain | Classification |
|---|---|---|---|---|
| J01 | freed `JsonArray`, retained child → `getRootProperty()` | UAF read 8 @ `JsonNode::getRootProperty()` | returns a freed address, exit 0 | **use-after-free** |
| J02 | … → `getParentProperty()->GetValueKind()` | UAF read 8 (vptr) | `pure virtual method called`, **SIGABRT** | **use-after-free** |
| J03 | freed `JsonObject`, retained value → `getRootProperty()` | UAF read 8 | returns a freed address | **use-after-free** |
| J04 | 3-level tree freed, retained grandchild → `getRootProperty()` | UAF read 8 (2 accesses under `asanr`) | returns a freed address | **use-after-free** |
| J05 | freed parent, retained child → `getParentProperty()` (no deref) | — | non-null stale pointer | **stale parent, silent** |
| J06 | freed parent, retained child → `Add` to a live array | — | throws `The node already has a parent.` | **permanently unusable orphan** |
| J07 | … `DetachParent()` first, then `Add` | — | succeeds | safe (public escape hatch) |
| J08 | `JsonArray copy = *orig;` then both die | UAF read 8 | returns a freed address | **silent aliasing + use-after-free** |
| J09 | `JsonNode& a = *attached; a = *detached;` | — | attached node's parent becomes **NULL while still stored** | **silent corruption** |
| J10 | `JsonObject::SetItem` throwing | — | old value detached but still stored; then accepted by a **second** container | **double ownership, exception path** |
| J18 | `JsonArray::SetItem` throwing (contrast) | — | old value keeps the correct parent | safe and matching .NET |
| J11 | iterator held across 64 `Add`s | UAF read 8 | **SIGSEGV** | **stale iterator** |
| J12 | `JsonObject` iterator held across `Clear()` | — | returns `"a"` from destroyed storage | **stale iterator, silent** |
| J13 | public `DetachParent()` while still stored, then `Add` elsewhere | — | one node in two containers | **double ownership** |
| J14 | freed parent, retained child → `ToJsonString()` | — | `"v"` | safe (no parent walk) |
| J15 | options not inherited from the parent | — | inner object stays case-sensitive | **safe but divergent** (§9.1) |
| J16 | freed parent slot refilled by a `JsonObject` | UAF read 8 | stale parent reports **kind 1 (Object)** where the real parent was an Array, at the **same address** | **wrong object, silent** |
| J17 | `AssignParent` cycle guard walks a stale ancestor chain | UAF read 8 @ `JsonNode.cpp:22` | `Add` into the orphan succeeds | **use-after-free** |
| J19a/b | 1,000 / 5,000-deep nest, build then release | — | built and released | safe |
| J19c | 20,000-deep nest, build then release | **stack-overflow** | **SIGSEGV on release** | **recursive teardown** |
| J19d | 100,000-deep nest | timed out (>5 s) | timed out | **quadratic `AssignParent`** |
| X28a/b | `JsonNode::Parse` of 1,000 / 5,000 nested arrays | — | parsed and released | safe |
| X28c | `JsonNode::Parse` of 20,000 nested arrays | **stack-overflow** | **SIGSEGV** | **recursive parse on untrusted text** |

### 4.3 Silent versus detected

Six JsonNode cases produce a **wrong answer with no diagnostic in either build**:
J05, J09, J10, J12, J13, J15. Two more (J06, J16) are silent without a sanitizer.
J16 is the exact shape the sanitizer's quarantine hides: the freed `JsonArray`
slot is reused by a `JsonObject` at the **identical address**, and the retained
child then reports that unrelated object as its parent, with the wrong
`GetValueKind`.

---

## 5. SR-AUD-333 reproduction (measured)

Same probe, same freshness guarantee, same harness.

### 5.1 The headline case — X01

```cpp
std::shared_ptr<XNode> orphanTextNode() {
    auto el = std::make_shared<XElement>(XName("root"));
    std::shared_ptr<XNode> t = std::make_shared<XText>("hello");
    el->Add(t);
    el->Add(std::shared_ptr<XNode>(std::make_shared<XComment>("c")));
    return t;                    // `el` dies here
}
...
t->getParentProperty();          // public accessor
```

| Fact | Value |
|---|---|
| Sanitizer diagnostic | `AddressSanitizer: heap-use-after-free` |
| Access | `READ of size 8` |
| Invalid address | `0x50d000000050` |
| Faulting frame | `System::Xml::Linq::XObject::getParentProperty() const` at `XObject.cpp:12` |
| Allocation site | `make_shared<XElement>` inside `orphanTextNode()` |
| Deallocation site | `~shared_ptr<XElement>` at the end of `orphanTextNode()` |
| Final access site | `parent_->getNodeTypeProperty()` — a **virtual call through the freed object's vptr** |
| Process exit status | 1 |
| Timeout | no |
| Without a sanitizer | `pure virtual method called`, `terminate`, **SIGABRT (signal 6)** |

This is strictly worse than J01: `XObject::getParentProperty` does not merely
read a field, it **dispatches a virtual call** through freed storage, which is
why eight distinct Xml.Linq entry points abort even with no sanitizer.

### 5.2 The full XObject matrix

| Case | Operation | ASan | plain | Classification |
|---|---|---|---|---|
| X01 | freed `XElement`, retained `XText` → `getParentProperty()` | UAF read 8 @ `XObject.cpp:12` | **SIGABRT** (`pure virtual method called`) | **use-after-free** |
| X02 | … → `getDocumentProperty()` | UAF read 8 @ `XObject.cpp:20` (2 under `asanr`) | **SIGABRT** | **use-after-free** |
| X03 | … → `getNextNodeProperty()` | UAF read 8 @ `vector::size()` (4 under `asanr`) | returns `nil` | **use-after-free** |
| X04 | … → `getPreviousNodeProperty()` | UAF read 8 (4) | returns `nil` | **use-after-free** |
| X05 | … → `NodesBeforeSelf()` | UAF read 8 (**8**) | **SIGSEGV** | **use-after-free** |
| X06 | … → `NodesAfterSelf()` | UAF read 8 (2) | returns 0 | **use-after-free** |
| X07 | … → `Remove()` | UAF read 8 @ `XNode.cpp:65` (2) | returns silently | **use-after-free on a mutating path** |
| X08 | … → `ReplaceWith(node)` | UAF read 8 (3) | **SIGABRT** | **use-after-free on a mutating path** |
| X09 | … → `CompareDocumentOrder` | UAF read 8 @ `XNode.cpp:119` | throws `A common ancestor is missing.` | **use-after-free** |
| X10 | freed `XElement`, retained `XAttribute` → `getParentProperty()` | UAF read 8 | **SIGABRT** | **use-after-free** |
| X11 | … → `getPreviousAttributeProperty()` | UAF read 8 | **SIGABRT** | **use-after-free** |
| X12 | … → `Remove()` | UAF read 8 | **SIGABRT** | **use-after-free on a mutating path** |
| X13 | retained `XAttribute` → `getNextAttributeProperty()` after its siblings die | UAF read 8 (2) | returns a freed `XAttribute*`; reading its name yields `"b"` | **dangling sibling handle, silent** |
| X14 | orphan re-`Add`ed to a **live** element | UAF read 8 @ `XContainer.cpp:40` (2) | succeeds; `<live>hello</live>` | **use-after-free on a mutating path** |
| X15 | `Extensions::Ancestors()` raw `XElement*` used after the tree dies | UAF read 8 (2) | returns `"root"` | **borrowed handle outlives its owner** |
| X16 | `getAttributesProperty()` reference held across 63 `Add`s | — | `size() == 64` | **safe** — the reference is to the vector *object*, not its buffer |
| X17 | `getAttributesProperty()` reference outliving the element | UAF read 8 (2) | `size() == 1` from freed storage | **borrowed view outlives its owner** |
| X18 | freed `XDocument`, retained root → `getDocumentProperty()` | UAF read 8 (2) | **SIGABRT** | **use-after-free** |
| X19 | freed `XDocument`, retained grandchild → `getDocumentProperty()` | UAF read 8 (3) | **SIGABRT** | **use-after-free, two freed levels** |
| X20 | `ReplaceWith` whose replacement is rejected | — | throws, and the original node is **gone from the tree** (`<a/>`) | **exception-path data loss** |
| X21 | `Add` an already-attached node | — | **moves** it (`<src/>` … `<dst><kid/></dst>`) | **safe but divergent** (§9.2) |
| X22 | freed parent slot refilled by another `XElement` | UAF read 8 | stale parent reports name **`"squatter"`** at the same address | **wrong object, silent** |
| X23 | `XElement::Parse` | — | parent null, document null, round-trips | **safe** |
| X24 | `XContainer::Nodes()` across `RemoveNodes()` | — | snapshot keeps 4 live nodes | **safe** — returns by value |
| X25 | freed element, two retained attributes → `a->Remove()` | UAF read 8 | **SIGABRT** | **use-after-free on a mutating path** |
| X26 | freed element, retained child → `ReplaceWith({r1, r2})` | UAF read 8 (3) | **SIGABRT** | **use-after-free on a mutating path** |
| X27a/b | 1,000 / 5,000-deep element nest | — | built and released | safe |
| X27c | 20,000-deep element nest | **stack-overflow** | **SIGSEGV on release** | **recursive teardown** |
| X27d | 100,000-deep element nest | timed out | timed out | **quadratic ancestor guard** |
| X29a/b/c | `XDocument::Parse` of 1,000 / 5,000 / 20,000 nested elements | — | rejected: `XML_ELEMENT_DEPTH_EXCEEDED` | **safe — tinyxml2 already bounds it** |

### 5.3 Read versus write

Under `-fsanitize-recover=address` with `halt_on_error=0`, the whole matrix
produces **57 faulting accesses, every one a READ, and zero WRITEs**
(`build-probe/1885_prefix_asanr.log`). The mutating paths — `XNode::Remove`,
`XNode::ReplaceWith`, `XContainer::InsertNodeAt`, `XElement::RemoveAttribute` —
*enter* a non-`const` member function on freed storage, but the searches they
begin (`std::find_if` over the freed `children_`/`attributes_`) do not match, so
they return before reaching their `erase`/`insert`. **This design record
therefore claims a read-after-free on a mutating path, not heap corruption.**
Whether a write occurs depends on the bytes left in the freed block, which is not
something a design may assume in either direction.

---

## 6. Understated or incorrect audit premises

The historical audit text is **preserved unchanged**. The corrections below are
appended, exactly as #1882/#1883 appended theirs to SR-AUD-015. **No new
`SR-AUD-*` identifier is issued** — every correction was found while analysing an
existing finding, in the files that finding already owns — and numbering stays
frozen at **364**.

1. **The surface is 76 public entries across 27 headers and 13 bodies**, not the
   three-plus-six files and two accessors the cross-cutting record names (§3).
2. **SR-AUD-333's severity is understated.** `XObject::getParentProperty`
   performs a **virtual call** through the freed parent, so eight public entry
   points **abort the process** (`pure virtual method called` → SIGABRT) with no
   sanitizer present at all. The audit describes it as a heap-use-after-free at
   one accessor.
3. **The two families are not symmetric, and the cross-cutting record's "the
   implementations differ" understates how.** `XObject` already deletes copy,
   move, copy-assign and move-assign hierarchy-wide, with a comment explaining
   exactly why. `JsonNode` deletes none of them: `JsonArray copy = *orig;`
   compiles today, shares the children, and leaves them reporting the **original**
   as their parent (J08); and `jsonNodeRefA = jsonNodeRefB` silently rewrites
   `parent_` on a node that is still stored in a container (J09).
4. **Insertion semantics differ from .NET in opposite directions.** .NET's
   `JsonNode.AssignParent` *rejects* an already-parented node; the port matches.
   .NET's `XContainer.AddNode` **clones** it (`if (n.parent != null) n =
   n.CloneNode();`, `XContainer.cs:512`); the port **moves** it. The move is an
   already-documented, authorised deviation (`XContainer.hpp`'s class comment) —
   recorded here so no repair silently "fixes" it.
5. **A retained child of a destroyed parent is permanently unusable**, not merely
   unsafe to read (J06). `AssignParent` sees the stale non-null `parent_` and
   throws `The node already has a parent.` forever. The only escape is the
   public `DetachParent()` — an entry point the header itself calls "internal".
6. **`JsonObject::SetItem`'s exception path leaves the object inconsistent**
   (J10): the stored value is detached *before* `AssignParent` can throw, so the
   `JsonObject` still holds a node that reports no parent, and a **second**
   container then accepts it. `JsonArray::SetItem`, 40 lines away, orders the two
   calls the other way and is correct (J18). The audit names neither.
7. **`XNode::ReplaceWith` can destroy data on its exception path** (X20). It
   removes `this` and *then* inserts the replacements; when `InsertNodeAt`
   rejects a replacement, the original node is already gone. Measured: `<a/>`
   where `<a>victim</a>` was expected.
8. **Three stack-overflow shapes exist that no CCF-019 text mentions**, all
   reachable from public API and one from **untrusted input**: a 20,000-deep
   programmatic `JsonArray` nest crashes on release (J19c), the same for
   `XElement` (X27c), and `JsonNode::Parse` of 20,000 nested arrays crashes
   during parsing (X28c). `XDocument::Parse` is the counter-example *inside the
   family*: tinyxml2 already rejects deep nesting with
   `XML_ELEMENT_DEPTH_EXCEEDED` (X29a–c). The ancestor/cycle guards are
   additionally **quadratic** in depth (J19d, X27d both time out at 100,000).
9. **`getAttributesProperty()`'s reference is not invalidated by reallocation**
   (X16) — it refers to the vector *object*, which lives inside the `XElement`.
   It is invalidated only by the element's destruction (X17). A repair aimed at
   reallocation would fix nothing.

Additionally, two premises in the audit are **confirmed correct** and are
recorded as such: the ASan reproduction of both findings, and the cross-cutting
record's statement that the two members "have different public surfaces and
[each need] their own compatibility review before repair". The measurements
above are what that review found.

---

## 7. JsonNode: the current ownership model (measured)

### 7.1 Storage

```cpp
class JsonNode {
    JsonNode* parent_ = nullptr;   // non-owning, no liveness information
    JsonNodeOptions options_;
};
class JsonArray  : public JsonNode { std::vector<std::shared_ptr<JsonNode>> items_; };
class JsonObject : public JsonNode { std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>> properties_; };
class JsonValue  : public JsonNode { nlohmann::ordered_json value_; };
```

Measured sizes (x86-64, libstdc++ 14): `JsonNode` **24**, `JsonArray` **48**,
`JsonObject` **48**, `JsonValue` **40**.

### 7.2 Owner / pointee matrix

| Public entry | Owner | Pointee | Representation | Who destroys | Detached node usable? | Retained child usable after parent dies? |
|---|---|---|---|---|---|---|
| `JsonArray::operator[]` | array | child | `const shared_ptr&` | last `shared_ptr` | yes | **no — UAF on any parent walk** |
| `JsonObject::operator[]` | object | value | `shared_ptr` by value | last `shared_ptr` | yes | **no** |
| `JsonObject::TryGetPropertyValue` | object | value | `shared_ptr&` out | last `shared_ptr` | yes | **no** |
| `JsonNode::getParentProperty` | — | parent | **raw `JsonNode*`** | the parent's owner | n/a | **no — stale non-null** |
| `JsonNode::getRootProperty` | — | ancestor | **raw `JsonNode*`** | that ancestor's owner | n/a | **no** |
| `JsonArray::begin`/`end` | array | items | raw `vector` iterator | — | n/a | **no — invalidated by any `Add`** |
| `JsonObject::begin`/`end` | object | properties | raw `vector` iterator | — | n/a | **no** |
| `JsonNode::DeepClone` | caller | fresh subtree | `shared_ptr` | caller | yes | yes — fully independent |
| `JsonNode::Parse` | caller | fresh tree | `shared_ptr` | caller | yes | subject to the same rules |

### 7.3 Structural-event behaviour

| Event | Effect on the affected child |
|---|---|
| `Add` / `Insert` / `Add(name, value)` | `AssignParent`: **rejects** an already-parented node; **rejects** a cycle; otherwise sets `parent_` |
| `RemoveAt` / `Remove` / `Remove(name)` | `DetachParent`: `parent_ = nullptr`; the node survives if retained, and is re-attachable |
| `Clear` | detaches every child, then clears the store |
| `JsonArray::SetItem` | assign-then-detach — **exception-safe** |
| `JsonObject::SetItem` | detach-then-assign — **not** exception-safe (§6.6) |
| **Owner destruction** | **nothing at all** — this is the finding |

### 7.4 Value semantics (measured, and absent from every audit report)

`JsonNode` declares a virtual destructor and nothing else, so the copy
constructor and copy assignment operator are **implicitly generated and public**
on all four types (`static_assert(std::is_copy_constructible_v<JsonArray>)`
passes in the probe). Two consequences, both measured:

- `JsonArray copy = *orig;` produces a second array holding the **same child
  objects**, each of which still reports `orig` as its parent (J08).
- `JsonNode& a = *attached; a = *detached;` slices, copying `parent_`, so a node
  still stored in a container silently reports **no parent** (J09).

`XObject` deletes all four operations. This is the single largest structural
difference between the families.

---

## 8. XObject: the current ownership model (measured)

### 8.1 Storage

```cpp
class XObject {
protected:
    XContainer* parent_ = nullptr;      // non-owning, no liveness information
public:
    XObject(const XObject&) = delete; XObject& operator=(const XObject&) = delete;
    XObject(XObject&&) = delete;       XObject& operator=(XObject&&) = delete;
};
class XContainer : public XNode { std::vector<std::shared_ptr<XNode>> children_; };
class XElement   : public XContainer { XName name_; std::vector<std::shared_ptr<XAttribute>> attributes_; };
class XAttribute : public XObject { XName name_; std::string value_; XAttribute* next_; };
```

Measured sizes: `XObject` **16**, `XNode` **16**, `XContainer` **40**,
`XElement` **128**, `XAttribute` **120**, `XText` **48**, `XDocument` **56**.

### 8.2 What distinguishes it from JsonNode

| | JsonNode | XObject |
|---|---|---|
| Parent link type | `JsonNode*` (a node) | `XContainer*` (a container) |
| Parent dereference | reads a **field** | performs a **virtual call** (`getNodeTypeProperty`) |
| Copy / move | implicitly generated | `= delete` ×4 |
| Insertion of an attached node | **rejects** | **moves** (documented deviation) |
| Second borrowed link | none | **`XAttribute::next_`**, plus a public setter |
| Views returned by value | `Nodes()`, `Attributes()`, `Elements()`, `Descendants()` — **safe** | |
| Views returned by reference | — | `getAttributesProperty()` → `const vector&` — **unsafe past the element's life** |
| Views returned as raw pointers | — | `Extensions::Ancestors`/`AncestorsAndSelf` → `vector<XElement*>` |
| Deep-nesting parse | unbounded (crashes at 20,000) | **bounded by tinyxml2** |

### 8.3 Owner / pointee matrix

| Public entry | Owner | Pointee | Representation | Who destroys | Retained handle usable after owner dies? |
|---|---|---|---|---|---|
| `XContainer::Nodes` / `Elements` / `Descendants` | container | children | `vector<shared_ptr>` **by value** | last `shared_ptr` | node yes, **parent walk no** |
| `XContainer::getFirstNodeProperty` / `getLastNodeProperty` | container | child | `shared_ptr` by value | last `shared_ptr` | as above |
| `XElement::Attribute` / `Attributes` | element | attribute | `shared_ptr` by value | last `shared_ptr` | as above |
| `XElement::getAttributesProperty` | element | the vector itself | **`const vector&`** | the element | **no — UAF (X17)** |
| `XObject::getParentProperty` | — | parent | **raw `XElement*`** | the parent's owner | **no — virtual call through freed storage** |
| `XObject::getDocumentProperty` | — | root document | **raw `XDocument*`** | its owner | **no** |
| `XAttribute::getNextAttributeProperty` / `getPreviousAttributeProperty` | element | sibling attribute | **raw `XAttribute*`** | the element | **no (X13)** |
| `XNode::getNextNodeProperty` / `getPreviousNodeProperty` | parent | sibling | `shared_ptr` by value | last `shared_ptr` | **no — reads the parent's vector** |
| `Extensions::Ancestors` / `AncestorsAndSelf` | tree | ancestors | **`vector<XElement*>`** | their owners | **no (X15)** |
| `XDocument::getDeclarationProperty` | document | declaration | `shared_ptr` by value | last `shared_ptr` | **yes — `XDeclaration` is not an `XObject`** |
| `XStreamingElement` content | itself | `std::any` items | value | itself | **yes — not in the node hierarchy** |

### 8.4 Structural-event behaviour

| Event | Effect |
|---|---|
| `Add` / `AddFirst` / `InsertNodeAt` | validate → reject self/ancestor → **detach from the old parent** → adopt → insert |
| `RemoveNode` / `XNode::Remove` | `AdoptObject(child, nullptr)` then erase |
| `RemoveNodes` / `RemoveAll` | detaches every child, then clears |
| `XElement::Add(attribute)` | duplicate-name check → move from the old owner → adopt → push → `RelinkAttributes` |
| `RemoveAttribute` / `RemoveAttributes` | detach, null `next_`, erase, relink |
| `ReplaceWith` | **remove first, insert second** — loses the node when the insert throws (§6.7) |
| **Owner destruction** | **nothing at all** — this is the finding |

---

## 9. The actual .NET lifetime contracts

Read on 2026-07-30 from
`/rv/tmp/runtime/src/libraries/System.Text.Json/src/System/Text/Json/Nodes/` and
`/rv/tmp/runtime/src/libraries/System.Private.Xml.Linq/src/System/Xml/Linq/`.
Implementation, not documentation.

### 9.1 `JsonNode`

- `private JsonNode? _parent;` (`JsonNode.cs:22`) — a **GC reference**. A
  retained child therefore keeps its parent, and transitively every ancestor,
  **alive**. *There is no "destroyed parent" state in .NET at all.*
- `AssignParent` (`JsonNode.cs:346`) throws `NodeAlreadyHasParent` if
  `Parent != null`, then walks `parent.Parent` to reject a cycle. **The port
  matches exactly.**
- `DetachParent` (`JsonObject.cs:316`, `JsonArray.IList.cs:231`) sets
  `item.Parent = null` on removal and replacement. A removed node stays alive,
  keeps its value, and is re-attachable. **The port matches.**
- `JsonArray` indexer setter: `value?.AssignParent(this); DetachParent(List[index]);`
  (`JsonArray.cs:236-237`) — assign first. **`JsonArray::SetItem` matches;
  `JsonObject::SetItem` does not** (§6.6).
- `Options` (`JsonNode.cs:37-42`) is **inherited from the parent on first read
  and then cached**. The port copies options at construction and never inherits
  (J15). This is a real divergence, and — note — implementing it literally would
  add a *new* parent dereference to a property getter, i.e. a new instance of
  this very finding. It is therefore explicitly **excluded** from the lifetime
  repair (§30).

### 9.2 `XObject`

- `internal XContainer? parent;` (`XObject.cs:16`) — again a GC reference, with
  the same consequence.
- `Parent => parent as XElement` (`XObject.cs:73`). The port's
  `getParentProperty` matches semantically but reaches the answer through a
  **virtual call**, where .NET uses a type test.
- `XContainer.AddNode` (`XContainer.cs:509-520`): `if (n.parent != null) n = n.CloneNode();`
  — .NET **clones** an already-attached node. `XElement`'s attribute path is the
  same: `if (a.parent != null) a = new XAttribute(a);` (`XElement.cs:1902`). The
  port **moves** instead — an authorised, documented deviation (X21).
- `XNode.Remove` / `XAttribute.Remove` (`XNode.cs:521`, `XAttribute.cs:180`)
  throw `InvalidOperation_MissingParent` when `parent == null`. **The port
  matches.**
- `AppendAttribute` (`XElement.cs:1916`) throws `InvalidOperation_ExternalCode`
  for an already-parented attribute on the internal path.

### 9.3 The consequence for any C++ port

.NET's contract is *"a retained child roots its entire ancestor chain"*. In C++
that is a **strong** child→parent reference, and the parent already holds a
**strong** parent→child reference. That is a reference cycle. Measured
(`build-probe/1885_cycle_leak_probe.cpp`, `1885_cycle_leak.log`):

```
F  strong parent link : constructed=2 destroyed=0  -> LEAKED
A  weak parent link   : constructed=2 destroyed=2  -> no leak
ERROR: LeakSanitizer: detected memory leaks
SUMMARY: AddressSanitizer: 272 byte(s) leaked in 3 allocation(s).
```

**.NET's exact lifetime contract is unreachable with `shared_ptr` ownership.**
Every candidate below therefore diverges from .NET somewhere; the design's job is
to choose *where*, and to choose a place that no program with a .NET analogue can
observe.

---

## 10. Commonality and differences between the families

**Shared cause (one sentence, and it is the only thing they share):** a child
holds its parent as a raw non-owning pointer that no destruction path clears.

**Everything else differs**, and the differences change the repair:

| Dimension | JsonNode | XObject | Consequence for the repair |
|---|---|---|---|
| Value semantics | copy/move implicitly generated | all four `= delete` | JsonNode needs a **separate, source-breaking** decision (§25 #1888) |
| Parent dereference | field read | virtual call | XObject's failure is louder (SIGABRT) but the fix is identical |
| Insertion of an attached node | rejects | moves | must not be unified; both are deliberate |
| Second borrowed link | none | `XAttribute::next_` + public setter | XObject needs one extra detach step in `~XElement` |
| Borrowed views | 2 iterator pairs | 2 iterator-free but 3 borrowed-handle surfaces | different follow-up tickets |
| Parse depth | unbounded → crash | bounded by tinyxml2 | only the JSON side needs a bound |
| Number of container types | 2 (`JsonArray`, `JsonObject`) | 2 (`XElement`, `XDocument`) + attributes | comparable |

**They must not receive one shared abstraction.** The cross-cutting record
already said each "needs its own compatibility review"; the measurements confirm
that a shared base-class lifetime policy would have to encode both "reject" and
"move" insertion, both "copyable" and "non-copyable" value semantics, and both
"one child store" and "two child stores". The repair that *is* shared is a
**rule**, not a type: *the owner detaches what it owns, in its own destructor.*

---

## 11. Candidate designs

Every candidate was modelled in `build-probe/1885_layout_probe.cpp` and measured;
`sizeof` figures are x86-64 / libstdc++ 14, using a 24-byte base and a 48-byte
container that match the real `JsonNode`/`JsonArray` exactly.

| # | Candidate | base | container | Extra allocations | Per-access cost |
|---|---|---|---|---|---|
| 0 | **shipped today** — raw parent pointer | 24 | 48 | none | none |
| A | `weak_ptr` parent + `enable_shared_from_this` | **48** | **72** | none | `lock()` = atomic CAS loop |
| A′ | `weak_ptr` parent, no `enable_shared_from_this` | 32 | 56 | none | as A |
| B | raw parent + separate shared liveness token | 40 | 80 | **+1 control block per container** | 1 extra load |
| C | intrusive reference counting on every node | 32 | 56 | −1 control block per node | atomic inc/dec on every copy |
| D | shared implementation state behind a handle | handle 16 | state 40 | +1 per node, +1 per view | 1 extra indirection |
| E | tombstone link cell carrying the parent pointer | 32 | **72** | +1 per container (lazy) | 1 extra indirection |
| F | **strong** parent link (a GC reference) | 32 | 56 | none | none |
| G | whole-tree arena + aliasing `shared_ptr` handles | 48 | 24 | 1 per tree | none |
| **H** | **owner detaches its children in its own destructor** | **24** | **48** | **none** | **none** |

---

## 12. Rejected alternatives, with measured reasons

**A — `weak_ptr` parent link.** Rejected on three measured grounds.
(i) It requires `enable_shared_from_this` on the base, because
`JsonArray::Add`/`XContainer::InsertNodeAt` only have `this`; measured cost is
`sizeof(base)` **24 → 48**, a doubling.
(ii) A container that is **not** owned by a `shared_ptr` cannot produce a
`weak_ptr` to itself:

```
weak_from_this() on a stack ContainerA expired=TRUE (parent link would read as null)
```

The repository has **77 automatic-storage node instances in its own tests**
(12 `JsonArray`, 15 `JsonObject`, 17 `XElement`, 11 `XDocument`, and 22 further
`XText`/`XComment`/`XAttribute` sites). Under A every one of them would compile
and then silently report **no parent** for children that are, in fact, attached —
a *new* silent wrong answer traded for the old one. The only escape is to forbid
automatic storage, which is a public source break far larger than H's.
(iii) `weak_ptr::lock()` on every parent read is an atomic compare-and-swap loop,
on accessors (`getParentProperty`, `getRootProperty`) that today are a load.

**A′ — `weak_ptr` without `enable_shared_from_this`.** Rejected as
**not implementable**, not merely costly. The container would have to supply a
`weak_ptr` to *itself* at attach time; inside `Add(node)` there is no such thing.
It is recorded because its `sizeof` looks attractive (32/56) and a future reader
would otherwise re-derive it.

**B — separate liveness token.** Works for automatic storage (unlike A) but costs
`sizeof(base)` 24 → 40 **and** `sizeof(container)` 48 → 80 **and** an extra
control-block allocation per container, for exactly the information E encodes in
one 8-byte cell and H encodes in nothing at all. Strictly dominated by E, which
is itself dominated by H.

**C — intrusive reference counting.** Rejected. It does not address the finding:
a refcounted node whose parent is refcounted still has a raw parent pointer, and
the cycle problem of F returns the moment the parent link becomes owning. It also
changes every public `shared_ptr<JsonNode>` / `shared_ptr<XNode>` return type in
both families — 30+ signatures — and would make `std::shared_ptr` and the
intrusive count two independent owners of one object.

**D — shared implementation state behind a handle.** Rejected. It relocates the
problem rather than solving it: the state graph has exactly the same parent/child
edges, so a strong parent edge still cycles and a weak one still needs a live
`shared_ptr` owner for the parent state. It additionally turns every public node
type into a handle (`sizeof` 16) whose `->` is an extra indirection, breaks all 77
automatic-storage sites, and changes every constructor. It buys nothing that H
does not already buy for free.

**E — tombstone link cell.** The best of the *representation-changing*
candidates, and the reason it is documented rather than dismissed: measured, it
gives the correct answer for automatic-storage containers, which A cannot:

```
E  tombstone link on an AUTOMATIC-storage container:
  inside scope : parent = the container (correct)
  after scope  : parent = null (DEFINED, no dereference of freed storage)
```

Rejected only because H produces **the identical observable contract** at
`sizeof(base)` 24 (vs 32), `sizeof(container)` 48 (vs 72), and zero extra
allocations (vs one per container). E remains the fallback if a future
requirement needs a child to distinguish *"detached"* from *"my parent died"* —
H deliberately collapses those two states into one (§30.3).

**F — strong parent link.** This is the only candidate that reproduces .NET's
contract exactly. Rejected on measured evidence: it **leaks by construction**.
`constructed=2 destroyed=0`, `272 byte(s) leaked in 3 allocation(s)`,
LeakSanitizer-confirmed after clobbering the stack so conservative scanning
cannot mask it (`build-probe/1885_cycle_leak.log`). A leak on *every* attached
node in *every* tree is not a trade; it is a different, worse defect.

**G — whole-tree arena with aliasing `shared_ptr` handles.** The only *other*
candidate that could reproduce .NET's contract without a cycle: one `TreeState`
owns every node, and each public handle is
`std::shared_ptr<Node>(treeState, rawNode)`, so retaining any handle keeps the
whole tree alive. Rejected on three grounds, each decisive on its own.
(i) It **over-retains** relative to .NET: in .NET a removed node's `parent` is
nulled and it stops rooting its former tree; with an arena, a retained removed
node would keep the entire former tree alive unless removal physically migrates
the subtree to a new arena — which is a deep copy or a re-parenting walk on every
`Remove`.
(ii) It makes every current construction spelling illegal: `make_shared<JsonArray>()`,
`JsonValue::Create(...)`, `std::make_shared<XElement>(...)`, and all 77
automatic-storage sites.
(iii) It is the largest change on the table — construction, insertion, removal,
reparenting, cloning and parsing all change — for a contract difference that, by
§13.4, no program with a .NET analogue can observe.

---

## 13. Selected design — JsonNode (**proposed**)

### 13.1 The rule

> **A container detaches every child it owns, in its own destructor, before that
> child storage is released.**

Concretely, and this is the entire core repair:

```cpp
class JsonArray : public JsonNode {
    std::vector<std::shared_ptr<JsonNode>> items_;
public:
    ~JsonArray() override { for (auto& i : items_) if (i) i->DetachParent(); }
    // ... unchanged ...
};

class JsonObject : public JsonNode {
    std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>> properties_;
public:
    ~JsonObject() override { for (auto& [n, v] : properties_) if (v) v->DetachParent(); }
    // ... unchanged ...
};
```

`DetachParent()` already exists, is already public, and is already what `Clear()`
calls for every child. The destructor body runs **before** `items_` /
`properties_` is destroyed, so every child object is still fully alive at that
point.

### 13.2 Why it is complete for the whole ancestor chain

Recursion is **structural, not written**. `~JsonObject` detaches `mid`; releasing
`properties_` drops the last `shared_ptr` to `mid`, which runs `~JsonArray`,
which detaches `leaf`. Measured, on the model, proof (3) in
`build-probe/1885_design_asan.log`:

```
(3) externally retained GRANDCHILD, whole tree destroyed
      leaf->parent_ = null (DEFINED) ; leaf->root() == leaf : yes
```

### 13.3 The resulting contract

| State | `getParentProperty()` | `getRootProperty()` | re-attachable |
|---|---|---|---|
| never attached | `nullptr` | `this` | yes |
| removed / `Clear`ed | `nullptr` | `this` | yes |
| **owner destroyed** (new) | **`nullptr`** | **`this`** | **yes** |
| attached | the owner | the tree root | no — `AssignParent` throws, matching .NET |

Three previously distinct outcomes — `nullptr`, undefined behaviour, and
"permanently unusable" — collapse into one defined state. This is exactly the
`LinkedListNode<T>` contract ticket #1769 shipped under this same cross-cutting
cause (`docs/LinkedListNodeLifetime.md` §4.3: *"Destruction of the owning
`LinkedList<T>` — the destructor performs the same detaching walk as `Clear()`"*).

### 13.4 How it diverges from .NET, and why that is the right divergence

In .NET the parent cannot be destroyed while a child references it, so the state
this design defines **cannot arise in any program that has a .NET analogue**. The
divergence is therefore unobservable except in code that has no .NET counterpart —
and in that code the alternative today is undefined behaviour. Every other
candidate diverges somewhere strictly worse: A and D make *attached* children
report no parent (a wrong answer in a correct program), F leaks, G over-retains.

### 13.5 Accompanying changes (each its own ticket)

| id | Change | Fixes | Breaks source? |
|---|---|---|---|
| J-1 | the destructors above | J01–J06, J08 (partly), J16, J17 | no |
| J-2 | `JsonObject::SetItem` → assign-before-detach, matching `JsonArray::SetItem` and .NET | J10 | no |
| J-3 | `= delete` copy/move on `JsonNode` (inherited by all four types) | J08, J09 | **yes** |
| J-4 | make `DetachParent()` non-public (`protected` + friend the two containers) | J13 | **yes** |
| J-5 | version-guarded enumerators for `JsonArray`/`JsonObject` | J11, J12 | no (new throw) |
| J-6 | bound `JsonNode::Parse` nesting depth; iterative teardown; linear cycle check | X28c, J19c, J19d | **yes** (a new rejection) |

---

## 14. Selected design — XObject (**proposed**)

### 14.1 The rule, and the one extra store

The same rule, applied to **both** of `XElement`'s stores:

```cpp
class XContainer : public XNode {
protected:
    std::vector<std::shared_ptr<XNode>> children_;
    ~XContainer() override { for (auto& c : children_) if (c) AdoptObject(*c, nullptr); }
};

class XElement : public XContainer {
    std::vector<std::shared_ptr<XAttribute>> attributes_;
public:
    ~XElement() override {
        for (auto& a : attributes_) if (a) { AdoptObject(*a, nullptr); a->setNextAttributeProperty(nullptr); }
    }
};
```

`AdoptObject(obj, nullptr)` already exists as the protected detach primitive and
is already what `RemoveNodes()` / `RemoveAttributes()` call. The attribute loop
must also clear `next_`, because that is a **second** borrowed link and X13 shows
it dangles independently of `parent_`.

`~XElement`'s body runs before `attributes_` is destroyed; `~XContainer`'s runs
before `children_` is destroyed. Destruction order therefore already gives both
loops live objects to work on.

### 14.2 The resulting contract

| State | `getParentProperty()` | `getDocumentProperty()` | `getNextNodeProperty()` | `Remove()` |
|---|---|---|---|---|
| never attached | `nullptr` | `nullptr` | `nullptr` | throws `The parent is missing.` |
| removed | `nullptr` | `nullptr` | `nullptr` | throws |
| **owner destroyed** (new) | **`nullptr`** | **`nullptr`** | **`nullptr`** | **throws** |
| attached | the owner | the document | the sibling | removes |

Every one of X01–X14, X18, X19, X22, X25 and X26 becomes one of the first three
rows — a defined answer the type already produces for a detached node.

### 14.3 Accompanying changes (each its own ticket)

| id | Change | Fixes | Breaks source? |
|---|---|---|---|
| X-1 | the destructors above | X01–X14, X18, X19, X22, X25, X26 | no |
| X-2 | `XNode::ReplaceWith` validates the whole replacement list **before** removing `this` | X20 | no |
| X-3 | `getAttributesProperty()` returns by value (or is removed in favour of `Attributes()`) | X17 | **yes** |
| X-4 | `Extensions::Ancestors`/`AncestorsAndSelf` return owning handles or are documented as borrowed | X15 | **yes** if changed |
| X-5 | `XAttribute::setNextAttributeProperty` becomes non-public | the public door onto `next_` | **yes** |
| X-6 | bound `XElement`/`XDocument` teardown depth | X27c, X27d | no |

### 14.4 What is deliberately **not** changed

- **`Add` keeps moving, not cloning.** .NET clones (§9.2); the port's move is an
  authorised documented deviation and X21 confirms it still behaves as documented.
  Changing it is a separate decision with its own approval.
- **`getParentProperty()` keeps returning a raw `XElement*`.** Returning
  `shared_ptr<XElement>` would require every parent to be `shared_ptr`-owned, i.e.
  candidate A's rejection (§12).
- **`Changed`/`Changing` stay inert.** SR-AUD-336 is a separate `confirmed`
  finding; §16.7 records only that they add no ownership edge.

---

## 15. Ownership-edge graph

### 15.1 Today (shipped)

```
JsonArray  --strong(shared_ptr)-->  JsonNode child
JsonObject --strong(shared_ptr)-->  JsonNode value
JsonNode   --RAW, UNGUARDED------->  JsonNode parent          <-- the defect
JsonArray::iterator  --RAW--------->  items_ buffer            <-- J11/J12
JsonNode (copy)      --ALIAS------->  the same children        <-- J08

XContainer --strong(shared_ptr)-->  XNode child
XElement   --strong(shared_ptr)-->  XAttribute
XObject    --RAW, UNGUARDED------->  XContainer parent        <-- the defect
XAttribute --RAW, UNGUARDED------->  XAttribute next_         <-- X13
XElement   --REFERENCE------------>  attributes_ (escapes)    <-- X17
Extensions --RAW vector<XElement*>->  ancestors               <-- X15
XDocument  --strong(shared_ptr)-->  XDeclaration              (safe, not an XObject)
```

### 15.2 After the selected design

```
JsonArray  --strong--> child        (unchanged)
JsonObject --strong--> value        (unchanged)
JsonNode   --OBSERVER, CLEARED BY THE OWNER'S DESTRUCTOR--> parent

XContainer --strong--> child        (unchanged)
XElement   --strong--> attribute    (unchanged)
XObject    --OBSERVER, CLEARED BY THE OWNER'S DESTRUCTOR--> parent
XAttribute --OBSERVER, CLEARED BY THE OWNER'S DESTRUCTOR--> next_
```

Every edge is classified:

| Edge | Kind | Cleared by |
|---|---|---|
| container → child | **strong** | the container's own destruction, or `Remove`/`Clear` |
| element → attribute | **strong** | as above |
| child → parent | **observer** | `Remove`, `Clear`, `RemoveNodes`, `RemoveAll`, **and the owner's destructor** |
| attribute → next attribute | **observer** | `RemoveAttribute(s)`, **and the owner's destructor** |
| iterator → container storage | **temporary**, invalidated by any mutation | ticket #1889 |
| `Extensions::Ancestors` result → element | **observer, unmanaged** | nothing — ticket #1892 |
| `getAttributesProperty()` reference → element member | **observer, unmanaged** | nothing — ticket #1892 |
| document → declaration | **strong** | the document |
| event source → handler | **none** — the accessors discard every handler | n/a |

---

## 16. Cycle analysis

### 16.1 Is there a cycle today?

**No.** Parent→child is the only owning edge; child→parent is raw. Measured
indirectly: the whole probe runs under LeakSanitizer with `detect_leaks=1` and
reports **zero leaks** across 47 cases.

### 16.2 Does the selected design introduce one?

**No.** It adds no edge at all. It only *clears* an existing non-owning edge
earlier. This is the decisive advantage over A/B/C/D/E/F/G, every one of which
adds at least one new edge.

### 16.3 Every cycle the prompt asks about

| Potential cycle | Today | After H |
|---|---|---|
| child → parent → child | broken (child→parent is raw) | broken (unchanged) |
| parent → child → parent | broken | broken |
| attribute → element → attribute | broken | broken |
| node → document → node | broken | broken |
| iterator → container → iterator | iterators are not owning | unchanged |
| event source → handler → source | no handler is stored | unchanged |
| wrapper → implementation → wrapper | no separate implementation object exists | unchanged |
| **node inserted into its own subtree** | rejected by `AssignParent`'s ancestor walk (JsonNode) and `InsertNodeAt`'s ancestor walk (XContainer) | unchanged, and now walks a chain that is guaranteed non-stale |

The last row is the one place a genuine `shared_ptr` cycle was once reachable;
`XContainer.cpp`'s own comment records that it was closed by the ancestor guard.
H strictly improves it: the guard currently walks *possibly freed* storage (J17,
an ASan-confirmed UAF at `JsonNode.cpp:22`), and after H the chain it walks is
always live.

### 16.4 The rejected candidates' cycles

F closes child→parent→child with two strong edges and leaks (measured, §9.3).
C reintroduces the same cycle if the parent link is made owning. G has no cycle
but retains the whole former tree behind any removed node.

### 16.5–16.7 Notes

- **16.5** `XDocument → XDeclaration` is strong and one-way; `XDeclaration` is not
  an `XObject` and holds nothing back.
- **16.6** `XStreamingElement` holds `std::vector<std::any>` which may contain
  `shared_ptr<XNode>` and `shared_ptr<XStreamingElement>`. A nested streaming
  element added to itself would cycle; it is not an `XObject`, it has no parent
  link, and it is outside CCF-019's cause. **Recorded, not repaired.**
- **16.7** `add_Changed`/`add_Changing` accept a handler and discard it, so the
  event surface contributes **no** ownership edge and cannot keep a node alive.
  SR-AUD-336 stays `confirmed`; if it is ever implemented, the handler list
  becomes a new owning edge and this section must be revisited.

---

## 17. Destruction proofs

Measured on the model in `build-probe/1885_design_probe.cpp`, built with
`-fsanitize=address,undefined` and LeakSanitizer; full output in
`build-probe/1885_design_asan.log`. The model reproduces the real storage shape
(children by `shared_ptr`, parent raw, `assignParent` rejecting an existing
parent and a cycle).

| # | Sequence | Result |
|---|---|---|
| 1 | ordinary whole-tree destruction | every node freed, **zero diagnostics, zero leaks** |
| 2 | externally retained child, parent destroyed | `parent_ == nullptr`; `root()` returns the child itself |
| 3 | externally retained **grandchild**, whole tree destroyed | `parent_ == nullptr` — the detach cascades structurally |
| 4 | detached child re-attached elsewhere | **succeeds** (today it throws `The node already has a parent.` forever) |
| 5 | reparented child (`Clear` then `Add` elsewhere) | `parent_ == the new container` |
| 6 | exception halfway through insertion | child keeps its **original** parent; the target container is unchanged (`size == 0`) |
| 7 | exception during replacement (assign-before-detach) | the stored value keeps the correct parent and is still stored |
| 8 | deep-chain teardown | **recursive through `shared_ptr`** — see below |

**Proof 8 is a negative result and is recorded as one.** Teardown of a deep chain
recurses one `shared_ptr` destructor frame per level, in the shipped code and
under H alike. Measured on the **production** types:

| depth | `JsonArray` | `XElement` | `JsonNode::Parse` | `XDocument::Parse` |
|---|---|---|---|---|
| 1,000 | ok | ok | ok | rejected by tinyxml2 |
| 5,000 | ok | ok | ok | rejected |
| 20,000 | **SIGSEGV on release** / ASan `stack-overflow` | **SIGSEGV on release** | **SIGSEGV during parse** | rejected |
| 100,000 | timed out (>5 s, quadratic guard) | timed out | — | rejected |

This is **pre-existing and unchanged by the selected design** — H adds no
recursion — but it is reachable from public API and, for `JsonNode::Parse`, from
**untrusted text**. It becomes ticket #1893.

**Clone failure** (the prompt's seventh sequence) needs no separate proof:
`DeepClone` builds a wholly independent subtree with `make_shared` and attaches
nothing to the source, so a throw part-way through releases only the partial
clone. **Event callback during mutation** (the eighth) is not reachable: no
handler is ever stored (§16.7).

---

## 18. Iterator / enumerator consequences

The selected design **changes nothing** about iterators, and this section exists
to say so explicitly rather than let it be inferred.

| Surface | Today | After H | Ticket |
|---|---|---|---|
| `JsonArray::begin`/`end` | raw `vector` iterators, no version guard; J11 is an ASan-confirmed UAF after a reallocating `Add` | unchanged | **#1889** |
| `JsonObject::begin`/`end` | same; J12 reads destroyed-but-not-freed storage after `Clear()` with **no** diagnostic | unchanged | **#1889** |
| `XContainer::Nodes`/`Elements`/`Descendants`/`DescendantNodes` | return `std::vector<shared_ptr<...>>` **by value** — a true snapshot (X24) | unchanged | — |
| `XElement::Attributes()` | returns by value — safe | unchanged | — |
| `XElement::getAttributesProperty()` | returns `const vector&`; survives reallocation (X16) but **not** the element (X17) | unchanged | **#1892** |
| `Extensions::*` | materialise `std::vector` results; `Ancestors`/`AncestorsAndSelf` hold **raw** `XElement*` (X15) | unchanged | **#1892** |
| `XAttribute::getNextAttributeProperty` | raw `XAttribute*` into the owner's vector (X13) | **fixed** — `~XElement` clears `next_` | #1890 |

The repository already has the machinery #1889 would use:
`System::Collections::detail::MutationCounter` / `MutationVersion`, the
`SHARP_RUNTIME_COLLECTION_VERSION_SEAM` test seam, and
`scripts/check_version_seam_odr.py`. Note that adopting it in `Text.Json` would
add a new public component edge `Text.Json → Collections.Core` (today
`Collections.Core` is only a **private** and test dependency of `Text.Json`), so
#1889 must regenerate the component catalogue. `Xml.Linq` does not need it.

---

## 19. Source compatibility

### 19.1 The core repair (J-1, X-1, J-2, X-2): **no source change for any consumer**

Nothing is added to, removed from, or renamed in any public declaration. No
signature, no default argument, no `noexcept`, no template parameter, no
namespace. Consumer code compiles unchanged.

One nuance, recorded rather than glossed: declaring `~JsonArray()` /
`~JsonObject()` **suppresses their implicitly generated move constructor and move
assignment operator**. Because their copy operations remain implicitly generated
(until J-3 removes them), `JsonArray a = std::move(b);` keeps compiling but
becomes a **copy** instead of a move. `XObject` is unaffected — it already
deletes all four. If J-3 lands in the same change, the point is moot, which is an
argument for ordering them together (§24).

### 19.2 The behaviour change the core repair does make

`child->getParentProperty()` after the parent's destruction returns **`nullptr`**
where it previously returned a dangling non-null pointer. Any consumer that
relied on the old value relied on undefined behaviour. There is no compile-time
signal for this; it is the change the approval request names first (§31).

### 19.3 The source-breaking follow-ups (each separately approvable)

| Change | Breaks | Diagnostic the consumer gets |
|---|---|---|
| J-3 `= delete` copy/move on `JsonNode` | `JsonArray b = a;`, `*n1 = *n2;` | `use of deleted function` — names `DeepClone()` in the message |
| J-4 `DetachParent()` non-public | any consumer calling it | `is protected within this context` |
| X-3 `getAttributesProperty()` returns by value | code binding it to `const auto&` still compiles (lifetime-extended); code taking its address changes meaning | none for the common spelling — **this is why X-3 needs a negative fixture** |
| X-4 `Extensions::Ancestors` return type | `std::vector<XElement*>` → `std::vector<std::shared_ptr<XElement>>` | `no viable conversion` at every call site |
| X-5 `setNextAttributeProperty` non-public | any consumer calling it | `is private within this context` |
| J-6 parse depth bound | JSON nested deeper than the bound | a new `JsonException` at run time |

---

## 20. ABI, layout, vtable and mangling consequences

**Measured** with the layout probe and by inspection of the declarations.

| Consequence | Core repair (J-1/X-1/J-2/X-2) | J-3/J-4/X-3/X-4/X-5 |
|---|---|---|
| `sizeof` of any public type | **unchanged** — 24/48/48/40 and 16/16/40/128/120/48/56 | unchanged |
| Data member added, removed or reordered | **none** | none |
| Alignment | unchanged | unchanged |
| Virtual functions added or removed | **none** — `~JsonArray`/`~JsonObject` are already virtual via `~JsonNode`, and `~XContainer`/`~XElement` via `~XObject` | none |
| Vtable slot count or order | **unchanged** — an explicitly declared destructor occupies the slots the implicit one already occupied | unchanged |
| Mangled names | the destructor symbols (`D0`/`D1`/`D2`) are already emitted; explicit definition changes their **linkage location**, not their names | J-3/J-4/X-5 **remove** symbols; X-4 **changes** a template's mangled return type |
| Recompilation required | **yes** — the destructor bodies are inline in headers, so a consumer must recompile, exactly as #1867/#1868/#1870/#1883 required | yes |
| Link-compatible with an old object file | **no** (inline change) | no |

The core repair is therefore a **recompile-compatible, source-compatible,
layout-identical** change. That is the strongest property any candidate on the
table has, and it is unique to H: A costs +24 bytes on every node, B +16/+32, E
+8/+24.

---

## 21. Allocation and performance consequences

| Metric | Core repair | Measured or estimated |
|---|---|---|
| Heap allocations per node | **unchanged** (0 added) | measured — H adds no member and no `make_shared` |
| Heap allocations per container | **unchanged** (0 added) | measured; A adds 0, B adds 1, E adds 1, D adds 1+ |
| Bytes per node | **unchanged** | measured (§20) |
| Cost of `getParentProperty()` | **unchanged** — one load | measured; A would make it an atomic CAS loop |
| Cost of `getRootProperty()` | **unchanged** — a pointer chase per level | measured |
| Cost of container destruction | **+1 pass over the child vector**, one store per child | estimated: O(children), the same walk `Clear()` already performs, with no allocation and no branch beyond the existing null check |
| Cost of `Add`/`Remove`/`Clear` | **unchanged** | measured |
| Deep-tree teardown | **unchanged** and still recursive; crashes at 20,000 (§17) | measured |
| `AssignParent`/`InsertNodeAt` guard | **unchanged** and still O(depth) per insertion, i.e. O(n²) to build a chain; times out at 100,000 | measured |

No benchmark is proposed for the core repair, because there is no plausible
mechanism for a regression: the added work is one already-existing loop that runs
once per container destruction. Ticket #1894 pins that with a sanitizer run
rather than a timing run.

---

## 22. Exception-safety requirements

| Requirement | Why | Ticket |
|---|---|---|
| Every destructor added must be `noexcept` in effect — it may call only `DetachParent()`/`AdoptObject()`, which are non-throwing pointer stores | a throwing destructor during stack unwinding calls `std::terminate` | #1886, #1890 |
| The detach loop must tolerate null slots | `JsonArray` and `JsonObject` legitimately store null children (JSON `null`) | #1886 |
| `JsonObject::SetItem` must assign before it detaches | measured: the current order leaves the object holding a parentless node (J10) and a second container then accepts it | #1887 |
| `XNode::ReplaceWith` must validate every replacement before removing `this` | measured: the current order loses the node when the insert throws (X20) | #1891 |
| `XContainer::InsertNodeAt` must leave the node attached to exactly one container on any throw path | today `AdoptObject(*n, this)` runs before `children_.insert`, so a throwing insert leaves `n` claiming a parent that does not contain it | #1891 |
| Strong guarantee where .NET has one; basic guarantee elsewhere, documented | .NET's own `JsonObject` indexer is not strongly exception-safe either (`JsonObject.cs:309-313`); the port should not silently claim more | #1887, #1891 |

---

## 23. Thread-safety implications

Neither family is thread-safe today and the selected design does not change that.
Recorded explicitly rather than skipped:

- No node in either family contains an atomic, a mutex, or a version counter.
- The `shared_ptr` **control blocks** are atomic, so concurrent *copies* of
  handles are safe; the **objects** are not.
- H introduces no shared mutable state, so it adds no data race and removes none.
- A and E *would* introduce cross-thread cost: `weak_ptr::lock()` on every parent
  read (A) and a shared cell written by the container's destructor while other
  threads may read it (E) — E's cell would need to become atomic, which its
  measured cost above does not include.
- **TSan is not applicable** to the core repair and is recorded as such rather
  than skipped (§27).

---

## 24. Implementation dependency order

```
#1885 (this design, done)
  │
  ├─ #1886  JsonNode: containers detach children on destruction        [core]
  │     ├─ #1887  JsonNode: mutation and exception-path consistency
  │     ├─ #1888  JsonNode: value semantics + DetachParent visibility  [source break]
  │     └─ #1889  JsonNode: enumerator lifetime
  │
  ├─ #1890  XObject: containers detach children and attributes         [core]
  │     ├─ #1891  XNode/XContainer: mutation and reparenting exception paths
  │     └─ #1892  Xml.Linq: borrowed-view surface                      [source break]
  │
  ├─ #1893  Both families: deep-tree and deep-parse bounds             [input change]
  │
  └─ #1894  Negative consumer fixtures + permanent sanitizer closure   [after all]
```

**#1886 and #1890 are independent of each other** and can land in either order or
in parallel. #1888 should land **with or immediately after** #1886 for the
move-suppression reason in §19.1.

---

## 25. Bounded implementation ticket breakdown

Every ticket below is created **blocked** or **needs_user**. None may start
without the approval in §31.

### #1886 — JsonNode: containers detach their children on destruction (core)

- **Depends on**: #1885.
- **Files**: `modules/text-json/include/System/Text/Json/Nodes/JsonArray.hpp`,
  `.../JsonObject.hpp`.
- **Public types**: `JsonArray`, `JsonObject`.
- **Representation**: unchanged. Adds one user-declared virtual destructor to each.
- **Source / ABI / layout effect**: no signature change, no member change,
  `sizeof` unchanged at 48/48, no vtable slot change; consumers must recompile
  (inline header change); implicit **move** operations become suppressed (§19.1).
- **Acceptance criteria**: J01–J06, J16 and J17 return defined values with no
  ASan diagnostic; `getParentProperty()` is `nullptr` for a child whose owner has
  been destroyed, at any depth; every existing `JsonNodeTests` case passes
  **unmodified**; a mutation that deletes either destructor body fails ≥ 4 new
  permanent tests.
- **Tests**: `JsonNodeLifetimeTests` — retained child, retained value, retained
  grandchild, retained great-grandchild, null child slot, retained child of a
  cleared array, re-attachment after owner destruction, `ToJsonString` on an
  orphan.
- **Sanitizers**: ASan + UBSan + LSan over the whole J-matrix, each case in its
  own process.
- **Rollback boundary**: revert two destructor bodies; nothing else changes.
- **Approval required**: §31 item 1.

### #1887 — JsonNode: mutation and exception-path consistency

- **Depends on**: #1886.
- **Files**: `.../JsonObject.hpp`.
- **Change**: `SetItem` assigns the new parent before detaching the old value.
- **Effect**: no signature, ABI or layout change. One observable change on a
  currently-throwing path.
- **Acceptance**: J10 leaves the object holding a value whose parent is the
  object; a second container then **rejects** it; J18's `JsonArray` behaviour is
  unchanged.
- **Rollback**: revert one method body.
- **Approval required**: §31 item 2.

### #1888 — JsonNode: value semantics and `DetachParent` visibility  *(source break)*

- **Depends on**: #1886.
- **Files**: `.../JsonNode.hpp` (+ the three derived headers if they need
  explicit re-declaration).
- **Change**: `= delete` the copy constructor, copy assignment, move constructor
  and move assignment on `JsonNode`; move `DetachParent()` to `protected` with
  `friend class JsonArray; friend class JsonObject;`.
- **Effect**: **public source break.** `JsonArray b = a;`, `*n1 = *n2;` and any
  external `DetachParent()` call stop compiling. No layout or vtable change.
- **Acceptance**: J08, J09 and J13 are compile errors; a negative consumer fixture
  proves each; `DeepClone()` is the documented replacement.
- **Approval required**: §31 item 3 — **this one is a genuine break and must be
  approved separately.**

### #1889 — JsonNode: enumerator lifetime

- **Depends on**: #1886.
- **Files**: `.../JsonArray.hpp`, `.../JsonObject.hpp`,
  `modules/text-json/CMakeLists.txt`, `docs/generated/ComponentCatalog.md`,
  `modules/collections/tests/support/CollectionVersionSeam.hpp`.
- **Change**: give both containers a `detail::MutationCounter`, give the
  enumerators a `detail::MutationVersion` snapshot, and throw
  `InvalidOperationException` on a stale dereference.
- **Effect**: **`sizeof(JsonArray)`/`sizeof(JsonObject)` grow by 8** (layout
  change); adds a new **public** component edge `Text.Json → Collections.Core`,
  taking the module graph from 91 to **92** edges and requiring the generated
  catalogue to be regenerated.
- **Acceptance**: J11 and J12 throw instead of reading freed or destroyed storage;
  `scripts/check_version_seam_odr.py` and its unit test pass with the new seam;
  a `test/consumer/*_negative.cpp` site pins the private counter.
- **Approval required**: §31 item 4 — object-layout change.

### #1890 — XObject: containers detach children and attributes (core)

- **Depends on**: #1885.
- **Files**: `modules/xml-linq/include/System/Xml/Linq/XContainer.hpp`,
  `.../XElement.hpp` (or their bodies).
- **Representation**: unchanged. Adds one user-declared virtual destructor to
  `XContainer` and one to `XElement`; the latter also clears each attribute's
  `next_`.
- **Effect**: no signature, member, `sizeof` (40/128) or vtable change; consumers
  recompile.
- **Acceptance**: X01–X14, X18, X19, X22, X25 and X26 return the same defined
  values a **detached** node returns today, with no ASan diagnostic and no
  `pure virtual method called`; all 92 existing `Xml.Linq` tests pass unmodified;
  a mutation deleting either loop fails ≥ 6 new permanent tests.
- **Tests**: `XLinqLifetimeTests` — retained text node, retained comment, retained
  attribute, retained grandchild, retained root of a destroyed document, sibling
  navigation on an orphan, `Remove`/`ReplaceWith` on an orphan, re-`Add` of an
  orphan to a live element, `getNextAttributeProperty` after the owner dies.
- **Rollback**: revert two destructor bodies.
- **Approval required**: §31 item 1.

### #1891 — XNode/XContainer: mutation and reparenting exception paths

- **Depends on**: #1890.
- **Files**: `modules/xml-linq/src/System/Xml/Linq/XNode.cpp`, `.../XContainer.cpp`.
- **Change**: `ReplaceWith` validates every replacement before removing `this`;
  `InsertNodeAt` adopts only after the insert cannot throw.
- **Effect**: no signature or layout change; X20's outcome changes from
  *"throws and the node is gone"* to *"throws and nothing moved"*.
- **Acceptance**: X20 leaves `<a>victim</a>` intact; `Add`'s documented move
  behaviour (X21) is unchanged.
- **Approval required**: §31 item 2.

### #1892 — Xml.Linq: the borrowed-view surface  *(source break)*

- **Depends on**: #1890.
- **Files**: `.../XElement.hpp`, `.../XAttribute.hpp`, `.../Extensions.hpp`.
- **Change**: `getAttributesProperty()` returns by value; `Ancestors`/
  `AncestorsAndSelf` return owning handles; `setNextAttributeProperty` becomes
  non-public.
- **Effect**: **public source break** at every `Ancestors` call site and every
  `setNextAttributeProperty` call site; a silent-but-safe change at
  `getAttributesProperty()` binding sites; a per-call `std::vector` copy where
  there was a reference.
- **Acceptance**: X15 and X17 are compile errors or return owning handles;
  negative consumer fixtures for all three.
- **Approval required**: §31 item 5.

### #1893 — Both families: deep-tree and deep-parse bounds  *(accepted-input change)*

- **Depends on**: #1886, #1890.
- **Files**: `modules/text-json/src/.../JsonNode.cpp`, `.../JsonArray.hpp`,
  `.../JsonObject.hpp`, `modules/xml-linq/src/.../XContainer.cpp`.
- **Change**: bound `JsonNode::Parse` nesting depth and raise `JsonException`
  beyond it (mirroring tinyxml2's `XML_ELEMENT_DEPTH_EXCEEDED`, which the XML
  side already has); make container teardown iterative; make the ancestor/cycle
  guard O(1) amortised or bound it.
- **Effect**: input previously accepted (deeply nested JSON) starts being
  rejected. No layout or signature change.
- **Acceptance**: X28c, J19c, J19d, X27c and X27d all terminate with a defined
  outcome; the chosen depth limit is documented and justified against .NET's own
  `JsonReaderOptions.MaxDepth` (default 64).
- **Approval required**: §31 item 6.

### #1894 — Negative consumer fixtures and permanent sanitizer closure

- **Depends on**: #1886, #1887, #1888, #1890, #1891, #1892.
- **Files**: `test/consumer/text_json_node_lifetime_negative.cpp`,
  `test/consumer/xml_linq_object_lifetime_negative.cpp`, plus
  `docs/NegativeConsumerFixtureValidation.md`.
- **Change**: one negative site per outlawed spelling, following the mandatory
  `// NEGATIVE-FIXTURE:` / `SHARP_RUNTIME_NEGATIVE_SITE` protocol; a full
  ASan+UBSan+LSan run of the permanent lifetime suites.
- **Effect**: test-only. Raises the fixture count from 9/66 to **11/~80**.
- **Approval required**: none beyond the tickets it closes.

---

## 26. Permanent test matrix (proposed)

| Suite | Cases | Pins |
|---|---|---|
| `JsonNodeLifetimeTests` | ~14 | retained child / value / grandchild / great-grandchild after owner destruction; null slots; re-attachment; `getRootProperty` on an orphan; `ToJsonString` on an orphan; orphan of a `Clear`ed container |
| `JsonNodeMutationTests` | ~6 | `SetItem` exception path on both containers; double-insert rejection; cycle rejection |
| `JsonNodeEnumeratorTests` | ~8 | stale iterator after `Add`, `Insert`, `RemoveAt`, `Clear`, `SetItem` — array and object |
| `XLinqLifetimeTests` | ~16 | retained node / attribute / grandchild / document root; sibling navigation; `Remove`; `ReplaceWith`; re-`Add`; `getNextAttributeProperty`; `getDocumentProperty` two levels deep |
| `XLinqMutationTests` | ~6 | `ReplaceWith` rejection keeps the original; `InsertNodeAt` throw leaves one owner; attribute relink after removal |
| `XLinqBorrowedViewTests` | ~4 | `Attributes()` snapshot; `Nodes()` snapshot; `Ancestors` handle lifetime |
| `PinsCurrentBehaviourTests` | ~6 | the deviations deliberately kept: `Add` moves rather than clones; options are not inherited; `getParentProperty` returns raw |
| Negative consumer fixtures | ~14 sites | every deleted copy/move, every non-public `DetachParent`/`setNextAttributeProperty`, every changed `Extensions` return type |

Estimated **+60 permanent add-only regressions**, taking the floor from 14,568
toward ~14,630. Every existing `JsonNodeTests` (53) and `XLinqNodeTests` (76)
case must pass **unmodified**; no test may be weakened, deleted or recategorised.

---

## 27. Sanitizer matrix

| Sanitizer | Applies | Why |
|---|---|---|
| **ASan** | **yes, mandatory** | the finding is a heap-use-after-free; 29 are measured today and all must fall silent |
| **UBSan** | **yes** | GCC's UBSan reported nothing on this matrix (GCC has no `-fsanitize=vptr`), so it is a **regression guard**, not a detector — recorded so a future reader does not mistake silence for coverage |
| **LSan** | **yes, mandatory** | the repair must not trade a use-after-free for a leak; candidate F's rejection is a LSan result |
| **ASan with `-fsanitize-recover=address`** | **yes** | counts *every* faulting access instead of the first; it is what established 57 reads / 0 writes (§5.3) |
| **TSan** | **no — recorded, not skipped** | neither family has an atomic, a lock, a cache or any shared mutable state, and the repair adds none (§23) |
| **No-sanitizer build** | **yes, mandatory** | ASan's quarantine hides allocator reuse; J16 and X22 are only visible without it |

Every reproducer must be compiled **from source into the probe** with the
sanitizer flags, as `build-probe/1885_build.sh` does, so no archive can be stale.

---

## 28. Migration strategy

1. **#1886 and #1890 need no consumer action** beyond a recompile. Consumers that
   never retained a child past its parent see no behaviour change at all.
2. Consumers that *did* retain one were relying on undefined behaviour; the
   correct migration is to check `getParentProperty()` for null, which they should
   already do for a detached node.
3. **#1888 and #1892 need mechanical edits**, each with a compiler diagnostic that
   names the replacement: `DeepClone()` for a copied `JsonArray`/`JsonObject`,
   `Attributes()` for `getAttributesProperty()`, and removal of any
   `DetachParent()` / `setNextAttributeProperty()` call.
4. **#1893 needs a consumer decision** only if that consumer parses JSON nested
   deeper than the chosen bound.
5. A `docs/Migration-OwnedTreeLifetimes.md` accompanies #1888 and #1892, following
   the `docs/Migration-ICollectionCopyTo.md` precedent from #1771.
6. **CNA and mobile-eggbert are out of scope**; #1773 remains `blocked` and no
   downstream repository is inspected.

---

## 29. Rollback strategy

| Ticket | Rollback | Blast radius |
|---|---|---|
| #1886 | revert two destructor bodies | 2 headers |
| #1887 | revert one method body | 1 header |
| #1888 | remove five `= delete`s and one access-specifier move | 1 header + 1 fixture |
| #1889 | revert the counter, the snapshot, the seam entry and the catalogue | 2 headers, 1 CMakeLists, 1 seam, 1 catalogue — **the largest rollback**, because it is the only layout change |
| #1890 | revert two destructor bodies | 2 headers |
| #1891 | revert two method bodies | 2 bodies |
| #1892 | revert three declarations | 3 headers + fixtures |
| #1893 | revert the depth check and the iterative walk | 4 files |
| #1894 | delete the fixtures | test-only |

Each ticket is a single commit touching a disjoint file set, so any one can be
reverted without disturbing the others — except #1887/#1889 (which assume #1886)
and #1891/#1892 (which assume #1890).

---

## 30. Explicit exclusions

1. **`getParentProperty()`'s raw return type is not changed.** Returning
   `shared_ptr` requires every parent to be `shared_ptr`-owned, which candidate
   A's measurement rejects (77 automatic-storage sites, §12).
2. **`XContainer::Add` keeps moving rather than cloning.** .NET clones
   (§9.2); the port's move is an authorised documented deviation.
3. **"My parent died" is deliberately indistinguishable from "I was removed".**
   Both give `parent_ == nullptr`. Distinguishing them needs candidate E's cell
   (+8/+24 bytes). If a future requirement needs it, E is the recorded fallback.
4. **`JsonNodeOptions` inheritance from the parent (J15) is not implemented.**
   It is a real .NET divergence, but implementing it adds a *new* parent
   dereference to a property getter — a new instance of this finding. It gets its
   own ticket if it is ever wanted.
5. **`SR-AUD-328`** (JsonValue integer truncation) and **`SR-AUD-336`**
   (inert `Changed`/`Changing`) are untouched and stay `confirmed`.
6. **`XStreamingElement`'s `std::any` self-reference cycle** is recorded (§16.6),
   not repaired: it is not an `XObject` and has no parent link.
7. **No consumer-held raw pointer is made safe.** A `JsonNode*` or `XElement*` a
   consumer stored from `getParentProperty()` before the parent died still
   dangles. Only exclusion 1 could change that.
8. **No new `SR-AUD-*` identifier.** Numbering stays frozen at **364**; the nine
   corrections in §6 are appended to SR-AUD-327's and SR-AUD-333's records.
9. **`XDocument::Parse` needs no depth bound** — tinyxml2 already rejects deep
   nesting (X29a–c).
10. **No performance benchmark** is proposed for the core repair (§21).

---

## 31. Exact approval request

Nothing below has been implemented. Each item is independently approvable; they
are listed in the order they would land.

### Item 1 — the core repair (tickets #1886, #1890) — **recommended**

> Approve adding a destructor to `System::Text::Json::Nodes::JsonArray` and
> `JsonObject`, and to `System::Xml::Linq::XContainer` and `XElement`, whose only
> effect is to clear each owned child's parent link (and, for `XElement`, each
> owned attribute's parent link and `next_` link) before that storage is
> released.

- **What representation changes:** nothing. `parent_` stays a raw
  `JsonNode*` / `XContainer*`; children stay `std::vector<std::shared_ptr<…>>`.
- **Public object layouts that change:** **none.** `JsonNode` 24, `JsonArray` 48,
  `JsonObject` 48, `JsonValue` 40, `XObject` 16, `XContainer` 40, `XElement` 128,
  `XAttribute` 120 — all unchanged (measured).
- **Virtual interfaces / vtables:** **unchanged.** The destructors are already
  virtual through the base; no slot is added, removed or reordered.
- **Copying behaviour:** unchanged by this item. (One side effect: declaring the
  destructors suppresses `JsonArray`/`JsonObject`'s implicit **move** operations,
  so `JsonArray a = std::move(b);` becomes a copy. Item 3 removes both anyway.)
- **Raw pointers becoming shared/weak/intrusive handles:** **none.**
- **Do externally retained detached objects become usable:** **yes.** Today a
  child of a destroyed parent can never be re-attached (`AssignParent` throws
  forever); after this item it can, exactly as a removed node can.
- **Insertion:** unchanged — JsonNode still **rejects** an already-parented node,
  Xml.Linq still **moves** it.
- **Must existing custom code migrate:** **no source edit.** A recompile is
  required (inline header change). One run-time behaviour change:
  `child->getParentProperty()` returns `nullptr` instead of a dangling pointer
  after the parent's destruction.
- **Memory and runtime overhead:** **zero bytes, zero allocations, zero per-access
  cost** (measured). Container destruction gains one pass over the child vector.
- **One approval or two:** **one approval covers both findings** for this item.
  The change is the same rule, the risk is identical, and splitting it would leave
  one family knowingly reachable to a use-after-free.

### Item 2 — exception-path consistency (tickets #1887, #1891)

> Approve reordering `JsonObject::SetItem` to assign the new parent before
> detaching the old value, and `XNode::ReplaceWith` to validate before removing.

Two currently-throwing calls change what they leave behind: `JsonObject::SetItem`
stops leaving a parentless node inside the object, and `ReplaceWith` stops losing
the node it was replacing. No signature, layout, vtable or allocation change.
**One approval covers both.**

### Item 3 — `JsonNode` value semantics (ticket #1888) — **a real source break**

> Approve `= delete` for `JsonNode`'s copy constructor, copy assignment, move
> constructor and move assignment (inherited by `JsonArray`, `JsonObject`,
> `JsonValue`), and moving `JsonNode::DetachParent()` from `public` to
> `protected`.

- Breaks `JsonArray b = a;`, `*node1 = *node2;`, and any external
  `DetachParent()` call, at **compile time**, with a diagnostic naming
  `DeepClone()`.
- Makes `JsonNode` match `XObject`, which already deletes all four.
- No layout or vtable change.
- **Needs its own approval** — it is the only item that stops existing code
  compiling for the JsonNode family.

### Item 4 — enumerator lifetime (ticket #1889) — **an object-layout change**

> Approve giving `JsonArray` and `JsonObject` a mutation counter and their
> iterators a version snapshot, so a stale iterator throws
> `InvalidOperationException` instead of reading freed storage.

- **`sizeof(JsonArray)` and `sizeof(JsonObject)` grow 48 → 56.**
- Adds a **public** component edge `Text.Json → Collections.Core`; the module
  graph goes 91 → **92** edges and the catalogue must be regenerated.
- **Needs its own approval** — it is the only layout change proposed.

### Item 5 — the Xml.Linq borrowed-view surface (ticket #1892) — **a real source break**

> Approve changing `XElement::getAttributesProperty()` to return by value,
> `Extensions::Ancestors`/`AncestorsAndSelf` to return owning handles instead of
> `std::vector<XElement*>`, and `XAttribute::setNextAttributeProperty` to become
> non-public.

- Breaks every `Ancestors`/`AncestorsAndSelf` call site and every
  `setNextAttributeProperty` call site at compile time.
- Adds a per-call `std::vector` copy where `getAttributesProperty()` returned a
  reference.
- **Needs its own approval.**

### Item 6 — deep-tree and deep-parse bounds (ticket #1893) — **an accepted-input change**

> Approve bounding `JsonNode::Parse`'s nesting depth (raising `JsonException`
> beyond it, mirroring what tinyxml2 already does for XML), making container
> teardown iterative, and bounding the ancestor/cycle guard.

- JSON text nested deeper than the bound starts being **rejected**, where today it
  crashes the process.
- No signature, layout or vtable change.
- **Needs its own approval** because it changes the accepted grammar, exactly as
  #1879/#1884 do for their families.

### What is *not* being asked for

Approval of a `weak_ptr`, `shared_ptr`, intrusive-refcount or arena
representation; of a change to `getParentProperty()`'s return type; of
clone-on-insert for Xml.Linq; of `JsonNodeOptions` inheritance; or of any change
to `SR-AUD-328` or `SR-AUD-336`. All are excluded in §30 with reasons.

---

## 32. Decision

**Item 1 approved by the user on 2026-07-31** — and only item 1. Items 2, 3, 4,
5 and 6 remain **unanswered**, so tickets #1887, #1888, #1889, #1891, #1892 and
#1893 stay `needs_user` and none of them was started. The approval covers
tickets **#1886** and **#1890** together, exactly as §31 item 1 asked, and it
explicitly accepts the one observable behaviour change (§19.2): after the owning
container is destroyed, `getParentProperty()` returns `nullptr` instead of a
dangling pointer, and consumers recompile.

The paragraph this section previously held — *"Pending. SR-AUD-327 and
SR-AUD-333 remain `confirmed`. Tickets #1886–#1894 are created `blocked` or
`needs_user` and may not start until the items in §31 are answered."* — is
retained here as the historical record of the state #1885 left behind.

**SR-AUD-327 and SR-AUD-333 both stay `confirmed (design-complete)`**: item 1
closes most of each finding but not all of it, and §33.6 / §34.6 list exactly
what each one still leaves reachable. See §33 and §34 for the implementation
evidence.

---

## 33. Implementation record — #1886, JsonNode owner-side detachment (2026-07-31)

*Everything in this section is **measured**. Nothing in §§1–31 was rewritten;
this section is appended, exactly as #1882/#1883 appended theirs.*

### 33.1 What changed

Two files, two destructors, 43 added lines of which 37 are doc-comment:

| File | Change |
|---|---|
| `modules/text-json/include/System/Text/Json/Nodes/JsonArray.hpp` | `~JsonArray() override` — detaches every owned child |
| `modules/text-json/include/System/Text/Json/Nodes/JsonObject.hpp` | `~JsonObject() override` — detaches every owned value |

```cpp
~JsonArray() override {
    for (const auto& item : items_)
        if (item && item->getParentProperty() == this) item->DetachParent();
}

~JsonObject() override {
    for (const auto& [name, value] : properties_)
        if (value && value->getParentProperty() == this) value->DetachParent();
}
```

**One deliberate strengthening over §13.1's sketch:** the `getParentProperty() ==
this` guard. §13.1 wrote the loop unconditionally; that is wrong for the two
shapes this family actually has, both measured in #1885. `JsonArray copy =
*orig;` (J08) leaves the *copy* holding children whose `parent_` names the
**original**, and the public `DetachParent()` (J13) lets a node sit in one
container while its link names another. An unguarded loop would let the
destroying container steal a link it does not own. Mutation-checked: removing the
guard fails exactly the two permanent tests written for it
(§33.5), while the loops themselves are pinned by 22.

### 33.2 Why there is no helper

§13.1's rule is one line of logic over two differently shaped stores
(`vector<shared_ptr<JsonNode>>` versus
`vector<pair<string, shared_ptr<JsonNode>>>`). The only class both containers
share is `JsonNode`, so any shared helper would have to be **added to a public
header** — a new protected or static member on the very type whose surface the
approval pins at "nothing changes". Both loops use only `getParentProperty()` and
`DetachParent()`, which are already public and are already what `Clear()` calls,
so the destructors need no new name anywhere. **No helper was added**, and
therefore no new entity of any linkage appears in any public header. A
file-local helper is not an option either: an anonymous-namespace function
referenced from an inline destructor is an ODR violation.

### 33.3 Measured evidence

Re-ran **the same** probe `build-probe/1885_ccf019_lifetime_probe.cpp`,
**unmodified**, through **the same** `build-probe/1885_build.sh`, in the same
three builds from one source, one forked process per case under the same
5-second watchdog. Every production translation unit is compiled from source into
the probe, so no archive can be stale. Case-by-case classification is mechanical
(`build-probe/1886_classify_cases.py`); the tables are
`build-probe/1886_asan_before_after.txt` and `1886_none_before_after.txt`.

| Case | before (ASan) | after (ASan) | before (plain) | after (plain) |
|---|---|---|---|---|
| J01 `getRootProperty()` | heap-use-after-free | **clean** | freed address | **the child itself** |
| J02 `parent->GetValueKind()` | heap-use-after-free | probe-side null deref (§33.4) | **SIGABRT** `pure virtual method called` | probe-side null deref |
| J03 `JsonObject` value | heap-use-after-free | **clean** | freed address | **the child itself** |
| J04 3-level tree | heap-use-after-free | **clean** | freed address | **the child itself** |
| J05 stale parent value | clean but non-null | **clean, `nullptr`** | non-null stale | **`nullptr`** |
| J06 re-`Add` an orphan | throws `The node already has a parent.` | **succeeds** | throws | **succeeds** |
| J08 copy-constructed array | heap-use-after-free | **clean** | freed address | defined |
| J16 allocator reuse | heap-use-after-free | probe-side null deref (§33.4) | wrong `GetValueKind` at the same address | probe-side null deref |
| J17 stale-ancestor cycle guard | heap-use-after-free at `JsonNode.cpp:22` | **clean** | succeeded on freed memory | **succeeds on live memory** |
| J11 stale iterator | heap-use-after-free | heap-use-after-free (**#1889**) | SIGSEGV | SIGSEGV |
| J19c / J19d | stack-overflow / timeout | unchanged (**#1893**) | SIGSEGV / timeout | unchanged |

**Totals.** JsonNode cases producing an ASan `heap-use-after-free`: **8 → 1**
(J11 alone). Faulting accesses in the JsonNode section under
`-fsanitize-recover=address` with `halt_on_error=0`: **9 → 1**. The Xml.Linq
section is byte-for-byte unchanged at 48 faulting accesses in the same run, which
is the control: it shows the JSON-side change did not perturb the other family
and that #1890 was still outstanding when this was measured.

### 33.4 The two cases that now fault in *probe* code

J02 and J16 call `child->getParentProperty()->GetValueKind()` **with no null
check**, because when the probe was written the pointer was never null. Post-fix
it is `nullptr`, so those two lines dereference null *inside the probe*, at
`1885_ccf019_lifetime_probe.cpp:141` and `:342`; UBSan names it
`member access within null pointer of type 'struct JsonNode'`. This is not a
library defect and is recorded rather than glossed: the library-side
use-after-free is gone, and the permanent suite asserts the defined answer for
the identical two shapes
(`RetainedChild_AfterOwnerDestroyed_ParentIsNotDereferenceable`,
`RetainedChild_ReportsNoParentEvenIfTheFreedOwnersStorageIsReused`). A consumer
that dereferences a null parent gets exactly what it already gets today for a
node it removed itself.

### 33.5 Permanent tests

`modules/text-json/tests/System/Text/Json/Nodes/JsonNodeLifetimeTests.cpp` —
**32 cases, +32 to the repository floor (14,568 → 14,600)**, none of the 53
existing `JsonNodeTests` cases modified. Coverage: owner alive (2); retained
child/value after a heap owner (2); after an automatic-storage owner (2);
non-dereferenceable parent (1); nested and leaf-only retention (2); links owned
by another container (2); removed / replaced / moved / cleared before destruction
(5); empty owners, null slots, eight children, eight properties (4); rejected
insertion, failed `SetItem` ×2, duplicate-key `Add` (4); destruction during
exception unwinding, flat and nested (2); re-attachment of a child, a value and a
whole subtree (3); allocator reuse (1); public layout (1) plus nine
`static_assert`s.

**Mutation checks.** Emptying both destructor bodies fails **22** of the 32
(ticket #1886 required ≥ 4). Removing only the `getParentProperty() == this`
guard fails exactly **2** — the two written for it — and nothing else, which is
what proves the guard is load-bearing and not merely defensive.

Under `build-asan` (`-fsanitize=address,undefined`, LSan on): **179/179
`SharpRuntimeTests_Text_Json` pass, zero diagnostics, zero leaks**
(`build-probe/1886_asan_tests.log`). Sanitizer activation is proved separately by
a controlled self-test compiled with the same flags: a deliberate
use-after-free aborts with ASan and a deliberate leak is reported by LSan, while
the control run is clean (`build-probe/1886_sanitizer_selftest.log`).

### 33.6 Invariants, and what is still open

| Claim | Evidence |
|---|---|
| `sizeof` of all eleven public types unchanged | `build-probe/1886_layout_sizes.log` — 24/48/48/40 and 16/16/40/128/120/48/56, plus nine `static_assert`s in the permanent suite |
| No member added, no vtable slot added or reordered | GCC's own `-fdump-lang-class` dumps, pre-fix headers versus current, are **identical** for all eleven types (`build-probe/1886_layout_{prefix,postfix}.public.txt`). The only whole-dump difference is extra `std::iterator_traits`/`__normal_iterator` instantiations the range-`for` pulls in. |
| No public signature change, no ABI surface change | 219 external/weak defined symbols in `JsonNode.cpp` before, **219 identical** after; the `D0`/`D1`/`D2` destructor symbols and the two vtable symbols were already emitted by the implicit destructors (`build-probe/1886_extsyms_*.txt`) |
| No allocation added to construction, access or destruction | counting `operator new`/`delete`: construct+4×`Add` 16 new both sides, 100,000 × (`getParentProperty` + `getRootProperty`) **0 new both sides**, destruction **0 new / 11 delete both sides** (`build-probe/1886_alloc_{prefix,postfix}.log`) |
| No per-access overhead | same probe: the access loop allocates nothing and the accessors are unchanged source |
| No new ownership cycle, no leak | LSan clean over the whole ASan test run and over the probe |
| Destructors cannot terminate | `std::is_nothrow_destructible_v` asserted for both; two permanent tests destroy owners during live exception unwinding |
| Repeated detach is harmless | `OriginalDestroyedBeforeItsCopy_DetachesOnceAndTheCopyIsHarmless` |
| Partially constructed containers are safe | the loop touches only `items_`/`properties_`, which are constructed before any derived body can throw; `EmptyOwnersDestroyCleanly` pins the empty case |
| Module graph unchanged | `scripts/validate_module_boundaries.py` and `generate_component_catalog.py --check` — 41 modules, 91 edges, no new dependency (the destructors call nothing new) |

**Still reachable after #1886, each mapped to its own approval item:** J11 and
J12 (enumerator lifetime → #1889, §31 item 4); J08's aliasing copy, J09's slicing
copy-assign and J13's public `DetachParent()` (→ #1888, §31 item 3); J10's
`SetItem` exception path (→ #1887, §31 item 2); J19c/J19d/X28c deep nesting (→
#1893, §31 item 6); J15's option inheritance (permanent exclusion, §30.4).
Because J11 is still an ASan-confirmed use-after-free in this finding's own
files, **SR-AUD-327 stays `confirmed (design-complete)`** and the post-audit
tally is unchanged at 57 remediated / 306 confirmed / 364.
---

## 34. Implementation record — #1890, XObject owner-side detachment (2026-07-31)

*Everything in this section is **measured**. Nothing in §§1–31 was rewritten.*

### 34.1 What changed

Two files, two destructors:

| File | Change |
|---|---|
| `modules/xml-linq/include/System/Xml/Linq/XContainer.hpp` | `~XContainer() override` — detaches every owned child node |
| `modules/xml-linq/include/System/Xml/Linq/XElement.hpp` | `~XElement() override` — detaches every owned attribute **and clears its `next_`** |

```cpp
~XContainer() override {
    for (const auto& child : children_) {
        if (!child) continue;
        XObject& obj = *child;                 // XContainer is a friend of XObject
        if (obj.parent_ == this) obj.parent_ = nullptr;
    }
}

~XElement() override {
    for (const auto& attr : attributes_) {
        if (!attr) continue;
        if (attr->parent_ != this) continue;   // XElement is a friend of XAttribute
        attr->parent_ = nullptr;
        attr->next_ = nullptr;
    }
}
```

Two deliberate differences from §14.1's sketch, both required by the ticket's own
constraints:

1. **The links are written directly instead of through `AdoptObject()` /
   `setNextAttributeProperty()`.** `~XElement` must *read* the attribute's parent
   link to decide whether it owns it, and `AdoptObject` only writes. `XContainer`
   is already a `friend` of `XObject` and `XElement` is already a `friend` of
   `XAttribute`, so both links are reachable without adding anything to any
   header. Writing `next_` directly also means the destructor does not depend on
   the public `setNextAttributeProperty()`, which #1892 proposes to remove.
2. **The `parent_` link is only ever *compared*, never dereferenced.** §14.1's
   `AdoptObject(*c, nullptr)` would have written unconditionally; the comparison
   is what makes "do not alter an object that has already been detached or
   transferred" true by construction, and it never touches the pointee, so a
   hypothetical stale link cannot be followed even to test it.

`~XElement` runs first and clears the attribute side; `~XContainer` then runs and
clears the child side. Both stores are still fully alive at their loop.

### 34.2 The ownership guard is **not** load-bearing here, and that is recorded

On the JsonNode side (§33.1) the `== this` guard is load-bearing: J08's aliasing
copy and J13's public `DetachParent()` both create a container that holds an
object owned by someone else, and removing the guard fails tests. **In this
family it is currently unreachable to violate.** `XContainer::InsertNodeAt` calls
`n->parent_->RemoveNode(n.get())` and `XElement::Add(attribute)` calls
`owner->RemoveAttribute(attr.get())`, both of which **erase** the object from the
previous owner's vector before adopting it; and `XObject` deletes all four
copy/move operations, so no aliasing container can exist. Measured: removing both
guards fails **zero** of the 35 permanent tests.

The guards are retained anyway — §31 item 1 specifies them, they make the
destructor's contract explicit, and #1891/#1892 may change insertion — but the
measurement is recorded rather than implied. What *is* pinned is the invariant
they rest on: `NodeMovedToAnotherOwner_KeepsTheNewParentAfterTheOldOwnerDies` and
`AttributeMovedToAnotherOwner_KeepsTheNewParentAfterTheOldOwnerDies` both assert
that the source container is empty after a move.

### 34.3 Measured evidence

Same probe, same build script, same three builds, same forked-per-case watchdog;
classification by `build-probe/1886_classify_cases.py`; tables in
`build-probe/1890_asan_before_after.txt` and `1890_none_before_after.txt`.

| Case | before (ASan) | after (ASan) | before (plain) | after (plain) |
|---|---|---|---|---|
| X01 `getParentProperty()` | heap-use-after-free | **clean** | **SIGABRT** `pure virtual method called` | **`(nil)`** |
| X02 `getDocumentProperty()` | heap-use-after-free | **clean** | **SIGABRT** | **`(nil)`** |
| X03 / X04 sibling pointers | heap-use-after-free | **clean** | `nil` (by luck) | **`(nil)`** |
| X05 `NodesBeforeSelf()` | heap-use-after-free (8 accesses) | **clean** | **SIGSEGV** | **size 0** |
| X06 `NodesAfterSelf()` | heap-use-after-free | **clean** | 0 | **size 0** |
| X07 `Remove()` | heap-use-after-free on a mutating path | **throws `The parent is missing.`** | returned silently | **throws** |
| X08 `ReplaceWith(node)` | heap-use-after-free | **throws** | **SIGABRT** | **throws** |
| X09 `CompareDocumentOrder` | heap-use-after-free | **throws `A common ancestor is missing.`** | throws | throws |
| X10 attribute `getParentProperty()` | heap-use-after-free | **clean** | **SIGABRT** | **`(nil)`** |
| X11 `getPreviousAttributeProperty()` | heap-use-after-free | **clean** | **SIGABRT** | **`(nil)`** |
| X12 `XAttribute::Remove()` | heap-use-after-free | **throws** | **SIGABRT** | **throws** |
| X13 `getNextAttributeProperty()` | heap-use-after-free | **clean** | a freed `XAttribute*`, name read `"b"` | **`nullptr`** |
| X14 orphan re-`Add`ed to a live element | heap-use-after-free at `XContainer.cpp:40` | **clean** | succeeded through a stale parent | **succeeds cleanly** |
| X18 / X19 destroyed `XDocument`, 1 and 2 levels | heap-use-after-free | **clean** | **SIGABRT** | **`(nil)`** |
| X22 allocator reuse | heap-use-after-free | **clean** | stale parent reported `"squatter"` | **`(nil)`, no name** |
| X25 two retained attributes → `Remove()` | heap-use-after-free | **throws** | **SIGABRT** | **throws** |
| X26 retained child → `ReplaceWith({r1,r2})` | heap-use-after-free | **throws** | **SIGABRT** | **throws** |
| X15 `Extensions::Ancestors` | heap-use-after-free | heap-use-after-free (**#1892**) | returns `"root"` | unchanged |
| X17 `getAttributesProperty()` reference | heap-use-after-free | heap-use-after-free (**#1892**) | size 1 from freed storage | unchanged |
| X20 `ReplaceWith` rejection | clean but loses the node | unchanged (**#1891**) | `<a/>` | unchanged |
| X21 `Add` moves | clean | unchanged — deliberate (§14.4) | moves | moves |
| X27c / X27d | stack-overflow / timeout | unchanged (**#1893**) | SIGSEGV / timeout | unchanged |

**Totals.** XObject cases producing an ASan `heap-use-after-free`: **21 → 2**
(X15 and X17). Faulting accesses in the XObject section under
`-fsanitize-recover=address`: **48 → 4** (two each for X15 and X17). Public entry
points aborting with `pure virtual method called`: **8 → 0**.

### 34.4 Both tickets together

| | #1885 baseline | after #1886 | after #1890 |
|---|---|---|---|
| ASan `heap-use-after-free` **cases**, both families | 29 | 22 | **3** |
| Faulting **accesses**, recoverable ASan | 57 | 49 | **5** |
| `pure virtual method called` aborts | 8 | 8 | **0** |
| Remaining cases | — | — | J11 (#1889), X15, X17 (#1892) |

**26 of the 29 measured use-after-free cases are closed.** §1 of this document
estimated 27; the measured figure is **26**, and the difference is recorded
rather than rounded: §13.5 and §14.3 always listed J11 (→ #1889) and X15/X17
(→ #1892) as *out* of the core repair, which is three, not two.

### 34.5 Permanent tests

`modules/xml-linq/tests/System/Xml/Linq/XLinqLifetimeTests.cpp` — **35 cases,
+35 to the repository floor (14,600 → 14,635)**, none of the 92 existing Xml.Linq
cases modified. Coverage: owner alive, element and document (2); retained text
node after a heap and an automatic owner (2); non-dereferenceable parent (1);
sibling navigation (1); `Remove`/`ReplaceWith`/`CompareDocumentOrder` on an
orphan (3); retained attribute parent, `next_`, `Remove` (3); eight attributes
and eight children (2); nested trees and destroyed documents at one and two
levels (4); moved node and moved attribute (2); removed / replaced / `RemoveAll`
before destruction (4); empty owners and attributes-only/children-only owners
(2); exception unwinding, flat and nested (2); rejected duplicate attribute and
self-insertion (2); re-attachment of a node, an attribute and a subtree (3);
allocator reuse (1); public layout (1) plus fourteen `static_assert`s.

**Mutation checks, each test run in its own process so an abort cannot hide the
rest.** Emptying both destructor bodies is detected by **23** of 35 (ticket #1890
required ≥ 6) — thirteen of those as `pure virtual method called` aborts, which is
the pre-fix failure mode reproduced exactly. Deleting only `attr->next_ = nullptr`
is detected by **2**. Removing both ownership guards is detected by **0**, for the
reason in §34.2.

Under `build-asan` (`-fsanitize=address,undefined`, LSan on): **127/127
`SharpRuntimeTests_Xml_Linq` pass, zero diagnostics, zero leaks**
(`build-probe/1890_asan_tests.log`), alongside 179/179 for Text.Json. Sanitizer
activation proved by the same controlled self-test (§33.5).

### 34.6 Invariants, and what is still open

| Claim | Evidence |
|---|---|
| `sizeof` of all eleven public types unchanged | `build-probe/1886_layout_sizes.log` — 16/16/40/128/120/48/56 on this side, plus fourteen `static_assert`s |
| No member added, no vtable slot added or reordered | GCC `-fdump-lang-class`, pre-fix headers versus current, **identical** for all eleven types |
| No public signature change | none of the four files touches a declaration other than adding the two destructors |
| ABI surface | **three weak COMDAT symbols added** — `_ZN6System3Xml4Linq10XContainerD0Ev`/`D1Ev`/`D2Ev` — because GCC previously inlined `XContainer`'s implicit destructor and emitted no standalone definition. `XElement`'s three were already emitted. **No symbol name was removed**, and the added names are the standard Itanium mangling the destructor already had as a language entity (`build-probe/1890_extsyms_*.txt`) |
| No allocation added anywhere | counting `operator new`/`delete`: construct+4 nodes+2 attributes 12 new both sides; 100,000 × (`getParentProperty` + `getDocumentProperty`) **0 new both sides**; destruction **0 new / 8 delete both sides** |
| No new ownership cycle, no leak | LSan clean over the ASan test run and the probe; the repair adds no edge, it clears two existing non-owning ones earlier |
| Destructors cannot terminate | `std::is_nothrow_destructible_v` asserted for `XContainer` and `XElement`; two permanent tests destroy owners during live exception unwinding |
| Value semantics untouched | `!is_copy_constructible`, `!is_copy_assignable`, `!is_move_constructible` still asserted for `XElement` |
| Module graph unchanged | 41 modules, 91 edges; catalogue check current |

**Still reachable after #1890:** X15 and X17 (borrowed views → #1892, §31 item 5);
X20 (`ReplaceWith` exception path → #1891, §31 item 2); X27c/X27d (deep nesting →
#1893, §31 item 6); X21's move-on-`Add` (permanent authorised deviation, §30.2);
SR-AUD-336's inert events (separate finding). Because X15 and X17 are still
ASan-confirmed use-after-free in this finding's own files, **SR-AUD-333 stays
`confirmed (design-complete)`** and the post-audit tally is unchanged at 57
remediated / 306 confirmed / 364.

---

## 35. Implementation record — #1887, `JsonObject::SetItem` assign-before-detach (2026-07-31)

*Everything in this section is **measured**. Nothing in §§1–34 was rewritten; the
corrections this ticket found are appended in §35.4.*

### 35.0 What authorised it

§31 item 2 was **unanswered** when §32 was written. The user's batch instruction
of 2026-07-31 directed this batch to *"Start the autonomous CCF-019 residual
batch now with §31 item 2: ticket #1887, followed by ticket #1891"*, and set an
approval boundary that grants nothing for public source breaks, virtual-interface
or vtable changes, object- or iterator-layout changes, return calling-convention
changes, mandatory downstream migration, or broad observable semantic changes.
**#1887 and #1891 require none of those** (§35.3, §36.3), so item 2 was taken as
approved and items 3, 4, 5 and 6 were **not**. §32's record of what item 1 covered
is unchanged.

### 35.1 What changed

One method body, in one header:
`modules/text-json/include/System/Text/Json/Nodes/JsonObject.hpp`.

```diff
     void SetItem(const std::string& propertyName, std::shared_ptr<JsonNode> value) {
         intcs idx = findIndex(propertyName);
         if (idx >= 0) {
             auto& slot = properties_[static_cast<size_t>(idx)].second;
             if (slot == value) return;
-            if (slot) slot->DetachParent();
-            if (value) value->AssignParent(this);
+            if (value) value->AssignParent(this);
+            if (slot) slot->DetachParent();
             slot = std::move(value);
         } else {
```

Two statements swapped. The `else` branch (a brand-new key) already validated
before it stored, and is untouched.

### 35.2 The structurally equivalent surface, enumerated

Every mutating entry point of both containers was inspected, not only the one the
probe named. `SetItem` on `JsonObject` was the **only** site that mutated before
it validated:

| Entry point | Order | Verdict |
|---|---|---|
| `JsonObject::SetItem` (existing key) | detach-then-assign | **the defect (J10)** — fixed |
| `JsonObject::SetItem` (new key) | assign-then-store | already correct |
| `JsonObject::Add` | duplicate-key check, assign, store | already correct |
| `JsonObject::Remove` / `Clear` | detach only | no validation to order against |
| `JsonArray::SetItem` | assign-then-detach | already correct (**J18**) |
| `JsonArray::Add` / `Insert` | range check, assign, store | already correct |
| `JsonArray::RemoveAt` / `Remove` / `Clear` | detach only | no validation to order against |

### 35.3 Measured evidence

**Probe.** The #1885 probe was rebuilt from source and re-run **unmodified**
(same `1885_build.sh`, same 60 translation units compiled into the binary, no
archive linked, one `fork()`ed process per case, same 5-second watchdog). The
pre-change replay reproduced `1890_postfix_asan.log` exactly — **0 of 58 cases
changed classification** — so this batch's baseline is the previous batch's
recorded end state, verified rather than inherited
(`build-probe/1887_prefix_asan.log`).

After the change, **exactly 1 of 58 cases changed**, and it is J10:

| Case | Before | After |
|---|---|---|
| **J10** | `clean` — *"object still holds it (count=1, same node: YES) but its parent is **NULL**"*, then *"the SAME node is now in two containers; second count = 1"* | `throws: The node already has a parent.` — *"object still holds it (count=1, same node: YES) but its parent is **non-null**"*, and the second container **rejects** it |
| J18 (contrast) | `clean` — old still stored with parent == the array | unchanged |
| every other 56 cases | — | **identical classification** |

`build-probe/1887_postfix_asan.log`. The residual matrix is unchanged: 3 ASan
`heap-use-after-free` cases (J11, X15, X17), 3 ASan `stack-overflow`s (J19c,
X27c, X28c), 2 timeouts (J19d, X27d), and J02/J16's *probe-side* null
dereferences (§33.4).

**Permanent tests.** `JsonNodeMutationConsistencyTests`, a new suite of **22**
cases in
`modules/text-json/tests/System/Text/Json/Nodes/JsonNodeMutationConsistencyTests.cpp`:
five rejected-`SetItem` shapes (already-parented value, the cycle door, a new
key, a value owned by the same object under another key, and the second-container
refusal that made J10 a double-ownership defect), seven success paths (replace,
re-attach the replaced value elsewhere, same-node-same-key no-op, new key, null
value, over a null slot, serialised form unchanged), the two `JsonArray`
contrasts (J18 and the out-of-range rejection), five further rejection paths on
`Add`/`Insert`, `Remove`/`Clear` detachment, and the composition with #1886's
destructor. `SharpRuntimeTests_Text_Json` goes **179 → 201**; all 179 pre-existing
cases pass **unmodified**.

**Mutation test.** Reversing the two statements — the exact pre-fix ordering —
fails **5** of the 22 (`RejectedObjectSetItem_LeavesTheReplacedValueOwned`,
`…_TheReplacedValueIsStillRefusedElsewhere`, `CycleRejectedObjectSetItem_…`,
`…_ValueOwnedBySameObject_MovesNothing`, `…_ThenOwnerDestroyed_DetachesTheValue`)
and passes the other 196 in the executable, so the repair is load-bearing and the
tests are specific to it.

**Sanitizers.** `build-asan` (`-fsanitize=address,undefined`,
`detect_leaks=1`, `print_stacktrace=1`), binary proved newer than both changed
files (`build-probe/1887_asan_freshness.log`): **201/201 pass, zero ASan, UBSan
or LeakSanitizer diagnostics** (`build-probe/1887_asan_tests.log`). Sanitizer
activation is the same controlled self-test §33.5 recorded
(`build-probe/1886_sanitizer_selftest.log`) — the flags and the tree are
unchanged.

**Source / ABI / layout / performance.** No declaration changed: `SetItem`'s
signature, `JsonObject`'s members and every other member are untouched, so
`sizeof(JsonObject)` stays 48, no vtable slot moves, and the inline member's weak
COMDAT symbol already existed under the same mangling. No allocation is added or
removed — the two swapped statements each write one raw pointer. Consumers
recompile because the body is inline in a public header; **no consumer source
edit is required**, and the only observable change is on a path that already
threw.

### 35.4 Corrected premise — .NET's own ordering

§13.5 row J-2 reads *"`JsonObject::SetItem` → assign-before-detach, matching
`JsonArray::SetItem` **and .NET**"*. That is preserved above as written, and the
second half of it is **wrong**. Read on 2026-07-31 from
`/rv/tmp/runtime/src/libraries/System.Text.Json/src/System/Text/Json/Nodes/JsonObject.cs`
(`internal void SetItem`, lines 284–314):

```csharp
DetachParent(replacedValue);
dict.SetAt(index, value);
…
value?.AssignParent(this);
```

.NET detaches the replaced value **and commits the dictionary write** before
`AssignParent` runs, so a .NET `SetItem` that throws leaves the object holding
the *new* value while that value still reports its original parent, and the
replaced value both detached and evicted. That is a strictly larger inconsistency
than the port's, not a model for it. **This ticket deliberately diverges from
.NET here and matches `JsonArray::SetItem` instead**, which is what the acceptance
criterion asked for and what §22's exception-safety requirement demands. The
divergence is unobservable on every path that does not throw.

### 35.5 What this ticket did **not** do

- `std::bad_alloc` from `properties_.emplace_back` (new-key branch) can still
  leave `value` adopted but unstored. It is **not** closed here: the allocator
  failure path cannot be exercised without an allocator injection point that
  `std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>>` does not
  provide, so a repair would be untestable code on an untested path, and .NET's
  `JsonObject`, `JsonArray::Add`, `JsonArray::Insert` and `XContainer` all carry
  the identical residue. Recorded, not hidden.
- Nothing owned by #1888 (J08/J09/J13), #1889 (J11/J12) or #1893 (J19c/J19d)
  was absorbed. `SR-AUD-327` therefore **stays `confirmed (design-complete)`**:
  J11 is still an ASan-confirmed use-after-free inside this finding's own files.

---

## 36. Implementation record — #1891, `ReplaceWith` keeps the node it replaces (2026-07-31)

*Everything in this section is **measured**. Nothing in §§1–35 was rewritten; the
correction this ticket found is appended in §36.4.*

### 36.0 What authorised it

The same §31 item 2 approval §35.0 records. #1891 changes two `.cpp` bodies and
one header's doc-comments; no declaration, member, layout, vtable slot or
symbol name changes (§36.3).

### 36.1 What changed

**`modules/xml-linq/src/System/Xml/Linq/XNode.cpp` — `ReplaceWith(vector)`.**
The node being replaced is now held in a local `shared_ptr` across its own
removal, the insertion loop runs inside a `try`, and a refused insertion undoes
every insertion the call had already made and puts the replaced node back at its
original index with its original parent:

```cpp
std::shared_ptr<XNode> replaced = (it != siblings.end()) ? *it : nullptr;
c->RemoveNode(this);
size_t inserted = 0;
try {
    for (const auto& n : replacements) { if (!n) continue; c->InsertNodeAt(index + inserted, n); ++inserted; }
} catch (...) {
    size_t undone = 0;
    for (const auto& n : replacements) { if (!n) continue; if (undone == inserted) break; ++undone; c->RemoveNode(n.get()); }
    if (replaced) { c->children_.insert(c->children_.begin() + index, replaced); XContainer::AdoptObject(*replaced, c); }
    throw;
}
```

**`modules/xml-linq/src/System/Xml/Linq/XContainer.cpp` — `InsertNodeAt`.**
`AdoptObject(*n, this)` moved from *before* the vector insert to *after* it.

**Why the rollback cannot itself throw.** `RemoveNode` only searches, writes one
raw pointer and erases — `std::vector<std::shared_ptr<XNode>>::erase` moves
`shared_ptr`s, which is `noexcept`. The final `insert` cannot reallocate: the
call erased one element (`this`) and then erased the `inserted` it had added, so
`size() < capacity()` holds and `vector::insert` is only permitted to reallocate
when `size() == capacity()`. Copying a `shared_ptr` does not allocate. A rollback
that threw would replace the caller's real diagnosis with a fabricated one, which
is why it is written with direct container operations rather than by calling
`InsertNodeAt` back.

**Why `replaced` is captured.** `c->children_` may hold the node's last owning
reference, in which case `RemoveNode` destroys it — pre-fix that was harmless
only because nothing afterwards touched `this`, and it is fatal to a rollback
that has to put the node back. Capturing it also defers that destruction to the
end of the call, which is the only success-path behaviour change and is not
observable through the public surface.

### 36.2 Measured evidence

**Probe, `asan` build.** The unmodified #1885 probe was rebuilt from source and
re-run. X20 changes from

```
-> victim still in the tree: NO (LOST); victim parent: null
   a serialises to <a/>
```

to

```
-> victim still in the tree: yes; victim parent: non-null
   a serialises to <a>victim</a>
```

(`build-probe/1891_postfix_asan.log`). The mechanical classifier reports **0 of
58 cases changed classification**, and that is the correct result rather than a
null one: X20 is one of the twelve cases §4.3/§5.3 record as *silent* — it
produces a wrong answer with **no** sanitizer diagnostic in any build — so its
repair is visible in the answer lines, not in the diagnosis. X21's documented
move-on-`Add` behaviour is unchanged.

**Probe, `none` build (the canonical build for the silent shapes).** Diffing
every answer line of `1890_postfix_none.log` against `1891_postfix_none.log`
across all 58 cases yields exactly **two** semantic differences in the whole
matrix — J10 (#1887) and X20 (#1891) — plus ASLR address noise in the three cases
that print a pointer. Nothing else in either family moved.

**Permanent tests.** `XLinqMutationConsistencyTests`, a new suite of **26** cases
in `modules/xml-linq/tests/System/Xml/Linq/XLinqMutationConsistencyTests.cpp`:
five rejection shapes (an ancestor, the container itself, a wrong node kind
inside an `XElement`, CDATA inside an `XDocument`, and a refused *second*
replacement), index restoration in the middle of a sibling list, the replaced
node's continued usability (`getPreviousNodeProperty`, `Remove()`), a rejected
call followed by a successful one, the container-holds-the-last-reference case,
seven success paths (single, several, null, empty vector, orphan, moving an
attached node, a sibling), the `XDocument` root replacement and the two-root
batch, and five `Add`/`AddFirst`/move-ordering cases for `InsertNodeAt`.
`SharpRuntimeTests_Xml_Linq` goes **127 → 153**; all 127 pre-existing cases pass
**unmodified**.

**Mutation tests, four**, each run one test per process so an abort cannot hide
the rest:

| Mutation | Detected by |
|---|---|
| the whole rollback removed (the pre-fix `ReplaceWith` body) | **12** of 26 |
| insertions undone but the replaced node **not** restored | **12** of 26 |
| the replaced node restored but the insertions **not** undone | **3** of 26 |
| `InsertNodeAt` adopts before it inserts (the pre-fix order) | **0** — see below |

The fourth mutation is recorded at zero rather than hidden. Adoption order is
only observable when `children_.insert` throws, i.e. on `std::bad_alloc`, and
`std::vector<std::shared_ptr<XNode>>` offers no allocator injection point, so the
difference cannot be exercised from a test. It is kept because the reverse order
lets a node report a parent that does not hold it — a state whose own `Remove()`
would then silently do nothing — and because it costs nothing. This is the same
kind of honest zero §34.2 recorded for the Xml.Linq ownership guards.

**Sanitizers.** `build-asan` (`-fsanitize=address,undefined`, `detect_leaks=1`,
`print_stacktrace=1`), binary proved newer than all three changed files
(`build-probe/1891_asan_freshness.log`): **153/153 pass, zero ASan, UBSan or
LeakSanitizer diagnostics** (`build-probe/1891_asan_tests.log`). The rollback
path is exercised under the sanitizers by eleven of the new cases, so the
"cannot throw, cannot reallocate" reasoning above is also checked dynamically.

### 36.3 Source / ABI / layout / performance

| Claim | Evidence |
|---|---|
| No declaration changed | `XNode.hpp` gains doc-comments only; both bodies are out-of-line in `.cpp` |
| `sizeof`/`alignof` unchanged | no member added anywhere; the 23 `static_assert`s in the two permanent lifetime suites still hold (`XLinqLifetimeTests.PublicLayoutIsUnchangedByTheDetachContract` passes unmodified) |
| No vtable slot added or reordered | no virtual function added, removed or reordered; `ValidateNode` keeps its signature (a change there would have been a vtable change, which §36.4 is precisely about avoiding) |
| ABI — `XContainer.o` | **49 external/weak defined symbols before, 49 identical after** (`build-probe/XContainer_{prefix,postfix}.syms`) |
| ABI — `XNode.o` | **46 before, 48 after**; the two added are weak COMDAT standard-library instantiations the new body needs locally — `std::_Sp_counted_base<_S_atomic>::_M_release()` and `std::vector<shared_ptr<XNode>>::insert(const_iterator, const&)`. **No symbol name was removed** (`build-probe/XNode_{prefix,postfix}.syms`) |
| Allocations | success path adds **one `shared_ptr` copy** — an atomic refcount increment, no allocation. The rollback path allocates nothing at all (it reuses capacity the container already had) |
| Migration | **none.** No consumer source edit; `XNode.cpp`/`XContainer.cpp` are library bodies, so consumers need only relink |

### 36.4 Corrected premise — "validate before removing" is not implementable as written

§25's #1891 entry and §14.3's row X-2 both say *"`ReplaceWith` validates every
replacement **before** removing `this`"*. Both are preserved as written, and taken
literally that design is **wrong**, because half of the validation is
state-dependent on the node that is about to be removed.

Measured: implementing exactly that wording — calling `c->ValidateNode(*n)` plus
the ancestor guard for every replacement before `c->RemoveNode(this)` — makes

```cpp
doc->getRootProperty()->ReplaceWith(newRoot);
```

throw `This document already has a root element.`, because at validation time the
document still holds the root that is being replaced. That is a supported
operation today, it is what .NET does (`XNode.cs:537-547` removes first and then
inserts), and the permanent test
`XLinqMutationConsistencyTests.ReplaceWith_ADocumentRoot_ReplacesIt` fails under
the experiment. The experiment was run and reverted; it is not shipped.

The two ways to keep pre-validation and still get this right were both rejected:
teaching `XContainer::ValidateNode` to ignore a node (a **protected virtual
signature change**, i.e. a vtable change, which §31 does not ask for and this
batch is not authorised to make), or giving `XContainer` a "node being replaced"
member (an **object-layout change**, likewise unauthorised). **The shipped design
removes first, and undoes the removal if any insertion is refused** — which
reaches the acceptance criterion (*"X20 leaves `<a>victim</a>` intact"*) with no
signature, vtable or layout consequence at all, and additionally covers the
cumulative case pre-validation could never see: `ReplaceWith({e1, e2})` on an
`XDocument` root, where both elements validate against a document that has no
root and only the *second* insertion conflicts (permanent test
`ReplaceWith_TwoRootElements_IsRejectedAndRestoresTheOriginalRoot`).

### 36.5 What this ticket did **not** do

- **A replacement that was already attached elsewhere and was inserted before a
  later replacement was refused is left detached, not returned to its previous
  position.** `Add`'s documented move semantics (§14.4, probe case X21) have no
  inverse to roll back to, and restoring one would mean snapshotting every
  container the call could touch — an allocation on every successful
  `ReplaceWith` to improve one failure path. The behaviour is **asserted**, not
  merely documented, by
  `RejectedLaterReplacement_LeavesAMovedReplacementDetached`.
- `std::bad_alloc` from a container's vector growth can still leave a node
  removed from its previous owner and unadopted — the same residue §35.5 records
  for `JsonObject`, for the same untestability reason.
- Nothing owned by #1892 (X15/X17) or #1893 (X27c/X27d) was absorbed.
  **SR-AUD-333 stays `confirmed (design-complete)`**: X15 and X17 are still
  ASan-confirmed use-after-free inside this finding's own files.

### 36.6 Where CCF-019 stands after §31 item 2

| | #1885 baseline | after #1886/#1890 | after #1887/#1891 |
|---|---|---|---|
| ASan `heap-use-after-free` cases | 29 | 3 | **3** (J11, X15, X17) |
| Faulting accesses, recoverable ASan | 57 | 5 | **5** |
| `pure virtual method called` aborts | 8 | 0 | **0** |
| ASan `stack-overflow`s | 3 | 3 | **3** (J19c, X27c, X28c) |
| Timeouts | 2 | 2 | **2** (J19d, X27d) |
| **Data-loss paths** | 2 (J10, X20) | 2 | **0** |
| Leaks | 0 | 0 | **0** |

**Both remaining data-loss paths are closed and neither finding is remediated.**
The residue is J11 (#1889, layout change), J08/J09/J13 (#1888, source break),
X15/X17 (#1892, source break) and the five deep-nesting cases (#1893,
accepted-input change) — §31 items 3, 4, 5 and 6, all still unanswered.

---

## 37. Compatibility review — #1888 (§31 item 3), **not implemented**

*Recorded 2026-07-31 by the batch that landed #1887/#1891. That batch's instruction
grants **no** approval for a public source break, so this section asks whether a
**compatible** repair for J08/J09/J13 exists. Measured answer: **no**, and the
half-repair that looks compatible is measurably worse than the break.*

### 37.1 What each case actually is, after #1886

| Case | Spelling | Still open? |
|---|---|---|
| **J08** | `JsonArray copy = *orig;` (implicit copy ctor) | the **use-after-free is closed** by #1886's `== this` guard; what remains is silent aliasing — two containers hold the same children, and the copy's `parent_` was copied from the original |
| **J09** | `JsonNode& a = *attached; a = *detached;` (implicit copy assign) | open — `parent_` is overwritten on a node that is still stored |
| **J13** | public `DetachParent()` on a still-stored node | open — one node then legitimately enters a second container |

### 37.2 The three candidate compatible repairs, and why each fails

**(a) A parent-preserving user-provided `operator=` on `JsonNode`** — the only J09
repair that keeps every existing spelling compiling. **Measured** with
`build-probe/1888_valuesem_probe.cpp` (a model of `JsonNode`'s exact shape,
compiled `-Wall -Wextra -Wpedantic`):

- it **does** fix J09 — *"attached parent is non-null (CORRECT)"*; and
- it makes the implicitly declared copy **constructor** deprecated, so GCC emits
  `warning: implicitly-declared 'Node::Node(const Node&)' is deprecated
  [-Wdeprecated-copy]` at **every copy-construction site of every derived
  container** (`build-probe/1888_valuesem_compile.log`). `-Wdeprecated-copy` is
  part of `-Wextra`, which this repository builds with and whose non-negotiable
  rule 1 is **zero warnings**. The warning would be emitted from a **public
  header**, i.e. in consumer builds, not only here; and
- it does **not** fix J08 — *"J08 still compiles; copy shares 1 item(s) whose
  parent is the original: YES"*.

A repair that fixes one of three cases and injects a warning into every consumer
that copies is not a compatible repair.

**(b) A deep-cloning user-provided copy constructor on `JsonArray`/`JsonObject`** —
would keep `JsonArray copy = *orig;` compiling and make it well-defined. Rejected:
it silently converts an O(1) alias into an O(tree) deep copy with allocations, and
invents a value semantics **.NET does not have** (`JsonNode` is a reference type;
`DeepClone()` is already the explicit spelling). Changing what an existing
expression *does* is a broad observable semantic change, which this batch's
instruction lists as unapproved alongside source breaks.

**(c) Leaving `DetachParent()` public** — J13 has **no** compatible repair. The
node cannot tell whether a container still stores it, so no runtime check exists;
only visibility can close the door. `XObject` has no equivalent hole because its
detach primitive (`AdoptObject`) is already `protected`.

### 37.3 What §31 item 3 costs the repository, measured

| Question | Measured answer |
|---|---|
| In-repo `DetachParent()` call sites outside the owning header | **1** — `JsonNodeLifetimeTests.cpp:186`, the test written *for* J13 |
| In-repo `JsonNode` copy-construction or copy-assignment sites | **0** |
| Automatic-storage `JsonArray`/`JsonObject` declarations in the repository's own tests | **96** — all unaffected, `= delete` on copy/move does not touch construction |
| Layout / vtable | **unchanged** — `= delete` and a visibility change alter neither |

So item 3's in-repository cost is **one test edit**. Its cost outside the
repository is whatever external code copies a `JsonNode` subclass or calls
`DetachParent()`; that cannot be measured here, because CNA and mobile-eggbert are
outside this batch's boundary and were not inspected.

### 37.4 Verdict

**#1888 stays `needs_user`.** The design in §25 (`= delete` all four value
operations, `DetachParent()` → `protected` + friend) remains the recommendation:
it makes `JsonNode` match `XObject`, which already deletes all four with a comment
explaining why, it is diagnosed at compile time rather than silently, and §37.2(a)
shows the compatible-looking alternative is strictly worse. The approval wording
in §31 item 3 is unchanged and still needed verbatim.

---

## 38. Compatibility review — #1892 (§31 item 5), **not implemented**

*Same authorisation boundary as §37. Measured answer: **no compatible repair
exists**, and — a new finding — **item 5 as currently worded is not implementable
at all**.*

### 38.1 X15 — `Extensions::Ancestors` / `AncestorsAndSelf`

```cpp
template <std::ranges::input_range R>
[[nodiscard]] std::vector<XElement*> Ancestors(const R& source)
```

The returned vector **outlives the full-expression** and **owns nothing**. Probe
X15 assigns it to a named variable, lets the tree die, and reads
`anc[0]->getNameProperty()` — ASan `heap-use-after-free`.

**§14.3's row X-4 says "return owning handles". That is not implementable.**
Measured:

1. `grep -rn "enable_shared_from_this" modules/xml-linq modules/text-json` → **0
   hits**. No node type can produce a `shared_ptr` to itself.
2. The only owning handles to a node live in its parent's `XContainer::children_`.
   The **topmost** ancestor has no parent, so **no owning handle to it exists
   anywhere in the tree**, and `Ancestors` returns exactly that ancestor for every
   input.
3. `XElement`, `XDocument` and `XContainer` are frequently **automatic-storage** —
   **51** such declarations in the repository's own Xml.Linq tests alone — and for
   those objects **no `shared_ptr` exists at all**, so there is nothing to return.

Making X-4 implementable therefore requires adding
`std::enable_shared_from_this` to `XObject` — **+16 bytes on a 16-byte base**,
i.e. exactly §12's rejected candidate and exactly the class of change §31 item 4
is about. **§31 item 5 as written silently depends on a layout change it does not
mention.** It must be restated before it can be approved. The honest restatements
are: *(i)* return `std::vector<std::shared_ptr<XElement>>` and accept that it can
only be produced for ancestors that have a parent, rejecting or omitting the root;
*(ii)* keep `std::vector<XElement*>` and **document** it as a borrowed view valid
only while the tree lives; or *(iii)* add `enable_shared_from_this` under its own
layout approval. Only *(ii)* is compatible, and *(ii)* closes nothing.

### 38.2 X17 — `XElement::getAttributesProperty()`

```cpp
[[nodiscard]] const std::vector<std::shared_ptr<XAttribute>>& getAttributesProperty() const;
[[nodiscard]] std::vector<std::shared_ptr<XAttribute>>        Attributes() const;   // ALREADY EXISTS
```

**A lifetime-safe spelling is already shipped and public.** `Attributes()` returns
the same content **by value**; it is the `.NET`-named twin of the accessor and has
no lifetime hazard. §14.3's parenthetical *"(or is removed in favour of
`Attributes()`)"* is therefore the accurate description of the choice, and it
changes what item 5 is asking for: if `getAttributesProperty()` returned by value
it would become an **exact duplicate** of `Attributes()`.

Compatibility of the by-value change, measured against the ordinary spellings:

| Spelling | After a by-value change |
|---|---|
| `const auto& a = el->getAttributesProperty();` | compiles — lifetime-extended temporary |
| `for (auto& a : el->getAttributesProperty())` | compiles — range-`for` extends the temporary |
| `auto a = el->getAttributesProperty();` | compiles |
| passing it to `const std::vector<…>&` parameter | compiles |
| **`&el->getAttributesProperty()`** (probe X17's exact spelling) | **compile error** — cannot take the address of a prvalue |

So it is *nearly* source-compatible and it fails exactly at the dangling
spelling — but it **is** a **return calling-convention change** (a reference is
returned in a register; a `std::vector` is returned through a hidden `sret`
pointer), and it adds **one allocation plus N atomic increments per call** to an
accessor that is free today. Both are on this batch's unapproved list.

There is also a framing correction worth recording: X17 is the **ordinary C++
reference-lifetime contract** — `std::vector::front()`, `std::string::c_str()` and
every other accessor returning a reference behave identically. What makes X15
different, and worse, is that its returned **object** survives the expression while
its elements are borrowed, and nothing in `std::vector<XElement*>` says so.

### 38.3 X-5 — `XAttribute::setNextAttributeProperty`

`void setNextAttributeProperty(XAttribute* n) { next_ = n; }` is public and writes
the intrusive sibling link directly. In-repo callers are the **three** sites in
`XElement.cpp` that maintain the chain; `~XElement` deliberately writes `next_`
directly instead (#1890), precisely so the destructor does not depend on a setter
item 5 proposes to remove. Making it non-public is a source break by definition
and has no compatible equivalent.

### 38.4 Verdict

**#1892 stays `needs_user`, and §31 item 5 needs rewording before it can be
approved.** Nothing in it is implementable within this batch's boundary: X-3 is a
return-convention change, X-4 is **not implementable as stated** and needs a
layout change to become so, and X-5 is a source break. The two remaining
ASan-confirmed use-after-free cases in CCF-019 are both here, so **SR-AUD-333
cannot be remediated until item 5 is answered.**

---

## 39. Design package — #1889 (§31 item 4), **not implemented, approval-blocked**

*Recorded 2026-07-31. **This prompt grants no approval for an object-layout
change**, and none is claimed. This section completes the durable design so the
decision can be taken from measurement rather than estimate. It corrects §31 item
4's own description of the change.*

### 39.1 Corrected premise — item 4 is **not only** a layout change

§25 and §31 item 4 describe #1889 as *"`sizeof(JsonArray)`/`sizeof(JsonObject)`
grow by 8"* and *"the only layout change proposed"*. Measured
(`build-probe/1889_layout_probe.cpp`, `1889_layout.log`), it is **three** changes,
two of which item 4 does not mention:

```
decltype(JsonArray::begin())  = __gnu_cxx::__normal_iterator<const std::shared_ptr<JsonNode>*,
                                                             std::vector<std::shared_ptr<JsonNode>>>
is vector<shared_ptr<JsonNode>>::const_iterator: YES        sizeof = 8
```

`JsonArray::begin()`/`end()` and `JsonObject::begin()`/`end()` return the
**raw libstdc++ vector iterator**, not a type this project owns. A version-guarded
enumerator cannot be that type, so item 4 additionally requires:

- **a new public iterator type** and therefore a **changed return type** on four
  public functions — a **source break** at every site that spells
  `std::vector<std::shared_ptr<JsonNode>>::const_iterator` explicitly, and a
  behaviour change for every `auto it = arr.begin();`;
- **a silent ABI break**: a function's return type is *not* part of its Itanium
  mangled name, so `_ZNK…9JsonArray5beginEv` keeps its name while changing what it
  returns. An un-recompiled consumer links successfully and misbehaves — strictly
  worse than a name change, which fails at link time.

### 39.2 Exact affected types

| Type | File | Role |
|---|---|---|
| `JsonArray` | `modules/text-json/include/System/Text/Json/Nodes/JsonArray.hpp` | gains the counter; `begin`/`end` change type |
| `JsonObject` | `.../JsonObject.hpp` | same |
| *(new)* `JsonArray::Enumerator`, `JsonObject::Enumerator` | same headers | the version-guarded iterators |
| `JsonNode`, `JsonValue` | `.../JsonNode.hpp`, `.../JsonValue.hpp` | **unaffected** (24 / 40, no counter) |
| `Text.Json` component | `modules/text-json/CMakeLists.txt` | gains a **public** edge → `Collections.Core` |
| `CollectionVersionSeam.hpp`, `test/consumer/*_negative.cpp` | test trees | a new seam and its two mandatory checks |

### 39.3 Layouts, measured before and modelled after

| | before | after | delta |
|---|---|---|---|
| `sizeof(JsonNode)` / `alignof` | 24 / 8 | 24 / 8 | 0 |
| `sizeof(JsonArray)` / `alignof` | **48** / 8 | **56** / 8 | **+8** |
| `sizeof(JsonObject)` / `alignof` | **48** / 8 | **56** / 8 | **+8** |
| `sizeof(JsonValue)` / `alignof` | 40 / 8 | 40 / 8 | 0 |
| `sizeof(JsonArray` iterator`)` | **8** | **24** | **+16 (×3)** |
| `sizeof(detail::MutationCounter)` | — | 8 | — |
| `sizeof(detail::MutationVersion)` | — | 8 | — |

Current field order is `vptr` (8), `JsonNode::parent_` (8), `options_` (8),
container (24) = 48 with no tail padding, so the counter appends at offset 48 and
`alignof` is unchanged. The iterator must carry the wrapped `__normal_iterator`
(8), an owner back-pointer (8) and the snapshot (8) = 24.

### 39.4 Vtable, symbols, class dumps

- **No vtable change.** No virtual function is added, removed or reordered; the
  counter is a data member and the enumerator is a non-polymorphic class.
- **Symbols.** Four inline member functions (`begin`/`end` × 2) keep their mangled
  names with different return types (§39.1); the new `Enumerator` types add weak
  COMDAT symbols for their members; `sizeof` changes are not encoded in any
  symbol. Reproduce with `build-probe/1886_layout_build.sh` and the
  `1886_extsyms_*.txt` procedure, which already exists.
- **Class dumps.** `-fdump-lang-class` before/after is the required evidence, as
  #1886 used; not produced here because nothing was implemented.

### 39.5 Stale-iterator behaviour, before and after

| Case | Today | After |
|---|---|---|
| **J11** — iterator held across 64 `Add`s | ASan `heap-use-after-free`, `SIGSEGV` without a sanitizer | `InvalidOperationException` on dereference |
| **J12** — `JsonObject` iterator held across `Clear()` | returns `"a"` from destroyed storage, **no diagnostic in any build** | `InvalidOperationException` |
| iterator held across a non-mutating traversal | works | works, unchanged |
| iterator outliving the **container** | undefined (the container's storage is gone) | **still undefined** — the snapshot lives in the iterator but the counter lives in the freed container, so the comparison itself reads freed memory |

That last row is the design's honest limit and it must be in the approval:
**a version counter closes mutation-invalidation, not owner-destruction.** J11's
probe case destroys nothing, so it is closed; a hypothetical "iterator outlives
its array" case is not, and would need §12's rejected shared-state candidate.

### 39.6 The six alternatives, evaluated

| Alternative | Layout cost | Closes J11/J12 | Verdict |
|---|---|---|---|
| **Counter in the container + snapshot in the iterator** (§25's proposal, and the contract 14 other collections in this repository already use) | +8 container, +16 iterator, new iterator type | **yes / yes** | the recommendation, *if* item 4 is approved |
| Counter in the container only, no snapshot | +8, no iterator change | **no** — nothing to compare against | useless; recorded because it is the tempting "smaller" version |
| **Shared invalidation state** (`shared_ptr<State>` co-owned by container and iterators, as `SortedSet<T>` uses) | +8 container, +16 iterator, **plus one allocation per container** | yes / yes, **and** survives owner destruction | strictly more capable and strictly more expensive; the only candidate that also closes the §39.5 last row |
| **Heap token** (container owns a `shared_ptr<bool>`, iterator holds a `weak_ptr`) | +8 container, +16 iterator, one allocation per container | yes / yes | equivalent to shared state with a worse name; no advantage |
| **Snapshot iterator** (`begin()` returns an iterator over a *copy* of the storage) | 0 container, iterator holds a `shared_ptr<vector>` | yes / yes | **no layout change**, but changes iteration semantics: mutations stop being visible to a live loop, and every `begin()` allocates and copies the whole container. A different contract, not a repair |
| **Registry / ownership indirection** | 0 container, global map | yes / yes | a process-wide side table on every mutation; rejected on cost and thread-safety |
| **Fail-fast vs lifetime-safe continuation** | — | — | .NET fail-fasts (`InvalidOperationException`). Recommended, but §39.7 records why .NET's choice does not by itself justify a C++ layout change |

### 39.7 Why .NET's fail-fast does not settle it

.NET's `List<T>`/`Dictionary<K,V>` enumerators throw
`InvalidOperationException: Collection was modified` — but in .NET the version
field costs nothing observable: there is no `sizeof` contract, no ABI, and the
enumerator is a `struct` whose layout no consumer depends on. In C++ all three
are observable, and the iterator type here is a **standard library type this
project does not own**, so matching .NET's behaviour costs a public type change
that .NET never had to make. **Parity is a reason to prefer the behaviour, not a
reason to accept the cost.**

### 39.8 Allocation and performance

- Counter+snapshot: **zero allocations**; one increment per mutation, one
  comparison per dereference; `+8` per container and `+16` per iterator.
- Shared state / heap token: **one allocation per container**, on a type
  constructed 96 times in this repository's own tests alone.
- Snapshot iterator: **one allocation and a full container copy per `begin()`**.
- Hot-path measurement required before landing: `1886_alloc_build.sh`'s
  `operator new`/`delete` counting over construction, traversal and mutation, as
  #1886 used.

### 39.9 Copy / move / destruction consequences

`detail::MutationCounter` deliberately has **no assignment operator that copies
the source's value** (`docs/CollectionVersionCounterSweep.md`): assigning one
container to another must not transplant the source's version. `JsonArray` and
`JsonObject` currently have implicitly generated copy operations (probe J08), so
adding the counter interacts with #1888 — **#1889 should land after #1888, not
before**, or the counter's assignment semantics have to be reasoned about for an
operation #1888 is going to delete anyway.

### 39.10 Migration and rollback

- **Migration.** Consumers must recompile (mandatory, and *silently* mandatory —
  §39.1). Any consumer that spells the iterator type explicitly must edit; any
  consumer that stores a `JsonArray`/`JsonObject` by value in a layout-sensitive
  structure must re-check. CNA and mobile-eggbert were **not inspected**; #1773
  remains `blocked`.
- **Rollback.** Remove the two members, the two enumerator types and the four
  `begin`/`end` bodies; revert `modules/text-json/CMakeLists.txt` and regenerate
  `docs/generated/ComponentCatalog.md`; remove the seam entry and the negative
  consumer sites. No data migration; no persisted format.

### 39.11 Permanent test and mutation matrix

| Test | Pins |
|---|---|
| J11 — `JsonArray` iterator across `Add` (reallocating and non-reallocating) | throws `InvalidOperationException` |
| J12 — `JsonObject` iterator across `Clear` | throws |
| iterator across `Insert`/`RemoveAt`/`Remove`/`SetItem`/`Add` (both containers) | every mutating entry point bumps |
| iterator across a **non**-mutating traversal, `ToJsonString`, `DeepClone`, `getCountProperty` | does **not** throw |
| `begin() == end()` on an empty container | works |
| two live iterators, one mutation | both invalidate |
| `static_assert(sizeof(JsonArray) == 56)` etc. | the approved layout, not an accidental one |
| counter wrap (`MutationVersion` at max) | the documented ABA horizon |
| `scripts/check_version_seam_odr.py` + `test/check_version_seam_odr_test.py` | 2 seams → **3** |
| `test/consumer/text_json_node_version_negative.cpp` | the private counter is unreachable from a consumer |
| **Mutations** | remove the bump from each mutator (must fail ≥1 each); remove the comparison from the dereference (must fail every stale case); widen/narrow the snapshot (must fail the wrap test) |

### 39.12 Precise approval wording required

> Approve, for `System::Text::Json::Nodes::JsonArray` and `JsonObject`:
> **(1)** adding a `System::Collections::detail::MutationCounter` member, growing
> `sizeof` from **48 to 56** for both (`alignof` unchanged at 8, no vtable change);
> **(2)** replacing `begin()`/`end()`'s return type — today the raw
> `std::vector<…>::const_iterator`, `sizeof` **8** — with a new project-owned
> version-guarded enumerator, `sizeof` **24**, which is a **public source break**
> at every site that spells the iterator type and a **silent ABI break** at
> `begin()`/`end()`, whose mangled names do not change;
> **(3)** a stale dereference throwing `System::InvalidOperationException` instead
> of reading freed or destroyed storage;
> **(4)** a new **public** component edge `Text.Json → Collections.Core`, taking
> the module graph from **91 to 92** edges and requiring the generated catalogue to
> be regenerated;
> **(5)** a third version seam, with both its mandatory checks.
>
> This does **not** close the case of an iterator outliving its *container* (§39.5),
> which no counter design can reach.

**#1889 stays `needs_user`.** It should land **after** #1888 (§39.9).

---

## 40. Case classification — #1893 (§31 item 6), **not implemented**

*Recorded 2026-07-31. Item 6 is an accepted-input change and is unapproved, so
this section classifies each case by **root cause** — which §25 never did, it
grouped all five under "deep-tree and deep-parse bounds" — and corrects one
premise. No production change was made.*

### 40.1 The five cases are three different defects, not one

| Case | Symptom | Root cause, measured | Reachable from |
|---|---|---|---|
| **J19c** | ASan `stack-overflow` on *release* of a 20,000-deep `JsonArray` nest | **recursive destruction.** The ASan frames are `~JsonArray` → `std::destroy_at` → `_Sp_counted_ptr_inplace::_M_dispose` → `_Sp_counted_base::_M_release` → `~JsonArray`, one stack frame per level (`1891_postfix_asan.log`) | programmatic `Add` only |
| **X27c** | the same for a 20,000-deep `XElement` nest | the same recursive destruction through `children_` | programmatic `Add` only |
| **J19d** | timeout at 100,000 deep | **quadratic construction.** `JsonNode::AssignParent`'s cycle guard walks the *whole* ancestor chain on every attach — O(depth²) | programmatic `Add` only |
| **X27d** | the same | **quadratic construction.** `XContainer::InsertNodeAt`'s ancestor-or-self guard walks the whole chain on every insert | programmatic `Add` only |
| **X28c** | ASan `stack-overflow` while parsing 20,000 nested arrays | **recursive tree building on untrusted text.** The frames sit in the port's own `fromNlohmann` recursion allocating `JsonArray`s, not in a mutation path | **`JsonNode::Parse` — untrusted input** |

None is cyclic ownership (the guards prevent cycles and LSan is clean), none is
reentrant mutation, none is malformed-tree acceptance, none is algorithmic
non-termination (J19d/X27d terminate, just not within the 5-second watchdog), and
none is a probe limitation — the probe uses only public API and each case runs in
its own process.

**Only X28c is reachable from untrusted input**, and it is the only one of the
five that is a security-shaped defect rather than a robustness-shaped one. The
other four require a program that deliberately builds a 20,000-deep tree.

### 40.2 Corrected premise — the depth bound already exists in this module

§31 item 6 presents bounding `JsonNode::Parse` as a new grammar decision, to be
*"justified against .NET's own `JsonReaderOptions.MaxDepth` (default 64)"*. The
in-repository justification is stronger and was not recorded: **this module
already has that bound, already public, already applied.**

- `modules/text-json/include/System/Text/Json/JsonDocumentOptions.hpp`:
  `static constexpr intcs DefaultMaxDepth = 64;`
- `modules/text-json/include/System/Text/Json/JsonDocument.hpp`: `Parse` calls
  `checkMaxDepth(*parsed, 1, effectiveMaxDepth)` and throws
  `JsonException("The maximum configured depth of N has been exceeded…")`.
- `JsonTests.ParseExceedingDefaultMaxDepth_Throws` and
  `…ExceedingCustomMaxDepth_Throws` already pin it.

So the two JSON parse entry points in one module disagree about the same
untrusted text: `JsonDocument::Parse` rejects beyond 64, `JsonNode::Parse`
crashes the process at 20,000. Item 6's X28c half is therefore better described
as **making `JsonNode::Parse` apply the bound its sibling already applies**, with
the same constant and the same exception type — a consistency repair, not a new
grammar. It still changes accepted input and still needs item 6's approval, but
the decision being asked for is much smaller than §31 states, and no new option
field is needed (`JsonNodeOptions` can stay at its current layout by using
`JsonDocumentOptions::DefaultMaxDepth` directly).

### 40.3 What each root cause would actually need

| Cause | Repair | Approval class |
|---|---|---|
| X28c — recursive parse on untrusted text | apply `JsonDocumentOptions::DefaultMaxDepth` in `fromNlohmann`, raising the existing `JsonException` | **accepted-input change** (item 6) |
| J19c / X27c — recursive destruction | iterative teardown in `~JsonArray`/`~JsonObject`/`~XContainer`: move children into a local worklist and unwind it in a loop | **none** — no signature, layout or accepted-input change; behaviour is identical for every tree that does not overflow today |
| J19d / X27d — quadratic guards | the ancestor walk is O(depth) per attach by construction. Making it O(1) needs a depth or root cached **in the node**, i.e. **+8 on `JsonNode` (24) and on `XObject` (16)** | **object-layout change** — item 4's class, which item 6 does not mention |

That third row is the second corrected premise: §31 item 6 says *"No signature,
layout or vtable change"*, but bounding the guard cost is only achievable by
bounding **depth** (rejecting deep trees at `Add`, another accepted-input change)
or by **caching depth in every node** (a layout change). Item 6 as written cannot
deliver J19d/X27d.

**A global recursion limit is deliberately not proposed** for J19c/X27c: the
reference contract does not require one, an iterative teardown removes the
failure entirely rather than converting it into a different one, and .NET's own
`XContainer`/`JsonNode` impose no teardown depth limit.

### 40.4 Verdict

**#1893 stays `needs_user`**, and it should be **split** before it is answered:
its J19c/X27c half needs no approval at all and could land on its own; its X28c
half is item 6's real content, reduced to a consistency repair by §40.2; and its
J19d/X27d half cannot be delivered under item 6's stated no-layout-change
constraint. None of the three was started.

---

## 41. Implementation record — #1895, iterative container teardown (2026-07-31)

*Everything in this section is **measured**. Nothing in §§1–40 was rewritten.*

### 41.0 What authorised it

The user's decision of 2026-07-31 split §31 item 6 into root-cause tickets and
**explicitly approved the compatible iterative-teardown half (probe cases J19c
and X27c)**, on condition that it requires no public signature change, no virtual
or vtable change, no public object-layout change, no iterator-layout change, no
mandatory consumer source migration, and no weakening of the existing
`DefaultMaxDepth = 64` contract. §41.4 shows each condition is met. The other
three halves of the old #1893 are now #1896 and #1897 and remain blocked.

### 41.1 The defect, restated precisely

Releasing an owned tree recursed. `~JsonArray` released `items_`, which dropped
the last `shared_ptr` to each child, which ran that child's destructor — **one
call frame per level**. Measured frames for J19c:

```
#0 JsonArray::~JsonArray()            JsonArray.hpp:42
#1 std::destroy_at<JsonArray>
#2 std::_Destroy<JsonArray>
#3 allocator_traits<...>::destroy<JsonArray>
#4 _Sp_counted_ptr_inplace<JsonArray>::_M_dispose()
#5 _Sp_counted_base<_S_atomic>::_M_release()      → back to #0
```

So a 20,000-deep nest crashed **after it had been built successfully**, and the
depth at which it crashed was a function of the thread's stack size — not of
anything the program could observe or bound. X27c is the identical shape through
`~XContainer` and `children_`.

This is **recursive destruction**, and §40.1 distinguishes it from the four other
recursive shapes in these families, none of which this ticket touches:
`fromNlohmann` (parsing, X28c → #1897), `toNlohmann`/`SerializeTo` (formatting),
`DeepClone` (cloning), `CollectDescendants`/`CollectDescendantNodes`
(traversal), and `AssignParent`/`InsertNodeAt`'s ancestor walks (ownership
validation, quadratic rather than deep → #1896).

### 41.2 The repair

Three destructors, in two bodies:
`modules/text-json/src/System/Text/Json/Nodes/JsonNode.cpp` (`~JsonArray`,
`~JsonObject`) and `modules/xml-linq/src/System/Xml/Linq/XContainer.cpp`
(`~XContainer`).

> The **outermost** container destructor publishes a worklist; every container
> destructor that runs while a worklist is published **donates** its own children
> to it and returns instead of releasing them. Stack depth is constant; the bound
> is the heap.

```cpp
JsonArray::~JsonArray() {
    for (const auto& item : items_)
        if (item && item->getParentProperty() == this) item->DetachParent();  // #1886, unchanged

    if (items_.empty()) return;                       // leaf containers cost nothing
    if (pendingRelease != nullptr) {                  // a teardown is draining: donate
        donateChildren(items_, *pendingRelease);
        return;
    }
    std::vector<std::shared_ptr<JsonNode>> worklist;  // outermost: publish and drain
    PendingReleaseScope scope(worklist);
    donateChildren(items_, worklist);
    drainPendingRelease(worklist);
}
```

Five properties, each deliberate:

1. **`thread_local` worklist pointer.** The worklist belongs to one in-progress
   teardown on one thread. It is never shared between threads, so it needs no
   synchronisation and introduces **no race** — two threads tearing trees down
   simultaneously each publish their own. TSan was therefore not run and is not
   required (§41.5).
2. **Donation is last-child-first, and the worklist is drained from its back.**
   That is what reproduces the recursive teardown's front-to-back depth-first
   order rather than merely terminating (§41.3).
3. **The `== this` detach guard runs first, unchanged.** #1886's contract is
   untouched: a child owned by a different container keeps that container's link,
   and the donate path never bypasses it — every node's children are detached by
   that node's own destructor before they reach the worklist.
4. **Allocation failure is contained, not converted into `terminate`.** The only
   way donation can fail is `std::bad_alloc` from the worklist's growth. It is
   caught inside the destructor; whatever is still in the store keeps its
   (already detached) children and is released by the store's own destructor —
   i.e. **exactly the recursive behaviour this repair replaces**. Nothing is
   lost, leaked, double-detached or released twice, because the worklist and the
   store never hold the same `shared_ptr`: each one is *moved* across. Partial
   progress is therefore safe at every point.
5. **Growth is geometric, not exact.** Reserving the exact requirement on every
   donation would make a container that repeatedly refills a full worklist
   reallocate on each one — trading a stack-depth problem for a quadratic-copying
   one. The reserve doubles instead (§41.4 measures the result).

**A retained node stops the teardown dead**, with no `use_count` check needed:
releasing the worklist's reference to a node someone else still holds simply
decrements, the destructor does not run, and the node keeps its whole subtree.
When its real owner later releases it, no worklist is published, so it publishes
its own.

### 41.3 Measured evidence

**Probe.** The retained 58-case probe was rebuilt from its tracked source and run
unmodified. The pre-change replay reproduced the previous batch's end state
exactly — **0 of 58 cases changed** — so this batch's baseline was verified, not
inherited (`build-probe/1895_prefix_asan.log`).

After the change, **exactly 2 of 58 cases changed**, and they are the two the
user approved:

| Case | Before | After |
|---|---|---|
| **J19c** — 20,000-deep `JsonArray`, build then release | `ASAN stack-overflow`; without a sanitizer `-> built` then **KILLED by signal 11** | **`clean`** — `-> built, released` |
| **X27c** — 20,000-deep `XElement`, build then release | `ASAN stack-overflow`; **KILLED by signal 11** | **`clean`** — `-> built, released` |
| J19d, X27d | `TIMEOUT` | **`TIMEOUT`** — unchanged (#1896) |
| X28c | `ASAN stack-overflow` | **`ASAN stack-overflow`** — unchanged (#1897) |
| J11, X15, X17 | `heap-use-after-free` | **`heap-use-after-free`** — unchanged (#1888/#1898) |

Diffing every answer line of the **no-sanitizer** build across all 58 cases
yields exactly those two differences plus ASLR address noise in the three cases
that print a pointer (`build-probe/1895_postfix_none.log`).

**J11, X15 and X17 do not belong to this repair, and that is now measured rather
than argued:** the teardown changed neither their classification nor their answer
lines, and none of the three reaches its defect through a destruction path — J11
is a stale iterator over reallocated storage, X15 a borrowed raw `XElement*`,
X17 a borrowed reference to a vector member.

**Permanent tests.** 34 new cases:
`modules/text-json/tests/System/Text/Json/Nodes/JsonNodeTeardownTests.cpp` (17)
and `modules/xml-linq/tests/System/Xml/Linq/XLinqTeardownTests.cpp` (17). They
cover the deep chain at the probe's own depth (20,000) for `JsonArray`, for a
mixed `JsonObject`/`JsonArray` chain, for `XElement`, under an `XDocument`, in
**automatic storage** (where the outermost container is not heap-owned), and
released through `Clear()`/`RemoveNodes()` (where the *child* rather than the
root is the outermost teardown); the empty, singleton, wide (10,000 siblings),
wide-of-wide (200×200) and mixed-node-kind shapes; 5,000 attributes, asserting
that the attribute side is deliberately not part of the worklist because an
`XAttribute` owns no children; #1886/#1890's invariants (retained child detached
and re-attachable, retained subtree intact, foreign child keeps its real owner,
retained attribute's `next_` cleared); destruction during live exception
unwinding; `is_nothrow_destructible_v` for all seven types; and `weak_ptr`
observers proving every node is genuinely destroyed rather than merely detached.

**Destruction order is pinned, not assumed.** Two tests assert the exact order
(`{0,1,2,3,4,5}` for a branching tree, root-to-leaf for a chain) using a
test-local `JsonArray`/`XElement` subclass that records its own destruction. Both
tests **pass under the recursive implementation as well** — that is what makes
them a proof of order *preservation* rather than a description of the new
behaviour.

**Mutation tests.** Reverting each family's teardown to the recursive form:

| Mutation | Detected by |
|---|---|
| `~JsonArray`/`~JsonObject` recursive again | **4** of 17 — all four as `SIGSEGV`, the exact pre-fix failure |
| `~XContainer` recursive again | **4** of 17 — all four as `SIGSEGV` |

Run one test per process so a crash cannot hide the rest. The order tests are in
the surviving 13 in both runs, as noted above.

**Sanitizers.** `build-asan` (`-fsanitize=address,undefined`, `detect_leaks=1`,
`print_stacktrace=1`), both binaries proved newer than both changed bodies
(`build-probe/1895_asan_freshness.log`): **218/218 Text.Json** and **170/170
Xml.Linq**, **zero** ASan, UBSan or LeakSanitizer diagnostics. The deep cases run
under ASan too, which uses more stack per frame than the plain build — so the
bound really is gone rather than merely raised.

### 41.4 Source, ABI, layout, allocation

| Condition the approval set | Result |
|---|---|
| No public signature change | **met.** `~JsonArray`, `~JsonObject` and `~XContainer` keep their declarations; only the *definition* moved out of line, because the JSON worklist is shared by two destructors whose headers cannot see each other's store |
| No virtual or vtable change | **met.** No virtual function added, removed or reordered; all three destructors were already virtual through their base |
| No public object-layout change | **met.** No member added anywhere; the 23 `static_assert`s in the existing lifetime suites still hold |
| No iterator-layout change | **met.** No iterator touched |
| No mandatory consumer source migration | **met.** No consumer source edit; consumers recompile (header change) and relink |
| No weakening of `DefaultMaxDepth = 64` | **met.** No parse path touched at all |

**ABI, measured** (`nm --defined-only --extern-only`, `-O2`, pre-fix bodies and
headers extracted from `72ed51b3`):

- `JsonNode.o`: **225 external/weak defined symbols before, 225 after, the same
  names.** The six destructor symbols
  `_ZN…9JsonArrayD0Ev`/`D1Ev`/`D2Ev` and `_ZN…10JsonObjectD0Ev`/`D1Ev`/`D2Ev`
  change **binding from `W` (weak COMDAT) to `T` (strong)** — the expected and
  only consequence of an out-of-line destructor. **No name added or removed.**
- `XContainer.o`: **49 → 62.** Added are `_ZN…10XContainerD0Ev`/`D1Ev`/`D2Ev` as
  `T`; the typeinfo, type-name and vtable objects for `XContainer`, `XNode` and
  `XObject`, because this translation unit is now the key-function TU; and
  `ValidateNode`'s weak COMDAT plus two `std::vector` destructor instantiations.
  **No name was removed**, and every addition is weak or a definition site that
  consumers already emitted for themselves.
- `build-probe/1895_{JsonNode,XContainer}_{prefix,postfix}.syms`.

**Allocation, measured with the retained `1886_alloc_probe`**, against the
`#1886`/`#1890` baseline it recorded:

| Phase | Before | After |
|---|---|---|
| `JsonArray` construct + 4 `Add` + retain child | 16 new | **16 new** — unchanged |
| 100,000 × (`getParentProperty` + `getRootProperty`) | 0 new | **0 new** — unchanged |
| `JsonArray` destruction (4 owned children) | **0 new** | **1 new** |
| `XElement` construct + 4 nodes + 2 attributes | 12 new | **12 new** — unchanged |
| 100,000 × (`getParentProperty` + `getDocumentProperty`) | 0 new | **0 new** — unchanged |
| `XElement` destruction (4 children, 2 attributes) | **0 new** | **1 new** |

**This is the one cost the repair has, and it is recorded rather than glossed:
destroying a container that has at least one child now performs exactly one heap
allocation** — the worklist — where it previously performed none. An **empty**
container still allocates nothing (the early return), and the deep chain that
motivated the ticket allocates **once for the whole 20,000-level teardown**,
because the worklist's capacity is reached immediately and never grows again. The
first draft of this repair allocated **three** times for four children (geometric
growth from empty); the geometric `reserve` in §41.2 property 5 reduced that to
one, and the measurement above is after that change.

### 41.5 What this ticket did **not** do

- **TSan was not run.** The only new mutable state is a `thread_local` pointer,
  which is per-thread by construction and is never published to another thread.
  No shared mutable state, atomic, cache or cross-thread ownership was introduced.
- **Reentrancy is bounded but not zero-cost.** If an unrelated container is
  destroyed *while* a worklist is published — for example from a destructor
  running inside the drain — it donates to that worklist, so its nodes are
  released a little later, inside the outer loop, rather than immediately. The
  set of objects destroyed is identical; only the interleaving differs. Recorded
  rather than hidden.
- **J19d/X27d (quadratic ancestor guards) and X28c (recursive parse) are
  untouched**, by design: they are #1896 and #1897.
