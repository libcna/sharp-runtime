<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Enumerator `Current` safety design

*Design, evidence, and decision record for ticket **#1792**
(`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M, category `defect`, area
`Collections`). Recorded 2026-07-28 on local branch
`feature/remediation-coll-ienumerator-current-design`. This ticket carries **no
`SR-AUD-*` identifier** — the audit numbering is frozen at 364 and this defect
was found during remediation, by ticket #1790's mutable-access inventory
(`docs/ListIndexerVersioningDesign.md` §4.1 route 6, §5.2, §27.3).*

***This ticket changes no production behaviour, no public signature, no object
layout, and no exception. It is design-only.*** The implementation it selects is
ticket **#1793**, opened `blocked` pending the explicit approval stated verbatim
in §33.

---

## 1. Executive decision

`System::Collections::IEnumerator::getCurrentProperty()` returns `void*`. The
generic bridge fills it with `const_cast<T*>(&Current())`. Those two facts
together mean that **a consumer holding nothing but the public non-generic
interface can obtain a writable, untyped, unbounded-lifetime pointer into the
live storage of the collection it is walking** — including collections whose own
members refuse to be mutated.

The selected architecture is **Alternative B: the non-generic accessor returns
`std::any` by value.** It is chosen because it is the only candidate that closes
*all six* defect classes at once, and because it is not an invention — it is the
direct C++ spelling of .NET's own `object IEnumerator.Current`, which returns a
value, boxes value types, and hands out no pointer at all.

The decisive measurement is §11's, and it is a refutation of the obvious answer:

| Candidate | A consumer can still mutate live storage |
|---|---|
| baseline `void*` | **yes** |
| A — `const void*` | **yes**, one `const_cast` away |
| D — read-only descriptor (pointer + `type_info` + size) | **yes**, one `const_cast` away |
| G — enumerator-owned copy behind `const void*` | no |
| **B — `std::any` by value (SELECTED)** | **no** |

**`const void*` is not a fix.** It is a *notice*. Any candidate that still
returns an address into collection storage is defeated by a `const_cast` the
caller writes in one line, and the probe does exactly that
(`probe4_alternatives.log`). Only the two candidates that stop returning an
address into live storage — G and B — actually close the write path. Between
them, B costs no object layout and adds runtime type validation; G costs
`sizeof(T)` on every enumerator in the repository and still has no type tag.

Four of this ticket's own premises are corrected rather than inherited, and all
four are corrections *against* this record's convenience:

1. **The defect does not reach "every collection in the repository."** The
   ticket description, `NEXT.md`, and the per-file audit note all say it does.
   It does not. `Dictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, and
   `SortedDictionary<K,V>` implement **no** `IEnumerator` at all — they expose
   STL-style version-checked `begin()`/`end()` iterators. The measured reach is
   **thirteen** generic enumerator implementations plus **eight** non-generic
   ones, plus two hand-written test-local implementers (§5) — not "every
   collection" (§2.1).
2. **The `const_cast` in `Generic/IEnumerator.hpp` is not the only one, and not
   the worst one.** Eight further implementations override
   `getCurrentProperty()` directly, never touching that bridge. Two of them
   (`Hashtable::MemberCollection::MemberEnumerator`,
   `ListDictionaryInternal`) carry their own independent `const_cast`, and one
   of those publishes a writable pointer to a **live `std::unordered_map`
   key** — reproduced as a broken container invariant, not merely a changed
   value (§6.5).
3. **The defect is not one defect.** Six materially different failure modes
   share the signature, and three of the eleven generic implementations are
   *not* affected by the storage-aliasing one at all, because their `Current()`
   returns enumerator-owned snapshot storage (§7, §8).
4. **The most dangerous property is not the source break; it is the ABI.** Under
   the Itanium C++ ABI a non-template function's return type is **not** part of
   its mangled name. `void*`, `const void*`, and `std::any` all produce the
   byte-identical symbol `_ZNK…18getCurrentPropertyEv`, while the calling
   convention differs — `this` moves from `%rdi` to `%rsi` under `std::any`'s
   sret return. **A partially rebuilt consumer links silently and corrupts
   memory** (§21.2). No candidate avoids this; the design's answer is to make
   the requirement explicit rather than to pretend it away.

---

## 2. Ticket and defect taxonomy

Ticket #1792, key `REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, priority **P3**,
size **M**, source path
`modules/collections/include/System/Collections/Generic/IEnumerator.hpp`.

The reported shape is real and is reproduced verbatim in §6.1. What the ticket
calls one defect is six, and they are separated here because they have different
scopes, different fixes, and different residual risk.

| Class | Name | What it is | Reproduced in |
|---|---|---|---|
| **A** | **Const-correctness breach** | `Current()` returns `const T&` specifically so an enumerator cannot mutate what it walks; the bridge `const_cast`s that away and republishes the *same object* as a writable `void*` on a public interface. | §6.1, §6.7 |
| **B** | **Mutation/version bypass** | A write through that pointer changes a live element while the owning collection's `MutationCounter` stays at rest, so every outstanding enumerator remains valid and silently observes the new value. | §6.1, §6.2, §6.8 |
| **C** | **Type-safety breach** | `void*` carries no type, no size, and no alignment. Every cast a consumer makes is unchecked; a same-width wrong cast is silently wrong with no sanitizer diagnostic, and a wrong-type *write* corrupts the element's value representation. | §6.6 |
| **D** | **Lifetime breach** | The interface documents no validity window whatsoever. The pointer outlives `MoveNext()`, `Reset()`, reallocation, `Clear()`, the enumerator, and the collection — four AddressSanitizer `heap-use-after-free` reports. | §6.4 |
| **E** | **Ownership ambiguity** | The pointer refers to live collection storage in eight implementations, to enumerator-owned snapshot storage in three, to an enumerator-owned `mutable` cache in two, to the *caller's own* object in one, and is the element *by value* in two. Nothing in the signature distinguishes them, so no consumer can know which lifetime rule applies. | §7, §8 |
| **F** | **Generic/non-generic inconsistency** | The typed and type-erased accessors on the *same enumerator, at the same instant*, designate the same object with opposite mutability. The repository's own `IAsyncEnumerator<T>` gets this right (`const T&`); the synchronous one does not. | §6.7, §9.3 |

These are deliberately not collapsed into one "unsafe pointer" statement,
because §11 shows they are not closed by the same measures: `const void*` closes
A only; a descriptor closes A and C only; an owned copy closes A, B, D, E;
`std::any` closes all six.

### 2.1 Corrected scope — which collections are actually affected

The ticket says the defect "affects EVERY collection in the repository". It does
not, and the correction matters because it changes the migration estimate.

| Collection | Enumeration surface | Affected |
|---|---|---|
| `Generic::List<T>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`, `SortedList<K,V>` | `IEnumerator<T>*` from `GetEnumerator()` | **yes** |
| `ObjectModel::Collection<T>`, `ObjectModel::ReadOnlyCollection<T>` | `IEnumerator<T>*` | **yes** |
| `Concurrent::ConcurrentBag/Queue/Stack<T>`, `BlockingCollection<T>` | `IEnumerator<T>*` (snapshot) | **yes**, class E only |
| `Runtime::CompilerServices::ConditionalWeakTable<K,V>` | `IEnumerator<Pair>*` | **yes**, class E only |
| `ArrayList`, `Hashtable`, `Queue`, `Stack`, `BitArray`, `ListDictionaryInternal` | direct `IEnumerator` override | **yes** |
| **`Generic::Dictionary<K,V>`** | version-checked `begin()`/`end()` **only** | **no** |
| **`Generic::HashSet<T>`** | version-checked `begin()`/`end()` **only** | **no** |
| **`Generic::SortedSet<T>`** | version-checked `Iterator` **only** | **no** |
| **`Generic::SortedDictionary<K,V>`** | version-checked `Iterator` **only** | **no** |

The four hash- and tree-backed generic collections — including every type
tickets #1782 through #1787 worked on — expose no `IEnumerator` at all. That is
verified by compiling against the tagged shim, not by grepping: none of them
appears anywhere in `sweep_callsites.log`.

---

## 3. Exact current declarations

`modules/collections/include/System/Collections/IEnumerator.hpp:100`:

```cpp
[[nodiscard]] virtual void* getCurrentProperty() const = 0;
```

with the doc-comment *"Pointer to the current element; cast to the appropriate
type."* — which is the whole of the type contract.

`modules/collections/include/System/Collections/Generic/IEnumerator.hpp:33,41`:

```cpp
[[nodiscard]] virtual const T& Current() const = 0;          // :33

void* getCurrentProperty() const override {                   // :38-41  <-- the defect
    return const_cast<T*>(&Current());
}
```

`modules/collections/include/System/Collections/IDictionaryEnumerator.hpp:36,43`
— the sibling interface, already const-correct, and the repository's own
precedent that `const void*` is the conventional spelling here:

```cpp
[[nodiscard]] virtual const void* getKeyProperty() const = 0;
[[nodiscard]] virtual const void* getValueProperty() const = 0;
```

`modules/collections-async/include/System/Collections/Generic/IAsyncEnumerator.hpp:37`
— the asynchronous sibling, which got it right:

```cpp
[[nodiscard]] virtual const T& getCurrentProperty() const = 0;
```

Measured layout, LP64/GCC 14.2/libstdc++, `probe5_abi.log`. Every number below is
unchanged by this ticket and is recorded so #1793 has a stored baseline:

| Type | `sizeof` | `alignof` | polymorphic |
|---|---:|---:|:---:|
| `System::Collections::IEnumerator` | 8 | 8 | yes |
| `Generic::IEnumerator<int>` | 8 | 8 | yes |
| `Generic::IEnumerator<std::string>` | 8 | 8 | yes |
| `Generic::List<int>` / `<std::string>` | 40 | 8 | yes |
| `Generic::Queue<int>` | 88 | 8 | no |
| `ObjectModel::Collection<int>` | 32 | 8 | yes |
| `ObjectModel::ReadOnlyCollection<int>` | 24 | 8 | yes |
| `Collections::ArrayList` | 40 | 8 | yes |
| `Collections::Hashtable` | 72 | 8 | yes |
| `Collections::BitArray` | 48 | 8 | no |

`Generic::IEnumerator<T>` has **no data members** — `sizeof` is one vtable
pointer regardless of `T`. That is the fact Alternative G would change and B
would not (§11.4).

---

## 4. Interface hierarchy inventory

```
System::Collections::IEnumerable                 GetEnumerator() -> IEnumerator*
   ^
   |  (public, non-virtual base)
System::Collections::Generic::IEnumerable<T>     GetEnumerator() -> IEnumerator<T>*   [covariant override]

System::Collections::IEnumerator                 MoveNext(), Reset(), getCurrentProperty() -> void*
   ^                       ^
   |                       |
   |                    System::Collections::IDictionaryEnumerator
   |                       getEntryProperty() -> DictionaryEntry     [by value]
   |                       getKeyProperty()   -> const void*         [const-correct]
   |                       getValueProperty() -> const void*         [const-correct]
   |
System::Collections::Generic::IEnumerator<T>     Current() -> const T*  [typed, const]
                                                 getCurrentProperty()   [bridge, MUTABLE void*]
```

| Declaration | Return | `const` member | Virtual | Visibility | Result mutable | Points at |
|---|---|:---:|:---:|---|:---:|---|
| `IEnumerator::getCurrentProperty()` | `void*` | yes | pure | public | **yes** | implementation-defined |
| `IEnumerator<T>::Current()` | `const T&` | yes | pure | public | no | implementation-defined |
| `IEnumerator<T>::getCurrentProperty()` | `void*` | yes | override | public | **yes** | the object `Current()` returns |
| `IDictionaryEnumerator::getKeyProperty()` | `const void*` | yes | pure | public | no | implementation-defined |
| `IDictionaryEnumerator::getValueProperty()` | `const void*` | yes | pure | public | no | implementation-defined |
| `IDictionaryEnumerator::getEntryProperty()` | `DictionaryEntry` | yes | pure | public | n/a — **by value** | nothing (a copy) |
| `IAsyncEnumerator<T>::getCurrentProperty()` | `const T&` | yes | pure | public | no | implementation-defined |

Two observations that bear directly on the design:

- **`getEntryProperty()` already returns by value.** The by-value answer this
  design selects is not foreign to the interface family; it is already how the
  richest accessor on the sibling interface works.
- **The naming is inconsistent, and #1793 must not make it worse.**
  `IAsyncEnumerator<T>` spells its *typed* accessor `getCurrentProperty()`
  (CLAUDE.md rule 5). The synchronous `IEnumerator<T>` spells the typed one
  `Current()` and reserves `getCurrentProperty()` for the type-erased bridge. The
  two interfaces therefore give the same name to two different things. This is
  recorded, and deliberately **not** fixed here: renaming either would be a
  second, unrelated public break (§30, risk 7).

---

## 5. Complete implementation inventory

Every class in the repository that overrides `getCurrentProperty()`, measured by
compiling against the tagged shim and by reading each body — not by grepping.

### 5.1 Through the generic bridge (eleven implementations)

These never write `getCurrentProperty()` themselves; they inherit
`Generic::IEnumerator<T>`'s bridge, so what the pointer designates is entirely
decided by their `Current()`.

| # | Enumerator | Header | `Current()` returns | Class |
|---|---|---|---|:---:|
| 1 | `Generic::List<T>::Enumerator` | `Generic/List.hpp:100` | `list_->items_[index_]` | **live** |
| 2 | `Generic::Queue<T>::Enumerator` | `Generic/Queue.hpp:75` | `queue_->queue_[index_]` | **live** |
| 3 | `Generic::Stack<T>::Enumerator` | `Generic/Stack.hpp:75` | `stack_->stack_[index_]` | **live** |
| 4 | `Generic::LinkedList<T>::Enumerator` | `Generic/LinkedList.hpp:473` | `cur_->item` (heap node) | **live** |
| 5 | `Generic::SortedList<K,V>::Enumerator` | `Generic/SortedList.hpp:77` | `it->second` (map value) | **live** |
| 6 | `ObjectModel::Collection<T>::Enumerator` | `ObjectModel/Collection.hpp:53` | `items_[index_]`, `items_` is a `const std::vector<T>&` borrowed from the collection | **live** |
| 7 | `ObjectModel::ReadOnlyCollection<T>::Enumerator` | `ObjectModel/ReadOnlyCollection.hpp:56` | `(*items_)[index_]`, `items_` is a co-owned `shared_ptr<vector<T>>` | **live, and the type is declared read-only** |
| 8 | `Concurrent::ConcurrentBag<T>::SnapshotEnumerator` | `Concurrent/ConcurrentBag.hpp:60` | `items_[index_]`, `items_` is the enumerator's own `std::vector<T>` | copy |
| 9 | `Concurrent::ConcurrentQueue<T>::SnapshotEnumerator` | `Concurrent/ConcurrentQueue.hpp:59` | as 8 | copy |
| 10 | `Concurrent::ConcurrentStack<T>::SnapshotEnumerator` | `Concurrent/ConcurrentStack.hpp:57` | as 8 | copy |
| 11 | `BlockingCollection<T>::SnapshotEnumerator` | `Concurrent/BlockingCollection.hpp:70` | as 8 | copy |
| 12 | `BlockingCollection<T>::ConsumingEnumerable::Enumerator` | `Concurrent/BlockingCollection.hpp:576` | `current_`, a `T` member | copy |
| 13 | `ConditionalWeakTable<K,V>::Enumerator` | `CompilerServices/ConditionalWeakTable.hpp:91` | `current_`, a `Pair` member | copy |
| 14 | test-local `VecEnumerator` | `Generic/GenericInterfacesTests.cpp:25` | `data_[idx_]` | **live** |

Fourteen rows — thirteen production, one test-local — of which **eight publish
live collection storage** and six publish enumerator-owned copies. Row 14 is a
hand-written implementer in ordinary consumer style: direct evidence, as in
#1790 §6.3, that downstream code implements these interfaces by hand.

### 5.2 Direct non-generic overrides (eight production, one test-local)

These never reach the generic bridge.

| # | Enumerator | Header | Body | Class |
|---|---|---|---|:---:|
| 15 | `ArrayList::Enumerator` | `ArrayList.hpp:755` | `const_cast<std::any*>(&list_->_items[index_])` | **live**, own `const_cast` |
| 16 | `Hashtable::Enumerator` | `Hashtable.hpp:401` | `&current_`, a `mutable DictionaryEntry` | cache |
| 17 | `Hashtable::MemberCollection::MemberEnumerator` | `Hashtable.hpp:475` | `const_cast<void*>(inner_->getKeyProperty()/getValueProperty())` | **live map key or value**, own `const_cast` |
| 18 | `BitArray::Enumerator` | `BitArray.hpp:391` | `&current_`, a `mutable bool` | cache |
| 19 | `Collections::Stack::Enumerator` | `Stack.hpp:232` | `s_->s_[…]` — the element **is** a `void*` | by value |
| 20 | `Collections::Queue::Enumerator` | `Queue.hpp:243` | `q_->q_[…]` — the element **is** a `void*` | by value |
| 21 | `ListDictionaryInternal::NodeEnumerator` | `ListDictionaryInternal.hpp:77` | `const_cast<void*>(getKeyProperty())` — the **caller's** key | caller-owned, own `const_cast` |
| 22 | `ListDictionaryInternal::MemberCollection::Enumerator` | `ListDictionaryInternal.hpp:116` | `const_cast<void*>(inner_->getKey/getValue)` | caller-owned, own `const_cast` |
| 23 | test-local `IntVectorEnumerator` | `Interfaces2Tests.cpp:21` | test storage | test |

**Four independent `const_cast`s exist outside `Generic/IEnumerator.hpp`.**
Repairing only the bridge, as the ticket's summary implies, would leave rows 15,
17, 21, and 22 exactly as they are.

### 5.3 Names that are *not* this interface

Recorded so the inventory cannot be mistaken for exhaustive over the string
`getCurrentProperty`. These are unrelated accessors that happen to share the
name and are **out of scope**: `Text::StringRuneEnumerator`,
`Text::StringBuilderRuneEnumerator`, `Text::RunePosition`,
`Text::StringBuilder::ChunkEnumerator`, `Core::CharEnumerator`,
`Core::SpanSplitEnumerator`, `Core::Delegate`'s invocation-list enumerator,
`Globalization::TextElementEnumerator`, `Xml::XPath::XPathNodeIterator`,
`Threading::SynchronizationContext` (a `static`), `Buffers::ReadOnlySequence`,
and `Generic::IAsyncEnumerator<T>`. None derives from
`System::Collections::IEnumerator`; all return by value or by `const` reference
already.

---

## 6. Pre-fix reproduction

All probes live in the repository-local, gitignored `build-probe-ienumerator/`
tree. **No production or test source was modified to produce any of this
evidence** — the committed headers are the pre-fix headers, because this ticket
changes no behaviour. `probe1_current.cpp` is built with `-fno-access-control`
**only** so it can read each collection's private mutation counter and state
what the counter did; that flag suppresses access checking and nothing else, and
no macro is defined over a library header. `probe3` and `probe4` are additionally
compiled in `strict` mode — `-Wall -Wextra -Wpedantic -Werror`, **without**
`-fno-access-control` — so their signature assertions hold for an ordinary
consumer translation unit.

Every log separates `invariants-failed` (**0 in every mode**, in every run below
— otherwise the probe itself would be wrong) from `defects-observed`.

### 6.1 The ticket's own shape — `probe1_list.log`

```
[list] Generic::List<int> -- the ticket's own reproduction
  void*-write   element 10 -> 88   counter 3 -> 3   fail-fast=0
    DEFECT: a write through IEnumerator::getCurrentProperty() changed LIVE List<int>
            storage and the mutation counter never moved
    DEFECT: the outstanding fail-fast enumerator stayed valid across that write
  control Add() fail-fast=1
invariants-failed=0
defects-observed=2
```

Read exactly: the returned `void*` compares equal to `&Current()` **and** to
`&list.items_[0]`, so it aliases live storage rather than a copy; the write
landed; the counter did not move; the enumerator that was mid-walk stayed valid.
The `control Add()` line is what makes the rest evidence rather than noise — the
guard works, and only this route bypasses it.

### 6.2 One collection per storage family — `probe1_families.log`

```
  Queue<int>            head=77 counter 2 -> 2 fail-fast=0
  Stack<int>            top=66  counter 2 -> 2 fail-fast=0
  LinkedList<int>       first=55 counter 2 -> 2 fail-fast=0
  SortedList<int,int>   v[1]=44 counter 2 -> 2 fail-fast=0
  OM::Collection<int>   [0]=33  (type has NO mutation counter) fail-fast=0
  ConcurrentQueue<int>  head=1 (snapshot: collection UNCHANGED) enumerator-copy=22
invariants-failed=0
defects-observed=5
```

Five families, five identical outcomes — contiguous vector, reverse cursor, heap
node, associative value, and a type that **has no mutation counter at all**, so
there is nothing that could have moved and no fail-fast guard to bypass. The
sixth line is the honest counterexample: `ConcurrentQueue`'s snapshot enumerator
absorbs the write into its own copy and the collection is untouched. That is
class E, not class B, and conflating them would overstate the defect.

### 6.3 A read-only type mutated through its own enumerator — `probe1_readonly.log`

```
  non-const operator[] NotSupportedException=1
  enumerator void*     ro[0]="MUTATED"  backing[0]="MUTATED"
    DEFECT: ReadOnlyCollection<std::string>: the READ-ONLY contract is bypassed
    DEFECT: the write also reached the caller's shared backing vector
invariants-failed=0
defects-observed=2
```

`ReadOnlyCollection<T>`'s non-const indexer, `Add`, `Insert`, `Remove`, and
`Clear` all throw `NotSupportedException("Collection is read-only.")`. Its
enumerator hands out a writable pointer into the same storage, and because
`items_` is a co-owned `shared_ptr<std::vector<T>>`, the write is visible to the
caller's own vector and to every other view sharing it. This is the single most
serious functional consequence in the report: **the type's entire reason to
exist is defeated through its own public enumeration path.**

### 6.4 Lifetime — `probe2_asan_*.log`

Six shapes, each its own process so every AddressSanitizer report is
attributable, all under ASan+UBSan with `-fno-omit-frame-pointer -g`.

| Mode | What is retained | Diagnostic |
|---|---|---|
| `after-movenext` | pointer, across `MoveNext()` | **no memory error** — silently aliases the *previous* element while `Current()` reports the new one |
| `after-reset` | pointer, across `Reset()` | **no memory error** — a *new* `getCurrentProperty()` correctly throws, and the *old* pointer keeps writing |
| `realloc` | `int*`, across 64 `Add()`s | **heap-use-after-free, WRITE of size 4** |
| `clear` | `std::string*`, across `Clear()` | **heap-use-after-free, READ of size 1** |
| `destroy-collection` | `int*`, across the collection's destruction | **heap-use-after-free, READ of size 4** |
| `destroy-enumerator` | `std::string*`, across `delete enumerator` | **heap-use-after-free, READ of size 8** (`ConcurrentQueue` snapshot) |

**Four AddressSanitizer `heap-use-after-free` reports.** Two qualifications,
both against this record's convenience:

- In the `clear` case, reading `s->size()` produces **no** ASan report and
  returns the stale 64: `std::vector::clear()` destroys the elements but keeps
  its buffer, so the string's control block still sits inside a live allocation.
  The lifetime has ended and the read is undefined behaviour all the same — it is
  simply not one any sanitizer here detects. The report above comes from the
  *following* line, `s->c_str()[0]`, which reaches the character buffer
  `~basic_string` released. Both lines are in the log, in that order, labelled.
  This is the same qualification #1790 §5.3 recorded.
- In the `destroy-enumerator` case, the **`ReadOnlyCollection` half does not
  fault**: its storage is `shared_ptr`-owned, so the pointer legally outlives its
  enumerator. The `ConcurrentQueue` half, which owns its snapshot outright,
  faults on the identical expression. *That difference is class E.* Two
  implementations of one interface give opposite answers to "may I keep this
  pointer after the enumerator dies?", and the signature does not say which.

The first two rows matter as much as the four crashes: they show the interface
has **no validity window at all**. `Reset()` correctly refuses to hand out a new
`Current`, and does exactly nothing about the pointer it already handed out.

### 6.5 The non-generic implementations — `probe1_nongeneric.log`

```
  ArrayList            [0] now holds NSt7__cxx1112basic_stringIcE  counter 2 -> 2 fail-fast=0
    DEFECT: the const_cast<std::any*> lets a caller replace a live element AND change
            its dynamic type, with the counter at rest
  Hashtable keys view  key "beta" -> "CORRUPTED"  ContainsKey(old)=0 ContainsKey(new)=1  Count=2
    DEFECT: writing through the key view's void* rewrote a LIVE std::unordered_map key
            in place; the entry is no longer reachable by the key it was inserted under
  Hashtable 64 entries key "key61" -> "zzz-not-a-real-key"  ContainsKey(old)=0
                        ContainsKey(new)=0  Count=64
    DEFECT: with enough entries that the two names cannot share a bucket, the rewritten
            entry is unreachable by BOTH its old and its new key while Count still
            reports it -- the container invariant is broken, not merely a value changed
  Hashtable enumerator Entry.key after write="spoofed"  table still has "alpha"=1
    NOTE: the write landed in the enumerator's own `mutable DictionaryEntry current_`
  BitArray             cached Current=0  ba.Get(0)=1
    NOTE: enumerator-owned `mutable bool current_`
  Collections::Stack   Current == &b : 1 (element IS the void*; no const_cast)
  Collections::Queue   Current == &a : 1 (element IS the void*)
  ListDictionaryInternal Current == &key : 1  (const std::string* laundered to void*)
    DEFECT: getCurrentProperty() const_casts the CALLER's own `const void*` key
invariants-failed=0
defects-observed=4
```

The `Hashtable` pair is the strongest evidence in this report and needed two
runs to state honestly. With two entries, the corrupted key was still findable
by its **new** name — pure bucket luck, because libstdc++ uses few buckets at
that size. Repeated with 64 entries, so the two names cannot share a bucket, the
entry became unreachable by **both** names while `Count` still reported 64. A
`std::unordered_map` whose key was rewritten in place is not a container with a
wrong value in it; it is a container whose invariant no longer holds.

`ListDictionaryInternal` is a different breach again: the pointer is the
*caller's own* `const void*` key, laundered through `const_cast`. Writing
through it modifies an object the caller declared `const` — genuinely undefined
behaviour if that object is const-qualified, and a change to the dictionary's key
identity in every case.

The two `NOTE:` lines are counterexamples, kept because they are: `Hashtable`'s
own enumerator and `BitArray`'s point at `mutable` caches, so a write
desynchronises the enumerator from the collection without touching the
collection. Class E, not class B.

### 6.6 Type erasure — `probe3_asan_*.log`

Bounded and sanitizer-controlled: the wrong-type reads are sized so they stay
inside a live allocation ASan is watching. Nothing dereferences an arbitrary or
unrelated pointer.

```
  as int32_t   : 0x41424344
  as float     : 12.1414   (same width, silently wrong, no diagnostic)
  as int64_t   : 0x4546474841424344  (reads element 0 AND element 1 as one value)
  after writing 1.0f through the void*: list[0] = 0x3f800000  counter=2
defects-observed=2
```

A same-width wrong cast produces a silently wrong value that **no sanitizer
reports**, and a wrong-type *write* through the same pointer rewrites the
element's value representation with the counter at rest. The `alignment` mode
records that the returned pointer happens to be correctly aligned because it
aliases real storage, and that nothing in the signature promises it.

### 6.7 The const breach in its smallest form — `probe3_asan_constness.log`

```
  &Current() == getCurrentProperty() : 1  -- the SAME object, one const, one not
  after the write, the const reference reads 42
```

One enumerator, one instant, one object, reachable as `const int&` and as a
writable `void*`. The `const` qualification on `Current()` is documentation, not
enforcement.

One thing this probe explicitly does **not** claim: there is no reproduction of a
write to a genuinely `const`-qualified element, because `GetEnumerator()` is
non-`const` on every collection in this repository, so a `const` collection
cannot produce an enumerator at all. Class A is therefore a hazard **of the
signature**, and is stated as such rather than overclaimed.

### 6.8 Consequences for other observers — `probe1_versioning.log`

```
  second enumerator     fail-fast=0  (sees 99)
  native begin()        *it=1
    DEFECT: a second enumerator created BEFORE the void* write is still accepted,
            and silently observes the new value
  control after Add()   fail-fast=1
```

The bypass is not local to the enumerator that performed the write: an
independent enumerator created *before* it stays valid and silently observes the
new value. The control line confirms the guard still fires for a real mutation,
so this is a bypass, not a broken guard.

### 6.9 UndefinedBehaviorSanitizer

All six `probe1` modes under `-fsanitize=undefined -fno-sanitize-recover=all`:
**exit 0, 0 runtime errors, every mode.** The routes contain no undefined
behaviour *of their own* — the defect is a missing guarantee, not bad arithmetic.
Stated so the absence is on the record rather than assumed.

### 6.10 LeakSanitizer

`probe2` in `lsan` mode on the two non-faulting shapes: **0 leaks.** The
ownership-counted mode of `probe1_nontrivial` independently confirms `live 0 -> 0`
across whole-element replacement through the `void*`, so neither the probe nor
the defect leaks or double-destroys an element.

---

## 7. Ownership and lifetime, as they stand today

The single table the interface should have had, reconstructed by reading every
implementation. **Nothing in the signature distinguishes any two rows.**

| Storage | Implementations | Pointer valid until | Write reaches the collection | Counter moves |
|---|---|---|:---:|:---:|
| live contiguous vector | List, Queue, Stack, ObjectModel::Collection, ArrayList | the next reallocating or erasing operation — **unbounded and undetectable** | **yes** | **no** |
| live heap node | LinkedList | the node is removed | **yes** | **no** |
| live associative value | SortedList | the entry is erased | **yes** | **no** |
| live shared vector, read-only wrapper | ReadOnlyCollection | the last co-owner drops it | **yes, defeating the type's contract** | n/a (no counter) |
| live map key | Hashtable member view (keys) | the entry is erased | **yes, breaking the container invariant** | **no** |
| enumerator-owned snapshot | ConcurrentBag/Queue/Stack, BlockingCollection | the **enumerator** is destroyed | no | n/a |
| enumerator-owned single value | BlockingCollection consuming, ConditionalWeakTable | the next `MoveNext()` | no | n/a |
| enumerator-owned `mutable` cache | Hashtable::Enumerator, BitArray | the next `MoveNext()`/`Reset()` | no, but desynchronises the enumerator | n/a |
| caller-owned object | ListDictionaryInternal | the **caller** decides | writes the caller's object | **no** |
| the element **is** the pointer | Collections::Stack, Collections::Queue | n/a — returned by value | n/a | n/a |

Ten distinct lifetime rules behind one signature. That is class E, and it is why
"document it" (Alternative H) is not a remediation: there is no single sentence
that could be written in the header and be true.

---

## 8. Classification of every implementation

Against the six categories the ticket's design step asked for:

| Category | Implementations | Count |
|---|---|---:|
| 1. exposes live mutable collection storage | List, Queue, Stack, LinkedList, SortedList, ObjectModel::Collection, ObjectModel::ReadOnlyCollection, test `VecEnumerator` | 8 |
| 2. exposes const storage through a mutable cast | ArrayList (`const_cast<std::any*>`), Hashtable member view, `ListDictionaryInternal` ×2 — the four `const_cast`s that live **outside** the bridge | 4 |
| 3. exposes enumerator-owned copy storage | ConcurrentBag/Queue/Stack, BlockingCollection ×2, ConditionalWeakTable, Hashtable::Enumerator, BitArray | 8 |
| 4. exposes boxed or type-erased storage | ArrayList (`std::any` elements), Hashtable member view (values are `std::any`) | 2 (overlaps 1–2) |
| 5. returns a temporary or unstable address | **none found** | 0 |
| 6. already safe | Collections::Stack, Collections::Queue (the element *is* the `void*`; nothing is aliased and nothing is cast) | 2 |
| 7. unrelated false positive | the twelve accessors of §5.3 | 12 |

Category 5 is worth its own line: **no implementation returns the address of a
temporary.** That was checked for explicitly, because a design that assumed it
would have justified a heavier fix than the evidence supports.

---

## 9. .NET comparison

Read from the local current .NET sources, not from memory.

### 9.1 The two interfaces

`System.Private.CoreLib/src/System/Collections/IEnumerator.cs`:

```csharp
public interface IEnumerator
{
    bool MoveNext();
    object Current { get; }     // #nullable disable
    void Reset();
}
```

`System.Private.CoreLib/src/System/Collections/Generic/IEnumerator.cs`:

```csharp
[Intrinsic]
public interface IEnumerator<out T> : IDisposable, IEnumerator
    where T : allows ref struct
{
    new T Current { get; }
}
```

| Question | .NET | This port |
|---|---|---|
| Non-generic `Current` type | `object` — **by value** | `void*` — **a mutable pointer** |
| Generic `Current` type | `T` — **by value** | `const T&` |
| Variance | **`out T`** — the type system *forbids* `T` in any input position | n/a; C++ has no interface variance |
| Relationship | generic **hides** (`new`) the non-generic | generic **overrides** the non-generic |
| Setter | **none, on either** | none, but the returned pointer is writable |
| Value types | **boxed** into a fresh `object` on each read | aliased in place |
| Reference types | the handle is returned; mutating the *object* is not mutating the *collection*, so `_version` correctly stays put | n/a |
| Can a caller mutate the collection through it? | **no** | **yes** |
| Before first `MoveNext` / after end | *"The returned value is undefined"* | throws `InvalidOperationException` |

**`out T` is the load-bearing detail.** Covariance is only legal when `T` never
appears in an input position, so .NET's type system makes a mutable `Current`
*inexpressible*, not merely absent. This port has no such mechanism, so the same
guarantee has to be bought with the representation.

### 9.2 How concrete .NET enumerators implement it

Every one of them caches a **copy**.

| Type | Field | Non-generic `Current` |
|---|---|---|
| `List<T>.Enumerator` (`List.cs:1191, 1219, 1226-1237`) | `private T? _current` | returns `_current`, boxed; throws `InvalidOperation_EnumOpCantHappen` when `_index <= 0` |
| `ArrayList` (`ArrayList.cs:2576, 2606-2624`) | `private object? _currentElement` | returns it; throws `InvalidOperation_EnumNotStarted` or `InvalidOperation_EnumEnded`, distinguished by `_index` |
| `Dictionary<K,V>.Enumerator` (`Dictionary.cs:1923-1939`) | `private KeyValuePair<TKey,TValue> _current` | **constructs a brand-new** `DictionaryEntry` or `KeyValuePair` on every read |
| `Dictionary.KeyCollection` / `ValueCollection` (`:2161`, `:2354`) | `_currentKey` / `_currentValue` | returns the cached copy |

Note the asymmetry .NET itself carries and this port already diverges from:
`List<T>.Enumerator`'s **generic** `Current` does *not* throw outside a valid
position — it returns `default`. Only the **non-generic** one throws. This port
throws on both, which is a deliberate, already-documented deviation recorded
under SR-AUD-356 / ticket #1767: "this C++ API returns `const T&`; throwing
follows the non-generic invalid-state contract and avoids inventing an unsafe
reference." **This design keeps that deviation** (§18).

### 9.3 The exact messages

From `Strings.resx`, and this port already matches all four byte for byte:

| Resource | Value |
|---|---|
| `InvalidOperation_EnumNotStarted` | `Enumeration has not started. Call MoveNext.` |
| `InvalidOperation_EnumEnded` | `Enumeration already finished.` |
| `InvalidOperation_EnumOpCantHappen` | `Enumeration has either not started or has already finished.` |
| `InvalidOperation_EnumFailedVersion` | `Collection was modified; enumeration operation may not execute.` |

### 9.4 .NET's non-boxable case — the precedent for `NotSupportedException`

The generic interface's own source comment:

> *NOTE: An implementation of an enumerator using a `ref struct T` will not be
> able to implement `IEnumerator.Current` to return that `T` (as doing so would
> require boxing). It should throw a `NotSupportedException` from that property
> implementation.*

This is decisive for the selected design's one behavioural addition. A C++ `T`
that is not copy-constructible is the exact analogue of a `ref struct`: it cannot
be boxed. .NET's answer is `NotSupportedException` from the non-generic property,
with the typed one still working. §14 adopts that answer verbatim.

### 9.5 The explicitly unsafe escape hatch

`System.Runtime.InteropServices.CollectionsMarshal` — *"An **unsafe** class that
provides a set of methods to access the underlying data representations of
collections"* — is where .NET puts `AsSpan<T>(List<T>)` and
`GetValueRefOrNullRef`, with the hazard documented at the call site. .NET does
have untracked mutable access to collection storage; it simply refuses to put it
on `IEnumerator`, names it unsafe, and quarantines it in a different assembly
surface. That is the split this design adopts, and it is the same one #1790 §8.2
adopted for `ToVector()`.

### 9.6 What C++ cannot reproduce

- **No common object root.** .NET's `object` is a real type every value can be
  boxed into. C++ has no such type, so the boxing has to be library-provided —
  which is why `std::any` is the counterpart, not an approximation of one.
- **No interface variance.** `out T` cannot be expressed, so "the type system
  forbids mutation" is unavailable and the representation must supply it.
- **No GC.** A managed boxed copy has no lifetime question at all. A C++ copy
  has one, which is why "by value" — rather than "a pointer to a copy" — is what
  actually removes the question (§11.4).

---

## 10. C++ constraints that bound the solution space

1. **`const` is not enforcement across a `void*`.** `const_cast` is one line and
   is not diagnosable. Any candidate whose safety rests on the caller not writing
   it has no safety property at all — *measured*, not argued (§11.1).
2. **A return type is not part of the Itanium mangled name.** Changing it
   changes no symbol, so the linker cannot detect the mismatch (§21.2). This is
   the single most dangerous constraint in the list.
3. **A covariant return applies to pointers and references only.** `void*` →
   `const void*` is *not* covariant (the qualification conversion goes the wrong
   way for an override), so every override must change together; `void*` →
   `std::any` is not covariant either. Either way this is an all-at-once
   migration of all nine direct overrides plus the bridge.
4. **`std::any` requires `is_copy_constructible_v<T>`.** A move-only `T` cannot
   be boxed. This is the C++ analogue of .NET's `ref struct` case, and .NET's own
   documented answer applies (§9.4).
5. **Adding a data member to a class template consumers derive from is an object-layout
   break.** That is why Alternative G costs what it costs (§11.4).

---

## 11. Alternatives evaluated

Every candidate was implemented as a self-contained miniature of the real
two-level hierarchy and compiled side by side in `probe4_alternatives.cpp` under
`-Wall -Wextra -Wpedantic -Werror`, then as a fully migrated header shim
compiled against the whole repository (§12).

### 11.1 The measurement that decides it — `probe4_alternatives.log`

```
  baseline (void*)         mutates-collection=YES  sizeof(ITyped<int>)=8   allocs/read int=0 string=0  move-only=compiles           runtime-type-check=impossible
  A const void*            mutates-collection=YES  sizeof(ITyped<int>)=8   allocs/read int=0 string=0  move-only=compiles           runtime-type-check=impossible
  B std::any (by value)    mutates-collection=no   sizeof(ITyped<int>)=8   allocs/read int=0 string=2  move-only=throws NotSupported  runtime-type-check=std::any::type()
  D descriptor             mutates-collection=YES  sizeof(ITyped<int>)=8   allocs/read int=0 string=0  move-only=compiles           runtime-type-check=typeid compare
  G enumerator-owned copy  mutates-collection=no   sizeof(ITyped<int>)=16  allocs/read int=0 string=1  move-only=throws NotSupported  runtime-type-check=impossible

  Read-only-contract check: can a consumer reach LIVE storage at all?
    baseline    address == &v[0] : 1
    A           address == &v[0] : 1
    D           address == &v[0] : 1
    G           address == &v[0] : 0
    B           returns no address at all; std::any holds a copy
```

`mutates-collection` is measured by doing exactly what a determined consumer
does: `const_cast` whatever guard the signature has, then write. **A and D
fail that test.** They are notices, not fixes.

### 11.2 The alternatives

| # | Alternative | Verdict |
|---|---|---|
| **A** | **`const void*`** | **Rejected as a complete fix; adopted as a fallback (§29).** It closes class A and *nothing else*: `const_cast` restores the write (measured), the pointer still aliases live storage so classes D and E are untouched, and `void*` still carries no type so class C is untouched. It is the cheapest candidate — the smallest diff, no allocation, no layout change — and it is the repository-conventional spelling (`IDictionaryEnumerator` already uses it). If §33's approval is declined, this is the right partial step, and it should then be described as a *narrowing*, never as a remediation. |
| **B** | **`std::any` by value** | **SELECTED.** The only candidate that closes all six classes. It is the direct counterpart of .NET's `object Current`, it is the representation this component already uses for boxed values (`ArrayList`, `Hashtable`, `DictionaryEntry`, and #1771's `CopyTo(std::vector<std::any>&)`), it needs no new type and no new module edge, it costs **zero** object layout, and `std::any::type()` gives the runtime type check class C needs and no other candidate but D provides. Costs: 28 measured call sites migrate; a silent ABI break (§21.2); 2 allocations per read for `std::string` and 0 for `int`; move-only `T` throws on the non-generic path only. |
| **C** | **Owning type-erased box (a bespoke `Object`/`BoxedValue` type)** | Rejected. It is Alternative B with a hand-written `std::any`. It would need a new public type in `Core` or `Collections`, its own copy/move/allocation policy, its own type-identity mechanism, and its own tests — to arrive at what `<any>` already provides, is already used by four types in this component, and already carries the small-buffer optimisation the allocation numbers depend on. The only argument for it is control over the SBO threshold, which is not worth a new public type. |
| **D** | **Read-only type-erased reference descriptor** (`const void*` + `const std::type_info*` + size, with a checked `tryAs<T>()`) | Rejected, and it is the most interesting rejection. It closes class C **properly** — `tryAs<float>()` on an `int` element correctly returns `nullptr`, measured — and it is the only candidate besides B that does. But it still hands out an address into live storage, so `const_cast` restores the write (measured: `mutates-collection=YES`) and the lifetime question is untouched. A "lifetime token" was considered as an extension and rejected: making it real needs a per-collection generation word the descriptor can compare, which is a second object-layout change to every collection, and it would still only *detect* a stale pointer rather than prevent one. `sizeof(CurrentRef)` is 24 — three times the current return — for a property it does not achieve. |
| **E** | **Remove `getCurrentProperty()` from `Generic::IEnumerator<T>`** | Rejected as a standalone answer; **partially adopted inside B**. Removing the *bridge* only makes `IEnumerator<T>` abstract in that slot, so all eleven implementations must write the accessor themselves — eleven chances to reintroduce the same `const_cast`, instead of one place that cannot. Keeping one correct bridge is strictly better. What is adopted from E is its insight that the generic and non-generic accessors are different operations that should not share a body by accident: under B the bridge *converts* rather than *aliases*. |
| **F** | **Typed `Current()` returns `T` by value** | Rejected. It is the closest to .NET (`T Current { get; }`) and it would close the `&Current()` retention hazard at the root. But it breaks **all 27 measured typed call sites**' zero-copy property, forces a copy on every read of every element of every collection in the hot enumeration path, and — decisively — makes move-only `T` *uninstantiable* rather than merely unboxable, because the pure virtual itself would require a copy. B confines the same restriction to the non-generic path, where .NET confines it too. |
| **G** | **Enumerator-owned stable copy behind `const void*`** | Rejected, narrowly, and it is the runner-up. It closes A, B, D, and E, it is exactly .NET's `_currentElement` model, and it preserves the pointer *shape* so some call sites survive. Two things decide against it. First, **object layout**: the box has to live in `Generic::IEnumerator<T>`, which today has no data members, so `sizeof` goes 8 → 16 for `int` and 8 → **48** for `std::string` (measured), on a class template consumers derive from. Second, it leaves class C entirely open — still a bare pointer, still no type tag — and it *adds* a subtle new hazard B does not have: the returned pointer silently changes meaning on the next non-generic read, so two `getCurrentProperty()` calls can return the same address holding different values. B has no address to go stale. |
| **H** | **Document it as an explicitly unsafe escape, as .NET does for `CollectionsMarshal`** | Rejected. The comparison does not hold. `CollectionsMarshal` is a *separate, differently named surface* a caller must deliberately reach for; `IEnumerator::getCurrentProperty()` is the ordinary way non-generic code reads an element. More decisively, §7 shows there is **no single true sentence** to write: ten distinct lifetime rules hide behind one signature, so the documentation would have to be per-implementation, on an interface whose whole purpose is that the implementation is unknown. And documentation cannot be a remediation for four reproduced use-after-frees and a broken `std::unordered_map` invariant. |

