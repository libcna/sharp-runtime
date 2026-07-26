# Audit: `modules/globalization/README.md`

## Metadata

- Audit status: AUDITED.

## Assessment

The module description correctly identifies its general API area but does not
identify the UTF-8 byte-index adaptation or the absence of locale/Unicode data.
Those constraints materially change callers' observable results and are
documented in SR-AUD-279, SR-AUD-283, and SR-AUD-284.

## Other missing assertions and diagnostics

- State whether a public offset is a UTF-8 byte position or a managed UTF-16
  code-unit position in every affected API.
- Link the documented unsupported-globalization surface to explicit tests.

## Final assessment

Documentation is incomplete with respect to confirmed behavior gaps.
