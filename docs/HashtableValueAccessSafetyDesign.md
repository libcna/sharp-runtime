<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# `Hashtable` value-access safety — design record

*Ticket #1797 (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, P3, size M, category
`design`, area Collections). Branch
`feature/remediation-coll-hashtable-value-access-design`, 2026-07-28. No
production source is changed by this ticket. Implementation is ticket #1796,
which stays `blocked`.*

---

## 1. Executive decision

**Ticket #1796's premise is materially incomplete, in four separate ways, and
each correction is against this record's own convenience.**

1. **There are four mutable or aliasing escape routes on `Hashtable`, not two.**
   #1796 names `operator[](const std::string&)` and `getItem()`. It does not name
   `at()`, which returns a `const std::any&` **into live map storage** — a
   `const_cast` through it is well-formed, fully defined C++ that rewrites the
   stored value with the counter unmoved (§6, defect A6), and the reference
   dangles after `Clear()` or destruction (§8, ASan). Nor does it name
   `setItem`/`Add`'s **non-`const` `void*` value parameter**, a type hole on the
   input side (§6, defect B2).
2. **The retained-alias hazard is real but is NOT the one usually assumed.**
   `std::unordered_map` is node-based, so a rehash relocates buckets and **not**
   elements: a retained alias survives 8,000 insertions unharmed, measured
   (§8.3). The hazard is `Remove`, `Clear`, copy assignment, move assignment and
   destruction — **nine AddressSanitizer `heap-use-after-free` reports across
   fourteen scenarios** (§8.2). Claiming rehash dangles would have been wrong.
3. **The most severe single defect is one #1796 does not mention at all.**
   `operator[]` on an **absent** key performs a *structural* insertion —
   `std::unordered_map::operator[]`'s own rule — without touching the counter. A
   bare read therefore changes `Count`, and an outstanding enumerator, seeing an
   unmoved counter, walks on: measured at 4,008 entries it visited **2,045
   distinct keys, reached only 6 of its 8 pre-mutation seed keys, threw nothing,
   and produced no sanitizer report at all** (§8.4). That is silent wrong data,
   not a crash.
4. **The sibling implementation of the same interface has its own, previously
   unrecorded defects.** `ListDictionaryInternal::setItem`'s *replace* branch
   returns before `++version_`, so a value replacement leaves an outstanding
   enumerator valid — while .NET's `ListDictionaryInternal` indexer setter does
   `version++` **first, unconditionally** (§9.3). It also accepts a **null key**
   on both `getItem` and `setItem` where .NET throws and where this port's
   `Hashtable` throws (§10.3). These are filed as new inactive ticket **#1798**,
   not absorbed here.

**Selected architecture: owning reads, tracked writes, and no public alias into
storage** — the same shape tickets #1793 and #1794 landed on this component's
enumerator accessors, now completed across the value-access surface.

| Member | Today | Selected |
|---|---|---|
| `getItem(const void*) const` | `void*` into live storage | **`std::any` by value** |
| `operator[](const std::string&)` | `std::any&` into live storage, inserts on read | **`ValueReference` proxy** (tracked write, owning read, no insert on read) |
| `operator[](const std::string&) const` | *does not exist* | **`std::any` by value** (new) |
| `at(const std::string&) const` | `const std::any&` into live storage, `std::out_of_range` | **`std::any` by value**, `KeyNotFoundException` |
| `setItem`/`Add` raw-key `void*` value | unchanged | **unchanged, deliberately** (§13.4) |
| `setItem(const std::string&, const std::any&)` | *does not exist* | **new typed tracked setter** (Phase 1) |

Two findings decide the shape of the proxy, and neither is a matter of taste:

- **A proxy whose read conversion returns `const std::any&` does not compile in
  this repository.** `const std::any& r = h[k];` trips GCC 14's
  `-Wdangling-reference`, which every module here builds with `-Werror` — a
  **false positive** (the reference names map storage that outlives the proxy),
  but a hard error all the same. A conversion returning `std::any` **by value**
  compiles every read spelling clean (§12.2). This is measured, not predicted.
- **The proxy must be non-copyable, and that is load-bearing.** `std::any`'s
  template converting constructor `any(T&&)` is constrained only on
  `is_copy_constructible_v<decay_t<T>>`, so with a copyable proxy
  `std::any b = h[k];` prefers **that** constructor over the proxy's own
  conversion operator: `b` silently holds a `ValueReference`, and the next
  `any_cast` throws `std::bad_any_cast` **at run time with nothing wrong at
  compile time**. This was hit during validation of this very design, not
  reasoned about in advance (§14.1).

Delivered in two phases, because only the second needs approval:

| Phase | Content | Break | Approval |
|---|---|---|---|
| **1** | Add `setItem(const std::string&, const std::any&)`; correct the header contract comments on `operator[]`, `at()` and `getItem()`. | **None** — pure addition | **Not required** |
| **2** | `getItem` → `std::any`; `operator[]` → proxy + const by-value overload; `at()` → by value + `KeyNotFoundException`; migrate `ListDictionaryInternal::getItem`. | Source-breaking **and** silently ABI-breaking | **Required (§32)** |

**Phase 1 does not close the defect and must never be recorded as remediation.**

---

## 2. Ticket handling — why #1796 was not reused

#1796's row is an **implementation** row: title *"Close the two Hashtable write
escapes that bypass the mutation counter"*, category `defect`, and its notes say
it is `blocked` because *"needs its own design ticket first (the #1795 → #1794
shape), and then, if a return type changes, its own explicit per-action user
approval"*. Recording it as a completed design ticket would log implementation
work as done when none was performed.

So #1796 **stays `blocked`**, and #1797 was opened as the next available number
and completed as the design — exactly the #1795 → #1794 precedent this
repository set one ticket earlier. #1796's acceptance criteria are rewritten
from this record (§31) and it now depends on #1797.

No new `SR-AUD-*` identifier: the audit numbering is frozen at 364 and every
defect below was found during remediation.

---

## 3. Exact current declarations

`modules/collections/include/System/Collections/Hashtable.hpp`, unchanged by this
ticket:

```cpp
// :119
[[nodiscard]] void* getItem(const void* key) const override {
    auto k = toKey(key);
    auto it = _map.find(k);
    if (it == _map.end()) return nullptr;
    return const_cast<std::any*>(&it->second);
}

// :135
void setItem(const void* key, void* value) override {
    _map[toKey(key)] = value ? *static_cast<std::any*>(value) : std::any{};
    ++version_;
}

// :220
void Add(const void* key, void* value) override { /* ... */ }

// :285
std::any& operator[](const std::string& key) { return _map[key]; }

// :291
const std::any& at(const std::string& key) const { return _map.at(key); }

// :335
std::unordered_map<std::string, std::any> _map;
System::Collections::detail::MutationCounter version_;
```

`modules/collections/include/System/Collections/IDictionary.hpp`:

```cpp
// :28
[[nodiscard]] virtual void* getItem(const void* key) const = 0;
// :37
virtual void setItem(const void* key, void* value) = 0;
// :83
virtual void Add(const void* key, void* value) = 0;
```

The header's own note at `:278` already concedes part of the gap:

> *Unlike `setItem()`/the .NET indexer setter, an insertion or assignment made
> through the reference this returns does not bump the fail-fast version counter
> … the same documented, narrow gap as `ArrayList::operator[]`.*

That note is accurate as far as it goes and **understates the defect in two
ways**: it does not say that a bare *read* of an absent key inserts, and it says
nothing about the returned reference's lifetime.

---

## 4. Complete mutable-access inventory

Every route by which a caller can obtain, or write through, something that
aliases `Hashtable`'s key or value storage. Found by reading the header in full
and confirmed by compile probes and the 629-translation-unit sweep of §11 — not
by grep alone.

| # | Route | Line | const | virtual | Escapes | Bumps? | Can dangle? | In #1796? |
|---|---|---|---|---|---|---|---|---|
| 1 | `void* getItem(const void*) const` | :119 | **const** | yes (`IDictionary`:28) | **value storage, writable, type-erased** | never | **yes** | yes |
| 2 | `std::any& operator[](const std::string&)` | :285 | non-const | no | **value storage, writable, typed** | never, **and inserts** | **yes** | yes |
| 3 | `const std::any& at(const std::string&) const` | :291 | **const** | no | **value storage, `const` alias** | never | **yes** | **no** |
| 4 | `void setItem(const void*, void*)` | :135 | non-const | yes (`IDictionary`:37) | reads *caller* storage through a non-`const` `void*` | **always** | n/a | **no** |
| 5 | `void Add(const void*, void*)` | :220 | non-const | yes (`IDictionary`:83) | same as 4 | always | n/a | **no** |
| 6 | `std::vector<std::any> getValues() const` | :302 | const | no | copies | n/a | no | — |
| 7 | `std::vector<std::string> getKeys() const` | :294 | const | no | copies | n/a | no | — |
| 8 | `CopyTo` / `copyToCore` | :96 / :329 | — | yes | copies (boxed `DictionaryEntry`) | n/a | no | — |
| 9 | `getKeysProperty()` / `getValuesProperty()` → `MemberCollection` | :163 / :178 | const | yes | read-only live view; `copyToCore` copies | n/a | no | — |
| 10 | `Enumerator::Key/Value/Entry/Current` | :411–:448 | const | yes | **owning snapshots since #1794** | n/a | no | — |
| 11 | `MemberEnumerator::getCurrentProperty()` | :519 | const | yes | forwards the owning boxes | n/a | no | — |
| 12 | `getSyncRootProperty()` (inherited) | `ICollection`:185 | const | yes | `const void*` to **`this`**, not to element storage | n/a | no | — |

**Rows 1–3 are the write/alias escapes. Rows 4–5 are input-side type holes.
Rows 6–12 are already safe**, rows 10–11 having been made so by tickets #1793
and #1794. `_map` itself is private and is reached only by the nested
`Enumerator` and `MemberCollection`.

### 4.1 Route 3 — `at()` — is the one nobody recorded

`at()` is `const`, so it *looks* like a read accessor. It returns
`const std::any&` bound to `_map.at(key)`, i.e. **the live mapped value**. The
referent is a non-`const` `std::any` inside a non-`const` `Hashtable`, so

```cpp
const_cast<std::any&>(h.at("alpha")) = std::any(1234);
```

is **not undefined behaviour**. It is well-formed, fully defined C++ that
rewrites live dictionary storage and leaves `version_` unmoved (§6, A6). It is
the *same* mechanism design #1795 found on the pre-#1794 enumerator
`getValueProperty()`, on a member nobody had looked at.

### 4.2 Routes 4–5 — the input side is type-erased too

`setItem(const void*, void*)` does `*static_cast<std::any*>(value)`. The
parameter is `void*`, so **any** pointer compiles, and a pointer to something
that is not a `std::any` is undefined behaviour with no diagnostic from any tool
(§6, B2). The parameter is additionally non-`const` although the callee only
reads it, which wrongly suggests the dictionary may write back through it.

---

## 5. Defect taxonomy

The ticket asks that the problems not collapse into *"`operator[]` is unsafe"*.
They do not. Nine classes apply, and **they apply to different routes**:

| Class | Name | Route 1 `getItem` | Route 2 `operator[]` | Route 3 `at` | Routes 4–5 `setItem`/`Add` |
|---|---|---|---|---|---|
| **A** | Mutation-counter bypass — value replacement | **yes** (§6 A4) | **yes** (§6 A1) | **yes** (§6 A6) | no — bumps correctly |
| **B** | Mutation-counter bypass — **structural** change | no | **yes** (§6 A2) | no | no |
| **C** | Retained-alias lifetime hazard | **yes** — 3 ASan | **yes** — 5 ASan | **yes** — 1 ASan | n/a |
| **D** | Const-correctness breach | **yes** — a `const` member publishes a writable pointer | n/a (non-`const`) | **yes** — `const_cast` through a `const` member's result | n/a |
| **E** | Type-erasure failure | **yes** — `void*` carries no type, size or alignment | no — typed `std::any&` | no | **yes** — input `void*` |
| **F** | Interface inconsistency | **yes** — the two `IDictionary` implementations return different things (§10.4) | **yes** — disagrees with `setItem` on versioning | **yes** | **yes** — the two implementations disagree on null keys and on replace-versioning |
| **G** | Exception inconsistency | no (`nullptr`, matching .NET) | no exception at all — it inserts | **yes** — `std::out_of_range` is invisible to `catch (System::Exception&)` | no |
| **H** | Read-vs-live-alias ambiguity | **yes** | **yes** | **yes** | n/a |
| **I** | Pointer-valued semantics | correct, but undocumented | correct, but undocumented | correct, but undocumented | correct |

Class **I** deserves its own line because it is the one thing the current code
gets *right* and nowhere says: mutating the object a stored `int*` points at is
**not** a dictionary mutation and correctly does not bump; replacing the stored
pointer **is** and currently does not bump (§6, D1/D2). The design must preserve
the first and fix the second.

---

## 6. Pre-fix reproduction — `1797_probe1_escapes`

Sixteen defects reproduced against the **committed** headers before any
production change, `build-probe/1797_probe1_escapes.log`. Compiled
`-Wall -Wextra -Wpedantic -Werror` clean; `-fno-access-control` is used **only**
so the probe can read the private counter and state what it did.