### 11.3 Compatibility matrix

✅ good, ⚠️ partial, ❌ bad. Scored against every criterion the ticket asked for.

| Criterion | baseline | A `const void*` | **B `std::any`** | C bespoke box | D descriptor | E remove | F by-value `Current` | G owned copy | H document |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Class A — const correctness | ❌ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ | ✅ | ❌ |
| Class B — mutation/version bypass | ❌ | ❌ | ✅ | ✅ | ❌ | ⚠️ | ✅ | ✅ | ❌ |
| Class C — type safety | ❌ | ❌ | ✅ | ✅ | ✅ | ❌ | ✅ | ❌ | ❌ |
| Class D — lifetime | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ | ⚠️ | ⚠️ | ❌ |
| Class E — ownership ambiguity | ❌ | ❌ | ✅ | ✅ | ⚠️ | ❌ | ⚠️ | ✅ | ❌ |
| Class F — generic/non-generic parity | ❌ | ⚠️ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ❌ |
| Value-type behaviour | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ |
| Reference-like (`shared_ptr`) `T` | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ |
| Move-only `T` | ✅ compiles (unsafely) | ✅ | ⚠️ throws | ⚠️ throws | ✅ | ✅ | ❌ uninstantiable | ⚠️ throws | ✅ |
| Non-trivial `T` | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ |
| Public source compatibility | ✅ | ❌ | ❌ | ❌ | ❌ | ❌❌ | ❌❌ | ❌ | ✅ |
| Virtual **ABI** compatibility | ✅ | ❌ silent | ❌ silent | ❌ silent | ❌ silent | ❌ | ❌ | ❌ silent | ✅ |
| Vtable **slot layout** | ✅ | ✅ same slot | ✅ same slot | ✅ | ✅ | ❌ slot removed | ✅ | ✅ | ✅ |
| Object layout | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ 8→16/48 | ✅ |
| Allocation per read (`int`) | 0 | 0 | **0** | 0 | 0 | — | 0 | 0 | 0 |
| Allocation per read (`std::string`) | 0 | 0 | **2** | ~2 | 0 | — | 1 | 1 | 0 |
| New module dependency | ✅ none | ✅ none | ✅ none (`<any>` already used) | ❌ new public type | ✅ none | ✅ none | ✅ none | ✅ none | ✅ none |
| Testability | ⚠️ | ⚠️ | ✅ | ✅ | ✅ | ⚠️ | ✅ | ⚠️ | ❌ |
| .NET parity | ❌ | ❌ | ✅ **exact** | ✅ | ❌ | ⚠️ | ✅ | ✅ | ❌ |

