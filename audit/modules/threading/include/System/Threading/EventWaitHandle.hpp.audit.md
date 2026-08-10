# Audit: `modules/threading/include/System/Threading/EventWaitHandle.hpp`

## Metadata

- AUDITED: 72-line condition-variable event implementation, fully read.
- Validation: `EventWaitHandleTests.*:WaitHandleTests.*` passed 9/9 in
  `SharpRuntimeTests_Threading` on 2026-07-27.  The EventWaitHandle cases
  account for five selected valid-mode tests; their complete fixture source is
  pending audit.
- Reference/probe: local current-.NET `EventWaitHandle.cs` and
  `EventResetMode.cs`; C++/managed probes compare construction with underlying
  enum value 42.

## SR-AUD-184 — medium — EventWaitHandle accepts invalid EventResetMode values instead of rejecting them at construction

The constructor stores `mode` without validation.  A C++ probe reports
`invalid_mode=normal` for `static_cast<EventResetMode>(42)`.  The equivalent
managed `new EventWaitHandle(false, (EventResetMode)42)` reports an argument
exception.  In C++, the invalid value also creates incoherent behavior: `Set`
uses the AutoReset `notify_one` branch because it is not ManualReset, while
WaitOne does not reset the event because it is not AutoReset.

All local tests use valid enum values, so the invalid public state is accepted
without any diagnostic.  This contradicts the managed construction boundary,
not merely the port's documented in-process condition-variable adaptation.

## Assessment

For valid modes, initial set/not-set behavior, manual repeated waits, reset,
finite timeout, and `WaitOne(-2)` diagnostics pass focused tests.  The header
is intentionally a minimal in-process event: it does not expose the current
.NET named/open-existing overloads or bool return values from Set/Reset.
Those scope limitations are visible in the declaration; the confirmed defect
is the accepted invalid mode within its exposed constructor.

## Other missing assertions and diagnostics

- The fixture has no invalid-mode case, no named-event/API-surface compilation
  coverage, and no Set/Reset return-value parity assertion.
- It omits cross-thread waiter release, exactly-one AutoReset release,
  repeated Set/reset interleavings, concurrent Set/Reset/WaitOne stress, and
  disposal/lifetime behavior.
- The condition predicate is an atomic updated outside the condition-variable
  mutex.  No stress test demonstrates that notification and AutoReset
  consumption remain free of lost wakeups under adversarial scheduling.

## Final assessment

SR-AUD-184 is confirmed by direct C++/managed construction probes.  No source
or test was modified.


---

## Remediation record — ticket #1954 (2026-08-03), SR-AUD-184 → `remediated`

Cause **T-C** of `docs/ThreadingNamespaceReviewPlan.md`.

The constructor now rejects any `EventResetMode` that is neither `AutoReset` nor
`ManualReset`:

```cpp
if (mode != EventResetMode::AutoReset && mode != EventResetMode::ManualReset)
    throw System::ArgumentException("Value of flags is invalid.", "mode");
```

**Reject, not normalise — and deliberately not unified with the other enum in the same
ticket.** `ReaderWriterLockSlim` (SR-AUD-205) *normalises* an undeclared
`LockRecursionPolicy` to `NoRecursion` instead of rejecting it. The two conventions are
.NET's own: `EventWaitHandle` validates `mode` and throws, while `ReaderWriterLockSlim`
stores `_fIsReentrant` as a bool and derives `RecursionPolicy` from it, so an undeclared
value silently becomes `NoRecursion`. This port reproduces both rather than picking one.

### The incoherence, measured

`build-probe/1954_probe1_argument_domain.cpp`, row
`eventwaithandle.mode42.incoherent`, before the change:

```
first=1 second_without_set=1
```

A handle constructed with mode 42 was signalled by `Set()` through the **AutoReset**
`notify_one` branch — taken because 42 is not `ManualReset` — and then failed to reset in
`WaitOne()` — skipped because 42 is not `AutoReset`. The result is neither an auto-reset nor
a manual-reset event: it releases one waiter per `Set()` yet stays signalled forever. The
finding described this; the probe confirms it.

### One recorded uncertainty

The exact **derived** exception type .NET throws could not be verified: `/rv/tmp/runtime/src/libraries/`
is not present in this environment, and this report's own managed evidence records only the
category (*"reports an argument exception"*). The candidates are
`ArgumentException(SR.Argument_InvalidFlag, nameof(mode))` and
`ArgumentOutOfRangeException(nameof(mode))`; the latter derives from the former in both .NET
and this port. The port therefore throws the **base** `System::ArgumentException`, so a
handler written for either candidate catches it, and
`ThreadingArgumentDomainTests.EventWaitHandle_UndeclaredMode_Rejected` asserts the category
and the `paramName` rather than a derived type. This is a knowingly incomplete parity claim,
not an unnoticed one.

Coverage: `ThreadingArgumentDomainTests.EventWaitHandle_*` — raw modes 2, 42 and -1 rejected
with `paramName == "mode"`, plus a control asserting that `ManualReset` stays signalled
across repeated waits, `AutoReset` consumes its signal, `Reset()` clears, and an
initially-set handle waits successfully. No signature, layout, vtable or
exception-specification change.
