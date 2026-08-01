# Audit: `modules/threading/include/System/Threading/WaitHandleCannotBeOpenedException.hpp`

## Metadata

- AUDITED: 21-line named-wait-handle exception declaration, fully read.
- Validation: `WaitHandleCannotBeOpenedExceptionTests.*` passed 3/3 on
  2026-07-27.

## Assessment

The ApplicationException hierarchy and constructor adaptation are coherent.
No local named Mutex/Semaphore/Event OpenExisting route exists because those
types explicitly remain process-local, so no production producer is present.

## Other missing assertions and diagnostics

- Tests omit message/inner/HResult and all named-handle producer integration.
- The exception's direct presence can misleadingly suggest named-handle support
  without a consumer-level capability diagnostic.

## Final assessment

No new finding. No production or test source was changed.

## Post-audit remediation — ticket #1875 (2026-08-01)

All three represented constructors now assign current .NET's
`COR_E_WAITHANDLECANNOTBEOPENED` (`0x8013152C`) instead of inheriting
`COR_E_APPLICATION`. The lack of a named-handle producer and the public
declaration remain unchanged.
