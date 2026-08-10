# Audit: `modules/core/include/System/DBNull.hpp`

## Metadata

- Audit status: AUDITED (216-line public header, fully read).
- Related interface: `IConvertible.hpp` (audited in this checkpoint).
- Validation: Core.Base `DBNullTests.*` passed 9/9 and integration
  `DBNullTests.*` passed 11/11 on 2026-07-26.

## Assessment

`DBNull` is an uncopyable function-local-static singleton that reports
`TypeCode::DBNull`, returns an empty string, and consistently throws the local
`InvalidCastException` message for every supported conversion operation.  The
implementation is fully inline and has no raw memory, numeric arithmetic, or
shared mutable state beyond C++'s thread-safe static initialization.

Returning a non-const singleton reference is wider than .NET's read-only
`DBNull.Value` surface, but this final stateless type has no public mutator or
observable mutable field.  It is therefore an API-adaptation observation, not
a demonstrated behavioral defect.

## Positive findings

- Core tests exercise all conversion members not covered by the integration
  smoke source; the integration suite covers singleton identity, type code,
  strings, interface inheritance, and the other throwing conversions.
- The exact local exception text is asserted for one representative method.

## Other missing assertions and diagnostics

- No test calls every throwing conversion through an `IConvertible*` base
  pointer; only `IsA_IConvertible` checks the hierarchy.
- The provider-aware `ToString` overload is tested with `nullptr` only.  No
  non-null provider confirms its documented intentional ignorance.
- No multi-threaded fixture verifies singleton identity across threads, though
  C++11 function-local static initialization supplies that guarantee.

## Final assessment

The singleton and its conversion failures conform to the documented local
contract.  No evidence-backed defect was found and no source or test was
modified during this audit.
