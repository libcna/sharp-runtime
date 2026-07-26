# Audit: `modules/core/tests/System/TimeOnlyTests.cpp`

## Metadata

- Audit status: AUDITED (427 lines, 77 tests, full read).
- Validation: all 77 TimeOnly tests passed within the focused 134-test run.

## Assessment

The suite is broad for construction, millisecond precision, wrapping,
comparison, format output, and common parsing.  Its sole invalid parser test
uses a completely unrelated word; it does not test valid-looking malformed
suffixes, empty fractional data, or output preservation after a false result.

## Finding reference

**SR-AUD-009:** those missing parser assertions allow successful parsing of
`"10:20:30.abc"` and other inputs outside the documented fixed grammar.

## Required post-audit assertions

Add false/throw cases for trailing text, a bare decimal point, nonnumeric
fractional content, and a fourth fractional digit.  Use a sentinel result to
verify failure is non-mutating.

## Final assessment

The existing clock-arithmetic coverage is useful but does not guard parser
strictness (SR-AUD-009).
