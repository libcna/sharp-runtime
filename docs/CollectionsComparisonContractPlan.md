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
| 5 default-ordering sites | **understated by one** | 5 reproduced as listed (§3, cases L1/L3/L5/L6/L7); `ImmutableList::BinarySearch` is a sixth, miscategorised there as an equality site — see §2.2 |
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

### 2.2 Default-ordering sites — 6, one more than #1912 counted

| Site | Entry | Algorithm |
|---|---|---|
| `Generic/List.hpp:420` | `List<T>::Sort()` | `std::sort` |
| `Generic/List.hpp:561` | `List<T>::BinarySearch(item)` | `std::lower_bound` + `*it == item` |
| `Immutable/ImmutableList.hpp:606` | `ImmutableList<T>::Sort()` | `std::sort` |
| `Immutable/ImmutableList.hpp:623` | `ImmutableList<T>::Sort(index,count)` | `std::sort` |
| `Immutable/ImmutableArray.hpp:283` | `ImmutableArray<T>::Sort()` | `std::sort` |

| `Immutable/ImmutableList.hpp:1009` | `ImmutableList<T>::BinarySearch(item)` | hand-rolled loop, `mid_val == item` + `mid_val < item` |

The last row is a **premise correction**. #1912 listed it among the *equality*
sites because its hit test was `==`, but it is an ordering site: the `==` test
was mixed with a raw `<` step, so it inherited both halves of the family. It is
repaired with `compareValues` throughout, by #1916, alongside the site that
misled the inventory.

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

---

## 13. Mutation matrix (ticket #1920)

`build-probe/1920_mutations.py` applies each mutation to the shipped headers,
rebuilds **only** the permanent contract-test translation unit
(`build-probe/1920_mutate.sh`, one TU, aggregate parallelism 1 job), runs the
suite, and reverts with `git checkout --`. Full log:
`build-probe/1920_mutation_matrix.log`.

| ID | Mutation | Expected | Result | Tests failed |
|---|---|---|---|---|
| M1 | default equality → raw `==` (`List::Contains`) | KILLED | **KILLED** | 1 |
| M2 | default ordering → raw `std::sort` (`List::Sort`) | KILLED | **KILLED** | 4 |
| M3 | remove NaN normalisation from `hashValue` | KILLED | **KILLED** | 4 |
| M4 | equality where **ordering equivalence** is required (`SortedList::IndexOfKey`) | KILLED | **KILLED** | 1 |
| M5 | **negative control** — ordering equivalence where equality is used (`List::Contains`) | SURVIVED | **SURVIVED** | 0 |
| M6 | ignore an explicit caller comparer (`ImmutableSortedSet::Create`) | KILLED | **KILLED** | 1 |
| M7 | restore `std::less` for a default ordering path (`SortedList`) | KILLED | **KILLED** | 1 |
| M8 | mismatch hash and equality (`DefaultKeyHash` → `std::hash`) | KILLED | **KILLED** | 1 |
| M9 | bypass nullable comparison (`NullableEqualityComparer::Equals`) | KILLED | **KILLED** | 2 |
| M10 | restore raw `>` for the `PriorityQueue` heap comparator | KILLED | **KILLED** | 1 |
| M11 | **negative control** — `std::stable_sort` for the NaN-free remainder | SURVIVED | **SURVIVED** | 0 |
| M12 | drop the NaN pre-pass and sort the whole range | KILLED | **KILLED** | 8 |

10 defect mutations killed, 2 declared negative controls survived, 0 surprises
in the final run.

### 13.1 What the campaign found in the *suite*

**M8 survived the first run**, and that was a real hole rather than a harmless
one. Every container test up to that point reused one NaN object, which hashes
to one bucket under either hasher, so leaving `std::hash` in place while the
equality policy stayed correct was invisible. The contract .NET actually
guarantees is stronger: `Single/Double.GetHashCode` folds **every** NaN to one
value, so a key stored under one NaN must be findable by a *different* NaN.
`HashedContainersFindANaNKeyByADifferentNaN` was added for exactly that, with an
`ASSERT_NE(std::hash<double>{}(a), std::hash<double>{}(b))` guard so the test
fails loudly if the two payloads ever stop hashing differently and the test
stops proving anything. M8 is killed by it. A companion test pins the
complement — folding NaN must not fold anything else, and `-0.0`/`+0.0` must
remain one key.

