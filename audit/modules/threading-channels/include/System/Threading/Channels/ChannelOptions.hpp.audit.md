# Audit: `modules/threading-channels/include/System/Threading/Channels/ChannelOptions.hpp`

## Metadata

- AUDITED: base options, bounded capacity/full mode, and prioritized comparer
  options.
- Validation: module fixture passed 39/39 on 2026-07-27.  Direct C++20/current
  .NET 10 probes covered capacity zero, negative capacity, and a cast full-mode
  value of 99.
- Reference basis: current .NET 10 `BoundedChannelOptions` constructor and
  `FullMode` setter contracts.

## SR-AUD-233 — medium — valid zero-capacity channels are silently changed into one-element buffers

The constructor accepts zero and `ChannelState::effectiveCapacity()` rewrites
it to one.  Native `CreateBounded(0).Writer->TryWrite(7)` returns true and the
reader immediately retrieves 7.  Current .NET also accepts zero, but its
equivalent `TryWrite` and `TryRead` both return false without a matching
waiting peer.  A zero-capacity channel is a rendezvous channel, not an
unannounced capacity-one buffer; the two existing zero-capacity tests lock in
the incompatible native behavior.

## SR-AUD-235 — medium — an invalid FullMode is accepted and lets a bounded channel exceed its advertised capacity

`FullMode` is a mutable public enum field and neither option assignment nor
factory construction validates it.  With capacity 1 and
`static_cast<BoundedChannelFullMode>(99)`, native writes of 1 and 2 both
succeed and Count becomes 2 because no switch arm handles the value.  Current
.NET throws `ArgumentOutOfRangeException` when the invalid value is assigned.
This permits a caller or deserialized value to defeat the bounded-memory
contract.

## Assessment

Negative capacity is rejected and normal full modes work in the test fixture.
The two non-default option boundaries above are publicly observable and not
described as adaptations.

## Other missing assertions and diagnostics

- Add a rendezvous test for zero capacity with and without a blocked peer;
  remove the capacity-one expectation from zero-capacity regressions.
- Require setter/factory rejection for every invalid FullMode value; test
  options mutation after construction, comparer emptiness/default ordering,
  and all SingleReader/SingleWriter/AllowSynchronousContinuations promises.

## Final assessment

SR-AUD-233 and SR-AUD-235 are confirmed by direct C++/current-.NET comparison.
No production or test source was changed during this audit.


---

## Correction and remediation -- ticket #1968, 2026-08-03 (cause TC-B/2)

*Audit text above preserved verbatim; this section is appended.*
**SR-AUD-235 is untouched by this ticket and remains `confirmed`** -- its repair needs the
property pair that gates it as #1969.

**Evidence:** `build-probe/1968_probe1_zero_capacity.cpp` (24 behavioural cases + a layout
dump) with logs `1968_probe1_before.log` / `1968_probe1_after.log` /
`1968_probe1_asan.log`, and the round-based concurrency probe
`build-probe/1968_probe2_rendezvous_tsan.cpp` with logs
`1968_probe2_tsan_before.log` / `1968_probe2_tsan.log`.

### The finding reproduced exactly

| case | before | after |
|---|---|---|
| `TryWrite` with no peer | **`true`**, `Count` becomes 1 | `false`, `Count` stays 0 |
| `TryRead` after that write | `true`, value 7 | `false` |
| `WriteAsync` with no reader | **completes immediately** | blocks until a reader arrives |
| `TryWrite` with a parked reader | `true` | `true`, item handed to that reader |
| every non-zero capacity, unbounded, and the drop modes at capacity >= 1 | -- | **byte-identical** |

### C1 -- the header's own comment asserted the OPPOSITE of this finding, and it had to be resolved

`Channel.hpp`'s `effectiveCapacity()` carried a comment claiming it was *"verified against
BoundedChannel.cs's TryWrite"* that a capacity-0 channel *"still buffers one item when no
reader is synchronously blocked waiting -- i.e. a capacity-0 channel is observably equivalent
to a capacity-1 channel for every publicly-visible TryWrite/WaitToWriteAsync outcome"*. That
is a direct contradiction of SR-AUD-233, which records a managed probe showing
`TryWrite`/`TryRead` both returning false without a peer. Only one can be right, and the
repository contained both.

The finding was preferred, for reasons stated so a later reader can re-open the question
rather than re-derive it:

1. The finding's evidence is a **behavioural managed probe** against current .NET; the
   comment's is a **reading of .NET source**. A measurement of the observable beats a reading
   when the two disagree about the observable.
2. The finding is the **later** record, and ticket #1964's review re-checked it against
   current source before opening this ticket.
3. The comment is self-undermining: it concedes the .NET implementation has a
   direct-hand-off-to-a-blocked-reader path and then asserts that path *"doesn't change any
   return value"*, which is exactly the claim SR-AUD-233's probe contradicts.

**Reference-evidence limitation, stated plainly:** `/rv/tmp/runtime/src/libraries/` is **not
present in this environment**, so `BoundedChannel.cs` could not be re-read to adjudicate
directly. The decision rests on the audit's managed probe. Were that probe wrong, the repair
is confined to `ChannelState`'s capacity predicates plus `TryWrite`/`WaitToWriteAsync`/
`WaitToReadAsync` and is revertible without touching any signature. The contradicting comment
has been **replaced**, not silently dropped, so the header no longer carries a claim the
repository has decided against.

### C2 -- the drop modes at capacity 0 had no evidence either way, and are reasoned

Measured before: `DropWrite`, `DropNewest` and `DropOldest` at capacity 0 each accepted a
write and left `Count == 1`. The audit's probe covers only the default `Wait` mode. The port
now discards the item and keeps `Count == 0` for all three, because that is the only reading
consistent with a channel that has no room to hold anything: `DropWrite` drops the incoming
item by definition, and with an always-empty buffer the incoming item is simultaneously the
newest and the oldest. Recorded as reasoned rather than measured.

### C3 -- object layout: one internal type grew, no public type did

Measured (LP64):

| type | before | after |
|---|---|---|
| `Channel<int>` | 32 / 8 | **32 / 8** |
| `ChannelReader<int>` | 24 / 8 | **24 / 8** |
| `ChannelWriter<int>` | 24 / 8 | **24 / 8** |
| `ChannelOptions` | 16 / 8 | **16 / 8** |
| `BoundedChannelOptions` | 24 / 8 | **24 / 8** |
| `detail::ChannelReaderImpl<int>` | 40 / 8 | **40 / 8** |
| `detail::ChannelWriterImpl<int>` | 40 / 8 | **40 / 8** |
| `detail::ChannelState<int>` | 240 / 8 | **248 / 8** |

The one growth is `detail::ChannelState<T>`, which gained the `waitingReaders` counter. It is
a `detail`-namespace type, appears in no public signature, is only ever reached through a
`shared_ptr` held inside the reader/writer implementations, and lives in an **INTERFACE**
target -- so there is no archive against which a stale layout could be linked. A
`static_assert` gate on all five public types is now part of the suite.

### Concurrency evidence

`1968_probe2_rendezvous_tsan.cpp` runs 300 rounds each of four scenarios on a fresh channel:
one reader and one producer, four readers and four producers, a completion racing a parked
reader, and three writers blocked in `WaitToWriteAsync` racing an arriving reader. Every round
asserts exact delivery, `Count == 0`, and that no further write is accepted once every peer is
served.

Per `docs/ThreadingNamespaceReviewPlan.md` §19.4 the probe's capability was proved first:
against the pre-fix header it reported **600 wrong outcomes**, all from the two hand-off
scenarios, and **0** against the repaired header. The close-while-parked and
`WaitToWriteAsync`-race scenarios reported **0 in both runs** -- honestly recorded, because a
capacity-1 buffer also delivers every item, so only the *"nothing may be buffered"* assertions
discriminate between the two behaviours. ThreadSanitizer reported **0 data races in both
runs**, with every translation unit on the racing path compiled from source
(21 `__tsan` symbols, no archive linked). ASan + UBSan + LSan over the surface probe: **0
reports**, outcomes identical to the plain run.

### Test rewrite, identified as required

`ZeroCapacityChannel_TryWrite_SucceedsOnceThenBlocksLikeCapacityOne` and
`ZeroCapacityChannel_WriteAsync_UnblocksOnceReaderDrains` asserted the incorrect behaviour and
were **replaced, not deleted**: each old test's concern -- a synchronous `TryWrite`, and a
`WriteAsync` that completes when a peer arrives -- still has a test asserting the corrected
contract, and the replacement is called out in the file and in the commit.
`SharpRuntimeTests_Threading_Channels` **52 -> 64**.

**SR-AUD-233: `confirmed` -> `remediated` (#1968, 2026-08-03).**
