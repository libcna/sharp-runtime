<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — interop metadata gets .NET's field types (ticket #1980, group G-5)

*2026-08-19.* `MarshalAsAttribute::Value` becomes get-only, `ArraySubType` becomes an
`UnmanagedType`, `SizeParamIndex` becomes a `short`, two absent fields arrive, and
`VarEnum` / `ComInterfaceType` / `ClassInterfaceType` are added.

The final-audit follow-up in §8 applies the two COM enums to `InterfaceTypeAttribute` and
`ClassInterfaceAttribute`; the original G-5 implementation added their types but accidentally
left both owning attributes as mutable, untyped `intcs` objects.

**This is a public source break**, landed under **SA-8** (the `Value` half) and **SA-5** (the
types and the additions), with SA-2's five conditions discharged.

---

## 1. Why field *types* are the contract here

The same reason group G-2 gave for the values: P/Invoke is a declared permanent deviation, so
these types exist to **preserve the managed metadata**. A field typed as a loose integer where
.NET types it as an enum is therefore not a stylistic choice — it lets **any number** be stored
where only a marshalling kind is meaningful, which is the whole reason the enum exists.

## 2. What changed

| Member | Was | Is | Reference |
|---|---|---|---|
| `Value` | public **mutable** field | **get-only**, via `getValueProperty()` | `MarshalAsAttribute.cs:18` (`{ get; }`) |
| `ArraySubType` | `intcs` | **`UnmanagedType`** | `:29` |
| `SizeParamIndex` | `intcs` | **`shortcs`** | `:30` |
| `SafeArraySubType` | **absent** | `VarEnum` | `:21` |
| `IidParameterIndex` | **absent** | `intcs` | `:25` |
| the class | not sealed | **`final`** | `public sealed partial class` |
| `VarEnum` | absent | added | `VarEnum.cs` |
| `ComInterfaceType` | absent | added | `ComInterfaceType.cs` |
| `ClassInterfaceType` | absent | added | `ClassInterfaceType.cs` |
| `InterfaceTypeAttribute::Value` | public mutable `intcs` | private typed state, get-only `ComInterfaceType` | `InterfaceTypeAttribute.cs` |
| `ClassInterfaceAttribute::Value` | public mutable `intcs` | private typed state, get-only `ClassInterfaceType` | `ClassInterfaceAttribute.cs` |
| both COM attribute classes | derivable, `intcs` constructor only | `final`, enum and raw-`shortcs` constructors | corresponding attribute sources |

`ArraySubType` and `SafeArraySubType` default to **0**, which is **not a declared enumerator** in
either enum (`UnmanagedType` starts at `Bool = 2`). That is deliberate and is the same reasoning
G-2 recorded for the two `CharSet` defaults: .NET's fields have no initializer either.

## 3. Two members stay out of scope, and one of them stays *absent*

.NET types **`MarshalTypeRef`** and **`SafeArrayUserDefinedSubType`** as `Type?`, and
`System::Type` is reflection — a declared permanent deviation of this port.

* `MarshalTypeRef` survives as a `std::string` holding the type's **name**. It is the closest
  available shape and it already existed.
* `SafeArrayUserDefinedSubType` is **absent**, not invented as a second string. A member that
  cannot carry what .NET's carries is worse than no member — it would look like parity while
  storing something else.

Both are stated in the header rather than left implicit.

## 4. To migrate

```cpp
attr.Value;                 // was — read
attr.getValueProperty();    // now

attr.Value = UnmanagedType::I4;          // was — write
attr = MarshalAsAttribute(UnmanagedType::I4);   // now: the type is fixed at construction

attr.ArraySubType = 7;                   // was
attr.ArraySubType = UnmanagedType::I4;   // now

InterfaceTypeAttribute oldI(static_cast<SharpRuntime::intcs>(2)); // was
oldI.Value = 1;                                                    // was writable

InterfaceTypeAttribute i(ComInterfaceType::InterfaceIsIDispatch); // now, preferred
InterfaceTypeAttribute rawI(static_cast<SharpRuntime::shortcs>(2)); // compatibility route
i.getValueProperty();                    // now typed and get-only
```

**First-party migration was one site**, a test, and the compiler found it.

## 5. Evidence

Six mutations, **all caught**, four at compile time:

