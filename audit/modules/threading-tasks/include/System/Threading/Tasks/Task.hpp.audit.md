# Audit: `modules/threading-tasks/include/System/Threading/Tasks/Task.hpp`

## Metadata

- AUDITED: non-generic and generic Task state, waits, continuations,
  cancellation, `WhenAll`/`WhenAny`, delay, and external-future bridge.
- Validation: the complete `SharpRuntimeTests_Threading_Tasks` executable
  passed 171/171 on 2026-07-27.  Direct C++20/current-.NET 10 probes covered
  empty delegates, cancellation status, scheduler diagnostics, and task
  lifetime; ASan confirmed SR-AUD-230 in the associated exception surface.
- Reference basis: current .NET 10 `Task`, `Task<TResult>`, `Task.Run`, and
  `ContinueWith` argument-validation contracts.  Explicitly documented eager
  execution, direct-exception, no-scheduler, and `Delay(-1)` adaptations are
  not duplicated as findings.

## SR-AUD-231 — medium — task entry points accept empty delegates and defer a managed argument error into an asynchronous bad_function_call

`Run` stores an empty `std::function` and starts it, while `ContinueWith`
stores an empty continuation and faults only when it is invoked.  The native
probe prints `task_run_empty=returned` and
`continue_with_empty=returned`, then each wait exposes `bad_function_call`.
The equivalent current-.NET calls `Task.Run((Action)null)` and
`Task.CompletedTask.ContinueWith((Action<Task>)null)` both synchronously throw
`ArgumentNullException`.  The same unchecked delegate route is inherited by
generic `TaskT` and `TaskFactory::StartNew` overloads.

This turns an immediate caller-input diagnostic into a delayed, often
background-thread failure and can make a null-equivalent callback appear as an
ordinary task fault.

## Assessment

The shared state, `shared_future` waits, terminal filtering, moved-from input
checks in multi-task helpers, and out-of-band TaskCompletionSource cancellation
marker are carefully documented.  The public callable boundary lacks the
corresponding validation consistently.

## Other missing assertions and diagnostics

- Add exact immediate-error tests for empty action/function/continuation
  `std::function` values across `Task`, `TaskT`, and `TaskFactory`; assert no
  task or worker is created before the error.
- Test `WhenAll` cancellation state separately from its documented direct
  exception simplification, `WhenAny` races with all inputs already terminal,
  continuation registration/completion races, and cancellation-token identity
  through reconstructed antecedents.
- Exercise `Delay(-1)` and the no-pthread route as explicitly adapted behavior
  rather than allowing them to be covered only by a permissive no-throw test.

## Final assessment

SR-AUD-231 is confirmed by direct C++/current-.NET comparison.  No production
or test source was changed during this audit.
