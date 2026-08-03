# Audit: `modules/threading/include/System/Threading/Thread.hpp`

## Metadata

- AUDITED: 324-line public Thread implementation, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27; isolated native/managed empty-start and
  CurrentThread-ID probes were run.
- Related fixture sources: ThreadingTests.cpp and Batch8ThreadingTests.cpp are
  pending their own complete audits; their existing normal Thread suites do
  not cover the probe boundaries below.

## SR-AUD-192 — high — Thread starts an empty std::function on a detached-capable worker and terminates the process

The constructor accepts an empty `std::function<void()>`, and `Start()` invokes
the moved function inside the new thread without validation or exception
containment.  The isolated C++ child terminates with exit 134 after
`std::bad_function_call`; the managed probe prints
`empty_start=argument_null` for `new Thread((ThreadStart)null)`.  The C++ API
therefore turns invalid construction input into an asynchronous process crash
instead of rejecting it at the public boundary.

## SR-AUD-193 — medium — CurrentThread gives every externally created native thread the same managed ID

`CurrentThreadProxy` returns ID 1 whenever `currentThreadState_` is null.
Two ordinary C++ `std::thread`s therefore print `ids=1,1`; two managed threads
print distinct `ids=3,4`.  The documented “main/other externally-created
thread” fallback erases identity for every thread not created through this
wrapper, breaking the public ManagedThreadId uniqueness contract and making
per-thread diagnostics ambiguous.

## SR-AUD-194 — medium — Start(void*) silently discards its advertised parameter

The `Start(void*)` overload captures `parameter` and immediately evaluates
`(void)parameter`; the only accepted constructor callback has no parameter
slot.  The managed `Thread(ParameterizedThreadStart)` probe prints
`parameter_preserved=True` after passing the supplied object to its delegate.
This native overload returns normally while making the caller's parameter
unobservable, rather than omitting the incompatible API or supplying a
typed/void-pointer callback form.

## Assessment

The `RunState` shared ownership correctly avoids the former detached
raw-`this` use-after-free: the launched lambda captures state, not the owning
Thread.  Ordinary start/join, static Sleep validation, state tracking, and
known COM stubs are coherent native adaptations.  That does not mitigate the
invalid callback, external identity, or ignored parameter contracts.

## Other missing assertions and diagnostics

- Existing normal Thread tests omit empty start callbacks (SR-AUD-192),
  callback throw/termination policy, repeated start/join/destruction stress,
  self-join, startup failure, and object destruction before/during callback.
- They omit ordinary external-thread CurrentThread identity/background state,
  concurrent ID uniqueness/wraparound, and transitions observed concurrently
  (SR-AUD-193).
- No fixture constructs a parameterized start consumer or verifies parameter
  identity/null/lifetime (SR-AUD-194).
- Priority is stored but intentionally not applied to the OS; apartment and
  interruption are documented stubs.  Tests do not establish platform
  diagnostics, invalid enum handling, native scheduling, background process
  lifetime, or the public semantic consequences of those adaptations.

## Final assessment

SR-AUD-192 through SR-AUD-194 are confirmed by direct source and native/
managed boundary evidence.  Shared RunState fixes the previously documented
raw-owner lifetime hazard.  No source or test was changed.


---

## Remediation record — ticket #1951 (2026-08-03), SR-AUD-192 → `remediated`

Cause **T-B** of `docs/ThreadingNamespaceReviewPlan.md` (CCF-011 in `modules/threading`; the
policy is `docs/EmptyCallableBoundaryPlan.md`).

`Thread`'s constructor now throws `System::ArgumentNullException("start")` for an empty start
function, matching `Thread(ThreadStart start)`'s `ArgumentNullException.ThrowIfNull(start)`.
This was the highest-severity member of the cause because deferring the failure did not
produce a catchable exception: `Start()` handed the empty function to a new OS thread, whose
call to it raised `std::bad_function_call` with no handler on that thread, so `std::terminate`
killed the entire process at a point no caller could observe.

The check precedes `nextManagedId_.fetch_add(1)`, so a rejected construction consumes no
managed thread id — pinned by
`ThreadingEmptyCallableTests.Thread_EmptyStart_ConsumesNoManagedThreadId`, which asserts that
two successful constructions separated by three rejected ones stay consecutive.

Evidence: `build-probe/1951_probe1_threading_empty_callables.cpp` runs this case in a forked
child precisely because its pre-fix outcome kills the process. Before:
`thread.empty_start=child-signal:6` (SIGABRT). After:
`thread.empty_start=ArgumentNullException|Value cannot be null. (Parameter 'start')` and child
exit 0. ASan/UBSan/LSan clean.

**SR-AUD-193 and SR-AUD-194 are untouched and remain `confirmed`** — both are cause T-H
(public shape) and belong to design ticket #1958.
