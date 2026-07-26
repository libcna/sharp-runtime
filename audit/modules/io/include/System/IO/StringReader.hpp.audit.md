# Audit: `modules/io/include/System/IO/StringReader.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-343 — medium — StringReader and StringWriter inherit no-op Close and remain usable after disposal

Neither in-memory text class tracks a closed state; the base TextReader/TextWriter Close is a no-op.  The direct probe calls Close then reads `x` from StringReader and writes `x` to StringWriter, printing `string-reader-after-close=120` and `string-writer-after-close=x`.  This differs from the public .NET text reader/writer lifecycle contract.

## Missing assertions and diagnostics

- In-memory text tests omit post-Close Read/Peek/ReadLine/ReadToEnd and post-Close Write/WriteLine/Flush cases.
- Empty/end-of-stream behavior is only distinguishable from a missing reader by return value, with no disposed-state assertion.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
