# Audit: `modules/xml-linq/include/System/Xml/Linq/XObject.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-333 — high — retained Xml.Linq children dereference a destroyed raw parent

Every XObject stores its parent as a non-owning raw XContainer pointer while
children are public shared_ptr values. Retaining a child does not retain its
parent. The standalone ASan/UBSan probe attaches XText to an XElement, releases
the sole parent shared_ptr, then calls getParentProperty on the retained child.
ASan reports heap-use-after-free in XObject::getParentProperty.

### Correction appended by ticket #1885 (2026-07-30) — design-only, still `confirmed`

The paragraph above is retained as the historical record. Measured against the
shipped bodies (same probe and freshness guarantee as SR-AUD-327's correction):

- **The severity is understated.** `XObject::getParentProperty` does not merely
  read a field: it dispatches a **virtual call** (`parent_->getNodeTypeProperty()`)
  through the freed parent. **Eight** public entry points therefore abort the
  process with `pure virtual method called` and SIGABRT **with no sanitizer
  present at all**: `getParentProperty`, `getDocumentProperty`, `ReplaceWith`,
  `XAttribute::getParentProperty`, `getPreviousAttributeProperty`,
  `XAttribute::Remove`, and `getDocumentProperty` reached through a destroyed
  `XDocument` at one and two levels.
- **The measured surface is 35 public entries across 13 files**, not the six the
  index names. **21** ASan `heap-use-after-free` accesses (X01–X15, X17–X19, X22,
  X25, X26). X01: `READ of size 8` at `0x50d000000050`, faulting frame
  `XObject::getParentProperty() const` at `XObject.cpp:12`, allocated by
  `make_shared<XElement>`, freed by `~shared_ptr<XElement>`, exit status 1, no
  timeout.
- **`XAttribute::next_` is a second, independent unguarded borrowed link**, with a
  **public** setter. A retained attribute returns a freed sibling from
  `getNextAttributeProperty()` and reading its name silently succeeds.
- **`XNode::ReplaceWith` destroys data on its exception path**: it removes `this`
  before inserting, so a rejected replacement leaves the original node gone from
  the tree. Measured: `<a/>` where `<a>victim</a>` was expected.
- **Two borrowed views outlive their owner and the parent-link repair does not
  reach either**: `getAttributesProperty()`'s reference (safe across
  reallocation — it refers to the vector *object* — but an ASan-confirmed
  use-after-free once the element dies) and `Extensions::Ancestors`/
  `AncestorsAndSelf`'s `std::vector<XElement*>`.
- **Two shapes measured here are safe and are recorded as such** so no repair
  breaks them: `XContainer::Nodes()` and `XElement::Attributes()` return by value
  and are true snapshots, and `XElement::Parse` correctly detaches its temporary
  `XDocument` root. `XDocument::Parse` is additionally already bounded against
  deep nesting by tinyxml2 (`XML_ELEMENT_DEPTH_EXCEEDED`), unlike the JSON parser.
- **`Add` moves an already-attached node where .NET clones it** (`XContainer.cs:512`,
  `XElement.cs:1902`). This is the authorised documented deviation, re-measured
  and recorded here so no lifetime repair silently changes it.

**No new `SR-AUD-*` identifier was issued**; numbering stays frozen at **364**.
Selected repair, rejected alternatives, cycle and destruction proofs and the exact
approval request: `docs/OwnedTreeLifetimeContractPlan.md`. Implementation is
#1890/#1891/#1892/#1893/#1894, all `needs_user` or `blocked`. **Nothing has been
implemented; SR-AUD-333 remains `confirmed`.**

### Correction appended by ticket #1890 (2026-07-31) — the approved core repair landed; finding still `confirmed`

Both blocks above are retained as the historical record. `XContainer` now
declares a destructor that clears the parent link of every child node that still
names *that* container, and `XElement` one that clears every owned attribute's
parent link **and** its intrusive `next_` sibling link, each before its store is
released (`docs/OwnedTreeLifetimeContractPlan.md` §14, §31 item 1, §34). Measured
by re-running **the same** probe `build-probe/1885_ccf019_lifetime_probe.cpp`,
unmodified, in the same three builds from one source:

