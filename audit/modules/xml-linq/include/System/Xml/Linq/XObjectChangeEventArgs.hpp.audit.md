# Audit: `modules/xml-linq/include/System/Xml/Linq/XObjectChangeEventArgs.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## Assessment

The enum and event-argument data holders are correct in isolation, but no XObject mutation ever raises the represented changes (SR-AUD-336).

### Correction appended by ticket #2198 (2026-08-10) — pinned, still `confirmed`, now design-complete

The premise is **confirmed verbatim**, and re-measured across **nine** mutation doors rather than
the one the probe named: `setValueProperty`, `setNameProperty`, `Add(node)`, `Add(attribute)`,
`Add(text)`, `AddFirst`, `RemoveAll`, an attribute's own `setValueProperty`/`Remove`, and a
handler on an **ancestor** observing a descendant's change. Every one delivers **zero**
notifications (`build-probe/2195_probe1_surface.log`, block `E*`).

**The classification, however, was wrong: this is not compatible-actionable.** Two independent
blockers were measured, and both are structural rather than a matter of effort
(`build-probe/2195_probe3_events.log`, `docs/SystemXmlLinqNamespaceReviewPlan.md` §12.3):

1. **There is nowhere to store a handler.** .NET keeps these registrations in `XObject`'s
   annotation slot; annotations are documented out of scope in this port. `sizeof(XObject)` is
   **16** — a vptr plus `parent_`, with **no padding** — so a handler field takes it to **24**
   and grows every derived node type with it (`XNode` 16, `XContainer` 40, `XElement` 128,
   `XAttribute` 120, `XText`/`XCData`/`XComment` 48, `XProcessingInstruction` 80, `XDocument`
   56). **That exact growth, `XObject` 16 → 24, was declined by the user on 2026-07-31** for
   ticket #1896's cycle guard. A different motive neither carries the refusal over nor reverses
   it — it has to be asked again, which is approval **XL-1** on ticket **#2199**.
2. **A registration cannot be named for removal, at any layout cost.**
   `XObjectChangeEventHandler` is a bare `std::function` alias and `std::function` has no
   `operator==` against another `std::function`, so `remove_Changed(handler)` cannot identify
   which registration to drop. Proved by `static_assert`, not asserted. Resolving it needs a
   public shape change (registration returns a token) or a documented deviation — approval
   **XL-2** on #2199.

A process-wide side table keyed by `const XObject*` avoids blocker 1 but not blocker 2, and
#1896's notes already record that shape among the alternatives judged **worse**. It is not
adopted on a namespace review's own authority.

**What #2198 did deliver.** The audit's own observation — "the focused test only asserts that
registration does not throw, thereby preserving the inert behavior" — is closed.
`XLinqChangeNotificationTests.cpp` adds **23 permanent regressions** that make the inert contract
**discriminating** at every door, plus two `static_assert`s that fail if blocker 2 ever stops
holding and a layout pin that fails if blocker 1's approval sentence goes stale. **Mutation-checked
against the exact failure the audit warned about**: a deliberate *half*-implementation — handlers
stored process-wide, only `XElement::setValueProperty` notifying — now fails
`SetValue_RaisesNothing`, where the previous coverage would have passed it unchanged. The
`XObject` doc-comment states both blockers and names #2199.

**No behaviour changed and nothing was implemented.** SR-AUD-336 stays **open**, counted as
`confirmed`, and now carries the `design-complete` qualifier because the selected repair and its
blocked implementation ticket are recorded. No `SR-AUD-*` identifier was issued; numbering stays
frozen at **364**.

## Missing assertions and diagnostics

Test that every mutation emits the correct changing/changed pair, sender, and argument instance.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