| id | Defect | Measured |
|---|---|---|
| A1 | `operator[]` value replacement | `1 → 99`, `version_` **2 → 2** |
| A2 | `operator[]` **structural insert on a bare read** | `Count 1 → 2`, `version_` **1 → 1**, `ContainsKey("ghost")` true |
| A3 | outstanding enumerator survives an `operator[]` write | walked to the end, threw nothing |
| A4 | `getItem()`'s `void*` write | `7 → 555`, `version_` **1 → 1** |
| A5 | the same write **through `IDictionary&`** | `version_` **1 → 1** |
| A6 | **`at()` + `const_cast` write** | `1 → 1234`, `version_` **1 → 1** |
| A7 | **a `const Hashtable&` rewrote every value** | two `const` members, `version_` **2 → 2** |
| A8 | in-place mutation of a non-trivial value | `"original" → "original-MUTATED"` via `any_cast<std::string&>`, counter unmoved |
| B1 | `getItem`'s `void*` admits every `static_cast` | `sizeof(std::any)==16 == sizeof(void*[2])`; a same-width wrong read is undiagnosed |
| B2 | `setItem`'s `void*` value parameter | a non-`std::any` pointer compiles with no diagnostic |
| C1 | `at()` throws `std::out_of_range` | invisible to `catch (const System::Exception&)` |
| C2 | `operator[]` inserts on a missing key | .NET's getter returns `null` and inserts nothing |
| D1 | replacing a stored **pointer** | counter unmoved |
| D4 | a **genuinely `const`** `Hashtable` object | `getItem()` still returns a writable `void*`; a write there is UB, undiagnosed |
| E2 | `operator[]` versioning | bumps on **none** of insert, replace, equal-replace |
| F1 | a live **value view** walked past an untracked write | threw nothing |

Correctly-behaving controls, recorded so the taxonomy is not overstated: `C3`
both raw-key paths reject a null key; `D2` mutating a pointee correctly does not
bump; `D3` every value path stores exactly what it is given, never flattening a
nested `std::any`; `E1` **`setItem` bumps on insert, replace *and* equal
replace**, which is exactly what .NET's `Insert` does (§9.2).

**Under UndefinedBehaviorSanitizer alone, all sixteen produce `0` runtime
errors** (`1797_probe1_ubsan.log`). Every one of them is silent.

A parity note, not a defect of this ticket: `Remove` of an absent key and
`Clear()` of an empty table both bump here (`0 → 1 → 2`), where .NET's `Remove`
never reaches `UpdateVersion()` if the key is absent and `Clear()` returns early
when `_count == 0 && _occupancy == 0`. That is a *stricter* port, it is not
caused by any route in §4, and it is out of scope.

---

## 7. Which routes a caller can reach from where

| Caller holds | Can read | Can write untracked |
|---|---|---|
| `Hashtable&` | all routes | routes 1, 2, 3 |
| `const Hashtable&` | routes 1, 3, 6–12 | **routes 1 and 3** — §6 A7 |
| `IDictionary&` | route 1 | **route 1** — §6 A5 |
| `const IDictionary&` | route 1 | **route 1** |
| `ICollection*` from `getValuesProperty()` | copies only | none |

The `const IDictionary&` row is the one to notice: a consumer that carefully
accepts the *most* restrictive reference the interface offers still gets a
writable pointer into the dictionary's storage.

---

## 8. Sanitizer evidence

One scenario per process — AddressSanitizer halts on the first report, so a
single binary running all of them would hide all but the first. Driver and logs:
`build-probe/1797_probe2_lifetime.cpp`, `1797_probe2_asan_*.log`. Values are a
`Tracked` type with an observable destructor, so a report is a real object
lifetime error and not a stale integer.

### 8.1 Result — nine of fourteen

| Scenario | Result |
|---|---|
| `operator[]` alias after **rehash** (3,900 inserts) | *(no report)* — see §8.3 |
| `operator[]` alias after **`Remove`** of its own key | **ASan heap-use-after-free** |
| `operator[]` alias after **`Clear()`** | **ASan heap-use-after-free** |
| `operator[]` alias after **copy assignment** | **ASan heap-use-after-free** |
| `operator[]` alias after **move assignment** | **ASan heap-use-after-free** |
| `operator[]` alias after **destruction** | **ASan heap-use-after-free** |
| `getItem` `void*` after **rehash** (4,000 inserts) | *(no report)* |
| `getItem` `void*` after **`Remove`** | **ASan heap-use-after-free** |
| `getItem` `void*` after **`Clear()`** | **ASan heap-use-after-free** |
| `getItem` `void*` after **destruction** | **ASan heap-use-after-free** |
| `at()` alias after **rehash** | *(no report)* |
| `at()` alias after **destruction** | **ASan heap-use-after-free** |
| enumerator after an untracked `operator[]` **insert** | *(no report)* — §8.4, silently wrong instead |
| enumerator after a **tracked** `setItem` *(control)* | *(no report)* — correctly threw `InvalidOperationException` |

**Nine reports.** A representative frame:

```
ERROR: AddressSanitizer: heap-use-after-free on address 0x506000000048
READ of size 8 at 0x506000000048 thread T0
    #0 std::any::has_value() const
    #1 std::any::reset()
    #2 std::any::operator=(std::any&&)
    #3 main .../1797_probe2_lifetime.cpp:62
0x506000000048 is located 40 bytes inside of 64-byte region
freed by thread T0 here: operator delete(void*, unsigned long)
```

**LeakSanitizer: 0 leaks in all fourteen scenarios, with detection proved
active** by a deliberate self-test reporting `317 byte(s) leaked in 2
allocation(s)`.

### 8.2 Copy and move assignment are the non-obvious two

