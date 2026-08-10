# Audit: `modules/xml-linq/src/System/Xml/Linq/XProcessingInstruction.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-335 — medium — direct XML serialization does not validate lexical delimiters

The object constructors/setters retain text that cannot be represented verbatim
in XML comments or processing instructions, and SerializeTo concatenates it
without escaping or rejection. The direct probe produces
<!--left--right--> and <?p left?>right?>. These are not valid XML lexical
forms; the local parser accepts the comment but does not make the output valid.

## Missing assertions and diagnostics

Test invalid XML delimiter diagnostics and valid serialization across ToString,
Save, and WriteTo. Cover comment --/trailing -, PI xml target/?> data, and
CDATA ]]> values.

## Final assessment

Confirmed XML lexical preservation/validation defect: SR-AUD-335.
