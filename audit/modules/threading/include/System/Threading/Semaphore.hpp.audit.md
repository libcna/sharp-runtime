# Audit: `modules/threading/include/System/Threading/Semaphore.hpp`

## Metadata

- AUDITED: 100-line condition-variable counting semaphore, fully read.
- Validation: focused `SemaphoreTests.*` and `ThreadingTests.Semaphore_*`
  cases passed 8/8 on 2026-07-27. A direct C++ probe was compiled with UBSan
  and compared with .NET 10 at the maximum valid count boundary.
- Reference basis: current .NET 10 Semaphore count/overflow behavior and the
  header's explicit process-local named-semaphore adaptation.

## SR-AUD-206 — high — `Release` overflows signed count arithmetic before detecting a full semaphore

`Release(intcs)` evaluates `count_ + releaseCount > maxCount_` in signed C++
arithmetic. For `Semaphore(1, INT_MAX).Release(INT_MAX)`, UBSan reports signed
integer overflow at `Semaphore.hpp:91`; the C++ call then prints
`Semaphore=normal` instead of rejecting the over-release. The matching .NET 10
call throws `System.Threading.SemaphoreFullException`. The same defect is
independently reproduced in `SemaphoreSlim` and is recorded there as an
extension of SR-AUD-206.

## Assessment

The normal count transitions, constructor diagnostics, full ordinary timeout,
and infinite wait are coherent under the reviewed fixtures. The maximum valid
argument pair reaches undefined behavior before their intended full-semaphore
guard, which makes this a high public-boundary defect.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-206 maximum-count/release arithmetic and a test that the
  full condition leaves count unchanged.
- They omit zero-timeout contention, multiple waiters/releases, wake ordering,
  Close/Dispose behavior inherited from the no-op WaitHandle base, and release
  races at the count boundary.
- Named sharing, existing/open semantics, ACLs, abandoned-handle behavior, and
  process boundaries remain outside the explicit native adaptation and have no
  diagnostic coverage.

## Final assessment

SR-AUD-206 is confirmed by UBSan and direct current-.NET comparison. No
production or test source was changed.
