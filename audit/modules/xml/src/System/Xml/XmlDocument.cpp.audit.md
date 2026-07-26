# Audit: `modules/xml/src/System/Xml/XmlDocument.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-352 — medium — XmlDocument node-change events are publicly exposed but never raised

The document retains NodeInserted/NodeRemoved/NodeChanging/NodeChanged handler collections, yet DOM mutation paths do not dispatch them.  A handler installed before an element insertion records zero events in the direct probe (`node-inserted-events=0`).  Consumers cannot observe the documented mutation notification surface.

## Missing assertions and diagnostics

- The DOM suite exercises mutation results but does not subscribe to any node-change event.
- Add event dispatch diagnostics including action, affected node, parent, and old/new values.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
