# Audit: `modules/globalization/include/System/Globalization/DigitShapes.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The declaration values were reviewed.  No formatter, parser, or
`NumberFormatInfo` consumer uses this setting, so it remains inert metadata.

## Other missing assertions and diagnostics

- Add a consumer-level test or explicitly label the enum unsupported/inert.

## Final assessment

No standalone defect is confirmed.
