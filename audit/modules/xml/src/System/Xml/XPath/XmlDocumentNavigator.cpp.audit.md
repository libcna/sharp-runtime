# Audit: `modules/xml/src/System/Xml/XPath/XmlDocumentNavigator.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-355 — medium — XPath navigator exposes adjacent text/CDATA nodes separately

XPath data-model navigation must collapse an adjacent text-like run into one logical text node.  The navigator returns the current native node directly: the direct probe first reports `xpath-first-text-value=left`, then successfully moves to a second text node with value `right`.  XPath position/value semantics consequently diverge for mixed CDATA/text content.

## Missing assertions and diagnostics

- XPath tests do not assert logical-node collapsing, navigation across adjacent text-like siblings, or combined string values.
- Navigator diagnostics should identify the native run collapsed into each XPath logical node.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
