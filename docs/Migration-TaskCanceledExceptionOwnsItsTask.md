<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `TaskCanceledException` owns the task it names (ticket #1970)

*2026-08-17.* `System::Threading::Tasks::TaskCanceledException` no longer stores a borrowed raw
pointer to the cancelled `Task`. It stores a copy of the task's **handle**, so
`getTaskProperty()` is safe to call for as long as the exception is reachable.

**No public signature changed.** The constructor still takes `const Task*`, the property still
returns `const Task*`, and no member became virtual or changed its exception specification. One
thing did change, and it is the reason this note exists.

---

## 1. The layout change, and who must rebuild

| Type | `sizeof` before | `sizeof` after | `alignof` |
|---|---:|---:|---|
| `TaskCanceledException` | 192 | **200** | 8, unchanged |

The `const Task*` member (8 bytes) became a `std::shared_ptr<const Task>` (16 bytes). **Every
consumer must be fully recompiled** — a `sizeof` change across a stale-header boundary is an ODR
violation with no diagnostic. sharp-runtime ships as a static library built from source, so this
is a rebuild requirement rather than a broken distributed binary.

Landed under `docs/StandingApprovals.md` SA-3; the figures are pinned by
`TaskCanceledExceptionLifetimeTests.LayoutPin_TheCostOfOwningTheHandle`, expressed as a
relationship — the exception is its base plus exactly one `shared_ptr` — so a standard-library
change to either component does not turn the pin into a false failure.

## 2. What was wrong

The `Task`-taking constructor stored the caller's pointer and `getTaskProperty()` handed it back
with no ownership and no validity check. ASan reported **stack-use-after-scope** when the
referenced `Task` was local to the construction scope — which is the ordinary case, because a
task that has just been cancelled and thrown out of is usually a local. The header documented the
hazard; the audit's position, which this ticket accepts, is that documenting a hazard does not
make an ordinary public property safe to call.

## 3. What you get instead

A `Task` in this port is a **handle** over a `std::shared_ptr<State>`, so copying one does not
copy the task — both handles observe the same state. The exception therefore:

- keeps that state alive for as long as it is reachable, so the pointer never dangles; **and**
- keeps *observing* it, so a status change after the exception was constructed is visible through
  `getTaskProperty()`.

The second half is what makes this .NET's contract rather than merely a safe approximation:
`TaskCanceledException.Task` there is a GC-tracked reference to the same task object, not a
snapshot of it.

`std::shared_ptr<const Task>` rather than `std::optional<Task>` because `Task.hpp` includes
`TaskCanceledException.hpp` (`Task.hpp:25`), so the complete type is not available in that header
and a by-value member would be a circular include.

## 4. The one behaviour that changed, and why it was never a contract

```cpp
Task t = Task::CompletedTask();
TaskCanceledException ex(&t);
ex.getTaskProperty() == &t;   // was: true.  now: FALSE
```

The returned pointer now addresses the exception's own handle, not the caller's object.

**Address identity was never the right analogue of .NET's contract.** There, `Task` is a
reference type, so `ReferenceEquals(ex.Task, t)` holds. Here `Task` is a value-semantics handle;
the counterpart of "the same object" is "the same **state**", which is preserved and asserted by
`TaskCanceledExceptionTests.TaskCtor_RetainsTheSameTaskState`. Code that compared the returned
pointer against its own `Task`'s address was relying on the borrowing that caused the defect.

**To migrate:** compare observable state (`getIsCompletedProperty()`, `getIsCanceledProperty()`,
`getStatusProperty()`) instead of addresses. A null task is still reported as `nullptr`.

## 5. Downstream, measured

Per `docs/StandingApprovals.md` SA-2 condition 5, both consumer checkouts were searched: neither
`cna` nor `mobile-eggbert` names `TaskCanceledException` — **zero sites in both**. Neither
repository was modified. Both must still be rebuilt, as every consumer must.
