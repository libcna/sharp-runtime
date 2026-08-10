# Audit: `modules/core/include/System/ValueType.hpp`

## Metadata

- Audit status: AUDITED (53-line public header-only implementation, fully
  read).
- Validation: `ValueTypeTest.*:ValueTypeTests.*` passed 10/10 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference: local .NET runtime
  `src/coreclr/System.Private.CoreLib/src/System/ValueType.cs`, fully reviewed
  for the public base-type, equality, hash, and `ToString` contracts.

## Assessment

The header documents the lack of C++ reflection and directs concrete wrappers
to override equality and hashing. The virtual destructor and virtual members
make such overrides possible, but the publicly constructible base's defaults
are a material contract change rather than a neutral implementation detail:
they turn every incompletely-overridden value wrapper into identity semantics.

## SR-AUD-068 — medium — ValueType is publicly constructible and defaults to identity rather than .NET value semantics

Current .NET declares `System.ValueType` abstract. Its default `Equals` first
requires the same runtime type and then compares field data; its default
`GetHashCode` derives a value hash from the type and fields; and `ToString`
returns the runtime type name. This C++ class is neither abstract nor
protected-only, `Equals` is `this == &other`, `GetHashCode` narrows the object
address, and `ToString` is the constant `"System.ValueType"`.

Consequently, a public `struct`-like C++ type that derives from `ValueType`
but does not reimplement all three members compares unequal to an equal
independent value and exposes an address-dependent hash. That behavior is
also available by directly constructing `ValueType`, a state no .NET caller
can construct. The header comments make the reflection limitation visible,
but neither the type system nor a diagnostic prevents callers from taking the
incompatible default path.

## Other missing assertions and diagnostics

- No test demonstrates two independently constructed field-equal derived
  values using the base implementation, nor asserts a deliberate documented
  diagnostic for that unsupported default.
- No test ensures a derived type with no override is rejected, warned about,
  or otherwise cannot silently receive identity semantics.
- The base address hash is converted from `uintptr_t` to `intcs`; on a wider
  pointer platform the out-of-range conversion is implementation-defined and
  hash truncation is unreported.
- No assertion contrasts base `ToString` with the derived/runtime type-name
  contract, or covers values with reference, floating-point, and padding
  fields that make a fieldwise replacement nontrivial.

## Final assessment

The reflection-free adaptation is plainly documented, but it changes the
default public semantic contract and is directly locked in by the test suite.
No source or test was modified during this audit.
