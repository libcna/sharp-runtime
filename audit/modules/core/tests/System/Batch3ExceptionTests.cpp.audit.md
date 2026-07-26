# Audit: `modules/core/tests/System/Batch3ExceptionTests.cpp`

## Metadata

- AUDITED: 129-line grouped exception fixture, fully read.
- Validation: `DllNotFoundExceptionNewTests.*` passed 3/3 within the selected
  33-test exception-fixture filter on 2026-07-27.

## Findings

The DllNotFound additions verify outer text, TypeLoadException inheritance,
and that the default contains `Dll`. They do not read the public HResult, so
they leave SR-AUD-095's inherited `COR_E_TYPELOAD`
(`0x80131522`) instead of `COR_E_DLLNOTFOUND` (`0x80131524`) unguarded.

## Missing assertions and diagnostics

- Missing derived-HResult checks for default, message, and inner-exception
  constructors, plus exact default resource text.
- The inner case does not verify stored-cause identity/rethrow; null and UTF-8
  message boundaries are absent.
- No native library resolver path constructs this exception in the reviewed
  fixture, so the tests only exercise manual construction.

## Final assessment

The fixture confirms ordinary text and immediate base inheritance but misses
the observable diagnostic regression. No source or test was modified.
