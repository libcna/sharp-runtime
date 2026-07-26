# Audit: `modules/text/tests/System/Text/UnicodeTests.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: included in the 233/233 focused Text run on 2026-07-27.

## Assessment

The comprehensive UTF-8/UTF-16 primitive tests cover key valid and malformed
paths. UnicodeRange's BMP-only behavior is correctly asserted. No tests cover
the separately declared Encoding raw-range/fallback/BOM contracts.

## Finding references

- SR-AUD-286 — high — fixed-width encodings' raw signed ranges are absent.
- SR-AUD-291 through SR-AUD-293 — medium — BOM, fallback, and partial-unit
  behavior are absent.

## Other missing assertions and diagnostics

- Add vector and raw-pointer boundaries for UTF-16/UTF-32, preamble versus
  payload, fallback exceptions, and zero/odd/truncated unit counts.

## Final assessment

The suite leaves the referenced findings unprotected.
