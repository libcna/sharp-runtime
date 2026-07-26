# Audit: `modules/core/include/System/ApplicationIdentity.hpp`

## Metadata

- Audit status: AUDITED (57-line sealed value type, fully read).
- Validation: `ApplicationIdentityTests.*` passed 6/6 in the 22-test identity
  filter on 2026-07-26.
- Reference basis: local `System.Security.Permissions` `ApplicationIdentity.cs`.

## Findings

The C++ type stores and returns useful full-name/code-base strings, while the
local legacy .NET source is a compatibility stub and its serialization API is
outside project scope.  Splitting the string at `#` is a port-specific parsing
rule; no local upstream implementation supplies a stronger behavior oracle.

## Other missing assertions and diagnostics

- Tests do not cover multiple separators, empty code base after `#`, malformed
  identity grammar, UTF-8/NUL content, copy/move, or serialization boundary.

## Final assessment

No evidence-backed defect was classified beyond the explicit legacy/reflection
adaptation.  No source or test was modified during this audit.
