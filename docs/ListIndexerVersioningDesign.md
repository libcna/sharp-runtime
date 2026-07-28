<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `List<T>` indexer mutation-version design

*Design, evidence, and decision record for ticket **#1790**
(`REMED-COLL-LIST-INDEXER-VERSION`, P3, size L, category `parity`, area
`Collections`). Recorded 2026-07-28 on local branch
`feature/remediation-coll-list-indexer-design`. This ticket carries **no
`SR-AUD-*` identifier** — the audit numbering is frozen at 364 and this
divergence was recorded during remediation, by ticket #1787's own Category D
classification (`docs/CollectionVersionCounterSweep.md` §17).*

***This ticket changes no production behaviour, no public signature, no object
layout, and no exception. It is design-only.*** The implementation it selects is
ticket **#1791**, opened `blocked` pending an explicit approval stated verbatim
in §28.

---

## 1. Executive decision

**No fully source-compatible correction exists.** A plain `T&` handed out by
`operator[]` cannot be intercepted: once the caller holds it, the collection has
no hook through which to learn that, or when, an assignment happened. Every
mechanism that closes the hole necessarily changes what the non-const indexer
returns, and that is a public source break.

The selected architecture is **Alternative A, refined** — the non-const indexer
returns a tracked proxy, `System::Collections::detail::ElementReference<T>`,
which reads as `const T&`, intercepts every write, and advances the mutation
counter. It is selected for one decisive reason and one supporting one. The
decisive reason is that **it is the only alternative that closes the write path
while keeping `list[i] = v` — the exact spelling C# uses — compiling.** The
supporting one is that it is measurably the least disruptive of the closing
alternatives: compiled against the whole repository, the refined proxy breaks
**1 site in 1 of 625 translation units**, and that one site is a hand-written
interface implementer — migration, not call-site breakage. The refined
value/setter alternative breaks **8 sites in 3 units** (§10.3).

Those two figures are close, and the honest reading is that **the
in-repository measurement does not on its own decide between A and B** — it
shows only that neither is expensive *here*. What decides it is that B deletes
`list[i] = v` from the API entirely. This repository happens to contain exactly
two such writes; a ported C# game does not, and CNA's usage is unmeasured by
instruction (§23, risk 3).

It is delivered in two phases, because only the second one needs approval:

| Phase | Content | Break | Approval |
|---|---|---|---|
| **1** | Add `getItem()` / `setItem()` to `List<T>` as tracked, concrete members. Correct the header's contract comment. | **None** — pure addition | **Not required** |
| **2** | Non-const `operator[]` returns `ElementReference<T>`; migrate `IList<T>` and its three other implementers; quarantine the raw-storage escapes. | Source-breaking | **Required (§28)** |

Three findings materially change the premises the ticket was written with, and
all three are corrections *against* this record's own convenience:

1. **The indexer is not the widest hole; `ToVector()` is.** The non-const
   `ToVector()` hands out the whole backing `std::vector<T>&`, so a caller can
   `push_back` or `clear` through it — a **structural** mutation the fail-fast
   guard never sees. The indexer can only replace an existing element. This was
   not documented anywhere before this ticket (§4.2, §5.2).
2. **The ticket's migration premise is wrong for this repository.** It records
   `operator[]` as "the single most call-site-heavy method in this repository".
   Measured across all 625 translation units, the non-const indexer has **61
   call sites, all of them in two test files**, and **no library source in the
   repository includes `List.hpp` at all** (§6.2). The migration burden inside
   this repository is small. The CNA/mobile-eggbert burden is real, unmeasured,
   and out of scope by instruction — it is *not* claimed here to be small.
3. **`IList<T>` has four implementers, not one.** `List<T>`,
   `ObjectModel::Collection<T>`, `ObjectModel::ReadOnlyCollection<T>`, and a
   hand-written one in the test suite. A grep for `public IList<` finds only the
   first; the other three spell it `public Generic::IList<T>`. They were found
   by compiling the repository against the candidate header, not by searching
   (§6.3). `Collection<T>` **has no mutation counter at all**, which is why
   Phase 2 carries a second object-size consequence (§12.3).

A **new, previously unrecorded defect** was found during the inventory and is
*not* absorbed into this ticket: `Generic::IEnumerator<T>::getCurrentProperty()`
`const_cast`s away constness and returns a `void*` to the live element, so any
non-generic consumer can mutate the element of **any** collection while
enumerating it, untracked. It affects every collection in the repository, not
`List<T>`, and is filed as ticket **#1792** (§27.3).

---

## 2. Ticket and defect

Ticket #1790, key `REMED-COLL-LIST-INDEXER-VERSION`, priority **P3**, size
**L**, source path
`modules/collections/include/System/Collections/Generic/List.hpp`.

Real .NET's `List<T>` index setter bumps `_version` (`List.cs:162`), so
`list[i] = value` fails an in-progress enumeration fast. This port's
`operator[]` returns a plain `T&` for C++ ergonomics, so there is no hook point
to intercept a later assignment through that reference, and a value-only index
write does **not** invalidate an outstanding enumerator.

The gap is **pre-existing** — documented in `List.hpp`'s own class comment since
ticket 1713 — and was neither introduced nor worsened by #1787, which changed
the counter's type and assignment behaviour and nothing about the indexer.

The asymmetry it sits in is unchanged and worth restating: `SortedList<K,V>` and
`OrderedDictionary<K,V>` **do** bump on a same-key value overwrite (matching
their .NET counterparts, which also do), and `Dictionary<K,V>` deliberately does
**not** (also matching .NET, whose `TryInsert` returns before `_version++` on
the overwrite branch). `List<T>` is the only one whose behaviour diverges from
its own .NET reference.

---

## 3. Exact current declarations

`modules/collections/include/System/Collections/Generic/List.hpp`, unchanged by
this ticket:

```cpp
// :142
[[nodiscard]] const T& operator[](intcs index) const override
{
    requireIndexInRange(index);
    return items_[static_cast<std::size_t>(index)];
}

// :148  <-- the defect
T& operator[](intcs index) override
{
    requireIndexInRange(index);
    return items_[static_cast<std::size_t>(index)];
}

// :231 / :233
[[nodiscard]] const std::vector<T>& ToVector() const { return items_; }
[[nodiscard]] std::vector<T>& ToVector() { return items_; }   // <-- wider hole

// :236-242
auto begin() { return items_.begin(); }
auto end()   { return items_.end(); }
[[nodiscard]] auto begin() const { return items_.cbegin(); }
[[nodiscard]] auto end()   const { return items_.cend(); }

// :49-50
std::vector<T> items_;
System::Collections::detail::MutationCounter version_;
```

`modules/collections/include/System/Collections/Generic/IList.hpp`:

```cpp
// :34
[[nodiscard]] virtual const T& operator[](intcs index) const = 0;
// :43  <-- the same defect, on the interface
virtual T& operator[](intcs index) = 0;
```

`modules/collections/include/System/Collections/Generic/IEnumerator.hpp`:

```cpp
// :38-41  <-- the newly found, separate defect (ticket #1792)
void* getCurrentProperty() const override {
    return const_cast<T*>(&Current());
}
```

Measured layout, LP64/GCC 14/libstdc++, `build-probe-listindexer/probe3_layout.log`:
`sizeof(List<int>) == sizeof(List<std::string>) == 40`, `alignof == 8`,
`is_polymorphic == true`, copy/move constructible and assignable, counter at
offset 32 (from `docs/CollectionVersionCounterSweep.md` §12.1);
`sizeof(List<int>::Enumerator) == 32`. For comparison,
`sizeof(ObjectModel::Collection<int>) == 32` and
`sizeof(ObjectModel::ReadOnlyCollection<int>) == 24`.

---

## 4. Complete mutable-access inventory

Derived by compiling the whole repository against a shim of the committed
headers in which each route carries a distinct `[[deprecated]]` tag
(`build-probe-listindexer/make_shim.py`, `sweep_callsites.py`), not by grepping.
Every one of the 625 translation units in `build/compile_commands.json` was
replayed with `-fsyntax-only`; 0 failed to compile.

