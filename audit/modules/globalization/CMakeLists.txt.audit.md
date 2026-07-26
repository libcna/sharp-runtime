# Audit: `modules/globalization/CMakeLists.txt`

## Metadata

- Audit status: AUDITED.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Globalization` and the
  resulting executable completed 676/676 tests on 2026-07-27.

## Assessment

The static `Globalization` target declares the narrow `Core.Base` public
dependency required by its headers.  Its test target covers the shipped source
set; the green suite nevertheless lacks Unicode grapheme, per-thread culture,
and unassigned-IDN behavior, recorded by this shard's findings.

## Other missing assertions and diagnostics

- Make the test target fail on ThreadSanitizer reports in CurrentCulture/UI
  culture concurrency scenarios.
- Add non-ASCII grapheme, culture-casing, comparison-option, and IDNA property
  behavior vectors.

## Final assessment

No build-definition defect is confirmed.
