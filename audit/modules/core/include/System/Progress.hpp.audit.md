# Audit: `modules/core/include/System/Progress.hpp`

## Metadata

- Audit status: AUDITED (84-line public template header, fully read).
- Validation: `ProgressTest.*` passed 9/9 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-progress-audit-probe.cpp` adds an empty
  handler and prints `bad_function_call` when it reports a value.

## Assessment

The type clearly documents its synchronous callback adaptation, validates an
empty constructor handler as `ArgumentNullException`, preserves registration
order, and gives subclasses an `OnReport` hook.  It does not retain a
synchronization context, which is an explicit behavioral difference rather
than an implicit race claim.

## SR-AUD-058 — medium — empty progress-event subscription becomes a delayed `std::bad_function_call`

`addProgressChangedHandler` unconditionally appends its `std::function` to the
callback vector (`Progress.hpp:80`), but `OnReport` calls every stored element
without checking truthiness (`:38`).  Therefore an empty subscription succeeds
at registration and a later unrelated `Report` throws native
`std::bad_function_call`; the standalone probe confirms this exact result.

Current local .NET `Progress.cs` represents `ProgressChanged` as a nullable
event delegate and invokes it conditionally.  C# event addition of a null
delegate is a no-op, so it cannot create a later invocation failure.  The
constructor already has a distinct, correctly documented null-handler rule;
the event-like subscription surface needs its own explicit empty-callback
policy rather than delayed native failure.

## Other missing assertions and diagnostics

- The direct suite tests empty constructor rejection but not an empty added
  handler, a handler that throws, reentrant registration, or handler mutation
  during a report.
- The synchronous adaptation has no thread-affinity/concurrent-report test;
  the .NET captured-context behavior is intentionally not reproduced.

## Final assessment

Ordinary synchronous progress delivery is covered, but empty event-style
subscription has a confirmed diagnostic and parity defect.  No source or test
was modified during this audit.
