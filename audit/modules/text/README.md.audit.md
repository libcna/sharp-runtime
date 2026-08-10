# Audit: `modules/text/README.md`

## Metadata

- Audit status: AUDITED.

## Assessment

The component description and dependency links are accurate but do not state
the runtime's UTF-8 `std::string` adaptation or the intentionally reduced
encoder/provider surface. This omission makes byte-vs-character contracts hard
to discover.

## Other missing assertions and diagnostics

- Document the units for each public index/count API and the incomplete
  `EncodingProvider` registration surface.

## Final assessment

No standalone documentation defect is confirmed; the omissions are recorded
with the affected APIs.
