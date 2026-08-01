<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — floating-point instantiations of seven Collections containers

**Ticket #1919** (family #1912, the Collections continuation of CCF-010).
Approved on 2026-07-31 against `docs/CollectionsComparisonContractPlan.md` §10.

**Correction/extension — ticket #1925 (2026-08-01):** the original #1919 text
below is preserved as its historical migration record. The coordinated approval
in `docs/CompositeFloatingKeyPolicyDesign.md` now additionally selects exactly
the three direct `std::optional<float>`, `std::optional<double>`, and
`std::optional<long double>` forms. Section 11 records that new boundary and
supersedes the optional-specific statements in sections 1, 2, and 7; it does
not extend to any other non-floating type.

This note exists for one reason: for a **`float`, `double` or `long double`**
element or key, seven container templates now expose a **different public C++
type** than they did before. Nothing else about them changed, and **no
non-floating instantiation changed in any respect**.

---

## 1. What changed, in one paragraph

`SortedSet<T>`, `Dictionary<K,V>`, `HashSet<T>`, `FrozenSet<T>`,
`FrozenDictionary<K,V>`, `ReadOnlySet<T>` and `ReadOnlyDictionary<K,V>` back
themselves with a `std::set`, `std::map`, `std::unordered_set` or
`std::unordered_map`. Until #1919 those used `std::less<T>`, `std::hash<T>` and
`std::equal_to<T>`. They now use `System::detail::DefaultKeyLess<T>`,
`DefaultKeyHash<T>` and `DefaultKeyEqual<T>` — the C++ statement of .NET's
`Comparer<T>.Default` and `EqualityComparer<T>.Default`.

Each of those three aliases is defined as

```cpp
template<typename T>
using DefaultKeyLess = std::conditional_t<std::is_floating_point_v<T>,
                                          DefaultLess<T>, std::less<T>>;
```

so for **every non-floating `T` it *is* the standard default**, token for
token. `std::set<T, std::less<T>>` and `std::set<T>` are the same type, so for
those instantiations there is no change to make and nothing to rebuild for this
reason.

---

## 2. Which floating-point instantiations change public types

`float`, `double` and `long double` — every type for which
`std::is_floating_point_v<T>` is `true`. `std::optional<double>`,
`std::pair<int,double>`, `std::tuple<double>` and a user-defined type holding a
floating member are **not** floating-point types and are **not** affected;
see §7.

| Type | Public surface that changes for a floating element/key |
|---|---|
| `Dictionary<K,V>` | both `ToMap()` overloads' return type; `iterator`; `const_iterator` |
| `HashSet<T>` | `iterator`; `const_iterator` |
| `FrozenSet<T>` | `const_iterator`; `CreateFromSet`'s parameter type |
| `FrozenDictionary<K,V>` | `const_iterator`; `CreateFromMap`'s parameter type |
| `ReadOnlySet<T>` | the `shared_ptr` constructor's parameter type |
| `ReadOnlyDictionary<K,V>` | the `shared_ptr` constructor's parameter type; `getDictionaryProperty()`'s return type (`protected`) |
| `SortedSet<T>` | **nothing.** See §3 — this is a correction to the plan. |

Each affected class now publishes an alias for its own backing type:

```
Dictionary<K,V>::MapType          HashSet<T>::SetType
FrozenDictionary<K,V>::MapType    FrozenSet<T>::SetType
ReadOnlyDictionary<K,V>::MapType  ReadOnlySet<T>::SetType
```

**Spelling the alias is correct for every element or key type, floating or
not.** It is the migration, and it needs no conditional.

---

## 3. Correction to the plan: `SortedSet` is not affected

`docs/CollectionsComparisonContractPlan.md` §6.1 and §10 list
`SortedSet<T>::SetIterator` and `SortedSet<T>::comparer()` as **public**.
Measured at the HEAD #1919 was implemented against, both are **private** — they
are declared above the class's `public:` label. The only public declaration
that mentions the backing type at all is `SortedSet<T>::Iterator`'s
constructor, whose other parameter is the private nested `State`, so no
consumer can call it or name its parameter types.

