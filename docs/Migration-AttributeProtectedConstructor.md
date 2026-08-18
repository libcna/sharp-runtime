<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::Attribute` is no longer directly constructible (ticket #2339)

*2026-08-18.* `System::Attribute a;` compiled. .NET declares the class abstract with a
**protected** constructor and C# rejects the equivalent with CS0144. This port rejects it now too.

Landed under `docs/StandingApprovals.md` **SA-8** — which explicitly authorises *"making a public
default constructor protected … where .NET's is"* — with SA-2's five conditions. No layout, vtable
or `noexcept` change.

---

## 1. What changed

| Spelling | Was | Is |
|---|---|---|
| `System::Attribute a;` | compiled | **rejected** |
| `new System::Attribute()` | compiled | **rejected** |
| `Attribute sliced = derived;` | compiled | **rejected** — the copy member went protected too |
| `std::is_default_constructible_v<Attribute>` | `true` | **`false`** |
| `class MyAttribute : public Attribute {};` | — | **unchanged**, and the only intended use |
| every one of the 46 subclasses | — | **unchanged** |

The class's doc-comment previously said the constructor was *"public so that the class can be
instantiated in tests"* and asked callers to *"treat it as logically abstract"*. A comment asking
callers to behave is not a contract; .NET spells the same intent in the language.

## 2. What was NOT repaired, and cannot be

.NET's `Attribute.Equals` compares **every instance field** and derives its hash from the first
non-array one. **That is reflection**, which `CLAUDE.md` lists as a permanent deviation and which
a C++ base class cannot do at all — it cannot enumerate a derived class's fields. So the identity
default stays.

**The consequence is measured and is pinned by a test rather than left to be rediscovered:**
forty-six types in this repository derive from `Attribute` and **not one** overrides `Equals`, so
all forty-six inherit object identity. Two independently constructed `CLSCompliantAttribute(true)`
objects compare **unequal** here and **equal** in .NET.

A subclass that needs value equality must override **both** `Equals` and `GetHashCode`. Both are
already `virtual`, which is why #2339 invented no new hook for it — overriding only one breaks the
equals/hash contract silently.

## 3. To migrate

Derive. That is the only thing the base was ever for:

```cpp
// before
System::Attribute a;

// after
class MyAttribute : public System::Attribute {};
MyAttribute a;
```

A reference or pointer to the base is unaffected: `const Attribute& ref = derived;` still works.
Only *materialising* a base object does not.

The nine direct instantiations in this repository's own tests were the ones that doc-comment
existed for; they now go through a local derived probe, which is what a caller has to do too.

## 4. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `System::Attribute` at all — **zero occurrences in
both**, and zero direct instantiations. Neither repository was modified. The downstream ticket is
**#2367**.
