# Audit: `modules/text-regular-expressions/include/System/Text/RegularExpressions/RegexParseException.hpp`

## Metadata

- AUDITED: parse exception inheritance and error/offset properties.
- Evidence: Regex constructor catches std::regex_error and constructs this type.

## Assessment

The type carries an explicit Error and Offset with a documented default Offset
of zero because the backing engine lacks parser-location information.  This
preserves useful public diagnostics without claiming false precision.

## Other missing assertions and diagnostics

- Add invalid-pattern construction, message, Error, Offset, inner/base
exception, and raw-flag-constructor error-boundary tests.

## Final assessment

No declaration-level defect was demonstrated. No source or test was changed.
