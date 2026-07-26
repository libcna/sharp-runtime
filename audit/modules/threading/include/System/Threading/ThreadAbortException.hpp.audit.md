# Audit: `modules/threading/include/System/Threading/ThreadAbortException.hpp`

## Metadata

- AUDITED: 35-line legacy abort exception declaration, fully read.
- Validation: complete Threading tests passed 359/359 on 2026-07-27; audited
  Batch8 checks normal construction and null ExceptionState.

## Assessment

The header explicitly records that modern .NET does not support Thread.Abort
and retains the type only for source compatibility.  Its null state result is
consistent with that declared native adaptation.  No new defect is
demonstrated.

## Other missing assertions and diagnostics

- Tests omit HResult, hierarchy/sealing, null/UTF-8/inner paths, obsolete API
  diagnostics, and any real abort producer; Thread itself omits Abort.

## Final assessment

This is a documented legacy compatibility type.  No source or test was
changed.
