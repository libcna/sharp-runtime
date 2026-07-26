# Audit: `modules/core/tests/System/Batch12ArgHandleTests.cpp`

## Metadata

- Audit status: AUDITED (82 lines, 11 tests, fully read).
- Validation: `RuntimeArgumentHandleTests.*:ArgIteratorTests.*` passed 11/11
  in `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference basis: C++ object lifetime rules and the explicit unsupported
  ArgIterator/CLR-varargs adapter.

## Assessment

The first three checks are valid smoke coverage for the empty
RuntimeArgumentHandle token, and both constructor checks correctly require the
documented NotSupportedException. The remaining five ArgIterator calls use
aligned character storage cast to a pointer without constructing an object.
They make the test pass only while the stub's member bodies remain accidentally
state-free.

This is the test-side evidence for SR-AUD-112. It cannot establish behavior
of a publicly constructible iterator, because no such iterator exists in the
port.

## Other missing assertions and diagnostics

- Replace the fabricated-object calls only after the API design defines a safe
  way to expose unsupported End/equality/hash/next methods (for example static
  helpers or a constructible sentinel); do not retain raw-storage invocation.
- Check exact exception message/category and include isolation. No test can
  cover CLR `__arglist` iteration in this runtime.
- The copying test does not discuss the .NET ref-struct/stack-only distinction.

## Final assessment

All 11 tests are green, but five rely on undefined object lifetime and are not
reliable regression evidence. No test was modified during this audit.