M5's first run reported `ANCHOR-BROKEN`, not a verdict: its substring matched
three sites in `List.hpp`. That is a tooling failure, not a result, and it was
fixed and re-run rather than counted.

---

## 14. Performance (ticket #1920)

`build-probe/1920_perf.cpp`, compiled `-O2` from **one** source against the
pre-change and post-change include trees (the pre-change tree is a temporary
`git worktree` at `bd7cce25` under `build-probe/1918-baseline`, repository-local
as the build policy requires). All data is produced at run time by a seeded
xorshift and consumed through a `volatile` sink, so nothing is constant-foldable.
Three alternating runs of each binary; each cell is a median of medians.
`noise` is `max/min` across the three runs of the **unchanged** binary — the
run-to-run floor for that workload on this machine.

| Workload | before ms | after ms | ratio | noise |
|---|---|---|---|---|
| sort 1M double, no NaN | 70.61 | 73.40 | **1.040** | 1.009 |
| sort 1M double, 1 % NaN | 64.88 | 65.96 | 1.017 | 1.056 |
| sort 1M double, 50 % NaN | 31.67 | 32.37 | 1.022 | 1.088 |
| sort 1M double, duplicate-heavy | 26.97 | 27.12 | 1.006 | 1.049 |
| sort 1M int (negative control) | 46.09 | 45.70 | 0.992 | 1.019 |
| sequential equality search, 1k misses over 1M | 264.10 | 261.25 | 0.989 | 1.026 |
| binary search, 100k hits | 4.29 | 5.02 | **1.169** | 1.114 |
| `SortedDictionary<double,int>` 200k insert+lookup | 107.38 | 114.96 | 1.071 | 1.105 |
| `OrderedDictionary<double,int>` 200k insert+lookup | 26.34 | 26.20 | 0.995 | 1.367 |
| `Dictionary::ContainsValue` ×200 over 200k | 35.40 | 37.09 | 1.048 | 1.049 |

Eight of the ten ratios are at or inside their own noise floor. Two are outside
it and are stated rather than absorbed:

- **sort, no NaN — 1.040.** The NaN pre-pass is an extra O(n) pass over a
  million doubles that finds nothing. .NET pays the same pass for the same
  reason, and CCF-010 measured 1.011× for `Array::Sort` over its own shape set.
  Accepted.
- **binary search — 1.169.** `DefaultLess` plus `equalValues` against raw `<`
  and `==`, on a 4.3 ms workload. Small in absolute terms, above noise, and the
  price of a search that agrees with the sort that produced the range.

### 14.1 Three regressions found and eliminated, not reported

The first paired run measured **1.80×** on the sequential equality search,
**1.45×** on `OrderedDictionary` insert+lookup and **1.16×** on
`SortedDictionary`. Both causes were mechanical, and both fixes are in
`ComparisonPolicy.hpp`:

1. **The needle's NaN-ness is loop-invariant.** Writing
   `std::find_if(…, [&]{ return equalValues(v, value); })` evaluates an `isnan`
   pair for **every element**, defeating vectorisation, even though it can only
   matter when `value` itself is NaN. `detail::findValue` tests the needle once
   and falls back to a plain `std::find` — which for a non-NaN needle *is*
   `equalValues`, exactly. 1.80× → 1.00×. Nineteen scan sites use it.
2. **`noexcept` on the hasher is load-bearing.** libstdc++'s `_Hashtable` turns
   on per-node hash-code caching unless the hasher is both "fast" and
   non-throwing, which grows every node by a word. `DefaultHash::operator()` and
   `DefaultEqualTo::operator()` are now `noexcept` — honestly so, since
   `std::hash` over an arithmetic type and `std::isnan` cannot throw. 1.45× →
   0.99×.
3. `DefaultLess::operator()` was rewritten to reach a verdict in **one**
   comparison for two ordinary numbers, evaluating the NaN tests only when
   neither ordering nor equality holds. 1.16× → within noise.

---

## 15. Closure

**Delivered:** #1913 (this plan), #1914 (six named default comparers), #1915
(six default-ordering sites), #1916 (38 sequence equality sites), #1917 (16
associative value-equality sites), #1918 (ten containers), #1920 (this section).

