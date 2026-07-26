# Audit: `modules/core/tests/System/PlatformNotSupportedExceptionTests.cpp`

## Metadata

- AUDITED: 47-line dedicated fixture, fully read.
- Validation: `PlatformNotSupportedExceptionTest.*` passed 6/6 in the combined
  36-test exception filter on 2026-07-26.
- Related production audit: `PlatformNotSupportedException.hpp.audit.md`
  confirms derived HResult assignment.

## Findings

The fixture checks custom message paths, inheritance, throwability, and the
platform-specific HResult for default/message/inner constructors. It corrects
the older generic header-report gap for those three HResult paths; no standalone
production defect is reproduced.

## Missing assertions and diagnostics

- Missing exact default resource text, inner identity/rethrow, null/empty/UTF-8
  message, copy/move, and std::exception catch vectors.
- No real platform-gated API is exercised, so the operational policy for
  selecting this exception rather than generic NotSupportedException remains
  untested.

## Final assessment

Strong constructor/HResult smoke coverage; no source or test was modified
during this audit.
