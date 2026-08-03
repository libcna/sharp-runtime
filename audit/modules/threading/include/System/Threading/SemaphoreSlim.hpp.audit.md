# Audit: `modules/threading/include/System/Threading/SemaphoreSlim.hpp`

## Metadata

- AUDITED: 120-line lightweight condition-variable semaphore, fully read.
- Validation: focused `ThreadingTests.SemaphoreSlim_*` cases passed 8/8 on
  2026-07-27. UBSan exercised maximum release arithmetic; separate TSan probes
  exercised concurrent `CurrentCount`/Wait and `Dispose`/Wait.
- Reference basis: current .NET 10 SemaphoreSlim count, disposal, and
  thread-safe public-member contracts.

## SR-AUD-206 — high — `Release` extends the shared signed-overflow full-semaphore defect

Like `Semaphore`, `Release(intcs)` adds signed `count_ + releaseCount` before
checking the maximum. `SemaphoreSlim(1, INT_MAX).Release(INT_MAX)` produces
UBSan's signed-overflow report at `SemaphoreSlim.hpp:108` and prints
`SemaphoreSlim=normal`; .NET 10 throws `SemaphoreFullException`. See the
owning `Semaphore.hpp` report for SR-AUD-206.

## SR-AUD-207 — high — `CurrentCount` and disposal state race with normal public operations

`getCurrentCountProperty()` returns `count_` without taking `mutex_`, while
Wait/Release write it under that mutex. A worker repeatedly Waits/Releases
while another thread reads CurrentCount; TSan reports the read at line 52
racing a write in `Wait()` at line 62. Separately, `disposed_` is an ordinary
bool read by `ThrowIfDisposed()` with no lock while `Dispose()` writes it; the
Dispose/Wait TSan probe reports the direct race at line 38. Both are C++
undefined behavior in public operations that current .NET exposes as
thread-safe.

## Assessment

The focused fixture correctly checks quiescent counts, standard release
returns, ordinary validation order, and post-disposal exceptions. It has no
concurrent observation or disposal tests, so all green cases coexist with the
two sanitizer-confirmed data races.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-206 maximum-count arithmetic, full-state preservation, and
  all close-to-`INT_MAX` release combinations.
- They omit SR-AUD-207 TSan coverage for CurrentCount with Wait/Release and
  Dispose with Wait/Release/CurrentCount, plus the behavior of a waiter already
  blocked when disposal occurs.
- They omit zero/infinite timeout contention, multiple waiter wake-ups, exact
  count accounting under high contention, and overflow/exception ordering at
  every constructor/release boundary.

## Final assessment

SR-AUD-206 extends here; SR-AUD-207 is confirmed by two independent TSan
probes. No production or test source was changed.

## Post-audit remediation — ticket #1947 (2026-08-03)

**SR-AUD-206 is `remediated`** here as well as in its owning `Semaphore.hpp`
report, which carries the full record and two corrections to the finding's extent
and consequence. In summary: the pre-fix probe reports overflow at **both**
`SemaphoreSlim.hpp:108` (the guard, recorded) and `:111` (the increment, not
recorded), and the surviving state was `CurrentCount == -2147483648`, which makes
the instance permanently unusable rather than merely under-diagnosed. The guard
now uses SemaphoreSlim.cs's own `maxCount_ - count_ < releaseCount`.

**SR-AUD-207 remains `confirmed`** and is untouched: the unlocked `CurrentCount`
read and the unsynchronised `disposed_` flag are cause T-A in
`docs/ThreadingNamespaceReviewPlan.md`, owned by ticket #1955, which needs a
layout measurement #1947 deliberately did not make.
