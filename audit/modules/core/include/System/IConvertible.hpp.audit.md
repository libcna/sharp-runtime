# Audit: `modules/core/include/System/IConvertible.hpp`

## Metadata

- Audit status: AUDITED (59-line public interface, fully read).
- Supporting validation: Core.Base `DBNullTests.*` passed 9/9 and integration
  `DBNullTests.*` passed 11/11 on 2026-07-26.

## Assessment

The header deliberately models a culture-invariant subset of .NET
`IConvertible`: every provider parameter is omitted and the public comment
limits the current implementation set to `DBNull`, whose conversions always
throw independently of culture.  The full scalar conversion shape, `Decimal`,
`DateTime`, `TypeCode`, and text operations are all present as const virtual
members.  This interface owns no numeric conversion implementation.

## Other missing assertions and diagnostics

- The provider omission is a documented adaptation, but no compile-time API
  baseline prevents a future culture-sensitive implementer from silently being
  unable to receive its provider.
- The only first-party implementation is `DBNull`; its deliberately throwing
  behavior cannot demonstrate normal conversions, overflow behavior, or
  provider handling.
- The generic test fixture in pending `Batch5Tests.cpp` uses `std::stoi`-like
  local behavior rather than checking the interface's error taxonomy across
  every member.

## Final assessment

The documented culture-invariant limitation is not an independently confirmed
defect for the sole current implementation.  No source or test was modified
during this audit.