`Hashtable` declares no copy or move operations, so the implicit ones run.
`detail::MutationCounter::operator=` correctly **advances the destination's**
counter (#1787), so an *enumerator* over the destination does fail fast. A
retained `std::any&` is **not** an enumerator: it aliases a node the assignment
destroyed, and nothing checks it. #1787's repair does not and cannot cover this
route.

### 8.3 Rehash does **not** dangle, and saying otherwise would be wrong

`std::unordered_map` is node-based: `[unord.req]` guarantees that rehashing
invalidates iterators but **not pointers or references to elements**. Measured
directly (`1797_probe3_semantics.log`): the address of a stored value is
**UNCHANGED across 8,000 insertions**. The three rehash scenarios above
correctly report nothing.

This matters for the design: the alias hazard is bounded to *erasure,
clearing, assignment and destruction*, which is narrower than "any mutation" —
and it is still nine use-after-frees.

### 8.4 The worst case is the one with no report at all

After an untracked structural insert through `operator[]`, an outstanding
enumerator's `version_` check passes, so `MoveNext()` proceeds over a rehashed
bucket array. Measured:

- the table holds **4,008** entries;
- the enumerator visited **2,045 distinct** keys, **0 duplicates**;
- it reached only **6 of its 8** pre-mutation seed keys;
- it threw **nothing**;
- **ASan, UBSan and LSan all report nothing.**

The walk is neither the pre-mutation snapshot nor the post-mutation contents. It
is memory-safe and wrong, which is the hardest failure mode to find in a game
port.

---

## 9. .NET comparison

Read from `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/`,
not from memory.

### 9.1 `Hashtable.cs:624` — the indexer

```csharp
public virtual object? this[object key]
{
    get {
        ArgumentNullException.ThrowIfNull(key);
        ...
        if (b.key == null) return null;
        if (((b.hash_coll & 0x7FFFFFFF) == hashcode) && KeyEquals(b.key, key))
            return b.val;
        ...
        return null;
    }
    set => Insert(key, value, false);
}
```

| Question | .NET's answer |
|---|---|
| Getter return | `object?` **by value** — a managed reference to the value object |
| Getter on a missing key | **`null`**. It does **not** throw and does **not** insert |
| Getter on a null key | `ArgumentNullException` |
| Getter and `_version` | never touched; the getter is pure |
| Setter | `Insert(key, value, false)` |
| Setter and `_version` | **`UpdateVersion()` on BOTH branches** — the new-key branch *and* the overwrite branch (`Hashtable.cs`, `Insert`) |
| Equal-value replacement | **still bumps**. `Insert` never compares the old value |
| Any ref-returning path | **none.** `Hashtable` has no `ref` return anywhere |

### 9.2 The version rule differs *between .NET's own dictionaries*

| Type | Overwrite an existing key | Reference |
|---|---|---|
| `Hashtable` | **bumps** | `Insert` calls `UpdateVersion()` on the overwrite branch |
| `Dictionary<K,V>` | **does not bump** | `TryInsert` returns before `_version++` on the overwrite branch |
| `ListDictionaryInternal` | **bumps** | setter does `version++` first, unconditionally |

This port already follows each type's own reference for `Generic::Dictionary`
(its `ValueProxy` bumps only on a new key, and its header says why). So the
answer for `Hashtable` is settled by .NET and needs no invention: **an
equal-value replacement through `Hashtable`'s indexer invalidates enumerators.**

### 9.3 `ListDictionaryInternal.cs` — the sibling, and what this port got wrong

```csharp
set {
    ArgumentNullException.ThrowIfNull(key);
    version++;                       // FIRST, unconditionally
    ...
    if (node != null) { node.value = value; return; }   // replace
    ...
    count++;                                            // insert
}
```

This port's `ListDictionaryInternal::setItem` (`:267`) returns from the replace
branch **before** `++version_`. Measured (`1797_probe5_interface.log`): insert
`0 → 1`, replace `1 → 1`, and an outstanding enumerator walked to the end after a
value replacement without throwing. **Previously unrecorded; filed as #1798.**

### 9.4 The three semantics that must not be conflated

| Layer | .NET | This port |
|---|---|---|
| **Replacing a dictionary value** | `ht[k] = v` rebinds the slot's reference; bumps `_version` | must bump — the defect |
| **Mutating the object a value refers to** | invisible to the dictionary; no bump; entirely legal | already correct (§6 D2) |
| **Boxed value types** | boxing copies; the box in the table is not the caller's variable | `std::any` copies on construction — same |

.NET's getter yields an **independent handle to the value object**, never a
handle to the *slot*. That single sentence is the semantic this design has to
reproduce, and it is what neither `void*`, nor `std::any&`, nor `const std::any&`
currently does.

### 9.5 .NET's own explicitly-unsafe escape hatch, for Alternative D

`System.Runtime.InteropServices.CollectionsMarshal.GetValueRefOrNullRef<TKey,TValue>`
returns `ref TValue` — but:

- it is on a **separate, explicitly named marshal class**, not on the collection;
- it applies to **`Dictionary<TKey,TValue>` only — never to `Hashtable`**;
- it carries a written caveat: *"Items should not be added or removed from the
  `Dictionary<TKey, TValue>` while the ref `TValue` is in use."*;
- and a write through it does **not** bump `_version` either.

So .NET does keep an untracked ref-returning path — behind an opt-in class, for
the generic dictionary, never for the type this ticket is about.

---

## 10. What C++ makes impossible, stated once

1. **A plain `std::any&` cannot be intercepted.** Once the caller holds it, the
   collection has no hook through which to learn that, or when, an assignment
   happened. Any design that keeps handing out `std::any&` and claims tracking is
   claiming something the language does not provide. *No fully
   source-compatible correction exists.*
2. **`const` does not mean immutable.** `const_cast` on a reference to a
   non-`const` object is defined behaviour, so `const std::any&` is a *documented
   intent*, not an enforcement (§4.1).
3. **`void*` carries no type, size or alignment.** `sizeof(std::any) == 16 ==
   sizeof(void*[2])`, so a same-width wrong `static_cast` is undiagnosed by the
   type system, by ASan and by UBSan (§6 B1).
4. **There is no GC.** .NET's `object?` handle keeps the value alive
   independently of the dictionary. In C++ only a **copy** reproduces that; a
   pointer or reference cannot.
5. **`std::any`'s converting constructor outranks a user-defined conversion
   operator** for a copyable proxy (§14.1). This is why the proxy is
   non-copyable.
6. **GCC's `-Wdangling-reference` fires on a proxy whose conversion returns a
   reference**, and this repository compiles with `-Werror` (§12.2).

---

## 11. Measured repository-wide source break

### 11.1 Call sites — the `[[deprecated]]` sweep

`build-probe/1797_sweep_callsites.py` compiled **all 629** translation units in
`build/compile_commands.json` against a shim whose value-access members carry
`[[deprecated]]`, collecting `-Wdeprecated-declarations`. `0` units failed to
compile. `MAX_JOBS = 3`.

| Member | Sites | Where |
|---|---|---|
| `at()` | **7** | `DictionaryEnumeratorKeyValueSafetyTests.cpp` ×2, `DictionaryKeyAndViewContractTests.cpp` ×1, `EnumeratorCurrentSafetyTests.cpp` ×4 |
| `getItem()` | **3** | `DictionaryKeyAndViewContractTests.cpp` :414, :499, :500 |
| `setItem()` | **2** | `DictionaryKeyAndViewContractTests.cpp` :409, :441 |
| `operator[](const std::string&)` | **0** | — |
| `getValues()` | **0** | — |

**Twelve call sites, every one of them in the test suite. No library source in
this repository calls any of them**, and `operator[]` — one of the two members
#1796 is named after — **has no caller at all**.

`test/consumer/` is not in `compile_commands.json` and was counted by hand:
**3 further sites** — `collections_dictionary_views.cpp` :98 (`setItem`) and
:100 (`getItem`), `collections_dictionary_enumerator.cpp` :131 (`at`).

### 11.2 Compile break, per candidate

`build-probe/1797_sweep_break.py` compiled all 629 units against each candidate.

| Candidate | Units broken | Distinct sites | Character of the break |
|---|---|---|---|
| `getItem` → `const std::any*` | 6 | 7 | 5 units break **only** because `ListDictionaryInternal` must be migrated too; 1 real call site |
| `getItem` → `std::any` by value | 6 | 8 | same 5, plus 2 real call sites |
| `operator[]` → proxy | **0** | **0** | nothing in this repository indexes a `Hashtable` by string |
| `operator[]` removed | **0** | **0** | same |
| `at()` → `std::any` by value | **0** | **0** | all seven sites are `any_cast<T>(h.at(k))`, which is identical for a value return |
| **the selected design, all four changes at once** | **3** | **5** | **fewer** than `getItem` alone — see below |

Two rows are counter-intuitive, and are the reason this was measured rather than
estimated:

- **`at()` → by value breaks nothing.** All seven sites are
  `std::any_cast<T>(h.at(k))`, identical for a value return.
- **The complete design breaks FEWER units than `getItem` alone.** The
  `getItem`-only candidates break 6 units because `ListDictionaryInternal` still
  declares `void* getItem(...) const override`, and `conflicting return type` then
  poisons every unit that includes it. The selected design migrates that body in
  the same change, so those five errors never occur and only the three units with
  genuine call sites remain. **The definitive Phase 2 figure is 3 of 629
  translation units and 5 sites**, not 6 and 8.

The five remaining sites, all in the test suite:

| Unit | Site |
|---|---|
| `DictionaryEnumeratorKeyValueSafetyTests.cpp` | `EXPECT_EQ(getItem(...), void* const)` |
| `DictionaryKeyAndViewContractTests.cpp` :500 | `static_cast<std::any*>(getItem(...))` |
| `DictionaryKeyAndViewContractTests.cpp` | `EXPECT_NE(getItem(...), nullptr)` |
| `ListDictionaryInternalTests.cpp` | `EXPECT_EQ(getItem(...), int* const)` |
| `ListDictionaryInternalTests.cpp` | `EXPECT_EQ(getItem(...), nullptr)` |

Four of the five are a GoogleTest comparison against a raw pointer that becomes
`std::any_cast<T>(...)` or `.has_value()`; the fifth deletes a `static_cast`.

The dominant cost of migrating `getItem` is therefore **not** call sites — it is
that `IDictionary` has a **second production implementer**, and changing a pure
virtual's return type forces both. That is one mechanical body per
implementation, not a migration burden.

### 11.3 The measurement's limits, stated

- `compile_commands.json` covers this repository only. **CNA and
  mobile-eggbert are unmeasured by instruction** and were not inspected,
  searched, configured, built or modified. The 12-site figure is *this
  repository*. It is **not** claimed to be small elsewhere.
- The sweep sees only units that already compile; a `[[deprecated]]` sweep cannot
  see a call in a `#if`-disabled branch.
- `test/consumer/` fixtures are invisible to both sweeps and were counted by
  hand.

---

## 12. Alternatives evaluated

### 12.1 The candidates

| | Alternative |
|---|---|
| **A** | Getter returns `std::any` **by value**; a setter performs replacement |
| **A′** | Getter returns **`const std::any*`** — read-only alias, no copy |
| **B** | Non-`const` `operator[]` returns a **tracked proxy** |
| **C** | **Remove** the mutable `operator[]`; `getItem`/`setItem` only |
| **D** | Keep a **named, explicitly unsafe** mutable-reference accessor |
| **E** | **Eager invalidation** — bump whenever a mutable reference is requested |
| **F** | **Edit guard** — a transactional accessor that commits on destruction |
| **G** | **Stable heap cells** with mutation hooks |
| **H** | **Document** value replacement as not invalidating enumerators |

### 12.2 The two measurements that decide it

**(i) The `-Wdangling-reference` result.** A proxy whose read conversion returns
`const std::any&` **fails to compile** the ordinary read spelling under this
repository's own warning settings. Measured across ten spellings
(`1797_probe9_proxy2.log`):

| Spelling | Current | Proxy, `const any&` conv. | **Proxy, `any` by-value conv.** |
|---|---|---|---|
| `h[k] = v` — the C# spelling | compiles | compiles | **compiles** |
| `const std::any& r = h[k]` | compiles | **`-Werror=dangling-reference`** | **compiles** |
| `std::any c = h[k]` | compiles | compiles | **compiles** |
| `auto p = h[k]` | compiles | compiles | **compiles** |
| `f(h[k])` → `const std::any&` param | compiles | compiles | **compiles** |
| `std::any& m = h[k]` | compiles | **rejected** | **rejected** |
| `&h[k]` | compiles | **rejected** | **rejected** |
| `any_cast<std::string&>(h[k])` | compiles | **rejected** | **rejected** |
| `f(h[k])` → `std::any&` param | compiles | **rejected** | **rejected** |
| `h[k].getValueProperty()` | n/a | rejected | **compiles** |

The by-value-conversion proxy rejects **exactly the four alias spellings** and
keeps **every read and the write**. That is the defect closing and nothing else
closing with it.

**(ii) The zero-call-site result.** `operator[]` has **0 call sites** in this
repository (§11.1), so the in-repository migration cost of B and of C is
identical: zero. What separates them is that **C deletes `h[k] = v` from the
API** — the exact spelling ported C# uses — and CNA's usage is unmeasured by
instruction. This is the same reasoning #1790 recorded for `list[i] = v`, and it
is the reason B wins.

### 12.3 Compatibility matrix

| | A (by value) | A′ (`const any*`) | **B (proxy)** | C (remove) | D (unsafe named) | E (eager bump) | F (edit guard) | G (cells) | H (document) |
|---|---|---|---|---|---|---|---|---|---|
| Closes A — value-replacement bypass | **yes** | **yes** | **yes** | **yes** | no | partly | **yes** | **yes** | **no** |
| Closes B — structural bypass | **yes** | **yes** | **yes** | **yes** | no | no | **yes** | no | **no** |
| Closes C — alias lifetime | **yes** | **no** | **yes** | **yes** | no | no | partly | no | **no** |
| Closes D — const breach | **yes** | **yes** | n/a | **yes** | no | no | n/a | no | no |
| Closes E — type erasure | **yes** | **yes** | n/a | partly | no | no | n/a | no | no |
| Closes G — exception inconsistency | **yes** | no | **yes** | **yes** | no | no | no | no | no |
| Keeps `h[k] = v` compiling | n/a | n/a | **yes** | **no** | yes | yes | no | yes | yes |
| .NET parity | **exact** | close | **exact** | partial | .NET does this only via `CollectionsMarshal`, and not for `Hashtable` | no | no | no | no |
| Source break (this repo) | 6 TU / 8 sites alone, **3 TU / 5 sites** in the selected combination | 6 TU / 7 sites | **0 TU** | **0 TU** | 0 | 0 | large | large | 0 |
| Virtual ABI: mangled name | **identical** | **identical** | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| Virtual ABI: vtable slot | **unchanged 0x38** | **unchanged 0x38** | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| Calling convention | **CHANGES — hidden sret** | **unchanged** | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| Object layout | **unchanged 72** | unchanged | **unchanged 72** | unchanged | unchanged | unchanged | unchanged | **changes** | unchanged |
| Allocations per read | +1 SSO string / +2 heap string / **0 for `int`** | 0 | same as A | 0 | 0 | 0 | ≥1 | ≥1/entry | 0 |
| Is it remediation? | **yes** | partial | **yes** | **yes** | **no** | **no** | yes | yes | **no** |

### 12.4 Why the selected design is A **and** B and not either alone

They apply to **different routes**. A (by value) is the answer for `getItem` and
`at()`, which are `const` read accessors with no write spelling to preserve. B
(proxy) is the answer for the non-`const` `operator[]`, which exists precisely so
`h[k] = v` compiles. Using A for `operator[]` would delete the write spelling;
using B for `getItem` would put a proxy on a virtual interface, changing the
return type to a class type anyway *and* leaving the `void*` type hole.

---

## 13. Selected architecture

**Owning reads, tracked writes, and no public alias into storage.**

1. **Every read returns an owning `std::any` by value.** It is equal by
   construction to what was stored, it survives `Remove`/`Clear`/assignment/
   destruction, and it reproduces .NET's *independent handle to the value object*
   exactly. This removes all nine ASan reports from the ordinary surface.
2. **Every write goes through a member that owns the counter.** `setItem`,
   `Add`, and the new `ValueReference::operator=` each advance `version_`; no
   other path can change a stored value.
3. **A read never inserts.** `std::unordered_map::operator[]`'s
   insert-on-read rule is removed from the public surface, matching .NET's getter
   and matching what `Generic::Dictionary::ValueProxy` already does in this
   component.
4. **A missing key reads as an empty `std::any`.** This deliberately does not
   distinguish "absent" from "present with a null value" — **because .NET does
   not either**: `ht[key]` returns `null` in both cases, which is why .NET has
   `ContainsKey`. `Contains`/`ContainsKey` remain the discriminator. `at()` is
   the throwing read.
5. **`const` means read-only.** After Phase 2 no `const` member of `Hashtable`
   returns anything a caller can write through.
6. **The raw-key `void*` *value* parameters stay** (§13.4).

### 13.4 Why `setItem`/`Add`'s `void*` value parameter is deliberately NOT changed

The obvious tidy-up is `setItem(const void*, const std::any&)`. **Measured, it
silently corrupts data** (`1797_probe10_overload.log`):

```cpp
h.Add("literal", std::any(1));
```

| Header | Result |
|---|---|
| current | key `"literal"`, `ContainsKey("literal") == true` |
| `const std::any&` raw-key overload | key **`"94525064757436"`**, `ContainsKey("literal") == false` |

Today the raw-key overload is not viable for that call (`std::any` does not
convert to `void*`), so the `std::string` overload is chosen. Once the raw-key
overload takes `const std::any&`, both are viable and the standard
`const char*` → `const void*` pointer conversion **beats** the user-defined
`const char*` → `std::string` one. The entry lands under the stringified address
of the literal, and it compiles clean under `-Wall -Wextra -Wpedantic -Werror`.

A `= delete`d `const char*` overload turns it into a hard error and was verified
to work (`use of deleted function`, with `Add(std::string("literal"), v)` still
correct). **It is still not selected**: the input-side `void*` is a lesser
defect than the escape routes this ticket is about, the guard adds two deleted
overloads to a public interface for it, and Phase 1's typed
`setItem(const std::string&, const std::any&)` gives every ordinary caller a safe
route without touching the raw-key surface at all. Recorded here with its
measurement so the implementation ticket does not "tidy" it up unaware.

---

## 14. Exact proposed public declarations

Compile-validated against the whole repository, not sketched:
`build-probe/1797_selected/` compiles standalone `-Wall -Wextra -Wpedantic
-Werror` clean and passes **21/21** contract assertions
(`1797_probe12_selected.log`).

### 14.1 `Hashtable` — the proxy

```cpp
class ValueReference {
    Hashtable* owner_;
    std::string key_;

public:
    ValueReference(Hashtable* owner, std::string key)
        : owner_(owner), key_(std::move(key)) {}

    /**
     * NON-COPYABLE ON PURPOSE, and this is load-bearing rather than stylistic.
     * std::any has a template converting constructor `any(T&&)` constrained only
     * on `is_copy_constructible_v<decay_t<T>>`. With a COPYABLE proxy,
     * `std::any b = h[k];` prefers that constructor (identity conversion of the
     * argument) over this class's own `operator std::any()`, so `b` silently holds
     * a ValueReference and the next any_cast throws std::bad_any_cast AT RUN TIME
     * with nothing wrong at compile time. Deleting the copy constructor removes
     * the proxy from that constructor's overload set. Guaranteed copy elision
     * still lets operator[] return the proxy by value.
     */
    ValueReference(const ValueReference&) = delete;

    // ---- reads: owning copies, never advance the counter, never insert ----
    operator std::any() const;
    [[nodiscard]] std::any getValueProperty() const;
    [[nodiscard]] bool hasValueProperty() const;

    // ---- tracked writes: advance the counter exactly once each ----
    ValueReference& operator=(const std::any& value);
    ValueReference& operator=(ValueReference&& other);
};
```

### 14.2 `Hashtable` — the members

```cpp
// Phase 1 -- additive, non-virtual, NO approval required.
void setItem(const std::string& key, const std::any& value);   // tracked

// Phase 2 -- the breaking half.
[[nodiscard]] std::any getItem(const void* key) const override;      // CHANGED from void*
[[nodiscard]] ValueReference operator[](const std::string& key);     // CHANGED from std::any&
[[nodiscard]] std::any operator[](const std::string& key) const;     // NEW
[[nodiscard]] std::any at(const std::string& key) const;             // CHANGED from const std::any&

// Unchanged, listed so the implementation ticket has no latitude to drift.
void setItem(const void* key, void* value) override;
void Add(const void* key, void* value) override;
void Add(const std::string& key, const std::any& value);
std::vector<std::any> getValues() const;
std::vector<std::string> getKeys() const;
```

Private helper, the single lookup site behind every by-value read:

```cpp
[[nodiscard]] std::any lookupCopy(const std::string& key) const {
    auto it = _map.find(key);
    return it == _map.end() ? std::any{} : it->second;
}
```

### 14.3 `IDictionary` — one line

```cpp
[[nodiscard]] virtual std::any getItem(const void* key) const = 0;   // CHANGED from void*
virtual void setItem(const void* key, void* value) = 0;              // unchanged
virtual void Add(const void* key, void* value) = 0;                  // unchanged
```

### 14.4 `ListDictionaryInternal` — one body, mechanical

```cpp
[[nodiscard]] std::any getItem(const void* key) const override {
    for (const auto& n : list_)
        if (n.key == key) return std::any(n.value);      // boxes the caller's void*
    return std::any{};
}
```

This preserves the existing (divergent) semantics — it still hands back the
caller's own pointer, now boxed — and deliberately does **not** fix the
implementation's other defects, which are #1798.

### 14.5 Internal representation

`Hashtable`'s members are **unchanged**: `std::unordered_map<std::string,
std::any> _map;` and `detail::MutationCounter version_;`. `ValueReference` is
40 bytes (`Hashtable*` + `std::string`), owns nothing beyond its key copy, and is
never stored by the collection.

---

## 15. Mutation and versioning rules

1. **Every write through the tracked surface advances the counter exactly
   once**: `ValueReference::operator=` (both forms), `setItem` (both overloads),
   `Add` (both overloads), `Remove`, `Clear`.
2. **An equal-value write still advances it.** `h[k] = h[k]` bumps. This matches
   .NET's `Hashtable.Insert`, which never compares the old value (§9.1), and it
   avoids requiring the payload to be equality-comparable — `std::any` is not.
   It is the **opposite** of `Generic::Dictionary`'s rule in this same component,
   and both match their own .NET reference (§9.2).
3. **Reads never advance it** and **never insert**: the conversion operator,
   `getValueProperty()`, `hasValueProperty()`, the `const` `operator[]`, `at()`,
   `getItem()`, `Contains`, `ContainsKey`, `ContainsValue`, `getKeys`,
   `getValues`, `CopyTo`, both views, and enumeration.
4. **A throwing write advances nothing.** The key is validated before the
   counter moves.
5. Copy construction still inherits the counter; copy and move assignment still
   advance the destination's (#1787). Unchanged.
6. **Mutating the object a stored pointer refers to is not a dictionary
   mutation** and correctly does not bump (§9.4). **Replacing the stored
   pointer is**, and after Phase 2 it does.

---

## 16. Ownership, lifetime and retained-alias rules

1. **No public member returns an alias into `_map`.** After Phase 2, every value
   a caller receives is an owning copy, valid independently of the table's later
   life. The nine ASan scenarios of §8.1 become **inexpressible**, not merely
   unlikely.
2. **`operator[]` yields a prvalue proxy**, valid for the full-expression that
   created it. It holds a `Hashtable*` and a key copy — **not** a pointer to an
   element — so it never dangles into freed storage; using one after the table is
   destroyed dereferences the owner pointer, which is the same borrow rule every
   enumerator and view in this port already has.
3. **The proxy is non-copyable** (§14.1), so it cannot be stored in a container,
   returned, or boxed by accident.
4. **`const std::any& r = h[k]` binds to a lifetime-extended temporary.** It is
   *safe*, and its meaning **changes silently**: it is a snapshot, not a live
   view. This is the one silent semantic change in the design and it is called
   out in §32 item 2.
5. **`const_cast<std::any&>(h.at(k))` stops compiling.** `const_cast` to an
   lvalue reference requires an lvalue operand; a by-value `at()` yields a
   prvalue. The §6 A6 abuse becomes a compile error rather than a silent no-op.
6. Enumerators and views keep #1793's and #1794's owning-snapshot contract
   unchanged. **This design touches neither.**
7. **Not closed and not claimed:** using any accessor after the *collection* is
   destroyed remains undefined, exactly as §30 risk 4 of
   `IDictionaryEnumeratorKeyValueSafetyDesign.md` says. The port-wide convention
   is that a borrower must not outlive the collection.

---

## 17. Missing-key, null-key and empty-value behaviour

| Operation | Key absent | Key present, value empty | Key null |
|---|---|---|---|
| `getItem(const void*)` | empty `std::any` | empty `std::any` | `ArgumentNullException("key")` |
| `operator[](const std::string&)` read | empty `std::any`, **no insert** | empty `std::any` | n/a — `std::string` has no null state |
| `operator[](const std::string&)` write | **inserts**, bumps | replaces, bumps | n/a |
| `operator[](const std::string&) const` | empty `std::any` | empty `std::any` | n/a |
| `at(const std::string&)` | **`KeyNotFoundException`** | empty `std::any` | n/a |
| `Contains` / `ContainsKey` | `false` | **`true`** | `ArgumentNullException` / n/a |
| `setItem` / `Add` | inserts / inserts | stores an empty `std::any` | `ArgumentNullException("key")` |

The absent/empty collapse on the getters is **.NET parity, not a defect** (§13
rule 4). `ContainsKey` is the discriminator, exactly as in .NET.

---

## 18. `std::any`, nested-any and pointer-valued semantics

1. **The box holds the stored payload, never a `std::any` wrapping a
   `std::any`.** Every path already agrees on this, measured across six accessors
   (`1797_probe3_semantics.log` §4): `at`, `operator[]`, `getItem`, the values
   view's `Current`, the enumerator's `Value`, and a `CopyTo` entry's value all
   report the same `type()`.
2. **A genuinely nested `std::any` is stored verbatim.** `std::any(std::any(5))`
   is a *copy* and stores `int`; only `std::make_any<std::any>(...)` or
   `emplace<std::any>` nests, and every value path preserves that nest without
   flattening. Measured, both spellings.
3. **A wrong `any_cast` throws `std::bad_any_cast`** instead of silently
   reinterpreting — the type tag travels with the value. This is what the
   `void*` return cannot offer.
4. **Pointer-valued elements keep the two-level semantics of §9.4**:
   `*std::any_cast<int*>(h.at(k)) = 100` mutates the pointee, is not a dictionary
   mutation, and does not bump; `h[k] = std::any(&other)` replaces the stored
   pointer, is a dictionary mutation, and does bump.
5. **A non-copyable contained value cannot be stored in a `std::any` at all**
   (`std::any` requires `is_copy_constructible`), so returning by value adds no
   new constraint on the element type — `_map`'s `mapped_type` is already
   `std::any`.

---

## 19. Exception matrix

| Operation | Condition | Exception | paramName | Message |
|---|---|---|---|---|
| `getItem`, `setItem`, `Add`, `Contains`, `Remove` (raw key) | key is null | `System::ArgumentNullException` | `"key"` | default |
| `Add` (both overloads) | key already present | `System::ArgumentException` | — | `Item has already been added. Key in dictionary: '<k>'` |
| `at` | key absent | **`System::Collections::Generic::KeyNotFoundException`** *(was `std::out_of_range`)* | — | `The given key '<k>' was not present in the dictionary.` |
| `operator[]` read/write | key absent | **none** — read yields empty, write inserts | — | — |
| `MoveNext`, `Reset` | counter differs from snapshot | `System::InvalidOperationException` | — | `Collection was modified; enumeration operation may not execute.` |
| enumerator accessors | before first `MoveNext` / after the last | `System::InvalidOperationException` | — | `Enumeration has either not started or has already finished.` |
| any accessor | wrong `any_cast` by the caller | `std::bad_any_cast` | — | std |

Ordering: the key is validated first, always, before any lookup and before the
counter moves. The only row that changes is `at()`, and it changes from a `std::`
exception a `catch (const System::Exception&)` cannot see to a `System::` one it
can.

---

## 20. Source consequences

| | Count |
|---|---|
| Translation units compiled | **629** |
| Units broken by Phase 2 | **3** |
| Distinct broken sites | **5**, all in the test suite |
| Units broken by migrating `getItem` *without* the sibling | 6 — `conflicting return type` poisons every includer |
| `test/consumer/` sites needing migration | **3** |
| Production `IDictionary` implementers to migrate | **2** |
| Library (non-test) call sites in this repository | **0** |
| `operator[]` call sites in this repository | **0** |

Migration is mechanical:

| Was | Becomes |
|---|---|
| `*static_cast<std::any*>(d.getItem(k))` | `d.getItem(k)` |
| `d.getItem(k) != nullptr` | `d.getItem(k).has_value()` **or** `d.Contains(k)` |
| `std::any& r = h[key]; r = v;` | `h[key] = v;` |
| `const std::any& r = h.at(k);` | unchanged — binds a lifetime-extended temporary |
| `auto& r = h.at(k);` | `auto r = h.at(k);` |
| `catch (const std::out_of_range&)` around `at` | `catch (const KeyNotFoundException&)` |

**A caller that only reads and never aliases needs no change at all.**

---

## 21. ABI consequences — measured, not predicted

### 21.1 Mangled names — `1797_abi_*.o`

Return type is not part of the Itanium mangling for an ordinary function, so
changing `getItem`'s return type leaves the caller symbol **byte-identical**:

```
current  : _Z11callGetItemRN6System11Collections11IDictionaryEPKv
std::any : _Z11callGetItemRN6System11Collections11IDictionaryEPKv   <- identical
const any*: _Z11callGetItemRN6System11Collections11IDictionaryEPKv   <- identical
```

### 21.2 Vtable slot — unchanged

`getItem` is called through **`*0x38(%rax)`** under the current headers and under
both candidates. No slot is added, removed or moved.

### 21.3 Calling convention — the dangerous half

```asm
; CURRENT: void* getItem(const void*) const
mov    (%rdi),%rax      ; this in %rdi
call   *0x38(%rax)      ; result in %rax
; SELECTED: std::any getItem(const void*) const
mov    %rdi,%rbx        ; %rdi is now the HIDDEN SRET POINTER
mov    (%rsi),%rax      ; this moved to %rsi
call   *0x38(%rax)
```

`std::any` is neither trivially copyable nor trivially destructible, so it is
returned through a hidden `sret` pointer and every argument shifts one register.
**Identical symbol, identical vtable slot, incompatible register assignment** —
the definition of a silent ABI break.

`const std::any*` is trivially copyable and produces **byte-identical machine
code** to today's `void*`: same symbol, same slot, `this` still in `%rdi`, result
still in `%rax`. That is Alternative A′'s single genuine advantage, and it is
recorded here rather than buried, because it is the fallback if the approval is
declined (§29).

### 21.4 Stale-object probe — reproduced end to end

A caller compiled against the **current** headers, linked against an
implementation compiled against the **selected** headers:

- **links with zero diagnostics** (`link exit=0`);
- then **segfaults** (`exit=139`);
- under UBSan, **14 diagnostics** before the crash, beginning
  `runtime error: member access within misaligned address 0x7ffc646167fc for
  type 'const struct Hashtable', which requires 8 byte alignment` — the callee is
  using the caller's *key* pointer as `this`.

Logs: `1797_stale.log`, `1797_stale_ubsan.log`. **A full consumer rebuild is
mandatory**, and it cannot be enforced by the linker.

---

## 22. Object-layout consequences

| Type | Now | After Phase 2 |
|---|---|---|
| `sizeof(Hashtable)` / `alignof` | **72** / 8 | **72** / 8 — unchanged |
| `sizeof(Hashtable::Enumerator)` | 72 | 72 — untouched |
| `sizeof(ListDictionaryInternal)` / `alignof` | **40** / 8 | **40** / 8 — unchanged |
| `is_polymorphic(Hashtable)` | true | true |
| `Hashtable::ValueReference` | *does not exist* | 40 bytes, never stored by the collection |

`Hashtable`'s members do not change, so **there is no public object-layout break
of the kind #1788/#1789/#1791 Phase 2 carry**. Measured against every candidate
shim (`1797_probe11_layout.log`). The `map + counter = 64` vs `sizeof = 72`
difference is the vptr, unchanged.

---

## 23. Allocation and performance

`-O2 -DNDEBUG`, `asm volatile` barrier, 200,000 iterations
(`1797_probe6_cost.log`, `1797_probe7_proxycost.log`).

| Operation | allocs | ns/op |
|---|---|---|
| **current** `at()` → `const any&` | 0 | 1.2 |
| **current** `operator[]` → `any&` | 0 | 10.2 |
| **current** `getItem()` → `void*` | 0 | 23.0 |
| **selected** by value, `int` payload | **0** | 5.4 |
| **selected** by value, SSO `std::string` payload | **1** | 15.7 |
| **selected** by value, 200-char `std::string` payload | **2** | 27.7 |
| candidate A′ `const any*` | 0 | 3.0 |
| **current** `operator[]` write (untracked) | 0 | 14.7 |
| **current** `setItem` write (tracked) | 0 | 40.7 |
| **selected** proxy write, SSO key | **0** | 29.3 |
| **selected** proxy write, 64-char heap key | **1** | 37.5 |
| **selected** proxy read (conversion) | 0 | 15.5 |

Stated plainly: **reads of an `int` value cost 0 allocations and 1.2 → 5.4 ns;
reads of a `std::string` value cost 0 → 1 allocation (2 for a large string) and
1.2 → 15.7 ns.** libstdc++'s `std::any` small-buffer optimisation admits only
types that fit in a `void*` and are nothrow-move-constructible, so even a short
`std::string` allocates. The proxy write costs **one allocation only when the key
itself exceeds the SSO buffer**, and is *faster* than today's `setItem` because
it skips the raw-key stringification.

This is the same cost profile #1794 accepted on the same component for the same
reason.

---

## 24. Relationship to ticket #1791, and implementation order

**#1791 is not implemented here and nothing in this design depends on it.**

| | #1796 Phase 2 (this design) | #1791 Phase 2 |
|---|---|---|
| Type | `Hashtable`, non-generic | `Generic::List<T>` |
| Proxy | `Hashtable::ValueReference`, **non-template, non-copyable, `std::string` key**, 40 bytes | `detail::ElementReference<T>`, **template, copyable, `T*` + counter pointer**, 16 bytes |
| Proxy read conversion | **`std::any` by value** (forced by `-Wdangling-reference` and by `std::any`'s converting constructor) | **`const T&`** |
| Locator | a **key** — stable across rehash, valid even if the entry is absent | a **slot pointer** — invalidated by any reallocation |
| Equal-value write | **bumps** (matches `Hashtable.Insert`) | **bumps** (matches `List.cs:161`) |
| Public object layout | **unchanged** | **changes** — `ObjectModel::Collection<T>` 32 → 40 |
| Break character | **loud** at every aliasing site | **silent** — `list[i]` keeps compiling, changes meaning |
| Virtual ABI | **silent break** on `IDictionary::getItem` | none on the indexer |
| Shared code | **none** | none |

**A shared generic proxy is explicitly rejected.** The two contracts differ in
locator (key vs pointer), in copyability (must not vs must), in read conversion
(by value vs by reference), and in element type (`std::any` vs `T`). Forcing one
template over both would mean either giving `List<T>` a by-value read — a
performance regression for every element type — or giving `Hashtable` a
reference read, which **does not compile in this repository** (§12.2). The
overlap is the phrase "returns a proxy", and that is not enough to share.

**Recommended order: #1796 before #1791.** #1796's break is loud at the call
site and its object layout is unchanged; #1791 grows a public object and
silently changes what `list[i]` means, so it deserves to land against an
otherwise-settled tree. This is the same ordering argument #1795 §34 made for
#1794.

**The migrations must not be merged.** One approval covering a public source
break, a silent ABI break, a public layout break *and* a silent semantic change
is exactly the "approval broader than its evidence" failure this sequence of
tickets has been avoiding since #1793.

---

## 25. Permanent test plan

A new suite, `modules/collections/tests/System/Collections/HashtableValueAccessSafetyTests.cpp`,
parameterised over both `IDictionary` implementations wherever the assertion is
about the interface:

1. **Versioning** — insert, replace and **equal replace** through `operator[]`,
   `setItem` (both overloads) and `Add` (both overloads) each bump exactly once;
   every read leaves the counter unmoved.
2. **Read never inserts** — `h[absent]`, `h.getItem(absent)` and the `const`
   `operator[]` leave `Count` unchanged and leave an outstanding enumerator
   valid.
3. **Fail-fast** — an outstanding enumerator and a live key/value view both
   throw `InvalidOperationException` after an indexer write.
4. **Ownership** — a value read before `Remove`, `Clear`, copy assignment, move
   assignment and table destruction still compares equal afterwards.
5. **Exceptions** — `at()` throws `KeyNotFoundException` and it is catchable as
   `System::Exception&`; the null-key rows of §19 for every raw-key entry point.
6. **Types** — the boxed payload is never a nested `std::any`; a wrong
   `any_cast` throws `std::bad_any_cast`; a deliberately nested `std::any`
   round-trips.
7. **Pointer values** — mutating a pointee does not bump; replacing the pointer
   does.
8. **Proxy** — `h[k] = v` compiles and tracks; `hasValueProperty()` distinguishes
   absent from empty; proxy-to-proxy assignment writes through and bumps.
9. **The three existing assertion sites are UPDATED, not deleted**:
   `DictionaryKeyAndViewContractTests.cpp` :414, :499, :500.

Negative (compile-rejection) fixture, one marked site per row: `std::any& m =
h[k]`, `&h[k]`, `any_cast<std::string&>(h[k])`, passing `h[k]` to a `std::any&`
parameter, `const_cast<std::any&>(h.at(k))`, and
`static_cast<std::any*>(d.getItem(k))`.

---

## 26. Sanitizer plan

Re-run §8's fourteen scenarios against the implemented headers under
**ASan + UBSan** and under **ASan + LSan** with `detect_leaks=1`, one scenario per
process, and require:

- **0** heap-use-after-free where nine were measured;
- **0** UBSan runtime errors;
- **0** leaks, with detection proved active by the deliberate-leak self-test in
  the same run — a clean LSan report means nothing without it;
- the enumeration-integrity measurement of §8.4 replaced by a
  `InvalidOperationException`.

TSan is **not** required, and the precondition must be *verified rather than
assumed*, as #1794 did: no `mutable` member exists on the proxy or on
`Hashtable`, every read accessor is `const`, and every write to `_map`/`version_`
is inside a non-`const` member.

---

## 27. Consumer-fixture plan

- **Positive**, `test/consumer/collections_hashtable_value_access.cpp`: exercises
  the whole selected surface through the public `Collections.Core` headers only,
  compiled `-Wall -Wextra -Wpedantic -Werror`, and **run**.
- **Negative**, `..._negative.cpp`: every §25 rejection row, each at a marked
  site, required to be rejected at **every** marked site.
- The three existing consumer sites (§11.1) are migrated.
- `scripts/check_selective_components.sh` must be run with a repository-local
  `TMPDIR` and **at most three jobs**, because public headers change.

---

## 28. Implementation phases

**Phase 1 — additive, no approval, no behaviour change.**
Add `setItem(const std::string&, const std::any&)`. Correct the header contract
comments: `operator[]`'s note at `:278` must say that a bare read of an absent
key **inserts** and that the returned reference dangles after
`Remove`/`Clear`/assignment/destruction; `at()` and `getItem()` gain the lifetime
and ownership statements they have never had. **Phase 1 does not close the
defect.**

**Phase 2 — the breaking half, blocked on §32.**
`IDictionary::getItem` and both implementations migrate to `std::any` by value;
`Hashtable::operator[]` becomes the proxy and gains a `const` by-value overload;
`at()` returns by value and throws `KeyNotFoundException`; the twelve call sites
and three consumer sites migrate; §25–§27 land; `README.md` gains a
breaking-change entry stating the **mandatory full consumer rebuild**.

---

## 29. Fallback if the approval is declined

**Alternative A′ — `getItem` returns `const std::any*`.** Measured, not argued:
it is **byte-identical machine code** to today (§21.3), so there is no silent ABI
break and no mandatory rebuild; it breaks 6 units / 7 sites; and it closes
classes A, B, D, E and F on `getItem`.

It leaves class **C** — the alias lifetime — **entirely open**: three of the nine
ASan reports remain reachable. **It must never be recorded as a remediation of
this ticket**, only as a partial measure. If even that is declined, Phase 1 alone
is the outcome, and the header must then say the escapes are a **permanent
accepted decision** rather than a gap awaiting repair — which is exactly the
alternative #1796's own acceptance criteria already contemplate.

---

## 30. Risks and residual limitations

| # | Risk | Severity | Position |
|---|---|---|---|
| 1 | Silent ABI break — identical mangled name, identical vtable slot, different calling convention | **High** | Measured §21.3, reproduced end to end §21.4. Only A′ avoids it. Mitigation: mandatory full rebuild, §32 item 3 |
| 2 | `const std::any& r = h[k]` silently becomes a snapshot instead of a live view | **Medium** | §16 rule 4. The only silent semantic change in the design; §32 item 2 |
| 3 | CNA and mobile-eggbert are unmeasured | **Medium** | Out of scope by instruction; not inspected, searched, built or modified. The 12-site figure is this repository only. #1773 stays blocked |
| 4 | Per-read allocation for non-`int` payloads | Medium | Measured §23: 0 for `int`, 1 for an SSO string, 2 for a large one |
| 5 | `ListDictionaryInternal`'s own defects stay open | **Medium** | §9.3, §10.3. Filed as **#1798**, not absorbed. Its `getItem` is migrated mechanically and its behaviour is otherwise untouched |
| 6 | `setItem`/`Add`'s raw-key `void*` value parameter stays | Medium | §13.4, with the data-corrupting overload measurement that is the reason |
| 7 | Accessor use after the *collection* is destroyed remains undefined | Medium | §16 rule 7. Not closed and not claimed |
| 8 | `-Wdangling-reference` is a GCC heuristic; another compiler may differ | Low | Measured on GCC 14.2.0, the repository's baseline. The by-value conversion is correct on every compiler regardless |
| 9 | `std::bad_any_cast` is a `std::` exception, not a `System::` one | Low | Consistent with the rest of this port's `std::any` surface |
| 10 | Probes depend on `-fno-access-control` | Low | Probes only, and only to read the private counter. The permanent suite asserts through the public API |

---

## 31. Rejected approaches, in one place

- **C — remove `operator[]`.** Rejected although it costs **0 call sites here**:
  it deletes `h[k] = v`, the exact spelling ported C# uses, and CNA's usage is
  unmeasured by instruction. Same reasoning #1790 applied to `list[i]`.
- **D — a named explicitly unsafe accessor.** Rejected as the *primary* answer:
  keeping any untracked mutable-reference path leaves the ticket unremediated by
  definition. .NET's own precedent (§9.5) puts such a path on a separate marshal
  class, for the *generic* dictionary, **never for `Hashtable`** — so copying it
  here would be less .NET-faithful, not more.
- **E — eager invalidation on mutable access.** Rejected: bumping when a
  reference is *requested* does not close the case where the reference is
  retained and written **after** a later enumerator is created. It converts a
  silent defect into a differently-silent defect and must not be called a
  solution.
- **F — edit guard / transactional accessor.** Rejected: it does not keep
  `h[k] = v` compiling, it needs rollback semantics `std::any` cannot express
  cheaply, nested edits are ill-defined, and it costs an allocation per edit.
- **G — stable heap cells with mutation hooks.** Rejected: an allocation per
  entry, a representation change to a public type, and there is still no hook
  through which `std::any` can report that its payload was mutated in place —
  so it does not even close A8.
- **H — document it.** Rejected on measurement: §8 shows nine use-after-frees
  that no documentation prevents. **Documentation alone is not remediation when
  aliases can dangle.**
- **A shared proxy with #1791.** Rejected on four measured incompatibilities
  (§24).
- **Migrating `setItem`/`Add`'s value parameter to `const std::any&`.** Rejected
  on the silent data corruption measured in §13.4.
- **Adding a version check to the read accessors.** Rejected: .NET's getter is
  pure and never fails fast, and doing so would convert permitted stale reads
  into exceptions.

---

## 32. Exact user approval required

Ticket #1796 Phase 2 may not begin until the user approves **all four items,
explicitly and per action**. Approvals granted for #1771, #1780, #1783, #1793 or
#1794 **do not carry over**.

1. **A public source break.** `IDictionary::getItem` and both implementations
   change return type from `void*` to `std::any`; `Hashtable::operator[]`
   changes from `std::any&` to `ValueReference`; `Hashtable::at()` changes from
   `const std::any&` to `std::any`. Measured cost in this repository: **3 of 629
   translation units, 5 call sites, 3 consumer-fixture sites, 2
   implementations**. CNA and mobile-eggbert are **unmeasured**.
2. **One silent semantic change.** `const std::any& r = h[k];` keeps compiling
   and becomes a **snapshot instead of a live view**. It is memory-safe; it is
   not source-visible. Every *other* meaning change is a compile error.
3. **A silent ABI break requiring a full consumer rebuild.** Identical mangled
   name, identical vtable slot `0x38`, `this` moving `%rdi → %rsi` behind a
   hidden `sret`. A stale caller **links with zero diagnostics and then
   segfaults** (§21.4). The linker cannot enforce the rebuild; only the release
   note can.
4. **A changed exception type.** `at()` throws
   `System::Collections::Generic::KeyNotFoundException` where it threw
   `std::out_of_range`. Code catching `std::out_of_range` around `at()` stops
   catching; code catching `System::Exception&` starts.

**Not** requested, and not to be inferred: any change to `setItem`/`Add`'s
raw-key signatures (§13.4), any change to `ListDictionaryInternal` beyond the
mechanical `getItem` migration (#1798), any change to enumerator or view
contracts (#1793/#1794 stand), and any object-layout change (there is none,
§22).

---

## 33. Validation performed under this ticket

No production or test source changed, so the repository gate is expected to be
identical to the tree this branch started from.

| Check | Result |
|---|---|
| `scripts/validate_module_boundaries.py --root .` | OK — **41 physical modules, 90 dependency edges** |
| `test/validate_module_boundaries_test.py` | **7/7** |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no consistency problems |
| `git diff --check` | clean |
| `scripts/check_doxygen_warnings.sh` | Doxygen 1.9.8, **1,940** warnings (ceiling 1,942) |
| `scripts/local_ci_check.sh build` | **13,602 tests across 37 executables**, zero warnings, zero errors |
| `SharpRuntimeTests_Collections_Core` | **2,316** |
| `scripts/check_selective_components.sh` | **not run** — no public header or component metadata changed. **Required when #1796 Phase 2 lands.** |
| `scripts/__pycache__` | absent; every Python tool run with `PYTHONDONTWRITEBYTECODE=1` |

### 33.1 Build directories and parallelism

| Directory | Use | Max jobs |
|---|---|---|
| `build/` | existing tree, reused; source of `compile_commands.json`; the gate | **3** |
| `build-probe/` | every probe, shim, sweep and ABI experiment for this ticket, prefixed `1797_` | **1 compiler process per probe**; the three Python sweeps use `MAX_JOBS = 3` |
| `build-tmp/` | repository-local `TMPDIR` | n/a |

**No compilation exceeded three jobs.** The three-job ceiling replaced the
previous four-job ceiling on 2026-07-28, during this ticket, at the user's
instruction; `scripts/local_ci_check.sh` and
`scripts/check_selective_components.sh` were corrected from their hard-coded
`--parallel 4` in the same change. The 629-unit sweeps were re-run at
`MAX_JOBS = 3` after the change.

The per-ticket build-directory habit was also ended in this ticket: `CLAUDE.md`
rule 10 is now a closed table of directory names, this ticket's probes live in
the **shared** `build-probe/` under a `1797_` file prefix rather than a
`build-probe-1797/` of their own, and nineteen stale one-shot directories (421 MB)
were deleted with the user's approval.

---

## 34. Implementation complete — ticket #1796 (2026-07-28)

*Branch `feature/remediation-coll-hashtable-value-access`. Implemented on the
user's explicit approval of all four §32 items. The design above is **not**
retro-edited: §1–§33 record what was measured before any production change, and
the corrections this section makes to it are stated as corrections.*

### 34.1 What landed

Both phases, in one change, exactly as §14 specifies. No declaration was
improvised and none was dropped.

| Member | Before | After |
|---|---|---|
| `IDictionary::getItem(const void*) const` | `void*` | **`std::any` by value** |
| `Hashtable::getItem(const void*) const` | `void*` into live storage | **`std::any` by value**, via `lookupCopy` |
| `Hashtable::operator[](const std::string&)` | `std::any&`, inserted on read | **`ValueReference`** |
| `Hashtable::operator[](const std::string&) const` | did not exist | **`std::any` by value** |
| `Hashtable::at(const std::string&) const` | `const std::any&`, `std::out_of_range` | **`std::any` by value, `KeyNotFoundException`** |
| `Hashtable::setItem(const std::string&, const std::any&)` | did not exist | **new typed tracked setter** |
| `ListDictionaryInternal::getItem(const void*) const` | `void*` | **`std::any` by value**, boxing the same caller pointer |
| `setItem`/`Add` raw-key `void*` *value* parameter | — | **unchanged** (§13.4) |

`ValueReference` is exactly §14.1's shape: non-copy-constructible (which also
implicitly deletes its move constructor and copy assignment), a by-value
`operator std::any()`, `getValueProperty()`, `hasValueProperty()`, and the two
assignment operators. It holds a `Hashtable*` and a `std::string` key — **never**
a pointer to an element — and is 40 bytes.

**One body-level strengthening beyond the sketch, no signature change.** §15
rule 4 requires that a throwing write advance nothing. `_map[key] = value;`
inserts a default-constructed node *before* the payload copy, so a throwing copy
would leave a partial mutation. Every tracked setter therefore copies first and
then move-assigns into the map (`std::any::operator=(any&&)` is `noexcept`, and
`unordered_map::operator[]`/`emplace` give the strong guarantee), so
`ValueReference::operator=`, both `setItem` overloads and both `Add` overloads
are genuinely all-or-nothing.

### 34.2 Corrections to this record's own measurements

**Two, and both are against this record's convenience.**

1. **The Phase 2 source break is 3 translation units and *seven* sites, not
   five.** §11.2's "3 TU / 5 sites" was right about the units — measured again
   after the change, exactly three failed: `DictionaryEnumeratorKeyValueSafetyTests.cpp`,
   `DictionaryKeyAndViewContractTests.cpp` and `ListDictionaryInternalTests.cpp`.
   The site count was a count of **distinct compiler diagnostics**, not of source
   lines. `ListDictionaryInternalTests.cpp` has **four** `getItem` call sites
   (`:34`, `:40`, `:48`, `:56`), and three of them (`:34`, `:48`, `:56`) compare
   against an `int*`, so GoogleTest instantiates one template for all three and
   GCC emits **one** diagnostic, at the first. The two that never got their own
   diagnostic still had to be edited. Corrected figure: **3 of 630 translation
   units, 7 source lines.**
2. **Zero `test/consumer/` fixtures needed migration, not three.** §11.1 counted
   three consumer sites by hand and §20 recorded them as "needing migration".
   All three compile and run **unchanged**: `collections_dictionary_views.cpp`
   `:98` calls the unaltered `setItem(const void*, void*)`, `:100` is
   `(void)table.getItem(nullptr)` which is valid for any return type, and
   `collections_dictionary_enumerator.cpp` `:131` is
   `std::any_cast<int>(table.at(k))`, identical for a value return. Verified by
   running all five pre-existing `Collections.Core` fixtures through
   `scripts/check_selective_components.sh` without editing one line of them.

Everything else in §1–§33 reproduced exactly. `1797_probe1_escapes` still
reported **16** defects and **0** UBSan runtime errors against the committed
headers; the fourteen lifetime scenarios still produced **9** ASan
`heap-use-after-free`; LSan still reported **0** leaks with a **317-byte /
2-allocation** self-test proving detection active; `sizeof(Hashtable)` was **72**
and `sizeof(ListDictionaryInternal)` **40**; and the enumeration-integrity walk
was still **2,045** distinct keys and **6 of 8** seeds at 4,008 entries.

### 34.3 Post-fix measurements

| Measurement | Pre-fix | Post-fix |
|---|---|---|
| ASan `heap-use-after-free` across the 14 lifetime scenarios | **9** | **0** |
| UBSan runtime errors, same scenarios | 0 | **0** |
| LSan leaks (detection proved active by a deliberate-leak self-test) | 0 / 317 B, 2 allocs | **0** / 318 B, 2 allocs |
| Enumeration integrity — 8 seeds, 4,000 missing-key reads through `operator[]` | Count **8 → 4,008**, walked **2,045** distinct, **6/8** seeds | Count **8 → 8**, walked **8** distinct, 0 duplicates, **8/8** seeds |
| Same at 4,008 pre-seeded entries, 64 missing-key reads | — | Count **4,008 → 4,008**, walked **4,008**, **8/8** seeds |
| `sizeof(Hashtable)` / `alignof` | 72 / 8 | **72 / 8** |
| `sizeof(ListDictionaryInternal)` / `alignof` | 40 / 8 | **40 / 8** |
| `Hashtable::ValueReference` | — | **40 / 8**, `Hashtable*` 8 + `std::string` 32 |
| Alias spellings rejected by the negative fixture | 0 of 11 | **11 of 11** |

The permanent suite (55 tests) and the positive consumer fixture both run clean
under **ASan + UBSan + LSan** with zero findings.

**TSan is not required, and the precondition was verified rather than assumed**
(§26): `Hashtable.hpp` declares no `mutable` member, contains no `const_cast` in
code (the remaining occurrences are doc-comment text describing the *removed*
defect), and holds no atomic, cache, static or `thread_local` state. Every read
accessor is `const` and every write to `_map`/`version_` is inside a non-`const`
member. **This ticket does not make `Hashtable` thread-safe and does not claim
to.**

### 34.4 ABI, re-measured on the real production declarations

Not a shim — `build-probe/1796_abi_caller_post.cpp` is
`build-probe/1797_abi_caller_real.cpp` with only the return type migrated.

| | Pre-fix | Post-fix |
|---|---|---|
| Caller mangled name | `_Z11callGetItemRN6System11Collections11IDictionaryEPKv` | **byte-identical** |
| Vtable slot | `*0x38(%rax)` | **unchanged** |
| `this` | `%rdi` | **`%rsi`** |
| Result | `%rax`, tail-call `jmp` | **hidden `sret` in `%rdi`**, real `call` |
| Symbols added / removed | — | **none** |
| `callSetItem` (unchanged control) | `*0x40(%rax)` | **byte-identical machine code** |

**Stale-object probe, reproduced end to end** with the *committed* old headers
extracted from git rather than a hand-written approximation: a caller TU compiled
against the old `void*` interface **links against the new implementation with
zero diagnostics (`exit=0`)** and then **segfaults (`exit=139`)**. Under UBSan it
emits **14** diagnostics first, beginning `member access within misaligned
address 0x7ffd… for type 'const struct Hashtable', which requires 8 byte
alignment` — the callee reading the caller's *key* pointer as `this`.
**A full consumer rebuild is mandatory and the linker cannot enforce it.**

### 34.5 Allocation and performance, post-fix

`-O2 -DNDEBUG`, `asm volatile` barrier, 200,000 iterations
(`build-probe/1796_cost.log`; pre-fix baseline re-measured on the same machine in
the same session, `build-probe/1796_prefix_probe6_cost.log`).

| Operation | allocs | ns/op |
|---|---|---|
| `at()` → `std::any`, `int` payload | **0** | 8.6 |
| `at()` → `std::any`, SSO `std::string` | **1** | 16.9 |
| `at()` → `std::any`, 200-char `std::string` | **2** | 24.4 |
| `at()` → `std::any`, nested `std::any` | 1 | 17.9 |
| `at()` → `std::any`, `shared_ptr` | 1 | 16.2 |
| `operator[]` read (proxy conversion), `int` | **0** | 5.3 |
| `const operator[]` read, `int` | **0** | 4.1 |
| `operator[]` **missing-key** read | **0** | 6.9 |
| `getItem(raw key)` → `std::any`, `int` | **0** | 16.7 |
| proxy construction alone, SSO key | **0** | 0.7 |
| proxy construction alone, 64-char heap key | **1** | 9.3 |
| proxy write (tracked), SSO key | **0** | 15.4 |
| proxy write (tracked), 64-char heap key | **1** | 30.3 |
| typed `setItem(string, any)` (tracked) | **0** | 14.9 |
| raw-key `setItem(const void*, void*)` (tracked) | **0** | 32.3 |

The allocation profile is exactly §23's: **0 for an `int` payload, 1 for an SSO
string, 2 for a large one**, because libstdc++'s `std::any` stores a payload
inline only if it fits in a `void*` and is nothrow-move-constructible. A proxy
allocates only when the **key** exceeds `std::string`'s SSO buffer. The tracked
proxy write is *faster* than today's raw-key `setItem` (15.4 ns vs 32.3 ns)
because it skips the raw-key stringification. **No live alias was reintroduced
for performance.**

### 34.6 Validation performed

| Check | Result |
|---|---|
| `scripts/local_ci_check.sh build` (from the clean tree) | **13,657 tests across 37 executables**, 0 warnings, 0 errors |
| `SharpRuntimeTests_Collections_Core` | **2,371** (was 2,316; +55) |
| `scripts/validate_module_boundaries.py --root .` | OK — **41 physical modules, 90 dependency edges** |
| `test/validate_module_boundaries_test.py` | **7/7** |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no consistency problems |
| `scripts/check_selective_components.sh` (full matrix) | **passed**, repository-local `TMPDIR`, ≤3 jobs |
| `scripts/check_selective_components.sh Collections.Core collections_hashtable_value_access.cpp` | fixture compiled **and run**: `OK` |
| Five pre-existing `Collections.Core` consumer fixtures | all pass **unmodified** |
| `build-probe/1796_check_negative.py` | **11/11** marked alias spellings rejected — superseded by the tracked `scripts/check_negative_consumer_fixtures.py` in ticket #1801, which reports the same 11/11 per site; do not run the old script, see `build-probe/1801_superseded_checkers.md` |
| `scripts/check_doxygen_warnings.sh` | Doxygen 1.9.8, **1,940** warnings (ceiling 1,942) — unchanged |
| `git diff --check` | clean |
| `scripts/__pycache__` | absent; every Python tool run with `PYTHONDONTWRITEBYTECODE=1` |

### 34.7 Build directories and parallelism

| Directory | Use | Max jobs |
|---|---|---|
| `build/` | **reconfigured from scratch** (`cmake --fresh`) and rebuilt with `--clean-first` for the silent ABI break; the repository gate | **3** |
| `build-probe/` | every probe, sweep, ABI and layout experiment, prefixed `1796_` | 1 compiler process per probe |
| `build-consumer/` | the negative fixture's compile log | 1 |
| `build-tmp/` | repository-local `TMPDIR` for the `mktemp`-based scripts | n/a |

**No compilation exceeded three jobs.** `CLAUDE.md`'s ceiling was lowered from
four to three on 2026-07-28 (commit `1bcb1c4d`), before this ticket began; the
three-job form was used throughout even where the instruction that authorised
this work said "four or lower".

**No new build directory was created.** `CLAUDE.md` rule 10 closes the set of
directory names, so the mandatory clean build was performed by reconfiguring
`build/` itself from scratch — `cmake --fresh` followed by
`cmake --build build --parallel 3 --clean-first` — rather than by adding a
`build-abi-1796/`. **626 translation units were compiled from scratch, 37
executables linked, and zero object files on disk predate the fresh
configuration**, so the clean-rebuild guarantee is the same one a new directory
would have given.

### 34.8 What is still open, and is not claimed closed

1. `setItem`/`Add`'s raw-key `void*` **value** parameter remains type-erased
   (§13.4). A pointer to something that is not a `std::any` still compiles and is
   still undefined behaviour with no diagnostic from any tool. The typed
   `setItem(const std::string&, const std::any&)` is the safe route; the raw-key
   overload was deliberately not "tidied up", for the measured
   `Add("literal", v)` address-key corruption.
2. Using any accessor after the **collection** is destroyed remains undefined
   (§16 rule 7). Unchanged port-wide borrow convention.
3. A `ValueReference` outliving its table dereferences a dangling owner pointer.
   That is the same borrow rule as every enumerator and view here, it is
   documented on the class, and it is **not** enforced. The proxy is normally a
   temporary within one full-expression.
4. `const std::any& r = table[key];` still compiles and is now a snapshot. It is
   memory-safe; it is the one silent meaning change, and it is documented in
   `README.md` with the instruction not to write it.
5. `ListDictionaryInternal`'s two defects (**#1798**) are untouched.
6. **A newly observed, pre-existing, unrelated finding, recorded rather than
   fixed:** `CollectionVersionAccess<Hashtable>` and
   `CollectionVersionAccess<ListDictionaryInternal>` are explicitly specialised in
   **two** translation units of the same binary with **different** bodies —
   `CollectionVersionCounterTests.cpp` (via `SR1787_SEAM_BODY`, which has
   `positionVersion`) and `DictionaryEnumeratorKeyValueSafetyTests.cpp` (via
   `SR1794_SEAM_BODY`, which does not). Differing token sequences for one
   explicit specialisation is IFNDR. It is benign in practice (both are
   header-only static functions that inline away), it predates this ticket, and
   this ticket did **not** make it worse: the new suite spells its specialisation
   token-for-token identically to `SR1794_SEAM_BODY`. Not fixed here — it is
   outside #1796's approval.

### 34.9 Relationship to #1791 and #1798, unchanged

**#1791 is not implemented and no shared List/Hashtable proxy was introduced**
(§24). The four measured incompatibilities stand. The recommended order —
**#1796 before #1791** — was followed, and #1791 remains `blocked`. **#1798
remains `blocked` and unbegun.** Ticket **#1773** remains `blocked`; **CNA and
mobile-eggbert were not inspected, searched, configured, built or modified**, so
the source-break figures in this record remain *this repository only*.

---

## 35. Separate follow-up — ticket #1802, `Remove`'s absent-key over-bump (2026-07-29)

**This section is a follow-up, not a continuation.** #1796 is `done` and is not
reopened; §1–§34 are retained unchanged. #1802
(`REMED-COLL-HASHTABLE-REMOVE-VERSION`, P3, size S, `defect`) touches one
private helper and three one-line bodies on this same class, on a surface #1796
did not examine — the *mutation counter*, not the *value accessors*. It carries
**no `SR-AUD-*` identifier**: the audit numbering is frozen at 364 and the defect
was found during remediation, by design ticket #1799's probe.

**Explicit per-action user approval was given for this behaviour change** in the
instruction that authorised the ticket, which named the effective-mutation
contract to implement. The ticket row required its own approval and states that
no earlier one carries over; this is that approval, and it covers exactly one
observable change: an outstanding enumerator that used to throw after an absent
`Remove` no longer throws.

### 35.1 The defect, reproduced before anything was edited

All three overloads were `_map.erase(key); ++version_;`, so the counter advanced
whether or not the key was present. `build-probe/1802_probe1_remove.cpp`,
compiled `-Wall -Wextra -Wpedantic -Werror` **without** `-fno-access-control`
against the committed headers, measured **24 defects over 43 checks**
(`build-probe/1802_prefix.log`). Every value is computed into a local before it
is printed, for the reason §6 of `ListDictionaryInternalSetterDesign.md` records.

| Measurement | Pre-fix | Post-fix |
|---|---|---|
| `Remove(const std::string&)` absent | **3 → 4** | 3 → 3 |
| `Remove(const char*)` absent | **3 → 4** | 3 → 3 |
| `Remove(const void*)` absent | **3 → 4** | 3 → 3 |
| the same absent key removed five times | **3 → 8** | 3 → 3 |
| a key removed twice (second call is a no-op) | **4 → 5** | 4 → 4 |
| `Remove` absent through an `IDictionary&` | **3 → 4** | 3 → 3 |
| `Remove` present, all three overloads | 3 → 4 ✔ | 3 → 4 |
| `Remove(nullptr)`, both raw-key overloads | `ArgumentNullException`, 3 → 3 ✔ | unchanged |
| `Clear()` non-empty | 3 → 4 ✔ | unchanged |
| `Clear()` already empty | 0 → 1 | **0 → 1 — deliberately unchanged (§35.4)** |
| `Count` and contents, either outcome | correct throughout ✔ | unchanged |
| `IDictionaryEnumerator` after an absent `Remove` | **throws** | live |
| key-view enumerator after an absent `Remove` | **throws** | live |
| value-view enumerator after an absent `Remove` | **throws** | live |
| through an `IDictionary&` | **throws** | live |
| full walk after an absent `Remove` | **0 entries, threw** | 3 entries, no throw |
| `Reset()` after an absent `Remove` | **throws** | live |
| `IDictionaryEnumerator` after an **effective** `Remove` | throws ✔ | throws |
| `Reset()` after an **effective** `Remove` | throws ✔ | throws |
| enumerator after a rejected null key | live ✔ | live |
| absent key in an **occupied bucket** (`c1092`, bucket 120) | **64 → 65** | 64 → 64 |
| 20,000-entry table, absent `Remove` | **20,000 → 20,001**, enumerator threw | 20,000 → 20,000, live |
| 64 missing-key reads then an absent `Remove` | count 3 → 3 ✔, version **3 → 4** | count 3 → 3, version 3 → 3 |
| keys `""` and `"0"`, absent | **+1** | +0 |
| non-trivial / `shared_ptr` values, absent | **+1**, `use_count` 2 ✔ | +0, `use_count` 2 |
| empty table, absent `Remove` | **0 → 1** | 0 → 0 |
| `ListDictionaryInternal` absent / present (post-#1798) | 3 → 3 ✔ / 3 → 4 ✔ | unchanged |
| **total** | **24 defects / 43 checks** | **0 defects / 43 checks** |

**The consequence is a false positive, and it is the exact opposite of #1798's.**
Nothing was corrupted and no memory was misused: `Count` and contents were
correct on every row. The counter simply claimed a mutation that never happened,
so a fail-fast enumerator reported `InvalidOperationException` after an operation
that changed nothing. #1798's defect was the memory-unsafe direction — a real
mutation the counter missed; this one is the merely-wrong direction.

### 35.2 .NET comparison — measured on the current source, not recalled

`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Hashtable.cs`:

- **`Remove` (`:968`–`:1005`)** validates with `ArgumentNullException.ThrowIfNull(key)`,
  computes the hash, and walks the collision chain. `UpdateVersion()` is called
  at `:999`, **inside** the branch that matched a bucket, immediately after
  `_count--` and before `_isWriterInProgress = false`. The absent case falls out
  of the `do … while` at `:1004` having touched neither `_count` nor `_version`.
  Removing an absent key is not an error and is not a mutation.
- **`UpdateVersion` (`:704`)** is a bare `_version++`; there is no second version
  channel to check.
- **`Clear` (`:422`)** **early-returns at `:426` without bumping** when
  `_count == 0 && _occupancy == 0`.
- **`ListDictionaryInternal.cs:177`–`:181`** does `version++` **first and
  unconditionally**, before it even searches, and its `Clear` (`:127`) bumps
  unconditionally too.

So .NET's own two `IDictionary` implementations genuinely disagree on the
absent-`Remove` row, exactly as `ListDictionaryInternalSetterDesign.md` §9.2
recorded. "Match .NET" is not by itself a specification; §9.3 chose the
`Hashtable` rule for the whole interface, and **this ticket is what makes the
port's `Hashtable` actually follow it.** The rule was *not* derived from
`ListDictionaryInternal`, whose bump-first shape #1799 deliberately rejected.

### 35.3 The contract, and the algorithm that implements it

**Advance the counter exactly when the dictionary's observable content changed.**

| Outcome | Count | Contents | Counter | Outstanding enumerators | Allocation |
|---|---|---|---|---|---|
| key present | −1 | that entry only | **+1** | invalidated (`MoveNext`/`Reset` throw) | none added |
| key absent | unchanged | unchanged | **+0** | stay valid | none |
| key absent, repeated | unchanged | unchanged | **+0 each time** | stay valid | none |
| null key | unchanged | unchanged | **+0** | stay valid | none |
| any pre-removal throw | unchanged | unchanged | **+0** | stay valid | n/a |

All three overloads now route through one private helper, the same
"decide once, structurally unskippable" shape `lookupCopy()` gives the reads and
`toKey()` gives the raw-key conversion:

```cpp
void removeKey(const std::string& key) {
    if (_map.erase(key) != 0) ++version_;
}

void Remove(const void* key) override { removeKey(toKey(key)); }
void Remove(const std::string& key)   { removeKey(key); }
void Remove(const char* key) {
    if (key == nullptr) throw System::ArgumentNullException("key");
    removeKey(key);
}
```

`std::unordered_map::erase(const key_type&)` returns `size_type` — the number of
elements removed. **The count that decides the question is the value the erase
call already computed and previously discarded**, so the correction adds **no
second lookup, no `Contains` pre-check, no second key conversion, no allocation
and no lock**. `toKey()` is unchanged and is still the single validating
conversion site; the null-key contract is untouched.

**Ordering and rollback.** The bump is *after* the erase, never before. A
throwing key conversion (`toKey`'s `ArgumentNullException`, or a `std::bad_alloc`
constructing the key string) happens before the map is touched, so the contents
and the counter are both unchanged — a **strong** exception guarantee, the one
.NET's bump-first `ListDictionaryInternal` shape cannot offer. `erase` itself
performs no user-defined operation that can throw here: `std::hash<std::string>`
and `std::equal_to<std::string>` are non-throwing, and `std::any`'s destructor is
`noexcept`.

### 35.4 `Clear()` — the decision, stated rather than left implicit

The ticket row required a decision on `Clear()` of an already-empty table.
**Decision: keep the unconditional bump, as a documented deliberate deviation
from .NET `Hashtable`.** Three reasons, in order of weight:

1. **The obvious "fix" would not actually match .NET.** .NET's early return is
   guarded by `_count == 0 && _occupancy == 0`, and `_occupancy` counts buckets
   whose collision bit was ever set. `std::unordered_map` exposes no analogue, so
   `if (_map.empty()) return;` would skip the bump on an emptied-but-previously-
   colliding table where .NET still bumps. It would trade one divergence for a
   different, less obvious one.
2. **It errs in the safe direction.** A spurious bump on an empty table can only
   invalidate an enumerator that had nothing to read. A *missed* bump is the
   memory-unsafe error; this is not that.
3. **It matches .NET's other `IDictionary`.** `ListDictionaryInternal.Clear`
   bumps unconditionally, and the port's own `ListDictionaryInternal` was
   deliberately left that way by #1798 (`ListDictionaryInternal.hpp:443`–`:453`).
   Both implementations therefore continue to agree with each other.

This is now an assertion rather than a comment, in both
`HashtableRemoveVersioningTests.cpp` (`ClearKeepsItsUnconditionalBumpIncluding‑
OnAnEmptyTable`, and across both implementations in
`ClearBumpsUnconditionallyOnBothImplementations`) and the consumer fixture.

### 35.5 The ten-row agreement is now complete

With #1798 and #1802 both closed, the two production `IDictionary`
implementations answer identically on every row of
`ListDictionaryInternalSetterDesign.md` §6.1:

| Operation | port `ListDictionaryInternal` | port `Hashtable` | .NET `LDI` | .NET `Hashtable` |
|---|---|---|---|---|
| `setItem` insert | +1 | +1 | +1 | +1 |
| `setItem` replace | +1 | +1 | +1 | +1 |
| `setItem` equal replace | +1 | +1 | +1 | +1 |
| `Add` duplicate (throws) | **+0** | **+0** | +1 | +0 |
| `Remove` present | +1 | +1 | +1 | +1 |
| `Remove` absent | **+0** | **+0 ← #1802** | +1 | +0 |
| rejected null key | +0 | +0 | n/a (throws first) | +0 |
| `Clear` non-empty | +1 | +1 | +1 | +1 |
| `Clear` already empty | **+1** | **+1** | +1 | **+0** |
| copy / move / self assignment | +1 on the destination | +1 | n/a | n/a |

Two rows are deliberate deviations from one of .NET's two implementations, and
both are recorded above and in `README.md`.

### 35.6 ABI, layout and calling convention — measured, not asserted

`build-probe/1802_abi_tu.cpp`, compiled twice at `-O2 -Werror`: once with
`build-probe/1802_pre/` first on the include path (the committed pre-fix
`Hashtable.hpp` and `IDictionary.hpp`, extracted with `git show HEAD:…`) and once
against the post-fix headers.

| Measurement | Pre | Post |
|---|---|---|
| `sizeof(Hashtable)` / `alignof` | 72 / 8 | **72 / 8 — unchanged** |
| `sizeof(Hashtable::ValueReference)` / `alignof` | 40 / 8 | **40 / 8 — unchanged** |
| `sizeof(IDictionary)` / `alignof` | 8 / 8 | **8 / 8 — unchanged** |
| `Hashtable` vtable (`-fdump-lang-class`) | 19 entries | **19 entries, byte-identical** |
| `Remove` vtable slot | dump offset 128 → `*0x70(%rax)` | **unchanged** |
| `Clear` / `Add` / `setItem` / `getItem` slots | `0x68` / `0x60` / `0x40` / `0x38` | **unchanged** |
| `Hashtable::Enumerator`, `MemberCollection` vtables | — | **identical** |
| defined symbols | 111 | 111 |
| undefined symbols | 37 | **37, identical list** |
| symbols removed | — | one **file-local** (`t`) libstdc++ `_M_erase…isra.0` optimiser clone |
| symbols added | — | one **weak COMDAT** `Hashtable::removeKey(std::string const&)` |
| calling convention, `Remove` | `this` in `%rdi`, `void` return, no `sret` | **unchanged** |
| `callClear` / `callAdd` / `callSetItem` machine code | — | **byte-identical** |

**No public signature, return type, parameter type, virtual slot, mangled name,
object size, alignment, member offset or calling convention changed.** The only
added symbol is the new private inline helper's own weak COMDAT definition; it
removes nothing a stale caller could need. Unlike #1794's and #1796's, this is
**not** a silent ABI break — it is a pure inline-body semantic change.

**A full consumer rebuild is nevertheless mandatory, and it is silent if
skipped**, exactly as #1798's was. Measured on the real headers
(`build-probe/1802_stale_caller.cpp` compiled pre-fix, linked with a post-fix
`main`), with **zero link diagnostics in every configuration**:

| Optimisation | Link order | stale TU | rebuilt TU |
|---|---|---|---|
| `-O0` | stale object first | **defective** | **defective — the rebuilt TU reverts** |
| `-O0` | rebuilt object first | correct | correct |
| `-O2` | stale object first | **defective** | correct |
| `-O2` | rebuilt object first | **defective** | correct |
| `-O2 -flto -Wodr` | either | **defective** | correct |

`-flto -Wodr` diagnoses nothing, because every declaration and the class layout
are identical and only an inline function *body* differs. The failure mode is
"the old false positive silently persists", never a crash. The linker cannot
enforce this rebuild; only `README.md` can.

### 35.7 Sanitizers

| Run | Result |
|---|---|
| whole `Collections.Core` suite in `build-asan/` (`-O1 -g -fsanitize=address,undefined`, `detect_leaks=1`) | **2,504/2,504**, **0** ASan, **0** UBSan, **0** LSan findings |
| `build-probe/1802_probe2_sanitizers` — removal during active enumeration (256 entries, a no-op `Remove` plus all three accessors at every step), 5,000 no-op removals on a 20,000-entry table then a full drain, non-trivial and `shared_ptr` values with snapshots retained past the table's destruction, rejected null keys | **0 findings, 0 assertion failures** |
| `test/consumer/collections_hashtable_remove.cpp` under ASan+UBSan+LSan | `OK`, 0 findings |
| `test/consumer/collections_hashtable_value_access.cpp` (#1796's, unmodified) under the same | `OK`, 0 findings |
| **LeakSanitizer proved active** | self-test leaks 350 bytes and LSan reports **383 bytes in 2 allocations**, `exit=1` |

**ThreadSanitizer was not run and is not applicable.** The change introduces no
atomic, no hidden `const` write, no cache and no new concurrency claim; the whole
of it is one `if` on a value the erase call already returned. **`Hashtable` is
not thread-safe and this ticket does not make it so** —
`getIsSynchronizedProperty()` still returns `false`.

### 35.8 Performance and allocation

`build-probe/1802_probe3_cost.cpp`, `-O2 -DNDEBUG`, built from the same two
header sets, with a counting `operator new`.

| Path | Allocations pre | Allocations post |
|---|---|---|
| absent `Remove(const std::string&)` | 0 | **0** |
| present `Remove(const std::string&)` | 0 | **0** |
| absent `Remove(const void*)` | 0 | **0** |
| absent `Remove(const char*)` | 1 (the temporary key string) | **1 — unchanged** |
| rejected null key | 4 (exception construction) | **4 — unchanged** |

Timing, three alternating runs each, ns/op:

| Path | Pre | Post |
|---|---|---|
| absent `Remove`, 8-entry table | 3.36 / 3.62 / 3.83 | 3.36 / 3.57 / 3.91 |
| absent `Remove`, 200,000-entry table | 5.66 / 5.84 / 5.82 | 5.90 / 5.60 / 5.53 |
| present `Remove`, 200,000-entry drain | 112.8 / 118.1 / 117.1 | 103.5 / 120.7 / 111.6 |
| **rejected null key** | 539 / 572 / 563 | **636 / 603 / 630** |

The two hot rows are noise-dominated and faster in two of three runs each. **The
null-key row is the one honest anomaly, and it is reported rather than
smoothed:** across **seven** alternating runs it is consistently ~13 % slower
post-fix (552–567 ns pre, 627–642 ns post). It is **not** added work, and that is
measured twice, not argued:

1. The disassembly of `Hashtable::Remove(const void*)` is **instruction-for-
   instruction identical** from the prologue through the `call` to `toKey` — the
   only path a null key ever executes, because `toKey` throws before `removeKey`
   is reachable at all. The two bodies first differ at the instruction *after*
   that call.
2. A control that times a bare `throw`/`catch` of the same exception type in a TU
   that includes the header (`build-probe/1802_probe4_throwcontrol.cpp`) shows
   **no systematic shift** (pre 854/861/868, post 883/820/852).

It is therefore a whole-binary code- and unwind-layout effect on a path that
costs ~0.6 µs because it throws. **No extra lookup, no allocation, no lock, and
no second key conversion was introduced anywhere.**

### 35.9 Interaction with ticket #1800, recorded and not fixed

The new suite adds a **fourth** `CollectionVersionAccess<Hashtable>` /
`<ListDictionaryInternal>` specialisation, spelled **token-for-token** as the
three existing `SR1794_SEAM_BODY` ones in
`DictionaryEnumeratorKeyValueSafetyTests.cpp`,
`HashtableValueAccessSafetyTests.cpp` and
`ListDictionarySetterContractTests.cpp`. Identical specialisations across
translation units are well-formed; the IFNDR is the **divergence** with
`CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY`, which adds
`positionVersion`. **No divergent body was added by this ticket** — the count of
distinct bodies is still two, and the new file joins the larger group. #1800 is
pre-existing, is not introduced, widened or fixed here, and **#1802 does not
claim to close it.** Testing was possible without worsening it, so no blocker
arose.

### 35.10 Negative-fixture status and #1801

**This ticket adds no negative fixture.** Nothing in it changes at compile time —
no signature, no return type, no overload set — so a compile-rejection fixture
would have nothing to fail on, which is exactly the reasoning
`ListDictionaryInternalSetterDesign.md` §28 applied. #1801 is untouched: the
per-site checkers for the four existing negative fixtures still live only under
the gitignored `build-probe/`, so those fixtures are still compiled by **no
tracked CI job**. The **positive** fixture added here *is* compiled `-Werror`
**and run**, by `scripts/check_selective_components.sh Collections.Core
collections_hashtable_remove.cpp`, which is not part of the script's default
matrix and must be invoked with those two arguments — stated plainly so "the
fixture is in CI" is not inferred.

> **Reconciled by ticket #1801 on 2026-07-29.** The paragraph above is preserved
> as the accurate record of what was true when #1802 closed. #1801 has since
> closed that gap for all **seven** negative fixtures — the four this section
> counted plus `collections_mutation_version_negative.cpp`,
> `collections_object_model_readonlydictionary_negative.cpp` and
> `collections_sorted_set_view_negative.cpp`, which had no per-site checker at
> all. `scripts/check_negative_consumer_fixtures.py` compiles all 37 sites
> individually and runs from `scripts/local_ci_check.sh`. This section's own
> claim — that #1802 adds no negative fixture — is unchanged and still correct.
> See `docs/NegativeConsumerFixtureValidation.md`.

### 35.11 Fresh-rebuild and validation evidence

`Hashtable` is header-only, so the whole repair lives in `inline` bodies and a
stale object would silently keep the defect. The rebuild was therefore performed
the way `CLAUDE.md` rule 10 permits — by reconfiguring the approved `build/` tree
from scratch rather than by inventing a per-ticket directory:

```
cmake --fresh -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --clean-first --parallel 3
```

| Claim | Measurement |
|---|---|
| every production object rebuilt after the fresh configuration | **632 objects**, **0** predating `build/CMakeCache.txt` (2026-07-29 08:40:04) |
| every executable relinked | **36** test executables, **0** predating the configuration |
| warnings / errors | **0 / 0** across 778 build steps |
| the full gate used those objects | `scripts/run_component_tests.sh build` run against that tree |

**One later edit, stated rather than glossed over.** After the clean rebuild, two
doc-comments were changed from `§9.3` to `section 9.3` so that `Hashtable.hpp`
and `IDictionary.hpp` stay pure ASCII, as they were before this ticket. That is a
comment-only change with no effect on any emitted code. It triggered one
incremental `cmake --build build --parallel 3`, which recompiled **11 translation
units and relinked 1 executable** — and **11 is the complete dependent set**:
exactly eleven `.d` dependency files in the whole build tree name
`System/Collections/Hashtable.hpp` or `System/Collections/IDictionary.hpp`, and
all eleven were rebuilt. **0 objects still predate the fresh configuration**, and
the final gate below ran from that tree.

| Check | Result |
|---|---|
| `scripts/run_component_tests.sh build` (freshly rebuilt tree) | **13,790 tests across 37 executables**, 0 failures, 0 crashes |
| `SharpRuntimeTests_Collections_Core` | **2,504** (was 2,437; **+67**) |
| `scripts/validate_module_boundaries.py --root .` | OK — **41 physical modules, 90 dependency edges** |
| `test/validate_module_boundaries_test.py` | **7/7** |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no consistency problems |
| `scripts/check_selective_components.sh` (full ten-component matrix) | **passed**, repository-local `TMPDIR`, ≤3 jobs |
| `… Collections.Core collections_hashtable_remove.cpp` | fixture compiled `-Werror` **and run**: `OK` |
| `… Collections.Core collections_hashtable_value_access.cpp` (#1796's, unmodified) | compiled **and run**: `OK` |
| `scripts/check_doxygen_warnings.sh` | Doxygen 1.9.8, **1,940** warnings (ceiling 1,942) — unchanged |
| `git diff --check` | clean |
| `scripts/__pycache__` | absent; every Python tool run with `PYTHONDONTWRITEBYTECODE=1` |

**Build directories and parallelism.**

| Directory | Use | Max jobs |
|---|---|---|
| `build/` | `cmake --fresh` + `--clean-first`, then one incremental build; the repository gate | **3** |
| `build-asan/` | reused, `ccache`, `-O1 -g -fsanitize=address,undefined` | **3** |
| `build-probe/` | every probe, the pre-fix header snapshot, the ABI/layout/stale-object/cost experiments, all prefixed `1802_` | 1 compiler process per probe |
| `build-consumer/` | the sanitizer builds of both `Collections.Core` fixtures, prefixed `1802_` | 1 |
| `build-tmp/` | repository-local `TMPDIR` for the `mktemp`-based scripts | n/a |

**No build-directory name outside `CLAUDE.md`'s closed set was invented, and no
compilation exceeded three jobs.** `scripts/check_selective_components.sh` needed
`TMPDIR` redirected into `build-tmp/` so its `mktemp -d` matrix root stayed out of
`/tmp`; it already caps its own builds at `--parallel 3`. Three unreferenced
2026-07-24 archives left over from an older configuration
(`libSHARP_RUNTIME.a`, `libtinyxml2.a`, `libminiz.a`, **47 MB**) had no target
directory under the current configuration and were removed; the probe binaries
and the extracted pre-fix header snapshot are retained under `build-probe/`
because they are this section's evidence.

### 35.12 What is still open, and is not claimed closed

Everything in §34.8 remains open and unchanged. Specifically, this ticket does
**not** close: the raw-key `void*` *value* parameter on `setItem`/`Add`; accessor
or enumerator use after the collection is destroyed; a `ValueReference` outliving
its table; `const std::any& r = table[key];` compiling as a snapshot; #1800's
seam divergence; #1801's untracked negative fixtures; or the deliberate
`Clear()`-on-empty deviation, which is now decided rather than closed. The real
blast radius in CNA and mobile-eggbert is **unmeasured by instruction** — both
deliberately remain on older revisions and neither was inspected, searched,
configured, built or modified.
