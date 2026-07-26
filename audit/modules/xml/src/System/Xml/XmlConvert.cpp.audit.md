# Audit: `modules/xml/src/System/Xml/XmlConvert.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-354 — medium — XmlConvert TimeSpan conversion uses native syntax instead of XML Schema duration

`ToTimeSpan` delegates to the framework's colon-form `TimeSpan::Parse`, and `ToString(TimeSpan)` delegates to its native formatter.  XML Schema durations use `P…` notation.  The direct probe rejects valid `P1D` with `String was not recognized as a valid TimeSpan: P1D`; serialization is correspondingly not XML Schema-compatible.

## Missing assertions and diagnostics

- Focused tests do not round-trip XML Schema duration values or distinguish them from framework-specific TimeSpan strings.
- Conversion failures should identify the XML lexical type and rejected token.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
