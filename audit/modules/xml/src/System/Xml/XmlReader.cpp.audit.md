# Audit: `modules/xml/src/System/Xml/XmlReader.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-348 — medium — XmlReader continues reading after Close

`Close()` only records the enum state.  `Read()` has no closed-state guard and resumes parsing: the direct probe prints `reader-after-close-read=1` and a non-closed state value after closing a reader for `<r><x/></r>`.  The lifecycle boundary is therefore not observable and can consume input through a closed public reader.

## Missing assertions and diagnostics

- The focused suite does not call `Read()` after `Close()`, assert a closed-state lifecycle failure, or verify that parsing position remains unchanged.
- Add a state-transition diagnostic before a closed reader can traverse native XML input.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
