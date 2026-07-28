<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# `IDictionaryEnumerator` key/value safety — design record

Ticket **#1795** (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, P3, size M,
category `design`), local branch
`feature/remediation-coll-idictenumerator-keyvalue-design`, 2026-07-28.

Implementation is ticket **#1794**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M), which stays **blocked**.

**No production or test source changed under this ticket.**

---

## 1. Executive decision

`System::Collections::IDictionaryEnumerator::getKeyProperty()` and
`getValueProperty()` should return an **owning `std::any` by value**, equal by
construction to the corresponding member of `getEntryProperty()`'s
`DictionaryEntry`; and **every implementation must snapshot the entry into
enumerator-owned storage at `MoveNext()` time**, which one of the two already
does and the other does not.

The return-type change alone is *not* the fix, and this is the single most
important correction this design makes to the shape #1794 assumed. Three of the
eight measured defects survive a pure return-type change, because they happen
while the box is being *produced*: both accessors dereference a container
iterator that no accessor version-checks, so on `ListDictionaryInternal` even
`getEntryProperty()` and the already-migrated `getCurrentProperty()` are
AddressSanitizer `heap-use-after-free` after the dictionary is cleared or
destroyed (§8, scenarios `ld-entry-accessor-after-clear` and
`ld-current-accessor-after-collection-destroyed`).

Three further premises written into ticket #1794's own description are
**contradicted by measurement** and are corrected here rather than quietly
worked around:

1. **"They are ALREADY const-correct, so class A (const-correctness) and class B
   (mutation/version bypass) do NOT apply — there is no write path through
   them."** False for `Hashtable`. `getValueProperty()` returns a pointer to the
   live `std::unordered_map`'s `mapped_type`, which is a **non-const
   `std::any`**. `const_cast` + assignment through it is not undefined
   behaviour and not a trick — it is well-formed, fully defined C++ that
   rewrites live dictionary storage, leaves the mutation counter unmoved, and is
   invisible to a second enumerator (§8, section B: `B.live-dictionary-value-changed=1`,
   `B.write-bypassed-mutation-counter=1`, `B.second-enumerator-silent=1`).
   `getKeyProperty()` reaches the `const std::string` key, where the write *is*
   undefined behaviour, and produces an entry that `Count` still reports but
   that **no lookup can return by either its old or its new key** (§8, section
   A2).
2. **"The obvious selected shape is the same one #1793 landed: return `std::any`
   by value."** Half right. `std::any` by value is selected, but it is
   necessary and not sufficient — see the paragraph above — and the second half
   of the fix (the snapshot) is what actually closes the lifetime class.
3. **"`getEntryProperty()` already returns `DictionaryEntry` BY VALUE, so the
   by-value answer is already the convention on this very interface."** True as
   a statement about the signature, and misleading as a statement about safety:
   on `ListDictionaryInternal` that by-value `DictionaryEntry` is *built* from a
   dangling `std::list` iterator, so its by-value return buys nothing (§8).

Two further, previously unrecorded parity defects were found while measuring and
are folded into the design because the ticket must decide the `Entry`/`Current`
relationship (§16):

- `ListDictionaryInternal::NodeEnumerator::getCurrentProperty()` boxes the
  **key**, where .NET's `NodeEnumerator` is `public object Current => Entry;`.
  The two implementations of one interface therefore disagree about what
  `Current` means: `Hashtable` boxes a `DictionaryEntry`, `ListDictionaryInternal`
  boxes a `const void*` (§8, section E).
- `ListDictionaryInternal` disagrees with **itself** about the `const` on a
  value: `DictionaryEntry`'s value member is `void*`, its value view's
  `getCurrentProperty()` boxes `const void*`, and `MemberCollection::copyToCore`
  boxes `void*` (§5.4).

Measured cost of the selected design, against the whole repository:

| Measurement | Result |
|---|---|
| Translation units that stop compiling | **1 of 628** (`ListDictionaryInternalTests.cpp`, one line) |
| Permanent tests that stop passing | **2 of 2,252**, both pinning the two `ListDictionaryInternal` parity defects above |
| Mangled name of either accessor | **byte-identical** before and after |
| Vtable slot | **unchanged** (offset `0x30`) |
| Calling convention | **changed** — `this` moves `%rdi` → `%rsi`, hidden `sret` in `%rdi` |
| Stale caller + new implementation | **links with zero diagnostics**, then SEGVs; UBSan reports an invalid vptr |
| `sizeof` | unchanged everywhere **except** `ListDictionaryInternal::NodeEnumerator`, **40 → 72** |
| Allocations per `Hashtable` key read | 0 → **1** (SSO key) / **2** (heap key) |
| Allocations per `ListDictionaryInternal` key or value read | 0 → **0** |

---

## 2. Ticket handling — why #1794 was not reused

`#1794`'s database row is an **implementation** row, not a design row:

- title: *"Migrate `IDictionaryEnumerator`'s `const void*` key/value accessors
  off live storage"* — a migration verb;
- `notes`: *"BLOCKED reason: like #1793 this is a public source break on a public
  virtual … It therefore needs its own explicit approval covering (1) the source
  break … and (2) acknowledgement of a second silent ABI break"* — blocked on
  approval to perform the change, not on a decision about what the change is;
- `acceptance_criteria`: permanent regressions, a measured call-site inventory,
  and `scripts/local_ci_check.sh build` with no test-count regression.

Converting it into a "completed design ticket" to reuse its number would have
recorded implementation work as done when none was done. It therefore stays
**blocked**, and design ticket **#1795** was opened as the next available
number, made `doing`, and completed here. #1794's dependency, acceptance
criteria, and exact approval text are updated from this record (§33).

This mirrors #1792 → #1793 exactly, with the roles of the two numbers swapped:
there, the *defect* ticket was opened first and closed as the design; here, the
*implementation* ticket existed first and the design is the new number.

---

## 3. Exact current declarations

`modules/collections/include/System/Collections/IDictionaryEnumerator.hpp`:

```cpp
class IDictionaryEnumerator : public IEnumerator {
public:
    virtual ~IDictionaryEnumerator() = default;
    [[nodiscard]] virtual DictionaryEntry getEntryProperty() const = 0;   // :37
    [[nodiscard]] virtual const void* getKeyProperty() const = 0;         // :47
    [[nodiscard]] virtual const void* getValueProperty() const = 0;       // :54
};
```

Inherited from `System::Collections::IEnumerator` (unchanged by this design, and
out of scope by instruction):

```cpp
virtual bool MoveNext() = 0;
virtual void Reset() = 0;
[[nodiscard]] virtual std::any getCurrentProperty() const = 0;            // #1793
```

`System::Collections::DictionaryEntry` (`DictionaryEntry.hpp`) is a value struct
holding two `std::any` members, with `getKeyProperty()`/`getValueProperty()`
returning `const std::any&`, `setKeyProperty`/`setValueProperty`, `Deconstruct`,
and `ToString`. `sizeof(DictionaryEntry) == 32`; `sizeof(std::any) == 16`.

`System::Collections::Generic::IEnumerator<T>` (`Generic/IEnumerator.hpp`) is
**not** in this hierarchy: no generic dictionary enumerator derives from
`IDictionaryEnumerator`, and `Current()` stays `const T&`. It is listed here only
to record that it was checked, not assumed.

The header already carries a `@warning` block, added by #1793, pointing at
`docs/IEnumeratorCurrentSafetyDesign.md` §15 and §30 risk 4. That warning
under-states the problem in exactly the way §1 corrects.

---

## 4. Interface hierarchy inventory

| Type | File | Relationship |
|---|---|---|
| `System::Collections::IEnumerator` | `IEnumerator.hpp` | base; `MoveNext`/`Reset`/`getCurrentProperty` |
| `System::Collections::IDictionaryEnumerator` | `IDictionaryEnumerator.hpp` | derives from `IEnumerator`; adds `Entry`/`Key`/`Value` |
| `System::Collections::IDictionary` | `IDictionary.hpp:120` | `[[nodiscard]] virtual IDictionaryEnumerator* GetEnumerator() override = 0;` — covariant narrowing of `IEnumerable::GetEnumerator()` |
| `System::Collections::DictionaryEntry` | `DictionaryEntry.hpp` | the value type `Entry` returns |
| `System::Collections::Generic::IEnumerator<T>` | `Generic/IEnumerator.hpp` | **unrelated**; no dictionary enumerator derives from it |
| `System::Collections::Generic::IAsyncEnumerator<T>` | — | **unrelated**; spells its *typed* accessor `getCurrentProperty()`; the known naming collision recorded by #1793 §4 risk 7, deliberately not repaired |

Ownership of the enumerator object itself is unchanged by this design:
`GetEnumerator()` returns a heap-allocated object and **the caller takes
ownership**, the convention this port uses throughout because it has no GC.

---

## 5. Complete implementation inventory

Measured by compiling, not by grepping: the whole-repository sweep
(`build-probe-idictenum/sweep_callsites.log`) replayed all **628** entries of
`build/compile_commands.json` with a `[[deprecated]]`-tagged shim first on the
include path, `-fsyntax-only`, four parallel jobs, **0 compile failures**.

### 5.1 Production implementations — exactly two

| # | Class | File:line | Visibility | Escapes as |
|---|---|---|---|---|
| 1 | `Hashtable::Enumerator` | `Hashtable.hpp:375–436` | **private** nested | `IDictionaryEnumerator*` from `Hashtable::GetEnumerator()` (`:318`) |
| 2 | `ListDictionaryInternal::NodeEnumerator` | `ListDictionaryInternal.hpp:51–111` | **private** nested | `IDictionaryEnumerator*` from `ListDictionaryInternal::GetEnumerator()` (`:302`) |

Both are private nested classes: **no consumer can name, allocate, embed, or
derive from either**. That fact is what keeps the object-layout consequence in
§23 out of the *public* layout category — and it is also exactly why the stale
object hazard in §22.4 is real, because `GetEnumerator()` is defined `inline` in
the header and therefore *its allocation is emitted into the consumer's own
translation unit*.

### 5.2 Adapters that consume an `IDictionaryEnumerator` through `IEnumerator`

| # | Class | File:line | Calls |
|---|---|---|---|
| 1 | `Hashtable::MemberCollection::MemberEnumerator` | `Hashtable.hpp:466–502` | `inner_->getKeyProperty()` (`:499`), `inner_->getValueProperty()` (`:500`) |
| 2 | `ListDictionaryInternal::MemberCollection::Enumerator` | `ListDictionaryInternal.hpp:118–138` | both, at `:136` |

Both own their inner `IDictionaryEnumerator*` and delete it. Both were rewritten
by #1793 to *copy out* instead of `const_cast`-ing; that removed the **write**
path that reached these accessors indirectly, and left the read path.

### 5.3 Test-local implementations, mocks, fixtures — **none**

There is no hand-written `IDictionaryEnumerator` anywhere in
`modules/*/tests/`, `test/`, or `tests/`. This is a genuine difference from
#1792, which found two hand-written `IEnumerator` implementers in this
repository's own tests and used them as evidence that consumers implement these
interfaces by hand. Nothing comparable exists for this interface, which
*narrows* the migration burden but does not eliminate it: absence in this
repository is not evidence about CNA or mobile-eggbert, which were not inspected
(§30 risk 3).

