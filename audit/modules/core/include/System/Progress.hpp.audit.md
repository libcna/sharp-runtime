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

---

## SR-AUD-058 — REMEDIATED (ticket #1868, 2026-07-30, CCF-011)

The original evidence above is retained unchanged.

`addProgressChangedHandler` now returns without storing anything when the
supplied `std::function` is empty, and `OnReport` invokes only truthy handlers.
The finding's own analysis selected this policy and it is what the reference
does: `Progress.cs` holds `ProgressChanged` as a nullable event delegate and
invokes it conditionally, and C# event addition of a null delegate is
`Delegate.Combine(d, null) == d`, so subscribing null cannot create a later
invocation failure. This surface therefore **does not throw** — unlike the
constructor, which mirrors `Progress(Action<T> handler)` and correctly keeps its
`ArgumentNullException("handler")`.

Measured before the fix (`build-probe/1866_prefix.log`): `progress.add=no-throw`,
`progress.add.report=bad_function_call`. Measured after
(`build-probe/1868_postfix_asan.log`): both `no-throw`.

The `OnReport` guard is defence in depth rather than the fix — the subscription
surface is the only public route into `progressChanged_`, and it can no longer
admit an empty handler — but it also makes the invocation loop textually match
`ProgressChanged?.Invoke(...)`.

Closure evidence: 5 new permanent regressions in `ProgressTests.cpp` (the empty
add itself; a later `Report` after one; registration order preserved across an
ignored empty subscription between two real ones; the constructor handler still
invoked; 100 ignored subscriptions followed by a real one). `ProgressTest` +
`EventHandlerTests` 46/46, `SharpRuntimeTests_Core_Base` 5,280/5,280. The direct
probe `build-probe/1866_empty_callable_probe.cpp`, compiled **with**
`-fsanitize=address,undefined` so this header-only template change is itself
instrumented, exits 0 with zero AddressSanitizer, UndefinedBehaviorSanitizer and
LeakSanitizer reports — including a case whose handler subscribes again from
inside `Report`.

Source, ABI and layout consequences: none. `addProgressChangedHandler` and the
`virtual OnReport` keep their signatures and their (absent) `noexcept`
specifications; no data member, virtual function or vtable slot changed.

The plan for this family is `docs/EmptyCallableBoundaryPlan.md` (ticket #1866).
