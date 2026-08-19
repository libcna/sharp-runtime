<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — three attribute shapes match .NET (ticket #1980, group G-4)

*2026-08-19.* `CompilerFeatureRequiredAttribute` loses its `IsOptional` setter and gains a
two-argument constructor; `ObsoletedOSPlatformAttribute` and `RequiresPreviewFeaturesAttribute`
lose their `url` constructor parameter and gain a `Url` setter.

**This is a public source break**, landed under **SA-8** (the mutability half) and **SA-5** (the
`Url` half), with SA-2's five conditions discharged. No layout or vtable change.

---

## 1. The port was wrong in **both** directions

That is the point of this group, and it is why the two halves could not be fixed by one rule.

| | Port had | .NET has | Port was |
|---|---|---|---|
| `CompilerFeatureRequiredAttribute::IsOptional` | full setter | `{ get; init; }` | **too permissive** |
| `ObsoletedOSPlatformAttribute::Url` | `url` ctor param, read-only accessor | no such param; `{ get; set; }` | **too restrictive** on the property, **inventive** on the constructor |
| `RequiresPreviewFeaturesAttribute::Url` | same | same | same |

## 2. `init` is two facts, not one

.NET's `public bool IsOptional { get; init; }` means the value **can** be supplied at
construction —

```csharp
new CompilerFeatureRequiredAttribute("RefStructs") { IsOptional = true }   // legal
```

— and **cannot** be assigned afterwards. C++ has no `init`, and the exact analogue of that pair is
a **constructor parameter with no setter**.

So removing `setIsOptionalProperty` *alone* would have been a narrowing, not a translation: the
value would have become unsettable. The two-argument constructor is what makes the removal
faithful. A test asserts both halves together, and the absence pin uses a **dependent** parameter,
because gcc evaluates a non-dependent `requires` eagerly and hard-errors instead of yielding
`false` — the #2299 trap.

## 3. `Url` was inverted

.NET declares exactly:

```csharp
public ObsoletedOSPlatformAttribute(string platformName)                    // PlatformAttributes.cs
public ObsoletedOSPlatformAttribute(string platformName, string? message)
public string? Message { get; }
public string? Url     { get; set; }

public RequiresPreviewFeaturesAttribute()                                   // …Attribute.cs:28
public RequiresPreviewFeaturesAttribute(string? message)                    // …Attribute.cs:34
public string? Url { get; set; }                                            // …Attribute.cs:47
```

There is **no** `url` constructor parameter in either type, and `Url` is **settable**. This port
had a third (respectively second) parameter .NET does not declare, feeding an accessor that .NET
makes writable.

## 4. To migrate

```cpp
// before
CompilerFeatureRequiredAttribute f("RefStructs");
f.setIsOptionalProperty(true);
ObsoletedOSPlatformAttribute o("ios", "Use X", "https://example.com");
RequiresPreviewFeaturesAttribute p("Preview", "https://aka.ms/preview");

// after
CompilerFeatureRequiredAttribute f("RefStructs", true);
ObsoletedOSPlatformAttribute o("ios", "Use X");
o.setUrlProperty("https://example.com");
RequiresPreviewFeaturesAttribute p("Preview");
p.setUrlProperty("https://aka.ms/preview");
```

**First-party migration was two sites**, both tests, and the compiler found both. Nothing in any
`modules/*/src` uses these types.

## 5. Evidence

Five mutations, **all caught**, two at compile time:

| Mutation | Caught by |
|---|---|
| M1 — `setIsOptionalProperty` reinstated | `Decl1980G4_IsOptionalIsNotMutableAfterConstruction` (compile time) |
| M2 — the two-argument constructor ignores its second argument | `StoresFeatureNameAndOptionalFlag` |
| M3 — the `url` constructor parameter restored | `Fix1980G4_UrlIsASettablePropertyNotAConstructorArgument` (compile time) |
| M4 — `ObsoletedOSPlatformAttribute::setUrlProperty` silently does nothing | that type's own case |
| M5 — `RequiresPreviewFeaturesAttribute::setUrlProperty` silently does nothing | that type's own case |

M4 and M5 are run **once per type** on purpose: they are two separate declarations, and fixing
one while leaving the other is the easy half-repair. M4 was invalid as first written — the anchor
spanned two doc-comments that differ between the types — and was reformulated rather than counted.

Negative consumer fixture: `test/consumer/runtime_g4_attribute_shape_negative.cpp`, four sites,
all rejected. Fixture set grows to **42 fixtures / 219 sites**. Site 4 is the trait query — the
shape that breaks a consumer **silently** rather than at the call site.

Gate: **17,506 run, 17,506 passed, 0 failed, 0 skipped** across 38 executables — `+3` on 17,503
(`SharpRuntimeTests_Runtime` 186 → 189; two pins migrated in place, three cases added). No other
executable moved. Module graph unchanged at 41/93.

## 6. Downstream, measured

All three types appear in **zero** places in `cna` and **zero** in `mobile-eggbert`. Neither
repository was modified, and no downstream ticket is needed.

## 7. Scope

This closes **G-4** of #1980. G-1 and G-2 landed earlier the same day. Remaining: **G-3**
(reparenting and sealing — a vtable *and* layout change SA-3 excludes) and **G-5** / SR-AUD-167
(retyping `MarshalAs` fields, adding `ComInterfaceType`/`ClassInterfaceType`).