### 11.4 Why B rather than G, stated once

Both close the write path. The comparison is:

| | B `std::any` | G owned copy |
|---|---|---|
| `sizeof(Generic::IEnumerator<int>)` | **8 → 8** | 8 → 16 |
| `sizeof(Generic::IEnumerator<std::string>)` | **8 → 8** | 8 → **48** |
| Class C (type safety) | closed | **open** |
| Returned thing can go stale | **no** — it is a value | yes — the address is reused on the next read |
| Allocations per `std::string` read | 2 | **1** |
| .NET counterpart | `object Current` | `_currentElement` |

G is cheaper per read and closes one class fewer at the cost of an object-layout
change to a class template consumers derive from. B costs one extra allocation
for large `T` and closes everything. Given that this is a *correctness* ticket
whose reproduction includes a broken container invariant, the extra allocation
is the right trade; §26 records the measurement so it can be revisited.

---

## 12. Measured repository-wide source break

Method: `build/compile_commands.json` (626 entries, 626 distinct translation
units) replayed with `-fsyntax-only` and the candidate shim first on the include
path, 4 parallel processes, no `nproc` and no `hardware_concurrency` anywhere.
Every shim is **regenerated from the committed headers on every run** and aborts
with `SHIM DRIFT` if a header it patches has changed, so it cannot silently drift.

