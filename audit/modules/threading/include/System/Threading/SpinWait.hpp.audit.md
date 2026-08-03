# Audit: `modules/threading/include/System/Threading/SpinWait.hpp`

## Metadata

- AUDITED: 65-line spin/yield helper, fully read.
- Validation: focused `SpinWaitTests.*` passed 9/9 on 2026-07-27; direct C++
  and .NET 10 probes covered invalid timed SpinUntil and an empty/null
  condition.

## SR-AUD-213 — medium — `SpinUntil` skips required timeout and callback validation

The timed overload accepts `-2` and returns `false` instead of throwing; the
matching .NET 10 `SpinWait.SpinUntil(() => false, -2)` throws
`ArgumentOutOfRangeException`. It also accepts an empty `std::function` until
invocation, then exposes native `bad_function_call`; .NET rejects a null
condition immediately with `ArgumentNullException`. Neither invalid boundary
has a direct fixture, so green normal spin tests cannot detect the divergence.

## Assessment

The basic count/reset and ordinary finite/infinite spin paths are coherent in
the reviewed tests. Public validation is incomplete at the method boundary.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-213 invalid timeouts and empty condition diagnostics for
  both SpinUntil overloads.
- They omit `SpinOnce(sleep1Threshold)` validation/effect, yield scheduling,
  Count overflow, timeout duration bounds, and callable exceptions after one
  or more spin iterations.
- No test distinguishes single-core/platform yield behavior from the local
  fixed threshold adaptation.

## Final assessment

SR-AUD-213 is confirmed by direct current-.NET comparison. No production or
test source was changed.


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-213 **callable half only**

SR-AUD-213 is **split by cause** and stays `confirmed` until both halves land.

**Callable half — done.** Cause **T-B** (CCF-011 in `modules/threading`). Both `SpinUntil`
overloads now throw `System::ArgumentNullException("condition")` before the first spin. The
infinite-timeout case is the one that matters most: with `millisecondsTimeout == -1` there is
no deadline to end the loop, so a deferred check meant an uninvocable condition spun forever
rather than failing. `.NET`'s parameterless overload delegates to the timed one with
`Timeout.Infinite`, whose `ArgumentNullException.ThrowIfNull(condition)` runs before any
spinning; this port's two overloads own separate loops, so each carries the check.

**Timeout half — still open, ticket #1954** (cause T-C): `SpinUntil(cond, -2)` still returns
`false` where .NET throws `ArgumentOutOfRangeException`. When #1954 lands, its check must go
**above** the condition check so the two run in .NET's order
(`ThrowIfLessThan(millisecondsTimeout, -1)` then `ThrowIfNull(condition)`). Until then the
port reports the condition error first for a call that is invalid in both ways — recorded
here rather than left to be discovered.

Evidence: `spinwait.spinuntil_empty` and `spinwait.spinuntil_timed_empty` both moved from
`bad_function_call` to `ArgumentNullException|Value cannot be null. (Parameter 'condition')`;
the two controls are unchanged. Tests: `ThreadingEmptyCallableTests.SpinWait_*`, including the
`-1` case that would hang a broken implementation.
