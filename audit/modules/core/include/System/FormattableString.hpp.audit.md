# Audit: `modules/core/include/System/FormattableString.hpp`

## Metadata

- Audit status: AUDITED (155-line public header, fully read).
- Validation: dedicated `FormattableStringTests2.*` passed 11/11 in
  `SharpRuntimeTests_Core_Base`; complementary integration
  `FormattableStringTests.*:FormattableStringFactoryTests.*` passed 13/13 on
  2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-formattable-string-audit-probe.cpp` prints
  `second`, `{value}`, and `{1}` for three cases that require literal
  preservation, escaped-brace output, and a format error, respectively.

## Assessment

The class clearly documents its string-only, culture-agnostic adaptation and
implements safe argument-index bounds checks.  Its output method, however, is
not a composite-format parser: it repeatedly searches/replaces `{0}`, `{1}`
in the mutable result.  That reprocesses inserted text and cannot enforce the
grammar promised by a FormattableString composite format.

## Finding references

- **SR-AUD-015 (extended):** `ToString()` replaces each index in successive
  passes.  `FormattableString("{0}", {"{1}", "second"})` becomes `second`,
  so an argument's literal text is reinterpreted as a format item.  Escaped
  `"{{0}}"` becomes `"{value}"` instead of literal `"{0}"`, and an unresolved
  `"{1}"` with one argument remains literal instead of producing the .NET
  composite-format `FormatException`.  The independent probe confirms all
  three results.  This is the same bespoke-parser family as the confirmed
  `String::Format` brace defect, not merely a culture adaptation.

## Other missing assertions and diagnostics

- Direct and integration tests cover only simple sequential `{n}` items; they
  omit escaped braces, repeated/nonsequential items, missing indices,
  alignment/format components, malformed braces, and an argument whose text
  contains brace syntax.
- `getArgumentCountProperty()` narrows vector size to signed `intcs` with no
  representable-size diagnostic.
- The provider methods intentionally ignore providers, but no non-null provider
  test documents this boundary.

## Final assessment

Storage and normal substitution work, but composite-format grammar and
argument isolation are confirmed incorrect.  No source or test was modified
during this audit.
