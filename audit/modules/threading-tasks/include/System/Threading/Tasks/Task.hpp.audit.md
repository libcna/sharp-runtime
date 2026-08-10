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

---

## Correction and remediation — ticket #1965, 2026-08-03 (cause TC-A)

*The audit text above is preserved verbatim. This section is appended, per the
SR-AUD-081 / SR-AUD-362 convention, because measurement disagreed with it in
three ways.*

**Evidence:** `build-probe/1965_probe1_tasks_empty_callables.cpp`, 37 cases,
logs `1965_probe1_before.log` / `1965_probe1_after.log` /
`1965_probe1_asan.log`.

### C1 — the site count is 22 public entries, not the two named

The finding names `Run` and `ContinueWith` and says the route is "inherited by
generic `TaskT` and `TaskFactory::StartNew` overloads". Enumerated by
measurement, the entries that accept an empty callable are:

| Type | Entries | .NET parameter name |
|---|---|---|
| `Task` | `Task(action)`, `Task(action, token)`, `Run(action)`, `Run(action, token)` | `action` |
| `Task` | `ContinueWith(continuationAction, options)` | `continuationAction` |
| `TaskT<T>` | `TaskT(func)`, `TaskT(func, token)`, `Run(func)`, `Run(func, token)` | **`function`**, not `action` |
| `TaskT<T>` | `ContinueWith(Action-shaped)` | `continuationAction` |
| `TaskT<T>` | `ContinueWith(result-producing)` | `continuationFunction` |
| `TaskFactory` | four `StartNew` overloads (+ `Task::Factory()`) | `action` / `function` |
| `Parallel` | `For` ×3, `ForEach` ×2 | `body` |
| `Parallel` | `Invoke` | *(see C3)* |

Only **eleven** distinct bodies needed an edit; the rest inherit the check by
forwarding. All 22 were measured before and after.

### C2 — `Parallel`'s failure was **not** outside the `System::Exception` hierarchy

CCF-011's third consequence — *"the failure is the wrong type … a consumer's
`catch (const System::Exception&)` does not catch it"* — is the strongest part
of the family's rationale, and it does **not** hold for the six `Parallel`
entries. Measured: `parallel.for.body=aggregate:bad_function_call`. `Parallel`
already collected every worker's exception into `System::AggregateException`, so
ported code catching `System::Exception` *did* see it. What was wrong there was
the diagnostic and the timing, not the catchability. The claim holds exactly for
the sixteen `Task`/`TaskT`/`TaskFactory` entries, where a bare
`std::bad_function_call` escaped.

### C3 — `Parallel::Invoke` is not an `ArgumentNullException` site

This is the same shape as `docs/ThreadingNamespaceReviewPlan.md` §17.1's two
`System::Threading` sites. .NET's `Parallel.Invoke(params Action[] actions)`
copies the array and rejects a null **element** with
`new ArgumentException(SR.Parallel_Invoke_ActionNull)` — an `ArgumentException`
with **no parameter name** — because the null is an element of the argument
rather than the argument itself. Applying the family's usual
`ArgumentNullException("actions")` spelling would have swapped one non-matching
result for another. Implemented as `ArgumentException("One of the actions was
null.")` and pinned by a test that fails if `ArgumentNullException` is thrown.

*Reference-evidence limitation, recorded rather than hidden:*
`/rv/tmp/runtime/src/libraries/` is **not present in this environment**, so the
per-entry .NET parameter names and the `Invoke` exception type could not be
re-read from local source; they come from the .NET API contract for the exact
overloads listed above, and the audit's own managed probe supplies the
`ArgumentNullException` category for `Task.Run` / `ContinueWith`. Where the
answer was uncertain the conservative choice was taken — for `Invoke`, the
**base** `ArgumentException`, following #1954's precedent for SR-AUD-184.

### C4 — the failure is data-dependent, in two shapes the finding does not name

- **Zero iterations.** `Parallel::For(0, 0, {})`, both `For` state overloads and
  both `ForEach` overloads over an empty source returned a normally completed
  `ParallelLoopResult` with an empty body. The same wrong call was silent or
  fatal depending on the iteration count.
- **An already-cancelled token.** `Task(action, cancelledToken)` short-circuits
  to the Canceled state *before* launching, so an empty action produced a
  perfectly ordinary Canceled task and never reported the bad argument at all.
  The repair puts the argument check **above** that short circuit, matching
  .NET's `InternalStartNew`, which validates the delegate before the task
  exists.

### C5 — `ContinueWith` did register the continuation before failing

`task.continuewith.registered_before_throw=no-throw|side_effects=1`: the empty
continuation was recorded on the antecedent and the returned continuation Task
was left **faulted**, so `ContinueWith` itself returned normally and the caller
received a broken task. The repair checks before the continuation Task is
constructed and before anything is registered on either task, which the
no-partial-state regressions pin.

### Result

All 22 entries reject the empty callable at the public boundary with .NET's
exception type and parameter name; the post-fix probe shows **zero**
`bad_function_call` and **zero** `aggregate:` outcomes, and every valid-callable
case is byte-identical to before. ASan + UBSan + LSan: **0 reports before, 0
after** — this was an exception-contract defect, not a memory defect, so the
sanitizer's role was CCF-011 §13's leak check on entry points that now throw
after copying a `std::function` by value. `SharpRuntimeTests_Threading_Tasks`
**171 → 208**. No public signature, object layout, vtable, `noexcept`
specification or component edge changed; the module graph stays 41 / 91.

**SR-AUD-231: `confirmed` → `remediated` (#1965, 2026-08-03).**
