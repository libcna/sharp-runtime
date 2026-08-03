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


---

## Correction and remediation -- ticket #1971, 2026-08-03 (cause T-H, #1958 Group A)

*Audit text above preserved verbatim; this section is appended.*

**Evidence:** `build-probe/1971_probe1_group_a.cpp` with logs `1971_probe1_before.log` /
`1971_probe1_after.log` / `1971_probe1_asan.log`, and the round-based concurrency probe
`build-probe/1971_probe2_group_a_tsan.cpp` with logs `1971_probe2_tsan_before.log` /
`1971_probe2_tsan.log`.

### The finding reproduced exactly, including the half it states only in passing

`asynclocal.callback_argument=5` with `asynclocal.callback_property=0` before the repair, and
`5`/`5` after -- the handler used to be told the value was 5 while the public property still
answered 0. The report's second sentence -- *"a reentrant handler write can also be overwritten
by the delayed outer `map[id_] = v` assignment"* -- is confirmed too and is the sharper half:
`asynclocal.reentrant_write_survives=5` before (a handler writing 99 left the value at 5) and
**99** after.

### Repair

`setValueProperty` commits `map[id_] = v` **before** invoking the handler. .NET commits first
for the same reason. The equal-value no-op that precedes both is untouched and is pinned by its
own regression, since reordering the commit past an early return is the obvious way to break it.

### Concurrency

The reordering touches a thread-local map, so per-thread isolation had to be re-proved rather
than assumed. Four threads x 2,000 iterations, each writing and reading its own slot, plus four
threads each running a handler that compares `getValueProperty()` with its `CurrentValue`
argument on every change. Capability proved first per
`docs/ThreadingNamespaceReviewPlan.md` §19.4: the ordering scenario reported **8,000 violations
against the pre-fix header -- every single iteration -- and 0 after**. ThreadSanitizer: 0 data
races in both runs, fully instrumented from source (18 `__tsan` symbols, no archive linked).
ASan + UBSan + LSan over the surface probe: 0 reports.

### Consequences

No signature, object layout, vtable, `noexcept` specification or component edge changed; the
change is the order of two statements in one inline body.
`SharpRuntimeTests_Threading` **429 -> 445** across this ticket's two members.

**SR-AUD-214: `confirmed` -> `remediated` (#1971, 2026-08-03).**
