# Audit: `modules/text/tests/System/Text/StringBuilderTests.cpp`

## Metadata

- Audit status: AUDITED.
- Validation: included in the 233/233 focused Text run on 2026-07-27.

## Assessment

The suite has good ASCII validation coverage for recent range fixes, but all
text fixtures are ASCII. It does not test negative `destinationLength`, UBSan
arithmetic boundaries, or any UTF-8 character-position behavior.

## Finding references

- SR-AUD-295 — high — signed CopyTo capacity overflow has no regression.
- SR-AUD-296 — medium — byte-based character positions have no regression.

## Other missing assertions and diagnostics

- Add UTF-8 BMP/non-BMP length/index/insert/remove/copy tests and sanitizer
  cases for negative/extreme capacity metadata.

## Final assessment

The suite leaves SR-AUD-295 and SR-AUD-296 unprotected.
