# Audit: `modules/core/include/System/Globalization/NumberStyles.hpp`

## Metadata

- AUDITED: 57-line public flags-enum declaration, fully read.
- Validation: `NumberStylesExtendedTests.*` passed 43/43 on 2026-07-27;
  C++/managed parser probes exercised exponent and style validation paths.
- Reference basis: local current-.NET `NumberStyles.cs` and
  `NumberFormatInfo.ValidateParseStyleInteger`.

## Assessment

The thirteen primitive flags and composite values, including current
`AllowBinarySpecifier`, `BinaryNumber`, and `HexFloat`, retain the managed bit
assignments.  The C++ `|` and `&` helpers make normal flag construction
possible.  Whether invalid/combined styles are accepted and which grammars
consume valid flags is implementation behavior owned by the shared parser
(SR-AUD-177 and SR-AUD-178), not an ordinal declaration defect.

## Other missing assertions and diagnostics

- The fixture does not assert every primitive/composite numeric value or
  bitwise composition, especially `None`, `Any`, `Float`, and `HexFloat`.
- No public conversion/type-safe validation prevents callers from casting an
  arbitrary integer into this enum; parser entry points must validate that
  input, as current .NET does.

## Final assessment

The public flag values match current .NET.  No new finding and no source or
test change.