| Mutation | Caught by |
|---|---|
| M1 — `Value` public and mutable again | the migrated test (compile time) |
| M2 — `ArraySubType` back to `intcs` | `Fix1980G5_ArraySubTypeIsAnUnmanagedTypeNotAnInt` (compile time) |
| M3 — `SizeParamIndex` back to `intcs` | `Fix1980G5_SizeParamIndexIsAShortNotAnInt` (compile time) |
| M4 — a `VT_UNUSED15 = 15` enumerator inserted | `VarEnumCensus` (compile time) — **only after that census was written** |
| M5 — a `VarEnum` flag value wrong | `Fix1980G5_VarEnumCarriesItsHoleAndItsFlags` |
| M6 — a `ComInterfaceType` value wrong | `Fix1980G5_TheTwoComEnumsExist` |

**M4 is the one worth recording.** .NET's `VarEnum` jumps from `VT_DECIMAL = 14` straight to
`VT_I1 = 16` — there is no member with value 15. Asserting that 14 and 16 are the neighbours
catches a **renumbering** but *not* an **insertion**, and M4 went uncaught at first for exactly
that reason: C++ offers no way to enumerate an enum's members.

The fix is an **exhaustive `switch` with no `default:` label**. The build runs with
`-Wall -Wextra -Werror`, so gcc's `-Wswitch` turns any unhandled enumerator into a compile error:

```
error: enumeration value 'VT_UNUSED15' not handled in switch [-Werror=switch]
```

That pins the enum's **membership**, which no value assertion can. It also fails in the other
direction — removing an enumerator breaks the census's own reference to it.

**`VarEnum`'s flag values** are the other row a transcription gets wrong: `VT_VECTOR`, `VT_ARRAY`
and `VT_BYREF` sit at `0x1000`, `0x2000` and `0x4000`, far above the rest.

Negative consumer fixture: `test/consumer/runtime_marshalas_shape_negative.cpp`, four sites, all
rejected. Fixture set grows to **43 fixtures / 223 sites**. Site 2 is the spelling this ticket
exists for — assigning to `Value`, which let a caller retarget an attribute after construction.

Gate: **17,515 run, 17,515 passed, 0 failed, 0 skipped** across 38 executables — `+9` on 17,506
(`SharpRuntimeTests_Runtime` 189 → 198; one pin migrated in place, nine cases added). No other
executable moved. Module graph unchanged at 41/93.

## 6. Downstream, measured

`MarshalAsAttribute` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`. Neither
repository was modified.

## 7. Scope at the original landing

At the time this was recorded as closing **G-5** of #1980. G-1, G-2 and G-4 had landed earlier the
same day and **only G-3 remained** —
reparenting `AmbiguousImplementationException` and five attributes and sealing them, a vtable
*and* layout change that SA-3 explicitly excludes.

## 8. Final-audit COM-attribute follow-up (2026-08-22)

The original implementation added `ComInterfaceType` and `ClassInterfaceType` but never connected
them to the two attributes that own those contracts. Both classes still exposed public mutable
`intcs Value` and accepted only `intcs`, exactly the residual clause in SR-AUD-167.

`InterfaceTypeAttribute` and `ClassInterfaceAttribute` are now `final`. Each stores its enum in a
private field, returns that enum from `getValueProperty()`, and provides the two .NET construction
routes: the strongly typed enum and the raw 16-bit compatibility value. The raw constructor casts
the value without rejecting unnamed values, matching the metadata compatibility overload.

The enum underlying type remains `SharpRuntime::intcs`, so replacing the public integer field with
the private enum field does not grow either object. Layout relationship tests verify that the
typed field replaces rather than supplements the old state.

Two Runtime tests pin finality, getter return type, absence of public `Value`, both constructors,
named values, unnamed raw-short values and layout. The negative fixture grows from four to
**eight sites**; the four new sites independently reject reading/writing the old fields and
deriving from either class. Its baseline exercises both typed and raw-short construction.

The explicitly accepted `MarshalTypeRef`/`SafeArrayUserDefinedSubType` reflection deviation is
unchanged. No reflection or interop execution subsystem was added.

Focused validation after both final-audit repairs: `SharpRuntimeTests_Runtime` **207/207** and the
two affected negative fixtures **18/18 sites rejected**.
