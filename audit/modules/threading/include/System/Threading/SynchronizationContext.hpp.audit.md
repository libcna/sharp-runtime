# Audit: `modules/threading/include/System/Threading/SynchronizationContext.hpp`

## Metadata

- AUDITED: 57-line default synchronization-context adapter, including
  synchronous/asynchronous dispatch and thread-local Current storage.
- Validation: `SynchronizationContextTests.*` passed 6/6 on 2026-07-27.  A
  direct C++20/current-.NET 10 probe exercised empty callbacks and Current
  lifetime; a dedicated native build used ASan with
  `-fsanitize-address-use-after-scope`.
- Reference basis: current .NET 10 `SynchronizationContext` Current, Send,
  and Post behavior, plus the installed API reference surface.

## SR-AUD-221 — high — Current retains a raw pointer after its synchronization context has been destroyed

`SetSynchronizationContext` stores a non-owning raw pointer in a
`thread_local` slot and has no destruction/reset hook.  Set Current to a stack
derived context, leave its scope, then call `Current->Send`: ASan reports a
stack-use-after-scope at the virtual call.  The direct C++ probe also shows
`sync_currentAfterScope=1`.  The matching managed probe drops its local
reference, forces GC, and still reports both non-null Current and a live weak
reference because .NET retains the active context.  The C++ adaptation instead
turns ordinary Current usage into a dangling-pointer crash.

## SR-AUD-222 — medium — Send silently discards an empty callback where the managed default path faults

The C++ `Send({}, nullptr)` path returns normally because it conditionally
invokes only nonempty `std::function` values.  Current .NET 10 base
`SynchronizationContext.Send(null, null)` throws
`System.NullReferenceException`; its `Post(null, null)` does return normally,
which the native implementation matches.  The local integration test locks in
only the Post no-op and its nominal Send test has the separate SR-AUD-013
tautological assertion, so neither distinguishes this incorrect Send result.

## Assessment

Default Post asynchronously queues normal callbacks and Send invokes normal
callbacks synchronously; the focused suite confirms those happy paths.  The
raw current-context lifetime and empty-Send result remain reachable defects.

## Other missing assertions and diagnostics

- Add a lifetime-safe Current test that establishes the chosen native ownership
  rule, clears it deterministically, and runs under ASan; never leave a raw
  stack pointer in Current between tests.
- Assert a visible Send state change (SR-AUD-013), Send/ Post empty callback
  behavior separately, callback exception propagation, state identity, and
  exactly-once asynchronous dispatch.
- The native surface omits `CreateCopy`, operation notifications, wait
  notification, and `Wait`; document the intentional subset or add explicit
  unsupported-operation diagnostics during remediation.

## Final assessment

SR-AUD-221 is ASan-confirmed and SR-AUD-222 is confirmed by direct
C++/current-.NET comparison.  No production or test source was changed.


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-222 → `remediated`

Cause **T-B** of `docs/ThreadingNamespaceReviewPlan.md` — the second of the two members whose
.NET answer is `NullReferenceException` rather than `ArgumentNullException`.

`SynchronizationContext.Send` is declared in the reference as
`public virtual void Send(SendOrPostCallback d, object? state) => d(state);` with no
validation, so a null delegate faults with `NullReferenceException` **synchronously, on the
calling thread** — precisely what this report measured. The base implementation here now
throws `System::NullReferenceException` instead of testing the callable and returning, so the
observable matches .NET rather than being replaced with a different diagnostic. An overriding
context still supplies its own behaviour, exactly as in .NET.

`Post` is deliberately unchanged: .NET's base `Post` hands the delegate to
`ThreadPool.QueueUserWorkItem` and so returns normally to its caller for a null delegate too,
which this report already records as matching. A doc-comment now states that, so the asymmetry
is not read as an oversight.

Evidence: `synccontext.send_empty` moved from `normal` to
`NullReferenceException|Object reference not set to an instance of an object.`;
`synccontext.send_control` and `synccontext.post_empty_control` unchanged. Tests:
`ThreadingEmptyCallableTests.SynchronizationContext_*`.

**SR-AUD-221 is untouched and remains `confirmed`** — the non-owning raw `Current` pointer is
cause T-D (CCF-019) and belongs to design ticket #1959, which is approval-gated.
