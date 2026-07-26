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
