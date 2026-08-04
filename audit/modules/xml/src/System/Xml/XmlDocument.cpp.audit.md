# Audit: `modules/xml/src/System/Xml/XmlDocument.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-352 — medium — XmlDocument node-change events are publicly exposed but never raised

The document retains NodeInserted/NodeRemoved/NodeChanging/NodeChanged handler collections, yet DOM mutation paths do not dispatch them.  A handler installed before an element insertion records zero events in the direct probe (`node-inserted-events=0`).  Consumers cannot observe the documented mutation notification surface.

## Remediation record — #2079 (2026-08-04)

**REMEDIATED.** Evidence above retained; this appends what implementation measured. Plan:
`docs/SystemXmlNamespaceReviewPlan.md` §4.6 and §20.6. Reproduction:
`build-probe/2079_probe1_events.cpp`; the **before** log was produced by compiling the
pre-#2079 bodies **from source** into the probe (`2079_probe1_before.log`), the after log by
the repaired ones (`2079_probe1_after.log`).

**Sixteen public mutation doors, zero dispatches.** The finding names three
(insert/remove/value change); the probe enumerated sixteen and all were silent.

Now dispatched, grouped by the rule that decides them — *every mutation whose affected node
survives the operation*:

| Pair | Doors |
|---|---|
| Insert | `PrependChild`, `AppendChild`, `InsertBefore` (both branches), `InsertAfter`, `ReplaceChild`, `InnerXml =`, `InnerText =`, and once per child when an `XmlDocumentFragment` is appended or prepended |
| Remove | `RemoveChild`, `ReplaceChild`'s removal half, and once per child as it leaves a fragment |
| Change | `XmlCharacterData::setDataProperty` — the single choke point `AppendData`/`InsertData`/`DeleteData`/`ReplaceData`/`setValueProperty`/`setInnerTextProperty` all reach — plus `XmlAttribute::setValueProperty`, `XmlDeclaration::setValueProperty`, `XmlProcessingInstruction::setDataProperty` |

`setInnerXml` needed its **own** dispatch: it inserts natively rather than through
`AppendChild`, so it would otherwise have been the one insert door still silent while
`setInnerText` (which routes through `AppendChild`) fired.

**The corrected premise held and did real work.** The handlers are public **data members**,
so the dispatcher reads them directly and lives in `modules/xml/src/System/Xml/
XmlNodeChangeEvents.hpp` — a `src`-only internal header, placed exactly where the module
already keeps `XPath/XPathAstInternal.hpp`, so **no public surface, type, signature, layout or
vtable changed**.

**One door deliberately stays silent, and it is a lifetime decision.**
`XmlNode::RemoveAllChildren` (reached by `RemoveAll`) `PurgeCache`es every wrapper and
`DeleteChildren`s the natives, so a `NodeRemoved` handler's `XmlNode*` would name **freed
storage** — precisely the borrowed-pointer defect CCF-019 tracks. Completing an event pair is
not worth introducing it. The silence is documented on `XmlDocument`, **pinned by a test**,
and tracked as **#2086**.

**+19 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Xml` 439 → 458; `SharpRuntimeTests_Xml_Linq` 184 unchanged.

**Five mutations, reverted from exact backups.** E1 (dispatcher inert — SR-AUD-352 fully
restored) fails 17 tests; E2 (only the `*ing` half inert) 12; E4 (`RemoveChild` silent) 4; E5
(character-data change silent) 3; **E6** adds the *unsafe* completion of the
`RemoveAllChildren` pair — raising `NodeRemoved` for destroyed nodes — and is caught by the
pin, 1 test. A **test defect was found and fixed** during E1 rather than reported as a result:
three assertions indexed the recorded-event vector after a non-fatal `EXPECT_EQ`, so the
mutated suite **segfaulted** instead of failing and hid every later test. They are `ASSERT_EQ`
now.

**The snapshot copy is proven load-bearing by ASan — after two non-discriminating attempts
that are recorded rather than hidden.** The behavioural mutation E3 (invoke through the field
instead of a copy) left all 458 tests passing. The first ASan probe was *also* clean both ways,
because the handler did nothing after reassigning itself. Only when the handler **reads a
capture after reassigning its own field** does the difference appear: without the copy that is
a `heap-use-after-free` in `std::string::size()`; with it, ASan/UBSan/LSan are clean over
**82,500** dispatches covering self-reassignment, reentrant tree mutation, throwing handlers on
both halves, and the remove/value-change paths. The changed bodies and `vendor/tinyxml2` were
compiled **from source** (`Xml` is `STATIC`) and instrumentation was proven by a control
heap-use-after-free (`build-probe/2079_probe2_asan.cpp`, logs `2079_probe2_asan.log` and
`2079_probe2_asan_E3.log`).

**Documentation corrected, not merely added.** `XmlDocument`'s own note said the handlers were
*"not yet wired into AppendChild/RemoveChild/etc."*. That sentence was true and is now false;
it was replaced with the dispatch rule and the `RemoveAllChildren` exception.

Migration note: `docs/Migration-XmlStrictnessAndLifecycle.md`. This is the only ticket in the
namespace that can run caller code that never ran before.

## Missing assertions and diagnostics

- The DOM suite exercises mutation results but does not subscribe to any node-change event.
- Add event dispatch diagnostics including action, affected node, parent, and old/new values.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
