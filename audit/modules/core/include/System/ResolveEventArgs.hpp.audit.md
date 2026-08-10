# Audit: `modules/core/include/System/ResolveEventArgs.hpp`

## Metadata

- Audit status: AUDITED (58-line resolution event-data declaration, fully read
  with its mixed fixture).
- Validation: `ResolveEventArgsTests.*` passed 4/4 in the 33-test related
  event filter on 2026-07-26.
- Reference basis: local .NET `System/ResolveEventArgs.cs:8-23` and the
  documented reflection exclusion.

## Findings

The port explicitly maps reflection `Assembly? RequestingAssembly` to a name
string.  The default empty-string sentinel is documented, so loss of the
Assembly object and null/empty distinction is an explicit reflection
adaptation, not a newly undisclosed fault.  AppDomain has no working resolve
event dispatch (SR-AUD-103), so this data object currently has no runtime
loader consumer.

## Other missing assertions and diagnostics

- Tests cover normal strings only; missing empty name, embedded NUL/UTF-8,
  copy/move/reference lifetime, and independent null-versus-empty adaptation
  diagnostics.
- No integration test raises TypeResolve/ResourceResolve/AssemblyResolve or
  verifies that the requesting-name representation identifies a real source.

## Final assessment

The stored C++ adaptation is consistent with the declared reflection boundary.
No source or test was modified during this audit.