### 5.4 What each implementation actually returns

This table is the core of the inventory. **`Key` and `Value` do not behave
alike, and the two implementations do not behave alike.**

| Aspect | `Hashtable::Enumerator` | `ListDictionaryInternal::NodeEnumerator` |
|---|---|---|
| `getKeyProperty()` returns | `&it_->first` — a pointer **into live map storage** | `it_->key` — **the caller's own pointer**, stored verbatim |
| Pointee type | `const std::string` (map key, genuinely `const`) | whatever the caller passed; the dictionary never dereferences it |
| `getValueProperty()` returns | `&it_->second` — a pointer **into live map storage** | `it_->value` — **the caller's own pointer** |
| Pointee type | `std::any` — **non-`const`** (`mapped_type`) | whatever the caller passed |
| Storage owner | the dictionary | the **caller** |
| `const_cast` write reaches | **live dictionary storage** | the caller's object; the dictionary is untouched |
| `const_cast` write is | key: **UB**; value: **well-defined** | well-defined (the value was never `const` — the interface narrowed it) |
| Write advances the mutation counter | **no** | n/a (nothing dictionary-side changes) |
| Write breaks lookup | **yes** for the key: the entry becomes unreachable | **no** — keys are compared by *address*, so mutating the pointee is irrelevant |
| `getEntryProperty()` source | the enumerator's own `current_` cache, filled at `MoveNext` | rebuilt from `it_->…` **on every call** |
| `Entry.Key` boxed type | `std::string` | `const void*` |
| `Entry.Value` boxed type | the stored value's own type (e.g. `int`) — **not** a nested `std::any` | `void*` |
| `getCurrentProperty()` boxes | `DictionaryEntry` | **`const void*` — the key only** |
| Accessors version-check | **no** | **no** |
| Accessors dereference the container | `it_` for Key/Value; **not** for Entry/Current | `it_` for **all four** |
| `sizeof` | 72 | 40 |

The `Hashtable` value view's element shape and `ListDictionaryInternal`'s
internal disagreement about `const` are both visible here: `Entry.Value` is
`void*`, the value view's `Current` is `const void*` (because it forwards
`getValueProperty()`), and `MemberCollection::copyToCore` writes `void*`. Three
spellings, one datum.

---

## 6. Affected-surface table