Each shim **migrates all nine direct overrides, the bridge, and the three
in-library call sites**. That is deliberate: #1790 §10.1 recorded that a shim
leaving implementers unmigrated inflates the count with cascade errors measuring
a skipped step rather than the design.

### 12.1 Call-site inventory — `sweep_callsites.log`

Measured against a `[[deprecated]]`-tagged shim rather than grepped, as the
ticket's acceptance criteria require.

| Route | Unique sites (file:line) | Where |
|---|---:|---|
| `IEnumerator::getCurrentProperty()` (non-generic) | **28** | 3 in library headers, 25 in tests |
| `IEnumerator<T>::getCurrentProperty()` (generic bridge) | **4** | all in tests |
| `IEnumerator<T>::Current()` (typed) | **27** | all in tests |
| **Total unique** | **59** | |
| compile failures | **0 / 626** | |

The three library-header sites are the `IEnumerable`-consuming constructors that
drain an enumerator into raw storage: `ArrayList.hpp:108`, `Queue.hpp:47`,
`Stack.hpp:47`. They migrate with the interface and are counted as such.

**One site is outside this measurement and must be added by hand:**
`test/consumer/collections_dictionary_views.cpp:81`
(`if (walk->getCurrentProperty() == nullptr) return false;`). The consumer
fixtures are compiled by `scripts/check_selective_components.sh`, not by the main
build, so they are absent from `compile_commands.json`. Under B it becomes
`if (!walk->getCurrentProperty().has_value()) return false;`. Stated explicitly
because a sweep that silently omitted it would understate the break.

