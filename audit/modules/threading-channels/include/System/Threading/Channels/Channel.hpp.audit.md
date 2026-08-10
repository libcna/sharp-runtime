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


---

## Correction and remediation -- ticket #1967, 2026-08-03 (cause TC-C)

*Audit text above preserved verbatim; this section is appended.*

**Evidence:** `build-probe/1967_probe1_channel_readasync_closed.cpp` (19 cases) with logs
`1967_probe1_before.log` / `1967_probe1_after.log` / `1967_probe1_asan.log`, and the
round-based concurrency probe `build-probe/1967_probe2_channel_tsan.cpp` with logs
`1967_probe2_tsan_before.log` / `1967_probe2_tsan.log`.

### The finding reproduced exactly

`fifo.readasync.errorclosed` and `prio.readasync.errorclosed` both printed
`std::exception("boom")/inner=none` before the repair and
`ChannelClosedException("The channel has been closed.")/inner=std::exception("boom")` after.
The three sibling paths kept printing the cause unwrapped in both runs, as the finding says
they should.

### C1 -- `WriteAsync` has the identical defect, which the finding does not name

The report states *"only ReadAsync loses its API-specific closed channel boundary"*.
Measured, that is wrong: `fifo.writeasync.cleanclosed` gave a `ChannelClosedException`, but
`fifo.writeasync.errorclosed` gave `std::exception("boom")/inner=none` -- the same
divergence, at the writer's mirror-image entry point, produced by the same
`if (!WaitToXAsync().getResultProperty()) throw ChannelClosedException();` shape. .NET
routes both `ChannelReader<T>.ReadAsync` and `ChannelWriter<T>.WriteAsync` through
`ChannelUtilities.GetInvalidCompletionValueTask`, so both owe the wrapper.

It is repaired **with** `ReadAsync`, as the same defect at a second site rather than a new
one -- the same treatment SR-AUD-008 ("six sites and five public doors, not the two the
audit recorded") and SR-AUD-183 ("three non-terminating shapes, not one") received.
**No new `SR-AUD-*` identifier is issued and audit numbering stays frozen at 364.**

### C2 -- a `ChannelClosedException` from a subclass is passed through, not double-wrapped

`ReadAsync`/`WriteAsync` are `virtual` and `WaitToReadAsync`/`WaitToWriteAsync` are pure
virtual, so a consumer subclass may already report the API-specific type. Both repairs
rethrow such an exception unchanged rather than nesting it inside a second
`ChannelClosedException`.

### C3 -- buffered items still drain first, and that had to be preserved deliberately

`fifo.readasync.errorclosed_but_buffered` returns 7 and
`prio.readasync.errorclosed_but_buffered` returns 3, before and after. `TryRead` runs before
the wait, so an error-completed channel that still holds items hands them over before
reporting the closure -- which is .NET's behaviour and is easy to break by hoisting the
completion check.

### Concurrency evidence

`1967_probe2_channel_tsan.cpp` runs 400 rounds each of four scenarios on a fresh channel per
round, with the completing thread released by a shared latch: one blocked reader, four
blocked readers, three writers blocked on a full bounded channel, and a mixed
`ReadAsync` + `WaitToReadAsync` pair observing the same completion on a prioritized channel.

Per `docs/ThreadingNamespaceReviewPlan.md` §19.4, the probe was first shown **capable** of
scheduling the interleaving: against the pre-fix header it reported **3,600 wrong outcomes
out of 3,600**; against the repaired header, **0**. ThreadSanitizer reported **0 data races
in both runs**, which is the correct result -- this is an exception-contract defect, not a
race -- and the pre-fix run is what proves the zero is about the code rather than about the
probe. 21 `__tsan` symbols; every translation unit on the racing path was compiled from
source with `-fsanitize=thread`, with no archive linked in.

ASan + UBSan + LSan over the 19-case probe: **0 reports**, exit 0, outcomes identical to the
plain run (34 sanitizer symbols vs 0 in the plain binary). The leak check matters here
because the repair keeps the producer's `std::exception_ptr` alive inside a new exception
object.

### Result

`SharpRuntimeTests_Threading_Channels` **39 -> 52**. `ChannelReader` keeps its
`enable_shared_from_this` lifetime design untouched. No public signature, object layout,
vtable, `noexcept` specification or component edge changed.

**SR-AUD-234: `confirmed` -> `remediated` (#1967, 2026-08-03).**