### 4.1 The measured table

| # | Public signature | Permits mutation | Advances the version | Enumerator can detect it | Reference can outlive reallocation | Call sites (whole repo) | Source/ABI impact of changing it |
|---|---|---|---|---|---|---:|---|
| 1 | `T& List<T>::operator[](intcs)` | element replacement, member write, compound assignment, address-taking | **no** | **OK** | **no** | **yes** — UAF, §5.3 | **61** | source-breaking; return type change; virtual override |
| 2 | `virtual T& IList<T>::operator[](intcs)` | same, through the interface | **no** | **OK** | **no** | **yes** | **0** | source-breaking for **all four** implementers |
| 3 | `std::vector<T>& List<T>::ToVector()` | element write **and structural: `push_back`, `clear`, `resize`, `erase`** | **no** | **FAIL** | **no** | **yes** | **1** | source-breaking if removed/renamed |
| 4 | `auto List<T>::begin()` → `std::vector<T>::iterator` | element write; `std::sort`; range-for mutation | **no** | **FAIL** | **no** | **yes** | **3** | source-breaking if constrained |
| 5 | `auto List<T>::end()` | pairs with 4 | **no** | **OK** | **no** | **yes** | **3** | as 4 |
| 6 | `void* IEnumerator<T>::getCurrentProperty() const` | element write through a `const_cast`ed `void*` | **no** | **OK** | **no** | yes | — | **all collections**; ticket #1792 |
| 7 | `const T& List<T>::operator[](intcs) const` | none | n/a | **OK** | n/a | yes (dangling read only) | — | none proposed |
| 8 | `const std::vector<T>& ToVector() const`, `begin() const`, `end() const` | none | n/a | **FAIL** | n/a | yes (dangling read only) | — | none proposed |

Routes explicitly checked and **absent** from `List<T>`: there is no `data()`,
no `at()`, no `front()`, no `back()`, no `Span`/`ReadOnlySpan` accessor, no
`AsSpan`, no `ref`-returning member, no public conversion operator to
`std::vector<T>`, no public reference typedef or alias, and no friend that
exposes storage. `GetEnumerator()` returns `IEnumerator<T>*` whose typed
`Current()` returns `const T&`; the only mutable path out of it is route 6.
`AsReadOnly()` returns a snapshot copy, not a view. The private `version_` is
reachable only through the test-only
`SharpRuntime::Testing::CollectionVersionAccess<List<T>>` seam, which is
declared and never defined in production and is proven unreachable by
`test/consumer/collections_mutation_version_negative.cpp`.

### 4.2 Route 3 is the widest hole, and it was undocumented

`List.hpp`'s class comment describes the indexer gap and says the STL-interop
`begin()/end()` follow `std::vector` rules. It says nothing about `ToVector()`,
and it calls the indexer "**one** narrow, documented gap". Both statements are
inaccurate: `ToVector()` permits *structural* mutation, which the indexer cannot,
and which is what the fail-fast contract exists to catch. Reproduced in §5.2.

This ticket corrects only the comment (documentation, no behaviour), and hands
the contract decision to #1791.

---

## 5. Pre-fix reproduction

All probes live in the repository-local, gitignored `build-probe-listindexer/`
tree. **No production or test source was modified to produce any of this
evidence** — the working tree was clean and the committed headers are the
pre-fix headers, because this ticket changes no behaviour. `probe1_escapes.cpp`
is built with `-fno-access-control` **only** so it can read the private counter
and state what the counter did; that flag suppresses access checking and nothing
else, and no macro is defined over a library header.

```
build-probe-listindexer/build.sh probe1_escapes none    # then: <mode>
build-probe-listindexer/build.sh probe1_escapes ubsan
build-probe-listindexer/build.sh probe1_escapes asan
```

Output separates `invariants-failed` (0 in every mode — the probe would be wrong
otherwise) from `defects-observed`.

### 5.1 The indexer — `probe1_indexer-write.log`

```
native-iterator  counter 0 -> 0  *it=99  invalidation-detected=0
enumerator       indexer-write fail-fast=0
enumerator       control Add() fail-fast=1
equal-value      counter 0 -> 0  (.NET bumps unconditionally)
IList<T>&        counter 0 -> 0  value=77
const-list       decltype(list[0]) = const int&  (no write path)
invariants-failed=0
defects-observed=4
```

Read exactly: a native `std::vector` iterator taken before `list[0] = 99` stays
usable and silently observes the new value; an outstanding fail-fast
`GetEnumerator()` enumerator survives the same write; the counter does not move
for a changing write, for an equal-value write, or for a write through
`IList<T>&`. The `Add()` control line is the point that makes the rest evidence
rather than noise — **the guard works**; only this route bypasses it.

### 5.2 The other escapes — `probe1_escape-routes.log`

```
ToVector()[i]=v  counter 0 -> 0  value=42
ToVector() struct counter 0 -> 0  Count=0  fail-fast=0
    DEFECT: push_back/clear through ToVector() advanced nothing --
            a STRUCTURAL mutation invisible to the fail-fast guard
*begin()=v       counter 0 -> 0  value=55
enum void* write counter 0 -> 0  value=88  fail-fast=0
const List<T>    ToVector()/begin() are const-correct (no write path)
invariants-failed=0
defects-observed=4
```

The second line is the one that matters: with an enumerator outstanding, a
`push_back` **and** a `clear` through `ToVector()` left `Count == 0` and the
guard silent. The last defect line is ticket #1792's.

### 5.3 Retained references — `probe1_asan_*.log`

Four cases, each run as its own process so every AddressSanitizer report is
attributable. ASan + UBSan, `-fno-omit-frame-pointer -g`.

| Sub-mode | What is retained | ASan diagnostic |
|---|---|---|
| `realloc-read` | `int& r = list[0]`, then `Add()` reallocates (capacity 1 → 2) | **heap-use-after-free, READ of size 4** |
| `realloc-write` | same, then `r = 123` | **heap-use-after-free, WRITE of size 4** |
| `clear` | `std::string& r = list[0]`, then `Clear()` | **heap-use-after-free, READ of size 1** |
| `move-assign` | `std::string& r = a[0]`, then `a = std::move(b)` | **heap-use-after-free, READ of size 8** |

One honest qualification. In the `clear` case, reading `r.size()` produces **no**
ASan report and returns the stale 64: `std::vector::clear()` destroys the
elements but keeps its buffer, so the string's control block still sits inside a
live allocation. The lifetime has ended and the read is undefined behaviour all
the same — it is simply not one any sanitizer here detects. The report above
comes from the *following* line, `r.c_str()[0]`, which reaches the character
buffer `~basic_string` released. Both lines are in the log, in that order,
labelled.