**Not delivered, blocked:** #1919 — `SortedSet`, `Dictionary`, `HashSet`,
`FrozenSet`, `FrozenDictionary`, `ReadOnlySet`, `ReadOnlyDictionary`. Probe rows
S1, S2, S2b, S3, S8, S9, S11, S12, S20 still return the wrong answer, and
`SortedSet<double>` still silently discards every element added after a NaN.
The approval text is §10. Nothing about this half is undecided except the
approval.

**Site counts, final and measured**

| Population | #1912 said | Measured | Repaired |
|---|---|---|---|
| named default comparers | not named | 6 | 6 |
| default-ordering sites | 5 | **6** (`ImmutableList::BinarySearch` was miscategorised as an equality site) | 6 |
| default-equality sites | 56 across 20 headers | **55 defect-capable across 16 headers**, plus 11 shaped-but-not-capable and 4 caller-supplied | 54 (38 in #1916, 16 in #1917; the 55th is `SortedList::IndexOfKey`, repaired as an ordering site in #1918) |
| ordered containers | 3 | **6** (incl. `ImmutableSortedSet`, `ImmutableSortedDictionary`, `PriorityQueue`) | 5 of 6 (`SortedSet` blocked) |
| hashed containers | not named | 11 | 6 of 11 (5 blocked) |

**Audit numbering:** no `SR-AUD-*` identifier was issued by any ticket in this
family. It stays frozen at **364**.

---

## 16. #1919 as built — corrections to §6.1, §9 and §10 (ticket #1924)

#1919 was approved on 2026-07-31 in the exact words of §10 and implemented as
**#1921** (SortedSet), **#1922** (Dictionary, HashSet), **#1923** (FrozenSet,
FrozenDictionary, ReadOnlySet, ReadOnlyDictionary) and **#1924** (this
section). §6.1, §9 and §10 above are preserved as written; the corrections are
additive, per §12.

### 16.1 `SortedSet` is representation-**private**, not public

§6.1 and §10 list `SortedSet<T>::SetIterator` and `SortedSet<T>::comparer()`
as **public**. Measured at the HEAD #1919 was implemented against, both are
**private** — they are declared above the class's `public:` label. The only
public declaration mentioning the backing type at all is
`SortedSet<T>::Iterator`'s constructor, whose other parameter is the private
nested `State`, so no consumer can call it or name its parameter types.

`SortedSet` therefore belonged with the ten compatible containers of #1918.
It was implemented under #1919's approval regardless, which names it
explicitly. **Why the original search missed it:** §6.1's table was built by
grepping each header for a mention of the backing `std::` type and recording
the enclosing declaration, without checking which access-specifier region that
declaration fell in. Six of the seven rows were right; this one was not.

**The blocked/compatible split is therefore 11 compatible and 6 blocked, not
10 and 7.**

### 16.2 The public **iterator** typedefs of `double` and `float` did **not** change

§10 approved changing "the public `iterator`/`const_iterator` typedefs of
`Dictionary<double,V>`, `HashSet<double>`, `FrozenSet<double>` and
`FrozenDictionary<double,V>`". Measured, **they do not change for `double` or
`float`.** libstdc++'s `_Node_iterator<Value, ConstantIterators, CacheHashCode>`
does not mention the hasher, and both bools are unchanged for those two types.

They **do** change for `long double`, and only there, for a reason worth
recording: libstdc++ specialises `__is_fast_hash<std::hash<long double>>` to
`false`, so the node cached its hash code; `System::detail::DefaultHash<long
double>` is a new type for which the primary template says `true`, so the cache
is switched off. Measured in the symbol inventory:

| | `_Hashtable_traits<CacheHashCode, …>` before | after |
|---|---|---|
| `unordered_set<long double>` | `<true, true, true>` | `<false, true, true>` |
| `unordered_set<double>` | `<false, true, true>` | `<false, true, true>` (unchanged) |

Both directions are pinned by baseline `static_assert`s in
`test/consumer/collections_floating_comparer_negative.cpp`. The performance
consequence is §19.

### 16.3 The measured public surface that **did** change

Six types, floating element/key only:

| Type | What moved |
|---|---|
| `Dictionary<K,V>` | both `ToMap()` overloads' return type; `iterator`/`const_iterator` **for `long double` only** |
| `HashSet<T>` | `iterator`/`const_iterator` **for `long double` only** |
| `FrozenSet<T>` | `CreateFromSet`'s parameter; `const_iterator` **for `long double` only** |
| `FrozenDictionary<K,V>` | `CreateFromMap`'s parameter; `const_iterator` **for `long double` only** |
| `ReadOnlySet<T>` | the `shared_ptr` constructor's parameter |
| `ReadOnlyDictionary<K,V>` | the `shared_ptr` constructor's parameter; `getDictionaryProperty()` (protected) |

