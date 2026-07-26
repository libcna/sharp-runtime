# Audit: `modules/xml-linq/src/System/Xml/Linq/XElement.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## SR-AUD-334 — medium — writer paths erase namespace identities

WriteTo and SerializeTo use getLocalNameProperty for every element and
attribute. The direct programmatic XName probe serializes a namespaced element
and attribute as unqualified XML, then reparses an empty namespace URI. The
comment describes the loss as a fidelity gap, but the public API exposes
namespace-aware XName/XNamespace and therefore cannot preserve its own model.

## Missing assertions and diagnostics

Add namespace URI and prefix-scope round trips through ToString, Save, and
WriteTo, including namespaced attributes and default namespaces.

## Final assessment

Confirmed namespace identity/round-trip defect: SR-AUD-334.
