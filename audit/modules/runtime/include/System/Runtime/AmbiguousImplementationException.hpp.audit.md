# Audit: `modules/runtime/include/System/Runtime/AmbiguousImplementationException.hpp`

## Metadata

- AUDITED: 18-line inline exception declaration, fully read.
- Validation: `AmbiguousImplementationExceptionTests.*` passed 3/3 on
  2026-07-27; the complete shared fixture passed 82/82.
- Reference/probe: local current-.NET
  `Runtime/AmbiguousImplementationException.cs` and `HResults.cs`; linked C++
  probe prints `80131501`, while the matching managed probe prints `8013106A`
  and reports that `SystemException` is not assignable from the managed type.

## SR-AUD-157 — medium — AmbiguousImplementationException and ExternalException retain SystemException HResult

Neither inline constructor assigns a derived code.  Consequently the C++ probe
prints `0x80131501` (`COR_E_SYSTEM`) for this type and for
`InteropServices::ExternalException`.  Current .NET assigns
`COR_E_AMBIGUOUSIMPLEMENTATION` (`0x8013106A`) in every Ambiguous overload and
`E_FAIL` (`0x80004005`) in every ordinary ExternalException overload.

The incorrect code is observable through the inherited `getHResultProperty()`
API and loses the diagnostic identity that consumers use to distinguish these
failure kinds.  The shared tests assert only text/throwability and do not
cover either code.

## SR-AUD-158 — medium — AmbiguousImplementationException has a different catch hierarchy and no causal constructor

Current .NET declares a sealed type directly derived from `Exception`, with a
public `string?` plus `Exception?` constructor.  The C++ type is non-final and
derives from `SystemException`; it exposes only default and non-nullable
`std::string` constructors.  This makes it catchable as `SystemException`
where its managed counterpart is not, permits unsupported derivation, and
prevents callers from preserving an inner cause.  The C++ API probe attempting
`AmbiguousImplementationException("message", exception_ptr)` fails with no
matching constructor; the matching managed hierarchy check prints `False` for
`SystemException.IsAssignableFrom(AmbiguousImplementationException)`.

## Other missing assertions and diagnostics

- The shared fixture omits all HResult checks, a `SystemException` catch
  distinction, final/sealing compile-time intent, and inner-cause retention or
  rethrow.
- It asserts exact English default text but has no null/empty/UTF-8 message
  boundary or source/stack diagnostic coverage.

## Final assessment

The ordinary messages and throwable shape are functional, but the type's
public inheritance, constructor, and diagnostic-code contracts diverge from
current .NET.  No source or test was modified.