Each of the six publishes a `SetType`/`MapType` alias naming its own backing
type. `SortedSet` publishes nothing (§16.1) and keeps a private `BackingSet`.
The migration note is `docs/Migration-CollectionsFloatingComparers.md`.

### 16.4 Representation, measured

`build-probe/1919_{prefix,postfix}_layout.log`, one fixture, 57 `sizeof`/
`alignof` measurements over `double`, `float`, `long double`, `int`,
`long long`, `std::string`, a user-defined comparable-and-hashable type,
`std::optional<double>` and `std::pair<int,double>`:

- **0 of 57 `sizeof` or `alignof` values changed**, for either instantiation
  family. The comparators are empty classes and the empty-base optimisation
  absorbs them.
- Standard-layout and trivially-copyable properties unchanged for all.
- The only type-name movement is the `long double` node/iterator flip of §16.2.

External symbols over the same fixture (`nm --extern-only --defined-only`,
demangled): **1,758 → 1,757**.

| | count | what |
|---|---|---|
| removed | 106 | 97 genuine predicate moves + 8 `long double` typeinfo names + 1 weak COMDAT |
| added | 105 | 97 genuine predicate moves + 8 `long double` typeinfo names |

**No symbol moved for any non-floating instantiation.** The unpaired removal is
`std::_Hashtable<std::optional<double>, …>::find(…)` — a **weak COMDAT** symbol
GCC previously emitted out of line and now inlines away. `std::optional<double>`
is not a floating-point type and its container did not change; this is a
codegen artefact of the same translation unit, the same shape #1918 recorded
for `std::priority_queue::pop`. It is stated here rather than papered over with
"no ABI change".

---

## 17. Recorded, not repaired — the composite-key gap (ticket #1925)

`std::is_floating_point_v` is the policy's selector, so the contract does not
reach **inside** a composite type. Measured (probe row `D10`):

```
Dictionary<std::optional<double>,int> d;
d.Add(std::optional<double>(NaN), 1);
d.TryGetValue(std::optional<double>(NaN), v);   // FALSE
```

The key is unfindable **by the very object that was inserted**, because
`std::equal_to<std::optional<double>>` is `optional::operator==`, which compares
the contained doubles with raw `==`. .NET's `Dictionary<double?,V>` finds it:
`EqualityComparer<double?>.Default` is `Nullable<T>.Equals` → `Double.Equals`,
which is NaN-reflexive. The same applies to `std::pair`, `std::tuple` and any
user type holding a floating member; the ordered case is the milder
non-strict-weak-ordering defect one nesting level down.

This is **outside #1919's approval**, which covers floating-point element and
key types only — repairing it means making the policy recurse and would move a
further family of public template types. It is ticket **#1925**, and today's
behaviour is pinned by
`CollectionsComparisonContract.NullableFloatingKeysKeepRawIeeeEqualityForNow`
so it cannot change silently in either direction.

---

## 18. Mutation matrix — #1919 (ticket #1924)

`build-probe/1919_mutations.py`, reusing #1920's harness
(`build-probe/1920_mutate.sh`) unchanged: one mutation, one compile of the
permanent contract-test translation unit, aggregate parallelism **1 job**. Log:
`build-probe/1919_mutation_matrix.log`.

Three verdicts, not two. **REJECTED** means the mutation does not *compile*
against the permanent suite, because the suite's cross-container
`static_assert`s — Dictionary, HashSet, FrozenSet, FrozenDictionary,
ReadOnlySet and ReadOnlyDictionary must all key on the **same** predicates —
contradict it. That is stronger than KILLED, not a harness error.

