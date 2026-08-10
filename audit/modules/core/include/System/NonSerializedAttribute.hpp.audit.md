# Audit: `modules/core/include/System/NonSerializedAttribute.hpp`

## Metadata

- Audit status: AUDITED (18-line marker declaration, fully read).
- Validation: `MarkerAttributeTests.NonSerializedAttribute_DefaultCtor` passed
  in the 77-test focused Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/NonSerializedAttribute.cs:8-15` and
  `CLAUDE.md` serialization exclusion.

## Findings

The header explicitly records that this marker has no observable effect because
the project excludes reflection and BinaryFormatter-style serialization.  That
is consistent with the repository-wide permanent-deviation policy.

## Other missing assertions and diagnostics

- There is no dedicated fixture; the shared test only checks construction.
- Target restriction to non-inherited fields, sealing, metadata visibility,
  and serializer omission behavior have no C++ equivalent in this scope.

## Final assessment

This is a clearly documented compatibility marker, not an undisclosed partial
serializer.  No source or test was modified during this audit.
