# Audit: `modules/xml-linq/src/System/Xml/Linq/XObject.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-333 — high — raw parent traversal is an ASan-confirmed use-after-free

getParentProperty calls a virtual method through parent_ and
getDocumentProperty repeatedly walks parent_. Neither method can know whether
a retained shared_ptr child has outlived the raw parent. The ASan/UBSan probe
reports heap-use-after-free exactly in getParentProperty after the owning
XElement is destroyed.

### Correction appended by ticket #1890 (2026-07-31) — the approved core repair landed; finding still `confirmed`

`XContainer` now clears the parent link of every child node it still owns in its
own destructor, and `XElement` clears every owned attribute's parent link and its
`next_` sibling link. 19 of this finding's 21 ASan `heap-use-after-free` cases
stop producing one; the XObject faulting-access count falls 48 → 4; **all eight**
public entry points that aborted with `pure virtual method called` stop doing so.
`sizeof`, class/vtable layout and allocation counts are unchanged (measured). The
full before/after table and the complete residual list — X15/X17 (#1892), X20
(#1891), X27c/X27d (#1893), X21 (permanent deviation) — are recorded once, on
`audit/modules/xml-linq/include/System/Xml/Linq/XObject.hpp.audit.md`, and in
`docs/OwnedTreeLifetimeContractPlan.md` §34. **SR-AUD-333 stays
`confirmed (design-complete)`**; numbering stays frozen at 364.

## Missing assertions and diagnostics

Add ASan coverage for getParentProperty, getDocumentProperty, sibling
navigation, removal, replacement, document-order comparison, and extension
ancestor queries after parent destruction.

## Final assessment

Confirmed high-severity lifetime defect: SR-AUD-333.
