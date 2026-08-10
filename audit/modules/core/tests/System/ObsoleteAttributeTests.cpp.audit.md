# Audit: `modules/core/tests/System/ObsoleteAttributeTests.cpp`

## Metadata

- Audit status: AUDITED (44-line dedicated fixture, fully read).
- Validation: `ObsoleteAttributeTest.*` passed 6/6 in the 77-test focused
  Core.Base attribute filter on 2026-07-26.
- Reference basis: `ObsoleteAttribute.hpp` and local .NET
  `ObsoleteAttribute.cs`.

## Findings

The tests cover mutable string storage and `IsError`, but never connect the
object to a declaration or compiler behavior; SR-AUD-115 is therefore not
observed.  They also expect empty default `DiagnosticId`/`UrlFormat`, retaining
the null/empty collapse in SR-AUD-116 rather than distinguishing default from
explicit empty input.

## Other missing assertions and diagnostics

- Missing compilation-warning/error tests, declaration target restrictions,
  non-inheritance, copy/move, UTF-8 text, and real C++ deprecation mapping.
- Equal values are not compared through `Attribute::Equals`, so the broader
  SR-AUD-114 contract is not revealed here.

## Final assessment

The fixture validates local storage only; it is not a diagnostic-behavior
oracle.  No source or test was modified during this audit.