This is why the indexer's `T&` is not only a fail-fast divergence: it is the
same hazard `LinkedListNode` (SR-AUD-357, ticket #1769) and `ReadOnlyDictionary::Empty`
(SR-AUD-362, ticket #1780) were repaired for.

### 5.4 Non-trivial element types — `probe1_nontrivial.log`

```
whole-element    counter 2 -> 2  name=gamma
member write     counter 1 -> 1  name=delta tag=42
compound/&addr   counter 0 -> 0  values=15,7,70
binding          T& / auto& / decltype(auto) / auto all resolve today
```

`list[0].tag = 42`, `list[0].renameInPlace(...)`, `list[0] += 10`, `++list[1]`,
and `*(&list[2]) = 70` all mutate and all advance nothing.

### 5.5 Copy and move — `probe1_copy-move.log`

```
copy-construct   src=1 dst=1 (inherited by design)
copy-assign      dst 0 -> 1  fail-fast=1
write-on-copy    original fail-fast=0 (must be 0)  a[0]=1 b[0]=99
invariants-failed=0
defects-observed=0
```

**Zero defects here.** #1787's assignment repair is intact, copy construction
still inherits the counter by design, and writing through a copy's indexer
correctly leaves the original's enumerator valid. Nothing in this ticket
disturbs any of it.

### 5.6 UBSan

All four non-lifetime modes under `-fsanitize=undefined
-fno-sanitize-recover=all`: **exit 0, 0 runtime errors**. The indexer path
contains no undefined behaviour of its own — the defect is a missing
notification, not bad arithmetic. Stated so the absence is on the record rather
than assumed.

---

## 6. Call-site and implementer inventory — measured

### 6.1 Method

`build/compile_commands.json` (625 entries, regenerated with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, which changes no compile flag) replayed
with `-fsyntax-only`, the shim first on the include path, and
`-Wno-error=deprecated-declarations` **appended** so the repository's own
`-Werror` cannot abort a translation unit at its first tagged site and hide the
rest. Coverage: **609 of 609** `.cpp` files under `modules/` plus 9 vendor and 7
other units — nothing on disk is missing from the database.

**The method's one real limitation:** a `[[deprecated]]` tag fires at
instantiation, so a use inside a template that no translation unit instantiates
is invisible to it. Nothing in the counts below is claimed to cover
uninstantiated templates, and nothing at all is claimed about CNA or
mobile-eggbert, which were not inspected.

### 6.2 Result

| Route | Unique call sites | Where |
|---|---:|---|
| `List<T>::operator[]` (non-const) | **61** | `CollectionsTests.cpp` 41, `ListTests.cpp` 20 |
| `List<T>::begin()` (non-const) | **3** | `CollectionsTests.cpp` 2, `ListTests.cpp` 1 |
| `List<T>::end()` (non-const) | **3** | `CollectionsTests.cpp` 2, `ListTests.cpp` 1 |
| `List<T>::ToVector()` (non-const) | **1** | `CollectionsTests.cpp` |
| `IList<T>::operator[]` (non-const) | **0** | — |
| **Total unique sites** | **65** | **two files, both tests** |

And, separately measured: **`System/Collections/Generic/List.hpp` is included by
zero library sources.** Its eight includers are four Collections tests, one Core
test, and three `test/consumer` fixtures. `List<T>` is a leaf public type in this
repository — public API, no internal dependents.

This is a direct correction of the ticket's own premise. It does **not** make
Phase 2 cheap: `List<T>` is shipped public API, the four `IList<T>` implementers
must all migrate, and CNA's usage is unmeasured and unmeasurable from here.

### 6.3 The four `IList<T>` implementers

| Implementer | Header | Has a mutation counter | Non-const `operator[]` today |
|---|---|---|---|
| `Generic::List<T>` | `Generic/List.hpp:148` | yes (`MutationCounter`) | returns `T&` |
| `ObjectModel::Collection<T>` | `ObjectModel/Collection.hpp:240` | **no** | returns `T&` |
| `ObjectModel::ReadOnlyCollection<T>` | `ObjectModel/ReadOnlyCollection.hpp:142` | **no** | throws `NotSupportedException` |
| `IntList` (test-local) | `Generic/ReadOnlyInterfacesTests.cpp:45` | no | returns `int&` |

The fourth is the important one: it is a hand-written implementation of the
public interface, in ordinary consumer style. It is direct evidence that
downstream code implements `IList<T>` by hand, and therefore that changing the
interface's virtual signature breaks consumers, not only this repository.

---

## 7. Current iterator and enumerator contract

| Accessor | Kind | Version-checked | Contract |
|---|---|---|---|
| `GetEnumerator()` → `IEnumerator<T>*` | caller-owned heap enumerator, snapshots `MutationVersion` at construction | **yes**, in `MoveNext()` and `Reset()`, equality only | `System::InvalidOperationException("Collection was modified; enumeration operation may not execute.")` |
| `begin()` / `end()` | raw `std::vector<T>::iterator` | **no** | plain `std::vector` invalidation rules, as the class comment already says |
| `begin() const` / `end() const` | `std::vector<T>::const_iterator` | **no** | as above |

Ticket 1713 established the fail-fast contract; #1787 preserved every increment
and guard site exactly and only changed the counter's representation. There are
14 increment sites and 3 read sites in `List<T>`; the guard compares with `==`
only, so nothing depends on ordering.

The contract's stated scope is *structural* modification —
"Add/Remove/Clear/Insert/etc." — and it is honoured for all of those. What it
does not cover is element replacement, which .NET does cover.

---

## 8. .NET comparison

Read from the local current .NET sources, not from memory.

### 8.1 The ordinary indexer — `List.cs:143-164`

```csharp
public T this[int index]
{
    get
    {
        if ((uint)index >= (uint)_size)
            ThrowHelper.ThrowArgumentOutOfRange_IndexMustBeLessException();
        return _items[index];
    }
    set
    {
        if ((uint)index >= (uint)_size)
            ThrowHelper.ThrowArgumentOutOfRange_IndexMustBeLessException();
        _items[index] = value;
        _version++;
    }
}
```

| Question | .NET's answer | This port today |
|---|---|---|
| Getter return semantics | **`T` by value** — a copy. Not `ref`, not `ref readonly`. | `const T&` (const overload) / `T&` (non-const) |
| Setter behaviour | assigns, then `_version++` | assigns, no bump |
| Does the setter advance `_version`? | **yes, unconditionally** | no |
| Does an **equal-value** write still advance it? | **yes** — the value is never compared | no |
| Exception type and order | bounds checked **first**, in both getter and setter, before any write: `ArgumentOutOfRangeException`, `paramName = "index"`, message `Index was out of range. Must be non-negative and less than the size of the collection.` | **identical** — `requireIndexInRange` throws the same type, paramName, and message, before writing |
| Active enumerators after an indexer assignment | invalidated: `MoveNext()`/`Reset()` compare `_version != _list._version` and throw `InvalidOperationException` (`List.cs:1207`, `:1241`) | **not** invalidated |
| `ref`-returning APIs on `List<T>` | **none.** `List.cs` contains no `ref T`, no `ref readonly`, and no public `Span<T>` member | `T&` from the indexer, plus routes 3-5 |
| Managed array element access exposed directly? | **no.** `_items` is private | `ToVector()` exposes the container |
| Non-generic `IList.this[int]` setter | `List.cs:173-189` — null check, then cast, then delegates to the generic setter, so it bumps too; a wrong type gives `ArgumentException` | n/a (this port's non-generic `IList` uses `getItem`/`setItem(void*)`) |

### 8.2 The explicitly unsafe escape hatch

`System.Runtime.InteropServices.CollectionsMarshal.AsSpan<T>(List<T>)` returns a
`Span<T>` over the backing array. Writes through it are invisible to `_version`,
exactly like this port's `ToVector()`. What .NET does with it is the part worth
copying:

- it lives in a class whose own summary says *"An **unsafe** class that provides
  a set of methods to access the underlying data representations of
  collections"*;
- it is in `System.Runtime.InteropServices`, not on `List<T>`;
- it is documented with the hazard at the call site: *"Items should not be added
  or removed from the `List<T>` while the `Span<T>` is in use."*
- the sibling `GetValueRefOrNullRef` for `Dictionary` carries the same warning.

So **.NET does have an untracked mutable escape** — it simply refuses to make it
the ordinary indexer, names it as unsafe, and quarantines it in a different
assembly-level surface. That is precisely the split §12 adopts.

### 8.3 What C++ cannot reproduce

C# has no user-visible references to storage. `T` is either a value type
(assignment copies; `list[i].Field = x` is **CS1612**, a compile error) or a
reference type (the indexer yields the handle, and mutating the *object* is not
mutating the *list*, so `_version` correctly stays put). Either way the CLR can
guarantee that the only way to change *which element the list holds* is the
setter.

C++ references are first-class aliases into the container's own storage. Once
`operator[]` returns `T&`:

- the caller can assign through it at any later point, with no member call;
- the caller can take its address and store the pointer;
- the caller can mutate members in place;
- the reference outlives nothing — any reallocation dangles it (§5.3);
- **no observer exists.** There is no C++ mechanism by which the collection is
  notified of a write through a reference it previously returned. This is not a
  gap in this port's implementation; it is a property of the language.

That last point is the whole design problem, and it is why §11 rejects every
alternative that claims to track mutation through an ordinary `T&`.

---

## 9. C++ reference constraints that bound the solution space

1. **`operator.` cannot be overloaded.** A proxy can forward `->`, but never
   `.`. So `list[i].member` — legal C# for a reference-type element, legal
   today — cannot survive any proxy design. Measured as cases 11, 12, 22 in
   §10.
2. **Implicit conversions are not considered during template argument
   deduction.** A proxy convertible to `const T&` still fails to match
   `template<class C,class T,class A> operator==(const basic_string<C,T,A>&,
   const C*)`, which is how `EXPECT_EQ(list[0], "abc")` is resolved. Fixable
   only by giving the proxy its own comparison operators (§13).
3. **A prvalue cannot bind to a non-const lvalue reference.** `T& r = list[i]`
   and `f(list[i])` where `f` takes `T&` stop compiling under any non-reference
   return. That is not collateral damage — it is the hole closing.
4. **Covariant return types apply to pointers and references only.** A virtual
   returning `T&` cannot be overridden by one returning a proxy *class*: GCC
   reports `conflicting return type specified`. So `List<T>` cannot change alone
   — `IList<T>` must change with it, and with it all four implementers. Measured
   in §10; this is the single largest component of Phase 2's cost.
5. **Taking the address of a prvalue is ill-formed.** `&list[i]` stops
   compiling. Intended.

---

## 10. Measured source break, per alternative

Each candidate was implemented as a generated shim over the committed headers
and the **whole repository** was recompiled against it with `-fsyntax-only`, 4
jobs. The shims are regenerated from the real headers on every run
(`make_shim_*.py`), so they cannot drift.

### 10.1 Whole-repository result

| Shim | What it does | TUs that stop compiling | Distinct broken sites |
|---|---|---:|---:|
| `shim-proxy` (A, minimal) | proxy return; migrates `IList<T>` + `List<T>` only | 10 / 625 | 4 |
| `shim-value` (B, minimal) | `getItem`/`setItem`, no mutable indexer; migrates `IList<T>` + `List<T>` only | 10 / 625 | 94 |
| **`shim-proxy2` (A, refined — SELECTED)** | proxy with compound assignment, inc/dec, and comparison; migrates **all four** implementers | **1 / 625** | **1** |
| `shim-value2` (B, refined) | as B, migrating all four implementers | 3 / 625 | 8 |

**The minimal pair's 4-against-94 asymmetry is reported and then discarded, not
relied on.** Both minimal shims leave the other three implementers unmigrated,
and that inflates both counts — but not equally: under the value shim every
unmigrated implementer of `IList<T>` and every subclass of `Collection<T>`
became *abstract* at once (`cannot declare variable ... to be of abstract type`
accounts for 90 of its 94 sites), whereas the proxy shim produced exactly one
`conflicting return type` per implementer. That asymmetry measures how the two
designs fail when a migration step is skipped, not what they really cost.
**The refined pair is the honest comparison, and it is far closer: 1 site
against 8.**

`shim-proxy`'s four sites:

```
ObjectModel/Collection.hpp:240          conflicting return type specified   (implementer, mechanical)
ObjectModel/ReadOnlyCollection.hpp:142  conflicting return type specified   (implementer, mechanical)
ReadOnlyInterfacesTests.cpp:45          conflicting return type specified   (implementer, mechanical)
vendor/googletest/gtest.h:1394          no match for 'operator==' (proxy vs const char[N])
```

Three of the four are the *interface* change reaching its implementers, which is
migration, not breakage of call-site code. **Only one is a real call-site
break**, and it is the template-deduction problem of §9.2, which §13's proxy
fixes with its own comparison operators.

`shim-value`'s 94 sites are dominated by `cannot declare variable ... to be of
abstract type` — every unmigrated implementer of `IList<T>` and every subclass
of `Collection<T>` (including `ObservableCollection<T>` and three test-local
subclasses) became abstract at once. Only 2 of the 94 are true call-site breaks
(`assignment of read-only location`).

### 10.2 Expression-level matrix — `probe2_matrix.log`

The same source compiled 24 times per column, one real-world expression shape
per case, `-Wall -Wextra -Wpedantic`, **without** `-fno-access-control` — this is
what an ordinary consumer translation unit can do.

| # | Expression | baseline | proxy v1 | **proxy v2** | value | Verdict |
|---|---|---|---|---|---|---|
| 1 | `int x = list[0];` | OK | OK | OK | OK | read preserved |
| 2 | `list[0] = 99;` | OK | **OK** | **OK** | **FAIL** | .NET's own spelling — B loses it |
| 3 | `int& r = list[0];` | OK | FAIL | FAIL | FAIL | **intended** — the hole |
| 4 | `auto& r = list[0];` | OK | FAIL | FAIL | FAIL | **intended** |
| 5 | `const int& r = list[0];` | OK | OK | OK | OK | read preserved |
| 6 | `decltype(auto) r = list[0]; r = 7;` | OK | OK | OK | FAIL | proxy keeps it |
| 7 | `auto v = list[0];` | OK | OK | OK | OK | copy preserved |
| 8 | `int* p = &list[0];` | OK | FAIL | FAIL | FAIL | **intended** |
| 9 | `list[0] += 5;` | OK | FAIL | **OK** | FAIL | incidental — fixed in v2 |
| 10 | `++list[0];` | OK | FAIL | **OK** | FAIL | incidental — fixed in v2 |
| 11 | `points[0].x = 42;` | OK | FAIL | FAIL | FAIL | **unavoidable** (§9.1) |
| 12 | `points[0].sum();` | OK | FAIL | FAIL | OK | **unavoidable** for the proxy |
| 13 | `genericMax(list[0], list[1])` | OK | OK | OK | OK | — |
| 14 | `std::swap(list[0], list[1]);` | OK | FAIL | FAIL | FAIL | **intended** |
| 15 | `genericEquals(list[0], 10)` | OK | OK | OK | OK | — |
| 16 | `list[0] = list[1];` | OK | OK | OK | FAIL | proxy keeps it |
| 17 | `takesConstRef(list[0]);` | OK | OK | OK | OK | — |
| 18 | `takesMutableRef(list[0]);` | OK | FAIL | FAIL | FAIL | **intended** |
| 19 | `list[0] + list[1]` | OK | OK | OK | OK | — |
| 20 | `std::sort(list.begin(), list.end())` | OK | OK | OK | OK | route 4 untouched |
| 21 | `const List<int>& c = list; c[0];` | OK | OK | OK | OK | — |
| 22 | `s[0] = std::string("x"); s[0].size();` | OK | FAIL | FAIL | FAIL | member access, §9.1 |
| 23 | `list[0] > 5 ? list[1] : 0` | OK | OK | OK | OK | — |
| 24 | `for (auto& e : list) e += 1;` | OK | OK | OK | OK | route 4 untouched |

Totals: baseline 24/24 compile; proxy v1 breaks 10; **proxy v2 breaks 8**; value breaks 11. v2's two recoveries over v1 are cases 9 and 10 — compound assignment and increment — which v1 broke incidentally and v2 forwards and tracks.

Read the two columns against each other and the decision is visible: the value
alternative breaks **`list[0] = 99`** — the exact spelling .NET uses and the
exact thing this ticket is trying to preserve — while the proxy keeps it. The
proxy's breaks are concentrated in reference-binding (3, 4, 8, 14, 18), which is
the hole closing on purpose, plus member access (11, 12, 22), which is the
genuine, unavoidable cost.

### 10.3 Refined pair — the comparison the decision rests on

Both refined shims migrate all four `IList<T>` implementers, so what remains is
real breakage rather than a skipped migration step.

**`shim-proxy2` — 1 translation unit, 1 site:**

```
modules/collections/tests/System/Collections/Generic/ReadOnlyInterfacesTests.cpp:45:
    conflicting return type specified for 'virtual int& {anonymous}::IntList::operator[](int)'
```

That is the hand-written `IList<T>` implementer of §6.3 — a one-line return-type
change. **Every one of the 61 measured indexer call sites still compiles**, and
the gtest `operator==` break that `shim-proxy` produced is gone, fixed by the
proxy's own comparison operators (§9.2, §13.1).

**`shim-value2` — 3 translation units, 8 sites:**

| Diagnostic | Count | Nature |
|---|---:|---|
| `cannot declare variable 'lst' to be of abstract type 'IntList'` | 5 | the same hand-written implementer |
| `'int& IntList::operator[](int)' marked 'override', but does not override` | 1 | same implementer |
| `assignment of read-only location 'lst...operator[](0)'` | **2** | **real call-site breaks** |

The two real breaks are `ListTests.cpp:33` and `CollectionsTests.cpp:65`, both
`lst[0] = 42;`. Those are the *only* two indexer writes in the repository —
verified independently by search, so the count is complete rather than truncated
by an early compiler abort.

**So the measured in-repository cost of the two closing alternatives is 1 site
against 8, of which 1 against 6 is the same mechanical implementer migration and
0 against 2 is genuine call-site breakage.** Neither is expensive here. The
decision is therefore made on §11's qualitative grounds — B removes `list[i] = v`
from the API — and not on these numbers.

### 10.4 The measurement's limits, stated

- The shim measures **this repository only**. CNA and mobile-eggbert were not
  inspected, searched, configured, built, or modified, by instruction.
- Uninstantiated templates are invisible to both the deprecation sweep and the
  break sweep.
- `-fsyntax-only` does not link, so nothing here measures a *symbol* break; §12.4
  covers that separately.
- Both refined shims give `Collection<T>` whatever it needs to compile, which for
  the proxy means **adding a mutation counter it does not have**. That is a real
  Phase 2 cost, not a shim artefact (§12.3).

---

## 11. Alternatives evaluated

| # | Alternative | Verdict |
|---|---|---|
| **A** | **Proxy reference from the non-const indexer** | **SELECTED**, refined (§13). The only alternative that closes the write path while keeping `list[i] = v` compiling. Measured least-disruptive of the closing alternatives (§10). Costs: member access on value-type elements is unavoidably lost (§9.1); `IList<T>` and all four implementers must migrate (§9.4); `Collection<T>` needs a counter it lacks (§12.3). |
| **B** | **Return by value / `const T&` plus explicit `setItem()`** | Rejected as the primary mechanism, **adopted as Phase 1's additive half.** The rejection rests on one thing: it removes `list[i] = v` — the only spelling .NET's own indexer has — from the API, so every ported C# assignment must be rewritten. It is *not* rejected on measured cost: refined, it breaks 8 sites against the proxy's 1, of which only 2 are genuine call-site breaks (§10.3), and that difference is too small to decide anything by itself. Its `setItem()` is nonetheless the right *addition*: it matches `ArrayList::setItem`/`Hashtable::setItem`, which already pair a bumping setter with a non-bumping reference indexer, and it matches CLAUDE.md rule 5. Adding it is free; *removing the indexer* is what costs. Note also the one place B is strictly better than A: it keeps read-only member access (`list[i].method()`, case 12) working, because `const T&` is a real reference. |
| **C** | **Keep `T&`, document it as unsafe, add a tracked setter** | Rejected as a *closure* and adopted as Phase 1. It leaves `list[i] = v` silently bypassing the guard, so by the ticket's own standard it documents the defect rather than remediating it. It is still worth doing on its own merits — it gives callers a correct path with zero break — which is exactly why it is Phase 1 and not the answer. |
| **D** | **Eager invalidation: bump on every non-const `operator[]` call** | Rejected, and it is the only alternative with **zero** source break, so the rejection is argued rather than assumed. Three defects. (i) **Reads invalidate.** 41 of the 61 measured call sites are reads on a non-const list; every one would start throwing mid-enumeration. (ii) **Retained references still mutate freely** after the single bump, so the hole is narrowed, not closed. (iii) An enumerator created *after* the reference was taken still misses every later write through it. It converts a silent miss into a loud false positive without closing the defect. |
| **E** | **Tracked edit guard (an RAII object bracketing the mutation)** | Rejected. It closes nothing on its own: ordinary `operator[]` remains available and unsafe, so it is Alternative C with more ceremony. It also has no .NET analogue, needs a new public type in the most call-site-heavy surface, and its exception safety is worse than the proxy's — a guard destroyed during stack unwinding must decide whether a partially applied edit counts as a mutation. The proxy's write is a single expression that either happened or did not. |
| **F** | **Redefine the contract: element replacement does not invalidate** | Rejected as the *primary* answer, adopted for routes 4 and 5. Redefining it for the indexer is a deliberate, permanent divergence from .NET on the one type whose .NET reference is unambiguous, and it cannot be stated safely in general: this port's `T` is a value type in storage, so "non-structural" replacement of a `std::string` element frees and reallocates that element's buffer, which is exactly what an enumerator holding a reference must not miss. For `begin()`/`end()`, however, it is already the documented contract and is the correct one: they are STL-interop extensions with no .NET counterpart, and they follow `std::vector` rules, as `List.hpp` already says. |
| **G** | **Generation-aware element cells (`std::vector<Cell<T>>`)** | Rejected. Every element gains a back-pointer or generation word, so `sizeof` per element grows, `ToVector()` can no longer return `std::vector<T>&` at all, contiguity of `T` is lost, and any future `data()`/span accessor becomes impossible. It changes the storage of the most-used collection in the port to solve a notification problem the proxy solves with a 16-byte temporary that never outlives the full expression. It is also the only alternative that would make `List<T>` incompatible with the `std::vector<T>` interop the class exists to provide. |
| **H** | **Do nothing; record the divergence as permanent** | Rejected. This is acceptance-criteria route (a), and it is defensible only if no compatible correction exists *and* the residual risk is acceptable. The first is true (§1); the second is not: the same `T&` is a reproduced use-after-free (§5.3), and the guard's silence is what lets a program keep walking storage that was replaced. Recording it as permanent would also freeze `ToVector()`'s structural hole, which nothing has ever documented. |

### 11.1 Comparison matrix

Scored against the selected criteria. ✅ good, ⚠️ partial, ❌ bad.

| Criterion | A (proxy) | B (value+setter) | C (document) | D (eager) | E (guard) | F (redefine) | G (cells) |
|---|---|---|---|---|---|---|---|
| Closes the indexer write path | ✅ | ✅ | ❌ | ⚠️ | ❌ | ❌ (by fiat) | ✅ |
| Memory safety of retained refs | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Mutation tracking exact | ✅ | ✅ | ❌ | ❌ | ⚠️ | n/a | ✅ |
| Enumerator semantics match .NET | ✅ | ✅ | ❌ | ❌ | ⚠️ | ❌ | ✅ |
| `list[i] = v` still compiles | ✅ | ❌ | ✅ | ✅ | ⚠️ | ✅ | ✅ |
| `list[i].member` still compiles | ❌ | ⚠️ read only | ✅ | ✅ | ✅ | ✅ | ❌ |
| .NET parity of the getter | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Public source compatibility | ❌ | ❌❌ | ✅ | ✅ | ❌ | ✅ | ❌ |
| Symbol compatibility | ⚠️ | ⚠️ | ✅ | ✅ | ⚠️ | ✅ | ❌ |
| Object-layout compatibility | ⚠️ `Collection<T>` | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Contiguity / `std::vector` interop | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Runtime cost | ✅ nil | ✅ nil | ✅ nil | ❌ bump per read | ⚠️ | ✅ | ❌ |
| Migration burden (measured) | ✅ 4 sites | ❌ 94 sites | ✅ 0 | ✅ 0 | ❌ | ✅ 0 | ❌ |
| Testability | ✅ | ✅ | ⚠️ | ✅ | ⚠️ | ✅ | ✅ |
| New module dependencies | ✅ none | ✅ none | ✅ none | ✅ none | ✅ none | ✅ none | ✅ none |

---

## 12. Selected architecture

### 12.1 The split, stated once

`List<T>` gets **two clearly separated surfaces**, mirroring the split .NET
itself draws between `List<T>.this[int]` and `CollectionsMarshal.AsSpan`:

- **The tracked surface** — `operator[]`, `getItem()`, `setItem()`. Every write
  advances the mutation counter; every outstanding enumerator fails fast. This
  is the surface ported code should use, and it is the one that matches .NET.
- **The explicitly unsafe surface** — `ToVector()`, `begin()`, `end()`. Raw
  access to `std::vector<T>` storage for STL interop. Writes are **not** tracked,
  references dangle on reallocation, and the header says so at each declaration
  in the same terms `CollectionsMarshal` uses. These are extensions with no .NET
  counterpart; constraining them would break the interop the class exists to
  provide, and .NET's own precedent is to keep such a hatch and name it clearly.

The defect is closed for the tracked surface and *documented, not silently
present,* for the unsafe one. That distinction is the whole design.

### 12.2 Phase 1 — additive, no approval required

Pure addition to `List<T>`. No signature changes, no layout change, no
behavioural change to anything that exists.

### 12.3 Phase 2 — the breaking half

The non-const indexer returns `ElementReference<T>`; `IList<T>` changes with it;
all four implementers migrate. Two object-layout consequences, both of which are
approval-bearing:

1. **`ObjectModel::Collection<T>` has no mutation counter.** To hand out a
   tracked reference it must gain one, which grows the type. The alternative —
   giving `Collection<T>` an untracked proxy — would make the same expression
   mean "tracked" or "untracked" depending on the static type, which is worse
   than either. Measured (`probe3_layout_baseline.log` →
   `probe3_layout_proxy2.log`, LP64/GCC 14):
   **`sizeof(Collection<int>)` and `sizeof(Collection<std::string>)` 32 → 40**,
   `alignof` 8 → 8.
2. **`ReadOnlyCollection<T>`** needs only a return-type change; its non-const
   indexer already throws `NotSupportedException` and continues to.

### 12.4 What Phase 2 does *not* change

`sizeof(List<T>)` (measured **40 → 40** for both `List<int>` and
`List<std::string>`), `alignof(List<T>)` (8 → 8), the counter's offset,
`sizeof(List<T>::Enumerator)`, `sizeof(ReadOnlyCollection<int>)` (24 → 24),
`is_polymorphic` (true → true), copy/move constructibility and assignability
(all four still true), the storage type (`std::vector<T>`, contiguous),
every `const` overload, every exception type/paramName/message, the enumerator's
guard, and every increment site. The proxy is a 16-byte prvalue (two pointers)
that never outlives the full-expression that creates it; it is not stored in
`List<T>` and adds no member — measured `sizeof(decltype(list[0])) == 16`,
`alignof == 8`, against a reference today. The const indexer's result stays a
reference in both columns.

---

## 13. Exact proposed public declarations

Validated by `build-probe-listindexer/shim-proxy2/` compiling against the whole
repository. These are the declarations #1791 implements; it should not need to
redesign the contract.

### 13.1 New header — `System/Collections/detail/ElementReference.hpp`

```cpp
// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <utility>

#include "System/Collections/detail/MutationCounter.hpp"

namespace System::Collections::detail
{
    template <typename T>
    class ElementReference
    {
        T* slot_;
        MutationCounter* version_;

    public:
        using value_type = T;

        constexpr ElementReference(T* slot, MutationCounter* version) noexcept;
        constexpr ElementReference(const ElementReference&) noexcept = default;

        // ---- reads (never advance the counter) ----
        constexpr operator const T&() const noexcept;
        [[nodiscard]] constexpr const T& getValueProperty() const noexcept;
        const T* operator->() const noexcept;

        // ---- tracked writes (advance the counter exactly once each) ----
        ElementReference& operator=(const T& value);
        ElementReference& operator=(T&& value);
        ElementReference& operator=(const ElementReference& other);

        template <typename U> ElementReference& operator+=(U&& rhs);
        //  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=   identically
        ElementReference& operator++();
        ElementReference& operator--();
        T operator++(int);
        T operator--(int);

        // ---- comparison (hidden friends; see section 9.2) ----
        template <typename U> friend bool operator==(const ElementReference&, const U&);
        template <typename U> friend bool operator==(const U&, const ElementReference&);
        friend bool operator==(const ElementReference&, const ElementReference&);
    };
}
```

### 13.2 `IList<T>` — Phase 2

```cpp
[[nodiscard]] virtual const T& operator[](intcs index) const = 0;          // unchanged
virtual System::Collections::detail::ElementReference<T>
        operator[](intcs index) = 0;                                       // CHANGED
[[nodiscard]] virtual const T& getItem(intcs index) const = 0;             // NEW (Phase 2)
virtual void setItem(intcs index, const T& value) = 0;                     // NEW (Phase 2)
```

### 13.3 `List<T>` — Phase 1 then Phase 2

```cpp
// Phase 1 -- additive, non-virtual, no approval needed.
[[nodiscard]] const T& getItem(intcs index) const;      // bounds-checked read
void setItem(intcs index, const T& value);              // bounds-checked, ++version_

// Phase 2 -- the return-type change.
[[nodiscard]] const T& operator[](intcs index) const override;             // unchanged
System::Collections::detail::ElementReference<T>
        operator[](intcs index) override;                                  // CHANGED
```

### 13.4 Internal representation

`List<T>`'s members are **unchanged**: `std::vector<T> items_;` and
`System::Collections::detail::MutationCounter version_;`. `ElementReference<T>`
stores `T*` + `MutationCounter*` = 16 bytes on LP64, is trivially copyable, owns
nothing, allocates nothing, and is never stored by the collection.

---

## 14. Mutation and versioning rules

1. **Every write through the tracked surface advances the counter exactly
   once**: `operator=` (copy, move, and proxy-to-proxy), every compound
   assignment, `operator++`/`operator--` in both forms, and `setItem()`.
2. **An equal-value write still advances it.** `list[i] = list[i]` bumps. This
   matches .NET, which never compares the old value (`List.cs:161-162`), and it
   avoids requiring `T` to be equality-comparable. It is the same rule
   `SortedList<K,V>` and `OrderedDictionary<K,V>` already follow, and the
   opposite of `Dictionary<K,V>`'s — each matching its own .NET reference.
3. **Reads never advance it**: the conversion operator, `getValueProperty()`,
   `operator->`, `getItem()`, the const `operator[]`, `Contains`, `IndexOf`,
   `Find*`, `ForEach`, `CopyTo`, `ToArray`, `ToVector() const`, and iteration.
4. **A throwing write advances nothing.** Bounds are validated before the proxy
   is constructed, so an out-of-range index never reaches the counter.
5. **The unsafe surface advances nothing**, by documented design (§12.1).
6. Copy construction still inherits the counter; assignment still advances the
   destination's (#1787). Unchanged.

---

## 15. Retained-reference rules

1. `operator[]` yields a **prvalue proxy**, valid only for the full-expression
   that created it. Storing one (`auto r = list[0];`) is legal C++ but binds to
   a slot that any structural mutation invalidates, exactly as `T&` did; the
   header says so.
2. **`const T&` obtained from the const indexer, `getItem()`, or the proxy's
   conversion follows `std::vector` invalidation rules** — valid until the next
   reallocating or erasing operation. Unchanged from today.
3. **Obtaining a plain mutable `T&` from the tracked surface stops being
   possible.** `T& r = list[i]`, `auto& r = list[i]`, `&list[i]`, and passing to
   a `T&` parameter no longer compile (§10.2 cases 3, 4, 8, 18). This is the
   defect closing, and it is what removes the four reproduced use-after-free
   shapes of §5.3 from the ordinary surface.
4. A mutable `T&` remains obtainable from the **unsafe** surface —
   `*list.begin()`, `list.ToVector()[i]` — where the hazard is documented.

---

## 16. Exception matrix

Unchanged in every row; listed so #1791 has no latitude to drift.

| Operation | Condition | Exception | paramName | Message |
|---|---|---|---|---|
| `operator[] const`, `operator[]`, `getItem`, `setItem` | `index < 0` or `index >= Count` | `System::ArgumentOutOfRangeException` | `"index"` | `Index was out of range. Must be non-negative and less than the size of the collection.` |
| `MoveNext()`, `Reset()` | counter differs from snapshot | `System::InvalidOperationException` | — | `Collection was modified; enumeration operation may not execute.` |
| `ReadOnlyCollection<T>::operator[]` (non-const), `setItem` | always | `System::NotSupportedException` | — | `Collection is read-only.` |

Ordering: bounds first, always, before any write and before the counter moves —
matching `List.cs:155-163`.

---

## 17. Copy, move, assignment, contiguity

- `List<T>` remains copy/move constructible and assignable; `is_polymorphic`
  stays `true`; no trait a consumer could rely on changes.
- Copy construction inherits the counter; copy/move assignment advances the
  destination's (#1787, unchanged).
- Storage stays `std::vector<T>` — **contiguous**, so a future `data()` or span
  accessor stays possible. This is the principal reason Alternative G is
  rejected.
- `ToVector()` keeps returning `std::vector<T>&` / `const std::vector<T>&`; the
  contract comment changes, the signature does not.
- `ElementReference<T>` is copyable (it copies the *alias*, like a pointer);
  assigning one proxy from another assigns **through** to the element and bumps
  (§13.1), which is the behaviour `list[i] = list[j]` needs.

---

## 18. Permanent test plan

Delivered by this ticket:
`modules/collections/tests/System/Collections/Generic/ListIndexerVersionTests.cpp`,
in `SharpRuntimeTests_Collections_Core`, split into two suites on purpose:

- **`ListIndexerVersionContract`** (8 cases) — behaviour that must survive #1791
  unchanged: reads never invalidate; `Add`/`Insert`/`RemoveAt`/`Clear` always do;
  the bounds/exception contract including paramName and message and the
  "nothing is written on a throw" rule; empty-list indexing; writing to a copy
  never disturbs the original; #1787's assignment repair; const-correctness of
  both const accessors; replacement replaces without resizing.
- **`ListIndexerVersionDivergence`** (6 cases) — the measured divergence, each
  asserting today's behaviour with .NET's named in a comment, so #1791 must
  consciously flip it: the indexer write during enumeration, the equal-value
  write, the write through `IList<T>&`, the `begin()` escape, the `ToVector()`
  **structural** escape, and a `static_assert` pinning that `operator[]` still
  returns `int&` and `ToVector()` still returns `std::vector<int>&`.

The `static_assert`s are the load-bearing part: #1791 physically cannot land
without editing them.

#1791 adds, on top: proxy read/write/compound/increment semantics; equal-value
bump; proxy-to-proxy assignment; `getItem`/`setItem` including their throw
paths; that a tracked write invalidates every outstanding enumerator; that the
unsafe surface still does not; overload resolution and `decltype` assertions;
and layout `static_assert`s for `sizeof(List<T>)` and
`sizeof(ElementReference<T>)`.

---

## 19. Sanitizer plan

- **ASan** — the four retained-reference shapes of §5.3, re-run under #1791 to
  show the ordinary surface can no longer express three of them (the fourth,
  through `ToVector()`, remains reachable **by design** and stays as a pinned,
  documented residual).
- **UBSan** — the whole permanent suite; expected 0, as today (§5.6).
- **LSan** — the permanent suite, because `GetEnumerator()` hands back a
  caller-owned raw pointer and #1787's own first draft leaked one.
- **TSan** — **not planned, and the reason is stated rather than omitted.** This
  design adds no atomic, no `mutable` cache, and no hidden `const` write; the
  counter is a plain non-atomic field before and after, written from the same
  sites. `List<T>` claims no thread safety and this design adds none. A TSan run
  would substantiate nothing that #1787's `probe3` has not already covered for
  the counter itself.

---

## 20. Consumer-fixture plan

For #1791, following the established pattern in `test/consumer/`:

- **positive** — `test/consumer/collections_list_indexer.cpp`, compiled against
  the public `Collections.Core` surface only with `-Wall -Wextra -Wpedantic
  -Werror`: reads through both overloads, a tracked write, a tracked compound
  assignment, `getItem`/`setItem`, and a fail-fast assertion after an index
  write. Exits 0.
- **negative** — a fixture that attempts `int& r = list[0];` and `&list[0]` and
  must **fail to compile**, proving the escape is closed rather than merely
  discouraged. This is the fixture that makes the whole ticket verifiable.
- `scripts/check_selective_components.sh Collections.Core` in isolation.

---

## 21. Migration guidance

For code that stops compiling under Phase 2:

| Was | Becomes | Why |
|---|---|---|
| `int& r = list[i];` | `list.setItem(i, v);` or `list[i] = v;` | the reference was untracked and dangling-prone |
| `auto& r = list[i];` then `r = v;` | `list[i] = v;` | one tracked expression |
| `const T& r = list[i];` | unchanged | reads are unaffected |
| `&list[i]` | `&list.ToVector()[i]` | explicitly opting into the unsafe surface |
| `list[i].member = v;` | `T copy = list[i]; copy.member = v; list[i] = copy;` | C# forbids this for value types too (CS1612) |
| `list[i].constMethod();` | `list.getItem(i).constMethod();` or `list[i]->constMethod()` | `operator.` is not overloadable |
| `std::swap(list[i], list[j]);` | `T t = list[i]; list[i] = list[j]; list[j] = t;` | both writes tracked |
| `EXPECT_EQ(list[i], "abc")` | unchanged | the proxy's own `operator==` handles it |
| implementing `IList<T>` by hand | change the non-const `operator[]` return type; add `getItem`/`setItem` | mechanical, one method each |

---

## 22. Performance impact

Expected **nil**, to be confirmed by #1791 rather than claimed here.

The proxy is two pointers constructed in registers, consumed within the
full-expression, and eliminated at `-O2` in every case where the current code
returns a reference — the generated code for `list[i] = v` becomes a store plus
the same `++version_` increment that `Add()` already performs. Reads gain one
indirection that the conversion operator inlines away. No allocation, no branch,
no lock, on any path. `#1787` measured `List<int>` `Add`+`RemoveAt` at 0.66
ns/op and an enumerated element at 1.12 ns/op; #1791 should re-run
`probe5_perf`-style measurements with an `asm volatile` barrier per iteration —
without one GCC hoists the loop-invariant call out and the benchmark measures
nothing, the mistake `#1786` §13.1 recorded.

---

## 23. Risks and residual limitations

| # | Risk | Severity | Position |
|---|---|---|---|
| 1 | `list[i].member` stops compiling for value-type elements | **Medium** | **Real, unavoidable, and the principal cost of the selected design.** `operator.` cannot be overloaded (§9.1). Mitigated by `getItem()`, `operator->`, and a mechanical rewrite (§21) — not eliminated. | **OK** |
| 2 | The unsafe surface (`ToVector()`, `begin()`, `end()`) still bypasses the counter after Phase 2 | **Medium** | **Deliberate and documented**, mirroring `CollectionsMarshal.AsSpan`. It is the honest position, but it does mean #1790 is not "the last untracked write path" — it is the last *ordinary* one. Pinned by a permanent test so it cannot be forgotten. | **OK** |
| 3 | CNA and mobile-eggbert's usage is unmeasured | **Medium** | Out of scope by instruction; not inspected, searched, built, or modified. The in-repo figure of 61 sites is **not** offered as a proxy for theirs. #1791's approval request must be answered with that unknown in view; ticket #1773 remains blocked. | **FAIL** |
| 4 | `ObjectModel::Collection<T>` grows a mutation counter | Low–Medium | An object-size change to a second public type, in the same approval category as #1788/#1789. Exact `sizeof` in §12.3. | **FAIL** |
| 5 | Uninstantiated templates are invisible to the measurement | Low | Stated in §10.4. Both sweeps share the limitation, so the A-vs-B comparison is unaffected. | **OK** |
| 6 | The proxy's templated `operator==` is a broad overload | Low | Hidden friends: they are found by ADL only when one operand really is an `ElementReference`, so they cannot hijack unrelated comparisons. | **OK** |
| 7 | Phase 1 alone leaves the defect open | **High if Phase 2 is never approved** | Phase 1 gives a correct path; it does not remove the incorrect one. If Phase 2 is declined, the divergence becomes permanent by decision and this document is where that decision is recorded (§28). | **OK** |
| 8 | `probe1` depends on `-fno-access-control` | Low | Probes only. The permanent suite uses no seam at all; it asserts through the public API. | **FAIL** |

---

## 24. Rejected approaches, in one place

Alternatives B (as primary), D, E, F (as primary), G, and H are rejected in §11
with the reasoning attached to each. The two rejections most likely to be
revisited, restated so they are not re-litigated by accident:

- **D (eager invalidation) is rejected despite being the only zero-break
  option.** 41 of 61 measured call sites are *reads* on a non-const list; making
  reads invalidate would break more programs at runtime than the proxy breaks at
  compile time, and it still would not close the hole.
- **H (record as permanent) is rejected** because the same `T&` is a reproduced
  use-after-free, not merely a fail-fast divergence.
- **B is not rejected for being expensive.** Measured, it is nearly as cheap as
  A inside this repository (8 sites against 1). It is rejected for deleting
  `list[i] = v`. If a future decision weighs read-only member access
  (`list[i].method()`, which B keeps and A cannot) above C# assignment syntax,
  B is the correct choice and this document should be revised rather than
  worked around.

---

## 25. Implementation-ticket scope

Ticket **#1791**, `REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, opened
**blocked**. Not begun.

**Phase 1 — no approval required**
1. `List<T>::getItem(intcs) const` and `List<T>::setItem(intcs, const T&)`,
   bounds-checked, `setItem` advancing `version_`.
2. Correct `List.hpp`'s class comment (already done by #1790 for the *factual*
   errors; #1791 documents the new members).
3. Permanent tests for both, including their throw paths.

**Phase 2 — blocked on §28**
4. `System/Collections/detail/ElementReference.hpp` (§13.1).
5. `IList<T>`: non-const `operator[]` returns the proxy; `getItem`/`setItem`
   become pure virtual.
6. Migrate `List<T>`, `ObjectModel::Collection<T>` (adding a counter), and
   `ObjectModel::ReadOnlyCollection<T>`.
7. Migrate the test-local `IntList` and the 61 + 3 + 3 + 1 measured call sites.
8. Document `ToVector()`/`begin()`/`end()` as the explicitly unsafe surface, in
   `CollectionsMarshal`'s terms.
9. Permanent tests, both consumer fixtures, ASan/UBSan/LSan runs, layout and
   symbol probes, `README.md` behaviour-change entry.

**Rollback.** Phase 1 is independently revertible with `git revert` and leaves
nothing behind. Phase 2's revert restores `T&` and with it the whole defect; a
revert must be validated by re-running `probe1_escapes` under ASan, not by CTest
alone, because the permanent suite's `static_assert`s would be reverted with it
and would then agree with the old behaviour.

**Explicitly excluded from #1791**: `SortedSet<T>` (#1786, done); `LinkedList<T>`
and `BitArray` counter widening (#1788/#1789, blocked); nested-view exception
ordering (#1785); the `IEnumerator::getCurrentProperty()` `void*` defect
(#1792); CNA and mobile-eggbert (#1773, blocked); and any change to
`Dictionary<K,V>`'s deliberate no-bump-on-overwrite behaviour, which matches its
own .NET reference and is correct.

---

## 26. What this ticket changed

| File | Change |
|---|---|
| `docs/ListIndexerVersioningDesign.md` | this record (new) |
| `modules/collections/tests/System/Collections/Generic/ListIndexerVersionTests.cpp` | permanent suite (new) |
| `modules/collections/include/System/Collections/Generic/List.hpp` | **doc-comment only** — corrects the inaccurate "one narrow gap" claim and records the `ToVector()` structural escape. No signature, behaviour, layout, or exception change. |
| `plan.sqlite3`, `NEXT.md`, `plan.md` | planning reconciliation; #1790 → `done`, #1791 and #1792 opened inactive |
| `audit/AUDIT_FINAL_REPORT.md`, `audit/AUDIT_PROGRESS.md` | design-batch record; **no `SR-AUD-*` issued, no finding reopened, findings index unchanged** |
| `audit/…/Generic/List.hpp.audit.md`, `audit/…/Generic/IEnumerator.hpp.audit.md` | follow-up notes appended below the original evidence, which is retained unchanged |
| `README.md`, `CLAUDE.md` | recorded test floor raised 13,463 → 13,477, matching the +14 permanent regressions |

`build-probe-listindexer/` is repository-local and gitignored (`build*`).

---

## 27. Follow-up ticket map

| Ticket | Key | Status | Scope |
|---|---|---|---|
| **#1791** | `REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT` | **blocked** (approval, §28) | Phases 1 and 2 above |
| **#1792** | `REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST` | **todo**, inactive | §27.3 |
| #1785 | `REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER` | **todo**, untouched | unchanged by this ticket |
| #1788 | `REMED-COLL-LINKEDLIST-VERSION-WIDEN` | **blocked**, untouched | unchanged |
| #1789 | `REMED-COLL-BITARRAY-VERSION-WIDEN` | **blocked**, untouched | unchanged |
| #1773 | `REMED-COLL-COPYTO-DOWNSTREAM` | **blocked**, untouched | out-of-repository |

### 27.3 The newly discovered defect — ticket #1792

`Generic/IEnumerator.hpp:38-41`:

```cpp
void* getCurrentProperty() const override {
    return const_cast<T*>(&Current());
}
```

`Current()` returns `const T&` precisely so an enumerator cannot be used to
mutate what it is walking. This bridge `const_cast`s that away and publishes a
`void*` to the live element on the **non-generic** `IEnumerator` interface, which
is public. Reproduced in §5.2: writing through it changed the element while an
enumerator was mid-walk, and the guard stayed silent — the counter never moved.

It is **not** absorbed into #1790 because it belongs to `IEnumerator<T>`, not
`List<T>`, and therefore affects every collection in the repository. No new
`SR-AUD-*` identifier is issued; the numbering stays frozen at 364.

---

## 28. Exact user approval required

Phase 1 of #1791 needs **no** approval and may be begun whenever it is
scheduled.

Phase 2 of #1791 is **blocked** pending explicit user approval of all four of
the following, together, scoped to that ticket:

1. **A public source-breaking change to `List<T>`:** the non-const
   `operator[](intcs)` return type changes from `T&` to
   `System::Collections::detail::ElementReference<T>`, so `T& r = list[i]`,
   `auto& r = list[i]`, `&list[i]`, `std::swap(list[i], list[j])`, passing
   `list[i]` to a `T&` parameter, and **member access on a value-type element**
   (`list[i].member`, `list[i].method()`) stop compiling.
2. **A public source-breaking change to the `IList<T>` interface:** the same
   return-type change on `virtual T& operator[](intcs)`, plus two new pure
   virtuals (`getItem`, `setItem`). Every implementer — including hand-written
   ones in consumer code, of which this repository's own test suite contains an
   example — must be migrated.
3. **An object-layout change to `System::Collections::ObjectModel::Collection<T>`**,
   which gains a mutation counter it does not currently have, requiring every
   consumer to be rebuilt. This is the same approval category as #1771, #1780,
   #1783, #1788, and #1789.
4. **Acknowledgement that CNA and mobile-eggbert's usage of `List<T>::operator[]`
   is unmeasured**, because those repositories are out of scope and were not
   inspected. The 61-call-site figure in §6.2 is this repository only and is
   explicitly not a proxy for theirs.

A previous approval — for `CopyTo` (#1771), `ReadOnlyDictionary::Empty` (#1780),
or `SortedSet::GetViewBetween` (#1783) — is **not** approval for any of the
above. None of them carries over.

If Phase 2 is declined, the correct outcome is not silence: `List.hpp` should
record the divergence as *permanent by decision* with a pointer to this section,
and #1791 should be closed `wontfix` with the reason attached, exactly as #1772
was.
