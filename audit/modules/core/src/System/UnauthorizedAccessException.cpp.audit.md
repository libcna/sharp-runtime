# Audit: `modules/core/src/System/UnauthorizedAccessException.cpp`

## Metadata

- Audit status: AUDITED (31-line implementation, fully read with declaration).
- Validation: the focused exception-family filter passed 38/38 on 2026-07-26.
- Reference basis: local .NET `UnauthorizedAccessException.cs` and `COR_E_UNAUTHORIZEDACCESS` (`0x80070005`).

## Assessment

All four constructor implementations preserve supplied message/inner state and explicitly set the documented access-denied HResult. No standalone implementation defect was confirmed.

## Other missing assertions and diagnostics

- Current tests omit all HResult checks, C-string null handling, exact default message, stored-inner identity/rethrow, and non-ASCII messages.
- The source relies on transitive declaration of `std::move`; it compiles now but has no include-self-sufficiency evidence.
- No permission-denied OS integration reaches this code in the reviewed fixture.

## Final assessment

The constructor implementation is correct for the reviewed ordinary paths. No source or test was modified.
