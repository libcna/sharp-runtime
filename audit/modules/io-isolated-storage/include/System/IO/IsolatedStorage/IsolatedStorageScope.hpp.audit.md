# Audit: `modules/io-isolated-storage/include/System/IO/IsolatedStorage/IsolatedStorageScope.hpp`

## Metadata

- AUDITED: scope flags and OR/AND operators.
- Evidence: IsolatedStorageFile factory methods and current .NET scope values
  were compared.

## Assessment

The declared flag values and basic composition operators match the public
vocabulary used by the local store factories.  The module intentionally maps
only application and assembly user stores to its practical StoragePaths root.

## Other missing assertions and diagnostics

- No test asserts all scope values, flag composition, unknown casts, or that
  application and assembly factories report the documented distinct scope
  flags while following the selected root policy.

## Final assessment

No enum-level defect was demonstrated. No source or test was changed.