| | before (#1885) | after (#1890) |
|---|---|---|
| XObject cases producing an ASan `heap-use-after-free` | 21 (X01–X15, X17–X19, X22, X25, X26) | **2** (X15, X17 only) |
| XObject faulting accesses, recoverable ASan | 48 | **4** |
| Public entry points aborting with `pure virtual method called` | **8** | **0** |
| Retained node: `getParentProperty()` / `getDocumentProperty()` | SIGABRT | **`nullptr`** |
| Retained node: sibling navigation | UAF / SIGSEGV | **`nullptr`, empty** |
| Retained node: `Remove()` / `ReplaceWith()` | UAF on a mutating path, SIGABRT | **throws `The parent is missing.`** |
| Retained attribute: `getNextAttributeProperty()` (X13) | a freed `XAttribute*` whose name read as `"b"` | **`nullptr`** |
| Retained node re-`Add`ed to a live element (X14) | UAF at `XContainer.cpp:40` | **succeeds cleanly** |
| Allocator reuse (X22) | stale parent reported the squatter's name | **`nullptr`, no name** |
| `sizeof` XObject/XNode/XContainer/XElement/XAttribute/XText/XDocument | 16/16/40/128/120/48/56 | **unchanged** |
| Allocations added to construction / access / destruction | — | **0 / 0 / 0** |

Every one of X01–X14, X18, X19, X22, X25 and X26 now produces exactly the answer
a **detached** object already produced, which is the contract §14.2 specified.

**Residual exposure, all of it approval-blocked and none of it closed by this
ticket:**

- **X15** — `Extensions::Ancestors`/`AncestorsAndSelf` return `std::vector<XElement*>`,
  raw handles that still outlive the tree; **X17** — `getAttributesProperty()`
  returns a reference to the element's own vector, which still outlives the
  element. Both are still ASan-confirmed use-after-free and both are ticket
  **#1892** (public source break; §31 item 5). The parent-link repair cannot
  reach either: neither goes through `parent_`.
- **X20** — `XNode::ReplaceWith` still removes `this` before it validates the
  replacements, so a rejected replacement still loses the original node
  (measured again post-fix: `<a/>` where `<a>victim</a>` was expected). Ticket
  **#1891** (§31 item 2).
- **X27c / X27d** — a 20,000-deep element nest still overflows the stack on
  release and a 100,000-deep one still times out. Ticket **#1893** (§31 item 6).
- **X21** — `Add` still **moves** an already-attached node where .NET clones it.
  That is the authorised documented deviation and is deliberately unchanged
  (`docs/OwnedTreeLifetimeContractPlan.md` §14.4, §30.2).
- **SR-AUD-336** — `Changed`/`Changing` remain inert; a separate `confirmed`
  finding, untouched.

One honest note on the ownership guard. Each destructor clears a link only if it
still names the destroying object. On the JsonNode side that guard is
load-bearing and mutation-detectable. Here it is **not currently reachable to
violate**: `XContainer::InsertNodeAt` and `XElement::Add(attribute)` both *erase*
the object from its previous owner's vector before adopting it, and `XObject`
deletes all four copy/move operations, so a container never holds an object whose
`parent_` names someone else. Removing the guard fails **no** test, and that is
recorded rather than papered over — the guard is retained because §31 item 1
specifies it, because it makes the destructor's contract explicit, and because
#1891/#1892 may change insertion. The invariant it depends on is itself pinned by
`NodeMovedToAnotherOwner_…` and `AttributeMovedToAnotherOwner_…`.

Because X15 and X17 are still ASan-confirmed use-after-free inside this finding's
own files, **SR-AUD-333 stays `confirmed (design-complete)`** and the post-audit
total is unchanged. No new `SR-AUD-*` identifier was issued; numbering stays
frozen at **364**.

## SR-AUD-336 — medium — Changed and Changing accept handlers but never notify mutations

The declared event accessors discard each handler. The direct probe registers
Changed on an XElement, calls setValueProperty, and prints
changed-event-fired=0. The focused test only asserts that registration does
not throw, thereby preserving the inert behavior.

## Missing assertions and diagnostics

- Add lifetime regressions for retained nodes and attributes after a parent or
  document is destroyed; every navigation/mutation path must be safe.
- Make Changed/Changing delivery, sender, change kind, ordering, unregister,
  and reentrant mutation behavior observable in the eventual repair.

## Final assessment

Confirmed SR-AUD-333 and SR-AUD-336.
