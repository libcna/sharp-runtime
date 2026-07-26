# Audit: `modules/core/include/System/MarshalByRefObject.hpp`

## Metadata

- AUDITED: 15-line public base declaration, fully read.
- Validation: `MarshalByRefObjectNewTests.*` passed 2/2 in the combined 14-test
  `ContextBoundObjectTests.*:LocalDataStoreSlotTests.*:MarshalByRefObjectNewTests.*`
  Core.Base filter on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-remoting-audit-probe` prints
  `marshal_default_constructible=1`; the local C# counterpart probe fails with
  `CS0144` when constructing `MarshalByRefObject`.
- Reference basis: local .NET `System/MarshalByRefObject.cs:8-31`.

## SR-AUD-128 — medium — MarshalByRefObject is constructible and omits the remaining public remoting API

The C++ class has an implicitly public default constructor and only a virtual
destructor. It therefore constructs directly, as both the probe and
`MarshalByRefObjectNewTests.DefaultCtor_DoesNotThrow` demonstrate. Current
.NET declares the class abstract with a protected constructor, so the C# probe
rejects `new MarshalByRefObject()` with CS0144.

It also omits the public obsolete `GetLifetimeService()` and virtual
`InitializeLifetimeService()` members that current .NET retains specifically
to throw `PlatformNotSupportedException`, as well as the protected
`MemberwiseClone(bool)` form. A marker-only adaptation can legitimately avoid
remoting, but it must not present a directly instantiable base and silently
remove observable public diagnostics under the corresponding `System` name.
Make the base abstract/protected and expose documented throwing compatibility
members, or rename/document it as a project-specific marker type.

## Other missing assertions and diagnostics

- No direct fixture attempts the absent obsolete methods or checks a stable
  unsupported-feature diagnostic.
- No compile-only test rejects base construction or verifies derived-only
  construction.
- No clone, reflection, serialization, COM-visibility, or cross-context
  boundary vectors exist.

## Final assessment

The virtual destructor is correct for C++ polymorphic ownership, but the
public type contract has the confirmed SR-AUD-128 gap. No source or test was
modified during this audit.
