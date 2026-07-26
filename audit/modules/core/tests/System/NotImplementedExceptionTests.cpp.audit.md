# Audit: `modules/core/tests/System/NotImplementedExceptionTests.cpp`

## Metadata

- AUDITED: 33-line dedicated fixture, fully read.
- Validation: `NotImplementedExceptionTest.*` passed 5/5 in the combined
  36-test exception filter on 2026-07-26.
- Related production audit: `NotImplementedException.hpp.audit.md` found no
  standalone implementation defect.

## Findings

The tests exercise default/custom/inner construction and SystemException
inheritance. They do not distinguish intentional unsupported behavior from an
unfinished implementation at a real API boundary.

## Missing assertions and diagnostics

- No exact default message, HResult, null/empty C-string, inner identity,
  std::exception catch, copy/move, or UTF-8/embedded-NUL coverage.
- No consumer verifies that a documented unimplemented route raises this type
  rather than NotSupportedException, PlatformNotSupportedException, or a
  native failure.

## Final assessment

Basic construction coverage with no independent implementation finding. No
source or test was modified during this audit.