| ID | Mutation | Expected | Result | Tests failed |
|---|---|---|---|---|
| N1 | restore `std::less` for the `SortedSet` tree | KILLED | **KILLED** | 6 |
| N2 | `Dictionary`: policy hash, native `equal_to` | REJECTED | **REJECTED** | — |
| N3 | `Dictionary`: native `std::hash`, policy equality | REJECTED | **REJECTED** | — |
| N4 | `HashSet`: restore both native predicates | REJECTED | **REJECTED** | — |
| N5 | fail to propagate into `FrozenSet` | REJECTED | **REJECTED** | — |
| N6 | fail to propagate into `FrozenDictionary` | REJECTED | **REJECTED** | — |
| N7 | fail to propagate into `ReadOnlySet` | KILLED | **KILLED** | 2 |
| N8 | fail to propagate into `ReadOnlyDictionary` | REJECTED | **REJECTED** | — |
| N9 | make NaN ordering-equivalent to every finite value | KILLED | **KILLED** | 12 |
| N10 | compare NaN **payload bits** instead of .NET equality | KILLED | **KILLED** | 18 |
| N11 | bypass the policy on **removal** | KILLED | **KILLED** | 2 |
| N12 | bypass the policy on **duplicate detection** | KILLED | **KILLED** | 2 |
| N13 | bypass the policy on the **lookup** path | KILLED | **KILLED** | 5 |
| N14 | bypass the policy in **constructor population** | KILLED | **KILLED** | 1 |
| N15 | bypass the policy in **one public factory** (`FrozenDictionary::Create`) | KILLED | **KILLED** | 1 |
| N16 | **control** — spell the same comparator longhand | SURVIVED | **SURVIVED** | 0 |
| N17 | **control** — pre-reserve on every `HashSet::Add` | SURVIVED | **SURVIVED** | 0 |
| N18 | **equivalent** — rebuild through a raw map after rehash | SURVIVED | **SURVIVED** | 0 |
| N19 | **equivalent** — `FrozenSet::Create` folds through a raw set | SURVIVED | **SURVIVED** | 0 |

9 killed, 6 rejected at compile time, 2 declared controls and 2 declared
equivalents survived, **0 surprises in the final run**.

### 18.1 What the campaign found, and the two mutations that were wrong

**N18 and N19 were written expecting a kill and are provably equivalent.** That
is the campaign's most useful result, and both are kept and relabelled rather
than deleted:

- **N18** ("bypass after rehash") rebuilds `Dictionary`'s map through a
  `std::hash`-keyed temporary inside `TrimExcess`. It cannot change anything,
  because the map it rebuilds *from* has already folded every equal key on
  insert and the map it rebuilds *into* folds again. **A "bypass after rehash"
  is structurally unreachable**: the hasher is a property of the container
  *type*, not a decision taken per call site. That is a property of the chosen
  repair, and it is why no per-site audit of the hashed containers is needed.
- **N19** ("bypass one factory") makes `FrozenSet::Create` fold through a raw
  set first. The raw set keeps *more* elements than the policy set and the copy
  folds them again, so the final set is identical. A set has no values, so there
  is no last-one-wins rule to disturb — which is exactly what makes the
  `FrozenDictionary` version (N15) a real defect, where the raw temporary both
  loses the deterministic last-value-wins rule for two equal NaN keys and makes
  the surviving value depend on an unspecified iteration order. N15 is killed.

The prompt for this batch asked for a mutation that "bypasses it after rehash".
It exists (N18), it survives, and the reason it survives is a **result**, not a
gap: reporting it as a killed mutation would have been false.

### 18.2 Sanitizers

`build-probe/1919_run.sh {asan,ubsan}` over all 46 probe cases:
**zero diagnostics from AddressSanitizer, LeakSanitizer and
UndefinedBehaviorSanitizer**, and every case returned the same answer as the
plain build. This restates §3.3's central fact for the seven containers: **the
sanitizers cannot see a semantic comparison defect at all.** The permanent
GoogleTest suite and this mutation matrix are the correctness gate; the
sanitizer runs prove only that the repair introduced no memory or
undefined-behaviour defect of its own.

---

## 19. Performance — #1919 (ticket #1924)

`build-probe/1919_perf.cpp`, compiled `-O2` from **one** source against the
pre-change tree (a repository-local `git worktree` at `1369fcda` under
`build-probe/1919-baseline`) and the post-change tree. All data is produced at
run time by a seeded xorshift and consumed through a `volatile` sink. **Seven**
alternating runs of each binary; each cell is a median of medians. `noise` is
`max/min` across the seven runs of that binary. 200,000 elements per workload.
Summary: `build-probe/1919_perf_summary.log`.

