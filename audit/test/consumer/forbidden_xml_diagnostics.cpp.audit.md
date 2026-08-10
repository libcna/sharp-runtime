# Audit: `test/consumer/forbidden_xml_diagnostics.cpp`

## Metadata

- Audit status: AUDITED (8 lines, full read).
- Role: deliberate Xml.Linq/Diagnostics include-leakage negative fixture.

## Assessment

The source intentionally requests an unrelated Diagnostics header through the
Xml.Linq closure.  It provides a narrow expected compilation failure when
component isolation holds.

## Final assessment

No fixture-local finding.
