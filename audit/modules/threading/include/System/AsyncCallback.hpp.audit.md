# Audit: `modules/threading/include/System/AsyncCallback.hpp`

## Metadata

- AUDITED: 20-line public APM callback alias, fully read.
- Validation: the Core `AsyncCallbackTests.*` cases and the Threading
  `AsyncCallbackTests.*` fixture are present; their complete fixture-source
  audits remain pending.  The related focused EventWaitHandle/WaitHandle run
  passed 9/9 on 2026-07-27.
- Reference basis: local current-.NET `AsyncCallback.cs`, audited
  `IAsyncResult.hpp`, and first-party APM consumer search.

## Assessment

The `std::function<void(IAsyncResult&)>` alias correctly represents an
invocable native callback over the local borrowed-result adaptation.  No
first-party Begin/End producer invokes it; it is currently exercised only by
test fixture types.  The C++ type necessarily uses a reference rather than a
managed nullable object reference.

## Other missing assertions and diagnostics

- Existing fixtures check empty/nonempty state and a normal invocation, but
  omit callback exceptions, reentrancy, lifetime after completion, and a real
  APM producer.
- Invoking an empty `std::function` throws `std::bad_function_call`, not a
  managed null-delegate failure.  Callers are expected to test the native
  boolean state before invocation; no consumer or standalone defect was found.
- No test establishes whether callback delivery may be inline, concurrent, or
  exactly once because the port has no Begin/End operation to bind it to.

## Final assessment

The alias is coherent for the currently unconsumed native APM adapter.  No new
finding and no source or test change.
