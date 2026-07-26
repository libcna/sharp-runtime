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

## Missing assertions and diagnostics

Add ASan coverage for getParentProperty, getDocumentProperty, sibling
navigation, removal, replacement, document-order comparison, and extension
ancestor queries after parent destruction.

## Final assessment

Confirmed high-severity lifetime defect: SR-AUD-333.
