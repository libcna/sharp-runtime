# Audit: `modules/core/tests/System/StringTests.cpp`

## Metadata

- AUDITED: 212 String and StringNormalizationExtensions cases.
- Validation: the complete Core.Base fixture passed 4,946/4,946; direct String
  source/probe evidence was reviewed.

## Assessment

The fixture covers a large normal byte-string surface and preserves several
prior range regressions, including single-argument search validation and
overflow-safe `ToCharArray` bounds.  It has no test for escaped composite
braces, malformed closing braces, or a match spilling past a bounded
`LastIndexOf` search end, so it does not detect SR-AUD-015 or SR-AUD-016.
Its normalization samples are ASCII and do not challenge the separate Unicode
normalization finding.

## Other missing assertions and diagnostics

- Add exact `{{0}}`, `{{{0:D}}}`, literal `}}`, unmatched closing-brace, and
  oversized/non-numeric format-precision tests for SR-AUD-015.
- Add positive and negative bounded substring `LastIndexOf` cases for
  SR-AUD-016, including empty value and `startIndex == Length` behavior.
- Add non-ASCII whitespace, case, comparison, trim, UTF-8 byte-index, and
  composed/decomposed normalization vectors.

## Final assessment

The broad happy-path fixture leaves SR-AUD-015/016 and Unicode boundaries
unprotected. No source or test was changed.
