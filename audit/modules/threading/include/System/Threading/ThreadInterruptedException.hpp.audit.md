# Audit: `modules/threading/include/System/Threading/ThreadInterruptedException.hpp`

## Metadata

- AUDITED: 32-line interrupt exception declaration, fully read.
- Validation: complete Threading tests passed 359/359 on 2026-07-27; audited
  Batch8 covers construction and one inner-message smoke case.

## Assessment

The lightweight exception construction is coherent in isolation.  Thread's
Interrupt is an explicit no-op stub, so no in-tree wait path produces this
exception; tests do not claim otherwise.  No new defect is demonstrated.

## Final assessment

The declaration is coherent, but real interruption behavior remains absent by
documented adaptation.  No source or test was changed.

## Post-audit remediation — ticket #1875 (2026-08-01)

All three represented constructors now assign current .NET's
`COR_E_THREADINTERRUPTED` (`0x80131519`) instead of `COR_E_SYSTEM`. The
unsupported producer behavior and every public declaration remain unchanged.
