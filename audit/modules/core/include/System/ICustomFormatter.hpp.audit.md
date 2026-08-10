# Audit: `modules/core/include/System/ICustomFormatter.hpp`

## Metadata

- Audit status: AUDITED (39-line public interface, fully read).
- Supporting validation: `ICustomFormatterTests2.*` passed 1/1 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The interface is a direct polymorphic custom-formatting hook.  It documents
the C++ `void*` adaptation for .NET's universal object parameter and accepts a
borrowed provider.  It performs no dereference or formatting itself, so
argument validation, type recognition, and formatting errors belong to each
formatter implementation.

## Other missing assertions and diagnostics

- The sole fixture ignores both `arg` and provider and only uppercases the
  format string; it does not prove typed argument handling, a null-argument
  policy, provider dispatch, or exception behavior.
- `const void*` makes wrong-type casts and borrowed-lifetime errors the
  formatter's responsibility; no typed helper or mismatch diagnostic exists.

## Final assessment

The type-erased boundary is documented and no declaration defect is confirmed.
No source or test was modified during this audit.
