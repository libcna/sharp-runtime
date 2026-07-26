# Audit: `modules/core/tests/System/BadImageFormatExceptionTests.cpp`

## Metadata

- AUDITED: 49-line dedicated fixture, fully read.
- Validation: `BadImageFormatExceptionTests2.*` passed 8/8 within the selected
  33-test exception-fixture filter on 2026-07-27.

## Findings

The fixture exercises ordinary default/custom messages, SystemException
catchability, filename storage, and the documented empty FusionLog adaptation.
It does not read `getHResultProperty()` from any constructor, leaving the
already confirmed SR-AUD-094 `COR_E_SYSTEM`-instead-of-`COR_E_BADIMAGEFORMAT`
defect green.

## Missing assertions and diagnostics

- Missing `COR_E_BADIMAGEFORMAT` (`0x8007000B`) checks for the default,
  message, filename, and inner-exception constructor routes.
- Missing exact default diagnostic, filename-sensitive diagnostic, UTF-8 and
  empty filename cases, and stored-inner identity/rethrow coverage.
- The empty FusionLog expectation establishes the native adaptation but does
  not verify whether an actual loader failure selects this exception.

## Final assessment

Useful public-property smoke coverage, but it cannot detect the existing
diagnostic-code regression. No source or test was modified during this audit.
