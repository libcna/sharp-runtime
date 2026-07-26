# Audit: `modules/core/include/System/TypedReference.hpp`

## Metadata

- Audit status: AUDITED (81-line explicit unsupported-feature stub, fully read
  with its supplemental pending-large-fixture coverage).
- Validation: the TypedReference subsection of
  `SystemTypesRemainingTests.cpp` was inspected; its parent source remains
  pending a full-file audit and is not counted as a completed test report.
- Reference basis: local .NET `TypedReference.cs` ref-struct/reflection and
  compiler-intrinsic contract.

## Assessment

The header carefully states that C++ cannot model CLR managed interior
references, `FieldInfo`, or compiler intrinsics. It supplies an ordinary
zero-hash token and throws deterministic `NotSupportedException` for the
reflection-dependent operations. This is a coherent compile-compatibility
adapter, not a partial attempt to dereference C++ memory as a typed reference.

## Other missing assertions and diagnostics

- The only observed tests live in the pending large mixed fixture; no dedicated
  focused filter verifies every unsupported method, exact messages, header
  isolation, or ref-struct escape differences.
- C++ permits default construction, copying, heap allocation, and lifetime
  escape where .NET's `ref struct` forbids them. The compatibility-only policy
  is documented but has no explicit capability marker.
- The `GetTargetType` C++ return type (`std::type_info`) is not the .NET
  `System.Type` contract; a caller cannot obtain even an unavailable sentinel
  without an exception.

## Final assessment

The intrinsic/reflection omission is explicit and no standalone implementation
defect was confirmed. No source or test was modified during this audit.
