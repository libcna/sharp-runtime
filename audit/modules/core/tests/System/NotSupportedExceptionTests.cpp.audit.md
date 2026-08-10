# Audit: `modules/core/tests/System/NotSupportedExceptionTests.cpp`

## Metadata

- AUDITED: 90-line dedicated fixture, fully read.
- Validation: `NotSupportedExceptionTests.*` passed 10/10 in the combined
  36-test exception filter on 2026-07-26.
- Related production audit: `NotSupportedException.hpp.audit.md` found no
  standalone implementation defect.

## Findings

This is the strongest fixture in the group: it checks default text fragments,
exact C-string/string message preservation, multiple catch routes, and basic
inner construction. The named `InnerExceptionCtor_ContainsInnerMessage` test,
however, asserts only that the outer message is nonempty, so it makes no
observable assertion about `"disk error"` or retained inner state.

## Missing assertions and diagnostics

- No HResult, exact default resource text, null/empty C-string, copy/move, or
  data/source/help-link coverage.
- Inner exception identity/rethrow and its actual message are not checked.
- No platform/stream/remoting consumer verifies that this type is selected at
  an unsupported-operation boundary.

## Final assessment

Good normal diagnostic coverage, but the inner-cause assertion is misleadingly
named and weak. No source or test was modified during this audit.