| Workload | before ms | after ms | ratio | noise |
|---|---|---|---|---|
| `SortedSet<double>` insert, no NaN | 43.30 | 45.07 | 1.041 | 1.102 |
| `SortedSet<double>` insert, duplicate-heavy | 5.22 | 5.28 | 1.010 | 1.075 |
| `SortedSet<double>` lookup, 200k hits | 35.67 | 37.61 | 1.055 | 1.451 |
| `SortedSet<int>` insert (control) | 31.03 | 28.07 | 0.905 | 1.214 |
| `Dictionary<double,int>` insert | 22.36 | 20.46 | 0.915 | 1.367 |
| `Dictionary<double,int>` insert, duplicate-heavy | 2.56 | 2.60 | 1.014 | 1.151 |
| `Dictionary<double,int>` lookup, 200k hits | 3.47 | 3.54 | 1.018 | 1.418 |
| **`Dictionary<long double,int>` insert** | **52.41** | **68.12** | **1.300** | 1.206 |
| `Dictionary<long double,int>` lookup | 29.56 | 23.38 | 0.791 | 1.485 |
| `Dictionary<int,int>` insert (control) | 12.89 | 13.12 | 1.018 | 1.108 |
| `Dictionary<string,int>` insert (control) | 25.93 | 29.25 | 1.128 | 1.749 |
| `HashSet<double>` insert | 17.41 | 18.06 | 1.037 | 1.491 |
| `HashSet<double>` lookup, 200k hits | 3.10 | 3.20 | 1.032 | 1.634 |
| `HashSet<int>` insert (control) | 13.00 | 13.18 | 1.014 | 1.249 |
| `FrozenSet<double>` lookup | 3.89 | 4.02 | 1.034 | 1.165 |
| `FrozenDictionary<double,int>` lookup | 4.16 | 4.25 | 1.021 | 1.186 |
| `FrozenSet<int>` lookup (control) | 1.66 | 1.66 | 1.004 | 1.351 |
| `ReadOnlySet<double>` lookup | 3.35 | 3.20 | 0.954 | 1.659 |
| `ReadOnlyDictionary<double,int>` lookup | 3.62 | 3.47 | 0.966 | 1.258 |

**Every control is inside its own noise floor**, and so is every `double` and
`float` workload. One row is outside it and is stated rather than absorbed.

### 19.1 The one genuine regression: `long double` hash insert, 1.300×

Cause, measured rather than guessed (§16.2): libstdc++ specialises
`__is_fast_hash<std::hash<long double>>` to `false`, so an
`unordered_map<long double, …>` node cached its hash code and a rehash never
recomputed it. `DefaultHash<long double>` is a new type for which the primary
template says `true`, so the cache is switched off and every rehash recomputes
200,000 `long double` hashes.

Accepted, for three reasons: the cost falls only on `long double` hashed
insertion, the rarest of the three floating types in ported game code; the
**lookup** side of the same container measured *faster* (0.791×, inside noise),
because the node lost a word and the table got denser; and the alternative —
specialising `std::__is_fast_hash`, a reserved libstdc++ internal, behind
`#ifdef __GLIBCXX__` — buys a 1.3× on one workload at the price of a
non-portable specialisation of a `std::` template this project does not own.
Recorded as ticket **#1926** so the decision is durable rather than lost.

### 19.2 Three ratios that are not comparisons

Three workloads cannot be expressed as a ratio, because the **before** binary
was not doing the same work:

| Workload | before ms | after ms | why |
|---|---|---|---|
| `SortedSet<double>` insert, 1 % NaN | 0.46 | 44.75 | before, every element after the first NaN was rejected as a duplicate — the set held **two** elements, not 200,000 |
| `SortedSet<double>` insert, 50 % NaN | 0.46 | 23.65 | same |
| `HashSet<double>` insert, 50 % NaN | **16,953.59** | **8.83** | before, 100,000 NaNs hashed to one bucket and none compared equal, giving a 100,000-long collision chain and quadratic insertion |

The third is worth stating plainly: on this shape the shipped code took
**17 seconds** where the repaired code takes **8.8 ms**, a factor of ~1,900.
That is not a performance improvement claimed for the repair — it is the
removal of a quadratic blow-up that any NaN-bearing input could trigger. The
first two are the cost of *keeping* 200,000 elements instead of silently
discarding 199,998 of them.

---

## 20. Closure of #1919 and of the #1912 family

**Delivered by this batch:** #1921, #1922, #1923, #1924 — the whole of #1919.

