# Audit: `modules/core/include/System/RuntimeType.hpp`

## Metadata

- Audit status: AUDITED (31-line public enum declaration, fully read with its
  dedicated eight-test fixture).
- Validation: `RuntimeTypeTest.*` passed 8/8 within the combined 16/16 runtime
  type/handle filter on 2026-07-26.
- Reference basis: local .NET `System/RuntimeType.cs`, where `RuntimeType` is
  an internal sealed `TypeInfo`/reflection class rather than this enum.

## SR-AUD-110 — medium — public RuntimeType enum occupies the name of an unrelated internal .NET reflection class

The port publishes `System::RuntimeType` as a six-value public enum and calls
it the C++ counterpart of the internal .NET `System.RuntimeType`
(`RuntimeType.hpp:10-13`).  Local .NET instead defines that name as an internal
sealed class deriving from `TypeInfo`, with type handles, metadata, assembly,
and reflection behavior.  The values `Primitive`, `ValueType`,
`ReferenceType`, `Array`, and `GenericParameter` are an invented category API,
not CoreCLR public constants.

This name collision prevents a future reflection-compatible runtime-type port
from using its source name and lets C++ callers depend on a semantic surface
that has no .NET counterpart.  The green test file protects only those invented
integer values and does not compare any referenced API behavior.

## Other missing assertions and diagnostics

- No documentation states whether this enum is a temporary internal classifier,
  a supported public API, or a permanent replacement for reflection.
- Tests do not cover enum serialization, unknown values, or an include/use
  boundary separate from the test's literal numeric assumptions.

## Final assessment

The enum is mechanically stable but its public identity and counterpart claim
are incompatible with the local .NET type.  No source or test was modified
during this audit.
