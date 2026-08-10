# Audit: `modules/globalization/tests/System/Globalization/Batch32Tests.cpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The suite checks many Persian calendar and RegionInfo values but asserts that
ordinary LCIDs become US and does not reject unknown region names, matching
the documented fabrication path.

## Finding references

- SR-AUD-285 — medium — fake region data is accepted as success.

## Other missing assertions and diagnostics

- Add unsupported name/LCID rejection, non-US real culture data, and full
  Persian range/calendar-reference comparisons.

## Final assessment

The test contract leaves SR-AUD-285 unprotected.