`SortedSet<T>` is therefore **representation-private in practice** and its
repair changes no type a consumer can name — it belongs with the ten
compatible containers of #1918, not with the six here. It was implemented under
#1919's approval anyway, which names it explicitly. The original §10 text is
preserved; this is the correction, per the family's additive-correction
convention.

---

## 4. What source code may fail to compile

Only code that names the backing `std::` container of a **floating**
instantiation. Concretely:

```cpp
// BEFORE -- now rejected for a floating element/key
auto backing = std::make_shared<std::unordered_set<double>>();
ReadOnlySet<double> ro(backing);

std::unordered_map<double,int> m;
auto f = FrozenDictionary<double,int>::CreateFromMap(m);

const std::unordered_map<double,int>& raw = dict.ToMap();

std::unordered_set<double>::iterator it = hashSet.begin();
```

```cpp
// AFTER -- correct for EVERY element/key type
auto backing = std::make_shared<ReadOnlySet<double>::SetType>();
ReadOnlySet<double> ro(backing);

FrozenDictionary<double,int>::MapType m;
auto f = FrozenDictionary<double,int>::CreateFromMap(m);

const Dictionary<double,int>::MapType& raw = dict.ToMap();

HashSet<double>::iterator it = hashSet.begin();
```

`auto` and range-`for` need no change at all, in either direction. Every
failure is a **hard compile error** naming both types — there is no silent
behaviour change and no way to link the old spelling against the new headers.

`test/consumer/collections_floating_comparer_negative.cpp` pins all eight
boundary cases: four rejected raw spellings, one rejected reference binding,
one static assertion that the floating type genuinely moved, one that the
non-floating type did **not**, and one rejected iterator spelling. Its `#else`
branches are the migrated spellings and are what the clean baseline compiles.

---

## 5. What must be rebuilt

**Every consumer of these headers, unconditionally.** All seven classes are
header-only templates, so a consumer that is not recompiled keeps its old
inline bodies and its old backing types. The linker cannot detect the mismatch
— there is no non-template symbol for it to disagree about. This is the same
standing note CCF-010 and #1918 carry, and it applies whether or not the
consumer uses a floating instantiation.

`Collections.Core` is an `INTERFACE` target and produces no archive.

---

## 6. Serialized and persisted data

**Unaffected.** None of the seven has a serializer, a formatter, or an on-disk
representation. What changes is which in-memory values are considered the same
key, and the direction of that change is strictly toward .NET's answer: two
NaNs that .NET calls one key are now one key here too. A program that persisted
its *own* rendering of a container's contents will read the same bytes back;
what it may observe is that reloading now produces the .NET count rather than
the inflated one — for example a frozen set built from `{NaN, NaN, 1}` used to
report `Count` 3 and now reports 2, which is what .NET reports.

---

## 7. What the policy does *not* cover

`std::is_floating_point_v` is the selector, so the contract does **not** reach
inside a composite type. `Dictionary<std::optional<double>,V>` still uses
`std::equal_to<std::optional<double>>`, which compares the contained doubles
with raw `==`, so a NaN key is not equal to itself and is unfindable **by the
very object that was inserted**. .NET's `Dictionary<double?,V>` finds it,
because `EqualityComparer<double?>.Default` is NaN-reflexive.

That divergence is measured (probe row `D10`), recorded as **ticket #1925**,
and pinned by the permanent test
`CollectionsComparisonContract.NullableFloatingKeysKeepRawIeeeEqualityForNow`
so it cannot change silently. Repairing it means making the policy recurse into
`optional`/`pair`/`tuple`, which would move a further family of public template
types and therefore needs its own approval.

---

## 8. Custom comparers

Unchanged and unaffected. Nothing in #1919 touches a caller-supplied comparer
or equality comparer: the affected containers take their predicate as a
template argument with a **default**, and only that default moved. A consumer
that wants the old raw-IEEE behaviour back for one container can name the
standard predicate explicitly — for example by holding a
`std::unordered_set<double, std::hash<double>, std::equal_to<double>>` directly
— and gets exactly today's pre-#1919 semantics, including the silent data loss.

---

## 9. Rollback

Per container, the change is one alias plus its uses:

