<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# `ListDictionaryInternal` setter, versioning and null-key semantics — design record

Design ticket **#1799** (`REMED-COLL-LISTDICT-SETITEM-DESIGN`, P3, size M,
category `design`, area Collections). Implementation ticket **#1798**
(`REMED-COLL-LISTDICTINTERNAL-PARITY`) stays **`blocked`** and depends on this
record. **No production source changed under this ticket.**

---

## 1. Executive decision

`System::Collections::ListDictionaryInternal` — the *second* production
implementer of `IDictionary` — will route **every** raw-key operation through a
single private `ValidatedKey` boundary that rejects `nullptr` with
`System::ArgumentNullException("key")`, and will advance its mutation counter on
**every effective mutation including a value replacement and an equal-value
replacement**, never on a throwing or no-op call. The one remaining
key-representation inconsistency — the key view's `CopyTo` boxing `void*` where
every other key surface boxes `const void*` — is corrected in the same change.

Selected shape, measured on a compile-validated shim
(`build-probe/1799_selected/`, 33/33 contract assertions, `1799_probe2.log`):

- **No public signature changes.** Every mangled name, every vtable slot, every
  object size is byte-identical — measured, not assumed (§21, §22).
- **Three behaviour changes**, each of which turns a currently-succeeding call
  into a throw or a currently-silent enumeration into an
  `InvalidOperationException`, and each of which therefore needs its own
  explicit user approval (§36).

This record **corrects four of ticket #1798's own premises** and adds three
defects it does not mention. In summary form, with the evidence in §6–§9:

1. **#1798 says two defects; there are six.** It names the `setItem` replace
   bypass and the accepted null key. It does not mention that the **key view's
   `CopyTo` launders away the caller's `const`** — reproduced as an
   AddressSanitizer **SEGV on a write to read-only storage** through a `void*`
   the library manufactured with `const_cast` (§8.3); that **`Add` on a
   duplicate key and `Remove` of an absent key both diverge from .NET
   `ListDictionaryInternal`'s version rule** in the *opposite* direction from
   the setter (§9.2); or that **the port's own `Hashtable` diverges from .NET
   `Hashtable` on `Remove` of an absent key**, a previously unrecorded defect on
   the *other* implementation, now inactive ticket #1802 (§9.4).
2. **"Matching .NET's unconditional `version++`" is the wrong instruction.**
   #1798's acceptance criteria say the fix is to match .NET
   `ListDictionaryInternal`, which increments **first, unconditionally, before
   it even searches**. Doing that literally would *introduce* two new defects:
   a duplicate-key `Add` that throws would still invalidate every outstanding
   enumerator, and a `Remove` of an absent key would too — and .NET's own
   `Hashtable` does neither (§9.2). The selected rule is
   **advance on effective mutation**, which matches .NET on every row where
   .NET's two implementations agree, and takes the `Hashtable` rule where they
   disagree (§9.3).
3. **The null-key rationale is not SR-AUD-363's.** On `Hashtable`, `nullptr`
   stringified to the address text `"0"` and *aliased* the ordinary string key
   `"0"`. Here keys are compared by raw address and **no valid object has the
   null address**, so a stored null key aliases nothing — measured (§6, row
   "null key ALIASES a real key"). The defect is purely that the **two
   implementations of one interface disagree**, so no polymorphic
   `IDictionary` consumer can rely on the contract. That is a weaker memory
   story and a stronger *interface* story than #1798 states.
4. **The stale-object hazard is real but is not #1794's or #1796's.** Every
   affected body is `inline` in a header and no signature changes, so a
   consumer that is not rebuilt does **not** crash: it silently keeps the
   defect. Worse, the outcome is **link-order and optimisation-level
   dependent** — at `-O0` with the stale object first on the link line, a
   *rebuilt* translation unit silently reverted to the old, defective bodies —
   and `-flto -Wodr` reports nothing (§23).

---

## 2. Ticket handling — why #1798 was not reused

#1798's row is an **implementation** row: category `defect`, title "…`setItem`
skips the version bump on replace and accepts a null key", acceptance criteria
written as the change to perform, and `blocked` because that change needs its
own approval. Recording it as a completed design would log implementation work
as done when none was performed.

This is the same handling as #1795 → #1794 and #1797 → #1796, two and one
tickets earlier respectively. #1798 stays `blocked`, now **depending on this
completed design**, with acceptance criteria and an exact three-item approval
rewritten from §36.

---

## 3. Exact current declarations

From the committed
`modules/collections/include/System/Collections/ListDictionaryInternal.hpp`
(line numbers as committed at `4931d7d3`):

```cpp
class ListDictionaryInternal : public IDictionary {           // :35
    struct Node { const void* key; void* value; };            // :36-39
    std::list<Node> list_;                                    // :40
    System::Collections::detail::MutationCounter version_;    // :41

    class NodeEnumerator : public IDictionaryEnumerator {…};  // :51-151
    class MemberCollection : public ICollection {…};          // :154-207

public:
    ListDictionaryInternal() = default;                                  // :211
    [[nodiscard]] intcs getCountProperty() const override;               // :218
    using ICollection::CopyTo;                                           // :223
    void CopyTo(std::vector<DictionaryEntry>&, intcs);                   // :240
    [[nodiscard]] std::any getItem(const void* key) const override;      // :268
    void setItem(const void* key, void* value) override;                 // :282
    [[nodiscard]] ICollection* getKeysProperty() const override;         // :296
    [[nodiscard]] ICollection* getValuesProperty() const override;       // :304
    [[nodiscard]] bool Contains(const void* key) const override;         // :312
    void Add(const void* key, void* value) override;                     // :325
    void Clear() override;                                               // :338
    void Remove(const void* key) override;                               // :346
    [[nodiscard]] IDictionaryEnumerator* GetEnumerator() override;       // :358
protected:
    void copyToCore(ObjectSpan destination, intcs index) override;       // :370
};
```

The four bodies this design changes, verbatim:

```cpp
// :282-288  — no null check; the REPLACE branch returns before ++version_
void setItem(const void* key, void* value) override {
    for (auto& n : list_) {
        if (n.key == key) { n.value = value; return; }   // <-- returns, no bump
    }
    list_.push_back({key, value});
    ++version_;
}

// :268-273  — no null check
[[nodiscard]] std::any getItem(const void* key) const override {
    for (const auto& n : list_) { if (n.key == key) return std::any(n.value); }
    return std::any{};
}

// :312-317, :325-331, :346-350  — Contains / Add / Remove, none with a null check
[[nodiscard]] bool Contains(const void* key) const override { … }
void Add(const void* key, void* value) override { … }
void Remove(const void* key) override {
    size_t before = list_.size();
    list_.remove_if([key](const Node& n){ return n.key == key; });
    if (list_.size() != before) ++version_;
}

// :202-206  — the key view manufactures a writable pointer from a const one
void copyToCore(ObjectSpan destination, intcs index) override {
    intcs i = index;
    for (const auto& n : d_->list_)
        destination[i++] = std::any(keys_ ? const_cast<void*>(n.key) : n.value);
}
```

---

## 4. Complete affected-surface inventory

Every path that takes a key, produces a key or value representation, or touches
`version_`. "Escapes" means a live alias or borrowed address leaves the class.

