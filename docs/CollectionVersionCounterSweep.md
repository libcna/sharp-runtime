<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Repository-wide collection mutation-counter sweep

*Design, evidence, and implementation record for ticket **#1787**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M). Recorded 2026-07-28
on local branch `feature/remediation-coll-version-counter-sweep`. This ticket
carries **no `SR-AUD-*` identifier** — the audit numbering is frozen at 364 and
this pattern was found during remediation, by ticket #1786's own required
inventory (`docs/SortedSetVersioningDesign.md` §16). It reopens no audit
finding: SR-AUD-361 stays `remediated`, and #1783, #1784, and #1786 stay done
and untouched. The counter, its type, and its increment all arrived with ticket
1713's fail-fast enumerator work.*

---

> **Follow-up landed — ticket #1788, 2026-07-29.** One of the two approval-blocked
> residuals below is **closed**. `LinkedList<T>` now carries the 64-bit
> `MutationCounter`, its enumerator the 64-bit `MutationVersion`, and
> `sizeof(LinkedList<T>)` grew **40 → 48** on LP64 under the explicit user approval
> §8.1 asked for. Everything §§1–17 record about the *state on the day #1787
> landed* is preserved unedited and is still accurate as history — this document
> deliberately does **not** pretend `LinkedList<T>` was always 64-bit. The
> implementation record, with re-measured layout, symbols, a stale-object probe and
> performance, is **§19**.
>
> **Follow-up landed — ticket #1789, 2026-07-29.** The **other** residual is now
> closed too. `BitArray` carries the 64-bit `MutationCounter` and its **public**
> nested `Enumerator` the 64-bit `MutationVersion`; `sizeof(BitArray::Enumerator)`
> grew **32 → 40** on LP64 under the explicit user approval §8.2 asked for, and
> `sizeof(BitArray)` stayed **48**. The implementation record is **§20**. With it,
> **no collection in this repository retains a 2^32 enumerator-snapshot ABA
> horizon** — every one is 2^64, and `detail::NarrowMutationCounter` has no user
> left. §§1–17 are again preserved unedited as the record of the day #1787 landed.

---

## 1. Executive summary

Every mutation counter in `modules/collections/include/` now uses the new
`System::Collections::detail::BasicMutationCounter`, whose increment is
**unsigned** (defined for every representable prior value) and whose **assignment
advances the destination instead of taking the source's value**. Thirteen
collections take the 64-bit `MutationCounter`; `LinkedList<T>` and `BitArray`
take the 32-bit `NarrowMutationCounter` because widening them would grow a
public object, which needs approval this repository has not been given.

Three defect classes were found and reproduced against the committed pre-fix
headers before anything changed. **Ticket #1786's inventory was also
incomplete and its arithmetic claim slightly wrong**, and both corrections are
recorded here rather than absorbed:

| # | Defect | Pre-fix reach | Types affected | Status |
|---|---|---|---|---|
| 1 | `++version_` at `INTCS_MAX` is signed-integer overflow — **undefined behaviour** | 2^31 − 1 mutations | **14** (all but `BitArray`) | **eliminated for all 14** |
| 2 | A wrapped counter silently revalidates a stale enumerator/iterator (ABA) | 2^32 mutations | **15** | **eliminated for 13**; 2 blocked on approval |
| 3 | Copy/move **assignment transplants the source's counter**, so an enumerator over the destination survives having every element destroyed | **no overflow needed at all** | **14** (all but `LinkedList<T>`) | **eliminated for all 14** |

Defect 3 was **not** in #1787's description, was **not** in #1786's inventory,
and is by far the most serious of the three. It needs no overflow: the two
counters merely have to be equal, which two collections that have taken the
same number of effective mutations routinely are. Six of the fourteen reproduce
as **AddressSanitizer heap-use-after-free or heap-buffer-overflow** errors
rather than merely as wrong answers (§4.3).

Corrections to #1786's §16 inventory:

1. It listed **fifteen** counter-carrying types. There are **sixteen**:
   `BitArray` was missed. It is the one type whose counter was already
   `std::uint32_t`, so it never had defect 1 — but it had defects 2 and 3.
2. It asserted that "all fifteen declare it `intcs`". Fourteen do; `BitArray`
   does not.
3. It stated that defects 3 and 4 (a stale cached Count and a colliding
   sentinel) are specific to `SortedSet<T>`. That is **confirmed** here with
   evidence (§10), not merely repeated.

Measured cost: **none.** Every benchmarked path is within run-to-run noise
(§15). Every affected container's and enumerator's `sizeof`, `alignof`, and
counter offset is unchanged (§12).

---

## 2. Ticket and scope

Ticket #1787, key `REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, priority **P3**,
size **M**, category `defect`, area `Collections`, source path
`modules/collections/include/System/Collections/`.

The objective was explicitly *not* to replace every `intcs version` with
`uint64_t`. Each type was assessed against ten questions (reachability of
overflow, UB, iterator ABA, sentinel collision, cache false-hit, copy/assignment
effects, object-layout impact, iterator-layout impact, header-only versus
symbol-emitting, and whether a compatible correction exists) and then classified
A/B/C/D. Two types genuinely landed in a different category from the other
thirteen, so a uniform mechanical replacement would have been wrong — which is
the outcome the ticket's own instruction anticipated.

`SortedSet<T>` is out of scope and untouched: #1786 already repaired it, and it
is structurally different (§10.3). Ticket #1785 (nested-view exception ordering)
is untouched and stays `todo`. Ticket #1773 stays `blocked`; CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified.

---

## 3. Complete inventory

Derived independently rather than copied from #1786. The search covered the
whole repository for `version`, `version_`, `_version`, `cachedVersion`,
`countVersion`, `enumeratorVersion`, `++version`, `version++`, equality-based
invalidation, `-1` sentinels, `InvalidOperationException`, and every collection
enumerator and iterator.

**Sixteen** types carry a mutation counter, all of them in
`modules/collections/include/`. Nothing outside that directory does: the other
`version`-shaped names in the repository are `System::Version`,
`OperatingSystem`/`ApplicationId` version numbers, HTTP/`Via`/product header
version strings, `XDocument`'s XML-declaration version, and
`Globalization::SortVersion`'s Unicode sort version — none is a mutation
counter, none is incremented, and none is compared for enumerator invalidation.

### 3.1 The measured table

`sizeof`/`alignof`/offset figures from `build-probe-collversion/probe1_layout_inventory.cpp`
(`probe1_prefix_layout.log` → `probe1_postfix_layout.log`), LP64, GCC 14,
libstdc++. "ver-off" is the counter's byte offset; sizes are for the `<int>` /
`<int,int>` instantiation.

