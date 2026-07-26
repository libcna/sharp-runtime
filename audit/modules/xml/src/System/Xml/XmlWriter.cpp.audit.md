# Audit: `modules/xml/src/System/Xml/XmlWriter.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-349 — medium — XmlWriter emits malformed XML because names and writer state are not validated

The writer forwards element names directly to tinyxml2 and exposes no writer-state validation.  The direct probe writes `WriteStartElement("1bad")`, obtains `<1bad/>`, and the project reader rejects that result with `XML_ERROR_PARSING`.  A successful writer call can thus produce a document its paired reader cannot consume.

## Missing assertions and diagnostics

- Tests do not assert rejection of invalid XML names, invalid call ordering, or operations after `Close()`.
- Add validation diagnostics naming the invalid element/attribute and current writer state before native serialization.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
