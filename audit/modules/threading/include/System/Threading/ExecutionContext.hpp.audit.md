# Audit: `modules/threading/include/System/Threading/ExecutionContext.hpp`

## Metadata

- AUDITED: 67-line documented minimal `ExecutionContext` adapter, including
  capture/copy/disposal, flow controls, restore operations, and `Run` input
  handling.
- Validation: `ExecutionContextTests.*` passed 4/4 on 2026-07-27.  Direct
  C++20 and current .NET 10 probes exercised `Run(nullptr, validCallback,
  nullptr)` and `Run(nullptr, emptyCallback, nullptr)`.
- Reference basis: current .NET 10 `ExecutionContext.Run` validation and the
  local header's explicit no-async-flow design note.

## SR-AUD-215 — medium — Run silently accepts a null execution context instead of rejecting an impossible managed invocation

The native `Run` discards its `ExecutionContext*` parameter and either invokes
the callback or returns silently.  Both direct C++ cases — a null context with
a valid callback and a null context with an empty `std::function` — print
`normal`.  The corresponding current-.NET 10 cases each throw
`System.InvalidOperationException` before callback invocation because `Run`
cannot execute a null context.  The local fixture instead locks in
`ExecutionContext::Run(nullptr, callback, nullptr)` as a success path, even
though its own `Capture()` always supplies `nullptr`.

This is distinct from the documented absence of asynchronous context flow:
minimal native behavior may avoid CLR capture machinery, but it must not claim
that a managed-invalid public call completed normally without a diagnostic.

## Assessment

`Capture()` returning null and the flow suppression no-ops are explicit,
documented native adaptations.  The argument-validation result from `Run` is
observable and its current test encodes the incompatible success behavior.

## Other missing assertions and diagnostics

- `Run_InvokesCallbackSynchronously` does not establish null-context,
  null-callback, callback-exception, state-identity, or callback-once
  behavior; its null-context success expectation masks SR-AUD-215.
- Tests omit CreateCopy identity/lifetime, Dispose interaction, Restore input
  validation, nested suppress/undo behavior, and explicit evidence for the
  no-flow asynchronous-boundary adaptation.

## Final assessment

SR-AUD-215 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed.


---

## Verification note -- ticket #1971, 2026-08-03: SR-AUD-215 is NOT compatible, and stays open

*Audit text above preserved verbatim. **SR-AUD-215 remains `confirmed`.***

`docs/ThreadingNamespaceReviewPlan.md` §20.3 grouped this finding with SR-AUD-214 and
SR-AUD-189 as *"compatible, no approval needed"*, while attaching the caveat that *"the port's
own `Capture()` always returns `nullptr` … and this must be checked before landing"*. Ticket
#1971 performed that check before splitting Group A, and the caveat turns out to be
disqualifying.

Measured (`build-probe/1971_probe1_group_a.cpp`):

- `ec.capture_is_null=true` -- `Capture()` returns `nullptr` unconditionally, by design;
- `ec.default_ctor_is_public=false` -- and a compile probe confirms it: enabling
  `-DPROBE_TRY_CONSTRUCT=1` fails with *"'ExecutionContext::ExecutionContext()' is private
  within this context"*;
- `CreateCopy()` is a **non-static** member, so it needs an instance that only `Capture()` could
  supply.

**There is therefore no reachable way for a consumer to obtain a non-null
`ExecutionContext*`.** Rejecting a null context would make `Run` throw for *every* call that can
be written, including the canonical `Run(Capture(), callback, state)` — which works today
(`ec.run_with_capture_result=invoked`). That is not "the only calls that change outcome are ones
.NET rejects": it is mandatory downstream migration with **no working alternative**, and it
converts a documented, working port API into a dead one.

The finding is real — the audit's managed comparison stands — but landing it requires pairing it
with a `Capture()` that returns a real context, which is an ownership and lifetime design
decision (who owns the returned pointer, when is it freed, what does `CreateCopy` mean), not a
validation change. It therefore **stays with the blocked #1958**, whose approval request (A) is
re-worded accordingly.

Two regressions in
`modules/threading/tests/System/Threading/ThreadingPublicShapeTests.cpp` pin the current
contract, so the exclusion is testable rather than only documented: one asserts that no non-null
context is obtainable (and fails if the default constructor ever becomes public, which would
re-open the question), and one asserts that the single writable `Run` call still invokes its
callback.
