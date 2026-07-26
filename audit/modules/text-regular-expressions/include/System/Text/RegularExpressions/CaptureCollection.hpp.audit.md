# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/CaptureCollection.hpp`

## Metadata

- AUDITED: owned capture vector, count, indexed access, iterator surface, and
  explicit std::regex capture limitation.
- Evidence: Group/Match consumers and current managed multi-capture behavior
  were considered.

## Assessment

The collection safely owns its available capture values and validates indexes.
Its documented maximum of one capture per group is an explicit std::regex
adaptation, not silently represented as complete managed semantics.

## Other missing assertions and diagnostics

- Add empty/out-of-range, quantified multi-capture limitation, iteration,
  copied collection, and Unicode index/value tests.

## Final assessment

No independent defect was demonstrated. No source or test was changed.
