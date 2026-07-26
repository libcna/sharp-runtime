# Audit: `modules/core/include/System/BadImageFormatException.hpp`

## Metadata

- Audit status: AUDITED (87-line inline implementation, fully read).
- Validation: the complete five-type exception-family filter passed 43/43 on 2026-07-26.
- Reference/probe: local .NET `BadImageFormatException.cs` assigns
  `COR_E_BADIMAGEFORMAT` (`0x8007000B`) in all five constructors; the shared
  standalone probe prints inherited C++ `80131501`.

## SR-AUD-094 — medium — five exception types retain their base HResult instead of their documented derived diagnostic code

None of the five constructors calls `setHResultProperty`. Consequently each
retains `SystemException`'s `COR_E_SYSTEM` (`0x80131501`) instead of
`COR_E_BADIMAGEFORMAT` (`0x8007000B`). The 43 passing focused tests cover
ordinary messages, file-name storage, and the documented empty FusionLog
adaptation, but never the public HResult. See the owning
`ApplicationException.hpp.audit.md` report for the shared family finding.

## Other missing assertions and diagnostics

- Tests omit HResult for every overload, exact default text, empty/UTF-8 file
  names, and stored-inner pointer identity/rethrow behavior.
- `getFusionLogProperty()` is a documented CLR-unavailable adaptation returning
  empty text; no actual loader integration identifies whether a native image
  load failure should construct this exception.
- File name storage is tested, but filename-sensitive message behavior and the
  .NET requesting-assembly-chain diagnostic are not modeled or documented.

## Final assessment

The extra filename API works in ordinary cases, but each constructor reports
the wrong HResult. No source or test was modified.
