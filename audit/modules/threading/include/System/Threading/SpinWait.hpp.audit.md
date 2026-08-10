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


---

## Remediation record — ticket #1954 (2026-08-03), SR-AUD-213 **timeout half** → finding now `remediated`

The callable half landed with #1951 (recorded above). #1954 adds the timeout half, so
**SR-AUD-213 is now fully remediated**.

`SpinUntil(condition, millisecondsTimeout)` opens with

```cpp
System::ArgumentOutOfRangeException::ThrowIfLessThan(
    millisecondsTimeout, static_cast<SharpRuntime::intcs>(-1), "millisecondsTimeout");
```

placed **above** the `condition` check added by #1951, reproducing .NET's
`ArgumentOutOfRangeException.ThrowIfLessThan(millisecondsTimeout, -1)` →
`ArgumentNullException.ThrowIfNull(condition)` order. That ordering was the explicit
requirement #1951 left behind in `docs/ThreadingNamespaceReviewPlan.md` §17.3: between the two
tickets, a call invalid in *both* ways reported the condition where .NET reports the timeout.
Probe row `spinwait.timeout_minus2_and_empty_condition` moved from
`ArgumentException|... (Parameter 'condition')` to
`ArgumentOutOfRangeException|param=millisecondsTimeout`, and
`ThreadingArgumentDomainTests.SpinWait_TimeoutCheckPrecedesConditionCheck` pins it.

Why the old result was worse than a missing diagnostic: `SpinUntil(cond, -2)` returned
**`false`**, which is byte-identical to a legitimate expiry, so a caller that passed a bad
timeout saw a plausible "condition never became true" instead of an error. Probe row
`spinwait.timeout_minus2` moved from `false` to
`ArgumentOutOfRangeException(millisecondsTimeout)`.

Both valid special cases are unchanged and asserted: `-1` still waits indefinitely and `0`
still makes exactly one attempt
(`ThreadingArgumentDomainTests.SpinWait_ValidTimeouts_Unchanged`).
