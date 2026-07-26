# Audit: `modules/core/include/System/DateTimeOffset.hpp`

## Metadata

- Audit status: AUDITED (280 lines, full read).
- Public API: partial C++ counterpart of `System.DateTimeOffset`.

## Assessment

The header candidly scopes out `DateTimeKind`-based inference, historical DST
rules, culture-aware parsing, exact parsing, span formatting, and FILETIME.
It accurately specifies whole-minute, ±14-hour offset validation and UTC tick
range checks for supported constructors.

## Finding references

Component constructors delegate to `DateTime`, so the documented supported
time-component contract inherits **SR-AUD-006**.  `Parse` and `TryParse` say
that malformed input fails, but the implementation accepts malformed offsets
and inherits DateTime's permissive parser; see **SR-AUD-007** in
[`DateTimeOffset.cpp.audit.md`](../../src/System/DateTimeOffset.cpp.audit.md).

## Final assessment

Partial-platform limitations are documented; supported input validation is
not yet faithful to the header contract (SR-AUD-006 and SR-AUD-007).
