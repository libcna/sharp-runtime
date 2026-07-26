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
