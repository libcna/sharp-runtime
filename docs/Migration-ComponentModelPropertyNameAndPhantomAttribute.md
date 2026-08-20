<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `PropertyName` is get-only and nullable, and `ComponentModel::Attribute` is gone (ticket #2405)

*2026-08-20.* Two independent findings from one sweep of `modules/component-model`, the pass that
followed **#2403**.

**Downstream, measured:** **zero** sites for either half in `cna` and in `mobile-eggbert`.
Downstream record: **#2407**.

---

## Half A — the duplicated, mutable, lossy `PropertyName`

`PropertyChangedEventArgs` and `PropertyChangingEventArgs` each carried **two representations of one
fact**: a private `std::optional<std::string> propertyName_` and a **public, mutable
`std::string PropertyName`** snapshotted from it in the constructor.

.NET's is four lines (`PropertyChangedEventArgs.cs`):

```csharp
public class PropertyChangedEventArgs : EventArgs
{
    public PropertyChangedEventArgs(string? propertyName) { PropertyName = propertyName; }
    public virtual string? PropertyName { get; }
}
```

**Three defects lived in that one member:**

1. **Lossy.** `value_or("")` collapsed `std::nullopt` and `""` into the same state — the **#2295**
   defect, where an absent value and an empty one become indistinguishable. And the absent state is
   not a corner case here: .NET documents a null or empty name as *"all properties may have
   changed"*.
2. **Mutable**, where .NET's is get-only. A subscriber could retarget the args object mid-dispatch,
   and every later subscriber would see the changed value.
3. **The two could disagree.** After such a write, `PropertyName` and `getPropertyNameProperty()`
   reported different things and the object contradicted itself. That is the worst of the three, and
   it is reachable *only* because the field was public.

**The header already documented defect 1** — the field's own doc-comment said it was *"empty when
getPropertyNameProperty() has no value"* — and kept it *"for existing sharp-runtime consumers that
predate the nullable-property port"*. **That reason was measured and is empty**: zero downstream
sites, and one first-party read in this repository's own tests.

### What a caller changes

| Was | Now |
|---|---|
| `args.PropertyName` | `args.getPropertyNameProperty()` → `const std::optional<std::string>&` |
| `args.PropertyName = x` | *(no replacement — .NET publishes no setter)* |

A caller that always supplies a name reads `.value()`; a caller that handles .NET's all-properties
convention checks `has_value()` first. The single first-party site took `.value()`, and that is an
assertion in itself: it would throw if the collection ever raised the `nullopt` notification.

---

## Half B — `System::ComponentModel::Attribute` is removed

```cpp
namespace System::ComponentModel { class Attribute { public: virtual ~Attribute() = default; }; }
```

**There is no `System.ComponentModel.Attribute` in .NET** — measured: no such file anywhere in the
reference tree. .NET's ComponentModel attributes derive from `System.Attribute`.

**And so do this port's.** Measured across the module: **20** derive from `System::Attribute`, **11**
from `ValidationAttribute`, and **zero** from the removed type. It had no members, no derived
classes and no callers. Its only appearance outside its own header was one test asserting it could
be default-constructed.

This is the **#2334** (`RuntimeType`) / **#2281** (`UnitySerializationHolder`) shape: a type that
existed only because someone believed .NET had one. **Migration: derive from `System::Attribute`**,
which every attribute in this namespace already did.

---

## Testing

Negative fixture `test/consumer/component_model_propertyname_and_phantom_attribute_negative.cpp`,
**4 sites**: a read and a **write** for `PropertyChangedEventArgs`, a read for the sibling
`PropertyChangingEventArgs` — repairing one and not the other would have left the port disagreeing
with itself — and the removed type. Fixture set **50 / 256 → 51 / 260**.

Three mutations, **all caught**. The interesting one is the pin they hang on: with a second
representation gone, the property worth asserting is that **there is exactly one**, so the case
compares `sizeof` against a one-`optional` shadow struct. That catches a reinstated field on either
type, which no value-based assertion would.

**One vacuous test was replaced rather than deleted.**
`EXPECT_NO_THROW(System::ComponentModel::Attribute{})` asserted that an empty type can be
default-constructed. What stands in its place asserts the fact that actually matters — that these
attributes derive from `System::Attribute` — and it dispatches **through a base reference** rather
than only `static_assert`ing, because a `static_assert` alone would pass against a base nothing can
call through. That is the fourth "assertion that cannot fail" this sweep has found.