### 12.2 Break per candidate — `sweep_breaks.log`

Each shim migrates all eight production overrides, the bridge, and the three
in-library call sites, then the whole repository is recompiled against it.

| Shim | TUs that stop compiling | Distinct error sites | Nature of the breaks |
|---|---:|---:|---|
| `shim-constvoid` (A) | **6 / 626** | **12** | 11 `casts away qualifiers` / `invalid conversion` in test sources; 1 `invalid covariant return type` on the hand-written implementer |
| **`shim-any` (B — SELECTED)** | **7 / 626** | **14** | 12 in repository test sources; 2 at `vendor/googletest/gtest.h` instantiation points, which are test assertions comparing the result to a pointer or to `nullptr` |
| `shim-ownedcopy` (G) | **6 / 626** | **12** | byte-identical to A — G also returns `const void*`, so its extra cost is entirely the object-layout change, which `-fsyntax-only` cannot see |

Three results matter more than the totals.

1. **Zero library sources break, under every candidate.** All 609 `modules/`
   sources plus the vendor and other units compile. Because the shims replace the
   real headers, a wrong migration in §14.3 or §14.4 would have failed the header
   itself and broken every translation unit that includes it. **The proposed
   bodies in §14 are therefore compile-validated, not sketched.**
2. **28 call sites, 14 breaks.** Most of the difference is
   `EXPECT_THROW((void)e->getCurrentProperty(), …)` and other discarded-value
   uses, which still compile under `std::any`. Only sites that *cast* or
   *compare* the result break. That is worth stating plainly: the migration is
   smaller than the call-site count suggests, and it is smaller for the honest
   reason that half the call sites never looked at the value.
3. **The one non-test break in every column is the same one** —
   `Interfaces2Tests.cpp:28`, the hand-written `IntVectorEnumerator`. Under A and
   G it is `invalid covariant return type`; under B it is `conflicting return
   type specified`. That single line is the whole consumer-migration story, and
   it confirms §10.3: `void*` → `const void*` is **not** a covariant return, so
   there is no candidate under which implementers migrate lazily.

The two `gtest.h` rows under B deserve one more sentence, because they are the
only diagnostics not reported at a repository line. They are
`EXPECT_EQ(e->getCurrentProperty(), &a)` (`QueueStackTests.cpp:93`, `:95`,
`:97`) and `EXPECT_NE(walk->getCurrentProperty(), nullptr)`
(`DictionaryKeyAndViewContractTests.cpp:233`), whose `operator==`/`operator!=`
fails to resolve for `std::any`. They are ordinary call-site breaks; only their
reported location is inside the vendored header, because that is where the
comparison is instantiated.

### 12.3 The measurement's limits, stated

- The sweep measures **this repository only**. CNA and mobile-eggbert were not
  inspected, searched, configured, built, or modified, by instruction. The
  figures below are **not** offered as a proxy for theirs.
- A `[[deprecated]]` tag fires at *instantiation*, so a use inside a template
  that no translation unit instantiates is invisible to it. Nothing here is
  claimed to cover uninstantiated templates.
- `-fsyntax-only` does not link, so nothing here measures a *symbol* break; §21
  covers that separately, and it is the more dangerous half.

---

## 13. Selected architecture

### 13.1 The split, stated once

The non-generic `IEnumerator` becomes a **read-only, by-value, type-checked**
view of the current element, exactly as `object IEnumerator.Current` is in .NET.
It stops being a window into storage.

The typed `Generic::IEnumerator<T>::Current()` is **unchanged**: it keeps
returning `const T&`, because it is already const-correct, because it is the
zero-copy read path 27 measured call sites use, and because changing it would
make move-only `T` uninstantiable (§11.2 F). Its residual hazard — a caller who
retains `&Current()` across a mutation — is real, is reproduced in §6.4, and is
**not closed by this design**. It is instead given, for the first time, a written
validity window (§16), which is a documentation change requiring no approval and
is therefore in Phase 1.

That asymmetry is the design, and it mirrors #1790's exactly: one tracked,
safe, ordinary surface, and one explicitly documented sharp edge, with the line
between them written down.

### 13.2 Phases

| Phase | Content | Break | Approval |
|---|---|---|---|
| **1** | Documentation only: write the ownership/lifetime/validity rules of §7 and §16 into both headers; record the `Current()` reference window; correct the "cast to the appropriate type" comment to say what it actually does not promise. | **none** | **not required** |
| **2** | `getCurrentProperty()` returns `std::any` by value; migrate the bridge, all nine direct overrides, the three in-library call sites, the 28 measured consumer sites, and the consumer fixture. | source + **silent ABI** | **required (§33)** |

---

## 14. Exact proposed public declarations

These are what #1793 implements; it should not need to redesign the contract.

### 14.1 `System/Collections/IEnumerator.hpp`

```cpp
#include <any>

/**
 * @brief Gets the current element in the collection, as a boxed copy.
 *
 * C++ counterpart of .NET IEnumerator.Current, which returns `object` -- a
 * value, and for a value type a boxed copy. The returned std::any OWNS its
 * value: it is unaffected by any later MoveNext(), Reset(), mutation of the
 * collection, destruction of the enumerator, or destruction of the collection,
 * and writing to it cannot reach the collection.
 *
 * Recover the value with std::any_cast<T>(); the boxed type is exactly the
 * element type the enumerator was declared over, and std::any::type() can be
 * queried when it is not known statically.
 *
 * @return A copy of the current element, boxed.
 * @throws System::InvalidOperationException if iteration has not started
 *         ("Enumeration has not started. Call MoveNext.") or has already
 *         finished ("Enumeration already finished.").
 * @throws System::NotSupportedException if the element type cannot be copied
 *         and therefore cannot be boxed. Matches .NET's documented behaviour
 *         for a `ref struct` element type; the typed accessor still works.
 */
[[nodiscard]] virtual std::any getCurrentProperty() const = 0;
```

### 14.2 `System/Collections/Generic/IEnumerator.hpp`

```cpp
// UNCHANGED -- still const, still by reference, still the zero-copy read path.
[[nodiscard]] virtual const T& Current() const = 0;

/**
 * @brief Boxes the current element for the non-generic interface.
 *
 * Converts rather than aliases: the returned std::any holds a COPY, so no
 * caller of the non-generic interface can reach the collection's storage.
 * Current()'s own state machine runs first, so the before-start and after-end
 * exceptions are unchanged.
 */
std::any getCurrentProperty() const override {
    if constexpr (std::is_copy_constructible_v<T>) {
        return std::any(Current());
    } else {
        throw System::NotSupportedException(
            "The element type cannot be boxed; use the typed Current() accessor.");
    }
}
```

### 14.3 The nine direct overrides

| Implementation | Today | Phase 2 |
|---|---|---|
| `ArrayList::Enumerator` | `const_cast<std::any*>(&list_->_items[i])` | `return list_->_items[i];` — the element already **is** a `std::any`; the copy is the whole change |
| `Hashtable::Enumerator` | `&current_` (`mutable DictionaryEntry`) | `return std::any(current_);` — and `current_` need no longer be `mutable` |
| `Hashtable::…::MemberEnumerator` | `const_cast<void*>(inner_->getKey/Value)` | keys: `std::any(*static_cast<const std::string*>(inner_->getKeyProperty()))`; values: `*static_cast<const std::any*>(inner_->getValueProperty())` |
| `BitArray::Enumerator` | `&current_` (`mutable bool`) | `return std::any(current_);` — `mutable` no longer needed |
| `Collections::Stack::Enumerator` | the stored `void*` | `return std::any(s_->s_[…]);` — recovered with `std::any_cast<void*>` |
| `Collections::Queue::Enumerator` | the stored `void*` | `return std::any(q_->q_[…]);` |
| `ListDictionaryInternal::NodeEnumerator` | `const_cast<void*>(getKeyProperty())` | `return std::any(getKeyProperty());` — boxes the `const void*`, so the `const` survives |
| `ListDictionaryInternal::…::Enumerator` | as above | as above |
| test-local `IntVectorEnumerator` | test storage | mechanical |

Every `const_cast` in the inventory disappears. Two `mutable` members become
ordinary members. `ArrayList`'s case is the clearest sign the design is right:
the element is already a box, and the current code goes out of its way to hand
out a pointer to it instead of a copy of it.

### 14.4 The three in-library call sites

```cpp
// ArrayList.hpp:108 -- element type is std::any; this gets SIMPLER
_items.emplace_back(e->getCurrentProperty());

// Stack.hpp:47 / Queue.hpp:47 -- element type is void*
s_.push_back(std::any_cast<void*>(e->getCurrentProperty()));
q_.push_back(std::any_cast<void*>(e->getCurrentProperty()));
```

### 14.5 Internal representation

**Nothing changes.** No collection gains a member, no enumerator gains a member,
no new type is introduced, and no new header is added to the public surface
beyond `<any>` — which `ArrayList.hpp`, `Hashtable.hpp`, and
`DictionaryEntry.hpp` already include. `Generic::IEnumerator<T>` stays at
`sizeof == 8`. That is the property §11.4 selects B for.

---

## 15. Generic/non-generic adaptation

| Path | Returns | Owns | Reaches storage | Cost |
|---|---|---|:---:|---|
| `IEnumerator<T>::Current()` | `const T&` | nothing | yes, read-only | free |
| `IEnumerator<T>::getCurrentProperty()` | `std::any` | its copy | **no** | 1 copy + boxing |
| `IEnumerator::getCurrentProperty()` (through a base pointer) | `std::any` | its copy | **no** | as above |
| `IDictionaryEnumerator::getKey/getValueProperty()` | `const void*` | nothing | yes, read-only | free — **unchanged, out of scope** |

The last row is deliberate and is §30 risk 4. Those two accessors are already
`const`-correct, so class A does not apply to them, but classes C, D, and E do,
and `Hashtable::MemberEnumerator` reaches a live map key *through* them. Under
this design that reach stops being *writable* — the member enumerator copies out
— but the `const void*` accessors themselves still alias live storage for anyone
who calls them directly. Bringing them to `std::any` is the obvious follow-on and
is **not** folded in here: it is a second public break on a second interface, and
folding it in would make the approval in §33 broader than the evidence.

---

## 16. Ownership, lifetime, and the `Current` state machine

### 16.1 After Phase 2

| Question | Answer |
|---|---|
| What owns the value `getCurrentProperty()` returns? | **The caller.** It is a `std::any` by value. |
| How long is it valid? | As long as the caller keeps it. Nothing invalidates it. |
| Does `MoveNext()` affect it? | No. |
| Does `Reset()` affect it? | No. |
| Does mutating the collection affect it? | No. |
| Does destroying the enumerator affect it? | No. |
| Does destroying the collection affect it? | No. |
| Can writing to it reach the collection? | **No.** |
| Does reading it advance the mutation counter? | No — it is a read. |
| Does writing to the returned box advance it? | No, and it cannot: the box is not the collection. |

### 16.2 The typed accessor — unchanged behaviour, newly written down

`Current()` keeps returning `const T&`. Phase 1 states its window in the header
for the first time:

