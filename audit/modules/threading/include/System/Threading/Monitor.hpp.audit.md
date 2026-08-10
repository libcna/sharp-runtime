# Audit: `modules/threading/include/System/Threading/Monitor.hpp`

## Metadata

- AUDITED: 187-line registry-backed Monitor implementation, fully read.
- Validation: complete Threading tests passed 359/359 on 2026-07-27; an
  isolated reentrant Wait/Pulse probe was run with a two-second timeout.

## SR-AUD-202 — high — Monitor.Wait deadlocks when the caller holds a recursive monitor more than once

`Wait` adopts the recursive mutex and calls `onReleasing()` once before
condition-variable wait. With two Enter calls, one recursive level remains
locked, so the signaling thread cannot Enter/Pulse. The isolated child enters
twice, waits, then attempts Enter/Pulse from a second thread; it reaches the
two-second timeout (exit 124) rather than completing. .NET Monitor.Wait fully
releases and restores recursive ownership.

## Assessment

The pointer registry provides real ordinary mutual exclusion and the audited
fixture covers depth-one wait/pulse behavior. The header itself documents the
depth-one limitation, but a public Monitor operation that deadlocks valid
recursive ownership is still a high contract failure.

## Other missing assertions and diagnostics

- Tests omit reentrant Wait (SR-AUD-202), null object arguments, invalid
  negative timed waits/TryEnter values, timed pulse races, registry growth and
  pointer-address reuse after object destruction.
- The process-lifetime registry retains a State for every distinct pointer;
  no bounded-cache, reclamation, or long-running allocation diagnostic exists.

## Final assessment

SR-AUD-202 is confirmed by the bounded deadlock probe. No source or test was
changed.
