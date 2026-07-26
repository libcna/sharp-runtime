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
