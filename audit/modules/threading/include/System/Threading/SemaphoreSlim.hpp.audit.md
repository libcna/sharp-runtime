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


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-207 → `remediated` (all three types)

Cause **T-A** of `docs/ThreadingNamespaceReviewPlan.md`, "shared mutable state is observed
outside its own mutex". Evidence: `build-probe/1955_probe1_shared_state_races.cpp` under
`-fsanitize=thread`, logs `1955_probe1_tsan_before.log` (**13** data-race reports across
seven scenarios, exit 66) and `1955_probe1_tsan_after.log` (**zero** reports, exit 0, every
control value unchanged). Instrumentation was proved rather than assumed: 132 `__tsan_*`
symbols in the sanitized binary. The layout gate passed — `sizeof` and `alignof` are
byte-identical for all six affected types before and after
(`1955_probe1_layout_before.log` / `1955_probe1_layout_after.log`), so no user approval was
required, and the numbers are pinned by
`ThreadingSharedStateTests.RepairedTypes_LayoutUnchanged` in
`modules/threading/tests/System/Threading/ThreadingSharedStateTests.cpp`.

### The count: an atomic field, not a locking property — and why

`SemaphoreSlim::count_` is now `std::atomic<intcs>`, acquire-loaded by
`getCurrentCountProperty()` and still written **only** under `mutex_`.

The namespace plan's §5 proposed *"a counter that a public property exposes is read under the
owning mutex"*. That is not what .NET does, and the difference is deliberate:
`SemaphoreSlim.CurrentCount` returns `m_currentCount`, declared `private volatile int`, with
no lock taken. An atomic field reproduces that exactly; a locking property would add
contention to a property .NET makes cheap, on a primitive whose whole purpose is to be
lighter than `Semaphore`.

Nothing about the compound state changes: every write stays inside the critical section, so
the class invariant `0 <= count_ <= maxCount_` and the condition-variable predicate
`count_ > 0` are exactly as before. The atomic only makes the unsynchronised *read*
well-defined.
`ThreadingSharedStateTests.SemaphoreSlim_CurrentCountStaysInRangeUnderContention` runs three
threads for 500 iterations each and asserts no observation ever leaves `[0, maxCount]`.

### The flag

`disposed_` is `std::atomic<bool>` with a release store in `Dispose()` and an acquire load in
`ThrowIfDisposed()`.

### Scope

The finding's index summary names `SemaphoreSlim`, `ManualResetEventSlim` and `CountdownEvent`,
and the namespace review's §3.1 item 3 records that all three are members rather than two plus
an extension. All three were repaired in this one change, so the family cannot close falsely.
`sizeof(SemaphoreSlim)` 104 → 104, `sizeof(ManualResetEventSlim)` 112 → 112,
`sizeof(CountdownEvent)` 104 → 104.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
