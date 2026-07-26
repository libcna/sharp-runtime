# Audit: `modules/core/include/System/IDisposable.hpp`

## Metadata

- Audit status: AUDITED (36-line public interface, fully read).
- Supporting validation: `IDisposableTests.*` passed 6/6 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The header gives clear local guidance: `Dispose()` should release resources,
avoid ordinary exceptions, and be idempotent/safe on repeated calls; C++ users
must employ RAII or call it explicitly.  The pure virtual interface has no
resource ownership or automatic destructor behavior of its own.  Multiple
first-party resource-bearing classes derive from it, whose individual disposal
contracts require their own audits.

## Other missing assertions and diagnostics

- The multi-call test uses a counter that deliberately reaches three, so it
  proves callability but not idempotent release of an owned resource.
- `SharedPtr_DisposeOnReset` calls `Dispose()` before leaving scope; a plain
  `shared_ptr` reset does not invoke `IDisposable::Dispose()` automatically.
  The name is therefore misleading and the test does not demonstrate RAII
  disposal behavior.
- No fixture covers a throwing destructor/dispose implementation, concurrent
  calls, resource release exactly once, or real first-party `IDisposable`
  implementers.

## Final assessment

The interface's C++ ownership guidance is correct.  The direct tests need
stronger resource/idempotence assertions but no declaration defect is
confirmed; no source or test was modified during this audit.
