# Audit: `modules/core/tests/System/VersionTests.cpp`

## Metadata

- Audit status: AUDITED (319 lines, 54 tests, full read).
- Validation: all 54 Version tests passed in the focused Core.Base run.

## Assessment

The suite strongly covers constructors, parsing, comparison, hash behavior,
trailing separators, and all `ToString(fieldCount)` values on a fully defined
four-component version.  It does not call field-count formatting on a version
whose Build or Revision remains unspecified.

## Finding reference

**SR-AUD-011:** missing assertions for undefined Build/Revision fields permit
`ToString(3/4)` to return strings containing the internal `-1` sentinel.

## Required post-audit assertions

Assert `ArgumentException` for two-component `ToString(3/4)` and
three-component `ToString(4)`, while retaining the valid lesser field counts.

## Final assessment

Strong parse/comparison coverage; field-availability formatting is missing
negative coverage required by SR-AUD-011.
