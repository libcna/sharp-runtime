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
