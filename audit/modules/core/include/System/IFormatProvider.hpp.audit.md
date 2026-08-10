# Audit: `modules/core/include/System/IFormatProvider.hpp`

## Metadata

- Audit status: AUDITED (27-line public interface, fully read).
- Supporting validation: `IFormatProviderTests2.*` passed 1/1 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The interface is a minimal, stateless adaptation of .NET's format-provider
lookup.  Its const virtual dispatch and null-for-unsupported convention are
clear.  `void*` is necessarily an untyped borrowed result in this C++ mapping;
the provider and caller must agree on type and lifetime, which cannot be
expressed by the .NET object-returning signature without a local object model.

## Other missing assertions and diagnostics

- The direct test verifies only the null branch.  It does not return a typed
  formatting object, assert pointer identity, or document ownership/lifetime.
- The API has no typed retrieval helper or mismatch diagnostic; a caller that
  casts an unrelated returned pointer has ordinary C++ undefined behavior.

## Final assessment

This declaration has no state or independently unsafe operation.  Its type and
lifetime boundary is an explicit C++ adaptation concern, not a separately
confirmed defect.  No source or test was modified during this audit.