| File | Revert |
|---|---|
| `Generic/SortedSet.hpp` | `BackingSet` → `std::set<T>` |
| `Generic/Dictionary.hpp` | `MapType` → `std::unordered_map<TKey,TValue>` |
| `Generic/HashSet.hpp` | `SetType` → `std::unordered_set<T>` |
| `Frozen/FrozenSet.hpp` | `SetType` → `std::unordered_set<T>` |
| `Frozen/FrozenDictionary.hpp` | `MapType` → `std::unordered_map<TKey,TValue>` |
| `ObjectModel/ReadOnlySet.hpp` | `SetType` → `std::unordered_set<T>` |
| `ObjectModel/ReadOnlyDictionary.hpp` | `MapType` → `std::unordered_map<K,V>` |

Reverting an alias restores the pre-#1919 behaviour of that container exactly,
including its defects. Reverting the whole family is
`git revert` of the three commits `#1921`, `#1922`, `#1923`. The permanent
tests added by each will fail loudly, which is the intended signal.

---

## 10. Evidence

- Before/after behaviour: `build-probe/1919_prefix_plain.log` and
  `build-probe/1919_postfix_plain.log` (46 cases).
- Representation and external-symbol inventory:
  `build-probe/1919_{prefix,postfix}_layout.log` and `…_symbols.log`.
- Design and approval: `docs/CollectionsComparisonContractPlan.md` §10, §16, §17.
- Boundary fixture: `test/consumer/collections_floating_comparer_negative.cpp`.
- Permanent tests:
  `modules/collections/tests/System/Collections/CollectionsComparisonContractTests.cpp`.

---

## 11. #1925 direct nullable-floating extension

For exactly `std::optional<float>`, `std::optional<double>`, and
`std::optional<long double>`, the three aliases now select
`DefaultLess<T>`, `DefaultHash<T>`, and `DefaultEqualTo<T>`. Null orders before
present and hashes to zero; two present values delegate to the established
floating policy, so all NaNs are equal/canonically hashed and ordered before
negative infinity, and both zero signs are equal and hash alike. Raw optional
operators remain unchanged.

This changes the public `MapType`/`SetType` aliases of `Dictionary`, `HashSet`,
`FrozenDictionary`, `FrozenSet`, `ReadOnlyDictionary`, and `ReadOnlySet` for all
three forms. Consequently, the `ToMap`, projection constructor, and
`CreateFromMap`/`CreateFromSet` signatures naming those aliases also move. Use
the owning class's alias or `auto`, exactly as shown in section 4:

```cpp
using Key = std::optional<double>;

// Old raw spelling: no longer compatible with the policy-bearing public type.
// std::unordered_map<Key, int> raw;
// auto frozen = FrozenDictionary<Key, int>::CreateFromMap(raw);

Dictionary<Key, int>::MapType map;
auto frozen = FrozenDictionary<Key, int>::CreateFromMap(map);

auto backing = std::make_shared<ReadOnlySet<Key>::SetType>();
ReadOnlySet<Key> view(backing);
```

Measured on the supported x86-64/libstdc++ 14 toolchain, the iterator premise
is narrower than “every iterator changes.” Predicate template arguments move,
but libstdc++ node iterator identity erases them. The public/deduced hash-node
iterators for nullable `float` and nullable `double` remain the same type.
Nullable `long double` iterators change because the old standard hasher cached
node hashes while the selected non-throwing policy hasher does not. Ordered
tree iterators and the nominal nested `SortedSet`/`SortedDictionary` iterator
types remain the same. The backing/container member types and affected inline
symbols still change in every approved instantiation; unchanged iterator
identity is not ABI neutrality.

Every one of the sixteen consumers inventoried by
`CompositeFloatingKeyPolicyDesign.md` inherits the new behavior. The six
public backing aliases above require source migration where callers used raw
standard types. The other ten consumers have private predicate-bearing storage
or `std::function` predicate storage, so their source-visible class template
name stays fixed while inline/template bodies must be rebuilt. A clean,
coordinated rebuild of every header consumer is required.

The extension deliberately does not select non-floating optionals, nested
optionals, pair, tuple, array, variant, vector, or arbitrary user-defined
types. In particular, pair/tuple hashed collections remain ill-formed; no new
hash capability was added. The negative-consumer fixture sites 9–15 pin the
new raw-container/alias boundary and the measured nullable-long-double iterator
transition.
