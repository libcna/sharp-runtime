# Audit: `modules/xml-linq/tests/System/Xml/Linq/XLinqNodeTests.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## Assessment

The 92-test focused suite covers normal node mutation, structure, parsing,
serialization, ordering, extensions, and support enums. It passes completely,
but contains only normal live-parent graphs and treats inert event registration
as success.

## Missing assertions and diagnostics

Add ASan lifetime tests for retained node/attribute children, namespace
parse-save identity tests, XML lexical delimiter tests, and real
Changed/Changing event delivery assertions. These omissions leave
SR-AUD-333 through SR-AUD-336 unprotected.

## Final assessment

AUDITED; test omissions materially fail to detect the confirmed findings.
