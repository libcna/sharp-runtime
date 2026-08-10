# Audit: `modules/globalization/include/System/Globalization/SortVersion.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The small immutable version/identifier value preserves construction and
equality state.  No collation provider populates it in this module.

## Other missing assertions and diagnostics

- Test hashes, all identifier bytes, and linkage to an actual CompareInfo
  version provider if such a provider is implemented.

## Final assessment

No standalone defect is confirmed.
