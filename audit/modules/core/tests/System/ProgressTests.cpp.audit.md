# Audit: `modules/core/tests/System/ProgressTests.cpp`

## Metadata

- Audit status: AUDITED (78 lines, 9 tests, fully read).
- Validation: `ProgressTest.*` passed 9/9 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The suite covers default and constructor callbacks, multiple subscription
order/effect, repeated reports, IProgress polymorphism, string payloads, and
the constructor's empty-handler exception.  It accurately validates normal
synchronous execution but treats the event-like added-handler path only with
nonempty functions.

## Finding references

- **SR-AUD-058:** no test adds an empty `std::function`.  Such a registration
  succeeds and the next `Report` throws `std::bad_function_call`, while .NET
  nullable event subscription has no such delayed failure.

## Other missing assertions and diagnostics

- No callback order assertion distinguishes the constructor handler from
  subsequently registered handlers.
- Handler exception, self-registration/removal, move-only captures, and
  concurrent/reentrant reports are untested.
- The tests do not make the intentional synchronous-versus-captured-context
  adaptation observable.

## Final assessment

Normal callback delivery is covered well for this small adapter, but the empty
event-subscription boundary is missing.  No test was modified during this
audit.