> The reference is valid until the next `MoveNext()` or `Reset()` on this
> enumerator, or the next operation that mutates the collection, whichever comes
> first. It follows the same invalidation rules as a reference into the
> collection's underlying storage. Retaining it beyond that window is undefined
> behaviour; copy the value if it must outlive the current position.

That sentence is true for all fourteen implementations of §5.1 — the snapshot
ones satisfy it trivially — which is exactly what could **not** be written for
`getCurrentProperty()` today (§7).

### 16.3 State machine — unchanged

| State | `Current()` | `getCurrentProperty()` |
|---|---|---|
| before first `MoveNext()` | throws `InvalidOperationException("Enumeration has not started. Call MoveNext.")` | same, via `Current()` |
| positioned on an element | returns `const T&` | returns `std::any` holding a copy |
| after `MoveNext()` returned false | throws `InvalidOperationException("Enumeration already finished.")` | same |
| after `Reset()` | back to before-start; both throw | same |
| collection mutated since construction | `MoveNext()`/`Reset()` throw `InvalidOperationException("Collection was modified; …")`; `Current()` still reports the last position | same |

The last row is today's behaviour and is deliberately preserved: the guard lives
in `MoveNext()`/`Reset()`, not in `Current()`, matching .NET (`List.cs:1207`,
`:1241`).

---

## 17. Mutation and versioning rules

1. **No enumerator accessor advances any mutation counter, before or after.**
   Enumeration is a read.
2. **After Phase 2 no enumerator accessor can cause a counter to *need* to
   advance**, because none of them exposes a writable path to collection storage.
   That is the defect closing.
3. Every existing increment and guard site is **unchanged**. Ticket #1787's
   `MutationCounter` architecture is untouched — no counter changes type, width,
   position, or assignment behaviour.
4. `ObjectModel::Collection<T>` and `ObjectModel::ReadOnlyCollection<T>` still
   have **no** counter and still get no fail-fast guard. This design does not
   give them one; it removes the mutable path that made their lack of one
   exploitable. Adding a counter to `Collection<T>` is #1791 Phase 2's business
   (`sizeof` 32 → 40) and is **not** folded in here.

---

## 18. Exception matrix

| Operation | Condition | Exception | Message | Changed? |
|---|---|---|---|:---:|
| `Current()`, `getCurrentProperty()` | before first `MoveNext()` | `System::InvalidOperationException` | `Enumeration has not started. Call MoveNext.` | no |
| `Current()`, `getCurrentProperty()` | after enumeration ended | `System::InvalidOperationException` | `Enumeration already finished.` | no |
| `getCurrentProperty()` on `ArrayList`/`Stack`/`Queue`/`Hashtable`/`ListDictionaryInternal` | invalid position | `System::InvalidOperationException` | `Enumeration has either not started or has already finished.` | no |
| `MoveNext()`, `Reset()` | counter differs from snapshot | `System::InvalidOperationException` | `Collection was modified; enumeration operation may not execute.` | no |
| **`getCurrentProperty()`** | **`T` is not copy-constructible** | **`System::NotSupportedException`** | **`The element type cannot be boxed; use the typed Current() accessor.`** | **NEW** |

**Ordering:** the state-machine check runs **first**, before any copy and before
any box is constructed. Under B this is automatic, because the bridge's first act
is to call `Current()`, which runs `EnumeratorState::requireCurrent()`. #1793
must preserve that ordering in the nine hand-written overrides too.

The one new row is the only behavioural addition in the design, and it is .NET's
own documented answer for the non-boxable case (§9.4).

---

## 19. Value, reference, move-only, and non-trivial types

