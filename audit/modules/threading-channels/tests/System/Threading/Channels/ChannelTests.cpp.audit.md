# Audit: `modules/threading-channels/tests/System/Threading/Channels/ChannelTests.cpp`

## Metadata

- AUDITED: 549-line FIFO, bounded, prioritized, completion, and concurrent
  channel regression fixture.
- Validation: the complete executable passed 39/39 on 2026-07-27.

## Assessment

The fixture meaningfully covers ordering, ordinary capacity/drop modes,
completion, concurrent fan-in/fan-out, lifetime preservation, and former
lost-wakeup paths.  Its two zero-capacity tests require the incompatible
capacity-one behavior, and it omits the closed-with-error ReadAsync path and
invalid option values.

## Other missing assertions and diagnostics

- Replace zero-capacity capacity-one expectations with a managed rendezvous
  comparison (SR-AUD-233).
- Add invalid FullMode assignment/factory diagnostics and a no-over-capacity
  assertion (SR-AUD-235).
- Add FIFO/prioritized ReadAsync close-error tests asserting
  ChannelClosedException plus its inner completion error (SR-AUD-234).
- Exercise task identity for Completion, blocking readers/writers during
  close, all drop-mode writer waits, custom comparer failures, and TSan
  rendezvous/race coverage.

## Final assessment

The green suite misses all three confirmed Channels findings.  No source or
test was changed during this audit.