| Surface | Kind | Changes under this design |
|---|---|---|
| `IDictionaryEnumerator::getEntryProperty()` | public pure virtual | **signature unchanged**; becomes the canonical representation |
| `IDictionaryEnumerator::getKeyProperty()` | public pure virtual | **`const void*` → `std::any`** |
| `IDictionaryEnumerator::getValueProperty()` | public pure virtual | **`const void*` → `std::any`** |
| `IEnumerator::getCurrentProperty()` | public pure virtual (inherited) | **signature unchanged** (`std::any`, #1793) |
| `Hashtable::Enumerator` | private nested override | two bodies; already has the snapshot |
| `ListDictionaryInternal::NodeEnumerator` | private nested override | four bodies **plus a new `DictionaryEntry current_` member and a `MoveNext` line** |
| `Hashtable::MemberCollection::MemberEnumerator` | private nested | one body simplifies (`static_cast` pair disappears) |
| `ListDictionaryInternal::MemberCollection::Enumerator` | private nested | one body simplifies |
| `DictionaryEntry` | public value struct | **unchanged** |
| `IDictionary`, `ICollection`, `IEnumerable` | public interfaces | **unchanged** |
| `Hashtable`, `ListDictionaryInternal` public API | public | **unchanged** |
| `Generic::IEnumerator<T>` | public template | **unchanged** |

---

## 7. Defect taxonomy

Seven classes, kept separate because different measures close them and they do
not all apply to both implementations. `HT` = `Hashtable::Enumerator`,
`LD` = `ListDictionaryInternal::NodeEnumerator`.

| Class | Name | HT key | HT value | LD key | LD value | Closed by |
|---|---|:---:|:---:|:---:|:---:|---|
| **A** | Const-correctness bypass — `const_cast` reaches mutable storage | ✅ real (UB write) | ✅ real (**defined** write) | ➖ hits the caller's object | ➖ hits the caller's object | owning value |
| **B** | Mutation/version bypass — the write does not advance the counter | ✅ real | ✅ real | ➖ | ➖ | owning value |
| **C** | Type safety — no runtime type, size, or alignment | ✅ | ✅ | ✅ | ✅ | owning value (`std::any::type()`, `bad_any_cast`) |
| **D** | Lifetime — the address, or the iterator behind it, outlives its storage | ✅ | ✅ | ✅ | ✅ | **snapshot at MoveNext** + owning value |
| **E** | Key-integrity corruption — a mutated key violates the hash invariant | ✅ | ➖ | ➖ (address-keyed) | ➖ | owning value |
| **F** | Interface inconsistency — `Entry`/`Current` own, `Key`/`Value` borrow | ✅ | ✅ | ✅ | ✅ | owning value |
| **G** | Implementation divergence — one signature, incompatible pointee types and `Current` payloads | ✅ | ✅ | ✅ | ✅ | owning value + `Current == Entry` |

Two important consequences of keeping them separate:

- **A and B genuinely do not apply to `ListDictionaryInternal`.** Its accessors
  hand back pointers the caller already owns and already has; writing through
  them is something the caller could always do, and it cannot corrupt the
  dictionary, whose key comparison is by address (`Contains(&key)` still
  succeeds after the pointee is rewritten — §8, section C). Saying "`const void*`
  is unsafe" about this implementation would be wrong.
- **D applies to `ListDictionaryInternal` more broadly than to `Hashtable`**,
  and in the opposite direction from what the signatures suggest: `Hashtable`
  caches, so its `Entry` and `Current` are safe-but-stale after a mutation,
  while `ListDictionaryInternal` caches nothing, so **all four** of its
  accessors — including the `std::any`-returning `getCurrentProperty()` that
  #1793 already migrated — are use-after-free.

---

## 8. Pre-fix reproduction

All probes are repository-local under `build-probe-idictenum/` (gitignored by the
`build*` entry), compiled `-std=c++23 -Wall -Wextra -Wpedantic`, one compiler
process at a time — never more than four, per `CLAUDE.md`. No production header
was modified to obtain any of this. `-fno-access-control` is used **only** so the
probe can read the collection's private mutation counter and name its private
nested enumerator; it changes no code path.

### 8.1 `probe1_current.log` — the write paths, per implementation

```
--- A. Hashtable::Enumerator::getKeyProperty ---
A.pointee-type=NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
A.aliases-live-map-key=1
A.version-before=3            A.version-after=3
A.write-bypassed-mutation-counter=1
A.count=3   A.contains-old-key=0   A.contains-new-key=1
A.second-enumerator-silent=1
A.entry-key-after=alpha       A.entry-copy-followed-the-write=0
```

Three entries is a small enough table that `"alpha"` and `"gamma"` share a
bucket, so the rewritten key is still findable under its *new* name. At scale it
is not:

```
--- A2. Hashtable key corruption at scale ---
A2.count-before=64   A2.original-key=key7
A2.count-after=64    A2.contains-old-key=0   A2.contains-new-key=0
A2.entry-unreachable-by-either-key=1
A2.count-still-includes-lost-entry=1
```

The table reports 64 entries and one of them can never be returned again by any
lookup. This is the same shape #1793's own pre-fix probe recorded at 64 entries,
reproduced here through a **different** accessor on a **different** interface.

```
--- B. Hashtable::Enumerator::getValueProperty ---
B.pointee-type=St3any        B.boxed-value-type=i
B.aliases-live-map-value=1
B.write-is-well-defined-not-ub=1
B.write-bypassed-mutation-counter=1
B.stored-type-after=NSt7__cxx1112basic_string…
B.live-dictionary-value-changed=1
B.stored-value-after=rewritten through the enumerator
B.second-enumerator-silent=1
B.entry-copy-followed-the-write=0   B.current-copy-followed-the-write=0
```

`ht.at("beta")` — the dictionary's own public read path — returns the string the
probe wrote through a `const void*` obtained from a `const` accessor. **This is
the fact that refutes #1794's "no write path exists" premise**, and it is not
undefined behaviour: the `std::any` inside `std::unordered_map<std::string,
std::any>` is not a `const` object, so `const_cast` + assignment is defined.

```
--- C/D. ListDictionaryInternal::NodeEnumerator ---
C.key-is-callers-object=1        D.value-is-callers-object=1
C.key-aliases-dictionary-storage=0
D.write-hits-callers-object-not-dictionary=1
C.key-object-after=999           C.lookup-still-finds-entry=1
C.count=2   C.version-before=2   C.version-after=2
D.getItem-returns-same-pointer=1
```

```
--- E. Entry vs Current vs Key vs Value ---
E.hashtable.current-boxed-type=N6System11Collections15DictionaryEntryE
E.hashtable.entry-key-type=…basic_string…    E.hashtable.entry-value-type=i
E.listdict.current-boxed-type=PKv
E.listdict.entry-key-type=PKv                E.listdict.entry-value-type=Pv
E.current-shape-diverges-between-implementations=1
```

```
--- F. state machine ---
F.{hashtable,listdict}.{before-first,after-end}.{current,entry,key,value}
    = InvalidOperationException:Enumeration has either not started or has already finished.
```

All sixteen state-machine cases already agree, on both implementations, with
.NET's `SR.InvalidOperation_EnumOpCantHappen`. `defects=20`.

Re-run under UndefinedBehaviorSanitizer (`probe1_ubsan.log`): **0 runtime
errors, exit 0**. The key corruption, the value rewrite, and the lost entry are
invisible to UBSan.

### 8.2 `probe2_asan_*.log` — lifetime, one scenario per process

AddressSanitizer + UndefinedBehaviorSanitizer, `detect_leaks=0`.

| Scenario | Result |
|---|---|
| retained key pointer, then `MoveNext` | survives — **silently names the previous entry** |
| retained key pointer, then `Reset` | survives — stale |
| retained key pointer, then 300 × `Add` (rehash) | survives (libstdc++ keeps nodes stable) |
| retained key pointer, then `Remove` of that entry | **ASan `heap-use-after-free`** |
| retained key pointer, then `Clear` | **ASan `heap-use-after-free`** |
| retained **value** pointer, then `Clear` | **ASan `heap-use-after-free`** |
| retained key pointer, then collection destroyed | **ASan `heap-use-after-free`** |
| retained key pointer, then **enumerator** destroyed | survives (it aliases the *table*, not the enumerator) |
| call `getKeyProperty()` **again** after `Remove` | **ASan `heap-use-after-free`** |
| call `getValueProperty()` **again** after `Clear` | **ASan `heap-use-after-free`** |
| call `getEntryProperty()` again after `Clear` (`Hashtable`) | safe — stale cached copy |
| call `getCurrentProperty()` after the `Hashtable` is destroyed | safe — stale cached copy |
| `ListDictionaryInternal`: retained key pointer, then `Clear` | safe — it is the *caller's* object |
| `ListDictionaryInternal`: call `getKeyProperty()` again after `Clear` | **ASan `heap-use-after-free`** |
| `ListDictionaryInternal`: call `getEntryProperty()` again after `Clear` | **ASan `heap-use-after-free`** |
| `ListDictionaryInternal`: `getCurrentProperty()` after the dictionary is destroyed | **ASan `heap-use-after-free`** |

**Eight AddressSanitizer `heap-use-after-free` reports.** The last three are the
ones that change the design: they are not about the `const void*` return type at
all, they are about an unchecked container iterator, and two of them are on
accessors whose return type is already an owning value.

UndefinedBehaviorSanitizer alone (`probe2_ubsan_*.log`), on six of the fatal
scenarios: **0 UBSan diagnostics**. Three SEGV; **three complete successfully
and print a plausible wrong answer** — `ht-value-accessor-after-clear`,
`ld-entry-accessor-after-clear`, `ld-current-accessor-after-collection-destroyed`.
Silent wrong data is the common case, not the crash.

LeakSanitizer on the five survivable scenarios: **0 leaks**, with leak detection
proved active by a deliberate-leak self-test reporting `284 byte(s) leaked in 1
allocation(s)` (`probe2_lsan_selftest.log`).

### 8.3 `probe3_asan_*.log` — type confusion

```
same-width-wrong-type:  actual float 3.5 read as int -> 1080033280
                        actual int 42 read as float -> 5.88545355e-44
                        silently-wrong=1   diagnostic-from-any-tool=0   exit=0
hashtable-value-shape:  stored 1234, read as int -> -1935366002
                        silently-wrong=1   diagnostic-from-any-tool=0   exit=0
cross-impl-hashtable:   key=alpha                                        exit=0
cross-impl-listdict:    ERROR: AddressSanitizer: stack-buffer-overflow
                        [256, 260) 'k' <== Memory access partially overflows this variable
any-cast-is-checked:    wrong-any_cast-on-current   = std::bad_any_cast
                        wrong-any_cast-on-entry-key = std::bad_any_cast
                        entry-key-runtime-type-queryable = …basic_string…
                        key-accessor-runtime-type-queryable = 0
```

`cross-impl-*` is the same function — `*static_cast<const std::string*>(e.getKeyProperty())`
— called through the same `IDictionaryEnumerator&`. Correct against one
implementation, a stack-buffer-overflow against the other. That is class G in
one measurement, and no amount of caller discipline fixes it, because the
interface does not say which implementation the caller has.

The contrast in the last block is the whole argument for the selected design:
the accessors that already return a box are checked, and the ones that return an
address are not.

### 8.4 Evidence retention

`build-probe-idictenum/` is preserved in place with every `.log`. Files:
`probe1_current.log`, `probe1_ubsan.log`, `probe2_asan_*.log` (16),
`probe2_ubsan_*.log` (6), `probe2_lsan_*.log` (5) + `probe2_lsan_selftest.log`,
`probe3_asan_*.log` (5), `probe4_alloc*.log`, `abi/probe5_symbols.log`,
`abi/probe5_callconv.log`, `probe6_stale.log`, `probe6_stale_ubsan.log`,
`probe6b_stale_ld.log`, `probe7_layout_{before,after}.log`, `probe8_{asan,lsan,ubsan}.log`,
`probe9_asan_*.log` (6), `sweep_callsites.log`, `sweep_breaks_any.log`,
`sweep_breaks_ownedcopy.log`, `rebuild_against_shim.log`, `shim_suite.log`,
`shim_selftest.log`.

---

## 9. .NET comparison

Read from `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/`
and `…/src/Resources/Strings.resx` at the current checkout, not from memory.

### 9.1 The interface

`IDictionaryEnumerator.cs`:

```csharp
public interface IDictionaryEnumerator : IEnumerator
{
    object Key { get; }
    object? Value { get; }
    DictionaryEntry Entry { get; }
}
```

Three properties returning **values**, never addresses. `Key` is non-nullable,
`Value` is nullable — .NET's dictionaries permit a null value and not a null key,
which this port reproduces through `Hashtable::toKey()`'s
`ArgumentNullException` (#1775). The file's own comment states the validity
window verbatim: *"The values returned by calls to `Key` and `Value` are
undefined before the first call to `MoveNext` and following a call to `MoveNext`
that returned false"*, and *"Multiple calls to `GetKey` with no intervening calls
to `GetNext` will return the same object."* It also states why `Entry` exists:
*"the `GetEntry` method will return the same `DictionaryEntry` and avoids boxing
the `DictionaryEntry` (boxing is somewhat expensive)"* — i.e. **`Entry` is the
unboxed form of `Current`, and `Current` is `Entry` boxed.**

### 9.2 `HashtableEnumerator`

```csharp
private object? _currentKey;
private object? _currentValue;

public bool MoveNext() {
    if (_version != _hashtable._version) throw new InvalidOperationException(SR.InvalidOperation_EnumFailedVersion);
    …  _currentKey = keyv; _currentValue = _hashtable._buckets[_bucket].val; _current = true; …
}
public object Key            { get { if (!_current) throw …EnumNotStarted;   return _currentKey!; } }
public object? Value         { get { if (!_current) throw …EnumOpCantHappen; return _currentValue; } }
public DictionaryEntry Entry { get { if (!_current) throw …EnumOpCantHappen; return new DictionaryEntry(_currentKey!, _currentValue); } }
public object? Current       { get { if (!_current) throw …EnumOpCantHappen;
                                     return _getObjectRetType == Keys   ? _currentKey
                                          : _getObjectRetType == Values ? _currentValue
                                          : new DictionaryEntry(_currentKey!, _currentValue); } }
```

**.NET snapshots the key and the value into enumerator fields at `MoveNext`
time**, and every one of the four accessors reads those fields. Not one of them
reaches back into `_buckets`. That is precisely the snapshot this design adopts,
and it is a stronger match to .NET than the current port achieves on either
implementation.

Note also that `Current` is *implementation-selected*: one `HashtableEnumerator`
class serves the entry enumerator and both member views, choosing the payload by
`_getObjectRetType`. This port instead uses two classes — `Hashtable::Enumerator`
and `Hashtable::MemberCollection::MemberEnumerator` — which is a different
factoring of the same behaviour and is not a defect.

### 9.3 `ListDictionaryInternal.NodeEnumerator`

```csharp
private DictionaryNode? current;
public object Current => Entry;                                   // <= Current IS Entry
public DictionaryEntry Entry { get { if (current == null) throw …EnumOpCantHappen;
                                     return new DictionaryEntry(current.key, current.value); } }
public object  Key           { get { if (current == null) throw …EnumOpCantHappen; return current.key; } }
public object? Value         { get { if (current == null) throw …EnumOpCantHappen; return current.value; } }
public bool MoveNext() { if (version != list.version) throw …EnumFailedVersion;
                         current = start ? list.head : current?.next; start = false; return current != null; }
```

`current` is a **strong reference to the node**, so `Clear()` — which in .NET
just drops `head` — cannot make it dangle; the GC keeps the node alive for
exactly as long as the enumerator holds it. That is the guarantee a C++ port has
no way to inherit, and the reason the C++ enumerator must copy rather than point.

`Current => Entry` is unambiguous. The port's
`return std::any(getKeyProperty());` is a parity defect, recorded in §16.

### 9.4 Exact messages

| Symbol | Text |
|---|---|
| `InvalidOperation_EnumNotStarted` | `Enumeration has not started. Call MoveNext.` |
| `InvalidOperation_EnumEnded` | `Enumeration already finished.` |
| `InvalidOperation_EnumOpCantHappen` | `Enumeration has either not started or has already finished.` |
| `InvalidOperation_EnumFailedVersion` | `Collection was modified; enumeration operation may not execute.` |

Both port implementations already use `EnumOpCantHappen`'s text for all four
accessors. .NET's `HashtableEnumerator.Key` alone uses `EnumNotStarted` instead
— a one-property inconsistency inside .NET itself, which .NET's own
`ListDictionaryInternal` does not copy. **This design does not adopt it**: the
port is already self-consistent, matches `ListDictionaryInternal` exactly, and
changing one accessor's message to match a .NET quirk would be a gratuitous
behaviour change with a permanent test to rewrite. Recorded as a deliberate,
documented deviation rather than left unstated.

### 9.5 Semantic mapping, stated once

| .NET | This port | Why |
|---|---|---|
| `object` (reference type) | `std::any` holding a handle (`void*`, `std::shared_ptr<X>`, …) | copying the box copies the handle, not the pointee — the same aliasing .NET has |
| `object` (boxed value type) | `std::any` holding a copy | a box is a copy in both languages |
| C++ raw pointer | stays a raw pointer *inside* the box | `ListDictionaryInternal` is address-keyed by design; boxing the address is the faithful translation |
| `std::any`-valued element (`Hashtable`'s `mapped_type`) | box holds the **payload**, not a nested box | measured: `std::any(std::any)` selects the copy constructor, never the value-forwarding one (§18) |
| `null` value | `std::any` with `has_value() == false` | `Hashtable::setItem`/`Add` already store `std::any{}` for a null value |
| `Entry` unboxed vs `Current` boxed | `DictionaryEntry` by value vs `std::any(DictionaryEntry)` | the port's `Entry` is already the cheaper unboxed form |

---

## 10. Relationship to ticket #1793

| Question | Answer | Basis |
|---|---|---|
| Should `Key` return `std::any` by value? | **Yes** | .NET returns `object`; class C/F close only with a box |
| Should `Value` return `std::any` by value? | **Yes** | same |
| Should `Entry` stay `DictionaryEntry` by value? | **Yes, unchanged** | already owning; .NET's `Entry` is the unboxed form on purpose |
| Should `Current` keep returning `std::any(DictionaryEntry)`? | **Yes — and `ListDictionaryInternal` must start doing so** | .NET: `Current => Entry` |
| Should `Key`/`Value` return exactly the values inside `Entry`? | **Yes, by construction** | removes class F/G at the source; makes the three accessors one datum |
| How is nested `std::any` avoided? | It is not *avoided*, it does not *occur* | `std::any(const std::any&)` is a copy, not a wrap (§18); pinned by test |
| What if the stored key/value type is already `std::any`? | `Hashtable`'s value **is**, and the box holds the payload | measured: `alloc` probe reports `hashtable-value-int` box type `int` |
| Pointer-valued keys/values? | Boxed as the pointer; `const void*` for `ListDictionaryInternal`'s key, `void*` for its value | matches `DictionaryEntry` and `getItem()`'s own types |
| Can `Key`/`Value` be non-copyable? | **No** — and this is where #1793's hardest constraint does not transfer | see below |
| Do the non-generic dictionaries support non-copyable keys/values today? | **No, and they cannot** | see below |

**The move-only problem does not exist here, and that is a real difference from
#1793.** `Generic::IEnumerator<T>` is a template over an arbitrary `T`, so #1793
had to decide what a move-only `T` does and chose `NotSupportedException`.
`IDictionaryEnumerator` is **not** a template. Its two implementations store
`std::string`/`std::any` (`Hashtable`) and `const void*`/`void*`
(`ListDictionaryInternal`) — four types, all trivially or cheaply copyable, all
fixed at compile time. `DictionaryEntry` already stores two `std::any`s, and
`std::any` **already requires** its contained type to be copy-constructible, so a
move-only key or value has been impossible on this interface since
`DictionaryEntry` was written. There is therefore:

- no `NotSupportedException` path on this interface;
- no `if constexpr` branch;
- no new constraint on any consumer;
- nothing to approve about non-copyable types.

The design does **not** mechanically copy #1793. It takes #1793's return-type
answer, drops #1793's move-only machinery as inapplicable, and adds the snapshot
requirement that #1793 did not need (`Generic::IEnumerator<T>`'s bridge calls
`Current()`, which is state-checked by every implementation; these accessors call
nothing).

---

## 11. Alternatives evaluated

### 11.1 The candidates

- **A — `std::any` by value on `Key` and `Value`.** Owning boxes, runtime type,
  `bad_any_cast` on a wrong read. Does not, by itself, stop an accessor from
  dereferencing a dangling iterator while building the box.
- **B — derive `Key`/`Value` from the `Entry` copy.** Keep `Entry` canonical;
  `Key`/`Value` return its members. Structurally guarantees the three agree.
- **C — remove `Key` and `Value`; callers use `Entry` or `Current`.** Maximal
  simplicity, minimal parity: .NET has all three, and `Entry`+`Current` cannot
  express "give me only the key" without materialising both.
- **D — a read-only type-erased descriptor** (`{ const void* data; const std::type_info& type; size_t size; }`).
  Adds a new public type and new runtime infrastructure to deliver strictly less
  than `std::any` already delivers: it still hands out an address, so class D
  stays open, and it still needs the caller to write the cast.
- **E — keep `const void*` and document the lifetime.** Measured against its own
  claim: a one-line `const_cast` defeats it (§8.1 B), and three of the eight
  ASan reports happen with **no** caller misuse at all (§8.2).
- **F — enumerator-owned copies behind an unchanged `const void*`.** The only
  candidate with no source break and no calling-convention change. Measured
  separately in §11.2.
- **G — a repository `Object`/boxing abstraction instead of `std::any`.** There
  is no such type in this repository, and `System::Object` is out of scope by
  the permanent-deviation list (no common object root, no reflection). Creating
  one for two accessors is a new architecture, not a remediation.

### 11.2 Alternative F, measured rather than argued

`shim-ownedcopy/` points both implementations' `const void*` accessors at
enumerator-owned snapshot members, leaving the interface untouched
(`make_shim_ownedcopy.py`).

Repository-wide break sweep (`sweep_breaks_ownedcopy.log`): **0 of 628
translation units stop compiling.** Behaviour (`probe9_asan_*.log`):

| Scenario | Result |
|---|---|
| `const_cast` write through `Key`/`Value` | dictionary key **not** corrupted, value **not** corrupted, counter unmoved — **A, B, E closed** |
| …but the enumerator's own view | `E.enumerator-cache-desynchronised=1`, `E.entry-still-reports-the-write=1` — **a new defect**: `Key` and `Entry` now disagree, exactly the `mutable current_` shape #1793 deleted |
| same-width wrong cast | `C.read-as-int=1080033280`, `C.still-silently-wrong=1`, `C.no-runtime-type-available=1` — **C wide open** |
| retained pointer across collection destruction | survives — **that half of D closed** |
| retained pointer across `MoveNext` | `D.retained-pointer-silently-changed-meaning=1` — **silently renames itself** |
| retained pointer across **enumerator** destruction | **ASan `heap-use-after-free`** — **D still open** |
| `ListDictionaryInternal` accessor after `Clear` | safe — **the three UAFs of §8.2 closed** |

So Alternative F is not a straw man: it closes A, B, E and most of D for free,
with no source break and no ABI break. It is rejected because it leaves **C
entirely open** — the class with the widest blast radius, since the same
caller code is correct against one implementation and a stack-buffer-overflow
against the other (§8.3) — leaves **G** untouched, converts one lifetime hazard
into a quieter one instead of removing it, and **introduces** an
enumerator/collection desynchronisation that this repository deliberately
removed three tickets ago. Its zero-cost profile makes it the right **fallback**
if the approval in §33 is declined (§29).

### 11.3 Compatibility matrix

`✔` closed / satisfied, `✖` open / broken, `~` partial.

| Criterion | A | **A+B (selected)** | C | D | E | F | G |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Key integrity (class E) | ✔ | ✔ | ✔ | ✖ | ✖ | ✔ | ✔ |
| Value integrity (class B) | ✔ | ✔ | ✔ | ✖ | ✖ | ✔ | ✔ |
| Const correctness (class A) | ✔ | ✔ | ✔ | ✖ | ✖ | ✔ | ✔ |
| Type safety (class C) | ✔ | ✔ | ✔ | ~ | ✖ | ✖ | ✔ |
| Lifetime (class D) | ~ | ✔ | ~ | ✖ | ✖ | ~ | ✔ |
| Mutation tracking | ✔ | ✔ | ✔ | ✖ | ✖ | ✔ | ✔ |
| Interface consistency (class F) | ~ | ✔ | ✔ | ✖ | ✖ | ✖ | ~ |
| Implementation divergence (class G) | ~ | ✔ | ~ | ✖ | ✖ | ✖ | ~ |
| Value/reference semantics preserved | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| Non-copyable keys/values | n/a — impossible on this interface (§10) | | | | | | |
| Source compatibility | ✖ 1/628 | **✖ 1/628** | ✖ ~7/628 | ✖ | ✔ | ✔ | ✖ |
| Virtual ABI compatibility | ✖ silent | **✖ silent** | ✖ **loud** (slots removed) | ✖ silent | ✔ | ✔ | ✖ silent |
| Object layout | ~ LD 40→72 | **~ LD 40→72** | ✔ | ~ | ✔ | ~ LD 40→56 | ✖ |
| Allocation | 0–2/read | **0–2/read** | 1/read | 0 | 0 | 0 | ≥1 |
| Module dependencies added | none | **none** | none | **new type** | none | none | **new subsystem** |
| Migration burden | 1 line | **1 line + 2 tests** | every call site | every call site | none | none | every call site |
| .NET parity | ✔ | **✔✔** | ✖ | ✖ | ✖ | ~ | ✔ |
| Testability | ✔ | ✔ | ✔ | ~ | ✖ | ~ | ✔ |

`<any>` is already included by `DictionaryEntry.hpp`, which
`IDictionaryEnumerator.hpp` already includes, so A/B add **no** module
dependency; the graph stays at 41 modules / 90 edges.

### 11.4 Why A+B and not A alone

A alone changes the return type and leaves each implementation free to build the
box however it likes — including from a dangling iterator, which is what
`ListDictionaryInternal` does today for all four accessors. Pinning `Key` and
`Value` to `Entry`'s members (B) does three things at once that A cannot: it
makes the three accessors provably consistent (class F/G), it forces the
snapshot into existence because `Entry` must be materialised at `MoveNext` for
`Key` to be cheap, and it matches how .NET's own `HashtableEnumerator` is
written. The two are therefore adopted together and described as one design.

---

## 12. Measured repository-wide source break

### 12.1 Call sites — `sweep_callsites.log`

628 translation units, 4 parallel jobs, 0 compile failures.

| Tag | Unique sites |
|---|---:|
| `SR1795-ENTRY` | 3 |
| `SR1795-KEY` | 5 |
| `SR1795-VALUE` | 3 |
| **total unique** | **10** |

Every site:

```
ENTRY  modules/collections/tests/System/Collections/CollectionsNewTests.cpp:104
ENTRY  modules/collections/tests/System/Collections/CollectionsNewTests.cpp:117
ENTRY  modules/collections/tests/System/Collections/EnumeratorCurrentSafetyTests.cpp:191
KEY    modules/collections/include/System/Collections/Hashtable.hpp:499              <= library
KEY    modules/collections/include/System/Collections/ListDictionaryInternal.hpp:136 <= library
KEY    modules/collections/tests/System/Collections/EnumeratorCurrentSafetyTests.cpp:192
KEY    modules/collections/tests/System/Collections/ListDictionaryInternalTests.cpp:96
KEY    modules/collections/tests/System/Collections/ListDictionaryInternalTests.cpp:107
VALUE  modules/collections/include/System/Collections/Hashtable.hpp:500              <= library
VALUE  modules/collections/include/System/Collections/ListDictionaryInternal.hpp:136 <= library
```

3 library-internal sites, 7 test sites, **0 in any other module**. The
`[[deprecated]]` attribute is not reported on an *override declaration*, only on
a *use*, so this measures true call sites; the two override declarations per
implementation are inventoried by hand in §5.

### 12.2 Compile break — `sweep_breaks_any.log`

Against the fully migrated `shim-any/` (interface + both implementation headers,
generated by `make_shim_any.py` and compiled standalone `-Werror` clean):

```
translation units that STOP COMPILING: 1 / 628
  modules/collections/tests/System/Collections/ListDictionaryInternalTests.cpp: 1 error
distinct error sites: 1
  gtest.h:1472: no match for 'operator!=' (operand types are 'const std::any' and 'std::nullptr_t')
```

The single site is `ListDictionaryInternalTests.cpp:96`,
`EXPECT_NE(e->getKeyProperty(), nullptr);`, whose migration is
`EXPECT_NE(std::any_cast<const void*>(e->getKeyProperty()), nullptr);`.

`ListDictionaryInternalTests.cpp:107` —
`EXPECT_THROW(e->getKeyProperty(), System::InvalidOperationException)` — and
`EnumeratorCurrentSafetyTests.cpp:191–193` compile unchanged.

### 12.3 Runtime break — `shim_suite.log`

`rebuild_against_shim.py` recompiles all **66** objects of
`SharpRuntimeTests_Collections_Core` with `shim-any/` first on the include path
(4 parallel jobs, the repository `build/` tree never written to), overlays the
one-line migration above, and relinks against the target's own link line:

```
2252 tests from 164 test suites ran.
[  PASSED  ] 2250 tests.
[  FAILED  ] 2 tests
  EnumeratorCurrentSafety.ListDictionaryInternalPreservesTheConstOnItsBoxedKey
  ListDictionaryInternalTest.Values_ReflectsContents
```

Both are the two `ListDictionaryInternal` parity defects §16 decides, and both
fail with the exact diagnostic the design predicts:

- `boxedKey.type()` is `System::Collections::DictionaryEntry` where the test
  expects `void const*` — because `Current` now *is* `Entry`, as .NET says;
- `std::any_cast<const void*>` on the value view throws `bad any_cast` — because
  the value view now boxes `void*`, agreeing with `DictionaryEntry::Value` and
  with `MemberCollection::copyToCore`, where it previously agreed with neither.

Neither is a regression to repair; both are assertions to update, and §26 lists
the exact edits.

### 12.4 The measurement's limits, stated

- `test/consumer/*.cpp` is **not** in `build/compile_commands.json` (0 of 628
  entries): those fixtures are configured on demand by
  `test/consumer/InjectFixture.cmake`. Checked by hand: only
  `collections_copyto.cpp` and `collections_linked_list.cpp` mention
  `IDictionaryEnumerator` at all, and neither calls `Key` or `Value`. A new
  fixture is part of the implementation scope (§28).
- Uninstantiated templates are invisible to both sweeps. Neither accessor is
  used from a template in this repository.
- **This is a measurement of this repository only.** It says nothing about CNA
  or mobile-eggbert, which were not inspected, searched, built, or modified
  (§30 risk 3).

---

## 13. Selected architecture

**Entry-canonical owning accessors, with a mandatory `MoveNext`-time snapshot.**

1. `getEntryProperty()` keeps returning `DictionaryEntry` **by value** and
   becomes the *canonical* representation of the current position. It is .NET's
   unboxed form and this interface's existing owning accessor.
2. `getKeyProperty()` and `getValueProperty()` return `std::any` **by value**,
   equal by construction to `Entry.Key` and `Entry.Value`. The obvious
   implementation is literally `return getEntryProperty().getKeyProperty();`;
   the recommended one reads the snapshot member directly, avoiding a
   `DictionaryEntry` copy per call.
3. `getCurrentProperty()` keeps the #1793 signature (`std::any` by value) and
   boxes the `DictionaryEntry` on **both** implementations, matching .NET's
   `Current => Entry`.
4. **Every implementation snapshots the entry into enumerator-owned storage in
   `MoveNext()`**, and no accessor dereferences container storage.
   `Hashtable::Enumerator` already does this and needs no new member;
   `ListDictionaryInternal::NodeEnumerator` gains a `DictionaryEntry current_`.
5. No borrowed address remains anywhere on this interface.

Rule 4 is the load-bearing one and is stated as an **invariant of the
interface**, not an implementation detail: *after `MoveNext()` returns `true`,
every accessor must be answerable from state the enumerator owns.* A future
`IDictionaryEnumerator` implementation that reads its container inside an
accessor is wrong even if its signatures are right.

---

## 14. Exact proposed public declarations

`modules/collections/include/System/Collections/IDictionaryEnumerator.hpp`:

```cpp
#include <any>
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/DictionaryEntry.hpp"

namespace System::Collections {

/**
 * @brief Enumerates the elements of a non-generic dictionary.
 *
 * C++ counterpart of .NET System.Collections.IDictionaryEnumerator.
 *
 * @par Ownership and lifetime
 * All four accessors return OWNING values. Nothing they return aliases the
 * dictionary, and nothing done to what they return can reach it. After
 * MoveNext() returns true, an implementation must be able to answer every
 * accessor from state the enumerator itself owns; reading container storage
 * inside an accessor is a defect even when the signature is right, because no
 * accessor performs the fail-fast version check that MoveNext() and Reset() do.
 */
class IDictionaryEnumerator : public IEnumerator {
public:
    virtual ~IDictionaryEnumerator() = default;

    /**
     * @brief Gets both the key and the value of the current dictionary entry.
     *
     * C++ counterpart of .NET IDictionaryEnumerator.Entry, the unboxed form of
     * Current. This is the canonical representation: getKeyProperty() and
     * getValueProperty() return exactly this entry's Key and Value, and
     * getCurrentProperty() returns exactly this entry, boxed.
     *
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished
     *         ("Enumeration has either not started or has already finished.").
     */
    [[nodiscard]] virtual DictionaryEntry getEntryProperty() const = 0;

    /**
     * @brief Gets the key of the current dictionary entry, as an owning box.
     *
     * C++ counterpart of .NET IDictionaryEnumerator.Key, which returns `object`.
     * Equal to getEntryProperty().getKeyProperty(). The returned std::any OWNS
     * its value: it is unaffected by any later MoveNext(), Reset(), mutation of
     * the dictionary, destruction of the enumerator, or destruction of the
     * dictionary, and writing to it cannot reach the dictionary.
     *
     * Recover the value with std::any_cast<T>(); query std::any::type() when the
     * boxed type is not known statically. The boxed type is implementation
     * defined and DIFFERS between implementations -- std::string for Hashtable,
     * const void* for ListDictionaryInternal -- because the two dictionaries
     * have genuinely different key domains. A wrong std::any_cast throws
     * std::bad_any_cast instead of silently reinterpreting bytes, which the
     * previous `const void*` return did.
     *
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished.
     */
    [[nodiscard]] virtual std::any getKeyProperty() const = 0;

    /**
     * @brief Gets the value of the current dictionary entry, as an owning box.
     *
     * C++ counterpart of .NET IDictionaryEnumerator.Value, which returns
     * `object?`. Equal to getEntryProperty().getValueProperty(); an absent value
     * is an empty std::any, matching .NET's null. Same ownership, lifetime, and
     * casting rules as getKeyProperty(). If the element type has shared
     * reference semantics of its own, the box copies the HANDLE, exactly as .NET
     * returns the reference for a reference type.
     *
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished.
     */
    [[nodiscard]] virtual std::any getValueProperty() const = 0;
};

} // namespace System::Collections
```

### 14.1 `Hashtable::Enumerator` — two bodies

```cpp
[[nodiscard]] std::any getKeyProperty() const override {
    ensureCurrent();
    return current_.getKeyProperty();
}

[[nodiscard]] std::any getValueProperty() const override {
    ensureCurrent();
    return current_.getValueProperty();
}
```

`current_` is the existing `DictionaryEntry` filled by `MoveNext()`; no new
member, no layout change, and `&it_->first`/`&it_->second` — the last two
container dereferences inside an accessor on this class — are deleted.

### 14.2 `ListDictionaryInternal::NodeEnumerator` — one member, one `MoveNext` line, four bodies

```cpp
// new member, beside started_/valid_:
DictionaryEntry current_;

bool MoveNext() override {
    …
    valid_ = (it_ != d_->list_.end());
    if (valid_) current_ = DictionaryEntry(it_->key, it_->value);   // new
    return valid_;
}

[[nodiscard]] std::any getCurrentProperty() const override {
    requireValid();
    return std::any(current_);              // .NET: Current => Entry
}
[[nodiscard]] DictionaryEntry getEntryProperty() const override {
    requireValid();
    return current_;
}
[[nodiscard]] std::any getKeyProperty() const override {
    requireValid();
    return current_.getKeyProperty();
}
[[nodiscard]] std::any getValueProperty() const override {
    requireValid();
    return current_.getValueProperty();
}
```

### 14.3 The two member views — one body each, both simplify

```cpp
// Hashtable::MemberCollection::MemberEnumerator
[[nodiscard]] std::any getCurrentProperty() const override {
    return keys_ ? inner_->getKeyProperty() : inner_->getValueProperty();
}

// ListDictionaryInternal::MemberCollection::Enumerator
[[nodiscard]] std::any getCurrentProperty() const override {
    return keys_ ? inner_->getKeyProperty() : inner_->getValueProperty();
}
```

The `Hashtable` view's element shapes are **unchanged**: `std::any(std::string)`
for keys, the stored value itself for values. The `ListDictionaryInternal` key
view is **unchanged** at `std::any(const void*)`; its **value** view changes from
`const void*` to `void*` (§16).

### 14.4 Compile-validated, not sketched

Every body above is the exact text `make_shim_any.py` generates. Both migrated
implementation headers compile standalone under
`-std=c++23 -Wall -Wextra -Wpedantic -Werror`, and `shim_selftest.log` records
the runtime shapes:

```
ht-key-type=…basic_string…      ht-value-type=i    ht-current-type=…DictionaryEntry
ld-key-type=PKv                 ld-value-type=Pv   ld-current-type=…DictionaryEntry
ld-key-equals-original=1        ld-value-equals-original=1
```

---

## 15. `Entry` / `Current` / `Key` / `Value` relationship

| Accessor | Returns | Relationship | Cost |
|---|---|---|---|
| `getEntryProperty()` | `DictionaryEntry` by value | **canonical** — a copy of the snapshot | 1 `DictionaryEntry` copy |
| `getKeyProperty()` | `std::any` by value | `== Entry.Key`, by construction | 1 `std::any` copy |
| `getValueProperty()` | `std::any` by value | `== Entry.Value`, by construction | 1 `std::any` copy |
| `getCurrentProperty()` | `std::any` by value | `== std::any(Entry)` | 1 `DictionaryEntry` copy + 1 box |

Invariants an implementation must satisfy, and the permanent suite must pin:

```
any_cast<DictionaryEntry>(getCurrentProperty()).getKeyProperty()   == getKeyProperty()
any_cast<DictionaryEntry>(getCurrentProperty()).getValueProperty() == getValueProperty()
getEntryProperty().getKeyProperty()                                == getKeyProperty()
getEntryProperty().getValueProperty()                              == getValueProperty()
```

(compared by boxed type and by the recovered value, since `std::any` has no
`operator==` — the same reason `Generic::List<std::any>` cannot be instantiated,
recorded by #1793 §34.3 correction 2.)

---

## 16. The two `ListDictionaryInternal` parity defects, decided

Both are **newly discovered under this ticket**, both are recorded here rather
than in a new `SR-AUD-*` identifier (the audit numbering is frozen at 364), and
both change observable behaviour, so both are named explicitly in the approval.

**16.1 `Current` must be `Entry`.** .NET:
`public object Current => Entry;`. The port:
`return std::any(getKeyProperty());`. The two implementations of one interface
therefore disagree about what `Current` means, which is class G on the accessor
#1793 has already migrated. **Decision: fix it.** `Current` boxes the
`DictionaryEntry` on both implementations.

This does **not** reopen #1793 and does not change
`IEnumerator::getCurrentProperty()`'s signature or its contract: the return is
still an owning `std::any` by value with the same ownership and lifetime rules.
Only one implementation's *payload* becomes .NET-correct. `EnumeratorCurrentSafetyTests`
`ListDictionaryInternalPreservesTheConstOnItsBoxedKey` is updated, not deleted:
its real subject — that the `const` on the caller's key survives boxing and no
non-`const` view exists — is still assertable, on `getKeyProperty()` and on the
key **view**, which both still box `const void*`.

**16.2 The value view's `const` must match `DictionaryEntry`.** Today
`DictionaryEntry::Value` holds `void*`, the value view's `Current` boxes
`const void*`, and `copyToCore` writes `void*`. Under §13 rule 2, `Value` is
`Entry.Value`, so the view boxes `void*` and all three agree. **Decision: take
the `void*` spelling**, because that is what the dictionary genuinely stores
(`Add(const void* key, void* value)`, `getItem()` returns `void*`) and narrowing
it to `const void*` would discard information .NET's `object?` does not discard.
`ListDictionaryInternalTest.Values_ReflectsContents` is updated from
`std::any_cast<const void*>` to `std::any_cast<void*>`.

Both are separable: if only 16.1 is declined, the design still holds with
`ListDictionaryInternal::getCurrentProperty()` left as-is and §15's first two
invariants restricted to `Hashtable`. That is stated so the approval can be
partial rather than all-or-nothing.

---

## 17. Ownership and lifetime matrix

After the change:

| Returned value | Owner | Valid until | Can reach the dictionary | Can be reached from the dictionary |
|---|---|---|---|---|
| `getEntryProperty()` | the **caller** | the caller destroys it | no | no |
| `getKeyProperty()` | the **caller** | the caller destroys it | no | no |
| `getValueProperty()` | the **caller** | the caller destroys it | no | no |
| `getCurrentProperty()` | the **caller** | the caller destroys it | no | no |
| the enumerator object | the **caller** (`GetEnumerator()` hands over ownership) | `delete` | — | — |

Caveat stated rather than buried: when the boxed value is itself a handle
(`ListDictionaryInternal`'s `void*`, or a `std::shared_ptr<X>` value in a
`Hashtable`), the box owns the **handle**, not the pointee. Mutating the pointee
is not mutating the dictionary and moves no mutation counter — exactly as .NET
behaves for a reference type, and exactly as #1793 documented for
`getCurrentProperty()`. For `ListDictionaryInternal` the pointee is the caller's
own object, so this is not even an escape: the caller already had it.

Measured post-fix (`probe8_asan.log`, 42 assertions, 0 failures, 0 ASan/UBSan
diagnostics, 0 LeakSanitizer leaks):

```
D.survives-MoveNext=1                      D.survives-Reset=1
D.survives-rehash=1                        D.survives-erase-of-its-own-entry=1
D.survives-Clear=1                         D.value-survives-Clear=1
D.survives-enumerator-destruction=1        D.survives-collection-destruction=1
D.accessor-after-Clear-is-safe=1           D.value-accessor-after-Clear-is-safe=1
D.entry-accessor-after-Clear-is-safe=1
D.listdict-key-after-destruction=1         D.listdict-value-after-destruction=1
D.listdict-entry-after-destruction=1       D.listdict-current-after-destruction=1
```

Every one of the eight pre-fix AddressSanitizer reports in §8.2 is gone.

---

## 18. Type and boxing rules

1. **The boxed type is implementation-defined and differs between
   implementations.** `Hashtable`: key `std::string`, value the stored value's
   own type. `ListDictionaryInternal`: key `const void*`, value `void*`. This is
   not papered over — the two dictionaries have genuinely different key domains
   (string-hashed vs address-identity) and forcing one spelling would be a lie
   about one of them. What changes is that the difference becomes
   **discoverable** (`std::any::type()`) and **diagnosed** (`std::bad_any_cast`)
   instead of silently reinterpreted (§8.3).
2. **No nesting.** `Hashtable`'s `mapped_type` *is* `std::any`, and
   `std::any(const std::any&)` selects the copy constructor, not the
   value-forwarding one, so the box holds the payload. Measured:
   `probe8` `F.no-nested-any=1`, `shim_selftest` `ht-value-type=i`. This
   reproduces #1793 §34.3 correction 3 on a second interface and must be pinned
   by a permanent test, because it is a property of the standard library's
   overload set rather than of this code.
3. **A wrong cast throws `std::bad_any_cast`** — a `std::` exception, not a
   `System::` one. Consistent with how `std::any` is already exposed by
   `ArrayList`, `DictionaryEntry`, `Hashtable`, and #1771's `CopyTo`.
4. **An absent value is an empty `std::any`** (`has_value() == false`), matching
   .NET's `object?` null. `Hashtable::Add`/`setItem` already store `std::any{}`
   for a null value pointer.
5. **Non-copyable keys and values do not exist on this interface** and cannot
   (§10). There is no `NotSupportedException` path.

---

## 19. Mutation and dictionary-integrity rules

1. No route through this interface may write to the dictionary. After the
   change, none can: measured `A.table-count-unchanged=1`,
   `A.old-key-still-present=1`, `A.spoofed-key-absent=1`,
   `B.stored-value-unchanged=1`, `AB.mutation-counter-unmoved=1`.
2. Reading through this interface never advances the mutation counter, before or
   after. `getEntryProperty()`/`getKeyProperty()`/`getValueProperty()`/
   `getCurrentProperty()` are `const` and remain so.
3. `MoveNext()` and `Reset()` keep their existing fail-fast version check and
   their `Collection was modified; enumeration operation may not execute.`
   message.
4. **The accessors deliberately do not gain a version check.** .NET's do not
   either — `HashtableEnumerator.Key` checks `_current`, not `_version` — and
   adding one would turn today's silent-stale reads into exceptions on a code
   path that .NET permits. The snapshot makes the stale read *safe*; §13 rule 4
   is what replaces the check. This is a decision, not an oversight: an
   implementation that needs the check has not snapshotted.
5. The `Hashtable` non-`const` `operator[](const std::string&)` escape, and
   `getItem()`'s `const_cast<std::any*>(&it->second)` return, are **pre-existing
   and out of scope**; both are already documented in `Hashtable.hpp` as narrow
   gaps that do not bump the counter. They are recorded in §30 risk 6 rather
   than silently absorbed.

---

## 20. State machine and exception matrix

Unchanged in every cell; the table is written down because no document currently
states it for this interface.

| State | `MoveNext()` | `Reset()` | `Entry` | `Key` | `Value` | `Current` |
|---|---|---|---|---|---|---|
| constructed, before first `MoveNext` | advances or `false` | resets | **`InvalidOperationException`** | **IOE** | **IOE** | **IOE** |
| on an element | advances | resets | value | value | value | value |
| after the last element | `false` | resets | **IOE** | **IOE** | **IOE** | **IOE** |
| after `Reset()` | advances | resets | **IOE** | **IOE** | **IOE** | **IOE** |
| collection modified since construction | **IOE** *(version)* | **IOE** *(version)* | last snapshot | last snapshot | last snapshot | last snapshot |
| collection destroyed | undefined — the enumerator borrows the collection | as `MoveNext` | last snapshot | last snapshot | last snapshot | last snapshot |
| enumerator destroyed | — | — | — | — | — | — |

Messages: the four accessors throw
`Enumeration has either not started or has already finished.`
(`SR.InvalidOperation_EnumOpCantHappen`); `MoveNext`/`Reset` throw
`Collection was modified; enumeration operation may not execute.`
(`SR.InvalidOperation_EnumFailedVersion`). **The state check runs before any copy
is made or any box is constructed**, so no exception path changes type or order
— the ordering rule #1793 §18 set, and the one whose violation #1793 §34.3
correction 1 had to repair.

Two rows deserve emphasis because they are the *only* observable behaviour
changes on this axis, and both are improvements:

- **collection modified**: today, `Key`/`Value` on both implementations, and
  `Entry`/`Current` on `ListDictionaryInternal`, are a use-after-free. After,
  they return the last snapshot.
- **collection destroyed**: `MoveNext()`/`Reset()` still read the collection's
  version and are still undefined behaviour if it is gone — this design does not
  close that, and does not claim to. The *accessors* become safe.

---

## 21. Source consequences

- **1 of 628 translation units stops compiling**, at 1 site, in a test (§12.2).
- **2 of 2,252 permanent tests stop passing**, both pinning the two
  `ListDictionaryInternal` parity defects §16 decides (§12.3).
- 3 library-internal call sites migrate with the implementations.
- 0 sites in any module other than `Collections.Core`.
- 0 hand-written implementers in this repository (§5.3); every hand-written
  implementer in consumer code must migrate, and there is no measurement of how
  many exist.
- No public class gains, loses, or renames a member; `DictionaryEntry`,
  `IDictionary`, `Hashtable`'s and `ListDictionaryInternal`'s own public APIs are
  untouched.

---

## 22. ABI consequences — measured

### 22.1 Mangled names — `abi/probe5_symbols.log`

Three candidates compiled from one source (`probe5_abi.cpp`), `const void*` /
`std::any` / `const DictionaryEntry&`:

```
BASELINE  _ZNK6System11Collections4Impl14getKeyPropertyEv
ANY       _ZNK6System11Collections4Impl14getKeyPropertyEv
ENTRYREF  _ZNK6System11Collections4Impl14getKeyPropertyEv
```

**Byte-identical, all three.** The Itanium C++ ABI does not encode a
non-template function's return type. Measured for *these* accessors rather than
inherited from #1793's measurement of a different one.

### 22.2 Vtable slot — `abi/probe5_callconv.log`, `probe7_layout_*.log`

The indirect call is `call *0x30(%rax)` in **all three** candidates: the
accessor keeps its slot and no other slot is renumbered. On the real headers,
`probe7_layout_before.log` and `probe7_layout_after.log` differ in exactly one
line (§23), and the eight-slot vtable dump is identical.

### 22.3 Calling convention — the dangerous half

```
BASELINE:  mov (%rdi),%rax          ; `this` in RDI
           call *0x30(%rax)         ; result in RAX

ANY:       mov %rdi,%rsi            ; `this` displaced to RSI
           mov %rsp,%rdi            ; hidden sret buffer in RDI
           mov (%rsi),%rax
           call *0x30(%rax)

ENTRYREF:  mov (%rdi),%rax          ; `this` in RDI, result in RAX -- as BASELINE
```

`std::any` is not trivially copyable, so it is returned through a hidden `sret`
pointer and **`this` moves from `%rdi` to `%rsi`**. The mangled name does not
change. A translation unit compiled against the old header and linked against a
library built with the new one links with **no diagnostic** and then calls the
accessor with the wrong `this`.

`ENTRYREF`'s row is recorded because it is the honest counter-example: a
reference return would *not* change the convention. It is rejected anyway,
because a reference is a borrowed address and reopens classes A, B, D and E.

### 22.4 Stale-object probe — `probe6_stale.log`, `probe6_stale_ubsan.log`

An "old" translation unit compiled against today's headers calls
`getKeyProperty()` on an enumerator manufactured by a "new" translation unit
compiled against `shim-any/`:

```
link-exit=0   linker-diagnostics=0
linked-and-started=1
Segmentation fault (core dumped)     run-exit=139
```

Under UndefinedBehaviorSanitizer:

```
Hashtable.hpp:422: runtime error: member call on address 0x… which does not point to an object of type 'Enumerator'
                   note: object has invalid vptr
Hashtable.hpp:433: runtime error: member access within address 0x… which does not point to an object of type 'Enumerator'
Hashtable.hpp:433: runtime error: load of value 114, which is not a valid value for type 'bool'
terminate called after throwing an instance of 'System::InvalidOperationException'
  what():  Enumeration has either not started or has already finished.
```

Note the last line: the corrupted call produced an **ordinary-looking
`System::InvalidOperationException`** out of garbage memory. A consumer catching
`System::Exception&` would log "enumeration not started" and carry on.

### 22.5 Layout stale-object probe — `probe6b_stale_ld.log`

A second, *independent* corruption vector this design has and #1793 did not,
because `ListDictionaryInternal::NodeEnumerator` grows (§23) and
`GetEnumerator()` is `inline`:

```
old-half-allocated-sizeof=40
linked-and-started=1
dictionary-cleared=1
new-half-expects-sizeof=72
ERROR: AddressSanitizer: heap-use-after-free
    #2 …ListDictionaryInternal::NodeEnumerator::getEntryProperty() const
link-exit=0   linker-diagnostics=0
```

The two halves define the same weak inline symbols; the linker kept **one**
arbitrarily, and which body runs is not something either side controls. Here the
*old* body won and dereferenced a freed list node; had the *new* body won it
would have read a snapshot member past the end of a 40-byte allocation. Both are
fatal, neither is diagnosed at link time, and the choice is arbitrary.

**Conclusion: a full rebuild of every consumer is mandatory, and the toolchain
will not tell anyone who skips it.**

---

## 23. Object-layout consequences

`probe7_layout_before.log` vs `probe7_layout_after.log` — one line differs:

```
sizeof-IDictionaryEnumerator=8            alignof=8       unchanged
sizeof-Hashtable=72                       alignof=8       unchanged
sizeof-Hashtable::Enumerator=72           alignof=8       unchanged
sizeof-ListDictionaryInternal=40          alignof=8       unchanged
sizeof-DictionaryEntry=32                                 unchanged
sizeof-ListDictionaryInternal::NodeEnumerator   40  ->  72          CHANGED
```

+32 bytes, one `DictionaryEntry`. Assessment:

- `NodeEnumerator` is a **private nested class**. No consumer can name it,
  allocate it, embed it, derive from it, or take its `sizeof`. In the sense the
  approvals for #1788, #1789 and #1791 Phase 2 use — a *public* object changing
  size — **this is not a public layout change**, and the approval in §33
  reflects that.
- It is nevertheless an **ABI-relevant** fact, because `GetEnumerator()` is
  defined inline in the public header and therefore its `new NodeEnumerator(…)`
  is emitted into the consumer's own object file. §22.5 measures the
  consequence. It is listed in the approval as part of the mandatory-rebuild
  item, not as a separate layout item.
- Alternative F would grow the same class 40 → 56 (two raw pointers) and carries
  the identical caveat, which is worth stating because F is otherwise the
  zero-cost option.

---

## 24. Allocation and performance

`probe4_alloc.log`, one counting `operator new`, `-O2`, an `asm volatile`
barrier per iteration (without it GCC hoists the loop-invariant call and the
benchmark measures nothing — #1786 §13.1), 200,000 iterations, single-entry
tables so the measured case is unambiguous:

| Case | allocations before | after | ns/read before | after |
|---|---:|---:|---:|---:|
| `Hashtable` key, SSO string (`"key0"`) | 0 | **1** | 1.5 | **16.3** |
| `Hashtable` key, heap string (44 chars) | 0 | **2** | — | — |
| `Hashtable` value, `int` | 0 | **0** | 1.2 | **5.2** |
| `Hashtable` value, 64-char string | 0 | **2** | — | — |
| `Hashtable` `Entry` | 1 | 1 | — | — |
| `Hashtable` `Current` | 2 | 2 | — | — |
| `ListDictionaryInternal` key | 0 | **0** | 1.5 | **6.0** |
| `ListDictionaryInternal` value | 0 | **0** | 1.2 | **6.8** |
| `ListDictionaryInternal` `Entry` | 0 | 0 | — | — |
| live allocations at exit | 0 | 0 | — | — |

Reading, honestly:

- **`ListDictionaryInternal` pays zero allocations**, because a pointer fits in
  `std::any`'s small buffer. The whole cost there is ~4.5 ns of boxing.
- **`Hashtable`'s key always allocates**, even for a short key: libstdc++'s
  `std::any` small-buffer optimisation admits only types that *fit in a `void*`*
  and are nothrow-move-constructible, and `std::string` is 32 bytes. This is the
  same correction #1793 §34 had to make to its own design's prediction, and it
  is stated up front here rather than discovered later.
- **`Hashtable`'s value costs nothing for a small value** and two allocations
  for a large one.
- The 1.5 → 16.3 ns worst case is a ~11× slowdown on an accessor that previously
  did nothing but return an address. Three things bound it: this is the
  *type-erased* path, already the slow one by construction; .NET pays the same
  (a heap box per read for a value type, plus a GC allocation); and a caller who
  needs the key in a hot loop should walk `getEntryProperty()` once instead of
  calling three accessors.
- `Entry` and `Current` are **unchanged** in both allocation count and shape,
  which matters because they are the accessors a typical `while (e->MoveNext())`
  loop actually uses.

---

## 25. Migration guidance

| Before | After |
|---|---|
| `const std::string* k = static_cast<const std::string*>(e->getKeyProperty());` | `auto k = std::any_cast<std::string>(e->getKeyProperty());` |
| `const std::any* v = static_cast<const std::any*>(e->getValueProperty());` | `std::any v = e->getValueProperty();` — already the payload, not a nested box |
| `const void* k = e->getKeyProperty();` *(ListDictionaryInternal)* | `auto k = std::any_cast<const void*>(e->getKeyProperty());` |
| `void* v = const_cast<void*>(e->getValueProperty());` | `auto v = std::any_cast<void*>(e->getValueProperty());` |
| `EXPECT_NE(e->getKeyProperty(), nullptr);` | `EXPECT_NE(std::any_cast<const void*>(e->getKeyProperty()), nullptr);` |
| any write through a `const_cast` of either result | **no longer expressible** — the box is the caller's own copy; delete the write, or use the dictionary's own mutating API |

Rules for consumers:

1. If you do not know the boxed type, query `std::any::type()`; it is a
   diagnosable question now, and was not before.
2. `Hashtable` boxes `std::string` keys; `ListDictionaryInternal` boxes
   `const void*` keys and `void*` values. Code written against `IDictionaryEnumerator`
   generically must branch on `type()`, or use `getEntryProperty()` and inspect
   its members — which was always true and was previously undiagnosable.
3. Reading all three accessors costs three copies; read `getEntryProperty()`
   once instead.
4. **Rebuild everything.** §22 is not advisory.

---

## 26. Permanent test plan

New file `modules/collections/tests/System/Collections/DictionaryEnumeratorKeyValueSafetyTests.cpp`,
parameterised over both implementations wherever the assertion is about the
*interface* — the shape #1775 used for `DictionaryKeyAndViewContractTests.cpp`,
so neither implementation can regress alone:

1. **Ownership** — writing to the returned box leaves `Count`, key lookup,
   stored value, and the mutation counter unchanged. Both implementations.
2. **Key integrity** — the §8.1 A2 scenario, 64 entries: after every legal
   operation on the box, the entry is still reachable by its original key.
3. **Type safety** — a wrong `std::any_cast` throws `std::bad_any_cast`;
   `type()` reports the documented type per implementation; `static_assert`s
   pinning the return types of all four accessors.
4. **Lifetime** — the box survives `MoveNext`, `Reset`, `Add`/rehash, `Remove`
   of its own entry, `Clear`, enumerator destruction, collection destruction.
   Both implementations, all four accessors. These are the eight ASan reports of
   §8.2, flipped.
5. **Accessor-after-mutation** — calling each accessor *again* after `Clear`
   returns the snapshot instead of reading freed storage. Both implementations.
6. **`Entry`/`Current`/`Key`/`Value` agreement** — §15's four invariants, both
   implementations.
7. **No nesting** — `Hashtable`'s `std::any`-valued element boxes the payload,
   not a `std::any`. Three cases, as #1793 pinned for `getCurrentProperty()`.
8. **State machine** — all four accessors, before-start and after-end, both
   implementations, exact message text.
9. **Member views** — the four view element shapes of §14.3, pinned so the
   `Hashtable` views cannot drift and the `ListDictionaryInternal` value view's
   new `void*` shape is deliberate.
10. **Null value** — an entry added with a null value boxes an empty `std::any`.

Existing tests to **update, not delete** (the exact edits §12.2/§12.3 measured):

- `ListDictionaryInternalTests.cpp:96` — `EXPECT_NE(std::any_cast<const void*>(…), nullptr)`;
- `ListDictionaryInternalTests.cpp:144` (`Values_ReflectsContents`) —
  `std::any_cast<void*>`;
- `EnumeratorCurrentSafetyTests.cpp:955–970`
  (`ListDictionaryInternalPreservesTheConstOnItsBoxedKey`) — assert the `const`
  survives on `getKeyProperty()` and on the **key view**, and that `Current` is
  now the `DictionaryEntry`.

Everything else in `EnumeratorCurrentSafetyTests.cpp`,
`DictionaryKeyAndViewContractTests.cpp`, `CollectionsNewTests.cpp`,
`CollectionVersionCounterTests.cpp`, `InterfacesTests.cpp`, and
`CopyToBoundaryTests.cpp` passes unchanged — measured, 2,250 of 2,252.

---

## 27. Sanitizer plan

| Sanitizer | Scope | Why |
|---|---|---|
| **ASan** | the whole new suite, plus the eight §8.2 shapes as standalone probes | the defect class it caught eight times |
| **UBSan** | the same | the `Hashtable` key write is UB; the stale-object call reports an invalid vptr |
| **LSan** | the new suite | `std::any` now allocates on the `Hashtable` path; leak detection must be proved active by a deliberate-leak self-test, as #1793 required |
| **TSan** | **not run** | no atomic, no `mutable` cache, no hidden `const` write is added or removed; the accessors stay `const` and stop touching shared container state, which strictly reduces the race surface. Stated so its absence is a decision, not an omission. |

---

## 28. Consumer-fixture plan

- A **positive** fixture `test/consumer/collections_dictionary_enumerator.cpp`,
  compiled against **only** `SharpRuntime::Collections.Core` + `Core.Base` under
  `-Wall -Wextra -Wpedantic -Werror`, walking both dictionaries through
  `IDictionaryEnumerator*`, recovering key and value with `std::any_cast`, and
  proving the dictionary is unchanged afterwards.
- A **negative** fixture
  `test/consumer/collections_dictionary_enumerator_negative.cpp` in which
  `*const_cast<std::string*>(static_cast<const std::string*>(e->getKeyProperty())) = "x";`
  and the same for the value **must fail to compile**, at marked sites, the way
  `collections_enumerator_current_negative.cpp` does for #1793.
- `scripts/check_selective_components.sh` must be run with a repository-local
  `TMPDIR` and at most four jobs, because a public header changes.

---

## 29. Fallback if the approval is declined

Land **Alternative F** (§11.2) as an explicitly-labelled *narrowing*, never as a
remediation:

- 0 source break, 0 ABI break, no approval needed beyond the ordinary
  compatible-fix rule;
- closes A, B, E, the `ListDictionaryInternal` use-after-frees, and the
  collection-destruction half of D;
- leaves **C** and **G** open, and introduces the enumerator/collection
  desynchronisation §11.2 measured, which must itself be documented as a
  deliberate trade;
- the header must then say plainly that the type-safety and
  cross-implementation-divergence classes remain open by decision, and #1794
  closes `wontfix` with that reason attached, as #1772 did.

Do **not** describe Alternative F as closing this ticket's finding. It does not.

---

## 30. Risks and residual limitations

| # | Risk | Severity | Position |
|---|---|---|---|
| 1 | Silent ABI break — identical mangled name, different calling convention | **High** | Measured §22.3, reproduced end-to-end §22.4. No candidate except E and F avoids it. Only mitigation: mandatory full rebuild, §33 item 3. |
| 2 | Second, independent stale-object vector from `NodeEnumerator` 40→72 with an `inline` `GetEnumerator()` | **High** | Measured §22.5. New relative to #1793. Folded into the same mandatory-rebuild item. |
| 3 | CNA and mobile-eggbert are unmeasured | **Medium** | Out of scope by instruction; not inspected, searched, built, or modified. The 10-site figure is this repository only. #1773 stays blocked. |
| 4 | `MoveNext()`/`Reset()` after the collection is destroyed remain undefined | **Medium** | Not closed and not claimed (§20). The enumerator borrows the collection; that is the port-wide convention. A separate finding if it is ever to be closed. |
| 5 | Per-read allocation on the `Hashtable` key path | Low–Medium | Measured §24: 1 for an SSO key, 2 for a heap key, 0 for `ListDictionaryInternal`. `Entry`/`Current` unchanged. |
| 6 | Two pre-existing `Hashtable` write escapes stay open | **Medium** | `operator[](const std::string&)` returns a non-`const` `std::any&` and `getItem()` returns `const_cast<std::any*>(&it->second)` — both bypass the mutation counter, both already documented in the header, **both outside this interface and out of scope**. Recorded here so they are not mistaken for closed. |
| 7 | The boxed key/value type still differs between implementations | Low | Inherent (§18 rule 1). Now discoverable and diagnosed rather than silent. |
| 8 | `std::bad_any_cast` is a `std::` exception, not a `System::` one | Low | Consistent with the rest of this port's `std::any` surface. |
| 9 | Consumer fixtures are invisible to both sweeps | Low | `test/consumer/` is not in `compile_commands.json`; checked by hand (§12.4). |
| 10 | .NET's `HashtableEnumerator.Key` uses a different message than its siblings | Low | Deliberately **not** copied (§9.4). Documented divergence. |
| 11 | Probes depend on `-fno-access-control` | Low | Probes only. The permanent suite asserts through the public API; `probe8` needs the flag only to read the private counter. |

---

## 31. Rejected approaches

- **C — remove `Key`/`Value`.** Rejected: .NET has all three; removing two pure
  virtuals is a *louder* ABI break (slots disappear) for less parity, and every
  caller that wants only the key would have to materialise both.
- **D — a bespoke descriptor type.** Rejected: new public type, new
  infrastructure, and it still returns an address, so class D stays open. It
  delivers less than `std::any`, which is already in this component's public
  surface.
- **E — document the `const void*`.** Rejected on measurement: §8.1 B defeats it
  with one `const_cast`, and §8.2's last three reports need no caller misuse at
  all. It must never be recorded as a remediation.
- **F — enumerator-owned copies behind `const void*`.** Rejected as the
  *selected* design (leaves C and G open, introduces a desynchronisation),
  retained as the §29 fallback. Its zero-break profile is measured, not assumed.
- **G — a repository boxing abstraction.** Rejected: no such type exists,
  `System::Object` is a permanent deviation, and inventing one for two accessors
  is architecture, not remediation.
- **Adding a version check to the accessors.** Rejected: .NET does not, and it
  would convert permitted stale reads into exceptions (§19 rule 4).
- **Adopting .NET's `EnumNotStarted` message on `Key` alone.** Rejected: the
  port is already self-consistent and matches .NET's other implementation
  (§9.4).

---

## 32. Implementation phases

Both are #1794's; neither may begin before the approval in §33.

**Phase 1 — documentation only, no approval needed, no behaviour change.**
Write §17's ownership/lifetime rules, §20's state machine, and §13 rule 4's
invariant into `IDictionaryEnumerator.hpp`, replacing the current `@warning`.
Correct the `Key` doc-comment, which today documents an
`InvalidOperationException` contract on `Key` and says nothing at all about
lifetime, ownership, or what the `const void*` points at, and gives `Value` no
`@throws` clause at all. **Phase 1 does not close the defect.**

**Phase 2 — the change, BLOCKED on §33.**
1. `IDictionaryEnumerator.hpp`: the two return types.
2. `Hashtable::Enumerator`: two bodies (§14.1).
3. `ListDictionaryInternal::NodeEnumerator`: one member, one `MoveNext` line,
   four bodies (§14.2).
4. Both member views: one body each (§14.3).
5. Three existing test assertions updated (§26).
6. The new permanent suite (§26), the sanitizer campaign (§27), the two consumer
   fixtures (§28).
7. `README.md` breaking-changes entry stating the mandatory full rebuild and the
   two behaviour changes of §16.
8. Re-measure `sizeof`/`alignof` against §23's table; re-run the stale-object
   probes; re-run `scripts/check_selective_components.sh` with a repository-local
   `TMPDIR` and at most four jobs.

If §33 item 4 alone is declined, drop steps 3's `getCurrentProperty()` line and
§16.2's view change, and restrict §15's invariants to `Hashtable`. Everything
else stands.

---

## 33. Exact user approval required

Ticket #1794 needs this approval, written out so it can be granted or refused
item by item. **#1793's approval does not carry over, and neither do #1771's,
#1780's or #1783's.**

1. **A public source break to `System::Collections::IDictionaryEnumerator`.**
   `getKeyProperty()` and `getValueProperty()` change their return type from
   `const void*` to `std::any`. Every `static_cast<T*>` on the result stops
   compiling, every write through the result becomes inexpressible, and every
   hand-written implementer — including any in consumer code — must migrate.
   Measured in this repository: **1 of 628 translation units, at 1 site**.

2. **Two observable behaviour changes on `ListDictionaryInternal`**, both .NET
   parity corrections (§16):
   (a) `getCurrentProperty()` boxes the `DictionaryEntry` instead of the key,
   matching .NET's `public object Current => Entry;`
   (b) the value view's element changes from `const void*` to `void*`, agreeing
   with `DictionaryEntry::Value` and `copyToCore`, where today it agrees with
   neither.
   Measured: **2 of 2,252 permanent tests change**, and those two tests are the
   only places either behaviour is pinned. **This does not reopen #1793**: the
   `getCurrentProperty()` signature and contract are unchanged; one
   implementation's payload becomes correct.

3. **Acknowledgement of a silent ABI break requiring a full consumer rebuild**,
   through **two independent mechanisms**:
   (a) the mangled name is byte-identical before and after while the calling
   convention is not — `this` moves from `%rdi` to `%rsi` — so a partially
   rebuilt consumer links with **zero** diagnostics and then corrupts memory
   (§22.3, reproduced §22.4: SEGV, UBSan "invalid vptr", and a bogus
   `System::InvalidOperationException` raised out of garbage);
   (b) `ListDictionaryInternal::NodeEnumerator` grows 40 → 72 bytes while
   `GetEnumerator()` stays `inline` in the public header, so a stale consumer's
   own object file allocates the old size for the new object (§22.5, reproduced:
   links clean, then ASan `heap-use-after-free`).
   **`NodeEnumerator` is a private nested class**, so this is *not* a public
   object-layout change in the sense of #1788's, #1789's, or #1791 Phase 2's
   approvals; it is part of this rebuild item.

4. **Optionally separable:** item 2 may be declined on its own (§32), leaving
   items 1 and 3 to stand. Items 1 and 3 cannot be separated from each other.

Phase 1 (§32) needs **no** approval and may be scheduled independently.

---

## 34. Relationship to ticket #1791, and implementation order

**#1791 is not implemented here, and nothing in this design depends on it.**

#1791 (`P2: Implement tracked List indexer mutation`, blocked) would give
`Generic::List<T>::operator[]` a proxy so an index assignment bumps the mutation
counter. The two tickets look related — both are about a mutation escaping a
collection's counter — and are structurally different:

| | #1794 | #1791 Phase 2 |
|---|---|---|
| Interface | non-generic `IDictionaryEnumerator` | generic `List<T>` indexer |
| Escape | a read accessor whose result can be `const_cast` | an ordinary assignment through a returned reference |
| Fix | return an owning value | return a proxy object |
| Public layout | none (`NodeEnumerator` is private) | **yes** — `ObjectModel::Collection<T>` 32 → 40 |
| Break | loud at every call site that casts | **silent**: `list[i]` keeps compiling and changes meaning |
| Shared code | none | none |

They share no header, no type, and no test. **Recommended order: #1794 before
#1791**, for the same reason #1793 was ordered before #1791 — #1794's break is
loud at the call site and its layout change is invisible to consumers, whereas
#1791 Phase 2 grows a public object and silently changes what `list[i]` means,
so it deserves to land against an otherwise-settled tree.

The migrations must **not** be merged. Merging them would produce one approval
covering a public source break, a public layout break, a silent ABI break and a
silent semantic change, which is exactly the "approval broader than its
evidence" failure #1793 avoided by not folding this ticket into itself.

#1788 and #1789 (the `LinkedList<T>` and `BitArray` 32-bit counters) are
unrelated and stay blocked. #1773 stays blocked. #1785, #1790, #1792, #1793 stay
done.

---

## 35. Validation performed under this ticket

No production or test source changed, so the repository gate is expected to be
identical to the tree this branch started from — and is.

| Check | Result |
|---|---|
| `python3 scripts/validate_module_boundaries.py --root .` | **OK** — 41 physical modules, 90 dependency edges |
| `python3 test/validate_module_boundaries_test.py` | **7/7 OK** |
| `python3 scripts/generate_component_catalog.py --check` | **OK** — catalogue current |
| `python3 scripts/db_consistency_check.py --db plan.sqlite3` | **OK** — no consistency problems |
| `git diff --check` | **clean** |
| `scripts/check_doxygen_warnings.sh` | **1,939** warnings, ceiling 1,942 — unchanged; `docs/` is outside `Doxyfile`'s `INPUT` and no `README.md` link was added |
| `scripts/local_ci_check.sh build` | **13,538 tests passed across 37 executables**, zero warnings/errors; `SharpRuntimeTests_Collections_Core` 2,252 |

`scripts/check_selective_components.sh` was **not** run: no public header and no
component metadata changed under this ticket. It is required by §28 when #1794
Phase 2 lands.

### 35.1 Build directories and parallelism

| Directory | Purpose | Max parallel jobs |
|---|---|---|
| `build/` | the existing repository build, reused incrementally by `scripts/local_ci_check.sh` | **4** (`cmake --build … --parallel 4`, the script's own hard-coded value) |
| `build-probe-idictenum/` | every probe, shim, sweep and rebuilt object of this ticket | **1** for each probe (a single `g++` process); **4** for `sweep_callsites.py`, `sweep_breaks.py` and `rebuild_against_shim.py`, each with `MAX_JOBS = 4` written into the source |
| `build-tmp/` | `local_ci_check` log capture | n/a |

No `nproc`, no `$(nproc)`, no `hardware_concurrency()`, no bare `-j`, no bare
`--parallel`, no unbounded `MAKEFLAGS` or `CMAKE_BUILD_PARALLEL_LEVEL` anywhere
in this ticket's scripts. **No compilation exceeded four jobs.** No build tree
was created under `/tmp`, `/var/tmp`, `/dev/shm`, or the per-session scratchpad.

`rebuild_against_shim.py` is the one script needing special handling: it
recompiles the 66 objects of `SharpRuntimeTests_Collections_Core` against the
migrated shim. It writes only into `build-probe-idictenum/shim-objects/`, never
into `build/`, and caps itself at four concurrent compiler processes.

---

## 36. Ticket status at completion

- **#1795** (this design) — `done`.
- **#1794** — stays **blocked**, now depending on this record, with its
  acceptance criteria and its exact approval text (§33) updated from it.
- **#1791**, **#1788**, **#1789**, **#1773** — unchanged, still `blocked`.
- **#1785**, **#1790**, **#1792**, **#1793** — unchanged, still `done`.
- **SR-AUD-356** stays `remediated`; **CCF-018** is not reopened; **no new
  `SR-AUD-*` identifier** was created — the audit numbering is frozen at 364 and
  this was found during remediation.
- The defect is **not** marked remediated. This ticket is design-complete only.
