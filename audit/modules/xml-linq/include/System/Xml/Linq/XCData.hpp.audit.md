# Audit: `modules/xml-linq/include/System/Xml/Linq/XCData.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-335 — medium — direct serializers emit or corrupt XML special lexical content

XCData SerializeTo writes the literal CDATA delimiter without splitting a
value containing ]]>. The probe serializes left]]>right as
<![CDATA[left]]>right]]>; parsing through the local XML path returns
leftright]]>, losing the delimiter. XComment and XProcessingInstruction direct
serializers similarly concatenate unvalidated -- and ?> sequences.

## Missing assertions and diagnostics

Add exact CDATA delimiter, comment double-hyphen/trailing-hyphen, processing
instruction target/data, and XML 1.0 control-character validation tests for
ToString, Save, and WriteTo.

## Final assessment

Confirmed XML lexical preservation/validation defect: SR-AUD-335.
