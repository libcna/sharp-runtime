# Audit: `modules/core/include/SharpRuntime/SharpRuntimeHelper.hpp`

## Metadata

- AUDITED: 192-line primitive-alias/constants helper, fully read.
- Validation: it is transitively compiled by the focused Core.Base and
  integration filters; a direct source search confirms broad first-party use.
- Reference basis: C++ fixed-width integer guarantees and audited primitive
  wrapper surfaces.

## Assessment

The aliases map the named C#-sized integer categories to fixed-width C++
types, `charcs` to UTF-16 `char16_t`, and all public extrema to
`std::numeric_limits` values.  The historical `IntPtr` alias deliberately
means unsigned native pointer storage here; the actual `System::IntPtr` value
type is separately audited and should be used for managed contract behavior.

The `CONTAINS` and `INTERNAL` preprocessor helpers are terse legacy utilities,
not .NET API promises.  They require expressions safe for macro substitution;
no first-party source relies on a side-effecting argument.

## Other missing assertions and diagnostics

- No static assertion independently locks every alias width, signedness, and
  extrema against the C#-name documentation.
- Tests do not compile the aliases under all supported ABI/endianness targets;
  the public `charcs` UTF-16 unit versus project `std::string` UTF-8 boundary
  needs continued caller-level testing.
- Macro helpers have no direct expansion test, especially side-effecting or
  non-string-like `CONTAINS` operands.

## Final assessment

The alias and constant definitions are coherent within their explicitly native
helper role.  No new finding and no source or test change.
