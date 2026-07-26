# Audit: `modules/xml-linq/include/System/Xml/Linq/XElement.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-334 — medium — namespace URI semantics are discarded by Xml.Linq parse and serialization

XElement exposes XName namespace-bearing construction and query APIs, but the
implementation only writes local names. The direct probe constructs
{urn:audit}root with a namespaced attribute and receives
namespaced-serialized=<root attribute="value"/>; reparsing reports an empty
namespace URI. Parsing a prefixed XML input likewise produces XName p:root
with an empty namespace URI instead of resolving urn:audit.

## Missing assertions and diagnostics

Add default/prefixed namespace parse, namespace-aware query, nested scope,
attribute namespace, save/reload, and OmitDuplicateNamespaces regressions.

## Final assessment

Confirmed namespace identity/round-trip defect: SR-AUD-334.
