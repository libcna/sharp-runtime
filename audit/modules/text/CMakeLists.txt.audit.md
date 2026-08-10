# Audit: `modules/text/CMakeLists.txt`

## Metadata

- Audit status: AUDITED.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Text` followed by
  `build/SharpRuntimeTests_Text --gtest_color=no` passed 233/233 tests on
  2026-07-27.

## Assessment

The static target has the declared `Buffers` and `Core.Base` public
dependencies and includes the shipped tests. The green target leaves key
encoding range, fallback, Unicode-count, and Web-default behavior untested.

## Other missing assertions and diagnostics

- Make negative index/count, fallback exception, preamble, UTF-16 count, and
  Unicode non-ASCII tests mandatory in this target.

## Final assessment

No build-definition defect is confirmed.
