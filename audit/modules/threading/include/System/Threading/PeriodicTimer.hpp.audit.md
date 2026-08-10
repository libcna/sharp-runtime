# Audit: `modules/threading/include/System/Threading/PeriodicTimer.hpp`

## Metadata

- AUDITED: 82-line PeriodicTimer implementation, fully read.
- Validation: no local direct fixture exists; native period/concurrency probes
  were run. Current .NET documentation was checked for the single-consumer
  contract.

## SR-AUD-200 — medium — PeriodicTimer accepts fractional periods despite its whole-millisecond contract

The constructor states that valid input represents a whole number of
milliseconds, but checks only `ms >= 1.0` and casts to `long long`. A 1.5 ms
native probe prints `fractional=normal`, silently scheduling a 1 ms timer.
The input must be rejected rather than rounded without a diagnostic.

## SR-AUD-201 — medium — Concurrent WaitForNextTick consumers each consume the same scheduled tick

The implementation has no in-flight waiter guard. Two native threads waiting
on a 20 ms timer print `concurrent=1,1`; both wake for the same deadline and
advance schedule state. Current .NET PeriodicTimer documentation permits only
one consumer at a time, so the C++ shape silently permits a contract-invalid
multi-consumer result.

## Assessment

Finite/infinite wait and concurrent Dispose paths are otherwise clear and the
timeout upper bound is documented. No direct test covers this source.

## Final assessment

SR-AUD-200/201 are confirmed by direct probes. No source or test was changed.


---

## Ticket #1954 (2026-08-03): SR-AUD-200 examined and **deliberately not repaired** — stays `confirmed`

SR-AUD-200 was in ticket #1954's scope and is the one member of cause **T-C** that #1954 did
not change. The reasoning is recorded here rather than silently deferred, and the open
question is carried by **inactive ticket #1963**. No new `SR-AUD-*` identifier is issued;
audit numbering stays frozen at 364.

### What the finding asks for

*"The input must be rejected rather than rounded without a diagnostic."*

### Why that was not done

1. **This report carries no managed probe for the row.** Its Metadata section records
   *"native period/concurrency probes were run. Current .NET documentation was checked for the
   single-consumer contract"* — that documentation check belongs to SR-AUD-201. For
   SR-AUD-200 the stated evidence is `fractional=normal` from a **native** probe measured
   against **this port's own doc-comment**, which promises "a whole number of milliseconds in
   [1, uint.MaxValue - 1]". The divergence established is therefore between the port's
   documentation and the port's code — not between the port and .NET. Every other T-C member
   repaired in #1954 (SR-AUD-184, 205, 213) quotes a managed result in its own report.
2. **.NET appears to truncate too.** `PeriodicTimer` converts its period with
   `(long)period.TotalMilliseconds` — a truncating cast, the same idiom
   `Timer(TimerCallback, object?, TimeSpan, TimeSpan)` uses for its `dueTime`/`period` — so
   `TimeSpan.FromMilliseconds(1.5)` becomes 1 ms in .NET as well. If that is right, rejecting
   the value here would be a **narrowing away from .NET** that refuses input a caller can
   legitimately produce (`TimeSpan` tick arithmetic reaches fractional milliseconds easily),
   and the correct repair would be to fix this port's doc-comment instead.
3. **The reference could not be consulted.** `/rv/tmp/runtime/src/libraries/` is **not
   present** in the environment that ran #1954. Neither reading could be confirmed, so
   changing behaviour in either direction would have rested on recollection.

### What was done instead

Nothing was changed — not the constructor, and not the doc-comment, because if .NET does
reject then the doc-comment is right and the code is wrong, and "fix the doc" would be the
wrong half. The current behaviour is **pinned** by
`ThreadingArgumentDomainTests.PeriodicTimer_FractionalPeriod_StillTruncates_SeeTicket1963`
so that #1963 can only change it on purpose. Measured rows, identical before and after
#1954 (`build-probe/1954_probe1_argument_domain.cpp`):

| Row | Result |
|---|---|
| `periodictimer.fractional_1_5ms` | `normal` |
| `periodictimer.whole_2ms_control` | `normal` |
| `periodictimer.zero_rejected_control` | `ArgumentOutOfRangeException(period)` |

**SR-AUD-201 is untouched and remains `confirmed`** — the missing in-flight-consumer state is
cause T-E/2 and belongs to approval-gated design ticket #1957.
