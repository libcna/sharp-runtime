# Audit: `modules/threading-tasks/include/System/Threading/Tasks/Parallel.hpp`

## Metadata

- AUDITED: Parallel options, loop state/result, batched For/ForEach, and
  Invoke exception collection.
- Validation: `ParallelTests.*`, `ParallelWithLoopStateTests.*`, and
  `ParallelLoopResultTests.*` passed 19/19 within the 171/171 module run on
  2026-07-27.  Direct C++20/current-.NET 10 probes used zero and -2 maximum
  degree values with a valid one-iteration body.
- Reference basis: current .NET 10 `ParallelOptions.MaxDegreeOfParallelism`
  validation and `Parallel.For` behavior.  The documented ascending-batch
  Break/Stop adaptation is not duplicated as a finding.

## SR-AUD-232 — medium — invalid maximum degrees are silently converted to hardware concurrency instead of rejected

The options overload changes every `MaxDegreeOfParallelism <= 0` to
`hardware_concurrency()`.  Consequently native calls with zero and -2 both
return a completed loop (`parallel_zero_valid_body_completed=1` and
`parallel_minus_two_valid_body_completed=1`).  The identical current-.NET
calls throw `ArgumentOutOfRangeException`; only -1 is the valid special value.
The unchecked conversion also obscures a caller's configuration error by
running work at a potentially much higher degree than requested.

## Assessment

The implemented batching avoids the former one-thread-per-item behavior in the
loop overloads and collects thrown worker exceptions.  Public options are still
accepted outside their managed domain.

## Other missing assertions and diagnostics

- Add zero, less-than--1, -1, one, and oversized degree tests; assert exact
  argument diagnostics and that invalid options run no body.
- Add null-equivalent body/action checks for every Parallel entry point;
  currently an empty `std::function` becomes an aggregate of delayed
  `bad_function_call` exceptions.
- Stress `Invoke`, which still launches every initializer-list action at once,
  and test thread-creation failure/aggregation, Break versus Stop under several
  in-flight batches, exception plus Stop races, and nested loops.

## Final assessment

SR-AUD-232 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed during this audit.
