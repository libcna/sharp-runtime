# Audit: `modules/text/include/System/Text/EncodingProvider.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The omitted global registration mechanism is explicitly documented, and no
first-party caller constructs a provider. The inert provider contract explains
but does not itself duplicate the `EncodingInfo` wrong-result finding.

## Finding references

- SR-AUD-299 — medium — EncodingInfo's fallback-to-UTF8 behavior is wrong.

## Other missing assertions and diagnostics

- Add a registry decision test before exposing provider-backed EncodingInfo
  instances, including unknown code page/name diagnostics.

## Final assessment

No independent new finding is confirmed.
