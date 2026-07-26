# Audit: `modules/xml-linq/include/System/Xml/Linq/XName.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## Assessment

The value-based XName/XNamespace adaptation is internally coherent for constructed names, but Xml.Linq parser/writer paths do not preserve the exposed namespace URI model (SR-AUD-334).

## Missing assertions and diagnostics

Add constructor-name validation, parsed URI, prefix scope, and constructed-name serialization regressions.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