Every probe row §3.1 listed as still wrong at §15's closure now returns the
.NET answer: S1, S2, S2b, S3, S8, S9, S11, S12, S20, plus every case the #1919
probe added (46 cases, `build-probe/1919_{prefix,postfix}_plain.log`). S16 and
every other declared control is unchanged.

**Final container tally, superseding §15's:**

| Population | Measured | Repaired |
|---|---|---|
| ordered containers | 6 | **6 of 6** (`SortedSet` was the last) |
| hashed containers | 11 | **11 of 11** |
| compatible / public-representation split | **11 / 6** (§16.1) | both halves shipped |

**Not repaired, ticketed:** #1925 (composite/nullable floating keys, §17) and
#1926 (`long double` hash-code caching, §19.1). Neither is a member of #1912's
population; both were discovered while implementing #1919 and are recorded
rather than folded in.

**Audit numbering:** no `SR-AUD-*` identifier was issued by any ticket in this
family, including #1919's four. It stays frozen at **364**.

---

## 20. #1926 isolated — the mechanism is proven, the number replicates, the *other* half does not (2026-07-31)

*Measured by the Group E subset batch. **Evidence only** — no production change
was made, no `std::` internal was specialised outside a throwaway probe, and
**#1926 stays `todo` and unapproved.** §19.1 is preserved verbatim; §20 is what
independent measurement adds and where it disagrees.*

### 20.1 Why this needed isolating at all

§19.1 attributes the 1.300× `long double` insert regression to libstdc++'s
per-node hash-code cache being switched off, and reads that attribution **out of
the header**. It was never isolated: the #1919 benchmark compares a pre-change
tree with a post-change tree, so *every* difference between them is in the
measurement, and "the hasher's `__is_fast_hash` answer" is only the most likely
of several.

`build-probe/1926_fasthash_probe.cpp` isolates it. Three hashers, one process,
one `std::unordered_map<long double,int>`, alternated within each round:

| | Hasher | `__is_fast_hash` | Cache | Meaning |
|---|---|---|---|---|
| **A** | `std::hash<long double>` | `false` (libstdc++ specialises it) | **on** | the pre-#1919 shape |
| **B** | `System::detail::DefaultHash<long double>` | `true` (primary template) | **off** | today |
| **C** | a probe-local hasher computing **exactly B's value**, with `std::__is_fast_hash` specialised to `false` | `false` | **on** | what the proposed fix would do |

If the attribution is right, A and C agree and B alone is slow. Specialising a
reserved `std::` internal is the very thing #1926 asks the user to decide about;
it is done **in the probe only**, so the decision can be taken with the number in
hand.

### 20.2 The mechanism is confirmed exactly

```
  A std::hash<long double>            __is_fast_hash=false  node_size=64
  B detail::DefaultHash<long double>  __is_fast_hash=true   node_size=48
  C same + __is_fast_hash=false       __is_fast_hash=false  node_size=64

  A iterator: _Node_iterator<pair<const long double,int>, false, true >
  B iterator: _Node_iterator<pair<const long double,int>, false, false>   <-- differs
  C iterator: _Node_iterator<pair<const long double,int>, false, true >   <-- back to A
```

Three things follow, and all three are measured, not argued:

1. **The named mechanism is the whole cause.** Restoring only the trait, changing
   nothing else, restores A's node size *and* A's iterator type.
2. **The specialisation really would undo #1919's only iterator-type movement** —
   §19.1 claimed this; here it is, in `typeid`.
3. **Corrected premise: the node loses 16 bytes, not "a word".** §19.1 and the
   ticket both say the node "loses a word". Measured, 64 → **48**: the cached
   `size_t` costs 16 bytes of node because `long double` is 16-byte aligned on
   x86-64. So today's shape is **25 % smaller per node** — the trade is speed for
   memory, and the memory side was understated by half.

### 20.3 The insert regression replicates; the lookup improvement does not

25 alternating rounds, 200,000 keys (`build-probe/1926_fasthash_25.log`,
`1926_fasthash_summary.log`):

| Workload | median B/A | min B/A | median C/A | rounds B slower than A |
|---|---|---|---|---|
| **insert** | **1.319×** | 1.571× | 0.895× | **24 of 25** |
| lookup | 1.210× | 1.006× | 1.107× | 17 of 25 |

