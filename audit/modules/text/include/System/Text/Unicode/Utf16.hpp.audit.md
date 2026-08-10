# Audit: `modules/text/include/System/Text/Unicode/Utf16.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The linear scanner correctly accepts surrogate pairs and reports either form
of unpaired surrogate at its first UTF-16 offset. No independent discrepancy
was observed.

## Other missing assertions and diagnostics

- Test empty input, consecutive pairs, pair boundaries, noncharacters, and
  maximum practical string length/narrowing.

## Final assessment

No evidence-backed finding is confirmed.
