# Audit: `modules/core/include/System/Type.hpp`

## Metadata

- Audit status: AUDITED (238 lines, full read; header-only implementation).
- Public API: a lightweight `std::type_info` handle used in place of .NET
  reflection.
- Validation: focused `ObjectTests.*:TypeTest.*` run passed 47/47 tests on
  2026-07-25.

## Assessment

The header explicitly labels its reflection predicates as a permanent stub and
documents their deliberately inconsistent fixed values.  This is a known,
intentional project deviation rather than an unreported implementation gap:
callers must not use `IsClass`, `IsValueType`, or the related predicates for
runtime classification.  Equality correctly handles two null handles and
delegates non-null comparison to `std::type_info`; the `std::hash<Type>`
specialization agrees with that wrapper's hash function.

`getName`, `getFullNameProperty`, and `ToString` expose compiler/ABI RTTI names
instead of stable .NET metadata names.  The header states this limitation; no
new defect is indexed.

## Other missing assertions and diagnostics

- Tests check the fixed predicate results for `int`, but no consumer-level
  negative test prevents production code from branching on these permanent
  stubs.  A future static review should flag such branching where it appears.
- `type_info::name()` portability is assessed only as non-empty output.  Any
  public persistence, wire format, or user-facing display based on that name
  requires a separately stable naming scheme.
- Cross-shared-library RTTI identity is not exercised.  The current core test
  executable has one linkage domain, so it is insufficient evidence for a
  plugin ABI guarantee.

## Final assessment

The class is a consciously constrained service-key adapter, not reflection.
Its permanent deviations are unusually clear and are correctly locked by the
current tests; no repair is proposed in this audit phase.
