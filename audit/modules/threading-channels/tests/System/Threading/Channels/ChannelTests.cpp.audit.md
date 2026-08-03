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


---

## Note -- tickets #1967 and #1968, 2026-08-03

Three of this report's four "Other missing assertions" items are now landed. The fixture went
**39 -> 64** across the two tickets.

- **"Replace zero-capacity capacity-one expectations with a managed rendezvous comparison
  (SR-AUD-233)"** -- done by **#1968**. `ZeroCapacityChannel_TryWrite_SucceedsOnceThenBlocks-
  LikeCapacityOne` and `ZeroCapacityChannel_WriteAsync_UnblocksOnceReaderDrains` were
  **replaced, not deleted**: each old test's concern still has a test asserting the corrected
  contract, and twelve further rendezvous cases were added (no peer, a parked reader handed the
  item, one write per waiting peer, four readers, `WaitToWriteAsync` unblocking on a reader's
  arrival, every drop mode keeping `Count` at 0, a parked reader released by a clean and by an
  error completion, non-zero capacities unchanged, the prioritized shape unaffected, and a
  `static_assert` layout gate).
- **"Add FIFO/prioritized ReadAsync close-error tests asserting ChannelClosedException plus its
  inner completion error (SR-AUD-234)"** -- done by **#1967**, for both shapes, plus the
  writer's mirror-image `WriteAsync` path that the finding does not name, plus assertions that
  `WaitToReadAsync`/`WaitToWriteAsync`/`Completion` keep exposing the cause **unwrapped**.
- **"TSan rendezvous/race coverage"** -- done as retained probes rather than as fixture cases,
  because they need hundreds of rounds to sample the interleavings:
  `build-probe/1967_probe2_channel_tsan.cpp` and
  `build-probe/1968_probe2_rendezvous_tsan.cpp`, each with a before/after log proving the probe
  schedules the race it claims to.

**Still open, deliberately:** the invalid-`FullMode` items belong to SR-AUD-235, whose repair
needs a property pair and is gated as ticket **#1969**. Completion-task identity, per-drop-mode
blocked-writer behaviour and comparer failures remain uncovered.
