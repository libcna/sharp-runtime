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
