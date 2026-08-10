# Audit: `modules/xml-linq/include/System/Xml/Linq/Extensions.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## Assessment

This hierarchy path follows the raw parent pointer declared by XObject.
Retained children can outlive their shared_ptr-owned parent; ASan confirms
SR-AUD-333 at the first parent query. Sibling navigation, mutation,
document-order comparison, and extension ancestor traversal share the same
unsafe ownership assumption.

## Missing assertions and diagnostics

Add post-parent-destruction ASan regressions for this operation, not only
normal attached-tree behavior.

## Final assessment

AUDITED; SR-AUD-333 applies to this raw-parent traversal path.
