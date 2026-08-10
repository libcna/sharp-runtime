# Audit: `modules/core/src/System/NotImplementedException.cpp`

## Metadata

- Audit status: AUDITED (37-line implementation, fully read).
- Validation: shared tests passed within the audited 124/124 Core.Base filter.

## Assessment

Constructor paths consistently use `E_NOTIMPL` (`0x80004001`) and the local
.NET default message. No implementation defect was established.

## Other missing assertions and diagnostics

- Existing tests omit C-string/null and inner-identity paths despite checking
  three HResults.
- No diagnostic identifies callers that encounter a stub expected to be a
  documented permanent limitation versus accidental missing behavior.

## Final assessment

The implementation is correct for its constructor contract. No source or test was modified.
