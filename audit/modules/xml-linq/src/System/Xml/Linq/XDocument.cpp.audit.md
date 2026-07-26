# Audit: `modules/xml-linq/src/System/Xml/Linq/XDocument.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-334 — medium — parser never resolves namespace prefixes to XName URIs

ConvertElement passes the source lexical tag text directly to XName rather than
resolving the in-scope namespace URI. A direct parse of
<p:root xmlns:p="urn:audit"/> yields XName p:root with an empty URI. This makes
namespace-aware element and attribute lookup impossible for parsed XML.

## Missing assertions and diagnostics

Add default/prefixed namespace parse vectors, shadowed prefixes, namespaced
attribute lookup, and parse-save-parse identity checks.

## Final assessment

Confirmed namespace identity/round-trip defect: SR-AUD-334.
