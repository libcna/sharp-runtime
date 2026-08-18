<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::ValueType` is no longer directly constructible (ticket #2322)

*2026-08-18.* `System::ValueType v;` compiled. .NET declares `public abstract class ValueType`,
so C# rejects the equivalent.

Landed under `docs/StandingApprovals.md` **SA-8** with SA-2's five conditions. Same shape as
#2339 (`Attribute`). No layout, vtable or `noexcept` change.

---

## 1. What changed

| Spelling | Was | Is |
|---|---|---|
| `System::ValueType v;` | compiled | **rejected** |
| `new System::ValueType()` | compiled | **rejected** |
| `ValueType sliced = derived;` | compiled | **rejected** — the copy member went protected too |
| `std::is_default_constructible_v<ValueType>` | `true` | **`false`** |
| `class MyValue : public ValueType {};` | — | **unchanged**, and the only intended use |

## 2. Protected, not abstract — deliberately

.NET's class is `abstract`. A C++ class becomes abstract only by having a **pure virtual**, and
.NET's `ValueType.ToString()` has a real body — so making one pure here would invent surface the
reference does not have. The protected constructor gets the property that actually matters: the
type can still be a base, and can no longer be an object. A `static_assert` records that
`is_abstract_v<ValueType>` is deliberately `false`.

## 3. What was NOT repaired, and cannot be

.NET's `Equals` and `GetHashCode` compare **fields**, and its `ToString()` returns
`this.GetType().ToString()` — the **runtime type name**. This port returns identity, an
address-derived hash, and the literal `"System.ValueType"`.

All three are **reflection**, permanently out of scope per `CLAUDE.md`, and a C++ base class can
neither enumerate a derived class's fields nor learn its name. A test pins all three as
declarations rather than leaving them to be rediscovered as bugs.

A derived type that needs value semantics must override `Equals` **and** `GetHashCode` together —
both are already `virtual`, so #2322 invented no hook for it — and `ToString` if it wants its own
name.

## 4. A premise the review got wrong

The #2322 review recorded that *"the only derived types anywhere are `SimpleValueType` and
`ConcreteValueType` in `ValueTypeTests.cpp`"*. That was true of **derivations** and missed five
direct **instantiations** in `Batch15TypesTests.cpp`, which the compiler found immediately. They
now go through a local probe, which is what a caller has to do too.

## 5. To migrate

Derive:

```cpp
// before
System::ValueType v;

// after
class MyValue : public System::ValueType {};
MyValue v;
```

A reference or pointer to the base is unaffected.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `System::ValueType` — **zero occurrences in both**.
Neither repository was modified. The downstream ticket is **#2370**.
