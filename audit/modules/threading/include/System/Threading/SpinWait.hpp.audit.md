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