- **Insert: real, stable, and it reproduces §19.1's 1.300× almost exactly** — an
  independent harness on an independent workload lands at 1.319×, and B is slower
  in 24 of 25 rounds. C tracks A (0.895× median, 1.018× on the least
  noise-sensitive min-of-rounds statistic), i.e. **the fix would recover it in
  full**.
- **Lookup: §19.1's "the same container's lookup got *faster* (0.791×)" does not
  replicate.** A 9-round run of this probe reproduced it (0.772×); the 25-round
  run reversed it (1.210×). Over both, lookup is **within noise** and should not
  be cited in either direction. The plausible cause of the original reading — a
  denser table from the smaller node — is real, but it is not worth a decimal
  place.

**The noise floor on this machine is enormous** — max/min within a single
series reached 3.0–6.3× across runs — which is why only the median over ≥25
rounds and the sign-test (24 of 25) carry any weight here, and why a
single-round comparison of any of these numbers is meaningless.

### 20.4 The finding that matters most: it is toolchain-specific

`__is_fast_hash`, `_Hash_node<T, cache>` and the whole per-node hash-cache
mechanism are **libstdc++ implementation details**. libc++ and the MSVC STL have
no such trait and make no such choice, so on those standard libraries **neither
the regression nor the proposed fix exists**. The repository's tracked CI is
Ubuntu/GCC only, so the measurement above describes the only configuration this
project currently tests — and a specialisation guarded by `#ifdef __GLIBCXX__`
would be dead code everywhere else.

That is a complete and valid answer to #1926's question in its own right: the
regression is **real, reproducible, fully explained, recoverable in full, and
confined to one standard library**.

### 20.5 What is still a decision, and what is not

**Not a decision any more** (measured here):

- whether the 1.300× is real — **yes**, 1.319× median, 24 of 25 rounds;
- whether `__is_fast_hash` is the cause — **yes**, proven by C;
- whether the fix would recover it — **yes**, in full, plus the iterator type;
- whether the lookup number should be cited — **no**, it is noise;
- whether it affects `float`/`double` — **no**, unchanged (§19.1, unrevised).

**Still a decision, and still the user's:** may this project specialise
`std::__is_fast_hash`, a **reserved libstdc++ internal it does not own**, behind
`#ifdef __GLIBCXX__`? Specialising a `std::` template not designated for user
specialisation is undefined by the letter of the standard. The measurement does
not settle that; it only prices it. **Recommendation unchanged from §19.1 and the
decision packet: defer, leaning `wontfix`** — 1.3× on insertion, on the rarest of
the three floating types, on one standard library, is a thin return for a
non-portable specialisation of a reserved name. The one argument on the other
side is now firmer than it was: it would also restore the pre-#1919 iterator
type, which is measured, not asserted.

### 20.6 Approved disposition — close #1926 `wontfix` (2026-08-01)

The user accepted the recommendation in §§19.1 and 20.5. Ticket #1926 is
closed `wontfix` for the measured GCC 14.2.0 / libstdc++ 14 x86-64
configuration. The reproducible result remains important evidence: for
`Dictionary<long double, int>` insertion over 200,000 keys, 25 alternating
rounds measured the current `DefaultHash<long double>` shape at **1.319×** the
pre-#1919 `std::hash<long double>` shape, slower in **24 of 25** rounds. The
previously reported lookup improvement is withdrawn as noise.

This disposition deliberately retains the CCF-010 correctness repair. NaN
keys remain findable and non-duplicating under the default dictionary policy;
NaN and signed-zero hashes remain canonical. It does not authorize a second
comparator/hasher framework, a container-representation change, or a
specialization of reserved `std::`/libstdc++ machinery. Restoring the old
mechanism would either restore the incorrect floating-key semantics or rely
on `std::__is_fast_hash`, an implementation detail that is not a portable
public customization point and that also changes the measured node/iterator
representation.

Reopen #1926 only with new evidence that materially changes that tradeoff: a
relevant standard-library update, a portable public customization point, a
stable behavior- and representation-preserving optimization, or measurements
on another supported compiler/standard-library configuration. Retain
`build-probe/1926_fasthash_probe.cpp`, `1926_fasthash.log`,
`1926_fasthash_25.log`, and `1926_fasthash_summary.log`; do not reinterpret
their toolchain-specific result as a cross-library regression.

No `SR-AUD-*` identifier was issued; numbering stays frozen at **364**.
