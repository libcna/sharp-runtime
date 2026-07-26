# Audit: `modules/threading/include/System/Threading/AsyncLocal.hpp`

## Metadata

- AUDITED: 80-line `AsyncLocal<T>` thread-local storage adapter, including
  generated instance IDs, current-thread cleanup, no-op equal assignment, and
  value-change notification ordering.
- Validation: `AsyncLocalTests.*` passed 6/6 on 2026-07-27.  A direct C++20
  probe and a current .NET 10 counterpart set an `AsyncLocal<int>` to 5 from a
  callback that immediately reads `Value`.
- Reference basis: current .NET 10 `AsyncLocal<T>.Value` setter and
  `IAsyncLocal.OnValueChanged` observable callback ordering.

## SR-AUD-214 — medium — value-change callbacks observe the old value because the native setter commits after notification

`setValueProperty` invokes `valueChangedHandler_` before it writes `v` into
the thread-local map.  The direct native probe prints `callbackValue=0` then
`afterSet=5`; the identical current-.NET 10 program prints `callbackValue=5`
then `afterSet=5`.  Thus a handler that observes its owning `AsyncLocal<T>`
receives the advertised `CurrentValue` argument but sees a contradictory old
ambient value through the public property.  A reentrant handler write can also
be overwritten by the delayed outer `map[id_] = v` assignment.

## Assessment

The monotonically increasing key correctly prevents the prior stale-address
cross-thread collision, and equal assignment remains a true notification-free
no-op.  The callback sequencing is nevertheless observable public behavior,
not merely an implementation detail.

## Other missing assertions and diagnostics

- `AsyncLocalTests.ValueChangedHandler_Called` validates only the argument
  object; it does not read `getValueProperty()` within the callback, so it
  cannot detect SR-AUD-214.
- Tests omit callback reentrancy, exception propagation, reference/value
  equality semantics, concurrent per-thread independence, and destructor
  cleanup behavior after multiple worker threads have populated slots.
- The documented thread-local adaptation needs an explicit negative assertion
  that values do not flow through an asynchronous/native-task boundary.

## Final assessment

SR-AUD-214 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed.
