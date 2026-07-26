# Audit: `modules/core/include/System/DateTime.hpp`

## Metadata

- Audit status: AUDITED (415 lines, full read).
- Public API: partial C++ counterpart of `System.DateTime`.
- Evidence: API contract, `DateTime.cpp`, 93 dedicated unit tests, and the
  focused Core.Base test run.

## Assessment

The header clearly declares the intentionally partial surface: no stored
`DateTimeKind`, timezone conversion, OLE/FILETIME/binary conversion, or
culture-provider parsing/formatting.  The numeric calendar API is otherwise
specific: component constructors promise ranges `hour` 0–23, `minute` and
`second` 0–59, and `millisecond` 0–999, and promise an out-of-range exception
for invalid values.

## Finding reference

The implementation does not uphold those explicit component-validation
contracts.  See **SR-AUD-006** in
[`DateTime.cpp.audit.md`](../../src/System/DateTime.cpp.audit.md).  The parser
contract is also weaker than the public `TryParse`/`Parse` wording implies;
see **SR-AUD-007** there.

## Positive findings

Range-sensitive arithmetic uses unsigned intermediate checks in `AddTicks`
and `Subtract(TimeSpan)`, avoiding signed-overflow undefined behavior.

## Final assessment

The intended partial scope is well documented, but the documented supported
constructor contract needs repair and regression assertions (SR-AUD-006).
