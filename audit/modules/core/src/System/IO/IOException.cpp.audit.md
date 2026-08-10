# Audit: `modules/core/src/System/IO/IOException.cpp`

## Metadata

- Audit status: AUDITED (31-line implementation, fully read with declaration).
- Validation: the focused I/O/crypto filter selected 0 tests; the shared probe prints `IOException=80131620`.
- Reference basis: local .NET `System/IO/IOException.cs` and `COR_E_IO` (`0x80131620`).

## Assessment

The implemented default, C-string, string, and inner constructors all set
`COR_E_IO`, matching current .NET for those overloads. The implementation lacks
the declaration and body for .NET's public custom-HResult constructor; see
SR-AUD-101. No separate runtime defect was confirmed in the available routes.

## Other missing assertions and diagnostics

- There are no direct tests for any implementation route, custom native HResult, null C-string, exact message, or stored inner identity.
- Native I/O failures are not mapped to this class in reviewed core code, so current validation cannot establish OS-error behavior.

## Final assessment

The existing constructor code is correct but incompletely exposed and entirely untested. No source or test was modified.
