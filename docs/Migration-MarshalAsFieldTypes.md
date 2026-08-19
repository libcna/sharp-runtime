<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `MarshalAsAttribute` gets .NET's field types, and the two COM enums arrive (ticket #1980, group G-5)

*2026-08-19.* `MarshalAsAttribute::Value` becomes get-only, `ArraySubType` becomes an
`UnmanagedType`, `SizeParamIndex` becomes a `short`, two absent fields arrive, and
`VarEnum` / `ComInterfaceType` / `ClassInterfaceType` are added.

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

## 7. Scope

This closes **G-5** of #1980. G-1, G-2 and G-4 landed earlier the same day. **Only G-3 remains** —
reparenting `AmbiguousImplementationException` and five attributes and sealing them, a vtable
*and* layout change that SA-3 explicitly excludes.