| `T` | `Current()` | `getCurrentProperty()` | Notes |
|---|---|---|---|
| `int`, `bool`, enum | `const T&` | `std::any` holding a copy | **0 allocations** — libstdc++ stores it in place |
| `std::string` | `const T&` | `std::any` holding a copy | **2 allocations** measured (the box, then the string's buffer) |
| a struct with members | `const T&` | copy | member writes hit the box only |
| `std::shared_ptr<X>` ("reference-like") | `const T&` | a copy of the **handle** | mutating `*X` through it is not mutating the collection — matching .NET's reference-type semantics exactly (§9.1) |
| move-only (`unique_ptr` member, deleted copy) | `const T&` — **works** | throws `NotSupportedException` | .NET's `ref struct` answer (§9.4) |
| ownership-counted | `const T&` | copy; `live` count verified balanced in `probe1_nontrivial.log` | no leak, no double destroy |

`probe1_nontrivial.log` records `live 0 -> 0` across member writes, writes through
an owned indirection, and whole-element replacement — so the copy semantics the
design relies on are measured, not assumed.

---

## 20. Runtime type validation

`std::any::type()` returns the `std::type_info` of the boxed value, and
`std::any_cast<T>` throws `std::bad_any_cast` on a mismatch instead of silently
reinterpreting bytes. This closes class C, which **no other candidate except D**
addresses, and it is the property §6.6 shows is entirely absent today.

One consequence #1793 must document: `std::any_cast<T>` throws
`std::bad_any_cast`, which is a **`std::` exception, not a `System::` one**. That
is consistent with how this port already exposes `std::any` (`ArrayList`,
`DictionaryEntry`, #1771's `CopyTo`), and it is left as-is rather than wrapped —
wrapping would mean re-implementing `any_cast`.

---

## 21. Source and ABI consequences

### 21.1 Source

- **28 measured in-repository call sites**, plus 1 consumer-fixture site the
  sweep structurally cannot see (§12.1). 3 of the 28 are library-internal.
- **9 direct overrides + 1 bridge** migrate. Every one is mechanical (§14.3).
- **Every hand-written implementer in consumer code must migrate**, and this
  repository's own test suite contains two of them
  (`GenericInterfacesTests.cpp:18`, `Interfaces2Tests.cpp:21`) — direct evidence
  that downstream code implements these interfaces by hand, the same evidence
  #1790 §6.3 found for `IList<T>`.
- The typed `Current()` path — **27 call sites** — is **unaffected**.

### 21.2 ABI — the dangerous half, measured

Measured in `build-probe-ienumerator/abi/`, one object file per candidate:

```
BASELINE   _ZNK6System11Collections4Impl18getCurrentPropertyEv
CONSTVOID  _ZNK6System11Collections4Impl18getCurrentPropertyEv
ANY        _ZNK6System11Collections4Impl18getCurrentPropertyEv
```

**Byte-identical, for all three.** The Itanium C++ ABI does not encode a
non-template function's return type in its mangled name. The vtables
(`_ZTVN6System11Collections11IEnumeratorE`) are likewise identically named, and
the accessor stays in the **same vtable slot** (offset `0x20` before and after),
so no other slot is renumbered.

The calling convention is not the same, and `objdump` shows it:

```
BASELINE:  mov %rax,%rdi          ; `this` in RDI
           call *%rdx             ; result in RAX

ANY:       lea -0x10(%rbp),%rax   ; hidden sret buffer ...
           mov %rax,%rdi          ; ... in RDI
           mov %rdx,%rsi          ; `this` displaced to RSI
           call *%rcx
```

**`this` moves from `%rdi` to `%rsi`.** A translation unit compiled against the
old header and linked against a library built with the new one will link
**without a single diagnostic** and then call the accessor with the wrong `this`.
No candidate avoids this — `const void*` has the identical mangled name too — so
it is not an argument between alternatives. It is a hard requirement on the
release, and it is the fourth item in §33's approval for that reason.

### 21.3 Object layout

**Unchanged, everywhere.** `sizeof`/`alignof` of `IEnumerator`,
`IEnumerator<T>`, and every collection in §3's table are identical before and
after; no type gains, loses, or reorders a member. This is B's principal
advantage over G (§11.4) and it means the approval in §33 does **not** include a
layout item, unlike #1788, #1789, and #1791 Phase 2.

### 21.4 Module dependencies

**None added.** `<any>` is a standard header already included by
`ArrayList.hpp`, `Hashtable.hpp`, and `DictionaryEntry.hpp` inside
`Collections.Core`. The module graph stays at 41 modules / 90 edges;
`scripts/validate_module_boundaries.py` and the Text.Json isolation fixture are
unaffected.

---

## 22. Performance and allocation

Measured in `probe4_alternatives.log` with a counting `operator new`:

| Element type | Allocations per non-generic `Current` read |
|---|---:|
| `int` (and every type ≤ one pointer, nothrow-move-constructible) | **0** — libstdc++'s small-buffer optimisation stores it in place |
| `std::string` | **2** — the `std::any` heap object, then the string's own buffer |

The typed `Current()` path — the one used by 27 of the 59 measured sites and by
every range-style walk — is **unchanged and still allocation-free**.

This is a real regression on the non-generic path for large `T`, and it is
stated rather than buried. Three things bound it: the non-generic path is the
*type-erased* one, which is already the slow path by construction; .NET pays the
identical cost (a heap box per read for a value type); and any caller in a hot
loop has the typed accessor available and should use it. #1793 should record a
before/after measurement with an `asm volatile` barrier per iteration — without
one GCC hoists the loop-invariant call and the benchmark measures nothing, the
mistake #1786 §13.1 recorded.

---

## 23. Migration guidance

| Was | Becomes |
|---|---|
| `T* p = static_cast<T*>(e->getCurrentProperty());` | `T v = std::any_cast<T>(e->getCurrentProperty());` |
| `*static_cast<T*>(e->getCurrentProperty())` | `std::any_cast<T>(e->getCurrentProperty())` |
| `EXPECT_EQ(*static_cast<int*>(e->getCurrentProperty()), 5)` | `EXPECT_EQ(std::any_cast<int>(e->getCurrentProperty()), 5)` |
| `if (e->getCurrentProperty() == nullptr)` | `if (!e->getCurrentProperty().has_value())` |
| `void* raw = e->getCurrentProperty();` (non-generic `Stack`/`Queue`, whose element **is** a `void*`) | `void* raw = std::any_cast<void*>(e->getCurrentProperty());` |
| `*static_cast<T*>(e->getCurrentProperty()) = v;` (a **write**) | **no replacement — this is the defect.** Use the collection's own setter (`setItem`, `operator[]`, `Insert`) so the mutation counter advances. |
| `std::any_cast<int>(*static_cast<std::any*>(e->getCurrentProperty()))` (an element that is **already** a box) | `std::any_cast<int>(e->getCurrentProperty())` — one unwrapping disappears |
| implementing `IEnumerator` by hand | change the return type; return `std::any(value)` instead of an address |
| implementing `IEnumerator<T>` by hand | **nothing** — the bridge is inherited |
| keeping the pointer past `MoveNext()` | keep the `std::any` instead; it owns its value and never dangles |

The sixth row is the point of the ticket: the one thing that stops being
expressible is the one thing that was never supposed to be.

---

## 24. Permanent test plan

### 24.1 Delivered by this ticket

`modules/collections/tests/System/Collections/EnumeratorCurrentSafetyTests.cpp`,
in `SharpRuntimeTests_Collections_Core`, split into two suites on purpose,
following #1790's pattern exactly:

- **`EnumeratorCurrentContract`** (8 cases) — behaviour that must survive #1793
  unchanged: both accessors reject a read before the first `MoveNext()` and after
  the end, with the exact .NET messages; `Reset()` returns both to before-start;
  a structural mutation still invalidates; a snapshot enumerator is still
  isolated from later mutation; `ReadOnlyCollection`'s own members still throw
  `NotSupportedException`; the `Hashtable` dictionary enumerator's three typed
  accessors still enforce the state machine; and `Current()` is still
  `const`-qualified for a trivial and a non-trivial `T`.
- **`EnumeratorCurrentDivergence`** (9 cases) — the measured divergence, each
  asserting today's behaviour with .NET's named in a comment, so #1793 must
  consciously flip it: the `static_assert`s pinning that both accessors still
  return `void*`; live vector storage aliased, writable, and untracked; the same
  for node storage and associative-value storage; `ReadOnlyCollection` mutable
  through its own enumerator; `ArrayList`'s element *type* rewritable; the
  `Hashtable` key view aliasing live key storage; the snapshot enumerator's write
  hitting only its own copy; and the typed and non-generic accessors designating
  the same object.

The `static_assert`s are the load-bearing part: **#1793 physically cannot land
without editing them.**

One deliberate restraint: the `Hashtable` case asserts the *aliasing* and does
not perform the key rewrite. Actually corrupting a live `std::unordered_map` key
belongs in a gitignored probe (§6.5), not in a permanent suite that runs on every
build.

### 24.2 Added by #1793

Flip every Divergence case rather than deleting it: assert that
`getCurrentProperty()` returns `std::any`; that a write to the returned box
leaves the collection unchanged for each storage family; that
`ReadOnlyCollection` is no longer mutable through its enumerator; that
`std::any_cast<WrongType>` throws `std::bad_any_cast`; that a move-only `T`
throws `NotSupportedException` from the non-generic path while `Current()` still
works; that the box survives `MoveNext()`, `Reset()`, collection mutation,
enumerator destruction, and collection destruction; and that the state-machine
exceptions still precede any boxing.

---

## 25. Sanitizer plan

- **ASan** — re-run all six `probe2` lifetime shapes under #1793. Four currently
  report `heap-use-after-free`; all four must become unreachable **through the
  non-generic interface**. The typed `&Current()` route remains reachable by
  design and stays as a pinned, documented residual (§30, risk 1).
- **UBSan** — the whole permanent suite; expected 0, as today (§6.9).
- **LSan** — the permanent suite, because `GetEnumerator()` hands back a
  caller-owned raw pointer, and because #1793 introduces per-read allocation on
  the non-generic path for large `T`.
- **TSan** — **not planned, and the reason is stated rather than omitted.** This
  design adds no atomic, no `mutable` cache, and no hidden `const` write — it
  *removes* two `mutable` members (§14.3). No enumerator claims thread safety
  before or after. A TSan run would substantiate nothing that #1784's and #1787's
  probes have not already covered.

---

## 26. Consumer-fixture plan

For #1793, following the established pattern in `test/consumer/`:

- **positive** — `test/consumer/collections_enumerator_current.cpp`, compiled
  against the public `Collections.Core` surface only with `-Wall -Wextra
  -Wpedantic -Werror`: read through both accessors; `std::any_cast` the box for
  a value type, a `std::string`, and the non-generic `void*` element case; keep
  the box across `MoveNext()`, `Reset()`, a collection mutation, and the
  enumerator's destruction; verify a write to the box leaves the collection
  unchanged. Exits 0.
- **negative** — a fixture that attempts
  `*static_cast<int*>(e->getCurrentProperty()) = 5;` and must **fail to
  compile**, proving the escape is closed rather than merely discouraged. This is
  the fixture that makes the ticket verifiable.
- `test/consumer/collections_dictionary_views.cpp:81` migrates (§12.1).
- `scripts/check_selective_components.sh Collections.Core` in isolation, with a
  repository-local `TMPDIR`.

---

## 27. Implementation phases

**Phase 1 — no approval required**

1. Write §7's ownership table and §16.2's validity window into
   `System/Collections/IEnumerator.hpp` and
   `System/Collections/Generic/IEnumerator.hpp`.
2. Correct the non-generic accessor's doc-comment: *"Pointer to the current
   element; cast to the appropriate type"* promises a type contract that does not
   exist and says nothing about lifetime, mutability, or ownership.
3. Record the `IAsyncEnumerator<T>` naming inconsistency (§4) as known and
   deliberate, so it is not "fixed" by accident.

**Phase 2 — blocked on §33**

4. `System/Collections/IEnumerator.hpp`: return `std::any`; add `<any>`.
5. `Generic/IEnumerator.hpp`: the boxing bridge of §14.2.
6. The nine direct overrides of §14.3; delete four `const_cast`s; drop two
   `mutable` qualifiers.
7. The three in-library call sites of §14.4.
8. The 28 measured consumer sites and the one consumer-fixture site.
9. Flip the Divergence suite (§24.2); add both consumer fixtures (§26);
   ASan/UBSan/LSan runs; the layout and symbol probes re-run against the stored
   baselines in §3 and §21.2; a `README.md` behaviour-change entry.

**Rollback.** Phase 1 is independently revertible and leaves nothing behind.
Phase 2's revert restores `void*` and with it every defect class; a revert must
be validated by re-running `probe1_current` and `probe2_lifetime` under ASan, not
by CTest alone, because the permanent suite's `static_assert`s would be reverted
with it and would then agree with the old behaviour.

---

## 28. Relationship to ticket #1791

Determined from repository evidence, not assumed.

1. **They are independent defects on disjoint surfaces.** #1791 changes
   `IList<T>::operator[]` and `List<T>::operator[]`. #1792/#1793 changes
   `IEnumerator::getCurrentProperty()`. No file, signature, or type is touched by
   both: `List.hpp` gains members under #1791 and is untouched here; its
   `Enumerator` gains nothing under #1791 and changes only through the inherited
   bridge here.
2. **Neither repairs the other.** A tracked `ElementReference<T>` from
   `List<T>::operator[]` does nothing about a pointer obtained from an
   enumerator, and this design's boxed `Current` does nothing about
   `list[i] = v`. Both were reproduced separately, on the same collection, in the
   same probe run.
3. **This design does not depend on #1791.** Nothing in §14 references
   `ElementReference<T>`, `getItem`, `setItem`, or any counter change.
4. **Recommended order: #1793 before #1791.** Three reasons, in decreasing
   weight. (a) #1793 needs no object-layout change; #1791 Phase 2 grows
   `ObjectModel::Collection<T>` from 32 to 40 bytes. Landing the
   no-layout-change break first means one rebuild boundary is strictly simpler
   than the other. (b) #1793's break is loud everywhere it is a *read*
   (`std::any_cast` versus `static_cast`) and impossible where it is a *write*;
   #1791's break silently changes what `list[i]` *means* in expressions that
   still compile. Loud first is easier to land. (c) #1793's approval is
   three-part; #1791 Phase 2's is four-part and includes the layout item.
5. **They must not be merged.** Nothing in the evidence requires one atomic
   migration: the surfaces are disjoint (point 1) and each is independently
   testable. Merging them would combine a three-part and a four-part approval
   into one, which is worse for the reviewer, not better.
6. **Migration notes must distinguish them.** A consumer hitting both sees two
   unrelated compile errors on two unrelated APIs. §23 covers only this ticket's;
   `docs/ListIndexerVersioningDesign.md` §21 covers #1791's. Neither should
   reference the other's replacement text.

---

## 29. Fallback if the approval is declined

If §33's approval is declined, the correct outcome is not silence.

1. Land **Phase 1** (documentation), which needs no approval and is the honest
   minimum.
2. Consider **Alternative A** (`const void*`) as a separately scheduled,
   separately approved narrowing. It still breaks source and ABI, so it needs its
   own approval; what it buys is that a consumer must now write `const_cast`
   deliberately, which turns an accident into a decision. It must be described in
   `README.md` and in the header as a **narrowing, never as a remediation** —
   §11.1 measures that it does not close class B.
3. Record the divergence as **permanent by decision** in both headers with a
   pointer to this section, and close #1793 `wontfix` with the reason attached,
   exactly as #1772 was.

---

## 30. Risks and residual limitations

| # | Risk | Severity | Position |
|---|---|---|---|
| 1 | The typed `Current()` reference hazard is **not** closed | **Medium** | Deliberate (§13.1). `&Current()` retained across a mutation is still a use-after-free, reproduced in §6.4. Closing it needs Alternative F, which makes move-only `T` uninstantiable. Mitigated by writing the validity window into the header (Phase 1) and by a pinned permanent test. **This design closes the type-erased path, not the typed one, and says so.** |
| 2 | Silent ABI break — identical mangled name, different calling convention | **High** | Measured (§21.2). No candidate avoids it. The only mitigation is a mandatory full consumer rebuild, which is §33 item 3. A partial rebuild produces memory corruption with no diagnostic from any tool in the toolchain. |
| 3 | CNA and mobile-eggbert usage is unmeasured | **Medium** | Out of scope by instruction; not inspected, searched, built, or modified. The 28-site figure is this repository only and is explicitly **not** a proxy for theirs. Ticket #1773 remains blocked. |
| 4 | `IDictionaryEnumerator::getKey/getValueProperty()` keep returning `const void*` into live storage | **Medium** | Deliberate (§15). Classes C, D, and E remain open on that sibling interface. Folding it in would widen the approval past the evidence. It should become its own ticket once #1793 lands, and this document is where that is recorded. |
| 5 | Per-read allocation on the non-generic path for large `T` | Low–Medium | Measured: 2 for `std::string`, 0 for `int` (§22). .NET pays the same. The typed path is unchanged and allocation-free. |
| 6 | Move-only `T` loses the non-generic path entirely | Low | .NET's own documented answer for the analogous case (§9.4). `Current()` still works. No move-only `T` is instantiated anywhere in this repository today, so nothing here regresses; the constraint is on future consumers. |
| 7 | The `getCurrentProperty()` naming collision with `IAsyncEnumerator<T>` is not fixed | Low | Recorded, not repaired (§4). Renaming either is a second unrelated public break and would need its own approval. |
| 8 | Uninstantiated templates are invisible to both sweeps | Low | Stated in §12.3. Both sweeps share the limitation, so it does not affect the comparison between candidates. |
| 9 | `std::bad_any_cast` is a `std::` exception, not a `System::` one | Low | Consistent with how `std::any` is already exposed by `ArrayList`, `DictionaryEntry`, and #1771's `CopyTo` (§20). Documented rather than wrapped. |
| 10 | `probe1` depends on `-fno-access-control` | Low | Probes only. The permanent suite uses no seam at all; it asserts through the public API, and `probe3`/`probe4` compile in `strict` mode without the flag. |

---

## 31. Rejected approaches, in one place

Alternatives A (as a complete fix), C, D, E, F, G, and H are rejected in §11 with
the reasoning attached to each. The three rejections most likely to be revisited,
restated so they are not re-litigated by accident:

- **A (`const void*`) is rejected because it is measurably not a fix**, not
  because it is unconventional — it is in fact the repository's own convention on
  `IDictionaryEnumerator`. One `const_cast` restores the write, and the probe
  performs it (§11.1). It remains the right fallback if the approval is declined
  (§29), and it must then be labelled a narrowing.
- **G (enumerator-owned copy) is rejected narrowly**, and would be the correct
  choice if per-read allocation for large `T` were ever measured to matter more
  than type safety and object layout. It is cheaper by one allocation and costs
  `sizeof(T)` on every enumerator plus leaves class C open. If a future decision
  reverses that weighting, this document should be revised rather than worked
  around.
- **H (document it) is rejected** not because documentation is worthless but
  because §7 shows there is **no single true sentence** to write: ten distinct
  lifetime rules hide behind one signature. The documentation half of H is
  adopted anyway, as Phase 1.

---

## 32. Implementation-ticket scope

Ticket **#1793**, `REMED-COLL-ENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, opened
**blocked**. Not begun. Scope is §27 exactly.

**Explicitly excluded from #1793**: `IDictionaryEnumerator`'s `const void*`
accessors (§15, §30 risk 4); the typed `Current()` signature (§13.1); the
`IAsyncEnumerator<T>` naming collision (§4); `List<T>`'s indexer (#1791); the
`LinkedList<T>`/`BitArray` counter widening (#1788/#1789); `SortedSet<T>`'s
nested-view exception ordering (#1785); CNA and mobile-eggbert (#1773); and every
`getCurrentProperty()` of §5.3, which is a different accessor that happens to
share a name.

---

## 33. Exact user approval required

Phase 1 of #1793 needs **no** approval and may be begun whenever it is scheduled.

Phase 2 of #1793 is **blocked** pending explicit user approval of all three of
the following, together, scoped to that ticket:

1. **A public source-breaking change to `System::Collections::IEnumerator`:**
   `getCurrentProperty()`'s return type changes from `void*` to `std::any`, so
   every `static_cast<T*>(e->getCurrentProperty())` stops compiling and every
   write through the returned pointer stops being expressible. **Every
   implementer must migrate, including hand-written ones in consumer code** — of
   which this repository's own test suite contains two.
2. **A public source-breaking change to
   `System::Collections::Generic::IEnumerator<T>`:** the inherited bridge returns
   `std::any` and **throws `System::NotSupportedException` for an element type
   that cannot be copied**, which is a new exception on an existing path. The
   typed `Current()` is unchanged.
3. **Acknowledgement of a silent ABI break requiring a full consumer rebuild.**
   The mangled name is byte-identical before and after (measured, §21.2) and the
   calling convention is not: `this` moves from `%rdi` to `%rsi`. **A partially
   rebuilt consumer will link with no diagnostic and then corrupt memory.** No
   tool in the toolchain detects this. There is **no** object-layout change — this
   approval is narrower than #1788's, #1789's, or #1791 Phase 2's in that one
   respect and wider in this one.

And, separately noted rather than approved: **CNA's and mobile-eggbert's usage of
`getCurrentProperty()` is unmeasured**, because those repositories are out of
scope and were not inspected. The 28-call-site figure in §12.1 is this repository
only and is explicitly not a proxy for theirs.

A previous approval — for `CopyTo` (#1771), `ReadOnlyDictionary::Empty` (#1780),
or `SortedSet::GetViewBetween` (#1783) — is **not** approval for any of the
above. None of them carries over.

If Phase 2 is declined, follow §29.

---

## 34. Implementation complete — ticket #1793

*Added 2026-07-28 on local branch
`feature/remediation-coll-ienumerator-current-safety`, after §33's three-part
approval was granted explicitly and scoped to #1793. Everything above this
section is the design record as #1792 wrote it and is **unchanged**: the unsafe
pointer, its reproductions, and the alternatives analysis stay on the record
exactly as measured. This section records what was built, and the four places
where the built thing differs from what §14 sketched.*

### 34.1 What landed

Both phases, together. The approval covered Phase 2, and Phase 1's
documentation was written into the same headers rather than split across two
commits that would have contradicted each other.

| Surface | Before | After |
|---|---|---|
| `System::Collections::IEnumerator::getCurrentProperty()` | `[[nodiscard]] virtual void* … const = 0` | `[[nodiscard]] virtual std::any … const = 0` |
| `Generic::IEnumerator<T>::Current()` | `[[nodiscard]] virtual const T& … const = 0` | **unchanged** |
| `Generic::IEnumerator<T>::getCurrentProperty()` | `return const_cast<T*>(&Current());` | `if constexpr (copyable) return std::any(Current()); else { (void)Current(); throw NotSupportedException(…); }` |

All **four** `const_cast`s outside the bridge are gone, and both `mutable`
members (`Hashtable::Enumerator::current_`, `BitArray::Enumerator::current_`)
are ordinary members again.

### 34.2 Every implementation migrated

Eight production non-generic overrides, the one bridge (covering thirteen
production generic implementations plus every hand-written one), two
test-local implementers, and the three in-library call sites — the counts §5
and §12.1 measured, confirmed exact by compilation:

| # | Implementation | Now returns |
|---|---|---|
| 1 | `Generic::IEnumerator<T>` bridge | `std::any(Current())`, or throws for non-copyable `T` |
| 2 | `ArrayList::Enumerator` | a copy of the element's own `std::any` — **not** nested |
| 3 | `Hashtable::Enumerator` | `std::any(DictionaryEntry)` |
| 4 | `Hashtable::…::MemberEnumerator` | keys: `std::any(std::string)`; values: a copy of the element's `std::any` — **not** nested |
| 5 | `BitArray::Enumerator` | `std::any(bool)` |
| 6 | `Collections::Stack::Enumerator` | `std::any(void*)` — the element *is* the pointer |
| 7 | `Collections::Queue::Enumerator` | `std::any(void*)` |
| 8 | `ListDictionaryInternal::NodeEnumerator` | `std::any(const void*)` — the `const` survives |
| 9 | `ListDictionaryInternal::…::Enumerator` | `std::any(const void*)` |
| 10 | test-local `IntVectorEnumerator` | `std::any(int)` |
| 11 | test-local `VecEnumerator` | **nothing to migrate** — it implements `IEnumerator<T>` and inherits the bridge |

In-library call sites: `ArrayList.hpp:108` stores the box directly (the element
type *is* `std::any`); `Stack.hpp:47` and `Queue.hpp:47` unbox with
`std::any_cast<void*>`.

### 34.3 Four corrections to §14's sketch

Recorded because the sketch was compile-validated but not *run*-validated, and
two of these were caught only by the permanent suite.

1. **The `if constexpr` else-branch had to call `Current()` and discard it.**
   §14.2's sketch throws `NotSupportedException` directly. That discards the
   only use of `Current()`, so for a move-only `T` a **before-start or after-end
   read reported `NotSupportedException` where the pre-#1793 bridge reported
   `InvalidOperationException`** — silently converting an existing exception
   path, which §18's ordering rule ("the state-machine check runs first")
   forbids. The implemented bridge calls `(void)Current();` first. Caught by
   `EnumeratorCurrentSafety.MoveOnlyStateMachineStillPrecedesTheBoxingRefusal`.
2. **`Generic::List<std::any>` cannot be instantiated at all**, so the
   nested-box case could not be tested through it as planned: `std::any` is not
   equality-comparable and `List<T>`'s `Contains`/`IndexOf` need `operator==`.
   The permanent suite uses a hand-written `IEnumerator<std::any>` implementer,
   which exercises the same bridge.
3. **`std::any(Current())` for `T = std::any` selects `std::any`'s COPY
   constructor**, not its value-forwarding one — that constructor is constrained
   with `!is_same_v<decay_t<ValueType>, any>`. So the result is a copy of the
   element's box, never a nested box. §14.3 assumed this for `ArrayList` without
   stating the mechanism; it is now pinned by three explicit tests
   (`ArrayListElementTypeIsNoLongerRewritable`,
   `HashtableValueViewReturnsTheElementBoxNotANestedOne`,
   `StdAnyElementIsCopiedNotNested`).
4. **`Stack(ICollection&)` and `Queue(ICollection&)` gained a throwing path.**
   They now `std::any_cast<void*>` each element, so a source collection that
   does not enumerate `void*` elements raises `std::bad_any_cast` where the old
   code silently stored a pointer into that source's live storage. No caller in
   this repository constructs either from an `ICollection`, so nothing
   regresses; it is documented on both constructors.

### 34.4 Measured results

| Gate | Result |
|---|---|
| Pre-fix reproduction, re-run before any production edit | 15 defects across `probe1`'s six modes; 4 ASan `heap-use-after-free`; 2 type-erasure defects; 0 invariants failed in every mode |
| `Collections.Core` | 2,208 → **2,229** tests, all passing |
| Full repository, clean rebuild | **13,515** tests across 37 executables |
| Library sources broken by the migration | **0 of 626**, as §12.2 predicted |
| ASan + UBSan, six migrated lifetime shapes | 0 reports, 0 runtime errors |
| LSan | 0 leaks; proved active by a self-test that leaks 289 bytes and exits non-zero |
| TSan | **not run, deliberately** — this change adds no atomic, no `mutable` cache and no hidden `const` write; it *removes* two `mutable` members. Concurrent-mutation tests here would substantiate nothing #1784's and #1787's probes have not covered. |
| Object layout | **identical**, `diff`-clean against §3's stored baseline |
| Mangled name | `_ZNK…18getCurrentPropertyEv` — byte-identical, confirmed on the real repository objects |
| Vtable slot | offset `0x20`, unchanged; no other slot renumbered |
| Calling convention | `this` `%rdi` → `%rsi`, sret buffer in `%rdi` — confirmed by `objdump` |
| Stale-object probe | old caller + new implementation **linked with zero diagnostics**; the mismatched call then took a SEGV with UBSan reporting an invalid vptr |
| Negative consumer fixture | rejected at all **6** marked sites |
| Positive consumer fixture | compiles under `-Wall -Wextra -Wpedantic -Werror` and exits 0 |
| Module graph | 41 modules / 90 edges, unchanged |
| Doxygen | **1,939** warnings, ceiling 1,942 — one more than the canonical 1,938, and the reason is structural (§34.8) |

### 34.5 Allocation and cost, measured rather than assumed

Counted with a replaced global `operator new`, measuring the full round trip
(construct **and** destroy the box):

| Element type | Allocations per type-erased read |
|---|---:|
| `int` | **0** |
| raw pointer (`int*`) | **0** |
| `ArrayList` element already holding `int` | **0** |
| `std::string`, SSO (4 chars) | **1** |
| `std::shared_ptr<int>` | **1** |
| `std::string`, 64 chars | **2** |
| `ArrayList` element holding a 64-char `std::string` | **2** |
| `DictionaryEntry` | **2** |

§22 predicted 0 for "every type ≤ one pointer, nothrow-move-constructible" and
2 for `std::string`. The middle row is the correction: a *small* `std::string`
and a `std::shared_ptr` still cost **1**, because libstdc++'s `std::any` small-
buffer optimisation admits only types that fit in a `void*`, and both are larger
than that regardless of their contents.

A non-trivial element costs exactly **1 copy and 1 destroy** per read, with the
live count balanced at 0.

Wall-clock, `-O2`, one `asm volatile` barrier per iteration so the
loop-invariant call is not hoisted (the mistake #1786 §13.1 recorded):

| Element type | typed `Current()` | boxed | ratio |
|---|---:|---:|---:|
| `int` | 1.96 ns | 4.76 ns | 2.4× |
| `std::string` (SSO) | 2.22 ns | 13.74 ns | 6.2× |
| `std::string` (64) | 2.21 ns | 18.78 ns | 8.5× |
| `std::shared_ptr<int>` | 0.55 ns | 11.60 ns | 21.1× |

These are **not** a regression threshold and no gate enforces them. The typed
path is unchanged and allocation-free; the type-erased path is the slow path by
construction; and .NET pays a heap box per read for a value type too.

### 34.6 The suite was flipped, not deleted

All nine `EnumeratorCurrentDivergence` cases still exist, renamed to
`EnumeratorCurrentSafety`, each asserting the opposite outcome on the same
collection through the same accessor, with a `WAS …` comment naming the case it
replaced. The `static_assert`s remain load-bearing in the other direction: they
now pin `std::any`, so a revert to `void*` cannot land silently either. The
eight `EnumeratorCurrentContract` cases are untouched, as they were designed to
be. Twenty-one further cases cover ownership, lifetime, the move-only type,
type safety, nested boxing, the mutation counter, and every remaining
non-generic implementation family.

### 34.7 What is still open

- **Risk 1 stands.** The typed `Current()` reference hazard is **not** closed,
  by design (§13.1). `&Current()` retained across a mutation is still a
  use-after-free. Its validity window is now written into the header for the
  first time, and the header says explicitly that #1793 did not close it.
- **Risk 4 stands, and now has a ticket.** `IDictionaryEnumerator::getKeyProperty()`
  and `getValueProperty()` still return `const void*` into live storage. They are
  const-correct, so classes A and B do **not** apply — no *write* path exists —
  but classes C, D and E remain open on that sibling interface. A warning now
  sits on the interface itself, pointing here. Opened as ticket **#1794**
  (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M), **blocked** and
  deliberately not begun: it is a second public source break on a second
  interface and, by the same Itanium ABI property §21.2 measured, a second
  **silent** one, so it needs its own two-part approval. #1793's does not carry
  over. Note what #1793 already removed: the two member-view enumerators that
  `const_cast`'d these accessors' results and republished them as a mutable
  `void*` both copy out now, so what remains is the direct-call surface only.
- **Risk 3 stands.** CNA's and mobile-eggbert's usage remains unmeasured; both
  were not inspected, searched, built, or modified, by instruction. Ticket
  #1773 remains blocked. Both repositories stay on an older revision until they
  deliberately upgrade — and when they do, §21.2 and README's ABI paragraph are
  the mandatory reading.
- **Risk 2 is now realised rather than hypothetical.** §21.2's silent ABI break
  is reproduced end to end in §34.4's stale-object probe. The only mitigation
  is the mandatory full rebuild, which README now states.

### 34.8 The Doxygen count moved by one, and why

The canonical count went **1,938 → 1,939**, against a ceiling of 1,942. It is
not a new documentation defect and no header caused it.

`Doxyfile`'s `INPUT` covers `modules/` and `README.md`; it does **not** cover
`docs/`. Doxygen resolves every Markdown link in a scanned file as a `\ref`, so
**each link from `README.md` to a file under `docs/` emits exactly one
`unable to resolve reference` warning** — 57 such warnings already exist, one per
link, including the ones the two sibling Breaking-changes entries make to
`docs/CollectionVersionCounterSweep.md` and `docs/SortedSetLiveViewDesign.md`.
This ticket's `README.md` entry links to this file, so the total moves by one.

The link was kept rather than downgraded to plain text, because the two entries
either side of it link to their design records and a reader following the ABI
warning needs to reach the measurements. Ticket #1781 established the 1,942
ceiling above the measured count precisely so that structural, already-explained
warnings like this one do not force a choice between a useful cross-reference and
a static number. Recorded here so the next ticket does not re-derive it.
