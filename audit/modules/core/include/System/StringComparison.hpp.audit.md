# Audit: `modules/core/include/System/StringComparison.hpp`

## Metadata

- AUDITED: 27-line enum; direct fixture passed 8/8 on 2026-07-26.
- Reference basis: local .NET `StringComparison.cs:6-14`.

## Findings

All six values match .NET. Actual culture/ordinal comparison divergences belong
to String/globalization consumers rather than this declaration.

## Final assessment

Correct constants; no source or test was modified.
