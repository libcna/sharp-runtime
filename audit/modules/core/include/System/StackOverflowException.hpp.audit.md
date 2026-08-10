# Audit: `modules/core/include/System/StackOverflowException.hpp`

## Metadata

- Audit status: AUDITED (38-line declaration, fully read with implementation).
- Validation: the direct four-fixture exception filter passed 41/41 on
  2026-07-27; its ten direct StackOverflowException cases and four companion
  cases are now fully audited across two fixture reports.
- Reference basis: local .NET `StackOverflowException.cs` and `COR_E_STACKOVERFLOW` (`0x800703E9`).

## Assessment

The sealed public declaration offers default, C-string, string, and inner
overloads matching its C++ implementation. The direct suite checks ordinary
messages, throw/catch hierarchy, and default HResult. No standalone
implementation defect was confirmed.

## Other missing assertions and diagnostics

- The default HResult is the only one checked; C-string, string, and inner overloads lack code assertions.
- The C-string null boundary, stored-inner identity/rethrow, and empty/UTF-8 message cases are absent.
- Native stack overflow cannot safely be induced and recovered in-process like a normal C++ exception, so construction tests must not be mistaken for real overflow translation coverage.

## Final assessment

The declaration is consistent with the reviewed implementation and ordinary tests. No source or test was modified.
