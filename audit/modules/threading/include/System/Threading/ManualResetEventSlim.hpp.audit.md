# Audit: `modules/threading/include/System/Threading/ManualResetEventSlim.hpp`

## Metadata

- AUDITED: 87-line atomic-flag/manual-reset implementation, fully read.
- Validation: focused `ManualResetEventSlimTests.*` cases are part of the
  complete Threading 359/359 baseline. A direct two-thread disposal/Wait probe
  was compiled with `-fsanitize=thread`; lifecycle comparison also established
  that `Set()` after disposal is permitted by both C++ and .NET 10.
- Reference basis: current .NET 10 ManualResetEventSlim disposal behavior and
  thread-safe public-member contract.

## SR-AUD-207 — high — unsynchronized disposal state extends the public-operation data race

`disposed_` is a non-atomic bool read by `ThrowIfDisposed()` without `mtx_`,
while `Dispose()` writes it without synchronization. A worker repeatedly calls
`Wait(0)` while another thread calls `Dispose()`; TSan reports the direct race
between the read at line 25 and the writer. This is the same public-lifecycle
undefined-behavior pattern as SemaphoreSlim and extends SR-AUD-207.

## Assessment

The atomic signal flag provides coherent normal Set/Reset/wait visibility, and
the reviewed fixtures cover ordinary state, timeout, and post-disposal Wait/
Reset. They do not cover disposal concurrently with an operation. The source
and direct .NET 10 probe both permit `Set()` after Dispose, so that behavior is
not recorded as a finding.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-207 TSan coverage for Dispose with Wait, Reset, Set, and
  IsSet reads.
- They omit infinite waits, multiple waiter release, Set/Reset races, spin
  count/WaitHandle property parity, and lifecycle of a waiter already blocked
  when Dispose occurs.
- No test verifies that Set-after-disposal remains permitted, so a future
  overzealous disposal guard could accidentally regress the current .NET
  behavior.

## Final assessment

SR-AUD-207 extends here through a TSan-confirmed data race. No production or
test source was changed.


---

## Remediation record — ticket #1955 (2026-08-03), SR-AUD-207 member → `remediated`

This type is one of SR-AUD-207's three members; the finding's primary record is in
`SemaphoreSlim.hpp.audit.md` and the index row. `disposed_` is now `std::atomic<bool>` with a
release store in `Dispose()` and an acquire load in `ThrowIfDisposed()`.
`sizeof(ManualResetEventSlim)` 112 → 112, `alignof` 8 → 8. Scenario
`manualreseteventslim.disposed` reported one race before and none after
(`build-probe/1955_probe1_tsan_{before,after}.log`).

`set_` was already `std::atomic<bool>` and is unchanged.

### A methodology correction worth keeping

The first version of the probe used a 2000-iteration loop per thread and reported **zero**
races for `ManualResetEventSlim` and `CountdownEvent` while reporting one for the
structurally identical `ReaderWriterLockSlim`. The code was equally racy in all three; the
probe was at fault. A writer loop of trivial stores completes before a reader that must set up
a try/catch reaches its first call, so the two threads never overlap and a happens-before
detector sees nothing. Rewriting the disposal scenarios as **1500 rounds of a fresh object
with exactly one access per thread** made all seven reproduce. A "TSan reported nothing"
result is evidence about the probe until the probe is shown to be able to report something.
