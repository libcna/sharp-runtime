# Audit: `modules/core/include/System/IObserver.hpp`

## Metadata

- Audit status: AUDITED (59-line public template interface, fully read).
- Supporting validation: `IObserverTests2.*` passed 1/1 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The three pure virtual callbacks map the local observer contract cleanly and
document both C++ variance limitations and required terminal behavior: after
`OnError` or `OnCompleted`, a provider must not issue another callback.  The
error argument is a borrowed `std::exception` reference, which is a reasonable
local counterpart where all runtime exceptions derive from the standard base.
The interface itself cannot enforce a provider's callback ordering.

## Finding references

- **SR-AUD-056:** the direct observable fixture allows a later `OnNext` after
  `OnCompleted`; its tests do not assert this terminal contract.

## Other missing assertions and diagnostics

- Direct tests exercise only `OnCompleted`; there is no source-level test of
  `OnError`, error identity/message propagation, terminal exclusivity, or
  reentrant observer callbacks in the reviewed fixture.
- The C++ invariant/contravariant adaptation is documented but has no
  compile-only example showing the deliberately unsupported cross-type use.

## Final assessment

This is a coherent pure interface.  The observable fixture rather than the
declaration is the confirmed coverage/contract gap; no source or test was
modified during this audit.