| # | Member | Vis. | Virtual | From | Key repr. | Value repr. | Null key today | Missing key | Insert vs replace | Version today | Exceptions | Escapes | Callers |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `getItem(const void*)` | public | yes | `IDictionary` | `const void*`, by address | returns `std::any(void*)` | **accepted**, looks up | empty `std::any` | n/a | never | none | no — box holds the caller's own pointer | 4 test TUs, 2 fixtures |
| 2 | `setItem(const void*, void*)` | public | yes | `IDictionary` | `const void*` | stores `void*` verbatim | **accepted, stored** | inserts | both | **insert only** | none | no | 4 test TUs, 2 fixtures |
| 3 | `Add(const void*, void*)` | public | yes | `IDictionary` | `const void*` | stores `void*` | **accepted, stored** | inserts | insert only | on success only | `ArgumentException` on duplicate | no | 9 test TUs, 5 fixtures |
| 4 | `Contains(const void*)` | public | yes | `IDictionary` | `const void*` | — | **accepted**, may return `true` | `false` | n/a | never | none | no | 6 test TUs, 1 fixture |
| 5 | `Remove(const void*)` | public | yes | `IDictionary` | `const void*` | — | **accepted**, may remove | no-op | n/a | on success only | none | no | 5 test TUs, 2 fixtures |
| 6 | `Clear()` | public | yes | `IDictionary` | — | — | n/a | n/a | n/a | **unconditional** | none | no | 6 test TUs, 3 fixtures |
| 7 | `getCountProperty()` | public | yes | `ICollection` | — | — | n/a | n/a | n/a | never | none | no | many |
| 8 | `getKeysProperty()` | public | yes | `IDictionary` | — | — | n/a | n/a | n/a | never | none | **owning `ICollection*`; borrows the dictionary** | 3 test TUs, 2 fixtures |
| 9 | `getValuesProperty()` | public | yes | `IDictionary` | — | — | n/a | n/a | n/a | never | none | same | 3 test TUs, 2 fixtures |
| 10 | `GetEnumerator()` | public | yes | `IDictionary` | — | — | n/a | n/a | n/a | never | none | **owning `IDictionaryEnumerator*`; borrows** | 6 test TUs, 3 fixtures |
| 11 | `CopyTo(std::vector<DictionaryEntry>&, intcs)` | public | no | own | boxes `const void*` in `Entry::Key` | boxes `void*` in `Entry::Value` | n/a | n/a | n/a | never | `ArgumentNullException`/`ArgumentOutOfRangeException`/`ArgumentException` via `requireValidCopyDestination` | no | 2 test TUs |
| 12 | `copyToCore(ObjectSpan, intcs)` (dictionary) | protected | yes | `ICollection` | boxes `std::any(DictionaryEntry)` | same | n/a | n/a | n/a | never | via caller | no | reached from `ICollection::CopyTo` |
| 13 | `MemberCollection::copyToCore` | private | yes | `ICollection` | **boxes `void*` (`const_cast`)** | boxes `void*` | n/a | n/a | n/a | never | via caller | **launders the caller's `const` away** | 1 test TU, 1 fixture |
| 14 | `MemberCollection::Enumerator::getCurrentProperty()` | private | yes | `IEnumerator` | boxes `const void*` | boxes `void*` | n/a | n/a | n/a | never | `InvalidOperationException` | no | 3 test TUs, 2 fixtures |
| 15 | `NodeEnumerator::getKeyProperty()` | private | yes | `IDictionaryEnumerator` | boxes `const void*` | — | n/a | n/a | n/a | reads snapshot | `InvalidOperationException` | no (ticket #1794) | 4 test TUs, 2 fixtures |
| 16 | `NodeEnumerator::getValueProperty()` | private | yes | `IDictionaryEnumerator` | — | boxes `void*` | n/a | n/a | n/a | reads snapshot | `InvalidOperationException` | no | 4 test TUs, 2 fixtures |
| 17 | `NodeEnumerator::getEntryProperty()` | private | yes | `IDictionaryEnumerator` | `Entry::Key` = `const void*` | `Entry::Value` = `void*` | n/a | n/a | n/a | reads snapshot | `InvalidOperationException` | no | 3 test TUs, 1 fixture |
| 18 | `NodeEnumerator::getCurrentProperty()` | private | yes | `IEnumerator` | boxes `std::any(DictionaryEntry)` | same | n/a | n/a | n/a | reads snapshot | `InvalidOperationException` | no | 3 test TUs, 1 fixture |
| 19 | `NodeEnumerator::MoveNext()` / `Reset()` | private | yes | `IEnumerator` | — | — | n/a | n/a | n/a | **compares** the snapshot | `InvalidOperationException` | no | many |
| 20 | implicit copy/move assignment | public | no | implicit | — | — | n/a | n/a | n/a | **advances the destination's own counter** (ticket #1787) | none | no | 1 test TU |

**Zero production library sources call this type.** A repository sweep of
`modules/*/src` and `modules/*/include` finds `ListDictionaryInternal` named
only in its own header and in three doc-comments (`Hashtable.hpp:203`, `:702`,
`IDictionaryEnumerator.hpp:79`, `:85`, `Comparer.hpp:24`). Every call site is a
test translation unit or a `test/consumer/` fixture.

`ListDictionaryInternal` is **not** `Hashtable` with a different container, and
the two must not be reasoned about as one type merely because both implement
`IDictionary`: `Hashtable` keys are `std::string` derived from an address,
`ListDictionaryInternal` keys are the raw address itself; `Hashtable` stores
`std::any` *values it owns*, this type stores a `void*` *it does not own*;
`Hashtable` has four mutable escape routes into live storage (design
`HashtableValueAccessSafetyDesign.md` §4), this type has **none** — every
accessor already returns the caller's own pointer, boxed.

---

## 5. Defect taxonomy

Twelve classes were considered; six apply. Each is separately reproducible and
separately fixable, and the record deliberately does not collapse them into
"`setItem` forgot `++version_`".

| Class | Applies | Evidence |
|---|---|---|
| **D1 — mutation-counter bypass on replacement** | **yes** | `setItem` replace: version `3 → 3` while the stored value changed (§6) |
| **D2 — enumerator invalidation failure** | **yes** | Four outstanding-enumerator kinds walked to the end with no throw (§6) |
| **D3 — null-key contract divergence** | **yes** | Five entry points accept `nullptr`; the sibling implementation throws on all five (§6) |
| **D4 — partial mutation on invalid input** | **no** | A throwing `Add` leaves both content and counter unchanged; measured |
| **D5 — key-representation inconsistency** | **yes** | Key view `Current` boxes `const void*`, key view `CopyTo` boxes `void*`; `any_cast<const void*>` on the `CopyTo` slot throws `std::bad_any_cast` (§6), and a write through the `CopyTo` slot **SEGVs** (§8.3) |
| **D6 — value-representation inconsistency** | **no** | All four value surfaces box `void*` and agree; corrected by ticket #1794 |
| **D7 — generic/non-generic interface inconsistency** | **yes** | The two `IDictionary` implementations disagree on every null-key row *and* on the replace-version row (§6) |
| **D8 — exception type or ordering divergence** | **partly** | Type and order are right where an exception exists; the duplicate-`Add` **message** omits the key text .NET's `Argument_AddingDuplicate__` carries (§9.5) — cosmetic, listed, not required |
| **D9 — equal-value replacement semantics** | **yes** | Equal replace: version `3 → 3`; .NET and the sibling both bump (§6) |
| **D10 — pointer-valued key/value semantics** | **no** | Mutating a pointee is invisible to the dictionary and does not bump — correct, and matches .NET (§9.6) |
| **D11 — nested `std::any` behaviour** | **no** | This type never copies a value; a `std::any`-valued object is stored as the caller's pointer and round-trips unchanged (§20) |
| **D12 — assignment/version interaction** | **no** | Copy and move assignment each advance the destination's counter and invalidate an outstanding enumerator; measured (§6). Ticket #1787's repair holds |

---

## 6. Pre-fix reproduction — `1799_probe1_defects`

`build-probe/1799_probe1_defects.cpp`, one compiler process, compiled
`-Wall -Wextra -Wpedantic -Werror` against the **committed** headers. Full
output retained at `build-probe/1799_probe1.log`.

> **A note on the probe's own first draft, because it changes what one row
> meant.** The first run read `Count` and the visited-element counter *inside
> the same `printf` as the call that changes them*. Function-argument
> evaluation order is unspecified, and GCC evaluated several of them before the
> call, so `Remove(nullptr)` appeared not to change `Count` and one enumerator
> appeared to visit zero further elements. Both were artefacts of the probe.
> Every value below is now computed into a local first. No conclusion in this
> record rests on the first draft.

### 6.1 Version counter, per operation, both implementations

| Operation | `ListDictionaryInternal` | .NET `ListDictionaryInternal` | port `Hashtable` | .NET `Hashtable` |
|---|---|---|---|---|
| `setItem` **insert** | `2 → 3` ✔ | `+1` | `2 → 3` ✔ | `+1` |
| `setItem` **replace** (new value) | **`3 → 3` ✘** | `+1` | `3 → 4` ✔ | `+1` |
| `setItem` **replace** (equal value) | **`3 → 3` ✘** | `+1` | `4 → 5` ✔ | `+1` |
| `Add` **duplicate** (throws) | `3 → 3` | **`+1`** | `5 → 5` | no bump |
| `Remove` **present** | `3 → 4` ✔ | `+1` | `5 → 6` ✔ | `+1` |
| `Remove` **absent** | `4 → 4` | **`+1`** | **`6 → 7`** | no bump |
| `Clear` non-empty | `4 → 5` ✔ | `+1` | ✔ | `+1` |
| `Clear` already empty | `5 → 6` | `+1` (unconditional) | bumps | **early-returns, no bump** |
| copy assignment | `2 → 3` ✔ | n/a (reference type) | ✔ | n/a |
| move assignment | `2 → 3` ✔ | n/a | ✔ | n/a |

Three rows diverge from .NET `ListDictionaryInternal`; one row on the *sibling*
diverges from .NET `Hashtable` (§9.4, ticket #1802). .NET's own two
implementations disagree on three of the ten rows, which is why "match .NET"
is not by itself a specification (§9.3).

### 6.2 Does an outstanding enumerator fail fast?

| Scenario | Result |
|---|---|
| `IDictionaryEnumerator`, then `setItem` replace | **NO THROW**, 2 further entries read |
| key-view `IEnumerator`, then `setItem` replace | **NO THROW**, 2 further entries read |
| value-view `IEnumerator`, then `setItem` replace | **NO THROW**, and it **observed the post-mutation value** |
| through an `IDictionary&` base reference | **NO THROW** |
| non-trivial (`std::string`) value objects | **NO THROW** |
| *contrast:* `setItem` **insert** | `InvalidOperationException` ✔ |
| *contrast:* `Hashtable` `setItem` replace | `InvalidOperationException` ✔ |
| copy assignment | `InvalidOperationException` ✔ |
| move assignment | `InvalidOperationException` ✔ |

The value view is the sharp one: it is the surface whose whole content is the
thing that was replaced, and it enumerated the new data with no diagnostic from
the type system, from ASan, or from UBSan.

### 6.3 Null keys, every path, both implementations

| Path | `ListDictionaryInternal` | `Hashtable` |
|---|---|---|
| `Add(nullptr, v)` | **no exception**; `Count` `1 → 2` | `ArgumentNullException` |
| `setItem(nullptr, v)` insert | **no exception**; version `0 → 1`, `Count` `0 → 1` | `ArgumentNullException` |
| `setItem(nullptr, v2)` replace | **no exception**; the stored value becomes `&v2` | `ArgumentNullException` |
| `getItem(nullptr)` | **no exception**; returns the stored `void*` | `ArgumentNullException` |
| `Contains(nullptr)` | **no exception**; returns `true` | `ArgumentNullException` |
| `Remove(nullptr)` | **no exception**; `Count` `2 → 1` | `ArgumentNullException` |
| via `IDictionary&` | identical — the divergence is on the interface | identical |
| enumeration after a null insert | 2 entries, **1 with a null key** | n/a |
| `CopyTo` after a null insert | 1 entry with a null key | n/a |
| **does the null key alias a real key?** | **NO** — pointer identity keeps the key spaces disjoint | it did, before #1775 (`"0"`) |
| absent vs. present-with-null-*value* | **distinguishable**: empty `std::any` vs. `std::any(void*)(nullptr)` | collapses, as .NET does |

### 6.4 Key/value representation across every surface

| Surface | Boxed type |
|---|---|
| `getItem` | `void*` |
| enumerator `getKeyProperty` | `const void*` |
| enumerator `getValueProperty` | `void*` |
| enumerator `getCurrentProperty` | `DictionaryEntry` |
| `DictionaryEntry::Key` | `const void*` |
| `DictionaryEntry::Value` | `void*` |
| key-view `Current` | `const void*` |
| **key-view `CopyTo`** | **`void*`** ← the only key surface that disagrees |
| value-view `Current` | `void*` |
| value-view `CopyTo` | `void*` |
| dictionary `copyToCore` | `DictionaryEntry` |
| typed `CopyTo(...).Key` | `const void*` |
| *contrast:* `Hashtable` key view `Current` and `CopyTo` | both `std::string` — self-consistent |

`std::any_cast<const void*>` on the key view's `CopyTo` slot throws
`std::bad_any_cast`: **one view, two incompatible element types**.

---

## 7. What C++ makes possible here that it did not for `Hashtable`

`HashtableValueAccessSafetyDesign.md` §10 records six things C++ makes
impossible for a type that owns its values. Four of them **do not bind here**,
and saying otherwise would import the wrong design:

1. **There is nothing to alias.** This dictionary stores a `void*` it does not
   own. Every accessor already hands back the caller's own pointer, boxed. No
   return-type change is needed and none is proposed.
2. **A raw `const void*` key can represent null unambiguously.** `nullptr` is
   not a valid object address, so rejecting it removes no legitimate key. This
   is *not* true of a stringified key space, which is why `Hashtable` needed
   `toKey`'s aliasing argument as well.
3. **Validation can be made structurally unskippable**, not merely
   conventional, because the key type is private to the lookup path (§14).
4. **The counter can be advanced *after* the mutation**, giving a **strong**
   exception guarantee that .NET's bump-first shape cannot offer (§16).

The one thing that does bind, identically: **`const` is not immutable.** A
`const void*` handed to a caller is a documented intent, not enforcement. What
the current key-view `CopyTo` does is worse than that — it removes the
documentation too (§8.3).

---

## 8. Sanitizer evidence

`build-probe/1799_probe3_sanitizers.cpp`, built `-O1 -g
-fsanitize=address,undefined`, one process per scenario so no abort can hide
another. `ASAN_OPTIONS=detect_leaks=1`. Logs:
`build-probe/1799_asan_<scenario>.log`, summary `1799_sanitizers.log`.

### 8.1 Leak detection is proved active

`lsan-selftest` reports **350 bytes in 2 allocations** (a deliberate 317-byte
`std::string` plus its 32-byte control object). Every "0 leaks" below is
therefore a measurement, not a tool that failed to start — the failure mode
#1767 hit under this sandbox's `ptrace` policy.

### 8.2 The version defect is entirely silent

| Scenario | ASan | UBSan | LSan | Observable |
|---|---|---|---|---|
| `enumerate-after-replace` | **0 reports** | **0 reports** | 0 leaks | "no throw; 1 further entry read" |
| `null-key-round-trip` | **0 reports** | **0 reports** | 0 leaks | null key stored, enumerated, copied out; `Count = 1` |
| `non-trivial-values` (400-byte payloads, replace then `Clear`) | 0 | 0 | **0 leaks** | owner still holds its value |
| `write-through-keyview-current` | 0 | 0 | 0 | `any_cast<void*>` **threw `std::bad_any_cast`** — the `const` is enforced on this surface |

Three of the six defect instances produce **no diagnostic from any tool**. That
is the point of the taxonomy: a sanitizer-clean run is not evidence that this
type is correct.

### 8.3 The one that is not silent — and #1798 does not mention it

`write-through-keyview-copyto`, against the committed headers:

```
key view CopyTo boxed: Pv
recovered a WRITABLE void* to a const object at 0x5650009299c0; writing 0xDEAD...
==3344088==ERROR: AddressSanitizer: SEGV on unknown address 0x5650009299c0
==3344088==The signal is caused by a WRITE memory access.
    #1 … in writeThroughKeyViewCopyTo … 1799_probe3_sanitizers.cpp:73
```

The caller declared its key `const int kReadOnlyKey = 7;`. It did **not**
`const_cast`. `MemberCollection::copyToCore` performed
`const_cast<void*>(n.key)` inside the library and boxed the result, so
`std::any_cast<void*>` — the only cast that matches the box's actual content —
yields a writable pointer to an object in `.rodata`. The write segfaults.

Reaching the *same* object through the key view's `Current` cannot do this: it
boxes `const void*`, and `any_cast<void*>` throws. **The library, not the
caller, removed the guarantee**, on exactly one of five key surfaces.

---

## 9. .NET comparison

Read from `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/`,
not from memory.

### 9.1 `ListDictionaryInternal.cs` — the indexer, `Add`, `Remove`, `Contains`, `Clear`

```csharp
public object? this[object key] {
    get {
        ArgumentNullException.ThrowIfNull(key);
        … return node.value … ;  return null;          // no version change
    }
    set {
        ArgumentNullException.ThrowIfNull(key);        // 1. validate
        version++;                                     // 2. bump, UNCONDITIONALLY, FIRST
        … if (node != null) { node.value = value; return; }   // replace: count unchanged
        … count++;                                            // insert
    }
}
public void Add(object key, object? value) {
    ArgumentNullException.ThrowIfNull(key);
    version++;                                         // bumped BEFORE the duplicate scan
    for (…) if (node.key.Equals(key)) throw new ArgumentException(SR.Argument_AddingDuplicate__…);
    … count++;
}
public void Remove(object key) {
    ArgumentNullException.ThrowIfNull(key);
    version++;                                         // bumped BEFORE the search
    … if (node == null) return;                        // absent: already bumped
    … count--;
}
public bool Contains(object key) { ArgumentNullException.ThrowIfNull(key); … }
public void Clear() { count = 0; head = null; version++; }   // unconditional
```

Answering the questions this design had to ask of the source directly:

- **Version increments before replacement**, and before the search that decides
  whether it *is* a replacement.
- **Unconditionally**, not only after a successful mutation — a throwing `Add`
  and a no-op `Remove` both leave the counter advanced.
- **Equal-value replacement bumps**: the value is never compared.
- **Null-key validation precedes the bump** on every one of the five entry
  points, so a rejected key never moves the counter.
- **Exception types**: `ArgumentNullException` (parameter `key`, from
  `ThrowIfNull`), `ArgumentException` for a duplicate `Add`,
  `InvalidOperationException` from the enumerator.
- **Messages**: `SR.Argument_AddingDuplicate__` is
  `"Item has already been added. Key in dictionary: '{0}'  Key being added: '{1}'"`;
  `SR.InvalidOperation_EnumFailedVersion` is
  `"Collection was modified; enumeration operation may not execute."`;
  `SR.InvalidOperation_EnumOpCantHappen` is
  `"Enumeration has either not started or has already finished."` The port's
  enumerator messages match both of the latter **exactly**.
- **`Entry`/`Key`/`Value`**: `Current => Entry`; `Key` returns `current.key`;
  `Value` returns `current.value`; each throws
  `InvalidOperationException` when unpositioned. Matched by the port since
  #1794.
- **Key and value collections**: `NodeKeyValueCollection.CopyTo` writes
  `isKeys ? node.key : node.value` — **the raw member, not a `DictionaryEntry`**
  — and `NodeKeyValueEnumerator.Current` returns the *same* expression. In .NET
  a view's `Current` and its `CopyTo` are the same object by construction. The
  port's key view is the only place where they are not.
- **`CopyTo`**: null array → `ArgumentNullException`; rank ≠ 1 →
  `ArgumentException`; negative index → `ArgumentOutOfRangeException`;
  `array.Length - index < Count` → `ArgumentException` with `paramName`
  `"index"`. The port routes this through `detail::requireValidCopyDestination`
  (tickets #1771/#1774) and is unaffected by this design.

### 9.2 .NET's two `IDictionary` implementations do not agree

`Hashtable.cs`: `Insert` calls `UpdateVersion()` **only inside** the successful
insert branch and the successful replace branch, and the duplicate-key `add`
path `throw`s **before** reaching it. `Remove` calls `UpdateVersion()` only
inside the found branch. `Clear` **early-returns without bumping** when
`_count == 0 && _occupancy == 0`.

So on three of ten rows .NET `ListDictionaryInternal` and .NET `Hashtable`
give different answers to the same question. "Match .NET" is therefore not a
specification for this ticket; a rule has to be chosen and stated.

### 9.3 The rule this design chooses, and why

**Advance the counter exactly when the dictionary's observable content changed;
never when the call throws or is a no-op; always on a replacement, including an
equal-value one.**

- It matches **.NET on every row where .NET's two implementations agree**.
- Where they disagree it takes the **`Hashtable`** rule, because that is the
  rule this port's other `IDictionary` implementation already follows and
  because the alternative *manufactures* invalidation out of operations that
  changed nothing.
- It matches the repository's own written contract: `MutationCounter.hpp`
  documents the counter as bumped "on each **effective** structural mutation",
  and #1796 chose exactly this rule for `Hashtable` ("insert, replace **and**
  equal replace … never compares the old value").
- **Replacement counts as effective** even though it is not *structural*: the
  enumerator's whole job is to fail fast when what it is about to read has
  changed, and the value view reads precisely the thing that was replaced.
- **Equal-value replacement bumps** because equality of a `void*` is address
  equality, not value equality, and comparing it would silently make the
  contract depend on the caller's aliasing. .NET does not compare either.
- **`Clear` keeps bumping unconditionally**, matching .NET
  `ListDictionaryInternal` and the port's existing behaviour, and is explicitly
  carved out of the "no bump for a no-op" rule rather than left ambiguous.

The **rejected** alternative — literal .NET `ListDictionaryInternal` parity,
`++version_` first and unconditionally — is rejected on the measured
consequence, not on taste: it would make a duplicate-key `Add` that throws, and
a `Remove` of an absent key, both invalidate every outstanding enumerator. That
is two new false-positive `InvalidOperationException`s introduced by a ticket
whose purpose is to remove a false negative, and it would newly *contradict*
`CollectionVersionCounterTests.cpp`'s `ListDictionaryAdapter`, which asserts
today that `Remove` of an absent key does **not** move the counter
(`kHasNoOpMutation = true`).

### 9.4 A previously unrecorded divergence on the *other* implementation

`Hashtable::Remove(const void*)` is `_map.erase(toKey(key)); ++version_;` — it
bumps **whether or not** the key was present, where .NET `Hashtable.Remove`
bumps only inside the found branch. Measured at §6.1 (`6 → 7` for an absent
key). This is not #1798, not #1796, and not this design's to fix; it is
recorded as **new inactive ticket #1802** and deliberately not begun. Closing
it would make the two implementations agree on all ten rows.

### 9.5 Cosmetic divergence, listed and not required

The port's `ListDictionaryInternal::Add` duplicate message is
`"Item has already been added."`; .NET's carries both keys, and the port's own
`Hashtable` carries one (`"… Key in dictionary: '140723365463548'"` — the
stringified address, measured). Adding the address text here would be
`std::to_string(reinterpret_cast<uintptr_t>(key))` in a message and is of
little diagnostic value for a pointer-identity key space. **Listed as a known
divergence, not required by the implementation ticket.**

### 9.6 Managed-object semantics versus `void*`, stated once

| Layer | .NET | This port |
|---|---|---|
| Replacing a dictionary value | `d[k] = v` rebinds the slot's reference; bumps | rebinds the stored `void*`; **must** bump — the defect |
| Mutating the object a value refers to | invisible to the dictionary; no bump | already correct — measured, version unchanged across `*p = 8` |
| Key identity | `key.Equals(other)` — value equality | **raw address equality** — a permanent, documented architectural limitation (header `@warning`, :26–33), unchanged by this design |
| Ownership of a value | GC keeps it alive independently | the dictionary owns nothing; the caller's lifetime governs |
| A null key | rejected by `ThrowIfNull` | must be rejected — but for interface-consistency reasons, not aliasing ones (§1.3) |

---

## 10. Key and value representation analysis

The value side is **already consistent** and this design does not touch it:
`getItem`, the enumerator's `Value`, `DictionaryEntry::Value` and both value
`CopyTo` paths all box `void*` (ticket #1794 made the value view agree, where
it previously agreed with neither).

The key side has **five** surfaces, four of which box `const void*`
(`getKeyProperty`, `DictionaryEntry::Key`, key-view `Current`, typed
`CopyTo(...).Key`) and one of which boxes `void*` (key-view `copyToCore`).

Two directions were possible:

- **Normalise to `const void*`** — delete one `const_cast`. Preserves the
  caller's declared `const` on every surface; matches four of five today;
  matches the principle #1793 and #1794 established ("the `const` the caller
  declared survives the boxing"); removes the §8.3 SEGV. Breaks three
  assertion lines (§11).
- **Normalise to `void*`** — `const_cast` in `getKeyProperty`,
  `DictionaryEntry`, and the key view's `Current` too. This is exactly the
  const-laundering ticket #1793 removed from this component and would reopen a
  closed finding. **Rejected.**

The header comment at :194–201 offers a rationale for the current `void*` — "so
that a caller of either view retrieves every slot the same way, with
`std::any_cast<void*>`". That rationale is **wrong on its own terms**: it buys
uniformity *between* the key and value views at the cost of uniformity *within*
the key view, and the key view's own enumerator has boxed `const void*` since
#1793. The comment must be corrected as part of the change, not left standing.

---

## 11. Measured source break

| Change | Compile break | Silent behaviour change | Sites |
|---|---|---|---|
| Null-key rejection | **none** | a currently-succeeding call throws | **0 existing assertions change.** The five `HashtableNullKey` tests are `Hashtable`-only, not parameterised; `test/consumer/collections_dictionary_views.cpp::rejectsNullKeys()` uses `Hashtable` only. Nothing asserts that this type accepts a null key |
| Version on replace | **none** | a currently-silent enumeration throws | **0 existing assertions change.** `CollectionVersionCounterTests.cpp`'s `ListDictionaryAdapter` mutates with `Add` and no-op-mutates with an absent `Remove`; both keep their current answers |
| Key-view `CopyTo` → `const void*` | **none** — `std::any_cast<void*>` still *compiles* | **`std::bad_any_cast` at run time** | **3 assertion lines, 2 files**: `CopyToBoundaryTests.cpp:540,541` (in `CopyToBoundaryValues.DictionaryViewsBoxKeysAndValuesIdentically`, whose name and comment also become wrong) and `test/consumer/collections_copyto.cpp:110` |

Zero production translation units are affected because zero production sources
call this type (§4).

**Limits of this measurement, stated:** it is *this repository only*. CNA and
mobile-eggbert were not inspected, searched, built or modified, by instruction;
they intentionally remain on older revisions and will be checked only when they
deliberately upgrade (ticket #1773, which stays `blocked`).

---

## 12. Alternatives evaluated

### A — canonical validated key conversion (a `toKey`-shaped static helper)

`static const void* requireKey(const void* key)` that throws on null and returns
the key; every entry point calls it. Mirrors `Hashtable::toKey` exactly.
Preserves every existing key form (raw addresses, string literals used as
addresses, pointer identity). No allocation: unlike `toKey`, which builds a
`std::string`, this returns the pointer unchanged. **Weakness:** it is a
convention. A future sixth entry point that forgets the call compiles and
silently reopens the defect.

### B — typed `std::any`/object key boundary

Replace `const void*` with `const std::any&` on `IDictionary` and both
implementations. **Rejected.** It is a public source *and* ABI break on the
interface, on a scale far beyond this ticket; it collides directly with the
measured `Add("literal", v)` hazard recorded in
`HashtableValueAccessSafetyDesign.md` §13.4 (the standard `const char*` →
`const void*` conversion beats the user-defined one, and the entry silently
lands under the stringified address of the literal, with no diagnostic under
`-Werror`); and it does not fix the versioning defect at all.

### C — retain the raw-key public API with an internal null guard

The API keeps `const void*`; the guard is internal. **This is the right
family**: null *can* be distinguished reliably from a legitimate pointer here,
because no valid object has the null address (§7.2). The residual type
ambiguity — any address of any type is an acceptable key — is a **pre-existing,
documented architectural limitation** of this non-generic type
(header `@warning`, :26–33) and is out of scope. A/C differ only in *how*
unskippable the guard is.

### D — split insert and replacement paths

Two private helpers, `insertNode` and `replaceNode`, with explicit version rules
each. **Rejected as the primary shape**: it duplicates validation and creates
two places for the version rule to drift apart — which is precisely how the
current defect arose (the replace path returns early and never reaches the
shared `++version_`).

### E — one upsert helper

`setItem` *is* the upsert: validate once, locate once, branch, bump once per
branch, bump last. **Selected**, combined with A/C.

### F — preserve null keys intentionally

Document the divergence and test it. **Rejected.** The audit expects .NET
parity; #1775 already established rejection on the sibling implementation and
`IDictionary`'s own doc-comments now describe rejection as the contract; and a
polymorphic consumer that cannot predict whether `IDictionary::Add(nullptr, v)`
throws has no usable contract at all. Documentation alone is not remediation
here.

### G — normalise the key/value view element types

**Separable but selected together.** It is not *inseparable* from the setter and
null-key work — it touches a different member (`MemberCollection::copyToCore`)
and has a different failure mode. It is selected into the same ticket because it
is three lines in the same file, it is the last member of the same
"one interface, disagreeing representations" family that #1793, #1794 and #1796
have been closing, and splitting it would mean two approvals and two consumer
rebuilds for one file. It is nevertheless listed as its **own approval item**
(§36.3) so it can be declined independently.

### Compatibility matrix

| | A (helper) | B (`std::any` key) | C (raw + guard) | D (split) | E (upsert) | F (keep null) | G (normalise view) | **Selected: C+E+G with A hardened** |
|---|---|---|---|---|---|---|---|---|
| Closes D1 (version bypass) | no | no | no | yes | **yes** | no | no | **yes** |
| Closes D2 (invalidation) | no | no | no | yes | **yes** | no | no | **yes** |
| Closes D3 (null key) | yes | yes | **yes** | no | no | **no** | no | **yes** |
| Closes D5 (key repr.) | no | no | no | no | no | no | **yes** | **yes** |
| Closes D7 (interface) | partly | partly | partly | no | partly | no | no | **yes** |
| Unskippable by a future method | **no** | yes | depends | no | no | n/a | n/a | **yes** (§14) |
| .NET parity | good | good | good | good | **see §9.3** | none | **exact** | **stated, with two documented deviations** |
| Public source compatible | yes | **no** | yes | yes | yes | yes | yes | **yes** |
| Virtual ABI compatible | yes | **no** | yes | yes | yes | yes | yes | **yes** (§21) |
| Symbol compatible | yes | **no** | yes | yes | yes | yes | yes | **yes** — 53/53 identical |
| Object layout compatible | yes | **no** | yes | yes | yes | yes | yes | **yes** — 40/40 |
| Allocations added | 0 | ≥1 per call | 0 | 0 | 0 | 0 | 0 | **0** |
| Time cost | ~0 | boxing | ~0 | ~0 | ~0.2 ns | 0 | 0 | **≤ 0.2 ns/op** (§25) |
| Migration burden | none | **large** | none | none | none | none | 3 assertion lines | **3 assertion lines** |
| Testability | good | good | good | fair | good | n/a | good | **good** |
| Interacts with #1794 | no | yes | no | no | no | no | **yes** — keeps its `const`-survives rule | **preserves #1794** |
| Interacts with #1796 | no | **yes** — reopens §13.4 | no | no | no | no | no | **no conflict, either order** |

---

## 13. Selected architecture

**Owning reads are already in place; what is added is a validated key boundary,
one upsert path, and one consistent key representation.**

1. Every raw-key operation constructs a **`ValidatedKey`** first, before any
   lookup and before the counter can move.
2. The **only** lookup is `findNode(ValidatedKey)`, and it is the only member
   that reads `list_` for a key. A future public method cannot search or insert
   without a `ValidatedKey`, and the only way to get one is to pass the null
   check.
3. `setItem` is a single upsert: locate, then replace-and-bump or
   insert-and-bump, with the bump **after** the mutation succeeds.
4. `Add` validates, rejects a duplicate **before** any mutation, then inserts
   and bumps.
5. `Remove` validates, and bumps only if it actually erased.
6. `Clear` is unchanged: unconditional bump, matching .NET.
7. `MemberCollection::copyToCore` stops manufacturing a writable key pointer.
8. **No signature, no return type, no parameter type, and no data member
   changes.**

---

## 14. Exact proposed declarations

Validated on the compile-checked shim at
`build-probe/1799_selected/System/Collections/ListDictionaryInternal.hpp`
(33/33 assertions, `1799_probe2.log`).

### 14.1 The key-validation boundary (new, private)

```cpp
/**
 * @brief A key that has passed the null check -- the only type the locator accepts.
 *
 * Constructing one is the single place a raw `const void*` key is validated,
 * and the private locator and both private mutators accept nothing else, so a
 * future public key-taking method physically cannot reach `list_` without
 * validating first. That is stronger than Hashtable's `toKey()`, which is a
 * convention every entry point must remember to follow.
 */
class ValidatedKey {
    const void* key_;
public:
    /** @throws System::ArgumentNullException if @p key is null. */
    explicit ValidatedKey(const void* key) : key_(key) {
        if (key == nullptr) throw System::ArgumentNullException("key");
    }
    /** @brief The validated, non-null key pointer. */
    [[nodiscard]] const void* getValueProperty() const noexcept { return key_; }
};

/** @brief The single lookup path; compares by address, as the class warning states. */
[[nodiscard]] std::list<Node>::const_iterator findNode(ValidatedKey key) const;
/** @brief Non-const overload of the single lookup path. */
[[nodiscard]] std::list<Node>::iterator       findNode(ValidatedKey key);
```

`sizeof(ValidatedKey) == 8`, it appears in no public signature, and it emits
**no symbol at all** at `-O2` (§21.1).

### 14.2 The five public bodies (signatures unchanged)

```cpp
[[nodiscard]] std::any getItem(const void* key) const override {
    const auto it = findNode(ValidatedKey(key));
    return it == list_.end() ? std::any{} : std::any(it->value);
}

void setItem(const void* key, void* value) override {
    const ValidatedKey k(key);              // 1. validate, before any mutation
    const auto it = findNode(k);            // 2. locate
    if (it != list_.end()) {                // 3a. REPLACE -- bumps, incl. equal value
        it->value = value;
        ++version_;
        return;
    }
    list_.push_back({k.getValueProperty(), value});  // 3b. INSERT -- may throw
    ++version_;                                      // 4. bump only after success
}

[[nodiscard]] bool Contains(const void* key) const override {
    return findNode(ValidatedKey(key)) != list_.end();
}

void Add(const void* key, void* value) override {
    const ValidatedKey k(key);
    if (findNode(k) != list_.end())
        throw System::ArgumentException("Item has already been added.");
    list_.push_back({k.getValueProperty(), value});
    ++version_;   // never reached by the throwing path: no partial mutation
}

void Remove(const void* key) override {
    const auto it = findNode(ValidatedKey(key));
    if (it == list_.end()) return;   // absent: no mutation, no bump
    list_.erase(it);
    ++version_;
}
```

`Remove` also stops using `list_.remove_if`, which scanned the whole list and
would have erased *every* match. `Add` rejects duplicates and `setItem`
replaces, so at most one can exist; `erase(it)` is the .NET shape (unlink one
node) and is O(1) after the O(n) locate. Behaviour is unchanged for every
reachable state.

### 14.3 The key view (signature unchanged)

```cpp
void copyToCore(ObjectSpan destination, intcs index) override {
    intcs i = index;
    for (const auto& n : d_->list_)
        destination[i++] = keys_ ? std::any(n.key) : std::any(n.value);
}
```

The `const_cast<void*>` is deleted. The doc-comment at :194–201 must be
rewritten: keys are **not** normalised to `void*`, and the reason is that the
caller's `const` must survive on every key surface (§10).

### 14.4 Doc-comment obligations

`getItem`'s `@note` at :262–266 currently says these divergences "are **not**
fixed here … They are ticket #1798". When #1798 lands, that note must be
replaced, not deleted — it should record what the contract now *is*.
`setItem`, `Add`, `Contains` and `Remove` each gain
`@throws System::ArgumentNullException if @p key is null.` and a statement of
the version rule.

---

## 15. Insert/replace algorithm and versioning rules

| Operation | Validate | Locate | Mutate | Bump | Enumerator effect |
|---|---|---|---|---|---|
| `setItem` insert | first | yes | `push_back` (may throw) | **after** success | invalidates |
| `setItem` replace | first | yes | `it->value = value` (`noexcept`) | after | **invalidates — the fix** |
| `setItem` equal-value replace | first | yes | same | **after — no value comparison** | **invalidates — the fix** |
| `setItem` null key | **throws here** | no | none | **no** | unaffected |
| `Add` new key | first | yes | `push_back` | after success | invalidates |
| `Add` duplicate | first | yes | **none** | **no** | **unaffected** |
| `Add` null key | throws | no | none | no | unaffected |
| `Contains` / `getItem` | first | yes | none | never | unaffected |
| `Contains` / `getItem` null key | throws | no | none | no | unaffected |
| `Remove` present | first | yes | `erase` | after | invalidates |
| `Remove` absent | first | yes | none | **no** | unaffected |
| `Remove` null key | throws | no | none | no | unaffected |
| `Clear` | n/a | n/a | `clear` | **unconditional** | invalidates |
| copy / move assignment | n/a | n/a | replaces contents | destination's own counter (#1787) | invalidates |
| mutating a value's **pointee** | n/a | n/a | not a dictionary mutation | **never** | unaffected — correct |

Two deviations from .NET `ListDictionaryInternal`, both deliberate, both
recorded here and required to be recorded in the header:

- **A throwing duplicate `Add` does not bump** (.NET does).
- **A `Remove` of an absent key does not bump** (.NET does).

Both follow §9.3's rule and both match .NET `Hashtable`.

---

## 16. Exception and rollback matrix

| Operation | Condition | Exception | `paramName` | Message | Guarantee |
|---|---|---|---|---|---|
| `getItem`/`setItem`/`Add`/`Contains`/`Remove` | key is null | `System::ArgumentNullException` | `"key"` | `Value cannot be null. (Parameter 'key')`, HResult `0x80004003` — measured, `1799_messages.log` | **strong**: nothing mutated, counter unmoved |
| `Add` | key present | `System::ArgumentException` | — | `Item has already been added.` (§9.5) | **strong** |
| `setItem`/`Add` insert | allocation fails | `std::bad_alloc` from `std::list::push_back` | — | std | **strong** — `push_back` is itself strongly exception-safe and the bump is *after* it, which .NET's bump-first shape could not offer |
| `setItem` replace | — | none possible: pointer assignment is `noexcept` | — | — | **no-throw** |
| `Remove`/`Clear`/`Contains`/`getItem` | valid key | none | — | — | **no-throw** after validation |
| `MoveNext`/`Reset` | counter ≠ snapshot | `System::InvalidOperationException` | — | `Collection was modified; enumeration operation may not execute.` — matches `SR.InvalidOperation_EnumFailedVersion` | — |
| enumerator accessors | unpositioned | `System::InvalidOperationException` | — | `Enumeration has either not started or has already finished.` — matches `SR.InvalidOperation_EnumOpCantHappen` | — |
| any accessor | wrong `any_cast` by the caller | `std::bad_any_cast` | — | std | — |

**Ordering is fixed and one-directional: validate → locate → mutate → bump.**
No operation performs a partial mutation, and no operation advances the counter
on a path that throws.

---

## 17. Null-key and missing-key matrix (after the change)

| Operation | Key null | Key absent | Key present, value null |
|---|---|---|---|
| `getItem` | `ArgumentNullException("key")` | empty `std::any` | `std::any(void*)` holding `nullptr` — **distinguishable from absent**, unlike `Hashtable`, because the box's presence is the discriminator |
| `setItem` | `ArgumentNullException("key")` | inserts, bumps | replaces, bumps |
| `Add` | `ArgumentNullException("key")` | inserts, bumps | inserts, bumps |
| `Contains` | `ArgumentNullException("key")` | `false` | `true` |
| `Remove` | `ArgumentNullException("key")` | no-op, no bump | erases, bumps |
| key view / enumerator | n/a — a null key can no longer be stored | — | — |

---

## 18. Enumerator invalidation rules

Unchanged in mechanism, corrected in coverage. `NodeEnumerator` snapshots
`detail::MutationVersion` at construction and compares it in `MoveNext()` and
`Reset()` (ticket #1787's counter, ticket #1794's `MoveNext`-time
`DictionaryEntry current_` snapshot). All four accessors read only the snapshot
and none dereferences the `std::list` iterator — **that must not regress.**

After the change, all four enumerator kinds fail fast on a value replacement:
the dictionary's `IDictionaryEnumerator`, the key view's `IEnumerator`, the
value view's `IEnumerator`, and the same reached through an `IDictionary&`.
Measured, 4/4, `1799_probe2.log`.

Not claimed closed, and unchanged by this design: `MoveNext()`/`Reset()` after
the *collection* has been destroyed remains undefined (the enumerator holds a
raw `const ListDictionaryInternal*`), exactly as `IDictionaryEnumeratorKeyValueSafetyDesign.md`
§37 records for both implementations.

---

## 19. `Entry` / `Current` / `Key` / `Value` / `CopyTo` relationship

After the change, every key surface boxes `const void*` and every value surface
boxes `void*`:

```
                          key            value
getItem                    —             void*
enumerator Key/Value    const void*      void*
enumerator Entry        const void*      void*     (DictionaryEntry)
enumerator Current      -------- DictionaryEntry --------
key view   Current      const void*        —
key view   CopyTo       const void*  <-- changed from void*
value view Current          —            void*
value view CopyTo           —            void*
dictionary copyToCore   -------- DictionaryEntry --------
typed CopyTo            const void*      void*     (DictionaryEntry)
```

One rule, stated once and testable: **a key is recovered with
`std::any_cast<const void*>`, a value with `std::any_cast<void*>`, and an entry
with `std::any_cast<DictionaryEntry>`, on every surface.**

---

## 20. `std::any`, nested-any and pointer-valued semantics

1. **The box holds a pointer, never a copy of the pointee.** This type stores
   `void*` and owns nothing, so no path can flatten, nest, or copy a value.
2. **A `std::any`-valued object round-trips unchanged.** Storing `&nested`
   where `nested` is `std::make_any<std::any>(std::any(5))` and reading it back
   yields the *same address*; nesting is entirely the caller's business.
   Measured (§6, section 5).
3. **A wrong `any_cast` throws `std::bad_any_cast`** rather than silently
   reinterpreting — which is exactly how the key-view inconsistency is
   detectable at all, and why normalising it is testable.
4. **Pointer-valued elements keep the two-level semantics of §9.6**: writing
   through a value pointer mutates the pointee and does **not** bump; passing a
   different pointer to `setItem` is a dictionary mutation and **does** bump.
5. `std::any` requires `is_copy_constructible`; `void*` and `const void*` both
   satisfy it trivially. No new constraint on any element type.

---

## 21. ABI consequences — measured, not predicted

`build-probe/1799_abi_tu.cpp`, compiled `-O2` twice (committed header, proposed
header) and compared.

### 21.1 Mangled names — identical

53 `ListDictionaryInternal` symbols in each object file; `diff` of the sorted
mangled-name lists is **empty**. The only symbols the proposed object gains at
all are `System::ArgumentNullException`'s typeinfo and destructors, pulled in
because the header now throws it — 7 symbols, none of them
`ListDictionaryInternal`'s. `ValidatedKey` and `findNode` emit **no symbols**:
both are fully inlined.

### 21.2 Vtable — identical

`-fdump-lang-class`, `Vtable for System::Collections::ListDictionaryInternal`:
**19 entries in both**, `diff` empty. `getItem` stays at offset **72**,
`setItem` at **80**, `Contains` at **104**, `Add` at **112**, `Clear` at
**120**, `Remove` at **128**.

### 21.3 Calling convention — unchanged

No parameter or return type changes on any member, so no `sret` appears and no
argument register moves. This is the material difference from #1794 and #1796,
whose mangled names were byte-identical while `this` moved `%rdi → %rsi` behind
a hidden `sret`.

### 21.4 Object layout — unchanged

| Type | Committed | Proposed |
|---|---|---|
| `ListDictionaryInternal` | `size=40 align=8` | `size=40 align=8` |
| `NodeEnumerator` | 72 | 72 |
| `MemberCollection` | 24 | 24 |
| `MemberCollection::Enumerator` | 24 | 24 |
| `ValidatedKey` | — | 8 (private, never in a signature) |

`CollectionVersionCounterTests.cpp:1143`'s `EXPECT_EQ(sizeof(NG::ListDictionaryInternal), 40u)`
continues to hold unmodified. **This is not a layout break in #1788/#1789/#1791's
sense.**

---

## 22. Stale-object analysis — the hazard is silent, and link-order dependent

`build-probe/1799_stale_caller.cpp` (compiled against the **committed** header)
linked with `1799_stale_main.cpp` (compiled against the **proposed** one), in
one binary. Log: `build-probe/1799_stale.log`.

| Build | Link order | Stale TU: replace invalidates / rejects null | Rebuilt TU: same | Link diagnostics |
|---|---|---|---|---|
| `-O0` | rebuilt object first | 1 / 1 — **silently gets the fix** | 1 / 1 | **none** |
| `-O0` | **stale object first** | 0 / 0 | **0 / 0 — the rebuilt TU silently LOSES the fix** | **none** |
| `-O2` | either | **0 / 0 — silently keeps the defect** | 1 / 1 | **none** |
| `-O2 -flto -Wodr` | either | 1 / 1 | 1 / 1 | **none — `-Wodr` says nothing** |

Three conclusions, all of which belong in the implementation ticket:

1. Because every affected body is `inline` in a header and no signature
   changes, a partially rebuilt program **does not crash**. It silently
   under-enforces — which is a *better* failure mode than #1794's and #1796's
   segfault, and a *worse* one to detect.
2. At `-O0` the outcome depends on **link order**, and the bad order makes a
   correctly rebuilt translation unit revert to the defective bodies. That is
   the case to warn about.
3. `-flto -Wodr` does **not** diagnose it, because the class layout and every
   declaration are identical; only inline function *bodies* differ, which
   `-Wodr` does not compare.

**A full consumer rebuild is mandatory** and must be stated in `README.md`'s
breaking-changes section, exactly as #1794 and #1796 stated theirs — with the
difference that the symptom here is a silently unfixed consumer, not a crash.

---

## 23. Copy, move and assignment behaviour

Unchanged, and preserved by this design. `ListDictionaryInternal` has implicit
copy/move construction and assignment. `detail::MutationCounter`'s assignment
operator **advances the destination's own counter** instead of transplanting
the source's (ticket #1787), so an enumerator outstanding over the destination
is invalidated by an assignment that destroyed everything it could refer to.
Measured at §6.2: both copy and move assignment moved the counter `2 → 3` and
both produced `InvalidOperationException`. Do not "restore" value-copying
assignment.

---

## 24. Performance and allocation analysis

`build-probe/1799_probe4_cost.cpp`, `-O2`, eight entries (the size .NET
recommends this type for), 2,000,000 operations per measurement, global
`operator new` counted. Three runs each; log `build-probe/1799_cost.log`.

| Operation | Committed | Proposed | Delta |
|---|---|---|---|
| `setItem` replace | 1.30 – 1.66 ns | 1.53 – 1.60 ns | **+0.2 ns**, one 64-bit increment plus one null compare |
| `getItem` hit | 3.07 – 3.53 ns | 2.37 – 2.86 ns | within noise (the shim's single-locator loop is if anything slightly better) |
| `Contains` hit | 1.15 – 1.46 ns | 1.32 – 1.71 ns | within noise |
| `setItem` + `Remove` pair | 14.88 ns | 14.32 ns | within noise |

| Allocations | Committed | Proposed |
|---|---|---|
| `setItem` replace × 2,000,000 | **0** | **0** |
| `getItem` × 2,000,000 | **0** | **0** |
| `Contains` × 2,000,000 | **0** | **0** |
| `setItem` insert + `Remove` × 200,000 | 200,000 (one node each) | 200,000 |

**No allocation is added anywhere.** Unlike `Hashtable::toKey`, which builds a
`std::string` per raw-key call, `ValidatedKey` is a compare and a pointer copy.

---

## 25. Migration guidance

For a consumer of `ListDictionaryInternal` or of `IDictionary`:

1. **Rebuild completely.** A stale object file links with zero diagnostics and
   then silently keeps — or, at `-O0` with an unlucky link order, silently
   reimposes — the old behaviour (§22).
2. **A null key now throws.** If code passed `nullptr` deliberately, it must
   choose a real sentinel address. Nothing legitimate is lost: no valid object
   has the null address, so the null key never named anything a real key could.
3. **An enumerator now fails fast across a value replacement.** Code that
   replaced a value mid-enumeration and kept enumerating was reading
   post-mutation data with no diagnostic; it must re-acquire the enumerator, as
   it already had to across an insert.
4. **Recover a key from the key view's `CopyTo` with
   `std::any_cast<const void*>`**, not `std::any_cast<void*>`. The old spelling
   still compiles and throws `std::bad_any_cast` at run time — the one silent
   source-compatible meaning change in this design, and the reason §36.3 is a
   separate approval item.
5. Value recovery, entry recovery, `getItem`, `Count`, both views' liveness and
   ownership, and every `CopyTo` validation rule are **unchanged**.

---

## 26. Permanent test plan

A new suite, `modules/collections/tests/System/Collections/ListDictionarySetterContractTests.cpp`,
plus targeted extensions. Every assertion about the *interface* is
parameterised over **both** `IDictionary` implementations, reusing
`DictionaryKeyAndViewContractTests.cpp`'s existing harness shape.

1. **Versioning** (parameterised where the interface allows): insert bumps;
   replace bumps; equal-value replace bumps; a throwing duplicate `Add` does
   not bump and does not change `Count`; `Remove` present bumps; `Remove`
   absent does not; `Clear` bumps even when empty; copy and move assignment
   bump the destination.
2. **Invalidation**: an outstanding enumerator throws
   `InvalidOperationException` after a value replacement, on the dictionary
   enumerator, the key view, the value view, and through an `IDictionary&` — on
   **both** implementations.
3. **Null keys**: the five `HashtableNullKey` tests are **generalised, not
   duplicated**, into a parameterised `DictionaryNullKey` suite covering
   `Add`/`setItem`/`getItem`/`Contains`/`Remove` on both implementations, each
   asserting `ArgumentNullException`, `paramName == "key"`, and that neither
   `Count` nor the version counter moved.
4. **Rollback**: after every rejected call, `Count`, contents and the counter
   are unchanged, and an outstanding enumerator is still valid.
5. **Representation**: one test asserting that the key view's `Current`, the
   key view's `CopyTo`, the enumerator's `Key`, `DictionaryEntry::Key` and the
   typed `CopyTo`'s `Key` all report the *same* `type()`, and that it is
   `const void*`; the mirror test for values and `void*`.
6. **Updated, not deleted** — the three assertion lines of §11:
   `CopyToBoundaryTests.cpp:540,541` and
   `test/consumer/collections_copyto.cpp:110`, plus the test name
   `DictionaryViewsBoxKeysAndValuesIdentically` and its comment, which state
   the superseded rule.
7. **Preserved unchanged**: `CollectionVersionCounterTests.cpp`'s
   `ListDictionaryAdapter` (including `kHasNoOpMutation = true` and the
   `sizeof == 40` assertion), every `DictionaryEnumeratorKeyValueSafetyTests`
   assertion from #1794, and every `HashtableValueAccessSafetyTests` assertion
   from #1796.

---

## 27. Sanitizer plan

Re-run §8's six scenarios against the fixed headers:

- `enumerate-after-replace` must change from "no throw" to
  `InvalidOperationException` — a **behaviour** assertion, not a sanitizer one.
- `write-through-keyview-copyto` must become **impossible to write**: the
  `any_cast<void*>` throws `std::bad_any_cast` before any write. The SEGV must
  not be reachable.
- `null-key-round-trip` must become `ArgumentNullException`.
- `non-trivial-values` must stay at **0 leaks**, with LSan proved active by the
  same 317-byte self-test in the same run.
- ASan and UBSan must report **0** on the full `Collections.Core` suite and on
  the consumer fixtures, as #1794 and #1796 required.

---

## 28. Consumer-fixture plan

- **Positive**: extend `test/consumer/collections_dictionary_views.cpp`'s
  `rejectsNullKeys()`, which today exercises `Hashtable` only, to run the same
  five checks against `ListDictionaryInternal` **through an `IDictionary&`** —
  the shape that proves the *interface* contract rather than a type's.
- **Positive**: a new check that an outstanding enumerator obtained through
  `IDictionary&` throws after `setItem` replaces a value, on both
  implementations.
- **Updated**: `test/consumer/collections_copyto.cpp:110` to
  `std::any_cast<const void*>`.
- **Negative**: a `test/consumer/collections_dictionary_setter_negative.cpp`
  is **not** proposed. There is nothing to reject at compile time: no signature
  changes, and the one meaning change (`any_cast<void*>` on a key slot) is a
  run-time `std::bad_any_cast` by design. Adding a negative fixture that cannot
  fail to compile would be theatre. This is a deliberate deviation from the
  #1796 pattern and is stated so it is not mistaken for an omission — and it is
  a second reason the §29 CI gap (ticket #1801) matters less here than there.

---

## 29. Implementation phases

**Phase 1 — behaviour, needs the §36 approval.** `ValidatedKey`, `findNode`,
the five rewritten bodies, the key-view `copyToCore`, the doc-comment
corrections of §14.4, the permanent suite of §26, the fixture changes of §28,
`README.md`'s breaking-changes entry with the **mandatory full rebuild**.

**Phase 2 — none.** Unlike #1796 there is no signature-change phase, so there
is nothing to split off, and no part of this can be landed as a
"no-approval-needed" preparatory step that would look like remediation. If the
approval is declined, the correct outcome is §31, not a partial landing.

---

## 30. Relationship to other tickets, and implementation order

- **#1794 (done)** — the `MoveNext`-time `DictionaryEntry current_` snapshot and
  the owning `std::any` Key/Value. **Preserved exactly.** This design reads only
  the snapshot, adds no accessor, and keeps "the caller's `const` survives the
  boxing" — indeed it extends that rule to the one surface #1794's approval did
  not cover. Its own record already notes the residual asymmetry
  (`IDictionaryEnumeratorKeyValueSafetyDesign.md` §37: "the key view boxes
  `const void*` while `copyToCore` normalises the key to `void*`"); §14.3 closes
  it.
- **#1796 (done)** — the `Hashtable` value-access contract and the mechanical
  `getItem` → `std::any` migration on this type. **No conflict in either
  order**, as #1796's own notes state: it changed this type's *return type* and
  deliberately none of its behaviour. This design changes behaviour and no
  signature. `Hashtable::ValueReference`, the owning `getItem`/`at`/`const`
  indexer, and the `KeyNotFoundException` change are all untouched.
- **#1791 (blocked)** — the `List<T>` indexer proxy. **No shared abstraction is
  proposed, and none should be.** `HashtableValueAccessSafetyDesign.md` §24
  rejected a shared proxy for `Hashtable`/`List<T>` on four measured
  incompatibilities; this type needs **no proxy at all** — it hands out no alias
  and its setter is already a normal method. Inventing a shared "tracked upsert"
  abstraction across three types whose locators, element types and ownership
  models all differ would be a rewrite in the shape of a fix. #1791 stays
  `blocked`.
- **#1802 (new, inactive)** — `Hashtable::Remove` bumps on an absent key where
  .NET does not (§9.4). Independent; closing it makes the two implementations
  agree on all ten version rows.
- **Recommended order: #1798 before #1791.** #1798 changes no signature, no
  layout and no symbol, and its rebuild hazard is silent-but-benign; #1791
  Phase 2 grows a public object and silently changes what `list[i]` means.
  Landing the cheap, layout-neutral one first keeps the two rebuild events
  distinguishable. **The migrations must not be merged.**
- **#1773, #1788, #1789 (blocked)** — untouched, and none of them is a
  prerequisite.

---

## 31. Fallback if the approval is declined

There is no partial fallback that closes any defect class, and this must not be
misrepresented:

- **Declining §36.1 (null keys)** leaves the two `IDictionary` implementations
  disagreeing on five entry points. The only honest outcome is to record the
  divergence as a **permanent accepted decision** in the header and in
  `IDictionary.hpp`'s contract, and to correct `IDictionary`'s doc-comments,
  which currently imply a rejection contract that one implementer does not
  honour.
- **Declining §36.2 (versioning)** leaves the fail-fast contract false for
  value replacement on this implementation. `ListDictionaryInternal.hpp` must
  then say so explicitly, as an accepted permanent gap, next to the enumerator.
- **Declining §36.3 (key representation)** leaves the §8.3 SEGV reachable. The
  minimum honest response is a `@warning` on `getKeysProperty()` stating that
  the key view's `CopyTo` hands out a writable pointer to an object the caller
  declared `const`.

**None of these three may be recorded as a remediation of #1798.**

---

## 32. Risks and residual limitations

1. **Key comparison stays address-based.** Two distinct objects with equal
   contents remain distinct keys. Permanent architectural limitation, unchanged,
   documented on the class.
2. **`MoveNext`/`Reset` after the collection is destroyed remains undefined.**
   Not claimed closed.
3. **A view or enumerator outliving its dictionary remains a borrow rule**,
   documented and not enforced — the port-wide rule.
4. **The stale-object hazard is silent** (§22) and cannot be diagnosed by the
   toolchain, including under LTO with `-Wodr`.
5. **The `IDictionary` contract will be enforced by two implementations and by
   nothing structural.** A third implementer could still diverge. The
   parameterised test suite of §26 is the mitigation.
6. **Zero production callers today** means the change's real blast radius is
   whatever CNA and mobile-eggbert do, which is **unmeasured by instruction**.
7. **`ValidatedKey` is unskippable within the class, not across the codebase.**
   A caller can still pass any address of any type.

---

## 33. Rejected approaches, in one place

| Approach | Why rejected |
|---|---|
| Literal .NET `ListDictionaryInternal` parity (`++version_` first, unconditional) | Would newly invalidate enumerators on a throwing `Add` and on an absent `Remove`; contradicts .NET `Hashtable`, the port's sibling, this repository's written "effective mutation" contract, and an existing passing assertion (§9.3) |
| `const std::any&` key parameter (Alternative B) | Public source + ABI break on `IDictionary` and both implementations; collides with the measured `Add("literal", v)` address-key corruption of `HashtableValueAccessSafetyDesign.md` §13.4; fixes no versioning defect |
| Scattered `if (key == nullptr)` at five entry points | A sixth entry point silently reopens the defect; §14.1 makes it structurally impossible instead |
| Split `insertNode`/`replaceNode` helpers (Alternative D) | Two places for the version rule to drift apart — the exact mechanism of the present defect |
| Normalising every key surface to `void*` | Would reintroduce the `const`-laundering ticket #1793 removed, on four more surfaces, and keep the §8.3 SEGV |
| Comparing the old and new value to skip an equal-value bump | Value equality of a `void*` is address equality; .NET compares neither; would make the contract depend on caller aliasing |
| A shared tracked-mutation proxy with #1791 / #1796 | This type hands out no alias and needs no proxy; §30 |
| Documenting the null-key divergence instead of fixing it (Alternative F) | Documentation alone is not remediation when the audit expects parity and the sibling already rejects |
| A compile-rejection negative consumer fixture | Nothing changes at compile time; §28 |
| Adding the key address to the duplicate-`Add` message | Cosmetic, low diagnostic value for a pointer key space; listed in §9.5, not required |

---

## 34. Exact implementation-ticket scope (#1798)

In scope: §14's declarations and bodies; §14.4's doc-comment corrections;
§26's permanent suite and the three updated assertion lines; §28's fixture
changes; `README.md`'s breaking-changes entry with the mandatory full rebuild;
§27's sanitizer re-runs; re-measurement of §21/§22's ABI and layout table and a
re-run of the stale-object probe.

Out of scope, explicitly: any signature change; `Hashtable` (including #1802);
`IDictionary`'s pure-virtual set; #1791; the address-based key comparison; the
duplicate-`Add` message text; anything under `test/consumer/` not named in §28.

---

## 35. Validation performed under this design ticket

No production or test source changed, so every figure is expected to be, and
is, unchanged.

| Check | Result |
|---|---|
| `scripts/local_ci_check.sh build` | **13,657 tests across 37 executables**, zero warnings, zero errors |
| `SharpRuntimeTests_Collections_Core` | **2,371 passed** |
| `scripts/check_doxygen_warnings.sh` | Doxygen 1.9.8, **1,940** warnings, ceiling 1,942 |
| `scripts/validate_module_boundaries.py --root .` | OK — **41 modules, 90 edges** |
| `test/validate_module_boundaries_test.py` | **7/7** |
| `scripts/generate_component_catalog.py --check` | catalogue current |
| `scripts/db_consistency_check.py --db plan.sqlite3` | no consistency problems |
| `git diff --check` | clean |
| `scripts/check_selective_components.sh` | **not run** — no public header and no component metadata changed. **Required when #1798 lands.** |

### 35.1 Build directories and parallelism

| Directory | Use |
|---|---|
| `build/` | reused incrementally by the gate, `cmake --build … --parallel 3` |
| `build-probe/` | **shared**, this ticket's artefacts under a `1799_` file prefix; one compiler process per probe |
| `build-tmp/` | repository-local `TMPDIR` for the gate |

No new build directory was created. **No compilation exceeded three jobs**; the
probes are single compiler invocations and `scripts/local_ci_check.sh` hard-codes
`--parallel 3`.

Probe artefacts, all under the gitignored shared `build-probe/`:
`1799_probe1_defects.cpp` / `.log`, `1799_probe2_selected.cpp` / `.log`,
`1799_probe3_sanitizers.cpp` and its six `1799_asan_*.log`,
`1799_probe4_cost.cpp` / `1799_cost.log`, `1799_probe5_messages.cpp` /
`1799_messages.log`, `1799_abi_tu.cpp` with `1799_ldi_old.txt` /
`1799_ldi_new.txt` / `1799_vtable_*.vt`, `1799_stale_caller.cpp` /
`1799_stale_main.cpp` / `1799_stale.log`, and the selected-design shim
`1799_selected/System/Collections/ListDictionaryInternal.hpp`. **This document
is the durable evidence; the binaries are not**, and the compiled probe
binaries should be deleted once it is committed.

---

## 36. Exact user approval required

#1798 stays `blocked` until the user grants these **three** approvals,
**explicitly and per action**. Approvals granted for #1771, #1780, #1783,
#1793, #1794 or #1796 **do not carry over**.

### 36.1 A behaviour change: a null key becomes an exception

`getItem`, `setItem`, `Add`, `Contains` and `Remove` throw
`System::ArgumentNullException("key")` where they currently succeed. Measured
consequence: a call that today stores, finds, or removes an entry under the null
address starts throwing. No compile break; no signature change; zero existing
assertions change. Rationale: .NET parity on all five entry points, and the two
`IDictionary` implementations of this port currently disagree on every one.

### 36.2 A behaviour change: a value replacement invalidates outstanding enumerators

`setItem` on a present key advances the mutation counter, including for an
equal-value replacement, so a currently-silent enumeration becomes
`InvalidOperationException`. Measured consequence: four enumerator kinds that
today walk to the end after a replacement — one of which observed the
post-mutation value — will throw. No compile break; no signature change; zero
existing assertions change. **Includes approval of the two deliberate
deviations from .NET `ListDictionaryInternal` in §15** (a throwing duplicate
`Add` and an absent `Remove` do not bump).

### 36.3 A silent behaviour change: the key view's `CopyTo` element type

`MemberCollection::copyToCore` boxes `const void*` instead of `void*` for keys.
`std::any_cast<void*>` on a key slot **keeps compiling** and starts throwing
`std::bad_any_cast` at run time. Measured consequence: 3 assertion lines in 2
files, zero production callers, and the §8.3 AddressSanitizer SEGV becomes
unreachable. This is the only silent source-compatible meaning change in the
design and can be declined independently of §36.1 and §36.2.

### 36.4 Not an approval item, but a required acknowledgement

**A full consumer rebuild is mandatory.** A stale object file links with zero
diagnostics and silently keeps the old behaviour, and at `-O0` with the stale
object first on the link line it silently reimposes the old behaviour on
*rebuilt* translation units. `-flto -Wodr` does not diagnose it (§22).
