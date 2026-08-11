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

### Status: CONFIRMED (DESIGN-COMPLETE) — approval-bound, #2297 `needs_user` (#2296 review, 2026-08-11)

**Both halves reproduce.** `System::MarshalByRefObject obj;` compiles, and the
three .NET members are absent. This is a **conjunction with three separate
compatibility costs**, which is why nothing was selected.

**Premise correction:** this finding was ranked with SR-AUD-129 and SR-AUD-126 as
a "no first-party production consumer" group. That is false here — **two**
production consumers derive from this class, `AppDomain` (which is `final`) and
`ContextBoundObject`.

- **Protected constructor:** breaks **two in-repo construction sites in two
  different executables** — `Batch3TypeTests.cpp:90` and `Task42Tests.cpp:1452` —
  plus any downstream site, which is not inspectable here.
- **Adding the three members:** additive in source, but
  `InitializeLifetimeService()` is **virtual**, so it adds a vtable slot to this
  class *and* to both derived classes; `GetLifetimeService()` is `[Obsolete]` in
  .NET, which collides with the still-undecided #2289; and
  `MemberwiseClone(bool)` has **no base to overload** — measured,
  `System::Object` declares no `MemberwiseClone` at all — so it would be an
  invention rather than a port.
- **The rename alternative** is a wider break than either, since both derived
  classes and every downstream base-specifier change.

**Taken anyway, true under every outcome (#2300):** the header now states the
constructibility divergence and its measured in-repo cost, that the three members
are absent rather than unimplemented, that .NET retains two of them specifically
so a caller receives `PlatformNotSupportedException`, and that restoring the
virtual one is a vtable change for three classes. **This does not close the
finding** — the base is still directly constructible and the members are still
absent. No test was added constructing the base beyond the two that already
exist, and none rejecting construction, since #2297 may make that a hard error.

**Not a family with SR-AUD-129 or SR-AUD-126.**
`docs/CoreMarshalSlotAndFuncShapePlan.md` §2.

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
