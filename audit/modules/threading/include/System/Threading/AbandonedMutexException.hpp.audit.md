# Audit: `modules/threading/include/System/Threading/AbandonedMutexException.hpp`

## Metadata

- AUDITED: 48-line abandoned-mutex exception declaration, fully read.
- Validation: `AbandonedMutexExceptionTests.*` passed 4/4 on 2026-07-27.

## Assessment

The declaration retains default/message/inner construction plus direct
location/Mutex metadata. The local Mutex explicitly does not implement native
abandonment detection, so no production path creates this exception; that is
the documented process-local adaptation rather than a new defect here.

## Other missing assertions and diagnostics

- Tests omit message/inner/HResult, non-Mutex WaitHandle input, null handle,
  negative/large index, and actual abandoned-owner production behavior.
- No diagnostic explains at a call site that native abandonment is unsupported.

## Final assessment

No new finding. No production or test source was changed.

## Post-audit remediation — ticket #1875 (2026-08-01)

All six represented constructors now assign current .NET's
`COR_E_ABANDONEDMUTEX` (`0x8013152D`) instead of inheriting `COR_E_SYSTEM`.
Permanent tests cover ordinary, inner, index, and handle forms. Unsupported
native abandonment production remains exactly as documented above.
