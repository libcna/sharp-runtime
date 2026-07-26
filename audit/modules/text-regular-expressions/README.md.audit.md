# Audit: `modules/text-regular-expressions/README.md`

## Metadata

- AUDITED: header-only component scope and Core.Base dependency statement.
- Evidence: all public regex headers and module CMake registration were read.

## Assessment

The README accurately identifies a header-only component.  The detailed
public-header notes, rather than this short overview, contain the material
std::regex grammar/options/capture/timeout limitations.

## Other missing assertions and diagnostics

- Link a consolidated compatibility matrix for grammar, options, replacement
  tokens, capture semantics, timeout behavior, and lifetime expectations after
  SR-AUD-245 is corrected.

## Final assessment

The dependency summary is accurate. No source or test was changed during this audit.
