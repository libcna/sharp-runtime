# Audit: `modules/xml-linq/CMakeLists.txt`

## Metadata

- Audit status: AUDITED.
- Component: `Xml.Linq`.
- Validation: `SharpRuntimeTests_Xml_Linq` built and passed 92/92 on 2026-07-27.
- Direct evidence: `/tmp/sharp-runtime-xml-linq-audit/xml_linq_probe`; SR-AUD-333 was rebuilt with ASan/UBSan against Xml.Linq sources.

## Assessment

The static module declares the documented Core.Base and Xml public dependencies; no dependency-boundary divergence was found.

## Missing assertions and diagnostics

Keep a selective consumer closure build for Xml.Linq in CI.

## Final assessment

AUDITED; component-scoped evidence and confirmed findings are recorded in the audit index.
