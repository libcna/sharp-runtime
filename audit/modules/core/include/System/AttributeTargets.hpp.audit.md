# Audit: `modules/core/include/System/AttributeTargets.hpp`

## Metadata

- Audit status: AUDITED (59-line enum and flag operators, fully read).
- Validation: `AttributeTargetsTests.*` passed 6/6 in the 77-test focused
  Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/AttributeTargets.cs:12-33`.

## Findings

All fifteen public bit values and the `All` aggregate match current .NET.
The scoped C++ enum needs the supplied `|`/`&` operators and those correctly
preserve the `int`-sized flag representation.

## Other missing assertions and diagnostics

- Tests sample only three individual values; they do not lock every value,
  `All == 0x7fff`, zero intersections, or combinations including
  `GenericParameter`.
- `AttributeTargets` is only a runtime value.  The project has no custom
  attribute attachment/reflection facility, an intentional global reflection
  exclusion recorded in `CLAUDE.md`; it therefore cannot validate a declared
  attribute target.

## Final assessment

The standalone enum contract is correct; declaration-target enforcement is
outside the explicitly excluded metadata subsystem.  No source or test was
modified during this audit.