| Type | Module | Header | Counter, pre-fix | Counter, post-fix | Enumerator / iterator | Snapshot, pre → post | Increment sites | Read/compare sites | sizeof coll. pre → post | sizeof enum. pre → post | ver-off pre → post | Category |
|---|---|---|---|---|---|---|---:|---:|---|---|---|---|
| `List<T>` | Collections.Core | `Generic/List.hpp` | `intcs` | `MutationCounter` (u64) | private `Enumerator` (`IEnumerator<T>*`) | `intcs` → `MutationVersion` | 14 | 3 | 40 → **40** | 32 → **32** | 32 → **32** | **A** |
| `HashSet<T>` | Collections.Core | `Generic/HashSet.hpp` | `intcs` | `MutationCounter` | private `VersionCheckedIterator`, public `iterator`/`const_iterator` alias | `intcs` → `MutationVersion` | 14 | 3 | 64 → **64** | 24 → **24** | 56 → **56** | **A** |
| `Dictionary<K,V>` | Collections.Core | `Generic/Dictionary.hpp` | `intcs` | `MutationCounter` | private `VersionCheckedIterator`, public aliases | `intcs` → `MutationVersion` | 9 | 3 | 64 → **64** | 24 → **24** | 56 → **56** | **A** |
| `SortedDictionary<K,V>` | Collections.Core | `Generic/SortedDictionary.hpp` | `intcs` | `MutationCounter` | **public** `Iterator` | `intcs` → `MutationVersion` | 6 | 3 | 56 → **56** | 24 → **24** | 48 → **48** | **A** |
| `SortedList<K,V>` | Collections.Core | `Generic/SortedList.hpp` | `intcs` | `MutationCounter` | private `Enumerator` | `intcs` → `MutationVersion` | 8 | 3 | 56 → **56** | 32 → **32** | 48 → **48** | **A** |
| `OrderedDictionary<K,V>` | Collections.Core | `Generic/OrderedDictionary.hpp` | `intcs` | `MutationCounter` | **public** `Iterator` | `intcs` → `MutationVersion` | 10 | 3 | 88 → **88** | 32 → **32** | 80 → **80** | **A** |
| `Queue<T>` | Collections.Core | `Generic/Queue.hpp` | `intcs` | `MutationCounter` | private `Enumerator` | `intcs` → `MutationVersion` | 6 | 3 | 88 → **88** | 32 → **32** | 80 → **80** | **A** |
| `Stack<T>` | Collections.Core | `Generic/Stack.hpp` | `intcs` | `MutationCounter` | private `Enumerator` | `intcs` → `MutationVersion` | 6 | 3 | 88 → **88** | 32 → **32** | 80 → **80** | **A** |
| `ArrayList` | Collections.Core | `ArrayList.hpp` | `intcs` | `MutationCounter` | private `Enumerator` | `intcs` → `MutationVersion` | 16 | 3 | 40 → **40** | 40 → **40** | 8 → **8** | **A** |
| `Hashtable` | Collections.Core | `Hashtable.hpp` | `intcs` | `MutationCounter` | private `Enumerator` (+ `MemberEnumerator` view projection) | `intcs` → `MutationVersion` | 8 | 3 | 72 → **72** | 72 → **72** | 64 → **64** | **A** |
| `ListDictionaryInternal` | Collections.Core | `ListDictionaryInternal.hpp` | `intcs` | `MutationCounter` | private `NodeEnumerator` (+ view projection) | `intcs` → `MutationVersion` | 5 | 3 | 40 → **40** | 40 → **40** | 32 → **32** | **A** |
| `Collections::Queue` | Collections.Core | `Queue.hpp` | `intcs` | `MutationCounter` | private `Enumerator` | `intcs` → `MutationVersion` | 4 | 3 | 96 → **96** | 32 → **32** | 88 → **88** | **A** |
| `Collections::Stack` | Collections.Core | `Stack.hpp` | `intcs` | `MutationCounter` | private `Enumerator` | `intcs` → `MutationVersion` | 4 | 3 | 96 → **96** | 32 → **32** | 88 → **88** | **A** |
| `LinkedList<T>` | Collections.Core | `Generic/LinkedList.hpp` | `intcs` | `NarrowMutationCounter` (u32) | private `Enumerator`; public `begin()/end()` iterator is **not** version-checked | `intcs` → `NarrowMutationVersion` | 7 | 3 | 40 → **40** | 40 → **40** | 36 → **36** | **A (partial) + B** |
| `BitArray` | Collections.Core | `BitArray.hpp` | `std::uint32_t` | `NarrowMutationCounter` | **public** `Enumerator`; `begin()/end()` return raw `vector<bool>` iterators | `std::uint32_t` → `NarrowMutationVersion` | 9 | 3 | 48 → **48** | 32 → **32** | 40 → **40** | **A (partial) + B/C** |
| `SortedSet<T>` | Collections.Core | `Generic/SortedSet.hpp` | `ulongcs` (fixed by #1786) | unchanged | public `Iterator` | `ulongcs` | 4 | 3 | 40 / 104 | 40 | State@48 | **already fixed** |

Notes the table cannot carry:

- **"Read/compare sites" is three for every type**: the snapshot at
  enumerator/iterator construction, the equality guard, and (for the
  enumerator-shaped types) the same guard reached from `Reset()`. No type has a
  fourth reader — in particular none compares the counter with `<`, `>`, or
  arithmetic, so nothing depends on monotonicity across a wrap.
- **No spare padding is measured anywhere after the change.** Before it, every
  wide-counter type had exactly four bytes of slack at the counter's position;
  the widening consumed precisely that. `ArrayList` is the one type whose
  counter sits *before* the payload (offset 8, right after the vptr) and whose
  slack was the four bytes of alignment padding between the counter and
  `_items`.
- **Emitted-symbol implications:** `Collections.Core` is an `INTERFACE` target
  and produces no archive, so there is no pre-built object in this repository
  that could disagree with a header change. All fifteen types are header-only
  (`.hpp` only, no owning `.cpp`). §12.2 measures the symbol delta anyway.
- **Tests covering invalidation before this ticket:**
  `Ticket1713VersionTrackingTests.cpp` (the original fail-fast suite),
  `EnumeratorLifecycleTests.cpp` (#1767's lifecycle guard),
  `BitArrayTests.cpp`, `DictionaryTests.cpp`, `HashSetTests.cpp`,
  `ListTests.cpp`, `QueueStackTests.cpp`, `ListDictionaryInternalTests.cpp`, and
  `DictionaryKeyAndViewContractTests.cpp`. None of them covered a boundary
  value, the 2^32 alias distance, or assignment-versus-enumerator interaction —
  which is why all three defects survived to here.

### 3.2 What is deliberately excluded

`BitArray`'s and every collection's element **counts**, `LinkedList<T>::count_`,
`Uri`/`Http` protocol version strings, `SortVersion`, timestamps, IDs, and
`Guid` version nibbles. None is a mutation counter and none participates in
enumerator invalidation.

---

## 4. Pre-fix evidence

All probes live in the repository-local, gitignored `build-probe-collversion/`
tree. **No production or test source was modified before this evidence was
taken**, and the pre-fix binaries are built from the *committed* headers,
extracted by `git show HEAD:…` into `build-probe-collversion/prefix-include/`,
so every reproduction stays runnable now that the working tree is repaired.

```
build-probe-collversion/build_prefix.sh probe2_defects ubsan   # pre-fix, UBSan
build-probe-collversion/build_prefix.sh probe2_defects asan    # pre-fix, ASan+UBSan
build-probe-collversion/build.sh        probe2_defects ubsan   # post-fix
```

`probe2_defects.cpp` is **one source used on both sides**: each mode positions
a counter at a chosen value and asks the public API what it answers, so nothing
is conditionally compiled and the two columns below are directly comparable.
Output separates `invariants-failed` (must be 0 on both sides) from
`defects-observed` (expected pre-fix, expected 0 post-fix).

The counter is positioned with GCC's **`-fno-access-control`**, which suppresses
access checking and nothing else: no macro is defined over a library header, no
declaration is edited, and the code generated for each collection is what an
ordinary translation unit generates. No mode performs more than a few dozen real
mutations.

| Mode | Pre-fix | Post-fix |
|---|---|---|
| `ub-increment` | **14 UBSan signed-integer-overflow reports**, every counter lands on −2147483648; `BitArray` wraps to 0 with no report | **0 reports**, every counter goes to 2147483648; `BitArray` unchanged |
| `iterator-aba` | **15 stale enumerators/iterators revalidated** | **2** — `LinkedList<T>` and `BitArray` only, by documented design (§8) |
| `assign-alias` | **8 stale enumerators survive whole-container assignment** | **0** |
| `assign-alias-uaf` | **6 ASan heap-use-after-free / heap-buffer-overflow** | **0** |
| `sentinel-scan` | 0 (no sentinel exists) | 0 |
| `no-op-mutation` | 0 invariants failed | 0 invariants failed |

### 4.1 The exact UBSan diagnostics — fourteen of them

`build-probe-collversion/probe2_prefix_ub-increment.log`, verbatim (paths
shortened to the header):

```
Generic/List.hpp:116:68:              runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/HashSet.hpp:99:20:            runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/Dictionary.hpp:108:9:         runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/SortedDictionary.hpp:145:9:   runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/SortedList.hpp:158:9:         runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/OrderedDictionary.hpp:252:9:  runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/LinkedList.hpp:367:9:         runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/Queue.hpp:100:65:             runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Generic/Stack.hpp:100:62:             runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
ArrayList.hpp:243:9:                  runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Hashtable.hpp:233:9:                  runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
ListDictionaryInternal.hpp:248:9:     runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Queue.hpp:112:52:                     runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
Stack.hpp:113:49:                     runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```

Each report is at one of that type's increment sites; the other sites are the
same expression reached the same way, and the probe exercises whichever
mutation is the shortest path to the boundary. The reports are *recoverable*
UBSan diagnostics, so execution continues and the probe can go on to show the
wrapped value — which is the point: the wrap is not merely theoretical, it is
what the ABA mode then exploits.

`BitArray` produces **no** report and lands on 0, because its counter was
already `std::uint32_t`. That is the evidence for its Category C standing on
defect 1 (§9).

### 4.2 Iterator/enumerator ABA — fifteen of them

`probe2_prefix_iterator-aba.log`. For each type the probe takes an
enumerator, proves the guard fires on the very next mutation, then positions the
counter 2^32 forward — which on a 32-bit field lands back on the snapshot,
exactly what 2^32 further effective mutations would do. The guard stops firing
in **every** case:

```
List<int>                  snapshot=4 counter-2^32-later=4 guard-fired=0
HashSet<int>               snapshot=1 counter-2^32-later=1 guard-fired=0
Dictionary<int,int>        snapshot=1 counter-2^32-later=1 guard-fired=0
SortedDictionary<int,int>  snapshot=1 counter-2^32-later=1 guard-fired=0
SortedList<int,int>        snapshot=3 counter-2^32-later=3 guard-fired=0
OrderedDictionary<int,int> snapshot=1 counter-2^32-later=1 guard-fired=0
LinkedList<int>            snapshot=3 counter-2^32-later=3 guard-fired=0
Generic::Queue<int>        snapshot=3 counter-2^32-later=3 guard-fired=0
Generic::Stack<int>        snapshot=3 counter-2^32-later=3 guard-fired=0
ArrayList                  snapshot=3 counter-2^32-later=3 guard-fired=0
Hashtable                  snapshot=2 counter-2^32-later=2 guard-fired=0
ListDictionaryInternal     snapshot=2 counter-2^32-later=2 guard-fired=0
Collections::Queue         snapshot=3 counter-2^32-later=3 guard-fired=0
Collections::Stack         snapshot=3 counter-2^32-later=3 guard-fired=0
BitArray                   snapshot=1 counter-2^32-later=1 guard-fired=0
defects-observed=15
```

Post-fix the same log reads `guard-fired=1` for thirteen of them, with
`counter-2^32-later=4294967300` and similar values a 32-bit field could never
hold, and `guard-fired=0` for exactly `LinkedList<int>` and `BitArray` — the two
Category B families, by design and by documented residual (§8).

### 4.3 Assignment transplanting the counter — the defect nobody had recorded

`probe2_prefix_assign-alias.log` and `probe2_prefix_assign-alias-uaf.log`.

Fourteen of the fifteen types use the **implicitly declared** copy and move
assignment operators, which perform member-wise assignment and therefore copy
`version_` verbatim out of the source. `LinkedList<T>` is the exception: ticket
#1769 gave it explicit operators that call `detachAll()` and `++version_`, so it
never had this defect.

The straightforward shape — two collections that have taken the same number of
mutations, so their counters are equal:

```
List<int>            snapshot=2 counter-after-assign=2 guard-fired=0
SortedList<int,int>  snapshot=1 counter-after-assign=1 guard-fired=0
Generic::Queue<int>  snapshot=1 counter-after-assign=1 guard-fired=0
Generic::Stack<int>  snapshot=1 counter-after-assign=1 guard-fired=0
ArrayList            snapshot=1 counter-after-assign=1 guard-fired=0
BitArray             snapshot=1 counter-after-assign=1 guard-fired=0
Collections::Queue   snapshot=1 counter-after-assign=1 guard-fired=0
Collections::Stack   snapshot=1 counter-after-assign=1 guard-fired=0
defects-observed=8
```

The node-based containers are worse than a wrong answer. With `a` holding four
elements and an outstanding iterator on one of them, `b` holding one, and the
counters equal, `a = std::move(b)` destroys everything the iterator can refer to
while leaving the guard silent:

| Sub-mode | ASan diagnostic, pre-fix |
|---|---|
| `hashset` | `heap-use-after-free`, READ of size 4 |
| `dictionary` | `heap-use-after-free`, READ of size 4 |
| `sorteddictionary` | `heap-use-after-free`, READ of size 4 |
| `ordereddictionary` | `heap-buffer-overflow`, READ of size 4 |
| `hashtable` | `heap-use-after-free`, READ of size 8 |
| `listdictionary` | `heap-use-after-free`, READ of size 8 |

Post-fix all six report `no-defect:…=0` and produce no ASan diagnostic.

Two honest qualifications:

1. A **symmetric** copy assignment (`a = b` where both hold one element) does
   **not** reliably produce an ASan error, because libstdc++'s associative
   containers may reuse existing nodes rather than freeing them. The first
   attempt at this reproduction produced silent revalidation with no
   diagnostic, and the asymmetric shrinking move-assignment above is what makes
   the memory error deterministic. The contract violation is present either way;
   the *memory* error depends on the standard library's assignment strategy,
   which is precisely why relying on it was never safe.
2. `OrderedDictionary`'s iterator stores an index rather than a node pointer, so
   its failure is a heap-buffer-overflow into a shrunken `std::vector` instead
   of a use-after-free.

### 4.4 What the no-op-mutation mode pins

`probe2_prefix_no-op-mutation.log` and its post-fix twin both report
`invariants-failed=0`: a rejected duplicate `HashSet::Add`, a rejected
`OrderedDictionary::TryAdd`, and an absent `Remove` on `HashSet`,
`Dictionary`, `SortedDictionary`, `SortedList`, `OrderedDictionary`,
`LinkedList`, and `ListDictionaryInternal` bump nothing, before and after. That
is ticket 1713's contract and this ticket preserves it exactly.

> **Follow-up (ticket #1802, 2026-07-29) — `Hashtable` joins that list.** It is
> absent above, and the reason was *not* that its `Remove` was correct: all three
> `Hashtable::Remove` overloads were `_map.erase(key); ++version_;`, so the
> collection had **no** operation that could be asked to change nothing, and
> `CollectionVersionCounterTests.cpp`'s `HashtableAdapter` therefore carried
> `kHasNoOpMutation = false`. #1802 made `Remove` advance the counter only when an
> entry was actually erased, matching .NET `Hashtable.Remove` (`Hashtable.cs:999`,
> `UpdateVersion()` inside the found branch) and the "advance on effective
> mutation" rule this document's §6 already stated. The adapter now carries
> `kHasNoOpMutation = true` with an absent-key `Remove` as its no-op mutation,
> matching `ListDictionaryAdapter`. Nothing in this ticket's own evidence changes;
> `Clear()` keeps its unconditional bump on both non-generic dictionaries, as a
> decided deviation from .NET `Hashtable` recorded in
> `docs/HashtableValueAccessSafetyDesign.md` §35.4.

---

## 5. .NET comparison

Read from the local current .NET sources, not from memory.

| Type | .NET source | Counter | Increment | Enumerator check | Sentinel |
|---|---|---|---|---|---|
| `List<T>` | `System.Private.CoreLib/…/Generic/List.cs:27` | `internal int _version` | `_version++`, unchecked | `if (version != _version) throw` — equality only (`:645-656`) | none |
| `Dictionary<K,V>` | `…/Generic/Dictionary.cs:34` | `private int _version` | `_version++`, unchecked | equality only | none |
| `HashSet<T>` | `…/Generic/HashSet.cs:48` | `private int _version` | `_version++`, unchecked | equality only | none |
| `SortedList<K,V>` | `System.Collections/…/Generic/SortedList.cs` | `private int version` | `version++`, unchecked | equality only | none |
| `OrderedDictionary<K,V>` | `System.Collections/…/Generic/OrderedDictionary.cs` | `private int _version` | `_version++`, unchecked | equality only | none |
| `LinkedList<T>` | `System.Collections/…/Generic/LinkedList.cs` | `internal int version` | `version++`, unchecked | equality only | none |
| `Queue`, `Stack` (non-generic) | `System.Collections.NonGeneric/…` | `private int _version` | `_version++`, unchecked | equality only | none |
| `ListDictionary` | `System.Collections.Specialized/…/ListDictionary.cs` | `private int version` | `version++`, unchecked | equality only | none |
| `ArrayList` | `System.Private.CoreLib/…/ArrayList.cs:21` | `private int _version` | `_version++`, unchecked | `private readonly int _version` snapshot, equality only | none |
| `Hashtable` | `System.Private.CoreLib/…/Hashtable.cs:147` | `private volatile int _version` | `UpdateVersion()` → `_version++`, unchecked | equality only | none |
| `BitArray` | `System.Private.CoreLib/…/BitArray.cs:44` | `private int _version` — **signed**, unlike this port's `uint32_t` | `_version++`, unchecked | equality only | none |
| `SortedSet<T>` | `System.Collections/…/Generic/SortedSet.cs:56` | `private int version` | unchecked | equality only | `TreeSubSet` sets `version = -1; _countVersion = -1` |

### 5.1 What the comparison settles

1. **.NET is unanimously `Int32`, unchecked, equality-compared, with no
   sentinel** outside `SortedSet`'s `TreeSubSet`. It has defect 2 in every one
   of these types, as *defined-but-wrong* behaviour: C# arithmetic is unchecked
   by default and the runtime is not compiled with `/checked`, so `_version++`
   at `int.MaxValue` wraps to `int.MinValue` with fully defined two's-complement
   semantics.
2. **.NET says so out loud.** `Hashtable.cs:704-708`:

   > ```csharp
   > private void UpdateVersion()
   > {
   >     // Version might become negative when version is int.MaxValue, but the oddity will be still be correct.
   >     // So we don't need to special case this.
   >     _version++;
   > }
   > ```

   That comment is correct **in C#** and wrong as a guide for C++: "the oddity
   will still be correct" relies on defined wraparound, which C++ does not
   provide for signed types. Copying .NET's width here is exactly how fourteen
   instances of undefined behaviour got into this repository.
3. **.NET has no analogue of defect 3 at all.** Its collections are reference
   types and cannot be assigned; `ArrayList.Clone()` (`:229`) and
   `Hashtable.Clone()` (`:450`) do `la._version = _version` / `ht._version =
   _version`, which is the *copy-construction* case, and copying the value there
   is harmless because a freshly created object has no enumerator over it. This
   port's `BasicMutationCounter` matches .NET exactly on copy construction and
   deliberately diverges on assignment, because assignment is a C++-only
   operation with a C++-only hazard.
4. **This port should exceed .NET's robustness, and now does for thirteen
   types.** .NET's ABA horizon is 2^32; theirs is now 2^64. Exceeding the
   reference is justified because the C++ consequence of the shortfall (UB) is
   categorically worse than the managed one (a stale number), and because
   nothing observable to a conforming program changes.
5. **One pre-existing divergence is now deliberate rather than accidental.**
   `BitArray`'s counter was `std::uint32_t` where .NET's is `int`. That
   divergence — whatever its origin — is what spared it defect 1, and it is now
   documented and intentional.

---

## 6. The selected contract, stated exactly

```cpp
namespace SharpRuntime::Testing {
/** Test-only access seam; declared here, never defined in production code. */
template <typename TOwner> struct CollectionVersionAccess;
}

namespace System::Collections::detail {

template <typename TValue>
class BasicMutationCounter {
    static_assert(std::is_unsigned_v<TValue>, ...);
    TValue value_ = 0;
    template <typename> friend struct SharpRuntime::Testing::CollectionVersionAccess;

public:
    using value_type = TValue;

    constexpr BasicMutationCounter() noexcept = default;
    constexpr BasicMutationCounter(const BasicMutationCounter&) noexcept = default;  // inherits
    constexpr BasicMutationCounter(BasicMutationCounter&&) noexcept = default;       // inherits
    ~BasicMutationCounter() = default;

    // ADVANCES the destination rather than taking the source's value.
    constexpr BasicMutationCounter& operator=(const BasicMutationCounter&) noexcept
    { ++value_; return *this; }
    constexpr BasicMutationCounter& operator=(BasicMutationCounter&&) noexcept
    { ++value_; return *this; }

    constexpr BasicMutationCounter& operator++() noexcept { ++value_; return *this; }
    constexpr operator value_type() const noexcept { return value_; }               // implicit
    [[nodiscard]] constexpr value_type getValueProperty() const noexcept { return value_; }
};

using MutationCounter       = BasicMutationCounter<SharpRuntime::ulongcs>;
using MutationVersion       = MutationCounter::value_type;
using NarrowMutationCounter = BasicMutationCounter<SharpRuntime::uintcs>;
using NarrowMutationVersion = NarrowMutationCounter::value_type;

}
```

The eleven requirements a correct solution had to satisfy, answered without
hedging:

1. **No signed-overflow UB.** `value_type` is unsigned; unsigned arithmetic is
   modulo 2^N by definition ([basic.fundamental]), so the increment is defined
   for **every** representable prior value, including the maximum, where it
   yields 0. Verified by UBSan (§14).
2. **No sentinel collision.** No collection reserves a counter value. There is
   nothing to collide with, proven rather than assumed (§10).
3. **No stale enumerator revalidation within the selected contract.** 2^64 for
   the thirteen wide types; 2^32 for the two narrow ones, stated as a residual
   and pinned by a test rather than hidden (§8).
4. **Mutation invalidation preserved.** Every increment site is unchanged —
   `++version_` still compiles to an increment of the same field at the same
   offset — and every guard is unchanged.
5. **No-op mutation behaviour preserved.** A rejected duplicate and an absent
   removal still bump nothing (§4.4, and permanent tests).
6. **Copy behaviour preserved.** Copy construction still inherits the value, so
   a clone behaves exactly as before and as .NET does.
7. **Assignment behaviour repaired, not changed gratuitously.** Assignment still
   replaces the destination's contents, is still self-assignment-safe, and still
   returns `*this`. The only difference is that the destination's counter
   advances instead of being overwritten — which is the defect being fixed. No
   *well-defined* program changes behaviour: the only program affected is one
   that keeps enumerating a collection after that collection was wholesale
   replaced, which is precisely what the fail-fast contract says must throw, and
   which was a use-after-free before.
8. **Thread-safety properties and limitations preserved.** No atomic was added,
   none widened, and no `mutable` cache was introduced. The counter is a plain
   non-atomic field before and after. Concurrent mutation is unsupported before
   and after; this ticket makes no concurrency claim (§14.3).
9. **Public layout and symbols preserved where claimed** — measured, §12.
10. **No allocation and no lock on any ordinary mutation.** The counter owns
    nothing and the increment is one instruction.
11. **Warning-free** at `-Wall -Wextra -Wpedantic`, and `-Werror` in the
    consumer fixture.

### 6.1 Why the conversion operator is implicit

It keeps every existing snapshot initialisation (`version_(owner->version_)`)
and every guard (`version_ != owner_->version_`) spelled exactly as it was when
the field was a bare integer, so the whole production diff is two field
declarations per header plus an include and a friend line. The type lives in
`System::Collections::detail`, is never part of a public signature, and the
narrower explicit `getValueProperty()` accessor exists alongside it for code
that wants to be explicit.

### 6.2 Self-assignment advances the counter, deliberately

`c = c` bumps. The alternative — writing fifteen pairs of hand-rolled
assignment operators with `if (this != &other)` guards, plus fifteen pairs of
explicitly defaulted copy/move constructors to keep `-Wdeprecated-copy` quiet —
is roughly 130 lines of new hand-written special-member code across classes
with polymorphic bases, for a degenerate case. And bumping is the *safe*
answer: member-wise self-assignment of `std::unordered_map`/`std::unordered_set`
is permitted to reallocate the nodes an outstanding iterator points at, so
failing fast on `c = c` is correct rather than merely conservative. .NET has no
analogue to compare against. `LinkedList<T>` keeps its own guarded operator and
therefore does *not* bump on self-assignment; both behaviours are pinned by
tests (§13).

---

## 7. Alternatives evaluated

| # | Alternative | Verdict |
|---|---|---|
| A | Change `intcs` → `uintcs` only (32-bit unsigned) | *Rejected for the thirteen*, **adopted for the two** whose layout forbids more. It closes defect 1 and leaves defect 2 untouched, and — as #1786's §7 noted — makes that ABA marginally *easier* to reach because the path to it becomes well-defined. That trade is unacceptable when a strictly better option costs the same, which it does for thirteen types, and it is the best available option when the better one costs a public object-size change, which it does for two. |
| B | Widen to `ulongcs` everywhere | **Selected for thirteen.** Free: the widening lands in padding each type already had, measured per type (§12.1). Blocked for `LinkedList<T>` and `BitArray` (§8). |
| C | Checked exhaustion — throw or refuse to increment past a terminal value | Rejected. A branch on **every** mutation across fifteen collections, to guard a condition reachable in about 20 seconds of hot looping at 32 bits and not at all at 64. An exhaustion throw would also have to fire *after* the backing container was already modified, leaving the counter and the container disagreeing, and no repository precedent or .NET behaviour supports inventing an exception type for it. #1786 measured a *real* +1 ns regression from exactly one such branch. |
| D | Pair the counter with independent state identity | Rejected as adding nothing. Every enumerator here already holds a raw `const Collection*`, and that pointer does not change when the counter wraps — the collection is the same object. Making identity change means replacing the collection, which assignment does and which defect 3's repair already handles directly. |
| E | A never-reused heap-allocated generation token per mutation | Rejected. An allocation on every `Add`/`Remove`/`Clear` across fifteen collections, replacing a counter that is already exact for 2^64 steps. Measured mutation cost is 0.6–14 ns/op (§15); a `new` would dominate it. |
| F | Saturating counter | Rejected, and it must be: once saturated, every subsequent mutation leaves the counter unchanged, so an enumerator snapshotted at the saturated value **never fail-fasts again**. It converts a rare wrong answer into a permanent one. |
| G | Move the counter behind a `shared_ptr`, as `SortedSet<T>` does | Rejected. It would fix `LinkedList<T>`'s and `BitArray`'s layout problem by adding an indirection *and* an allocation to every collection, change every container's `sizeof` (the very thing being protected), and make `SortedSet<T>`'s live-view semantics — where copies share state — accidentally apply to containers whose documented value semantics are independent copies. |
| H | Reorder members so the wider counter fits | Rejected as arithmetically impossible for the two blocked types, not merely unattractive. `LinkedList<T>` is `shared_ptr` (16) + `weak_ptr` (16) + `intcs count_` (4) + counter (4) = 40 with **zero** padding; any 8-byte counter makes it 48 regardless of order. `BitArray::Enumerator` needs 4 + 1 + 4 bytes after the snapshot and has 8 available; any order gives 40. |

---

## 8. Category B — the two families that need approval

Both are **fully repaired for defects 1 and 3** and both retain **defect 2**,
because closing it means growing a public object. Neither breaking change is
implemented; each has a blocked ticket stating the exact approval required.

### 8.1 `LinkedList<T>` — ticket #1788 — **APPROVED AND CLOSED, 2026-07-29**

> The approval this section asks for was granted and the change landed. Everything
> below is #1787's original analysis, preserved unedited because its predictions
> are what #1788 was measured against — and every one of them held exactly, including
> `sizeof` 40 → 48 and `sizeof(Enumerator)` staying 40. The implementation record
> is **§19**.


- **Affected public type:** `System::Collections::Generic::LinkedList<T>` and
  its private `Enumerator`.
- **Exact current layout (LP64):** `head_` (`shared_ptr`) @0 (16), `tail_`
  (`weak_ptr`) @16 (16), `count_` (`intcs`) @32 (4), `version_`
  (`NarrowMutationCounter`, `uintcs`) @36 (4). `sizeof` **40**, `alignof` 8,
  **no padding at all**. `Enumerator` is 40 with room to spare, so the
  *enumerator* is not the constraint.
- **Exact proposed layout:** `version_` becomes `MutationCounter` (8 bytes) at
  offset 40 (or 32 with `count_` moved to 40). `sizeof(LinkedList<T>)`
  **40 → 48** for every `T`.
- **Source impact:** none. No signature, return type, parameter, or `const`
  qualification changes; the diff is one field's type plus the enumerator's
  snapshot type.
- **Symbol impact:** none — no mangled name encodes a private field's width.
- **ABI impact:** real. Any consumer that stores a `LinkedList<T>` by value in
  its own type, embeds one in an array, or passes one across a translation-unit
  boundary compiled against the older header has a layout mismatch.
- **Iterator/enumerator impact:** the snapshot widens from `uintcs` to
  `ulongcs` inside `Enumerator`'s existing 40 bytes; `sizeof(Enumerator)` stays
  40. The public `begin()/end()` iterator (`detail::LinkedListIterator`) does
  **not** version-check and is unaffected.
- **Rebuild requirement:** every consumer must be recompiled. There is no
  compatibility shim, and no link error would announce the mismatch.
- **Alternatives considered:** §7 A (shipped as the partial fix), C, D, G, H —
  all rejected there. H is impossible, not merely unattractive.
- **Recommended solution:** widen to `MutationCounter` and accept `sizeof`
  40 → 48, in the same approval category tickets #1771, #1780, and #1783 used.
- **Exact approval required:** explicit user approval that
  `sizeof(LinkedList<T>)` may grow from 40 to 48 bytes on LP64, with the
  consequent full-consumer rebuild.
- **Implementation and test scope:** one field type, one snapshot type, flip
  `LinkedListAdapter::kNarrowCounter` to `false` in
  `CollectionVersionCounterTests.cpp` (which turns the pinned residual into the
  wide-family assertion automatically), update the published `sizeof` in that
  suite and in `docs/LinkedListNodeLifetime.md`, and re-run
  `build-probe-collversion/probe1` and `probe2 iterator-aba`.

### 8.2 `BitArray` — ticket #1789 — **APPROVED AND CLOSED, 2026-07-29**

> The approval this section asks for was granted and the change landed. Everything
> below is #1787's original analysis, preserved unedited because its predictions
> are what #1789 was measured against — and every one of them held exactly,
> including `sizeof(Enumerator)` 32 → 40 and `sizeof(BitArray)` staying 48. The
> implementation record is **§20**.

- **Affected public types:** `System::Collections::BitArray` and its **public**
  nested `Enumerator`.
- **Exact current layout (LP64):** `BitArray` is `bits_` (`vector<bool>`) @0
  (40) + `version_` (`NarrowMutationCounter`) @40 (4), `sizeof` **48** with 4
  bytes of tail padding — so the *container* could absorb a widening for free.
  `Enumerator` cannot: vptr @0, `arr_` @8, `version_` @16 (4), `index_`
  (`intcs`) @20 (4), `current_` (`bool`) @24 (1), `state_`
  (`EnumeratorState`) @28 (4), `sizeof` **32**.
- **Exact proposed layout:** `version_` becomes `MutationCounter` (8) @16, after
  which `index_` @24, `current_` @28, `state_` @32 → `sizeof(Enumerator)`
  **32 → 40**. `sizeof(BitArray)` stays 48. Widening the container alone would
  be *wrong*, not merely partial: the snapshot would then be a truncation of the
  counter and the 2^32 alias would remain while the code claimed otherwise.
- **Source impact:** none.
- **Symbol impact:** none.
- **ABI impact:** real but narrower than #1788's — `BitArray::Enumerator` is a
  public nested class, so a consumer *can* name and store one, though every
  repository use hands it out as `IEnumerator*` from `GetEnumerator()`.
- **Rebuild requirement:** every consumer must be recompiled.
- **Alternatives considered:** §7 A (shipped), H (impossible: 9 bytes needed, 8
  available, in any member order).
- **Recommended solution:** widen both, accepting
  `sizeof(BitArray::Enumerator)` 32 → 40.
- **Exact approval required:** explicit user approval that
  `sizeof(BitArray::Enumerator)` may grow from 32 to 40 bytes on LP64.
- **Implementation and test scope:** two field types, flip
  `BitArrayAdapter::kNarrowCounter`, update the published `sizeof`, re-run
  `probe1` and `probe2 iterator-aba`.

### 8.3 Why these are two tickets and not one

They share the *symptom* and nothing else. #1788 grows a **container** because
its members are exactly packed; #1789 grows a **public enumerator** because its
members are exactly packed. The representations differ, the ABI consequence
differs in blast radius, and a user might reasonably approve one and not the
other. Grouping them would force a single yes/no on two independent decisions.
For the same reason they are **not** grouped with the thirteen Category A types,
which needed no approval at all.

---

## 9. Category C — where no change was required, with evidence

| Claim | Evidence |
|---|---|
| `BitArray` never had defect 1 | Its counter was already `std::uint32_t`. `probe2 ub-increment` produces **no** UBSan report for it and the counter lands on 0 from `UINT32_MAX`, while the other fourteen all report (§4.1). Unsigned wraparound is defined; nothing needed fixing. |
| `LinkedList<T>` never had defect 3 | Its `operator=(const LinkedList&)` and `operator=(LinkedList&&)` (`LinkedList.hpp:508`, `:523`, from ticket #1769) call `detachAll(); ++version_;` before adopting the other's nodes. `probe2 assign-alias` does not list it, and `CollectionVersionCounterSpecifics.LinkedListAssignmentAlreadyBumpedAndStillDoes` pins it. |
| `SortedSet<T>` never had defect 3 | Its `Iterator` holds `shared_ptr<const State>`, so it **co-owns** the state it enumerates. Assignment rebinds the handle's `state_` and cannot free the state an outstanding iterator observes; the iterator continues to see an unchanged counter on a state that still exists. Recorded in #1786 §9.5 and unchanged here. |
| No collection outside `SortedSet<T>` caches a value against its counter | Every one derives `Count` from the backing container's `size()` on every call. `probe2 sentinel-scan` records `types-with-a-count-cache-keyed-on-the-counter=0`, and `NoCounterValueIsReservedAsASentinel` enumerates each collection with its counter positioned at five distinct values, including the maximum, and gets the exact element count every time. So #1786's defects 3 and 4 genuinely do not apply. |
| No collection reserves a counter sentinel | The repository-wide search for `version_ == -1`, `version_ = -1`, and equivalents returns nothing outside `SortedSet<T>`'s repaired Count-cache tag (§10). |
| No guard depends on counter ordering | All three read sites per type use `==`/`!=` only. Nothing compares with `<`, `>`, or subtraction, so wraparound cannot make a guard read "older" or "newer" incorrectly — it can only make it read "equal". |
| The public `begin()/end()` iterators of `List`, `SortedList`, `Queue<T>`, `Stack<T>`, `BitArray`, and `LinkedList<T>` are unaffected | They return raw standard-library iterators (or, for `LinkedList<T>`, `detail::LinkedListIterator`, which stores no snapshot) and follow ordinary STL invalidation rules, as their own class documentation already states. Only the `GetEnumerator()`-returned enumerator is fail-fast. |

---

## 10. Sentinel analysis

`SortedSet<T>` before #1786 was the only place in the repository where a counter
value carried a second meaning: `kCountNotCached = -1` marked "this view has
never computed its Count", and the counter itself reached −1 after 2^32 − 1
effective mutations, so a cold cache read as warm and answered 0. #1786 fixed it
by biasing the tag by one.

For the fifteen types in this sweep the analysis is short and negative:

1. **Nothing reserves a value.** Every counter is default-initialised to 0 and
   only ever incremented. There is no "unset", "invalid", or "not computed"
   marker anywhere.
2. **Nothing stores a counter value outside the counter itself and an
   enumerator's snapshot.** With no cache and no tag, there is no second field
   whose domain could overlap the counter's.
3. **0 is not special.** A fresh collection's counter is 0 and a fresh
   enumerator's snapshot is 0, and those two agreeing is exactly correct — the
   collection has not been mutated since the enumerator was made.
4. **The maximum is not special.** `ExhaustionWrapsToZeroWithoutUndefinedBehaviour`
   positions each counter at `numeric_limits<value_type>::max()`, mutates, and
   checks that the counter reaches 0 without UB *and* that an enumerator
   snapshotted at the maximum is still invalidated by the wrap.

So the sentinel-collision question is answered "not applicable, and here is why"
rather than left open.

---

## 11. Iterator/enumerator ABA analysis

The mechanism is identical in all fifteen: an enumerator snapshots the counter
at construction and compares `snapshot == current` before touching storage. The
guard is exact while the counter is injective over the enumerator's lifetime and
silent the moment the counter returns to the snapshot value.

| Family | Pre-fix ABA distance | Post-fix ABA distance | Consequence of a false positive |
|---|---|---|---|
| The thirteen Category A types | 2^32 effective mutations | **2^64** | — |
| `LinkedList<T>`, `BitArray` | 2^32 | 2^32 (residual, §8) | see below |
| `SortedSet<T>` (#1786) | 2^32 | 2^64 | — |

What a false positive costs, per storage category — this is why the defect is
not merely cosmetic:

- **Node-based** (`HashSet`, `Dictionary`, `SortedDictionary`, `Hashtable`,
  `ListDictionaryInternal`, `LinkedList`): the enumerator holds a real iterator
  or node pointer into storage that may have been freed. Reading it is
  undefined behaviour, demonstrated as `heap-use-after-free` in §4.3.
- **Vector-backed** (`List`, `OrderedDictionary`, `ArrayList`): the enumerator
  holds an index which is bounds-checked against the *current* size, so a stale
  enumerator returns wrong elements rather than reading out of bounds — except
  `OrderedDictionary::Iterator`, which indexes `entries_` directly from
  `operator*` and produced a `heap-buffer-overflow`.
- **Deque-backed** (`Queue<T>`, `Stack<T>`, `Collections::Queue`,
  `Collections::Stack`): index-based and bounds-checked, so wrong elements.
- **`BitArray`**: index-based and bounds-checked against the current length.

Is 2^64 reachable? A measured mutation costs 0.6–14 ns (§15), so ~10^8–10^9
increments per second at best. 2^64 increments at an implausibly generous 10^9/s
takes 1.8 × 10^10 seconds — **over 580 years of uninterrupted mutation of one
collection instance**. It is stated as a residual (§16) rather than claimed
impossible.

Is 2^32 reachable for the two narrow types? At 10^8 increments per second, in
about **43 seconds**. That is why #1788 and #1789 exist and are recommended
rather than filed and forgotten.

---

## 12. Source, symbol, and layout compatibility — measured

### 12.1 Object layout — ✅ unchanged for every type

`build-probe-collversion/probe1_layout_inventory.cpp`, diffing
`probe1_prefix_layout.log` against `probe1_postfix_layout.log`. Every
`sizeof`, every `alignof`, and **every counter offset** is identical; the only
changes in the whole diff are the counter's *width* and the derived slack
column.

| Type | `sizeof` pre → post | `alignof` | counter offset pre → post | counter width pre → post |
|---|---|---:|---|---|
| `List<int>`, `List<std::string>` | 40 → **40** | 8 | 32 → **32** | 4 → 8 |
| `HashSet<int>` | 64 → **64** | 8 | 56 → **56** | 4 → 8 |
| `Dictionary<int,int>` | 64 → **64** | 8 | 56 → **56** | 4 → 8 |
| `SortedDictionary<int,int>` | 56 → **56** | 8 | 48 → **48** | 4 → 8 |
| `SortedList<int,int>` | 56 → **56** | 8 | 48 → **48** | 4 → 8 |
| `OrderedDictionary<int,int>` | 88 → **88** | 8 | 80 → **80** | 4 → 8 |
| `Generic::Queue<int>` | 88 → **88** | 8 | 80 → **80** | 4 → 8 |
| `Generic::Stack<int>` | 88 → **88** | 8 | 80 → **80** | 4 → 8 |
| `ArrayList` | 40 → **40** | 8 | 8 → **8** | 4 → 8 |
| `Hashtable` | 72 → **72** | 8 | 64 → **64** | 4 → 8 |
| `ListDictionaryInternal` | 40 → **40** | 8 | 32 → **32** | 4 → 8 |
| `Collections::Queue` | 96 → **96** | 8 | 88 → **88** | 4 → 8 |
| `Collections::Stack` | 96 → **96** | 8 | 88 → **88** | 4 → 8 |
| `LinkedList<int>`, `LinkedList<std::string>` | 40 → **40** | 8 | 36 → **36** | 4 → **4** |
| `BitArray` | 48 → **48** | 8 | 40 → **40** | 4 → **4** |

| Enumerator / iterator | `sizeof` pre → post | snapshot offset pre → post | snapshot width pre → post |
|---|---|---|---|
| `List<int>::Enumerator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `HashSet<int>::iterator` / `const_iterator` | 24 → **24** | 8 → **8** | 4 → 8 |
| `Dictionary<int,int>::iterator` / `const_iterator` | 24 → **24** | 8 → **8** | 4 → 8 |
| `SortedDictionary<int,int>::Iterator` | 24 → **24** | 16 → **16** | 4 → 8 |
| `SortedList<int,int>::Enumerator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `OrderedDictionary<int,int>::Iterator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `LinkedList<int>::Enumerator` | 40 → **40** | 16 → **16** | 4 → **4** |
| `Generic::Queue<int>::Enumerator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `Generic::Stack<int>::Enumerator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `ArrayList::Enumerator` | 40 → **40** | 16 → **16** | 4 → 8 |
| `Hashtable::Enumerator` | 72 → **72** | 16 → **16** | 4 → 8 |
| `ListDictionaryInternal::NodeEnumerator` | 40 → **40** | 16 → **16** | 4 → 8 |
| `Collections::Queue::Enumerator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `Collections::Stack::Enumerator` | 32 → **32** | 16 → **16** | 4 → 8 |
| `BitArray::Enumerator` | 32 → **32** | 16 → **16** | 4 → **4** |

**"`sizeof` is unchanged" is deliberately not the claim being made.** The claim
is that `sizeof`, `alignof`, and the counter's own offset are all unchanged, and
that the counter's width grew into padding the type already had at that exact
position — so every member before it keeps its offset and there is no member
after it. Where a type had no such padding (`LinkedList<T>`, and
`BitArray::Enumerator`), the widening was **not** performed; that is the whole
content of §8. The published figures are LP64 and are asserted in the permanent
suite behind a `sizeof(void*) == 8` guard.

### 12.2 Emitted symbols — ten new weak inline definitions, nothing changed or removed

`build-probe-collversion/probe4_symbols.cpp` explicitly instantiates all nine
class templates and touches every non-template collection, then `nm
--defined-only` is diffed between the two header revisions
(`probe4_prefix_symbols.log` → `probe4_postfix_symbols.log`, 3,398 → 3,408
symbols):

- **Symbols removed or renamed: 0.** Verified with `comm -23`
  (`probe4_symbols_removed.log` is empty).
- **Symbols added: 10** (`probe4_symbols_added.log`), all of them the new
  counter class's own inline members — eight weak (`W`) and two local (`n`):

```
W _ZN6System11Collections6detail20BasicMutationCounterIjEC1Ev   (ctor, uint32)
W _ZN6System11Collections6detail20BasicMutationCounterIjEC2Ev
W _ZN6System11Collections6detail20BasicMutationCounterIjEppEv   (operator++)
W _ZN6System11Collections6detail20BasicMutationCounterImEC1Ev   (ctor, uint64)
W _ZN6System11Collections6detail20BasicMutationCounterImEC2Ev
W _ZN6System11Collections6detail20BasicMutationCounterImEppEv
W _ZNK6System11Collections6detail20BasicMutationCounterIjEcvjEv (conversion)
W _ZNK6System11Collections6detail20BasicMutationCounterImEcvmEv
n _ZN6System11Collections6detail20BasicMutationCounterIjEC5Ev
n _ZN6System11Collections6detail20BasicMutationCounterImEC5Ev
```

These are weak inline definitions emitted by the *consumer's own* translation
unit, exactly as `detail::EnumeratorState`'s already are, not new library
exports: `Collections.Core` is an `INTERFACE` target and emits no archive.
Recording them is the honest version of the claim — "the symbol list is
byte-identical" would have been false.

### 12.3 Public source compatibility — ✅ unaffected

No public signature, return type, parameter, or `const` qualification changed.
The entire production diff is, per header: one `#include`, one field type, one
snapshot field type, and one `friend` declaration. `getCountProperty()` still
returns a plain `intcs` by value everywhere; no 64-bit counter, counter class,
or atomic appears in any public surface. Pinned by `static_assert`s in both the
permanent suite and the consumer fixture.

### 12.4 Value semantics and type traits — ✅ unaffected

All fifteen remain copy-constructible, copy-assignable, move-constructible, and
move-assignable; `is_polymorphic` is unchanged per type (false for the generic
templates, true for `ArrayList`/`Hashtable`/`ListDictionaryInternal`/the
non-generic `Queue` and `Stack`, which derive from `IList`/`IDictionary`/
`ICollection`). None was trivially copyable before — every one contains a
standard container — so the counter's user-provided assignment operator changes
no trait a consumer could have relied on. Asserted in
`ValueSemanticsTraitsAreUnchanged`.

### 12.5 Practical rebuild requirement

**None on this revision's account.** No mangled name changed, no public layout
changed, and nothing links against a stale symbol. Consumers that recompile
normally pick the change up. Mixing translation units compiled against two
revisions of a header is an ODR violation regardless of this change. This is the
same standing #1784 and #1786 had, and unlike #1783, which required a full
rebuild.

---

## 13. Permanent test matrix

`modules/collections/tests/System/Collections/CollectionVersionCounterTests.cpp`,
**336 cases**. Near-boundary cases position the counter through the test-only
friend seam `SharpRuntime::Testing::CollectionVersionAccess<T>` (§13.2). No test
performs more than a few dozen real mutations; the longest loop is 25 steps. The
whole suite executes in under 20 ms.

Two typed suites cover the two enumerator shapes, plus targeted `TEST`s:

- `CollectionVersionCounter` — 11 adapters × 24 cases = **264**: the
  enumerator-shaped types (`List`, `SortedList`, `Generic::Queue`,
  `Generic::Stack`, `ArrayList`, `Hashtable`, `ListDictionaryInternal`,
  `Collections::Queue`, `Collections::Stack`, `LinkedList`, `BitArray`).
- `CollectionIteratorVersion` — 4 adapters × 15 cases = **60**: the types whose
  `begin()/end()` return a version-checked iterator (`HashSet`, `Dictionary`,
  `SortedDictionary`, `OrderedDictionary`).
- `CollectionVersionCounterElementTypes` (**3**), `…Specifics` (**4**), and
  `…Compatibility` (**5**) — 12 targeted cases carrying 30+ `static_assert`s.

| Required coverage | Where |
|---|---|
| Ordinary mutation invalidates an existing enumerator/iterator | `OrdinaryMutationInvalidatesAnOutstandingEnumerator`, `OrdinaryMutationInvalidatesAnOutstandingIterator`, `ResetAlsoFailsFastAfterAMutation` |
| A rejected duplicate `Add` does not invalidate | `ARejectedOrAbsentMutationInvalidatesNothing` (both suites) — `HashSet::Add` duplicate, `OrderedDictionary::TryAdd` duplicate |
| An absent `Remove` does not invalidate | same, for `List`, `SortedList`, `ListDictionaryInternal`, `LinkedList`, `HashSet`, `Dictionary`, `SortedDictionary`, `OrderedDictionary` |
| `Clear` behaviour | `ClearInvalidatesAnOutstandingEnumerator`, `ClearInvalidatesAnOutstandingIterator` (skipped with an explicit `SUCCEED` for `BitArray`, which has no `Clear`) |
| Enumerator created immediately before a near-boundary transition | `AnEnumeratorTakenAtTheBoundaryIsInvalidatedByTheNextMutation`, `AnIteratorTakenAtTheBoundaryIsInvalidatedByTheNextMutation` |
| Snapshot at the transition | `IncrementAtTheOldInt32BoundaryMovesForward`, `MutationAtSevenBoundaryValuesNeverThrowsOrGoesBackwards` (0, 1, 2^31−2, 2^31−1, 2^31, 2^32−2, 2^32−1) |
| Mutation beyond the old 32-bit boundary | `MutationPastTheOldBoundaryStaysExactAndKeepsInvalidating` — 25 interleaved mutations from 2^32 (or 2^31−1 for the narrow families), each checked exactly |
| No stale snapshot becomes valid | `NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance` at +1·2^32, +2·2^32, +7·2^32; `…AcrossTheOld2Pow32Distance` at +1, +2, +5 for the iterator suite |
| Counter exhaustion is defined | `ExhaustionWrapsToZeroWithoutUndefinedBehaviour` — positions at `numeric_limits<value_type>::max()`, mutates, checks 0 and that the pre-wrap snapshot is still rejected |
| Sentinel never collides with a valid version | `NoCounterValueIsReservedAsASentinel` — five positions per type including the maximum, each producing the exact element count |
| Copy | `CopyConstructionLeavesTheSourceEnumerable`, `CopyConstructionLeavesTheSourceIterable` |
| Move | `MoveAssignmentInvalidatesEnumeratorsOverTheDestination` (both suites) |
| Assignment | `CopyAssignmentInvalidatesEnumeratorsOverTheDestination`, `CopyAssignmentStillReplacesTheContents`, `AssignmentWithAMatchingSourceCounterStillInvalidates` (the exact pre-fix defect shape), `AssignmentDoesNotDisturbTheSourcesOwnEnumerators`, `SelfAssignmentBehavesAsThatCollectionDocuments` |
| Empty and one-element collections | `AnEmptyCollectionEnumeratesToCompletionAndStaysExact`, `AMutationOnAnEmptyCollectionInvalidatesItsEnumerator`, `AFreshCollectionStartsCountingFromZero`, `AnEmptyCollectionIteratesToCompletion` |
| Public interface iteration | `RangeForOverAnUnmutatedCollectionSeesEveryElement`, `ReadingWithoutMutatingNeverInvalidates`, and `HashtableKeyAndValueViewsInheritTheTablesFailFast` (through `ICollection*`/`IEnumerator*` only) |
| Custom comparer / non-trivial values | `ListOfStringsInvalidatesAcrossTheOldBoundary`, `DictionaryOfStringsInvalidatesOnAssignment`, `SortedDictionaryWithAnOrderOnlyKeyStillInvalidates` (a key type providing only `operator<`) |
| Counter type and width | `TheCounterIsUnsigned`, `TheCounterHasTheWidthItsLayoutPermits`, `TheCounterIsUnsignedAndSixtyFourBits` |
| The two documented residuals | `NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance` asserts the *opposite* for `LinkedList` and `BitArray`, so #1788/#1789 must flip it on purpose |
| Per-type specifics | `DictionaryIndexerInsertBumpsButOverwriteDoesNot`, `LinkedListAssignmentAlreadyBumpedAndStillDoes`, `BitArraySetAllAndLengthChangesInvalidate` |
| Source/layout/trait compatibility | `TheCounterTypeItselfBehavesAsSpecified`, `NoCounterOrCounterTypeLeaksThroughAPublicSurface`, `ValueSemanticsTraitsAreUnchanged`, `PublishedObjectSizesAreUnchanged`, `PublishedIteratorSizesAreUnchanged` |

**No existing assertion was edited.** All 1,841 pre-existing Collections.Core
tests still pass unmodified — which is itself evidence that the assignment
repair breaks no established behaviour.

### 13.1 Consumer fixtures

- `test/consumer/collections_mutation_version.cpp` — positive, compiled against
  only the public `Collections.Core` surface with `-Wall -Wextra -Wpedantic
  -Werror` (applied to the fixture target by `test/consumer/CMakeLists.txt`).
  Exercises the fail-fast contract, the assignment repair on every family, copy
  construction, and the counter's invisibility. Exits 0.
- `test/consumer/collections_mutation_version_negative.cpp` — negative,
  proving a consumer cannot use the test seam at all. Fails to compile with
  `error: incomplete type 'SharpRuntime::Testing::CollectionVersionAccess<…>'
  used in nested name specifier`, which is the intended and only possible
  outcome.

### 13.2 The test-only access seam

Near-boundary behaviour cannot be reached through the public API without
billions of real mutations, which the ticket forbids. The permanent suite
therefore positions counters through

```cpp
namespace SharpRuntime::Testing { template <typename TOwner> struct CollectionVersionAccess; }
```

which `detail/MutationCounter.hpp` **declares**, which `BasicMutationCounter`
befriends for **all** specialisations (`template <typename> friend struct …`) so
that one seam serves every collection, and which each of the fifteen collections
befriends for **its own** specialisation only. Properties that make this
acceptable rather than a dangerous hook:

- It grants *access* and defines *no behaviour*. Production code cannot call it,
  because nothing defines it in production — demonstrated, not asserted, by the
  negative consumer fixture.
- A friend declaration changes no object layout, no signature, and no mangled
  symbol — measured in §12.1 and §12.2.
- It is portable ISO C++, unlike the `-fno-access-control` the throwaway probes
  use, so it works on every toolchain this repository builds for.
- It is one seam name for fifteen classes, rather than fifteen names. #1786 set
  the precedent with `SortedSetVersionAccess<T>`; this generalises it.

### 13.2.1 Where the definition lives — corrected by ticket #1800

This section originally said the seam is "**defined in exactly one translation
unit** — the test file", and that was true on the day #1787 landed. It stopped
being true almost immediately. #1794 wrote a **second, different** body — one
without `positionVersion`, and a counter-level partial specialisation without
`write` — and #1796, #1798 and #1802 each copied that second body into a further
suite. By 2026-07-29 **five** translation units of the one
`SharpRuntimeTests_Collections_Core` program defined three of these
specialisations, with **two token-different bodies each**. Two definitions of one
class with different member sets violate [basic.def.odr]/12 and are ill-formed,
no diagnostic required.

Ticket **#1800** closed it. The definition now lives in exactly one **file** —
`modules/collections/tests/support/CollectionVersionSeam.hpp` — which all five
suites include, so the token sequence is identical by construction rather than by
discipline. The canonical body is the **richer** one, this ticket's: `version()`
plus `positionVersion()`, and `read()` plus `write()` at the counter level, so
nothing §13's matrix needs was traded away.
`scripts/check_version_seam_odr.py` now fails the repository gate if a second
file defines the same specialisation, if two definitions of one specialisation
differ, if a seam is defined in a production tree, or if one file writes seam
bodies through two macros. The reproduction, the C++ analysis, the alternatives
and the measured evidence are in `docs/CollectionVersionTestSeamDesign.md`; the
short version is that at `-O0` the link order decided which body the whole
program executed, at `-O1` the two units disagreed with each other, and `ld`,
`-flto -Wodr`, ASan with `detect_odr_violation=2` and UBSan all reported nothing.

---

## 14. Sanitizer results

| Check | Result |
|---|---|
| UBSan, pre-fix `probe2 ub-increment` | **14 signed-integer-overflow reports**, one per collection (§4.1) |
| UBSan, post-fix, all six `probe2` modes | **0 diagnostics** |
| ASan, pre-fix `probe2 assign-alias-uaf` | **6 heap-use-after-free / heap-buffer-overflow** (§4.3) |
| ASan, post-fix, all six sub-modes | **0 diagnostics**, all report `no-defect` |
| ASan + UBSan + LSan over the permanent suites | **349/349 pass**, 0 diagnostics, 0 leaks (`build-asan-collversion/run_1787_asan.log`) |
| LeakSanitizer actually active | Twice over: it caught a **real leak in the first draft of this ticket's own test** (`Hashtable::getKeysProperty()`'s caller-owned view, 24 bytes in 1 allocation — fixed by adding the `delete`), and the repository's deliberate-leak self-test still reports 4,112 bytes in 102 allocations, exit 1 |
| TSan `probe3`, `read-only-enumeration` | **0 races** — four threads enumerating one shared instance of twelve collections, 200 rounds each |
| TSan `probe3`, `independent-instances` | **0 races** |
| TSan `probe3`, `copy-then-enumerate` | **0 races** — concurrent copy construction from a shared const source |
| TSan `probe3`, `known-race` self-test | **2 races**, as designed, so the zeroes above are evidence rather than a silent no-op |

### 14.1 The ASan/UBSan/LSan suite build

`build-asan-collversion/build_1787.sh` compiles only
`CollectionVersionCounterTests.cpp` and `EnumeratorLifecycleTests.cpp` plus the
exception support sources against a locally built sanitizer GoogleTest, so no
whole-repository sanitizer tree is created. One compiler process at a time.

### 14.2 Why TSan was run at all

This ticket adds **no atomic, no `mutable` cache, and no hidden `const` write** —
the counter was a plain non-atomic field before and is a plain non-atomic field
inside a wrapper afterwards, and the read/write sites are the same ones. TSan
therefore has nothing *new* to find, and `probe3` exists to substantiate that
claim rather than assert it. It covers the one path where the counter is read
from more than one thread (concurrent read-only enumeration, the supported
pattern) and the one new-ish path (concurrent copy construction from a shared
source).

### 14.3 What is explicitly *not* claimed

**No general concurrent-mutation safety.** No `probe3` mode ever mutates a
collection from two threads at once. Concurrent mutation is unsupported before
and after this ticket, and a report produced by it would say nothing about this
contract. That is the same position #1784's and #1786's TSan campaigns took.

---

## 15. Performance impact

`build-probe-collversion/probe5_perf.cpp`, one source compiled at `-O2 -DNDEBUG`
against both headers, median of seven timed runs. Every measured loop carries an
`asm volatile` compiler barrier per iteration — without one GCC hoists the
loop-invariant call out and the benchmark measures nothing, the mistake #1786's
§13.1 recorded and this probe avoids by construction.

| Operation | Pre-fix (ns/op) | Post-fix (ns/op) | Verdict |
|---|---:|---:|---|
| `List<int>` `Add`+`RemoveAt` pair | 0.932 | 0.664 | within noise |
| `HashSet<int>` `Add`+`Remove` pair | 13.947 | 13.082 | within noise |
| `Dictionary<int,int>` `Add`+`Remove` pair | 14.066 | 14.650 | within noise |
| `Queue<int>` `Enqueue`+`Dequeue` pair | 0.592 | 0.626 | within noise |
| `ArrayList` `Add`+`RemoveAt` pair | 7.655 | 6.940 | within noise |
| `List<int>` enumerated element | 1.169 | 1.118 | within noise |
| `HashSet<int>` iterator step | 0.867 | 0.868 | unchanged |
| `Dictionary<int,int>` iterator step | 0.867 | 0.873 | unchanged |
| `List<int>` copy assignment | 2.519 | 2.534 | unchanged |
| `List<int>` copy construction | 10.343 | 10.077 | within noise |
| `Dictionary<int,int>` copy assignment | 82–93 | 85–100 | within noise — see below |

The one figure worth a note: `Dictionary<int,int>` copy assignment looked 7%
slower on the first pair of runs. Three further paired runs
(`probe5_dict_assign_repeat.log`) gave pre 82.2 / 81.9 / 87.5 and post 84.9 /
86.9 / 85.8, so the two ranges overlap and the run-to-run spread (~12 ns) is
larger than the apparent difference. Reported as noise, with the repeat data,
rather than as a silent "no change".

Memory: unchanged for every collection and every enumerator (§12.1). No
allocation appears on any path; the counter owns nothing. A 64-bit increment and
a 64-bit compare cost the same as 32-bit ones on x86-64.

---

## 16. Risks and residual limitations

| # | Risk | Severity | Position |
|---|---|---|---|
| 1 | `LinkedList<T>` and `BitArray` keep a 2^32 ABA horizon | **Medium** | **Real and not eliminated.** Reachable in tens of seconds of hot mutation. Closing it needs a public object-size change and therefore user approval; blocked tickets #1788 and #1789 state exactly what is required. Pinned by a test so the residual cannot be forgotten or accidentally "fixed" without updating the record. **Fully closed 2026-07-29 under two separate approvals: #1788 widened `LinkedList<T>` (§19) and #1789 widened `BitArray` (§20). No collection in this repository retains a 2^32 horizon.** |
| 2 | The thirteen wide types keep a 2^64 ABA horizon | Negligible | Over 580 years of uninterrupted mutation of one instance (§11). Guarding it would cost a branch on every mutation (§7 C/F). Stated, not hidden. |
| 3 | Self-assignment now invalidates enumerators on fourteen collections | Low | Deliberate and argued (§6.2): member-wise self-assignment of the backing container may reallocate. `LinkedList<T>` keeps its no-op behaviour. Both are pinned by tests. .NET has no analogue. |
| 4 | Assignment now throws where it previously continued silently | Low | Only for a program that keeps enumerating a collection after that collection was wholesale replaced — which was a use-after-free for six of the fourteen. No well-defined program changes behaviour (§6, point 7). All 1,841 pre-existing Collections.Core tests pass unmodified. |
| 5 | `BasicMutationCounter`'s implicit conversion operator is a loose interface a future reader could widen | Low | It exists to keep the production diff to two field declarations per header (§6.1). It is `detail`, never in a public signature, and `getValueProperty()` is available where explicitness is wanted. |
| 6 | The test seam is a new name in a public header | Low | §13.2. Grants access only; never defined in production, proven by a negative consumer fixture; no layout, signature, or symbol effect, all measured. |
| 7 | The counter's width now differs from .NET's `int` on thirteen types | Low | Deliberate and argued (§5.1). Nothing observable to a conforming program differs. |
| 8 | `probe1`–`probe5` depend on `-fno-access-control`, a GCC/Clang extension | Low | Probes only, never the permanent suite, which uses the portable friend seam. |
| 9 | `BitArray`'s container could absorb a wider counter but its enumerator cannot | Low | Widening only the container would leave a truncated snapshot comparison and a *silent* 2^32 alias, which is worse than the honest one. Deliberately not done; #1789 covers both together. **Closed 2026-07-29 (§20): #1789 widened both in one change, exactly as this row required, and `ASnapshotSharingOnlyItsLowThirtyTwoBitsIsStillRejected` pins the truncation shut.** |

---

## 17. Follow-up ticket map

| Ticket | Key | Status | Scope |
|---|---|---|---|
| **#1788** | `REMED-COLL-LINKEDLIST-VERSION-WIDEN` | **done** 2026-07-29 | Widened `LinkedList<T>`'s counter *and its enumerator's snapshot* to 64 bits; approval granted, `sizeof(LinkedList<T>)` 40 → 48 on LP64 as predicted (§8.1, implementation §19) |
| **#1789** | `REMED-COLL-BITARRAY-VERSION-WIDEN` | **done** 2026-07-29 | Widened `BitArray`'s counter *and its public `Enumerator`'s snapshot* to 64 bits; approval granted, `sizeof(BitArray::Enumerator)` 32 → 40 and `sizeof(BitArray)` unchanged at 48 on LP64 as predicted (§8.2, implementation §20) |
| **#1790** | `REMED-COLL-LIST-INDEXER-VERSION` | **todo**, inactive | Category D, non-versioning: `List<T>::operator[]` returns a plain `T&`, so `list[i] = value` cannot bump the counter, unlike .NET's index setter. Documented in `List.hpp`'s own class comment since ticket 1713 and **not** introduced or worsened here. Closing it means a proxy-object return on the most call-site-heavy method in the repository. Recorded so it is tracked rather than only commented. |
| #1785 | `REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER` | **todo**, untouched | Unchanged by this ticket; no exception ordering was altered |
| #1773 | `REMED-COLL-COPYTO-DOWNSTREAM` | **blocked**, untouched | Out-of-repository; CNA and mobile-eggbert were not inspected |

Deliberately **not** created: a ticket to widen `SortedSet<T>`'s Count-cache
tag (#1786 §17 already explains why), and any ticket bundling #1788 with #1789
(§8.3).

No new `SR-AUD-*` identifier was issued. The audit numbering stays frozen at
364.

---

## 18. Implementation status

**Complete for the thirteen Category A families, and complete for the
non-blocking half of the two Category B families.** Implemented, tested,
sanitizer-validated, layout-verified, and committed on local branch
`feature/remediation-coll-version-counter-sweep`.

| Gate | Result |
|---|---|
| `cmake --build build --parallel 4` | 0 errors, 0 warnings |
| `SharpRuntimeTests_Collections_Core` | **2,177** passed (1,841 before, +336) |
| `scripts/local_ci_check.sh build` | **13,463** tests across 37 executables (13,127 before), exit 0 |
| `scripts/validate_module_boundaries.py --root .` | 41 modules / 90 edges — no new edge |
| `test/validate_module_boundaries_test.py` | 7 tests OK |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |
| `git diff --check` | clean |
| `scripts/check_doxygen_warnings.sh` | **1,938** warnings, ceiling 1,942 — one more than the pre-ticket 1,937, attributable entirely to the single new `README.md` markdown link to this document (§18.2) |
| `scripts/check_selective_components.sh` | all ten components pass |
| `check_selective_components.sh Collections.Core collections_mutation_version.cpp` | passes in isolation, 2,177 tests |
| Positive consumer fixture, `-Wall -Wextra -Wpedantic -Werror` | compiles, exits 0 |
| Negative consumer fixture (test seam) | correctly rejected as an incomplete type |
| ABI/layout probe | every `sizeof`, `alignof`, and counter offset unchanged; 0 symbols removed or renamed, 10 new weak inline definitions |

### 18.1 The one new Doxygen warning, accounted for

The canonical count moved **1,937 → 1,938**. Diffing the full warning list
against `HEAD` shows exactly one addition and zero removals:

```
+README.md:257: warning: unable to resolve reference to '…/docs/CollectionVersionCounterSweep.md' for \ref command
```

`Doxyfile` scans the module include trees and `README.md` only, not `docs/`, so
**every** markdown link from `README.md` into `docs/` produces one unresolvable
`\ref` warning. There were already six of them — to
`SortedSetLiveViewDesign.md`, `Migration-ICollectionCopyTo.md`, `plan.md`,
`NEXT.md`, `CLAUDE.md`, and `prompt.md`. This is the seventh, and it is the cost
of the README link that tells a consumer where the assignment behaviour change is
documented. It is well inside the 1,942 ceiling and is disclosed here rather than
described as "unchanged".

### 18.2 Rollback

`git revert` of the implementation commit restores the `intcs` counters and,
with them, all three defect classes. A revert must be validated by re-running
`build-probe-collversion/probe2_defects.cpp` under UBSan **and** ASan against
both headers, not by CTest alone — the permanent suite's near-boundary cases
position the counter through the seam and would observe the old behaviour as
correct if the seam's expectations were reverted with it.

---

## 19. Ticket #1788 implementation record — `LinkedList<T>` widened to 64 bits

*Recorded 2026-07-29 on local branch
`feature/remediation-coll-linkedlist-version-widen`. Ticket **#1788**, key
`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, priority **P3**, size **S**, category
`defect`, area `Collections`. **No new `SR-AUD-*` identifier** — the audit
numbering stays frozen at 364 and this was found during remediation, by #1787's
own §8.1. Sections 1–18 above are #1787's record and are preserved unedited;
this section is additive.*

### 19.1 The approval that was applied

The user granted, scoped to #1788 only, explicit approval to: widen the counter
to 64-bit unsigned; widen every iterator/enumerator snapshot that compares
against it; accept the measured `sizeof(LinkedList<int>)` **40 → 48**; accept any
corresponding enumerator layout change; accept the resulting public template
ABI/layout break; require a complete rebuild of every consumer adopting this
revision; and accept the removal of the pinned 2^32 ABA behaviour in favour of
the 64-bit contract.

The approval explicitly did **not** extend to #1789, #1803, any
`LinkedListNode` lifetime redesign, any unrelated `LinkedList` API change, any
downstream CNA/mobile-eggbert migration, or any additional public source or ABI
break. None of those was performed.

### 19.2 Old and new representation, and every site

| | Before #1788 | After #1788 |
|---|---|---|
| `LinkedList<T>::version_` | `detail::NarrowMutationCounter` (`BasicMutationCounter<uintcs>`, 32-bit) | `detail::MutationCounter` (`BasicMutationCounter<ulongcs>`, 64-bit) |
| `LinkedList<T>::Enumerator::version_` | `detail::NarrowMutationVersion` (`uintcs`) | `detail::MutationVersion` (`ulongcs`) |

Both moved together, deliberately. Widening the container alone would make the
comparison a silent truncation and leave the 2^32 alias in place while the code
claimed otherwise — the failure mode §8.2 identifies for `BitArray` and refuses.

**Seven increment sites**, all unchanged in spelling (`++version_`) and all still
one instruction:

| Site | `LinkedList.hpp` member | Reached from |
|---|---|---|
| 1 | `insertLast` | `AddFirst`/`AddLast` (both overloads), `insertAfter` at the tail, `copyNodesFrom` |
| 2 | `insertBefore` | `AddFirst`/`AddBefore` (both overloads), `insertAfter` in the middle |
| 3 | `unlink` | `Remove(T)`, `Remove(node)`, `RemoveFirst`, `RemoveLast` |
| 4 | `adoptNodesFrom` — `++other.version_` | move construction and move assignment, invalidating the **emptied source** |
| 5 | `operator=(const LinkedList&)` | copy assignment, after `detachAll()` |
| 6 | `operator=(LinkedList&&)` | move assignment, after `detachAll()` |
| 7 | `Clear()` | `Clear()`, unconditionally |

**Three read/compare sites**, all unchanged, all `==` only — nothing anywhere
compares the counter with `<`, `>` or subtraction, so no guard depends on
monotonicity across a wrap:

| Site | Expression |
|---|---|
| 1 | `Enumerator::Enumerator` — `version_(list->version_)`, the snapshot |
| 2 | `Enumerator::MoveNext` — `requireUnmodified(version_ == list_->version_)` |
| 3 | `Enumerator::Reset` — the same guard |

The production diff is two field declarations plus comments. Every increment and
every guard is spelled exactly as it was.

### 19.3 The pre-fix reproduction, taken before anything changed

`build-probe/1788_probe2_defects.cpp` — one source run against both headers, with
the pre-fix header extracted by `git show` into
`build-probe/1788_prefix-include/`. The counter is positioned with GCC's
`-fno-access-control`, which suppresses access checking and nothing else; no
macro is defined over a library header and no declaration is edited. No mode
performs more than a few dozen real mutations.

`build-probe/1788_prefix_defects.log`, verbatim:

```
counter-value-type-bytes=4
counter-max=4294967295
enumerator-snapshot-bytes=4
LinkedList<int>          snapshot=3 counter-2^32-later=3 guard-fired=0
LinkedList<std::string>  snapshot=2 counter-2^32-later=2 guard-fired=0
LinkedList<int> Reset()   guard-fired=0
defects-observed=3
```

`build-probe/1788_postfix_defects.log`, same source, same modes:

```
counter-value-type-bytes=8
counter-max=18446744073709551615
enumerator-snapshot-bytes=8
LinkedList<int>          snapshot=3 counter-2^32-later=4294967299 guard-fired=1
LinkedList<std::string>  snapshot=2 counter-2^32-later=4294967298 guard-fired=1
LinkedList<int> Reset()   guard-fired=1
defects-observed=0
```

The complete diff of the two logs is: the counter's width, those three
`guard-fired` outcomes, and one sentinel probe reaching a larger maximum.
**Every delta line is byte-identical**, which is the evidence that the mutation
semantics did not move.

**No signed-overflow UB remained to fix.** The pre-fix probe under UBSan
(`1788_prefix_defects_ubsan.log`) reports **0 runtime errors** and exits 0 —
#1787 had already removed that, and #1788 closes only the residual *logical* ABA
horizon. Post-fix UBSan is likewise 0.

Also reconfirmed pre-fix, and unchanged post-fix: `LinkedList<T>`'s guarded
`operator=` from #1769 already bumped on copy and move assignment and already
short-circuited self-assignment (`self-copy-assign before=3 after=3`); no
sentinel exists (six counter positions including the maximum each enumerate the
exact element count); and there is no `Count` cache keyed on the counter.

### 19.4 The mutation-version delta matrix

Measured pre-fix and post-fix; **identical on both sides**. Pinned permanently by
`LinkedListVersionWideningTests.cpp`, not only by the probe.

| Operation | Δ version |
|---|---:|
| `AddFirst(value)`, `AddFirst(node)` | +1 |
| `AddLast(value)`, `AddLast(node)` | +1 |
| `AddBefore(node, value)`, `AddBefore(node, newNode)` | +1 |
| `AddAfter(node, value)`, `AddAfter(node, newNode)`, `AddAfter` at the tail | +1 |
| `Remove(value)` that finds a match | +1 |
| `Remove(node)`, `RemoveFirst()`, `RemoveLast()` | +1 |
| `Clear()` on a non-empty list | +1 |
| `Clear()` on an **already empty** list | **+1** (unconditional — the pre-existing contract, deliberately unchanged) |
| detach then reattach one node | +2 |
| copy assignment `a = b` | +1 for the explicit bump, **+1 per node copied** |
| move assignment `a = std::move(b)` | +1 on the destination, **+1 on the emptied source** |
| self copy assignment `a = a` | **0** — #1769's `if (this != &other)` guard |
| self move assignment | **0** |
| `Remove(value)` that finds nothing | 0 |
| cross-list `Remove`/`AddBefore`/`AddAfter` (throws) | 0 |
| duplicate attachment of an attached node (throws) | 0 |
| null node handle (throws `ArgumentNullException`) | 0 |
| `RemoveFirst`/`RemoveLast` on an empty list (throws) | 0 |
| `Contains`, `Find`, `FindLast`, `CopyTo`, `getCountProperty`, range-`for` | 0 |
| `node.setValueProperty(v)` — a value write, not a structural change | 0 |

The last row is the pre-existing documented contract and #1788 deliberately did
not revisit the mutation/no-op policy: this ticket changed a width, nothing else.

### 19.5 Object layout — re-measured, not assumed

`build-probe/1788_probe1_layout.cpp`, one source run against both headers
(`1788_prefix_layout.log` → `1788_postfix_layout.log`), LP64, GCC 14.2,
libstdc++. Offsets are computed from a live object, because `LinkedList<T>` is
not standard-layout in the `offsetof` sense for every `T`.

| | pre | post |
|---|---|---|
| `sizeof(LinkedList<int>)` / `<std::string>` / `<double>` | 40 | **48** |
| `alignof(LinkedList<T>)` | 8 | **8** |
| `head_` offset (`shared_ptr`, 16) | 0 | **0** |
| `tail_` offset (`weak_ptr`, 16) | 16 | **16** |
| `count_` offset (`intcs`, 4) | 32 | **32** |
| `version_` offset (width) | 36 (4) | **40 (8)** |
| `sizeof(LinkedList<T>::Enumerator)` | 40 | **40** |
| `Enumerator::version_` offset (width) | 16 (4) | **16 (8)** |
| `Enumerator` `list_` / `cur_` / `started_` / `state_` offsets | 8 / 24 / 32 / 36 | **8 / 24 / 32 / 36** |
| `sizeof(LinkedListNode<T>)` | 16 | **16** |
| `sizeof(iterator)` / `sizeof(const_iterator)` | 16 / 16 | **16 / 16** |
| `sizeof(detail::LinkedListNodeData<int>)` / `<std::string>` | 48 / 72 | **48 / 72** |

Two things the table is worth reading carefully for:

1. **Only the container grew.** Every member *before* `version_` keeps its
   offset, and `version_` is last, so nothing after it moved — there is nothing
   after it. `count_` stays at 32 and the four bytes at 36–39 became alignment
   padding.
2. **The enumerator was free.** Its snapshot at offset 16 was followed by four
   bytes of padding before `cur_` at 24; the wider snapshot consumed exactly
   that. `sizeof(Enumerator)` is 40 before and after and **every other member
   keeps its offset**. §8.1 predicted this and the prediction held.

Type traits are unchanged: `LinkedList<T>` remains copy-constructible,
copy-assignable, move-constructible, nothrow-move-constructible, non-polymorphic,
non-trivially-copyable and standard-layout, for `int` and for `std::string`.

### 19.6 Emitted symbols — zero `LinkedList` symbols changed

`build-probe/1788_probe4_symbols.cpp` explicitly instantiates `LinkedList<T>`,
`LinkedListNode<T>` and both iterator specialisations for three element types and
touches every public member; `nm --defined-only` is then diffed between the two
header revisions. **1,321 symbols on both sides.**

- **`LinkedList`-named symbols: 796 pre, 796 post, and the sorted name lists are
  byte-identical** — 0 added, 0 removed, 0 renamed. No mangled name encodes a
  private field's width, which is precisely why this ABI break is silent.
- The only delta anywhere is 5 removed / 5 added, and all ten are the counter
  class's own weak inline members swapping instantiation:
  `BasicMutationCounter<unsigned int>`'s constructor (three aliases),
  `operator++` and conversion operator give way to
  `BasicMutationCounter<unsigned long>`'s. In this probe the `<unsigned int>`
  instantiation disappears entirely because the probe does not include
  `BitArray`; in the real library `BitArray` still emits it.

### 19.7 Source compatibility — unaffected

No public signature, return type, parameter, or `const` qualification changed on
`LinkedList<T>`, `LinkedListNode<T>`, or either iterator.
`getCountProperty()` still returns a plain `intcs` by value; `GetEnumerator()`
still returns `IEnumerator<T>*`; `AddLast(const T&)` still returns
`LinkedListNode<T>`; `Remove(const T&)` still returns `bool`. No new overload
ambiguity: no overload set changed at all. `operator bool`, node equality, and
the `operator T()` conversion are untouched.

In-repository caller impact: **every existing call site compiles unchanged, with
no edit anywhere.** The consumers are
`LinkedListNodeLifetimeTests.cpp` (49 cases), `LinkedListSortedSetTests.cpp`
(30 LinkedList cases), `ListTests.cpp` (10 `GenLinkedList*` cases),
`EnumeratorLifecycleTests.cpp` (#1767's case),
`CollectionVersionCounterTests.cpp`, `Ticket1713VersionTrackingTests.cpp`, and
the `collections_linked_list.cpp` consumer fixture. There is no production
consumer of `LinkedList<T>` inside this repository. The only test edits made were
deliberate assertion changes, listed in §19.10.

### 19.8 The stale-object probe — what the break actually looks like

This is the half that matters, because "requires a rebuild" is worth nothing
unless someone measured what skipping it does.
`build-probe/1788_stale_old_caller.cpp` is compiled against the **pre-fix**
header (40 bytes) and `1788_stale_main.cpp` against the **current** one (48), and
the two objects are linked into one program in **both orders**.

**The linker says nothing.** Both link commands exit 0 with empty logs
(`1788_stale_linkA.log`, `1788_stale_linkB.log`) — no error, no warning. The two
halves genuinely disagree: `old-half-sizeof-LinkedList=40`,
`new-half-sizeof-LinkedList=48`, and for a consumer type embedding one by value,
`old-half-sentinel-offset=40` against `new-half-sentinel-offset=48`.

What then happens depends entirely on which object file won the COMDAT race:

| Mode | Link order A (new object first) | Link order B (old object first) |
|---|---|---|
| heap-allocated list made by old code, used by new code | **SEGV**; under ASan a **heap-buffer-overflow**, READ of size 8, "0 bytes after 40-byte region", in `BasicMutationCounter<unsigned long>::operator++()` | runs, correct counts, no diagnostic |
| `LinkedList<T>` embedded by value in a consumer struct | sentinel **corrupted**, `intact=0`, and **no ASan report at all** | sentinel intact |
| mutation invalidation across the mismatch | **`guard-fired=0`** — fail-fast silently lost | `guard-fired=1` |

Three properties make this the worst kind of break, and all three are measured
rather than asserted:

1. **No diagnostic at link time, in either order.** Not from `ld`, and this is
   the same class of miss `#1800` measured for `-flto -Wodr`, ASan
   `detect_odr_violation=2` and UBSan.
2. **The embedded case corrupts a neighbouring member with no sanitizer report**,
   because the corrupted bytes are inside the same allocation — ASan cannot see
   it. That is silent memory corruption in a program that looks healthy.
3. **In one of the two link orders everything appears to work.** A consumer who
   forgets to rebuild and happens to get link order B will observe nothing wrong
   until the link line changes.

The ASan transcript is `build-probe/1788_stale_asan_heap.log`; the full run in
both orders is `build-probe/1788_stale_object.log`.

**CNA and mobile-eggbert remain unmeasured.** They were not inspected, searched,
configured, built or modified, no claim is made about whether they use
`LinkedList<T>`, and ticket #1773 remains `blocked`.

### 19.9 Performance and allocation

`build-probe/1788_probe5_perf.cpp`, one source at `-O2 -DNDEBUG` against both
headers, with an `asm volatile` compiler barrier per iteration and replaced
global `operator new`/`delete` counting allocations.

The first pass — seven runs of one binary then seven of the other — showed
**every** row several percent faster post-fix, which is not a credible result for
a 32-to-64-bit increment and is the signature of machine drift. It was therefore
re-measured with the two binaries **interleaved**, seven paired runs, median
reported (`1788_perf_summary_paired.log`). Both logs are retained; the
interleaved one is the honest measurement.

| Operation | pre (ns/op) | post (ns/op) | Δ | allocs pre | allocs post |
|---|---:|---:|---:|---:|---:|
| `AddLast`+`RemoveLast` pair | 48.971 | 50.467 | +1.496 | 200000 | 200000 |
| `AddFirst`+`RemoveFirst` pair | 40.229 | 39.060 | −1.169 | 200000 | 200000 |
| `Remove(value)` at the head | 43.801 | 43.836 | +0.035 | 200000 | 200000 |
| iteration, per element | 0.490 | 0.493 | +0.003 | 0 | 0 |
| iterator creation (`begin`/`end`) | 0.224 | 0.230 | +0.006 | 0 | 0 |
| enumerator create + 64 × `MoveNext` | 76.051 | 76.176 | +0.125 | 12500 | 12500 |
| node detach + reattach | 32.164 | 32.818 | +0.654 | 0 | 0 |
| copy construction, 64 nodes | 1378.692 | 1410.506 | +31.814 | 800000 | 800000 |
| move construction, 64 nodes | 1529.939 | 1533.569 | +3.630 | 800000 | 800000 |
| copy assignment, 64 nodes | 1442.930 | 1426.480 | −16.450 | 800000 | 800000 |
| mutation across the old boundary | 50.131 | 48.832 | −1.299 | 200000 | 200000 |
| `AddLast`+`RemoveLast`, `std::string` | 45.731 | 45.330 | −0.401 | 200000 | 200000 |

**Verdict: no measurable cost.** The deltas straddle zero, and every one of them
is far below the ~30–50 ns node allocation that dominates each mutation; a 64-bit
increment and a 64-bit compare cost the same as 32-bit ones on x86-64. Crucially
the **allocation counts are identical in every row** — the widening adds no
allocation on any path, which was requirement 10 of §6.

Memory cost, stated plainly rather than buried: **+8 bytes per `LinkedList<T>`
object** (40 → 48, a 20% increase for an empty list) and **0 bytes per node**,
per enumerator, per node handle and per iterator. A list of *n* nodes costs
48 + 48*n* bytes for `T = int` where it previously cost 40 + 48*n*, so the
relative cost falls away immediately: it is 20% for an empty list, 4% at ten
nodes, and 0.4% at a hundred. Cache consequence: `LinkedList<int>` no longer fits
in a single 64-byte cache line together with an adjacent object at a 16-byte
offset, but the type was already 40 bytes of pointers and every traversal
immediately chases a `shared_ptr` to a separately allocated node, so the working
set is dominated by node locality, not by the header.

### 19.10 Permanent tests

A new focused suite,
`modules/collections/tests/System/Collections/Generic/LinkedListVersionWideningTests.cpp`,
**40 cases**, all near-boundary positioning done through the one authoritative
seam from #1800 (`modules/collections/tests/support/CollectionVersionSeam.hpp`) —
**no new explicit specialisation body was written**, as CLAUDE.md requires. No
test performs more than a few dozen real mutations; the whole suite runs in 7 ms.

| Required coverage | Where |
|---|---|
| The counter is unsigned, 64-bit, for every element type | `TheCounterIsUnsignedAndSixtyFourBits`, `TheCounterIsWideForEveryElementType` |
| The approved object growth | `TheObjectGrewToFortyEightBytesOnLp64` (48, `alignof` 8, node handle and both iterators still 16) |
| No counter leaks through a public surface; traits unchanged | `NoCounterOrCounterTypeLeaksThroughAPublicSurface`, `ValueSemanticsTraitsAreUnchanged` |
| **No stale snapshot revalidated across the old 2^32 distance** | `NoStaleEnumeratorIsRevalidatedAcrossTheOld2Pow32Distance` (multiples 1, 2, 7, 1000), `ResetAlsoFailsFastAcrossTheOld2Pow32Distance` |
| **No low-32-bit ABA** — the truncation a container-only widening would have left | `ASnapshotSharingOnlyItsLowThirtyTwoBitsIsStillRejected`, which asserts the low 32 bits match *and* the guard still fires |
| The horizon is 2^64, stated as a bound not an impossibility | `TheHorizonIsNowTwoToTheSixtyFourNotTwoToTheThirtyTwo` |
| Every named boundary position | `MutationAtEachNamedBoundaryValueMovesForwardExactly` — 0, 1, 2^31−2, 2^31−1, 2^31, 2^32−2, **2^32−1**, **2^32**, 2^32+5, 2^64−2 |
| The step that used to wrap | `TheStepThatUsedToWrapNowJustAdvances` (UINT32_MAX → 2^32, explicitly *not* 0) |
| Enumerator taken before, at, and after the transition | `AnEnumeratorTakenBeforeTheTransitionIsInvalidatedByIt`, `MutationPastTheOldBoundaryStaysExactAndKeepsInvalidating` (25 interleaved steps) |
| Exhaustion is defined | `ExhaustionWrapsToZeroWithoutUndefinedBehaviour` |
| No sentinel | `NoCounterValueIsReservedAsASentinel` (6 positions incl. the maximum) |
| The full delta matrix (§19.4) | `EveryEffectiveMutationAdvancesTheCounterByExactlyOne`, `EveryRejectedOrReadOnlyOperationAdvancesNothing`, `AThrowingRemoveOnAnEmptyListAdvancesNothing`, `ClearOnAnAlreadyEmptyListStillAdvancesUnconditionally`, `DetachAndReattachIsTwoEffectiveMutations` |
| Assignment, copy, move at the boundary | `CopyAssignmentAdvancesTheDestinationAcrossTheBoundary`, `MoveAssignmentAdvancesTheDestinationAcrossTheBoundary`, `AssignmentWithAMatchingSourceCounterStillInvalidates`, `SelfAssignmentIsStillANoOp`, `SelfMoveAssignmentIsStillANoOp`, `AssignmentDoesNotDisturbTheSourcesOwnEnumerators`, `MoveConstructionFollowsTheEstablishedOwnershipContract` |
| **A copied list is independent** | `AnIndependentCopyDoesNotInvalidateTheOriginalsEnumerator` |
| Node lifetime (#1768/#1769) at the boundary | `RemovalThroughACopiedHandleInvalidatesIterationAtTheBoundary`, `ReattachmentInvalidatesIterationAtTheBoundary`, `ClearAtTheBoundaryDetachesEveryRetainedHandle`, `OwnerDestructionWithRetainedNodesIsStillSafeAtTheBoundary`, `ADetachedNodeCanRejoinAnotherListAtTheBoundary`, `DuplicateAttachmentAndCrossListInsertionAreStillRejected`, `ALargeTeardownAtTheBoundaryFreesEveryNode` |
| Element types | `int` throughout; `StringsInvalidateAcrossTheOldBoundary`; `ANonTriviallyCopyableElementTypeBehavesIdentically` (live-instance counted); `AMoveOnlyElementTypeReachesTheSameContract` (through the existing-node overloads) |
| The public STL iterators are still **not** version-checked | `ThePublicStlIteratorsStillCarryNoSnapshot` |

**Deliberate assertion changes** — these are the ones a reviewer should look at,
and there are exactly four:

1. `LinkedListAdapter::kNarrowCounter` in `CollectionVersionCounterTests.cpp`
   flipped `true` → `false`. That is the flip #1787 designed the flag for: it
   automatically converts the pinned 2^32-residual assertion into the wide-family
   no-revalidation assertion, and converts
   `TheCounterHasTheWidthItsLayoutPermits` from asserting 4 bytes to 8.
2. `sizeof(G::LinkedList<int>) == 40` was **removed** from
   `PublishedObjectSizesAreUnchanged` rather than edited to 48, because that
   figure did *not* stay unchanged and a test with that name must not claim it
   did. A comment there points at the new home, and 48 is asserted in the new
   suite's `TheObjectGrewToFortyEightBytesOnLp64`.
3. The narrow branch of `NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance`
   was **strengthened** — see §19.11.
4. Comments in `CollectionVersionCounterTests.cpp`, `MutationCounter.hpp` and
   `LinkedList.hpp` were updated so none of them still describes
   `LinkedList<T>` as narrow.

`test/consumer/collections_linked_list_version.cpp` is a new **tracked** consumer
fixture, compiled against only the public `Collections.Core` surface with
`-Wall -Wextra -Wpedantic -Werror` and **run**, exercising ordinary construction,
every mutating member, the fail-fast contract, node lifetime, all four iteration
forms, and the 48-byte size a consumer can observe. It deliberately does **not**
reach the counter: no public accessor exists and this ticket added none. The
pre-existing `test/consumer/collections_linked_list.cpp` (#1769) is unchanged and
still compiles.

### 19.11 A weakness in #1787's own pin, found by mutation-testing this ticket

The flip in §19.10 item 1 was verified by putting the flag back to `true` and
rebuilding, to check the assertion was load-bearing rather than vacuous. Only
**one** test failed — `TheCounterHasTheWidthItsLayoutPermits`.
`NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance` passed either way.

The reason is a real, if harmless, weakness in #1787's narrow branch: it
positioned the counter at `static_cast<Value>(snapshot)` and expected no throw.
For a 32-bit counter that is the same value as `snapshot + 2^32`, so the
assertion was *correct* — but it holds for a counter of **any** width, so it
pinned nothing about the residual its comment described. The comment claimed more
than the code checked.

It is now written as `static_cast<Value>(snapshot + kOldAliasStep)` with an added
`EXPECT_EQ` asserting that the narrowing genuinely lands back on the snapshot.
That makes the truncation itself the thing under test, so the assertion is
load-bearing for `BitArray` and would fail if #1789 widened it without flipping
the flag. Recorded here rather than quietly fixed, because it means #1787's §13
matrix row *"The two documented residuals"* was, for that one test, weaker than
stated.

### 19.12 Sanitizers

| Check | Result |
|---|---|
| ASan + UBSan + LSan over `SharpRuntimeTests_Collections_Core` | **2,594/2,594 pass, zero reports**, exit 0 (`build-asan/1788_collections_core_asan.log`), run with `detect_leaks=1:detect_odr_violation=2` |
| UBSan, `1788_probe2_defects`, pre-fix | **0 runtime errors** — #1787 had already removed the signed overflow |
| UBSan, `1788_probe2_defects`, post-fix | **0 runtime errors** |
| ASan+UBSan+LSan `1788_probe3_sanitizers`, `boundary-mutation` | `failures=0`, no diagnostic |
| … `owner-destruction` (retained node, counter at the maximum, wrap to zero) | `failures=0`, no diagnostic |
| … `detach-reattach` (100 cycles across two lists, crossing the boundary) | `failures=0`, no diagnostic |
| … `copy-move-assign` (incl. self-assignment) | `failures=0`, no diagnostic |
| … `large-teardown` (**200,000 nodes**, counter positioned to wrap mid-loop, owning values) | `failures=0`, no diagnostic |
| **LeakSanitizer proved active** | the bounded self-test leaks deliberately and reports **336 bytes in 7 allocations**, exit 1 — so the zeros above are evidence, not a silent no-op |
| ASan, stale-object probe | **heap-buffer-overflow** reproduced as designed (§19.8) |

Each probe mode runs as its own process so no abort can hide another.

**TSan was not run, and that is a decision rather than an omission.** This ticket
adds no atomic, widens none, introduces no `mutable` cache and no hidden `const`
write; the counter is a plain non-atomic field of a different width, read and
written at the same three sites. TSan has nothing new to find. **No thread-safety
guarantee follows from this ticket** — concurrent mutation of a `LinkedList<T>`
is unsupported before and after, exactly as #1787 §14.3 states.

### 19.13 Gates

Everything below ran from a **fresh configuration plus a clean-first rebuild**,
which the object-layout change makes mandatory: any object file left over from
before the change would disagree about `sizeof(LinkedList<T>)` and be an ODR
violation inside the test binaries themselves.

| Gate | Result |
|---|---|
| `cmake --fresh -S . -B build` then `cmake --build build --clean-first --parallel 3` | **0 errors, 0 warnings**; 634 objects (630 C++, 4 C), **0 of them predating the fresh-configure marker**, 37 of 38 executables relinked |
| `scripts/run_component_tests.sh build` | **13,880** tests across **37** executables (13,840 before; +40, exactly the new suite) |
| `SharpRuntimeTests_Collections_Core` | **2,594** (2,554 before) |
| `scripts/validate_module_boundaries.py --root .` | 41 physical modules / 90 dependency edges — unchanged, no new edge |
| `test/validate_module_boundaries_test.py` | 7/7 |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |
| `scripts/check_version_seam_odr.py` | 2 seams / **18** specialisation definitions — unchanged, no seam added |
| `test/check_version_seam_odr_test.py` | 12/12 |
| `scripts/check_negative_consumer_fixtures.py` | **8 fixtures / 51 sites, every site rejected**, 59 compiler invocations, **peak 3 jobs** — unchanged; no negative fixture was added |
| `test/check_negative_consumer_fixtures_test.py` | 37/37 |
| `scripts/check_selective_components.sh` | all ten components pass |
| `check_selective_components.sh Collections.Core collections_linked_list_version.cpp` | passes in isolation, fixture exits 0, 2,594 tests |
| `scripts/check_doxygen_warnings.sh` | **1,941** of the 1,942 ceiling |
| `git diff --check` | clean |

The one **+1 Doxygen warning** (1,940 → 1,941) is fully attributed rather than
waved through: `Doxyfile` scans the module include trees and `README.md` only,
not `docs/`, so every markdown link from `README.md` into `docs/` produces one
unresolvable `\ref` warning — *including a second link to a document already
linked*. The new breaking-change entry adds `README.md:287` pointing at this
document, alongside the pre-existing `README.md:935` from #1787's entry. That is
the entire delta.

The 37th of 38 executables not relinked is `build/SharpRuntimeTests`, an
`EXCLUDE_FROM_ALL` 85 MB historical binary dated 2026-07-24 that the gate does
not match (it globs `SharpRuntimeTests_*`). **It is now definitively stale**: it
contains `LinkedList<T>` code compiled against the 40-byte header. It is left in
place, as #1791 left it, but it should be deleted rather than trusted.

### 19.14 Residual limitations, stated not hidden

| # | Residual | Position |
|---|---|---|
| 1 | The ABA horizon is now 2^64, **not infinity** | Over 580 years of uninterrupted mutation of one instance at an implausibly generous 10^9/s. It is modular arithmetic and `TheHorizonIsNowTwoToTheSixtyFourNotTwoToTheThirtyTwo` asserts the wrap explicitly rather than pretending it cannot happen. Guarding it would cost a branch on every mutation (§7 C/F). |
| 2 | `sizeof(LinkedList<T>)` grew 20% for an empty list | Approved, measured (§19.5, §19.9), and unavoidable — §7 H is arithmetically impossible here. |
| 3 | **A stale object file links silently and can corrupt memory** | §19.8. No linker, sanitizer or LTO diagnostic. The mandatory full rebuild is the only mitigation, and `README.md` says so in those terms. |
| 4 | `BitArray` keeps its 2^32 horizon | Untouched by design; ticket #1789 remains `blocked` on its own separate approval (§8.2, §8.3). **Closed later the same day by #1789 — §20.** |
| 5 | `begin()`/`end()` are still not version-checked | Pre-existing, documented, deliberate, and pinned by `ThePublicStlIteratorsStillCarryNoSnapshot`. `LinkedListNode<T>` remains the lifetime-safe handle. |
| 6 | `Clear()` on an already-empty list still bumps unconditionally | Pre-existing contract, deliberately not revisited — this ticket changed a width. Now pinned so the next reader sees it is a decision. |
| 7 | No thread-safety guarantee | The counter is non-atomic before and after. TSan not run, with reasons (§19.12). |
| 8 | CNA and mobile-eggbert are **unmeasured** | Not inspected, searched, configured, built or modified. #1773 stays `blocked`. |

### 19.15 Rollback

`git revert` of the implementation commit restores the 32-bit counter and, with
it, the 2^32 ABA horizon. A revert must also flip
`LinkedListAdapter::kNarrowCounter` back to `true` and remove or invert the new
suite — otherwise the permanent tests fail, which is the intended safety
property. Validate a revert by re-running
`build-probe/1788_probe2_defects.cpp` against both headers and confirming it
reports `defects-observed=3` again, and by a **fresh clean-first rebuild**, since
the layout moves back.

---

## 20. Ticket #1789 implementation record — `BitArray` widened to 64 bits

*Recorded 2026-07-29 on local branch
`feature/remediation-coll-bitarray-version-widen`. Ticket **#1789**, key
`REMED-COLL-BITARRAY-VERSION-WIDEN`, priority **P3**, size **XS**, category
`defect`, area `Collections`. **No new `SR-AUD-*` identifier** — the audit
numbering stays frozen at 364 and this was found during remediation, by #1787's
own §8.2. Sections 1–19 above are #1787's and #1788's records and are preserved
unedited; this section is additive.*

### 20.1 The approval that was applied

The user granted, scoped to #1789 only, explicit approval to: widen the
`BitArray` mutation version from 32 bits to the 64-bit unsigned
`MutationCounter`; widen every `BitArray::Enumerator` snapshot that compares
against it; accept the measured `sizeof(BitArray::Enumerator)` **32 → 40**;
accept any directly resulting `BitArray` object-layout change proven necessary;
accept the resulting public template/type ABI and layout break; require a
complete rebuild of every consumer adopting this revision; and replace the 2^32
stale-enumerator ABA horizon with the 64-bit contract.

The approval explicitly did **not** extend to #1803, any new `BitArray` public
API, any unrelated `BitArray` semantic change, any further collection layout
change, any downstream CNA/mobile-eggbert migration, or general thread safety.
None of those was performed.

### 20.2 Old and new representation, and every site

| | Before #1789 | After #1789 |
|---|---|---|
| `BitArray::version_` | `detail::NarrowMutationCounter` (`BasicMutationCounter<uintcs>`, 32-bit) | `detail::MutationCounter` (`BasicMutationCounter<ulongcs>`, 64-bit) |
| `BitArray::Enumerator::version_` | `detail::NarrowMutationVersion` (`uintcs`) | `detail::MutationVersion` (`ulongcs`) |

Both moved together, deliberately. Widening the container alone would have made
the comparison a silent truncation and left the 2^32 alias in place while the
code claimed otherwise — the exact failure mode §8.2 identified and refused, and
the one `ASnapshotSharingOnlyItsLowThirtyTwoBitsIsStillRejected` now pins shut.

**Nine increment sites**, all unchanged in spelling (`++version_`) and all still
one instruction:

| Site | `BitArray.hpp` member | Note |
|---|---|---|
| 1 | `setLengthProperty` | after the negative-length check and the resize |
| 2 | `Set` | after the bounds check; bumps even for an equal-value write |
| 3 | `SetAll` | unconditional |
| 4 | `And` | after `requireSameLength` |
| 5 | `Or` | after `requireSameLength` |
| 6 | `Xor` | after `requireSameLength` |
| 7 | `Not` | unconditional |
| 8 | `LeftShift` | after the negative-count check; bumps for `count == 0` too |
| 9 | `RightShift` | after the negative-count check; bumps for `count == 0` too |

Plus the **implicitly declared** copy and move assignment operators, which reach
`BasicMutationCounter::operator=` and advance the destination by one. `BitArray`
declares no assignment operator of its own, so — unlike `LinkedList<T>` and its
#1769 guard — self-assignment advances too. There is **no `Clear()`, no `Add`,
and no non-const indexer**: `operator[]` is `const`-only, so `Set` is the sole
indexed write path and there is no `setItem` to track.

**Three read/compare sites**, all unchanged, all `==` only — nothing anywhere
compares the counter with `<`, `>` or subtraction, so no guard depends on
monotonicity across a wrap:

| Site | Expression |
|---|---|
| 1 | `Enumerator::Enumerator` — `version_(arr->version_)`, the snapshot |
| 2 | `Enumerator::MoveNext` — `requireUnmodified(version_ == arr_->version_)` |
| 3 | `Enumerator::Reset` — the same guard |

The production diff is two field declarations plus comments. Every increment and
every guard is spelled exactly as it was.

### 20.3 The pre-fix reproduction, taken before anything changed

`build-probe/1789_probe2_defects.cpp` — one source run against both headers, with
the pre-fix header extracted by `git show` into
`build-probe/1789_prefix-include/`. The counter is positioned with GCC's
`-fno-access-control`, which suppresses access checking and nothing else; no
macro is defined over a library header and no declaration is edited. No mode
performs more than a few dozen real mutations.

`build-probe/1789_prefix_defects.log`, the load-bearing lines verbatim:

```
counter-value-type-bytes=4
counter-max=4294967295
enumerator-snapshot-bytes=4
BitArray Set     snapshot=2 counter-2^32-later=2 truncated-onto-snapshot=1 guard-fired=0
BitArray Reset() guard-fired=0
BitArray 7x2^32  snapshot=2 counter=2 low-words-match=1 guard-fired=0
defects-observed=3
```

`build-probe/1789_postfix_defects.log`, same source, same modes:

```
counter-value-type-bytes=8
counter-max=18446744073709551615
enumerator-snapshot-bytes=8
BitArray Set     snapshot=2 counter-2^32-later=4294967298 truncated-onto-snapshot=0 guard-fired=1
BitArray Reset() guard-fired=1
BitArray 7x2^32  snapshot=2 counter=30064771074 low-words-match=1 guard-fired=1
defects-observed=0
```

The complete diff of the two logs is: the counter's width, those three
`guard-fired` outcomes, and one sentinel probe reaching a larger maximum.
**Every delta line and every `pre-horizon` line is byte-identical**, which is the
evidence that the mutation semantics did not move.

The distance is spelled `snapshot + 2^32` and never simply `snapshot`, and the
probe asserts `truncated-onto-snapshot` separately — because an assertion written
the second way holds for a counter of any width and pins nothing, which is the
weakness §19.11 found in #1787's own narrow branch. The third line is the
strongest form: at seven laps the low 32 bits still match (`low-words-match=1` on
**both** sides) and only the widened counter rejects it.

**No signed-overflow UB remained to fix, and none existed to begin with.**
`BitArray` is the one collection whose counter was already unsigned
(`std::uint32_t` before #1787, diverging from .NET's signed `int` at
`BitArray.cs:44`). The pre-fix probe under UBSan
(`1789_prefix_defects_ubsan.log`) reports **0 runtime errors** and exits 0;
post-fix is likewise 0.

Also reconfirmed pre-fix and unchanged post-fix: assignment already advanced the
destination's counter (`copy-assign before=2 after=3`), including with a matching
source counter and including self-assignment; no sentinel exists (six counter
positions including the maximum each enumerate the exact bit count and report the
exact `PopCount`); and there is no `Count` cache keyed on the counter —
`getLengthProperty()` derives from `bits_.size()` on every call.

### 20.4 The mutation-version delta matrix

Measured pre-fix and post-fix; **identical on both sides**. Pinned permanently by
`BitArrayVersionWideningTests.cpp`, not only by the probe.

| Operation | Δ version |
|---|---:|
| `Set(i, true)` on a `false` bit | +1 |
| `Set(i, false)` on a `true` bit | +1 |
| `Set(i, v)` where the bit **already holds `v`** | **+1** (unconditional — the pre-existing contract, matching .NET `BitArray.cs:341`) |
| `SetAll(true)`, `SetAll(false)` | +1 |
| `Not()` | +1 |
| `And`, `Or`, `Xor` with a same-length operand | +1 |
| `And(*this)` / `Or(*this)` — no bit changes | **+1** |
| `LeftShift(n)`, `RightShift(n)` for `n > 0` | +1 |
| `LeftShift(0)`, `RightShift(0)` — no bit changes | **+1** (matching .NET's `count <= 0` early return, `BitArray.cs:521`, `:584`) |
| `setLengthProperty` grow / shrink / shrink-to-zero | +1 |
| `setLengthProperty(currentLength)` — no change | **+1** (matching .NET `BitArray.cs:665`) |
| copy assignment `a = b` | +1 |
| move assignment `a = std::move(b)` | +1 |
| self copy assignment `a = a` | **+1** — no `if (this != &other)` guard; §6.2 |
| self move assignment `a = std::move(a)` | **+1**, and the backing `std::vector<bool>` is left valid but unspecified (libstdc++ empties it) — pre-existing standard-library behaviour, recorded rather than hidden |
| `Set` with an index `>= Length` or `< 0` (throws) | 0 |
| `setLengthProperty(-1)` (throws) | 0 |
| `LeftShift(-1)`, `RightShift(-1)` (throws) | 0 |
| `And`/`Or`/`Xor` with a different length (throws) | 0 |
| `Get`, `operator[]`, `getLengthProperty`, `getCountProperty` | 0 |
| `getIsReadOnlyProperty`, `getIsSynchronizedProperty`, `getSyncRootProperty` | 0 |
| `PopCount`, `HasAllSet`, `HasAnySet` | 0 |
| both `CopyTo` overloads, `Clone`, range-`for`, a full enumerator walk | 0 |

Copy **construction** inherits the source's counter (matching .NET's `Clone`
semantics for the counter field); `Clone()` does **not**, because it goes through
`BitArray(const std::vector<bool>&)` and default-constructs the counter at zero.
Both are pre-existing and both are now pinned.

The rows marked "no bit changes" are the pre-existing contract and #1789
deliberately did not revisit the mutation/no-op policy: this ticket changed a
width, nothing else. They are pinned by
`AMutationThatChangesNoBitStillAdvancesTheCounter`, which also asserts that the
bit pattern really was unchanged, so the case is the one being claimed.

### 20.5 Object layout — re-measured, not assumed

`build-probe/1789_probe1_layout.cpp`, one source run against both headers
(`1789_prefix_layout.log` → `1789_postfix_layout.log`), LP64, GCC 14.2,
libstdc++. Offsets are computed from a live object, because
`BitArray::Enumerator` is polymorphic and therefore not standard-layout.

| | pre | post |
|---|---|---|
| `sizeof(BitArray)` | 48 | **48** |
| `alignof(BitArray)` | 8 | **8** |
| `BitArray::bits_` offset (`vector<bool>`, 40) | 0 | **0** |
| `BitArray::version_` offset (width) | 40 (4) | **40 (8)** |
| `BitArray` tail padding | 4 | **0** |
| `sizeof(BitArray::Enumerator)` | 32 | **40** |
| `alignof(BitArray::Enumerator)` | 8 | **8** |
| `Enumerator` vptr offset | 0 | **0** |
| `Enumerator::arr_` offset (8) | 8 | **8** |
| `Enumerator::version_` offset (width) | 16 (4) | **16 (8)** |
| `Enumerator::index_` offset (`intcs`, 4) | 20 | **24** |
| `Enumerator::current_` offset (`bool`, 1) | 24 | **28** |
| `Enumerator::state_` offset (`EnumeratorState`, 4) | 28 | **32** |
| counter class `sizeof`/`alignof` | 4 / 4 | **8 / 8** |

Two things the table is worth reading carefully for:

1. **The container was free.** `version_` is the last member and sat at offset 40
   with four bytes of tail padding after it; the wider counter consumed exactly
   that. `bits_` keeps offset 0 and nothing moved. `sizeof(BitArray)` is 48
   before and after, which is why `PublishedObjectSizesAreUnchanged` still
   asserts 48 and is still telling the truth.
2. **The enumerator was not.** Its snapshot at offset 16 was immediately followed
   by `index_`; the three members after the snapshot need 4 + 1 + 4 = 9 bytes and
   only 8 were available, in any member order. Every member after the snapshot
   moved by 8. §8.2 predicted `sizeof` 32 → 40 and the prediction held exactly.

Type traits are unchanged: `BitArray` remains copy-constructible,
copy-assignable, move-constructible, nothrow-move-constructible,
non-polymorphic, non-trivially-copyable and standard-layout;
`BitArray::Enumerator` remains polymorphic, derived from `IEnumerator`,
copy-constructible and non-standard-layout. The vptr is still one 8-byte slot at
offset 0, and the vtable still holds the same four entries (destructor pair plus
`MoveNext`, `Reset`, `getCurrentProperty`). The calling convention of every
member is unchanged — no parameter or return type moved between register and
memory classes, because none of them changed at all.

### 20.6 Emitted symbols — zero `BitArray` symbols changed

`build-probe/1789_probe4_symbols.cpp` touches every public member of `BitArray`
and of its public nested `Enumerator`, directly and through `IEnumerator*`;
`nm --defined-only` is then diffed between the two header revisions.
**727 symbols on both sides.**

- **`BitArray`-named symbols: 64 pre, 64 post, and the sorted name lists are
  byte-identical** — 0 added, 0 removed, 0 renamed. No mangled name encodes a
  private field's width, which is precisely why this ABI break is silent.
- The only delta anywhere is 7 removed / 7 added, and all fourteen are the
  counter class's own weak inline members swapping instantiation:
  `BasicMutationCounter<unsigned int>`'s constructor (three aliases),
  both assignment operators, `operator++` and the conversion operator give way to
  `BasicMutationCounter<unsigned long>`'s.

```
- _ZN6System11Collections6detail20BasicMutationCounterIjEaSEOS3_
- _ZN6System11Collections6detail20BasicMutationCounterIjEaSERKS3_
- _ZN6System11Collections6detail20BasicMutationCounterIjEC1Ev
- _ZN6System11Collections6detail20BasicMutationCounterIjEC2Ev
- _ZN6System11Collections6detail20BasicMutationCounterIjEC5Ev
- _ZN6System11Collections6detail20BasicMutationCounterIjEppEv
- _ZNK6System11Collections6detail20BasicMutationCounterIjEcvjEv
+ _ZN6System11Collections6detail20BasicMutationCounterImEaSEOS3_   (and the six mirrors)
```

`BitArray` was the **last** production user of the `<unsigned int>`
instantiation, so after this ticket nothing in `modules/collections/include/`
emits it; only `CollectionVersionCounterTests.cpp`, which pins
`NarrowMutationCounter`'s behaviour, still names it.

### 20.7 Source compatibility — unaffected

No public signature, return type, parameter, or `const` qualification changed on
`BitArray` or `BitArray::Enumerator`. `getLengthProperty()`,
`getCountProperty()` and `PopCount()` still return a plain `intcs` by value;
`GetEnumerator()` still returns `IEnumerator*`; `And`/`Or`/`Xor`/`Not`/both
shifts still return `BitArray&`; `getCurrentProperty()` still returns an owning
`std::any`; `operator[]` is still `const`-only and still returns `bool` by value.
No new overload ambiguity: no overload set changed at all. No exception type,
message, or ordering changed.

In-repository caller impact: **every existing call site compiles unchanged, with
no edit anywhere.** The consumers are `BitArrayTests.cpp` (21 cases),
`Batch18CollectionsTests.cpp` and `Batch18bCollectionsTests.cpp` (the shift,
`PopCount`, `Clone`, enumerator and property gap-fills),
`CollectionsRemainingTests.cpp`, `EnumeratorCurrentSafetyTests.cpp` (#1793),
`EnumeratorLifecycleTests.cpp` (#1767), `CollectionVersionCounterTests.cpp`,
`modules/collections/tests/support/CollectionVersionSeam.hpp`, and the
`collections_mutation_version.cpp` consumer fixture. There is no production
consumer of `BitArray` inside this repository. The only test edits made were the
deliberate assertion changes listed in §20.10.

### 20.8 The stale-object probe — what the break actually looks like

This is the half that matters, because "requires a rebuild" is worth nothing
unless someone measured what skipping it does.
`build-probe/1789_stale_old_caller.cpp` is compiled against the **pre-fix**
header (`Enumerator` 32 bytes) and `1789_stale_main.cpp` against the current one
(40), and the two objects are linked into one program in **both orders**, at
`-O0` and `-O2`, with and without ASan+UBSan. Everything crossing the boundary is
`extern "C"` over `void*`, so the halves agree on the calling convention and
disagree only about the layout.

**The linker says nothing.** All eight link commands exit 0 with **empty**
diagnostic logs — no error, no warning. The two halves genuinely disagree:
`old-half-sizeof-Enumerator=32` against `new-half-sizeof-Enumerator=40`, while
`sizeof(BitArray)` reads 48 on both.

| Mode | `-O0`, link order A (new object first) | `-O0`, link order B (old first) | `-O2`, both orders |
|---|---|---|---|
| consumer struct **embedding** an `Enumerator` by value | sentinel **corrupted** `0xFEEDFACECAFEBEED` → `0xFEEDFACE00000002`, `intact=0`, and **no ASan report at all** | sentinel intact, appears healthy | corrupted in **both** orders |
| enumerator **heap-allocated by old code**, driven by new code | walks 8 of 8 bits | walks 8 of 8 bits | **reports 0 elements for an 8-bit array** — a silently wrong answer, no diagnostic |
| the same under ASan+UBSan, `-O2` | `new-delete-type-mismatch`: "size of the allocated type: 32 bytes; size of the deallocated type: 40 bytes" | order B/embedded **aborts** on an uncaught `ArgumentOutOfRangeException` from a bad index | — |
| mutation invalidation across the mismatch | `guard-fired=1` | `guard-fired=1` | `guard-fired=1` |

Four properties make this the worst kind of break, and all four are measured
rather than asserted:

1. **No diagnostic at link time, in any of the eight configurations.** Not from
   `ld`. This is the same class of miss #1800 measured for `-flto -Wodr`, ASan
   `detect_odr_violation=2` and UBSan.
2. **The embedded case corrupts a neighbouring member with no sanitizer report**,
   because the corrupted bytes are inside the same allocation — ASan cannot see
   it. That is silent memory corruption in a program that looks healthy. The
   corrupting value, `0x00000002`, is `EnumeratorState::Position::AfterLast`
   written at the new offset 32.
3. **At `-O2` a stale enumerator silently reports an empty array.** The old half
   inlines its own `GetEnumerator()` and constructs at the old offsets; the new
   half's inlined `MoveNext` reads `index_` and `state_` at the new ones and
   concludes immediately. Nothing throws and nothing is diagnosed.
4. **At `-O0` one of the two link orders appears to work.** A consumer who
   forgets to rebuild and happens to get link order B observes nothing wrong
   until the link line changes.

Notably, **the fail-fast guard itself keeps firing in every configuration**, so a
consumer cannot use "my enumerators still throw when I mutate" as evidence that
it rebuilt correctly.

The transcripts are `build-probe/1789_stale_O0.log`, `1789_stale_O2.log`,
`1789_stale_O0_asan.log` and `1789_stale_O2_asan.log`; the empty link logs are
`1789_stale_link{A,B}_*.log`.

**CNA and mobile-eggbert remain unmeasured.** They were not inspected, searched,
configured, built or modified, no claim is made about whether they use
`BitArray`, and ticket #1773 remains `blocked`.

### 20.9 Performance and allocation

`build-probe/1789_probe5_perf.cpp`, one source at `-O2 -DNDEBUG` against both
headers, with an `asm volatile` compiler barrier per iteration and replaced
global `operator new`/`delete` counting allocations. Seven **interleaved** paired
runs, median reported (`1789_perf_summary_paired.log`), then seven more as a
repeat (`1789_perf_summary_repeat.log`).

| Operation | pre (ns/op) | post (ns/op) | Δ | allocs pre | allocs post |
|---|---:|---:|---:|---:|---:|
| bit read `Get(i)` | 0.608 | 0.598 | −0.010 | 0 | 0 |
| bit write `Set(i, v)` | 1.056 | 1.043 | −0.013 | 0 | 0 |
| `SetAll` (1024 bits) | 2.025 | 1.994 | −0.031 | 0 | 0 |
| `Not` (1024 bits) | 1208.925 | 1212.403 | +3.478 | 0 | 0 |
| `And` (1024 bits) | 1026.737 | 1029.338 | +2.601 | 0 | 0 |
| `Or` (1024 bits) | 994.056 | 999.585 | +5.529 | 0 | 0 |
| `Xor` (1024 bits) | 1498.556 | 1500.262 | +1.706 | 0 | 0 |
| enumerator create + delete | 5.924 | 5.901 | −0.023 | 200000 | 200000 |
| enumerator create + 64 × `MoveNext` | 111.333 | 112.878 | +1.545 | 3125 | 3125 |
| copy construction (1024 bits) | 9.359 | 9.254 | −0.105 | 25000 | 25000 |
| copy assignment (1024 bits) | 2.491 | 2.264 | −0.227 | 0 | 0 |
| move assignment (1024 bits) | 25.567 | 25.133 | −0.434 | 50000 | 50000 |
| `LeftShift(1)` (1024 bits) | 1122.957 | 1127.227 | +4.270 | 0 | 0 |
| **`RightShift(1)` (1024 bits)** | **1038.547** | **1127.020** | **+88.473** | 0 | 0 |
| `setLengthProperty` toggle | 3.937 | 3.941 | +0.004 | 1 | 1 |

**Verdict: no measurable cost from the widening, and one row that needed
explaining rather than waving through.** Every row but one straddles zero, and
the **allocation counts are identical in every row** — the widening adds no
allocation on any path, which was requirement 10 of §6.

`RightShift(1)` is the exception, and it is *not* noise: fourteen paired runs give
pre 1028–1063 and post 1114–1142, two non-overlapping ranges. It is also **not
the counter**, and that was established rather than assumed:

1. `BitArray::RightShift`'s own generated code is **instruction-for-instruction
   identical** on both sides — 130 lines of `objdump` output each, byte-identical.
   The only codegen difference anywhere in the header is inside
   `BasicMutationCounter::operator++`, where `mov (%rax),%eax` / `mov %edx,(%rax)`
   become `mov (%rax),%rax` / `mov %rdx,(%rax)`, one byte longer each.
2. Recompiling **both** sides with `-falign-loops=32 -falign-functions=64` inverts
   the sign completely: `RightShift(1)` becomes pre 1306.607 / post 1109.200, the
   post side now 197 ns *faster* (`1789_perf_summary_aligned.log`). A difference
   that a pure alignment flag can move by 200 ns in either direction is a
   code-layout artefact of this particular `-O2` binary, not a property of the
   counter.

That is disclosed here in full rather than reported as "within noise", because
the first measurement genuinely reproduced and a reader who re-ran the probe
would have found it.

Memory cost, stated plainly rather than buried: **0 bytes per `BitArray` object**
(48 → 48; the counter grew into padding) and **+8 bytes per outstanding
enumerator** (32 → 40). An enumerator is a short-lived, singly-allocated object,
so the practical cost is 8 bytes per live iteration. Cache consequence: none
measurable — `BitArray` occupies the same 48 bytes and the enumerator still fits
comfortably inside one 64-byte line.

### 20.10 Permanent tests

A new focused suite,
`modules/collections/tests/System/Collections/BitArrayVersionWideningTests.cpp`,
**43 cases**, all near-boundary positioning done through the one authoritative
seam from #1800 (`modules/collections/tests/support/CollectionVersionSeam.hpp`,
which already carried a `BitArray` specialisation) — **no new explicit
specialisation body was written**, as CLAUDE.md requires. No test performs more
than a few hundred real mutations; the whole suite runs in 20 ms.

| Required coverage | Where |
|---|---|
| The counter is unsigned, 64-bit; the snapshot is exactly as wide | `TheCounterIsUnsignedAndSixtyFourBits`, `TheEnumeratorSnapshotIsExactlyAsWideAsTheCounter` |
| The approved object growth | `ThePublicEnumeratorGrewToFortyBytesOnLp64` (Enumerator 40 / align 8, `BitArray` still 48 / align 8) |
| No counter leaks through a public surface; traits unchanged | `NoCounterOrCounterTypeLeaksThroughAPublicSurface`, `ValueSemanticsTraitsAreUnchanged` |
| **No stale snapshot revalidated across the old 2^32 distance** | `NoStaleEnumeratorIsRevalidatedAcrossTheOld2Pow32Distance` (multiples 1, 2, 7, 1000), `ResetAlsoFailsFastAcrossTheOld2Pow32Distance` |
| **No low-32-bit ABA** — the truncation a container-only widening would have left | `ASnapshotSharingOnlyItsLowThirtyTwoBitsIsStillRejected`, which asserts the low words match, that the high words differ, *and* that the guard still fires |
| The horizon is 2^64, stated as a bound not an impossibility | `TheHorizonIsNowTwoToTheSixtyFourNotTwoToTheThirtyTwo` |
| The step that used to wrap | `TheStepThatUsedToWrapNowJustAdvances` (UINT32_MAX → 2^32, explicitly *not* 0) |
| Every named boundary position | `MutationAtEachNamedBoundaryValueMovesForwardExactly` — 0, 1, 2^31−2, 2^31−1, 2^31, 2^32−2, **2^32−1**, **2^32**, 2^32+5, 2^64−2 |
| Enumerator taken before, at, and after the transition | `AnEnumeratorTakenBeforeTheTransitionIsInvalidatedByIt`, `MutationPastTheOldBoundaryStaysExactAndKeepsInvalidating` (25 interleaved steps) |
| Every mutation family, at the boundary | `EveryMutationFamilyInvalidatesAcrossTheOldBoundary` — 11 members including both assignments |
| Exhaustion is defined | `ExhaustionWrapsToZeroWithoutUndefinedBehaviour` |
| No sentinel | `NoCounterValueIsReservedAsASentinel` (6 positions incl. the maximum) |
| The full delta matrix (§20.4) | `EveryEffectiveMutationAdvancesTheCounterByExactlyOne`, `AMutationThatChangesNoBitStillAdvancesTheCounter`, `EveryRejectedOrReadOnlyOperationAdvancesNothing`, `ARejectedMutationLeavesAnOutstandingEnumeratorValid` |
| Assignment, copy, move at the boundary | `CopyAssignmentAdvancesTheDestinationAcrossTheBoundary`, `MoveAssignmentAdvancesTheDestinationAcrossTheBoundary`, `AssignmentWithAMatchingSourceCounterStillInvalidates`, `SelfAssignmentAdvancesAsThisCollectionDocuments`, `SelfMoveAssignmentAdvancesAndIsMemorySafe`, `AssignmentDoesNotDisturbTheSourcesOwnEnumerators`, `MoveConstructionInheritsTheCounterAndTransfersTheBits` |
| **A copied array is independent**; `Clone` starts fresh | `AnIndependentCopyDoesNotInvalidateTheOriginalsEnumerator`, `CloneStartsAFreshCounterAtZero` |
| Ordinary sizes: empty, one bit, word boundaries, multi-word | `AnEmptyBitArrayEnumeratesToCompletionAndStaysExact`, `AMutationOnAnEmptyBitArrayInvalidatesItsEnumerator`, `ASingleBitBehavesAtTheBoundary`, `WordBoundarySizesEnumerateExactlyAtTheBoundary` (7/8/31/32/33/63/64/65/127/128/129) |
| Value patterns and a large bounded array | `EveryValuePatternSurvivesTheBoundary` (all-false, all-true, alternating, sparse), `ALargeBoundedBitArrayEnumeratesExactlyAcrossTheWrap` (100,000 bits, counter positioned to wrap mid-loop) |
| `CopyTo`, both overloads | `CopyToBothOverloadsStayExactAtTheBoundary` |
| The #1767 state machine and the #1793 owning `Current` | `TheEnumeratorStateMachineIsUnchangedAtTheBoundary`, `TheBoxedCurrentIsAnOwningCopyAtTheBoundary` |
| Interface enumeration and the directly named public `Enumerator` | `EnumerationThroughTheIEnumeratorInterfaceIsUnchanged`, `TheDirectlyNamedPublicEnumeratorBehavesLikeTheInterfaceOne`, `TwoEnumeratorsOverOneArrayAreIndependentUntilAMutation` |
| Reading never invalidates; `begin()`/`end()` still carry no snapshot | `ReadingWithoutMutatingNeverInvalidatesAtTheBoundary`, `ThePublicStlIteratorsStillCarryNoSnapshot` |

**Deliberate assertion changes** — these are the ones a reviewer should look at,
and there are exactly four:

1. `BitArrayAdapter::kNarrowCounter` in `CollectionVersionCounterTests.cpp`
   flipped `true` → `false`. That is the flip #1787 designed the flag for. It was
   **mutation-checked**: putting it back to `true` and rebuilding fails **two**
   tests — `TheCounterHasTheWidthItsLayoutPermits` *and*
   `NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance`
   (`build-probe/1789_mutation_check.log`). Only the first would have failed
   before #1788 strengthened the narrow branch (§19.11), so that correction is
   what made this flip load-bearing rather than cosmetic.
2. `sizeof(NG::BitArray) == 48` **stays** in `PublishedObjectSizesAreUnchanged`,
   because it genuinely did not change — re-measured, not assumed. A comment there
   now says why, and points at where the figure that *did* change lives.
   `sizeof(BitArray::Enumerator)` was never in `PublishedIteratorSizesAreUnchanged`;
   a comment now records that it is deliberately absent and pinned in the new
   suite instead.
3. Comments in `CollectionVersionCounterTests.cpp` and `MutationCounter.hpp` were
   updated so none of them still describes `BitArray` as narrow, and
   `NarrowMutationCounter`'s doc-comment now states plainly that **no collection
   uses it any more**.
4. `BitArray.hpp` gained a class-level and an `Enumerator`-level documentation
   block stating the versioning contract, the layout change, the mandatory
   rebuild, the remaining 2^64 horizon, and the absence of any thread-safety
   guarantee.

`test/consumer/collections_bitarray_version.cpp` is a new **tracked** consumer
fixture, compiled against only the public `Collections.Core` surface with
`-Wall -Wextra -Wpedantic -Werror` and **run**, exercising ordinary construction
through all four constructors, every mutating member, every argument rejection,
the fail-fast contract across nine mutation families, copy/`Clone`/assignment
independence, all iteration forms, the #1767 lifecycle guard, the #1793 owning
`Current`, the directly named public `Enumerator`, and the 40-byte enumerator a
consumer can observe. It deliberately does **not** reach the counter: no public
accessor exists and this ticket added none.

### 20.11 Sanitizers

| Check | Result |
|---|---|
| ASan + UBSan + LSan over `SharpRuntimeTests_Collections_Core` | **2,637/2,637 pass, zero reports**, exit 0 (`build-asan/1789_collections_core_asan.log`), run with `detect_leaks=1:detect_odr_violation=2` |
| UBSan, `1789_probe2_defects`, pre-fix | **0 runtime errors** — `BitArray` never had signed-overflow UB |
| UBSan, `1789_probe2_defects`, post-fix | **0 runtime errors** |
| ASan+UBSan+LSan `1789_probe3_sanitizers`, `boundary-mutation` | `failures=0`, no diagnostic — ten lengths from 0 to 1023, each crossing UINT32_MAX and then the 2^64 wrap |
| … `enumerator-invalidation` | `failures=0` — 11 mutation families with the enumerator mid-walk, including a `setLengthProperty` that **shrinks** under it |
| … `copy-move-assign` (incl. self-copy and self-move) | `failures=0`, no diagnostic |
| … `retained-current` (64 owning `std::any` boxes outliving the array) | `failures=0`, no diagnostic |
| … `large-bitarray` (**200,000 bits**, counter positioned to wrap mid-loop) | `failures=0`, no diagnostic |
| **LeakSanitizer proved active** | the bounded self-test leaks deliberately and reports **96 bytes in 3 allocations**, exit 1 — including a "Direct leak of **40** byte(s) in 1 object(s)", which is the enumerator at its new size |
| ASan, stale-object probe | `new-delete-type-mismatch` (32 allocated / 40 deallocated) and an abort, reproduced as designed (§20.8) |

Each probe mode runs as its own process so no abort can hide another.

**TSan was not run, and that is a decision rather than an omission.** This ticket
adds no atomic, widens none, introduces no `mutable` cache and no hidden `const`
write; the counter is a plain non-atomic field of a different width, read and
written at the same three sites. TSan has nothing new to find. **No thread-safety
guarantee follows from this ticket** — concurrent mutation of a `BitArray` is
unsupported before and after, exactly as #1787 §14.3 states, and
`getIsSynchronizedProperty()` still returns `false`.

### 20.12 Gates

Everything below ran from a **fresh configuration plus a clean-first rebuild**,
which the enumerator-layout change makes mandatory: any object file left over
from before the change would disagree about `sizeof(BitArray::Enumerator)` and be
an ODR violation inside the test binaries themselves.

| Gate | Result |
|---|---|
| `cmake --fresh -S . -B build` then `cmake --build build --clean-first --parallel 3` | see §20.12 table in the ticket report |
| `scripts/run_component_tests.sh build` | **13,923** tests across **37** executables (13,880 before; +43, exactly the new suite) |
| `SharpRuntimeTests_Collections_Core` | **2,637** (2,594 before) |
| `scripts/validate_module_boundaries.py --root .` | 41 physical modules / 90 dependency edges — unchanged, no new edge |
| `test/validate_module_boundaries_test.py` | 7/7 |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |
| `scripts/check_version_seam_odr.py` | 2 seams / **18** specialisation definitions — unchanged, no seam added |
| `test/check_version_seam_odr_test.py` | 12/12 |
| `scripts/check_negative_consumer_fixtures.py` | **8 fixtures / 51 sites, every site rejected** — unchanged; no negative fixture was added, and none was needed because no public signature changed |
| `test/check_negative_consumer_fixtures_test.py` | 37/37 |
| `scripts/check_selective_components.sh` | all ten components pass |
| `check_selective_components.sh Collections.Core collections_bitarray_version.cpp` | passes in isolation, fixture exits 0 |
| `scripts/check_doxygen_warnings.sh` | **1,941** of the 1,942 ceiling — **unchanged** |
| `git diff --check` | clean |

The Doxygen count is deliberately **unchanged at 1,941**. `Doxyfile` scans the
module include trees and `README.md` only, not `docs/`, so every markdown link
from `README.md` into `docs/` costs one unresolvable `\ref` warning — including a
second link to an already-linked document, which is how #1788 spent its +1. This
ticket's new `README.md` entry therefore refers to this document as an inline
code span rather than a markdown link, and reuses the existing link in the #1788
entry directly below it. That is a deliberate choice to stay off the ceiling, and
it is disclosed here rather than presented as a free lunch.

### 20.13 Residual limitations, stated not hidden

| # | Residual | Position |
|---|---|---|
| 1 | The ABA horizon is now 2^64, **not infinity** | Over 580 years of uninterrupted mutation of one instance at an implausibly generous 10^9/s. It is modular arithmetic and `TheHorizonIsNowTwoToTheSixtyFourNotTwoToTheThirtyTwo` asserts the wrap explicitly rather than pretending it cannot happen. Guarding it would cost a branch on every mutation (§7 C/F). |
| 2 | `sizeof(BitArray::Enumerator)` grew 25%, 32 → 40 | Approved, measured (§20.5, §20.9), and unavoidable — §7 H is arithmetically impossible here: nine bytes are needed after the snapshot and eight are available, in any member order. |
| 3 | **A stale object file links silently and can corrupt memory or silently report an empty array** | §20.8. No linker diagnostic in any of eight configurations; no ASan report for the embedded case; a wrong answer rather than a crash at `-O2`. The mandatory full rebuild is the only mitigation, and `README.md` says so in those terms. |
| 4 | `Set`, both zero-count shifts, `And(*this)` and a same-length `setLengthProperty` still bump although no bit changes | Pre-existing contract, matching .NET, deliberately not revisited — this ticket changed a width. Now pinned by `AMutationThatChangesNoBitStillAdvancesTheCounter` so the next reader sees it is a decision. |
| 5 | Self **move** assignment leaves the backing `std::vector<bool>` valid but unspecified | Pre-existing standard-library behaviour, not introduced or fixable here. Recorded and pinned as memory-safe by `SelfMoveAssignmentAdvancesAndIsMemorySafe` rather than left unstated. |
| 6 | `begin()`/`end()` are still not version-checked | Pre-existing, documented, deliberate, and pinned by `ThePublicStlIteratorsStillCarryNoSnapshot`. |
| 7 | No thread-safety guarantee | The counter is non-atomic before and after. TSan not run, with reasons (§20.11). `getIsSynchronizedProperty()` still returns `false`. |
| 8 | The `RightShift(1)` benchmark row moved by ~8% | Attributed to `-O2` code alignment, not to the counter, and demonstrated by inverting the sign with an alignment flag (§20.9). Disclosed rather than filed under "noise". |
| 9 | `detail::NarrowMutationCounter` now has no user | Kept as the historical record and as the counter template's second instantiation for the tests that pin its behaviour; its doc-comment says plainly that no collection uses it and that none should. Removing it was outside this ticket's scope. |
| 10 | CNA and mobile-eggbert are **unmeasured** | Not inspected, searched, configured, built or modified. #1773 stays `blocked`. |

### 20.14 Rollback

`git revert` of the implementation commit restores the 32-bit counter and, with
it, the 2^32 ABA horizon. A revert must also flip `BitArrayAdapter::kNarrowCounter`
back to `true` and remove or invert the new suite — otherwise the permanent tests
fail, which is the intended safety property. Validate a revert by re-running
`build-probe/1789_probe2_defects.cpp` against both headers and confirming it
reports `defects-observed=3` again, and by a **fresh clean-first rebuild**, since
the enumerator layout moves back.
