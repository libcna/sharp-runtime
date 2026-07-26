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
