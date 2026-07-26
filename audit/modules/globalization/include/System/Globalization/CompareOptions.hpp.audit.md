# Audit: `modules/globalization/include/System/Globalization/CompareOptions.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The numeric enum constants mirror .NET.  Their public semantic contract is not
realized by `CompareInfo`, which ignores all but ASCII case folding; see
SR-AUD-283.

## Finding references

- SR-AUD-283 — medium — declared option values are silently ignored by the
  sole comparison consumer.

## Other missing assertions and diagnostics

- Test that each flag changes an observable operation or is rejected rather
  than accepted as a no-op.

## Final assessment

Covered by SR-AUD-283; no independent enum-value defect is confirmed.
