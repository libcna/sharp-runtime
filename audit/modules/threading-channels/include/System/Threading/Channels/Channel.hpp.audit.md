# Audit: `modules/threading-channels/include/System/Threading/Channels/Channel.hpp`

## Metadata

- AUDITED: reader/writer base operations, FIFO/bounded/prioritized state,
  completion, blocking async bridges, concurrent access, and factories.
- Validation: `SharpRuntimeTests_Threading_Channels` passed 39/39 on
  2026-07-27.  Direct C++20/current-.NET 10 probes compared zero capacity and
  error completion through WaitToRead, ReadAsync, WaitToWrite, and Completion.
- Reference basis: current .NET 10 `ChannelReader<T>.ReadAsync`, channel close
  semantics, and zero-capacity bounded channel behavior.

## SR-AUD-234 — medium — ReadAsync leaks the close error instead of throwing ChannelClosedException with it as the cause

When a writer completes an empty channel with `runtime_error("boom")`, native
`ReadAsync().getResultProperty()` rethrows `boom` directly.  Current .NET
`await Reader.ReadAsync()` throws `ChannelClosedException` whose
`InnerException` is the supplied `InvalidOperationException`.  The native
WaitToReadAsync, WaitToWriteAsync, and Completion paths correctly expose the
underlying completion error; only ReadAsync loses its API-specific closed
channel boundary even though `ChannelClosedException` has the required
inner-exception constructor.

## Assessment

The mutex/condition-variable state protects FIFO and prioritized queues, and
the tests cover multi-producer/multi-consumer execution plus prior wakeup and
reader/writer lifetime repairs.  Capacity-zero and invalid-full-mode handling
are SR-AUD-233/235 in the paired options report.  The missing ReadAsync wrapper
is a distinct caller-visible exception contract violation.

## Other missing assertions and diagnostics

- Add error-completion ReadAsync tests for FIFO and prioritized channels that
  require ChannelClosedException and verify its retained cause.
- Test Completion task identity/caching, repeated Completion reads, blocked
  writer behavior for every drop mode, close/read/write races, external
  shared_ptr subclasses using ReadAsync/WriteAsync, comparer exceptions, and
  zero-capacity rendezvous under TSan.

## Final assessment

SR-AUD-234 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed during this audit.
