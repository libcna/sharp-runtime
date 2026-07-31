<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# The Collections default comparison contract family (ticket #1912)

Design record for ticket **#1912**, written by design ticket **#1913**.

This family is the `Collections` continuation of **CCF-010**
(`docs/ComparisonContractPlan.md`, SR-AUD-046, tickets #1904–#1910). It carries
**no `SR-AUD-*` identifier**: audit numbering stays frozen at **364**, exactly as
CCF-010's §18a recorded when it opened #1912. Everything below is a
*newly discovered population found during remediation*, tracked as ordinary
post-audit tickets.

The one-line statement of the family: **a .NET collection compares its elements,
keys and values with `Comparer<T>.Default` and `EqualityComparer<T>.Default`,
never with the operand type's `<` or `==`; the port uses `<` and `==` everywhere,
and for `float` and `double` those are different functions.**

---

## 0. What #1912 said, and what measurement says

#1912's inventory came from a token sweep written while planning CCF-010
(`docs/ComparisonContractPlan.md` §18a) and was explicitly labelled there as
"measured-by-analogy rather than by their own probe", with the instruction that
"#1912 must reproduce each site itself before claiming anything". This section
is that reproduction's verdict. **The original §18a text stays as written**; the
corrections are additive, per the convention SR-AUD-081, SR-AUD-100 and
SR-AUD-362 established.

| #1912's claim | Verdict | Measured |
|---|---|---|
| 5 default-ordering sites | **correct** | 5, all reproduced (§3, cases L1/L3/L5/L6/L7) |
| the four `std::sort` sites carry §6.2's precondition violation | **correct, and understated** | 164 of 196 shapes corrupted, worst 216,078,912 inversions (§3.2) |
| 3 ordered associative containers | **understated** | 5 ordered containers; `ImmutableSortedSet` and `ImmutableSortedDictionary` were not named |
| 56 default-equality sites across 20 headers | **wrong in composition, close in count** | **55** defect-capable sites across **16** headers; 11 of the sites the named header list implies are **not** defect-capable (§2.4) |
| — | **entirely omitted** | the **six named default comparers** — the port's own `Comparer<T>.Default` and `EqualityComparer<T>.Default` (§2.1) |
| — | **entirely omitted** | **11 hash-based containers**, whose failure is worse than the ordered ones (§2.5) |
| — | **entirely omitted** | `PriorityQueue<TElement,TPriority>` (§2.5) |
| "not simply the same fix again" (§18a.2) | **correct** | and it splits further: the containers divide into a *compatible* half and a *public-representation* half (§6) |

Two premises of the family as a whole also need correcting, and both make the
family **more** serious rather than less:

1. §18a called the associative-container shape a lifetime-of-the-container
   ordering-requirement violation. It is that, but the *observable* consequence
   is **silent data loss**, not merely an invalid container:
   `SortedSet<double>` with `Add(NaN); Add(1); Add(2)` ends with **`Count == 1`**
   and contents `[NaN]` — elements 1 and 2 were accepted by the call and
   discarded (case S2b).
2. The hash-based containers, which §18a does not mention at all, fail in the
   opposite direction and unboundedly: `Dictionary<double,int>` accepts the
   **same** NaN key any number of times, so `Count` grows while `ContainsKey`
   and `TryGetValue` answer *false* forever (cases S9/S20).

---

## 1. Scope

**In scope.** Every entry point in `modules/collections*` whose .NET counterpart
uses `Comparer<T>.Default` or `EqualityComparer<T>.Default`, and every backing
`std::` container whose default comparator/hasher/equality stands in for one of
them.

**Out of scope, deliberately.**

1. **Caller-supplied comparers.** `List<T>::Sort(comparison)`,
   `ArrayList::Sort(IComparer)`, `ImmutableList<T>::Remove(item, comparer)`,
   `ImmutableSortedSet<T>::Create(less)` and every sibling pass the caller's
   predicate straight through. A caller who supplies an inconsistent comparer
   gets undefined behaviour in .NET too. Same rule as
   `docs/ComparisonContractPlan.md` §18.1.
2. **Sites whose operand type cannot be floating-point** — `ArrayList` and the
   non-generic `Queue`/`Stack` (`void*` identity through `std::any`),
   `ListDictionaryInternal`, `Specialized::ListDictionary`, `StringCollection`
   and `StringDictionary` (`std::string`). Eleven sites, listed in §2.4. They
   are *shaped* like the defect and cannot exhibit it. Recorded, not changed.
3. **`ReferenceEqualityComparer`.** Pointer identity is the contract; it is a
   declared negative control (case C11) and must not change.
4. **Non-generic `Collections::Comparer`** (`const void*` pointer ordering) and
   **`StructuralComparisons`**. Both already carry a permanent-limitation note
   in their own headers: C++ has no common object root, so a `const void*`
   comparer cannot reach the pointee's comparison logic. Unrelated cause.
5. **`NonRandomizedStringEqualityComparer`** — `std::string` only.
6. **`ImmutableArray<T>::Replace` returning the array unchanged when the old
   value is absent**, where .NET throws `ArgumentException` (its `ImmutableList`
   sibling does throw). A real divergence, unrelated cause, **no `SR-AUD-*`**;
   recorded here in §11 so a future reader does not mistake it for part of this
   family.
7. **Anything outside this repository.** CNA and mobile-eggbert are not
   inspected; ticket #1773 stays blocked.

---

## 2. Inventory

### 2.1 The named default comparers — 6 classes, omitted by #1912

These are not "sites that behave like `Comparer<T>.Default`". They **are** the
port's `Comparer<T>.Default` and `EqualityComparer<T>.Default`, and every one of
them is the raw built-in operator.

| Class | File:line | Body today | .NET |
|---|---|---|---|
| `Generic::Comparer<T>::Default()` | `Generic/Comparer.hpp:47-48` | `if (x<y) -1; if (y<x) 1; 0` | `GenericComparer<T>.Compare` → `x.CompareTo(y)` |
| `Generic::EqualityComparer<T>::Default()` | `Generic/EqualityComparer.hpp:58-61` | `x == y`, `std::hash<T>` | `GenericEqualityComparer<T>` → `x.Equals(y)`, `x.GetHashCode()` |
| `Generic::ObjectComparer<T>::Compare` | `Generic/ObjectComparer.hpp:33-34` | `if (x<y) -1; if (y<x) 1; 0` | `ObjectComparer<T>` → `Comparer.Default` |
| `Generic::ObjectEqualityComparer<T>` | `Generic/ObjectEqualityComparer.hpp:35,46` | `x == y`, `std::hash<T>` | `ObjectEqualityComparer<T>` |
| `Generic::NullableComparer<T>::Compare` | `Generic/NullableComparer.hpp:37-38` | `*x < *y` | `NullableComparer<T>` → `Comparer<T>.Default` |
| `Generic::NullableEqualityComparer<T>` | `Generic/NullableEqualityComparer.hpp:39,52` | `*x == *y`, `std::hash<T>` | `NullableEqualityComparer<T>` → `EqualityComparer<T>.Default` |

`Comparer<double>::Default().Compare(NaN, x)` returns **0 for every `x`**
(cases C1–C4). That is not merely a wrong answer. `0` means *equivalent*, so
NaN is equivalent to `1.0` and equivalent to `2.0` while `1.0` and `2.0` are
not equivalent to each other: the induced equivalence is **not transitive**, and
the object is therefore not a valid comparator for `std::sort`, for
`std::lower_bound`, for `std::set`, or for any caller who hands it to one. It is
the same `[alg.sort]`/`[associative.reqmts]` precondition violation as
CCF-010 §6.2, one level of indirection further out.

### 2.2 Default-ordering sites — 5, exactly as #1912 said

| Site | Entry | Algorithm |
|---|---|---|
| `Generic/List.hpp:420` | `List<T>::Sort()` | `std::sort` |
| `Generic/List.hpp:561` | `List<T>::BinarySearch(item)` | `std::lower_bound` + `*it == item` |
| `Immutable/ImmutableList.hpp:606` | `ImmutableList<T>::Sort()` | `std::sort` |
| `Immutable/ImmutableList.hpp:623` | `ImmutableList<T>::Sort(index,count)` | `std::sort` |
| `Immutable/ImmutableArray.hpp:283` | `ImmutableArray<T>::Sort()` | `std::sort` |

`ArrayList::Sort` (`ArrayList.hpp:552,568`) and every `Sort(comparison)` /
`Sort(comparer)` overload take a caller-supplied predicate and are excluded on
§1.1's rule.

### 2.3 Default-equality sites — 55 defect-capable, across 16 headers

| Header | Lines | Count | Entry points |
|---|---|---|---|
| `Generic/List.hpp` | 159, 164, 245, 562, 580, 603, 616, 633, 662 | 9 | `Contains`, `Remove`, `IndexOf` ×3, `LastIndexOf` ×3, `BinarySearch`'s hit test |
| `Immutable/ImmutableList.hpp` | 147, 172, 177, 346, 390, 470, 848, 859, 878, 920, 950, 1009 | 12 | `Builder::Remove/Contains/IndexOf`, `Replace`, `Remove`, `Builder::Remove(item)`, `Contains`, `IndexOf` ×3, `LastIndexOf` ×2, `BinarySearch`'s hit test |
| `Immutable/ImmutableArray.hpp` | 220, 237, 296, 308, 321 | 5 | `Replace`, `Remove`, `Contains`, `IndexOf`, `LastIndexOf` |
| `Generic/LinkedList.hpp` | 281, 757, 797, 811, 825 | 5 | `LinkedListNode::operator==(const T&)`, `Remove`, `Contains`, `Find`, `FindLast` |
| `Immutable/ImmutableDictionary.hpp` | 84, 97, 145, 169 | 4 | `ContainsValue`, `Contains(pair)`, `SetItem`, `SetItems` |
| `Immutable/ImmutableSortedDictionary.hpp` | 151, 164, 214, 238 | 4 | same four |
| `Concurrent/ConcurrentDictionary.hpp` | 73, 191, 229 | 3 | `TryUpdate`, `AddOrUpdate` ×2 compare-and-retry |
| `ObjectModel/Collection.hpp` | 200, 235, 316 | 3 | `Contains`, `Remove`, `IndexOf` |
| `Generic/SortedList.hpp` | 273, 314, 330 | 3 | `ContainsValue`, `IndexOfKey`, `IndexOfValue` |
| `Generic/Dictionary.hpp` | 180 | 1 | `ContainsValue` |
| `Generic/SortedDictionary.hpp` | 200 | 1 | `ContainsValue` |
| `Generic/OrderedDictionary.hpp` | 307 | 1 | `ContainsValue` |
| `Generic/Queue.hpp` | 172 | 1 | `Contains` |
| `Generic/Stack.hpp` | 172 | 1 | `Contains` |
| `ObjectModel/ReadOnlyCollection.hpp` | 190 | 1 | `IndexOf` (and `Contains`, which delegates) |
| `ObjectModel/KeyedCollection.hpp` | 59 | 1 | `Contains(item)` / `Remove(item)` by value |
| | | **55** | |

One of the 55 is not an equality site at all and must not be repaired as one.
`SortedList<K,V>::IndexOfKey` (`:314`) compares keys with `==`, but .NET's
`SortedList.IndexOfKey` is `Array.BinarySearch(keys, …, comparer)` — **ordering
equivalence**, the same equivalence the backing map uses for membership. Today
that inconsistency is directly visible: `ContainsKey(NaN)` answers `true` while
`IndexOfKey(NaN)` answers `-1` (case S5). It belongs to the container ticket
(#1918), not the value-equality ticket.

### 2.4 Shaped like the defect, not defect-capable — 11 sites

Every one has an operand type that cannot be `float` or `double`. Recorded so a
later sweep does not "find" them again.

| Site | Operand |
|---|---|
| `ArrayList.hpp:327,347` | `void*` recovered from `std::any` — identity, not value |
| `Queue.hpp:149`, `Stack.hpp:150` (non-generic) | `void*` — identity |
| `ListDictionaryInternal.hpp:92,98` | `std::string` key |
| `Specialized/ListDictionary.hpp:27` | `std::string` key |
| `Specialized/StringCollection.hpp:132,164,175` | `std::string` |
| `Specialized/StringDictionary.hpp:163` | `std::string` |

`Hashtable`, `Specialized::OrderedDictionary`, `HybridDictionary` and
`NameValueCollection` are `std::string`-keyed throughout and appear nowhere
above for the same reason.

### 2.5 Ordered and hashed containers — 5 + 11, not 3

**Ordered** (`std::less` on the element or key):

| Type | Backing | Default comparator |
|---|---|---|
| `Generic::SortedSet<T>` | `std::set<T>` | `std::less<T>` |
| `Generic::SortedDictionary<K,V>` | `std::map<K,V>` | `std::less<K>` |
| `Generic::SortedList<K,V>` | `std::map<K,V>` | `std::less<K>` |
| `Immutable::ImmutableSortedSet<T>` | `std::set<T, std::function<…>>` | value `std::less<T>{}` |
| `Immutable::ImmutableSortedDictionary<K,V>` | `std::map<K,V,std::function<…>>` | value `std::less<K>{}` |
| `Generic::PriorityQueue<E,P>` | `std::priority_queue<Entry, vector, std::greater<Entry>>` | `Entry::operator>` → `priority > o.priority` |

**Hashed** (`std::hash` + `std::equal_to`):

| Type | Backing | Floating operand |
|---|---|---|
| `Generic::Dictionary<K,V>` | `std::unordered_map<K,V>` | key |
| `Generic::HashSet<T>` | `std::unordered_set<T>` | element |
| `Generic::OrderedDictionary<K,V>` | `std::unordered_map<K,size_t>` | key |
| `Frozen::FrozenSet<T>` | `std::unordered_set<T>` | element |
| `Frozen::FrozenDictionary<K,V>` | `std::unordered_map<K,V>` | key |
| `Immutable::ImmutableDictionary<K,V>` | `std::unordered_map<K,V>` | key |
| `Immutable::ImmutableHashSet<T>` | `std::unordered_set<T, std::function<…>, std::function<…>>` | element |
| `Concurrent::ConcurrentDictionary<K,V>` | `std::unordered_map<K,V>` | key |
| `ObjectModel::ReadOnlySet<T>` | `shared_ptr<std::unordered_set<T>>` | element |
| `ObjectModel::ReadOnlyDictionary<K,V>` | `shared_ptr<std::unordered_map<K,V>>` | key |
| `ObjectModel::KeyedCollection<K,Item>` | `std::unordered_map<K,intcs>` | key |

---

## 3. Reproduction

`build-probe/1913_collcmp_probe.cpp` (74 cases) and
`build-probe/1913_run.sh <plain|asan|ubsan|assertions> <log>`. Each case runs in
its own process under a 300 s watchdog. Compilation is one translation unit,
strictly serial: **aggregate parallelism 1 job**.

```bash
cd /rv/data/development/github.com/openeggbert/sharp-runtimervc
TMPDIR=$PWD/build-tmp bash build-probe/1913_run.sh plain build-probe/1913_prefix_plain.log
```

Logs: `build-probe/1913_prefix_{plain,asan,ubsan,assertions}.log` (+`.raw`).

### 3.1 Wrong-answer matrix

`port` is the measured answer on the shipped bodies; `.NET` is the reference
answer derived in §4. Rows marked **control** are already correct and must stay
so.

| Case | Surface | Port | .NET |
|---|---|---|---|
| C1 | `Comparer<double>::Default().Compare(NaN, 1)` | `0` | `-1` |
| C2 | `…Compare(NaN, NaN)` | `0` | `0` (agrees by accident) |
| C3 | `…Compare(1, NaN)` | `0` | `1` |
| C4 | `…Compare(NaN, -Inf)` | `0` | `-1` |
| C5 | `EqualityComparer<double>::Default().Equals(NaN,NaN)` | `false` | `true` |
| C6 | `…GetHashCode(NaN) == …GetHashCode(otherNaN)` | `false` | `true` |
| C7 | `ObjectComparer<double>::Compare(NaN,1)` | `0` | `-1` |
| C8 | `ObjectEqualityComparer<double>::Equals(NaN,NaN)` | `false` | `true` |
| C9 | `NullableComparer<double>::Compare(NaN,1)` | `0` | `-1` |
| C10 | `NullableEqualityComparer<double>::Equals(NaN,NaN)` | `false` | `true` |
| **C11** | `ReferenceEqualityComparer<int>` identity | `same:true different:false` | **control, correct** |
| **C12** | `Comparer<int>::Default().Compare(1,2)` | `-1` | **control, correct** |
| **C13** | `EqualityComparer<string>::Default().Equals("a","a")` | `true` | **control, correct** |
| L1 | `List<double>{3,NaN,1,2}.Sort()` | `[1,3,NaN,2]` | `[NaN,1,2,3]` |
| L2 | 196-shape adversarial sweep, inversions among the **finite** elements | `corrupted=164 worst=216078912` | `corrupted=0` |
| L3 | `List<double>{NaN,1,2,3}.BinarySearch(NaN)` | `-1` | `0` |
| **L4** | `…BinarySearch(2)` | `2` | **control, correct** (and see §3.3) |
| L5 | `ImmutableList<double>{3,NaN,1,2}.Sort()` | `[1,3,NaN,2]` | `[NaN,1,2,3]` |
| L6 | `ImmutableList<double>{9,3,NaN,1}.Sort(1,3)` | `[9,1,3,NaN]` | `[9,NaN,1,3]` |
| L7 | `ImmutableArray<double>{3,NaN,1,2}.Sort()` | `[1,3,NaN,2]` | `[NaN,1,2,3]` |
| **L8** | `List<int>{3,1,2}.Sort()` | `[1,2,3]` | **control, correct** |
| E1 | `List<double>{1,NaN,2}.Contains(NaN)` | `false` | `true` |
| E2 | `…IndexOf(NaN)` | `-1` | `1` |
| E3 | `List<double>{NaN,1,NaN}.LastIndexOf(NaN)` | `-1` | `2` |
| E4 | `List<double>{1,NaN,2}.Remove(NaN)` | `false`, count 3 | `true`, count 2 |
| E5 | `List<double>{1,NaN,2,NaN}.IndexOf(NaN,2,2)` | `-1` | `3` |
| E6 | `Collection<double>` `Contains`/`IndexOf`/`Remove` | `false/-1/false` | `true/1/true` |
| E7 | `ReadOnlyCollection<double>{1,NaN}.IndexOf(NaN)` | `-1` | `1` |
| E8 | `Generic::Queue<double>.Contains(NaN)` | `false` | `true` |
| E9 | `Generic::Stack<double>.Contains(NaN)` | `false` | `true` |
| E10 | `LinkedList<double>` `Contains`/`Find`/`Remove` | `false/false/false` | `true/found/true` |
| E11 | `LinkedList<double>.FindLast(NaN)` | `null` | found |
| E12 | `ImmutableArray<double>` `Contains`/`IndexOf` | `false/-1` | `true/1` |
| E13 | `ImmutableArray<double>` `Replace`/`Remove` | `[1,NaN]` / `[1,NaN]` | `[1,7]` / `[1]` |
| E14 | `ImmutableArray<double>{NaN,1,NaN}.LastIndexOf(NaN)` | `-1` | `2` |
| E15 | `ImmutableList<double>` `Contains`/`IndexOf` | `false/-1` | `true/1` |
| E16a | `ImmutableList<double>{1,NaN}.Remove(NaN)` | `[1,NaN]` | `[1]` |
| E16b | `ImmutableList<double>{1,NaN}.Replace(NaN,7)` | **throws** `ArgumentException("Cannot find the old value in the list.")` | `[1,7]` |
| E17 | `ImmutableList<double>{NaN,1,NaN}.LastIndexOf(NaN)` | `-1` | `2` |
| E18 | `ImmutableList<double>{NaN,1,2}.BinarySearch(NaN)` | `-1` | `0` |
| E19 | `ImmutableList<double>::Builder` `Contains`/`IndexOf`/`Remove` | `false/-1/false` | `true/1/true` |
| E20 | `KeyedCollection<int,double>.Contains(item)` | `false` | `true` |
| **E21** | `List<string>.Contains("b")` | `true` | **control, correct** |
| V1 | `Dictionary<int,double>.ContainsValue(NaN)` | `false` | `true` |
| V2 | `SortedDictionary<int,double>.ContainsValue(NaN)` | `false` | `true` |
| V3 | `SortedList<int,double>.ContainsValue(NaN)` | `false` | `true` |
| V4 | `SortedList<int,double>.IndexOfValue(NaN)` | `-1` | `1` |
| V5 | `Generic::OrderedDictionary<int,double>.ContainsValue(NaN)` | `false` | `true` |
| V6 | `ImmutableDictionary<int,double>.ContainsValue(NaN)` | `false` | `true` |
| V7 | `ImmutableDictionary<int,double>.Contains({0,NaN})` | `false` | `true` |
| V8 | `ImmutableSortedDictionary<int,double>.ContainsValue(NaN)` | `false` | `true` |
| V9 | `ConcurrentDictionary<int,double>.TryUpdate(0,5,NaN)` | `false` | `true` |
| **V10** | `Dictionary<int,string>.ContainsValue("x")` | `true` | **control, correct** |
| S1 | `SortedSet<double>` `Add(NaN)` twice | `true/false`, count 1 | same — **agrees by accident** |
| S2 | `SortedSet<double>{NaN,1,2}.Contains(NaN)` | `true` | `true` — **agrees, and hides S2b** |
| S2b | `SortedSet<double>` `Add(NaN); Add(1); Add(2)` | **count 1, `[NaN]`** | count 3, `[NaN,1,2]` |
| S3 | `SortedSet<double>` 1..8 then `Add(NaN)` | **count 8**, NaN dropped | count 9, `[NaN,1..8]` |
| S4 | `SortedDictionary<double,int>` single NaN key | threw on duplicate, count 1, found | same — agrees on this shape |
| S5 | `SortedList<double,int>` single NaN key | `ContainsKey` **true**, `IndexOfKey` **-1** | `true`, `0` |
| S6 | `ImmutableSortedSet<double>` `Add(NaN)` twice | count 1, contains | same — agrees on this shape |
| S7 | `ImmutableSortedDictionary<double,int>` NaN key twice | count 1, contains | same — agrees on this shape |
| S8 | `HashSet<double>` `Add(NaN)` twice | **`true/true`, count 2, `Contains` false** | `true/false`, count 1, `true` |
| S9 | `Dictionary<double,int>` `Add(NaN,..)` twice | **no throw, count 2, `ContainsKey` false** | throws, count 1, `true` |
| S10 | `ImmutableHashSet<double>` `Add(NaN)` twice | **count 2, contains false** | count 1, `true` |
| S11 | `FrozenSet<double>` from `{NaN,NaN,1}` | **count 3, contains false** | count 2, `true` |
| S12 | `FrozenDictionary<double,int>` NaN key | count 1, **`ContainsKey` false** | count 1, `true` |
| S13 | `Generic::OrderedDictionary<double,int>` NaN key twice | **count 2, `ContainsKey` false** | throws, count 1, `true` |
| S14 | `ConcurrentDictionary<double,int>` `TryAdd(NaN)` twice | **`true/true`, count 2, `TryGetValue` false** | `true/false`, count 1, `true` |
| S15 | `PriorityQueue<int,double>` NaN priority among 1,2,3 | `[1,9,2,3]` | `[9,1,2,3]` |
| **S16** | `SortedSet<int>` `{3,1,2,1}` | count 3, `[1,2,3]` | **control, correct** |
| S17 | `SortedDictionary<double,int>` keys 1,2 then `Add(NaN)` | **rejected as duplicate**, count 2, `ContainsKey(NaN)` **true** | count 3, `true` |
| S18 | `SortedList<double,int>` keys 1,2 then `Add(NaN)` | **rejected as duplicate**, count 2, `ContainsKey(NaN)` **true** | count 3, `true` |
| S19 | `ImmutableSortedSet<double>::Create({1,2,NaN})` | **count 2** | count 3 |
| S20 | `Dictionary<double,int>` keys 1,2,NaN then `Add(NaN)` again | **no throw, count 4, `ContainsKey` false** | throws, count 3, `true` |

**64 defect rows, 6 declared controls, and 4 rows (S1, S2, S4, S6, S7) where the
shipped body happens to agree with .NET on the shape probed** and disagrees the
moment a second element exists (S2b, S17, S18 are those same containers with one
more element). Those four are why a hand-picked example cannot establish that
this family is absent.

### 3.2 The sort corruption, measured

`L2` runs 196 shapes — 7 sizes (16 … 65,536) × 4 NaN densities (0.1 % … 50 %) ×
7 placements — with a fixed-seed xorshift, so the result is exactly
reproducible. It counts inversions **among the non-NaN elements only**: those
are the elements a correct sort must order no matter what NaN policy is chosen.

```
result=shapes=196 corrupted=164 worst-inversions=216078912
```

CCF-010 measured `corrupted=64 worst=3874` for `Array::Sort` over its own shape
set. `List<T>::Sort` is worse on this shape set because the set includes 50 %
NaN densities. Either way the conclusion is the one CCF-010 §6.2 reached: the
finite elements come out **unsorted**, which is a consequence of the
`[alg.sort]` precondition violation, not of any NaN-ordering choice.

### 3.3 Sanitizer matrix — what can and cannot see this

| Build | Result |
|---|---|
| **plain** | every defect row above |
| **ASan** (`-fsanitize=address -fsanitize-address-use-after-scope`) | **no report on any of the 74 cases**, including L2 |
| **UBSan** (`-fsanitize=undefined -fno-sanitize-recover=undefined`) | **no report on any of the 74 cases** |
| **`_GLIBCXX_ASSERTIONS` + `_GLIBCXX_DEBUG`** | exactly **one** abort, at **L4**: `std::lower_bound` — *"Error: elements in iterator range [first, last) are not partitioned by the value __val"* (`/usr/include/c++/14/bits/stl_algobase.h:1537`). L2 hit the 300 s watchdog under debug iterators and produced no diagnostic before it; that is a cost of debug mode, not a finding. The other 72 cases were silent. |

This is the family's central testing fact and it is why the permanent
GoogleTest suite is the primary correctness gate. **ASan, UBSan and LSan cannot
see a semantic comparison defect at all**; libstdc++ debug mode checks
irreflexivity (which NaN satisfies — `!(NaN<NaN)` is true) and *partitioning*,
but nothing about transitivity of equivalence, so it catches the
`std::lower_bound` site and none of the 63 others.

An earlier revision of the probe had an out-of-bounds write of its own in the
L2 shape generator (`pos = n/2 + k` could reach `n`), which ASan and
`_GLIBCXX_DEBUG` both reported. That was the probe's bug, not the library's; it
is fixed (`(n/2 + k) % n`) and the numbers above are from the corrected probe.
It is recorded here rather than quietly dropped, because it is the reason ASan
appears in the `.raw` history of this ticket at all.

---

## 4. Reference behaviour — current .NET

Read from `/rv/tmp/runtime/src/libraries` on 2026-07-31, and reproduced in
`docs/ComparisonContractPlan.md` §4.1, which this family does not restate:
`Comparer<T>.Default` → `x.CompareTo(y)`, which for `float`/`double` orders
**NaN below every value including negative infinity and treats two NaNs as
equal**; `EqualityComparer<T>.Default` → `x.Equals(y)`, which for
`float`/`double` is `obj == m_value || (IsNaN(obj) && IsNaN(m_value))`; and
`Single/Double.GetHashCode` folds every NaN (and both zeros) to one value.

The Collections-specific reference sites:

| Port surface | Reference | Rule |
|---|---|---|
| `List<T>.Sort()` | `List.cs` → `Array.Sort(_items,0,_size)` → `ArraySortHelper.cs:285-305` | NaN pre-pass, then introsort of the remainder |
| `List<T>.BinarySearch` | `List.cs` → `Array.BinarySearch(…, Comparer<T>.Default)` | `float.CompareTo` |
| `List<T>.Contains/IndexOf/LastIndexOf/Remove` | `List.cs` → `EqualityComparer<T>.Default` | `float.Equals` |
| `Collection<T>` / `ReadOnlyCollection<T>` | `Collection.cs` → the wrapped `IList<T>` | inherits the above |
| `KeyedCollection<K,T>.Contains(item)` | `KeyedCollection.cs` → `Collection<T>.Contains` | `EqualityComparer<T>.Default` |
| `Queue<T>.Contains` / `Stack<T>.Contains` | `Queue.cs`, `Stack.cs` | `EqualityComparer<T>.Default` |
| `LinkedList<T>.Find/FindLast/Remove/Contains` | `LinkedList.cs` — `EqualityComparer<T> c = EqualityComparer<T>.Default;` | `float.Equals` |
| `Dictionary<K,V>.ContainsValue` | `Dictionary.cs` | `EqualityComparer<TValue>.Default` |
| `Dictionary<K,V>` key lookup | `Dictionary.cs` | `EqualityComparer<TKey>.Default` + `TKey.GetHashCode` |
| `HashSet<T>` | `HashSet.cs` | `EqualityComparer<T>.Default` |
| `SortedDictionary<K,V>.ContainsValue` | `SortedDictionary.cs` | `EqualityComparer<TValue>.Default` |
| `SortedDictionary<K,V>` key ordering | `SortedDictionary.cs` | `Comparer<TKey>.Default` |
| `SortedList<K,V>.IndexOfKey` | `SortedList.cs` → `Array.BinarySearch(keys, …, comparer)` | **ordering equivalence**, not `==` |
| `SortedList<K,V>.IndexOfValue/ContainsValue` | `SortedList.cs` → `Array.IndexOf` | `EqualityComparer<TValue>.Default` |
| `SortedSet<T>` | `SortedSet.cs` | `Comparer<T>.Default` |
| `ImmutableArray<T>` / `ImmutableList<T>` | `ImmutableArray.cs`, `ImmutableList.cs` | `EqualityComparer<T>.Default`, `Comparer<T>.Default` for sort/`BinarySearch` |
| `ImmutableSortedSet<T>` / `ImmutableSortedDictionary<K,V>` | same files | `Comparer<T>.Default` unless a comparer is supplied |
| `ImmutableHashSet<T>` / `ImmutableDictionary<K,V>` | same files | `EqualityComparer<T>.Default` unless supplied |
| `ConcurrentDictionary<K,V>.TryUpdate` | `ConcurrentDictionary.cs` | `EqualityComparer<TValue>.Default` on the comparison value |
| `PriorityQueue<E,P>` | `PriorityQueue.cs` | `Comparer<TPriority>.Default` |
| `FrozenSet<T>` / `FrozenDictionary<K,V>` | `Frozen/*.cs` | `EqualityComparer<T>.Default` |

---

## 5. Root causes

### 5.1 The shared cause

Identical to CCF-010 §6.1, one module further out: a .NET expression whose
operand type carries a type-specific comparison contract is ported to the C++
built-in operator on the same operand. For every type in the port except the two
IEEE binary floating types the two agree exactly, which is why every existing
test passes.

### 5.2 The precondition-violation cause, at three depths

CCF-010 found it at one depth (a predicate handed to `std::sort`). Here it
appears at three:

1. **Algorithm call** — `List<T>::Sort`, the three `Immutable*::Sort`,
   `List<T>::BinarySearch`. Violated for the duration of one call.
2. **Container lifetime** — `std::set`/`std::map`/`std::priority_queue` with
   `std::less`/`std::greater` on a NaN-bearing element or key. Violated for as
   long as the container exists, which is why the consequence is *lost elements*
   rather than a wrong return value (S2b, S3, S17, S18, S19).
3. **The comparator object itself** — `Comparer<T>::Default()`, which any caller
   may hand to any of the above. `Compare(NaN, anything) == 0` makes the object
   an invalid comparator on its own, independently of who uses it.

### 5.3 The hash-based cause, which has no CCF-010 counterpart

`std::equal_to<double>` says `NaN != NaN` and `std::hash<double>` hashes NaN's
payload bits. So an `unordered_*` container with a NaN key or element accepts
the same key **unboundedly** and can never find it again (S8, S9, S14, S20).
Repairing equality without also normalising the hash would break the
equal-objects-equal-hashes invariant instead, so the two must move together —
`System::detail::hashValue` exists precisely for that, and CCF-010's §7.1
already states the rule.

### 5.4 The cause that must **not** be "fixed"

`ReferenceEqualityComparer` is identity, and identity is the contract. C#'s
lifted `==` on `T?` is likewise raw IEEE and CCF-010 §6.3 already pinned
`Nullable<T>::operator==` for it. `NullableEqualityComparer<T>`, by contrast, is
`EqualityComparer<T>.Default` and **is** reflexive — the same split, one class
apart. Case C10 and case C11 sit next to each other in the probe for exactly
this reason.

---

## 6. Selected repair, and the compatible/blocked split

**One policy, the one that already exists.** Every repair below uses
`System/detail/ComparisonPolicy.hpp` (ticket #1905): `compareValues`,
`equalValues`, `hashValue`, `DefaultLess`, `DefaultGreater`, `moveNaNsToFront`,
`defaultSort`. **No second policy layer is created.** `Collections.Core` already
declares `PUBLIC_DEPENDENCIES Core.Base`, so the include is an existing edge and
the module graph does not move.

Each entry point is assigned exactly one contract:

| Contract | Applies to |
|---|---|
| `EqualityComparer<T>.Default` → `detail::equalValues` (+ `detail::hashValue` where hashed) | `Contains`, `IndexOf`, `LastIndexOf`, `Remove`, `Replace`, `ContainsValue`, `IndexOfValue`, `Contains(pair)`, `TryUpdate`/`AddOrUpdate` comparison values, and every `unordered_*` key/element |
| `Comparer<T>.Default` → `detail::compareValues` / `DefaultLess` / `defaultSort` | `Sort`, `BinarySearch`, `IndexOfKey`, and every `std::set`/`std::map`/`std::priority_queue` ordering |
| **explicit caller comparer** — passed through unchanged | every `Sort(comparison)`, `Sort(comparer)`, `Remove(item, comparer)`, `Create(less)`, `WithComparer(…)` overload |
| **identity** — unchanged | `ReferenceEqualityComparer`, the `void*` sites in §2.4, non-generic `Comparer` |
| **native ordering as an internal detail only** | inside `defaultSort`, after `moveNaNsToFront` has removed NaN from the comparator's input |

### 6.1 Why the containers split in two

For a container the repair means changing the backing `std::` container's
comparator/hasher/equality **template argument** to a conditional alias:

```cpp
using KeyLess = std::conditional_t<std::is_floating_point_v<T>,
                                   System::detail::DefaultLess<T>,
                                   std::less<T>>;
```

For every **non-floating** `T` that is token-identical to today —
`std::set<T, std::less<T>>` *is* `std::set<T>` — so `sizeof`, `alignof`,
iterator types, mangled names and source compatibility are all unchanged for
every instantiation that works correctly today. For a **floating** `T` the
backing container's type genuinely changes.

That only matters where the backing type is part of the public surface.
Measured, per type:

| Type | Backing type is | Verdict |
|---|---|---|
| `ImmutableSortedSet<T>` | a `std::function` **value** | **compatible** — value change only, no type changes at all |
| `ImmutableSortedDictionary<K,V>` | a `std::function` **value** | **compatible** |
| `ImmutableHashSet<T>` | two `std::function` **values** | **compatible** |
| `SortedList<K,V>` | private `map_` | **compatible** |
| `SortedDictionary<K,V>` | private `map_`; the nested `Iterator`'s constructor takes `std::map<K,V>::const_iterator` | **compatible** (see §6.2) |
| `Generic::OrderedDictionary<K,V>` | private `keyIndex_` | **compatible** |
| `ImmutableDictionary<K,V>` | private `MapT` alias | **compatible** |
| `ConcurrentDictionary<K,V>` | private `map_` | **compatible** |
| `KeyedCollection<K,Item>` | private `keyIndex_` | **compatible** |
| `PriorityQueue<E,P>` | private `heap_` | **compatible** |
| `SortedSet<T>` | **public** `SetIterator` typedef; **public** `comparer()` returning `std::set<T>::key_compare` | **blocked** |
| `Dictionary<K,V>` | **public** `ToMap()` ×2; **public** iterator typedefs | **blocked** |
| `HashSet<T>` | **public** iterator typedefs | **blocked** |
| `FrozenSet<T>` | **public** `const_iterator`; **public** `CreateFromSet(const std::unordered_set<T>&)` | **blocked** |
| `FrozenDictionary<K,V>` | **public** `const_iterator`; **public** `CreateFromMap(…)` | **blocked** |
| `ReadOnlySet<T>` | **public** constructor taking `shared_ptr<std::unordered_set<T>>` | **blocked** |
| `ReadOnlyDictionary<K,V>` | **public** constructor; `protected getDictionaryProperty()` | **blocked** |

Ten compatible, seven blocked. #1912's row says it "requires no approval"; that
was written before this measurement existed, and it does not authorise the seven.
The seven get a complete design here and a `blocked` ticket (**#1919**) with the
exact approval text; the ten ship as **#1918**.

### 6.2 The one judgement call in the compatible half

`SortedDictionary<K,V>::Iterator` has a **public** constructor whose parameter
is `typename std::map<TKey,TValue>::const_iterator`. It is not a typedef a
consumer is expected to name — the class's own `begin()`/`end()` produce the
iterators — but it is technically a public signature mentioning the backing
type. It is counted as compatible because for a **non-floating** key nothing
changes at all, and for a floating key a consumer would have had to spell
`std::map<double,V>::const_iterator` explicitly to be affected. If that judgement
is wrong the type moves to #1919's list; it is called out here rather than
buried.

---

## 7. Implementation tickets

| Ticket | Scope | Status |
|---|---|---|
| **#1913** | this document | design only |
| **#1914** | the six named default comparers (§2.1) | compatible |
| **#1915** | the five default-ordering sites (§2.2) | compatible |
| **#1916** | the sequence equality sites (§2.3, the `List`/`Collection`/`ReadOnlyCollection`/`KeyedCollection`/`Queue`/`Stack`/`LinkedList`/`Immutable*` rows) | compatible |
| **#1917** | the associative **value**-equality sites (§2.3, the `ContainsValue`/`Contains(pair)`/`IndexOfValue`/`TryUpdate`/`AddOrUpdate` rows) | compatible |
| **#1918** | the ten compatible containers (§6.1) plus `SortedList::IndexOfKey` | compatible |
| **#1919** | the seven public-representation containers (§6.1) | **blocked**, §10 |
| **#1920** | mutation campaign, premise corrections, closure of #1912 | compatible |

Order: #1914 → #1915 → #1916 → #1917 → #1918 → #1920. #1914 is first because
the six comparers are what every other surface *claims* to be using, and because
a caller may hand `Comparer<T>::Default()` to any of the algorithms the later
tickets repair.

---

## 8. Test matrix

Permanent GoogleTest coverage lands in
`modules/collections/tests/System/Collections/CollectionsComparisonContractTests.cpp`.
Every ticket adds to it. For each repaired surface:

1. the **default** path with `float` **and** `double`;
2. the **explicit caller comparer** path, asserted to still use the caller's
   predicate and not the policy;
3. **negative controls**: the same surface with `int` and with `std::string`,
   asserted unchanged;
4. ordinary, **empty**, single-element, **duplicate-heavy**, all-NaN and
   mixed-NaN data;
5. **signed zero** (`+0.0` must equal and hash-equal `-0.0`, and must not be
   reordered relative to it) and ±Infinity;
6. `std::optional` element/key types for the two `Nullable*` comparers;
7. for the sort sites, a **deterministic adversarial vector** large enough to
   reproduce silent corruption, asserted by **inversion count** rather than by
   spot-checking positions;
8. for the mutating surfaces, **no partial mutation** when the match fails;
9. for the hashed containers, **hash/equality consistency**: two NaNs with
   different payloads must land in the same bucket *and* compare equal.

---

## 9. Source, ABI, layout and iterator consequences

For #1914–#1918:

- **Signatures** — none change. Every repair is a body change or a private
  template argument.
- **`noexcept`** — none change. `equalValues` is
  `noexcept(noexcept(a == b))`; `compareValues` and `hashValue` are used only in
  bodies that were not `noexcept`.
- **Layout / vtable** — `DefaultLess<T>`, `DefaultGreater<T>` and
  `std::less<T>` are all empty classes, so the empty-base optimisation makes the
  backing container's `sizeof` identical. #1918 asserts this by measurement for
  both instantiation families.
- **Iterator types** — unchanged for every type in #1918 (that is the criterion
  that put them there).
- **Mangled names** — every affected entity is a template in a header; no
  archive in `build/` exports a non-template symbol for any of them.
  `Collections.Core` is an `INTERFACE` target and produces no archive at all.
  #1918 records `nm --extern-only` over a fixture that instantiates the affected
  types, before and after, rather than claiming "no ABI change" from the absence
  of a non-template symbol.
- **Component graph** — unchanged at **41 modules / 91 edges**.
  `Collections.Core → Core.Base` already exists.
- **Recompilation** — every body is `inline` in a header, so a consumer must be
  fully rebuilt; the linker cannot enforce that. Same standing note as CCF-010.

---

## 10. #1919 — the exact approval required

> **Approve, for floating-point element and key types only**, changing the
> comparator / hasher / equality template argument of the backing `std::`
> container inside `SortedSet<T>`, `Dictionary<K,V>`, `HashSet<T>`,
> `FrozenSet<T>`, `FrozenDictionary<K,V>`, `ReadOnlySet<T>` and
> `ReadOnlyDictionary<K,V>`, and with it these public types:
>
> - `SortedSet<float>::SetIterator` and the return type of
>   `SortedSet<float>::comparer()`;
> - the return type of both `Dictionary<double,V>::ToMap()` overloads;
> - the public `iterator`/`const_iterator` typedefs of `Dictionary<double,V>`,
>   `HashSet<double>`, `FrozenSet<double>` and `FrozenDictionary<double,V>`;
> - the parameter type of `FrozenSet<double>::CreateFromSet` and
>   `FrozenDictionary<double,V>::CreateFromMap`;
> - the parameter type of `ReadOnlySet<double>`'s and
>   `ReadOnlyDictionary<double,V>`'s `shared_ptr` constructors, and the return
>   type of `ReadOnlyDictionary<double,V>::getDictionaryProperty()`.
>
> **No non-floating instantiation is affected in any respect** — the alias is
> token-identical to today for every other `T`. What the change buys is listed
> in §3.1 rows S1–S3, S8–S14, S17, S20: today `SortedSet<double>` silently
> discards every element added after a NaN, and `Dictionary<double,V>` accepts
> the same NaN key without limit and then answers `ContainsKey` false forever.

**Alternatives considered and rejected.**

1. *Normalise NaN on entry* — impossible: NaN has no orderable representative,
   and mapping it to a sentinel would collide with a real value.
2. *Specialise the whole class for floating `T`* — a far larger source change
   and it duplicates every member; the public types change anyway.
3. *Leave the seven alone and document the limitation* — rejected: the
   consequence is silent data loss with no diagnostic in any build (§3.3), which
   is precisely the class of defect this programme exists to remove.
4. *Route only the lookup members through the policy, leaving the container's
   own comparator alone* — rejected: it would make `Contains` and the container's
   membership disagree, which is worse than either alone.

**Rollback.** Each of the seven is a one-line alias change plus its uses;
reverting the alias restores today's behaviour exactly.

---

## 11. Recorded, not repaired, no `SR-AUD-*`

1. `ImmutableArray<T>::Replace(oldValue,newValue)` returns the array unchanged
   when `oldValue` is absent; .NET throws `ArgumentException`, and this port's
   own `ImmutableList<T>::Replace` throws. Divergence between two siblings,
   unrelated cause. Case E13 shows it.
2. `Generic::Comparer<T>::Default()` and `EqualityComparer<T>::Default()` return
   a reference to a function-local `static` — fine, but the `Default()` of
   `ObjectComparer`/`ObjectEqualityComparer` is instead a plain constructible
   type; the two spellings coexist. Style, not behaviour.
3. `NullableComparer<T>`/`NullableEqualityComparer<T>` are declared over
   `std::optional<T>` rather than over `System::Nullable<T>`. Whether the two
   should be unified is a separate question this family does not answer.

---

## 12. Nomenclature

"the family" is ticket #1912; "CCF-010" and "§18a" refer to
`docs/ComparisonContractPlan.md`. Where §7 of this document corrects §18a, the
original §18a text stays as written and the correction is additive — the same
convention CCF-010 §19 adopted.
