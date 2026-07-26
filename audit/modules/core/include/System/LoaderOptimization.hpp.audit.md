# Audit: `modules/core/include/System/LoaderOptimization.hpp`

## Metadata

- Audit status: AUDITED (44-line enum declaration, fully read).
- Validation: `LoaderOptimizationTests.*` passed 11/11 in the 77-test focused
  Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/LoaderOptimization.cs:6-16`.

## SR-AUD-117 — low — Deprecated LoaderOptimization values have documentation-only deprecation and emit no C++ compiler diagnostic

Current .NET marks `DomainMask` and `DisallowBindings` with `Obsolete`, so
source use produces a compiler diagnostic.  The C++ declarations use Doxygen
`@deprecated` prose only (`LoaderOptimization.hpp:29-41`), not C++
`[[deprecated]]`; both values compile silently.  The focused tests exercise
both deprecated values without any diagnostic expectation.

## Other missing assertions and diagnostics

- Numeric values, including the `DomainMask == MultiDomainHost` alias, are
  correctly covered, but the fixture does not use a compiler warning-as-error
  consumer to distinguish documentation from a language diagnostic.
- The enum cannot influence loader domain behavior.  Modern .NET itself has a
  single AppDomain, and this port documents the corresponding legacy status.

## Final assessment

Numeric compatibility is correct, but callers receive no compile-time warning
for the two publicly deprecated choices.  No source or test was modified during
this audit.
