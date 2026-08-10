# Audit: `modules/text-regular-expressions/CMakeLists.txt`

## Metadata

- AUDITED: INTERFACE component registration and Core.Base dependency.
- Validation: the current Makefile exposes no standalone build target for this
  unconsumed interface library; public headers were compiled directly in the
  ASan lifetime probe.

## Assessment

The component is correctly described as header-only with the required base
exception/value dependencies.  Its lack of a direct fixture means ordinary
configuration does not compile this public API independently.

## Other missing assertions and diagnostics

- Add a dedicated test target that includes every public header and runs
  parser/match/lifetime/sanitizer regressions, including SR-AUD-245.

## Final assessment

Registration is coherent but lacks direct validation coverage. No source or
test was changed during this audit.
