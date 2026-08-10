# Audit: `modules/core/tests/System/ExceptionGroupTests.cpp`

## Metadata

- AUDITED: 89-line grouped exception fixture, fully read.
- Validation: its BadImageFormat, DuplicateWaitObject, and
  EntryPointNotFound cases passed 6/6 within the selected 33-test
  exception-fixture filter on 2026-07-27.

## Findings

The reviewed cases add ordinary custom-text, broad-base, and one outer-inner
message check. They never assert `COR_E_BADIMAGEFORMAT`,
`COR_E_DUPLICATEWAITOBJECT`, or `COR_E_ENTRYPOINTNOTFOUND`, so green tests do
not protect the already confirmed SR-AUD-094, SR-AUD-095, and SR-AUD-100
diagnostic defects.

## Missing assertions and diagnostics

- Missing constructor-by-constructor HResult and exact default-resource checks
  for the three reviewed types.
- DuplicateWaitObject tests omit the parameter-name suffix and wait-array
  default diagnostic; the test only confirms that a custom message is present.
- Inner-exception tests inspect outer text alone, without preserving/rethrowing
  the cause or testing null/UTF-8 inputs.

## Final assessment

The supplemental cases improve message-path breadth but leave all relevant
public diagnostic codes unguarded. No source or test was modified.
