<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Non-generic `ICollection::CopyTo` destination contract

*Design record for ticket #1770 (`REMED-COLL-COPYTO-DESIGN`), audit findings
SR-AUD-358 / CCF-020. Recorded 2026-07-27 before any production change. No
production or test source was modified under this ticket.*

> **Implemented by ticket #1771 on 2026-07-27 — see [section 21](#21-implementation-closure-ticket-1771-2026-07-27).**
> Sections 1-20 below are the design as originally written and are deliberately
> left unchanged. One decision was superseded by the user's explicit approval:
> the deprecated, never-writing `CopyTo(void*, intcs)` shim of sections 9.5, 9.7,
> and 12 was **not** retained — the raw overload is removed outright, so a stale
> call site is a compile error instead of a run-time throw. Section 21 records
> that, the resulting ABI break, and the closure evidence.
>
> **Corrected by ticket #1774 on 2026-07-27 — see [section 22](#22-follow-up-correction-ticket-1774-2026-07-27).**
> Section 21.4's "known behavioural note" is left unchanged as a historical
> record, but its rule is superseded: a null-pointer destination with a
> **zero** length is a valid empty destination and is no longer rejected.
> Only a null pointer paired with a **positive** length remains rejected.

---

## 1. Executive decision

The raw destination is replaced by a **length-aware, statically typed span of
constructed boxed-object elements**, and the interface is restructured so that
**validation happens exactly once, in the base class, before any implementation
runs**:

```cpp
namespace System::Collections {

/** Canonical destination for the non-generic copy boundary. */
using ObjectSpan = System::Span<std::any>;

class ICollection : public IEnumerable {
public:
    void CopyTo(ObjectSpan destination, intcs index);            // validating, non-virtual
    void CopyTo(std::vector<std::any>& destination, intcs index); // convenience, non-virtual
protected:
    virtual void copyToCore(ObjectSpan destination, intcs index) = 0;  // the only virtual
};

}
```

The element type at the non-generic boundary is fixed to `std::any` — the
port's boxed-`object` representation, already used by `ArrayList` and
`Hashtable` storage. Concrete collections keep an additional **typed, checked**
overload over their own natural element type
(`std::vector<void*>` for `Queue`/`Stack`, `std::vector<DictionaryEntry>` for
`Hashtable`/`ListDictionaryInternal`).

`CopyTo(void*, intcs)` is **removed from the virtual interface**. It is retained
for one migration window as a non-virtual `[[deprecated]]` member that throws
`System::NotSupportedException` and **never writes**.

This eliminates the finding rather than hiding it, because after the change the
destination carries its own element count and its element type is fixed at
compile time. Every quantity the current boundary lacks — capacity, element
size, alignment, construction state, nullability — becomes either statically
known or explicitly validated, and the three ASan/UBSan/LSan-reproducible
failure modes in section 2 become unreachable rather than merely diagnosed.

The change is a **narrow but real public-API break** (a pure virtual member is
removed from a public interface). Under `plan.md`'s "Requires explicit user
direction" list, implementation ticket #1771 must be approved before it starts.
It is proposed and left **inactive**.

---

## 2. Audit finding

### 2.1 What the audit recorded

`audit/modules/collections/include/System/Collections/ICollection.hpp.audit.md`,
finding **SR-AUD-358** (high, `confirmed`):

> The polymorphic `CopyTo(void*, int)` interface carries neither an element type
> nor a destination length. Implementations in ArrayList, Queue, Stack,
> Hashtable, and ListDictionaryInternal trust the pointer/index and write
> directly. The direct ASan/UBSan probe performs `ArrayList::CopyTo(nullptr, 0)`;
> it reaches `std::any::operator=` through a null destination and crashes.
> Negative indexes and undersized buffers have the same unchecked native-write
> character, whereas the .NET contract diagnoses null, rank/type, index, and
> capacity.

`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`, **CCF-020**, records the shared cause:
this is *one interface-design fault*, not five independent bounds omissions, and
"a safe repair needs a typed/length-aware adapter or a deliberately constrained
API migration".

**The original audit evidence is unchanged by this ticket.** SR-AUD-358 stays
`confirmed`; only a separated design note is added (section 20).

### 2.2 Traced root causes (source-level, not from the summary)

`modules/collections/include/System/Collections/ICollection.hpp:37`

```cpp
virtual void CopyTo(void* array, intcs index) = 0;
```

| Question | Answer in the current implementation |
|---|---|
| How is the destination element type inferred? | It is **not**. Each implementation privately `static_cast`s the `void*` to a type of its own choosing. |
| Is destination capacity known? | **No.** No length parameter, no container, no sentinel. |
| Is element size known? | Only inside each implementation, from its private cast — the *caller* has no way to learn it through the interface. |
| Is alignment checked? | **No.** `static_cast<std::any*>(array)` on an under-aligned pointer is undefined behaviour before the first write. |
| Constructed or raw storage? | Implicitly assumed **constructed**: every implementation uses copy-assignment (`dest[i] = ...`). Passing raw storage is undefined behaviour and is undetectable. |
| Assignment, placement construction, byte copy, or cast? | **Cast + copy-assignment.** No placement-new, no `memcpy`. |
| Are non-trivial element types safe? | Only if the caller guessed the exact element type *and* pre-constructed it. Otherwise object lifetime is corrupted (2.3, scenario `typemix`). |
| Null / negative index / overflow / insufficient capacity detectable? | **None of them.** There is no check of any kind in any of the six implementations. |
| Is covariance / runtime element compatibility expected? | .NET expects it (`Array` carries an element `Type`). The port has no runtime array type, so the expectation is silently unmet. |
| Does it emulate `System.Collections.ICollection.CopyTo(Array, int)`? | It emulates the *signature shape* only. None of .NET's five diagnostics is reproduced. |

The decisive structural problem is that **the six implementations do not agree
on the destination element type**:

| Implementation | Cast performed | Element written | Element size |
|---|---|---|---|
| `ArrayList` | `static_cast<std::any*>` | `std::any` | 16 |
| `Queue` | `static_cast<void**>` | `void*` | 8 |
| `Stack` | `static_cast<void**>` | `void*` | 8 |
| `Hashtable` | `static_cast<DictionaryEntry*>` | `DictionaryEntry` | 32 |
| `ListDictionaryInternal` | `static_cast<DictionaryEntry*>` | `DictionaryEntry` | 32 |
| `ListDictionaryInternal::MemberCollection` | `static_cast<void**>` | `void*` | 8 |
| *(test)* `MinimalCollection` | `static_cast<int*>` | `int` | 4 |

A caller holding only an `ICollection*` therefore cannot allocate a correct
destination even in principle. Element sizes differ by 8×, and three of the
element types are non-trivial. This is why a per-collection bounds patch cannot
close the finding.

### 2.3 Direct re-verification (this ticket, probe 5)

Run against the **current, unmodified** production headers with
`-fsanitize=address,undefined` and LeakSanitizer active
(`build-probe-copyto/probe5_current_boundary.cpp`, commands in section 17):

| Scenario | Call | Result |
|---|---|---|
| `null` | `ArrayList::CopyTo(nullptr, 0)`, `Count == 1` | UBSan `member call on null pointer of type 'struct any'` at `ArrayList.hpp:127`, then `AddressSanitizer: SEGV on unknown address 0x000000000000` in `std::any::has_value()` ← `std::any::operator=`. Exit 141. |
| `small` | `ArrayList::CopyTo(dest.data(), 0)` with `Count == 8`, `dest.size() == 2` | `AddressSanitizer: heap-buffer-overflow`, `READ of size 8`, `0 bytes after 32-byte region`, frame `#4 ArrayList::CopyTo(void*, int)`. Exit 141. |
| `negative` | `Queue::CopyTo(buf.data(), -1)` | `AddressSanitizer: heap-buffer-overflow`, `WRITE of size 8`, `8 bytes before 32-byte region`, frame `#0 Queue::CopyTo(void*, int)`. Exit 141. |
| `typemix` | `Hashtable::CopyTo` reached through `ICollection*` with `std::vector<void*>` storage | **No crash.** `LeakSanitizer: detected memory leaks — Direct leak of 32 byte(s)` allocated in `std::any::_Manager_external<std::string>::_S_create` ← `Hashtable::CopyTo(void*, int)`. Exit 1. |

The fourth scenario is the most important one for the design: an element-type
mismatch through the polymorphic interface is **not** detected at all. The
implementation assigns a `DictionaryEntry` over storage that was never a
`DictionaryEntry`, so the assignment reads uninitialised `std::any` manager
pointers and the constructed boxes are never destroyed. A capacity check alone
would not have caught it.

---

## 3. Current implementation inventory

Every declaration, override, and adapter of the raw boundary in the repository
(2026-07-27, excluding `audit/`, `vendor/`, and build trees):

| # | Owning type | Source file:line | Public signature | Destination element type in practice | Capacity known? | Non-trivial values | Current exceptions | Observed tests | Compatibility risk | Recommended migration |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `System::Collections::ICollection` | `modules/collections/include/System/Collections/ICollection.hpp:37` | `virtual void CopyTo(void*, intcs) = 0` | undefined — chosen by each override | no | undefined | none | `InterfacesTests.cpp` (via `MinimalCollection`) | **high** — pure virtual removal breaks downstream overriders | NVI pair: public validating `CopyTo(ObjectSpan, intcs)` + protected `copyToCore` |
| 2 | `System::Collections::IList` | `modules/collections/include/System/Collections/IList.hpp:18` | inherits (1) | inherited | no | — | none | — | none (no own declaration) | no change |
| 3 | `System::Collections::IDictionary` | `modules/collections/include/System/Collections/IDictionary.hpp:16` | inherits (1) | inherited | no | — | none | — | none (no own declaration) | no change |
| 4 | `System::Collections::ArrayList` | `ArrayList.hpp:124-128` | `void CopyTo(void*, intcs) override` | `std::any*` (constructed) | no | yes — `std::any` copy-assign | none | none call it directly | **medium** — signature change | `copyToCore` writes `std::any`; semantics unchanged |
| 5 | `System::Collections::Queue` | `Queue.hpp:68-72` | `void CopyTo(void*, intcs) override` | `void**` | no | no | none | `QueueStackTests.cpp:121-130` | **medium** | `copyToCore` boxes each `void*`; add typed `CopyTo(std::vector<void*>&, intcs)` |
| 6 | `System::Collections::Stack` | `Stack.hpp:68-72` | `void CopyTo(void*, intcs) override` | `void**` | no | no | none | none call it directly | **medium** | as (5), top-to-bottom order preserved |
| 7 | `System::Collections::Hashtable` | `Hashtable.hpp:81-85` | `void CopyTo(void*, int) override` | `DictionaryEntry*` (constructed) | no | yes — two `std::any` members | none | `CollectionsNewTests.cpp:133-142` | **medium** | `copyToCore` boxes each `DictionaryEntry`; add typed `CopyTo(std::vector<DictionaryEntry>&, intcs)`; also normalise `int` → `intcs` (CLAUDE.md rule 7) |
| 8 | `System::Collections::ListDictionaryInternal` | `ListDictionaryInternal.hpp:151-155` | `void CopyTo(void*, intcs) override` | `DictionaryEntry*` (constructed) | no | yes | none | none call it directly | **medium** | as (7) |
| 9 | `ListDictionaryInternal::MemberCollection` (private nested; escapes as `ICollection*` from `getKeysProperty()` / `getValuesProperty()`) | `ListDictionaryInternal.hpp:117-121` | `void CopyTo(void*, intcs) override` | `void**` | no | no | none | `ListDictionaryInternalTests.cpp:117,137` construct it but call only `getCountProperty`/`GetEnumerator` | **low** — type is not nameable by consumers | `copyToCore` boxes each `void*`; no public typed overload needed |
| 10 | *(test-only)* `MinimalCollection` | `modules/collections/tests/System/Collections/InterfacesTests.cpp:21-30` | `void CopyTo(void*, int) override` | `int*` | no | no | none | `InterfacesTests.cpp:51-57` | test-owned | re-implement as `copyToCore` boxing `int` |

**Not affected** (verified, so they are not silently in scope):

- `System::Collections::Generic::ICollection<T>` declares **no** `CopyTo` at
  all; the generic collections declare their own typed, already-checked
  `CopyTo(std::vector<T>&, intcs)` / `CopyTo(T*, intcs length, intcs index)`.
- `IProducerConsumerCollection<T>::CopyTo(std::vector<T>&, intcs)` and
  `BlockingCollection<T>::CopyTo` (`BlockingCollection.hpp:355-357`) are on the
  **generic** path and are untouched.
- `BitArray::CopyTo(std::vector<bool>&)` / `CopyTo(std::vector<bytecs>&)`,
  `StringCollection::CopyTo`, `OidCollection`, `Span`/`Memory`/`Buffers`,
  `StringBuilder`, `HttpContent`, `FileInfo`, `Vector2`, `Colors` all use their
  own typed signatures and never reach `ICollection`.

---

## 4. Current caller inventory

Searched with `grep -rn "\.CopyTo(\|->CopyTo(\|::CopyTo("` across `modules/`,
`tests/`, `test/`, and `bench/`, then filtered to the non-generic boundary.
Also searched for indirect acquisition through `ICollection*`, `ICollection&`,
interface wrappers, and templates.

| Caller | File:line | How the method is reached | Destination it passes | Fate under the selected design |
|---|---|---|---|---|
| `QueueTest.CopyTo` | `modules/collections/tests/System/Collections/QueueStackTests.cpp:121-130` | concrete `Queue` | `void* buf[4]` | migrate to `std::vector<void*> buf(4); q.CopyTo(buf, 1);` |
| `HashtableTests.CopyTo_CopiesAllEntriesAsDictionaryEntry` | `modules/collections/tests/System/Collections/CollectionsNewTests.cpp:133-142` | concrete `Hashtable` | `std::vector<DictionaryEntry>::data()` | migrate to `ht.CopyTo(dest, 0)` (typed overload) |
| `ICollectionTest.CopyToFillsBuffer` | `modules/collections/tests/System/Collections/InterfacesTests.cpp:51-57` | test-local `MinimalCollection` | `int buf[5]` | migrate to `std::vector<std::any>` and `std::any_cast<int>` |
| **Production callers** | — | — | — | **none exist** |
| Indirect via `ICollection*`/`&` | `ListDictionaryInternalTests.cpp:117,137` obtain `ICollection*` from `getKeysProperty()`/`getValuesProperty()`; `ArrayList(ICollection&)`, `Queue(ICollection&)`, `Stack(ICollection&)` take the interface | none of them calls `CopyTo` — the constructors use `GetEnumerator()` | — | unaffected |
| Template/wrapper acquisition | none — searched for templates and adapters taking `ICollection`; only the three converting constructors above exist | — | — | — |
| Documentation references | `modules/core/include/System/Array.hpp:118` and `modules/core/include/System/Buffer.hpp:45` cite `ArrayList::CopyTo(void*, int)` as the precedent for "a raw pointer carries no length information" | doc-comment only | — | both doc-comments must be updated at implementation closure; the cited precedent disappears |

**Blast radius: three test call sites, one test-only override, zero production
callers.** The governing compatibility constraint is therefore downstream
source compatibility (CNA / mobile-eggbert), not in-repository churn.

---

## 5. .NET reference behaviour

Read on 2026-07-27 from the local checkout, not from memory:

- `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/ICollection.cs`
- `.../System/Collections/ArrayList.cs`, `Hashtable.cs`, `ListDictionaryInternal.cs`
- `/rv/tmp/runtime/src/libraries/System.Collections.NonGeneric/src/System/Collections/Queue.cs`, `Stack.cs`
- `.../System/Collections/Generic/List.cs` (the explicit `ICollection.CopyTo`)
- `.../System/Array.cs` (`Copy`, `CopyImpl`, `SetValue`)
- `.../src/Resources/Strings.resx` (exact message text)

### 5.1 Interface

```csharp
public interface ICollection : IEnumerable
{
    void CopyTo(Array array, int index);
    int Count { get; }
    object SyncRoot { get; }
    bool IsSynchronized { get; }
}
```

`Array` is a runtime object carrying element `Type`, `Rank`, per-dimension
lower bounds, `Length`, and element-assignment semantics. Every diagnostic below
exists because that object exists.

### 5.2 Per-collection validation, as written in the source

| Collection | Null | Rank | Negative index | Capacity | Element assignment |
|---|---|---|---|---|---|
| `Queue.CopyTo` | `ArgumentNullException.ThrowIfNull(array)` | `array.Rank != 1` → `ArgumentException(Arg_RankMultiDimNotSupported, "array")` | `ArgumentOutOfRangeException.ThrowIfNegative(index)` | `arrayLen - index < _size` → `ArgumentException(Argument_InvalidOffLen)` | `Array.Copy` (type-checked) |
| `Stack.CopyTo` | same | same | same | `array.Length - index < _size` → `ArgumentException(Argument_InvalidOffLen)` | `object[]` fast path, else `array.SetValue` per element |
| `Hashtable.CopyTo` | same | same | `ThrowIfNegative(arrayIndex)` | `array.Length - arrayIndex < Count` → `ArgumentException(Arg_ArrayPlusOffTooSmall)` | `array.SetValue(new DictionaryEntry(...), i)` |
| `ListDictionaryInternal.CopyTo` | same | `ArgumentException(Arg_RankMultiDimNotSupported)` (no paramName) | `ThrowIfNegative(index)` | `array.Length - index < Count` → `ArgumentException(ArgumentOutOfRange_IndexMustBeLessOrEqual, "index")` | `array.SetValue(new DictionaryEntry(...), index)` |
| `ArrayList.CopyTo` | **no explicit null check** — `if ((array != null) && (array.Rank != 1))`, then delegates | `ArgumentException(Arg_RankMultiDimNotSupported, "array")` | delegated | delegated | `Array.Copy(_items, 0, array, arrayIndex, _size)` |
| `List<T>` explicit `ICollection.CopyTo` | delegated | `Arg_RankMultiDimNotSupported` | delegated | delegated | `Array.Copy`, catching `ArrayTypeMismatchException` → `ArgumentException` |

This is the "behaviour of concrete collections where it intentionally differs":
`ArrayList` and `List<T>` deliberately delegate null/index/capacity checking to
`Array.Copy` instead of pre-checking, and `ListDictionaryInternal` deliberately
uses a different capacity message and paramName from its siblings.

### 5.3 What `Array.Copy`/`Array.SetValue` add (`Array.cs:437-500`, `:848`)

- `ArgumentNullException` for either array.
- `RankException(Rank_MustMatch)` when types differ and ranks differ.
- `ArgumentOutOfRangeException.ThrowIfNegative(length)`.
- Lower bounds are read with `GetLowerBound(0)` and **subtracted** from the
  supplied indices; `ArgumentOutOfRangeException.ThrowIfLessThan(index, lb)`.
- `ArgumentException(Arg_LongerThanSrcArray / Arg_LongerThanDestArray)` for a
  range past either end, using an unsigned comparison so the index arithmetic
  cannot overflow.
- `ArrayTypeMismatchException(ArrayTypeMismatch_CantAssignType)` when the
  element types are not assignment-compatible;
  `ArrayTypeMismatch_ConstrainedCopy` for `ConstrainedCopy`.
- `Array.SetValue` throws `InvalidCastException(InvalidCast_StoreArrayElement)`
  — *"Object cannot be stored in an array of this type."* — per element.
- Copying uses `Memmove` / `BulkMoveWithWriteBarrier`, so **overlap is defined**.
- Arrays of reference values copy references; arrays of value types copy the
  boxed value (`UnboxValueClass` path), so a `DictionaryEntry[]` and an
  `object[]` destination both work but through different paths.

### 5.4 Exact message strings (`Strings.resx`)

| Resource | Text |
|---|---|
| `Arg_ArrayPlusOffTooSmall` | `Destination array is not long enough to copy all the items in the collection. Check array index and length.` |
| `Argument_InvalidOffLen` | `Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.` |
| `Arg_RankMultiDimNotSupported` | `Only single dimensional arrays are supported for the requested action.` |
| `Arg_NonZeroLowerBound` | `The lower bound of target array must be zero.` |
| `ArgumentOutOfRange_NeedNonNegNum` | `Non-negative number required.` |
| `ArgumentOutOfRange_IndexMustBeLessOrEqual` | `Index was out of range. Must be non-negative and less than or equal to the size of the collection.` |
| `Arg_ArrayTypeMismatchException` | `Attempted to access an element as a type incompatible with the array.` |
| `InvalidCast_StoreArrayElement` | `Object cannot be stored in an array of this type.` |

---

## 6. Constraints of sharp-runtime's type system

Classified into the four buckets the ticket requires.

### 6.1 Representable safely

| .NET semantic | Representation here |
|---|---|
| Destination length | `Span<T>::getLengthProperty()` / `std::vector<T>::size()` |
| Null destination | `Span<T>::getPointer() == nullptr` |
| Negative / out-of-range index | plain `intcs` comparison, using the repository's unsigned-compare idiom to avoid overflow |
| Insufficient remaining capacity | `length - index < Count` |
| Element type identity | fixed at compile time — `std::any` at the interface, the collection's own type on the concrete overloads |
| Heterogeneous boxed values | `std::any` (already the storage type of `ArrayList` and `Hashtable`) |
| Non-trivial element lifetime | `std::any` / the destination element type's own copy-assignment |
| "No partial copy before a detected error" | validation precedes the loop |

### 6.2 Requires runtime metadata this port does not have

| .NET semantic | Why it cannot be reproduced |
|---|---|
| `Rank != 1` → `ArgumentException(Arg_RankMultiDimNotSupported)` | there is no runtime array object with a rank; `System::Array` (`modules/core/include/System/Array.hpp`) is a **static helper class over `std::vector<T>`**, deleted default constructor, no instances, no element `Type`, no rank, no lower bound |
| Non-zero lower bound → `Arg_NonZeroLowerBound`, `GetLowerBound(0)` subtraction | same — a `std::vector`/`Span` always starts at 0 |
| `ArrayTypeMismatchException` / `InvalidCastException` on element assignment | requires runtime assignability between element `Type`s; `System::Type` is an explicit permanent **STUB** (`Type.hpp`: RTTI-only, predicates return fixed values, documented as the intended end state under CLAUDE.md's reflection deviation) |
| Covariant destinations (`object[]` accepting a `string[]`) | requires the same runtime assignability |

### 6.3 Intentionally unsupported

Rank, lower bound, element-type mismatch, and covariance are declared
**permanently out of scope** for this boundary. They are consequences of the
already-documented reflection deviation, not new gaps. Reproducing them would
require a first-class runtime `Array` object plus a working `System::Type`,
which CLAUDE.md lists as a permanent deviation. **This design therefore has no
prerequisite ticket for a general runtime `Array` abstraction** — it deliberately
removes the need for one by fixing the element type statically.

### 6.4 Compatibility shims needed

Exactly one: the deprecated, non-writing `CopyTo(void*, intcs)` described in
section 9.5.

### 6.5 Language constraints established by probe

- **Virtual templates are ill-formed** (probe 1): `templates may not be
  'virtual'`. So `template<typename T> virtual void CopyTo(T*, intcs)` — the
  first "obvious" typed fix — is not expressible at all. Any polymorphic
  boundary must fix its element type.
- **Derived-class name hiding is real** (probe 4): a derived class that declares
  its own `CopyTo` overload hides every inherited `CopyTo`. Each concrete
  collection that adds a typed overload must write `using ICollection::CopyTo;`.
- **`Span<T>::operator[]` is bounds-checked** and throws
  `System::IndexOutOfRangeException` (`Span.hpp:97-110`), giving
  defence-in-depth *inside* `copyToCore` even if a future implementation
  miscomputes an offset.

---

## 7. Alternatives considered

### A. Typed span-style virtual boundary — **selected (with NVI)**

`virtual void CopyTo(Span<T> destination, intcs index)`. Because virtual
templates are impossible (6.5), `T` must be fixed. For the non-generic
interface, whose .NET element type is `object`, the port's boxed-object type
`std::any` is the correct fixed choice: it is already the storage type of
`ArrayList` and `Hashtable`, it accepts heterogeneous values, and it manages
non-trivial object lifetime.

*How it works for non-generic collections whose runtime element type is
`Object`:* each implementation boxes its natural element into `std::any` at the
boundary, exactly as .NET boxes into `object[]` through `Array.SetValue`.
`Queue`/`Stack` box `void*`; `Hashtable`/`ListDictionaryInternal` box
`DictionaryEntry`; `ArrayList` copies its `std::any` elements unchanged.
Retrieval is `std::any_cast<T>`, the port's existing boxing idiom.

Refined with the **non-virtual interface** pattern so validation cannot be
forgotten or diverge — which is precisely CCF-020's root cause. Precedent for
the `...Core` hook name already exists in this codebase
(`BlockingCollection<T>::tryAddCore` / `tryTakeCore`).

### B. Runtime `Array` abstraction

Introduce an object carrying element type, rank, lower bound, length, and
assignment semantics.

*Does it already exist or is it planned?* **No.** `System::Array` is a static
helper class over `std::vector<T>` with `Array() = delete` and no instance
state. `System::Type` is a permanent stub. Nothing in `plan.md`, `NEXT.md`, or
the audit backlog plans a runtime array object.

**Rejected.** It would require a working reflection layer — explicitly and
permanently out of scope — and would turn a bounded S/M repair into an
open-ended runtime redesign. It is also unnecessary: fixing the element type
statically achieves memory and type safety without any runtime metadata.

### C. Type-erased but metadata-rich destination descriptor

```cpp
struct Destination { void* data; intcs count; std::size_t elementSize;
                     std::size_t alignment; const std::type_info* type;
                     void (*assign)(void*, const void*); bool constructed; };
```

**Rejected.** It is strictly worse than A on every axis that matters:

- It restores memory safety (count is present) but **not** type safety: the
  `type` field is caller-supplied, so a wrong descriptor is exactly as
  corrupting as today's wrong cast — `typemix` in 2.3 would still pass.
- `constructed` is unverifiable; a caller that lies about it produces the same
  undefined behaviour.
- It reintroduces `void*` casts and per-element function-pointer dispatch in
  the hot path.
- It duplicates information the C++ type system already carries for free.
- It invents a public type with no .NET counterpart and no house precedent.

Its only genuine advantage — supporting a *different* element type per
collection — is better served by the concrete typed overloads in E, which the
compiler checks.

### D. Additive typed overloads plus a deprecated legacy shim

Keep `CopyTo(void*, intcs)` while routing known-safe callers through new typed
overloads.

**Can the legacy entry point ever validate capacity and element type?**
**No.** It receives one pointer and one index. It can check `array != nullptr`
and `index >= 0`; it cannot know the element count, the element size, the
element type, or whether the storage is constructed. Retaining it as a *working*
copy path therefore leaves SR-AUD-358 open — this design does not claim
otherwise.

**Partially adopted:** the shim is retained, but as a member that **never
writes**. It throws `System::NotSupportedException` with a migration message.
That is the CLAUDE.md-sanctioned pattern for an operation that cannot be
supported ("throw with a clear message — never silently fail").

Probe 7 quantifies its value: under the repository's `-Werror` policy a call to
a `[[deprecated]]` member is a **compile error**
(`error: ... is deprecated: use CopyTo(Span<std::any>, int)
[-Werror=deprecated-declarations]`), so it is a named, actionable diagnostic
rather than a silent runtime surprise; for consumers that do not use `-Werror`
it degrades to a warning plus a clear exception. Both are strictly better than
plain removal, whose only diagnostic is `no matching function for call`.

### E. Concrete-type-only CopyTo

Remove copying from the non-generic interface entirely and expose typed
operations only on concrete classes.

**Partially adopted.** The typed concrete overloads are kept (they are the
migration target for the three existing call sites and are compiler-checked).
But removing `CopyTo` from `ICollection` altogether is **rejected**: .NET's
`ICollection` *is* `{ CopyTo, Count, SyncRoot, IsSynchronized }`, and dropping
the polymorphic copy would leave `getKeysProperty()`/`getValuesProperty()`
consumers — which only ever see an `ICollection*` — with no copy path at all.
The parity loss is larger than the safety gain, which A already delivers.

### F. Immediate breaking replacement

Remove `CopyTo(void*, intcs)` outright with no shim.

**Rejected as the primary mechanism, not because it is unsafe but because it is
a worse diagnostic.** Probe 2 establishes that removal is at least *loud*: a
legacy `void*[]` call site produces
`error: no matching function for call to 'Coll::CopyTo(void* [4], int)'` and
lists both typed candidates — there is no implicit conversion from `void**`,
`std::any*`, or `DictionaryEntry*` to `Span<std::any>` or
`std::vector<std::any>&`, so a **silent misbind is impossible**. D's deprecated
shim is chosen on top of that because its message names the replacement.

It is worth recording explicitly that F was *not* rejected for being hard: the
repository has no installed package, no export configuration, and
`Collections.Core` is a header-only `INTERFACE` target, so there is no shipped
ABI to protect (section 9.7). F was rejected on diagnostic quality alone.

---

## 8. Compatibility matrix

| Criterion | A + NVI (selected) | B runtime `Array` | C descriptor | D shim only | E concrete-only | F bare removal |
|---|---|---|---|---|---|---|
| Memory safety | **full** — count always present, validated once | full | full | **none** — unchanged | full | full |
| Type safety | **full** — compile-time element type | full (runtime-checked) | **none** — caller-supplied `type_info` | none | full | full |
| Non-trivial C++ objects | safe (`std::any` / typed assign) | safe | unsafe (`constructed` unverifiable) | unsafe | safe | safe |
| .NET semantic fidelity | high; rank/lower-bound/type-mismatch dropped by documented design | highest | medium | current (low) | **low** — no polymorphic copy | high |
| Public source compatibility | **breaks** the virtual member; deprecation message names the fix | breaks | breaks | preserved | breaks harder | breaks |
| ABI implications | none in practice (header-only `INTERFACE` target) | none | none | none | none | none |
| Implementation complexity | **S/M** — 1 interface + 6 implementations + 3 test sites | XL | M | XS | M | S |
| Dependency impact | **none** — `Span`/exceptions are Core.Base, already a public dep | new reflection layer | none | none | none | none |
| Generic/non-generic interop | good — mirrors the generic `CopyTo(std::vector<T>&, intcs)` house style | good | poor | poor | poor | good |
| Testability | high — every failure class is a plain `EXPECT_THROW` | high | low | none | high | high |
| Migration burden | 3 test call sites + 1 test override, in-repo; downstream gets a named diagnostic | very high | medium | zero | high | 3 sites, generic diagnostic |
| **Eliminates SR-AUD-358?** | **yes** | yes | **no** | **no** | yes | yes |

---

## 9. Selected architecture

### 9.1 Canonical safe destination representation *(decision 1)*

`System::Collections::ObjectSpan`, a documented port-local alias for
`System::Span<std::any>`: a pointer plus an element count over **constructed**
`std::any` objects. It is not a .NET type name; it exists to keep the interface
readable, and every use site could equally spell `System::Span<std::any>`.

`std::any` is the port's boxed-`object`. Empty `std::any` represents .NET
`null`.

### 9.2 Public / internal layering *(decision 2)*

Layered, using the non-virtual interface pattern:

- **public, non-virtual** — `CopyTo(ObjectSpan, intcs)` and
  `CopyTo(std::vector<std::any>&, intcs)`. These validate, then dispatch.
- **protected, pure virtual** — `copyToCore(ObjectSpan, intcs)`. One hook per
  implementation, preconditions already met.
- **internal helper** — `System::Collections::detail::requireValidCopyDestination`,
  following the `System::Collections::detail::EnumeratorState` precedent
  introduced by ticket #1767 in `IEnumerator.hpp`.

This is the direct answer to the audit's "the public boundary needs … a checked
adapter with a single failure diagnostic before the first write" and to
CCF-020's "shared interface-design fault rather than five independent bounds
omissions": after the change there is exactly **one** place where a destination
is validated, and an implementation physically cannot skip it.

### 9.3 Capacity *(decision 3)*

Carried by the destination (`Span::getLengthProperty()` /
`std::vector::size()`), checked as `length - index < count` — the .NET
subtraction form, so `index + count` never overflows. The repository's existing
unsigned-compare idiom (`Array.hpp:519-524`) is used for the index check.

### 9.4 Element type *(decision 4)*

Fixed at compile time: `std::any` on the interface, the collection's own element
type on the concrete overloads. No runtime type check is performed **or needed**
— a mismatch is a compile error. .NET's `ArrayTypeMismatchException` /
`InvalidCastException` paths become statically unreachable.

### 9.5 Proposed signatures

`modules/collections/include/System/Collections/ICollection.hpp`:

```cpp
#pragma once
#include <any>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/Span.hpp"
#include "System/Collections/IEnumerable.hpp"

namespace System::Collections {

using SharpRuntime::intcs;

/**
 * @brief Canonical destination for the non-generic collection copy boundary:
 *        a length-aware view over constructed std::any elements.
 *
 * Not a .NET type. C++ has no runtime Array object, so the boxed-object element
 * type is fixed at compile time instead of being carried as runtime metadata.
 */
using ObjectSpan = System::Span<std::any>;

namespace detail {

/**
 * @brief Validates a copy destination once, before any element is written.
 * @throws System::ArgumentNullException       if @p data is null.
 * @throws System::ArgumentOutOfRangeException if @p index is negative.
 * @throws System::ArgumentException           if the destination cannot hold
 *         @p count elements starting at @p index.
 */
inline void requireValidCopyDestination(const void* data, intcs length,
                                        intcs index, intcs count) {
    if (data == nullptr)
        throw System::ArgumentNullException("destination");
    if (index < 0)
        throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
    if (index > length || length - index < count)
        throw System::ArgumentException(
            "Destination array is not long enough to copy all the items in the "
            "collection. Check array index and length.", "destination");
}

/** @brief Same contract for the concrete typed std::vector destinations. */
template<typename T>
inline void requireValidCopyDestination(std::vector<T>& destination,
                                        intcs index, intcs count) {
    requireValidCopyDestination(destination.data(),
                                static_cast<intcs>(destination.size()), index, count);
}

} // namespace detail

class ICollection : public IEnumerable {
public:
    virtual ~ICollection() = default;

    [[nodiscard]] virtual intcs getCountProperty() const = 0;

    /**
     * @brief Copies every element, boxed as std::any, into @p destination
     *        starting at @p index.
     *
     * C++ counterpart of .NET ICollection.CopyTo(Array, int). Elements are
     * assigned into existing, constructed storage; the destination is never
     * resized and no element is constructed in place.
     */
    void CopyTo(ObjectSpan destination, intcs index) {
        detail::requireValidCopyDestination(destination.getPointer(),
                                            destination.getLengthProperty(),
                                            index, getCountProperty());
        copyToCore(destination, index);
    }

    /** @brief std::vector convenience overload; identical contract. */
    void CopyTo(std::vector<std::any>& destination, intcs index) {
        CopyTo(ObjectSpan(destination), index);
    }

    /**
     * @brief Removed contract retained only to produce a named diagnostic.
     * @throws System::NotSupportedException always; this overload never writes.
     */
    [[deprecated("ICollection::CopyTo(void*, int) cannot validate the destination "
                 "element type or capacity (SR-AUD-358). Use "
                 "CopyTo(System::Collections::ObjectSpan, intcs) or "
                 "CopyTo(std::vector<std::any>&, intcs).")]]
    void CopyTo(void* /*array*/, intcs /*index*/) {
        throw System::NotSupportedException(
            "ICollection::CopyTo(void*, int) is not supported: a raw pointer carries "
            "no destination element type or capacity. Use CopyTo(ObjectSpan, int).");
    }

    [[nodiscard]] virtual const void* getSyncRootProperty() const { return this; }
    [[nodiscard]] virtual bool getIsSynchronizedProperty() const { return false; }

protected:
    /**
     * @brief Single implementation hook. Called only after the destination has
     *        been validated, so it must not re-validate and must not throw for
     *        argument reasons.
     */
    virtual void copyToCore(ObjectSpan destination, intcs index) = 0;
};

} // namespace System::Collections
```

`IList.hpp` and `IDictionary.hpp` need **no change**; they inherit.

### 9.6 Assignment versus construction; non-trivial objects; overlap *(decisions 5, 6, 7)*

- **Assignment.** `destination[index + i] = value;` into existing constructed
  elements. No placement-new, no `memcpy`, no byte copying, no reinterpretation.
  This matches .NET's `Array.SetValue` into pre-existing storage and the nine
  existing typed `CopyTo` implementations in this repository.
- **Non-trivial C++ objects** are handled by `std::any`'s copy semantics at the
  interface and by the destination element type's own copy-assignment on the
  concrete overloads. The `typemix` leak in 2.3 is impossible because the
  destination's element type is the same type the implementation assigns.
- **Overlap is not supported.** The destination must not alias the collection's
  internal storage. In every implementation the two have different element
  types, so aliasing requires a deliberate `const_cast` of
  `ArrayList::getItems()`. Documented as unsupported; the copy order is
  front-to-back and no overlap handling is added. (.NET's `Array.Copy` defines
  overlap through `Memmove`; that guarantee is not carried over, and this
  divergence is deliberate.)

### 9.7 Compatibility consequences *(decisions 11, 12, 13, 14)*

| Consumer shape | Effect |
|---|---|
| Calls `col.CopyTo(rawPtr, i)` | `-Werror` build: **compile error** naming the replacement. Non-`-Werror` build: deprecation warning, then `NotSupportedException` at run time. Never writes. |
| Overrides `void CopyTo(void*, intcs) override` | **compile error** — the member is no longer virtual. Intended: an override of an unsafe contract must be revisited. |
| Declares `void CopyTo(void*, intcs)` without `override` | compiles, hides the base overloads in that class, is never dispatched to by sharp-runtime. Harmless but should be migrated; the deprecation message points the way. |
| Calls `col.CopyTo(vec, i)` with `std::vector<std::any>` | works |
| Holds an `ICollection*` from `getKeysProperty()`/`getValuesProperty()` | works; allocate `std::vector<std::any>(c->getCountProperty())` — for the first time this is actually possible |

**ABI.** `Collections.Core` is registered `TYPE INTERFACE` (header-only) in
`modules/collections/CMakeLists.txt`, and the repository ships no installed
package or export configuration (`plan.md`, P2 item 5). There is no exported
binary surface whose vtable layout could break. The only ABI concern is mixing
translation units compiled against different header vintages inside one
downstream build — a full-rebuild situation, which `add_subdirectory` consumers
already get.

**Deprecation and migration strategy.**

1. #1771 lands the new boundary and migrates the three in-repo test call sites
   and the test-only override.
2. The deprecated shim ships for one migration window, carrying the message
   above.
3. A named follow-up ticket (**#1772, `REMED-COLL-COPYTO-CLEANUP`, P2, XS**)
   deletes the shim and updates the `Array.hpp:118` / `Buffer.hpp:45`
   doc-comments that cite `ArrayList::CopyTo(void*, int)` as the
   "raw pointer carries no length" precedent.

### 9.8 Non-zero lower bound and multidimensional arrays *(decision 9)*

Rejected by construction, not at run time: `ObjectSpan` is always rank 1 with
lower bound 0, so `Arg_RankMultiDimNotSupported` and `Arg_NonZeroLowerBound`
have no representable input. Documented in the header as an intentional
deviation stemming from the permanent reflection/`System::Type` deviation. No
prerequisite ticket.

### 9.9 Heterogeneous `Object` values *(decision 10)*

Each implementation boxes its natural element into one `std::any` per
destination slot, mirroring .NET's `array.SetValue(box, i)` into `object[]`:

| Collection | Boxed value | Caller retrieval |
|---|---|---|
| `ArrayList` | the stored `std::any`, copied unchanged | `std::any_cast<T>` for whatever the caller stored |
| `Queue`, `Stack` | `std::any(void*)` | `std::any_cast<void*>` |
| `Hashtable`, `ListDictionaryInternal` | `std::any(DictionaryEntry)` | `std::any_cast<DictionaryEntry>` |
| `MemberCollection` (keys) | `std::any(const_cast<void*>(key))` — normalised to `void*` so keys and values box identically | `std::any_cast<void*>` |
| `MemberCollection` (values) | `std::any(value)` | `std::any_cast<void*>` |

### 9.10 Module boundary and dependency consequences *(decision 19)*

`ICollection.hpp` gains `<any>`, `<vector>`, `System/Span.hpp`,
`System/ArgumentException.hpp`, `System/ArgumentNullException.hpp`,
`System/ArgumentOutOfRangeException.hpp`, and `System/NotSupportedException.hpp`.
All of them are owned by `modules/core` = **`Core.Base`**, which is already the
sole `PUBLIC_DEPENDENCIES` entry of `Collections.Core`
(`docs/ComponentCatalog.md`). Probe 6 compiles every affected public header
against only the `Collections.Core` + `Core.Base` include roots with
`-Wall -Wextra -Wpedantic -Werror`.

**No new dependency edge. The graph stays at 41 physical modules and 90
production edges.** No CMake metadata changes, no catalogue regeneration
required.

---

## 10. Per-collection examples

### `ArrayList`

```cpp
public:
    using ICollection::CopyTo;
protected:
    void copyToCore(ObjectSpan destination, intcs index) override {
        for (intcs i = 0; i < static_cast<intcs>(_items.size()); ++i)
            destination[index + i] = _items[static_cast<std::size_t>(i)];
    }
```

Element semantics are unchanged — `ArrayList` already stores `std::any`. Caller:

```cpp
ArrayList a;  a.Add(std::any(1));  a.Add(std::any(std::string("two")));
std::vector<std::any> dest(3);
a.CopyTo(dest, 1);
int    x = std::any_cast<int>(dest[1]);
auto&& s = std::any_cast<std::string>(dest[2]);
```

### `Queue` (and `Stack`, top-to-bottom)

```cpp
public:
    using ICollection::CopyTo;                       // required: probe 4
    void CopyTo(std::vector<void*>& destination, intcs index) {
        detail::requireValidCopyDestination(destination, index, getCountProperty());
        intcs i = index;
        for (void* p : q_) destination[static_cast<std::size_t>(i++)] = p;
    }
protected:
    void copyToCore(ObjectSpan destination, intcs index) override {
        intcs i = index;
        for (void* p : q_) destination[i++] = std::any(p);
    }
```

Migration of `QueueStackTests.cpp:121-130`:

```cpp
- void* buf[4] = {};
- q.CopyTo(buf, 1);
+ std::vector<void*> buf(4);
+ q.CopyTo(buf, 1);
```

### `Hashtable` (and `ListDictionaryInternal`)

```cpp
public:
    using ICollection::CopyTo;
    void CopyTo(std::vector<DictionaryEntry>& destination, intcs index) {
        detail::requireValidCopyDestination(destination, index, getCountProperty());
        intcs i = index;
        for (const auto& [k, v] : _map)
            destination[static_cast<std::size_t>(i++)] = DictionaryEntry(k, v);
    }
protected:
    void copyToCore(ObjectSpan destination, intcs index) override {
        intcs i = index;
        for (const auto& [k, v] : _map) destination[i++] = std::any(DictionaryEntry(k, v));
    }
```

`getCountProperty()` and `CopyTo`'s index also move from `int` to `intcs`
(CLAUDE.md rule 7) while the signature is being touched.

Migration of `CollectionsNewTests.cpp:133-142`:

```cpp
- ht.CopyTo(dest.data(), 0);
+ ht.CopyTo(dest, 0);          // dest is std::vector<DictionaryEntry>
```

### `ListDictionaryInternal::MemberCollection`

```cpp
protected:
    void copyToCore(ObjectSpan destination, intcs index) override {
        intcs i = index;
        for (const auto& n : d_->list_)
            destination[i++] = std::any(keys_ ? const_cast<void*>(n.key) : n.value);
    }
```

No public typed overload: the type is private and only escapes as
`ICollection*`. This is the first time a `getKeysProperty()` consumer can copy
safely, because `getCountProperty()` plus a fixed element type is now enough to
allocate correctly.

### Test-only `MinimalCollection` (`InterfacesTests.cpp`)

```cpp
protected:
    void copyToCore(ObjectSpan destination, intcs index) override {
        for (std::size_t i = 0; i < data_.size(); ++i)
            destination[index + static_cast<intcs>(i)] = std::any(data_[i]);
    }
```

---

## 11. Exception matrix *(decision 8)*

| Invalid argument class | .NET | Selected design | Notes |
|---|---|---|---|
| Null destination | `ArgumentNullException(nameof(array))` (`ArrayList` delegates to `Array.Copy`) | `System::ArgumentNullException("destination")` | thrown even when `Count == 0`, matching Queue/Stack/Hashtable/ListDictionaryInternal |
| Negative index | `ArgumentOutOfRangeException(nameof(index))`, `Non-negative number required.` | `System::ArgumentOutOfRangeException("index", "Non-negative number required.")` | exact .NET text |
| Index past destination end | `ArgumentException` via the capacity check | `System::ArgumentException(Arg_ArrayPlusOffTooSmall text, "destination")` | `index > length` |
| Insufficient remaining capacity | `ArgumentException(Arg_ArrayPlusOffTooSmall)` (Hashtable) / `Argument_InvalidOffLen` (Queue, Stack) | `System::ArgumentException("Destination array is not long enough to copy all the items in the collection. Check array index and length.", "destination")` | one message for all six implementations; the .NET per-collection message split is a deliberate simplification and is recorded here |
| `index + Count` overflow | avoided by `Length - index < Count` | same subtraction form; probe 3 covers `index == INT32_MAX` | no signed overflow |
| Rank ≠ 1 | `ArgumentException(Arg_RankMultiDimNotSupported)` | **not representable** — `ObjectSpan` is always rank 1 | documented intentional deviation |
| Non-zero lower bound | `ArgumentException(Arg_NonZeroLowerBound)` | **not representable** | documented intentional deviation |
| Incompatible element type | `ArrayTypeMismatchException` / `InvalidCastException` / `ArgumentException` | **compile error** | statically prevented, probe 2 |
| Unconstructed destination storage | impossible (arrays are zero-initialised) | prevented by the type system for `std::vector`/`std::any[N]`; documented precondition for a hand-built `Span` | |
| Overlapping source/destination | defined (`Memmove`) | **unsupported**, documented | deliberate deviation |
| Empty collection, `index == length` | legal | legal | probe 3 asserts it |
| Partial write before a detected error | .NET may partially copy on a late type mismatch | **never** — validation precedes the loop | probe 3 asserts it |
| Legacy `CopyTo(void*, intcs)` | n/a | `System::NotSupportedException`, always, no write | probe 3 asserts it |

---

## 12. Migration strategy

| Phase | Content | Ticket |
|---|---|---|
| 0 | This design record; SR-AUD-358 remains `confirmed`, annotated `design-complete` | **#1770 (done)** |
| 1 | Approval of the narrow public-API break (removing a pure virtual member from `ICollection`), per `plan.md`'s "Requires explicit user direction" | user gate, blocks #1771 |
| 2 | Land the NVI boundary, six `copyToCore` implementations, typed concrete overloads, `using ICollection::CopyTo;`, deprecated shim; migrate three test call sites and one test override; new regression suite; consumer fixture; sanitizer probe | **#1771 (proposed, inactive)** |
| 3 | Delete the deprecated shim; update the `Array.hpp` / `Buffer.hpp` doc-comments that cite the removed precedent | **#1772 (proposed, inactive, P2/XS)** |
| — | *Optional, explicitly out of scope:* `ArrayList::CopyTo(Array)` and `CopyTo(int, Array, int, int)` .NET parity overloads; retro-fitting `detail::requireValidCopyDestination` into the nine existing generic `CopyTo` implementations | separate P2 breadth tickets |

---

## 13. Implementation phases for #1771

1. `ICollection.hpp` — `ObjectSpan`, `detail::requireValidCopyDestination`,
   the two public overloads, `copyToCore`, the deprecated shim, doc-comments
   recording the intentional rank/lower-bound/type deviations.
2. `ArrayList.hpp`, `Queue.hpp`, `Stack.hpp` — `copyToCore`,
   `using ICollection::CopyTo;`, typed overloads for `Queue`/`Stack`.
3. `Hashtable.hpp`, `ListDictionaryInternal.hpp` (+ `MemberCollection`) —
   `copyToCore`, typed `DictionaryEntry` overloads, `int` → `intcs`.
4. Migrate `QueueStackTests.cpp`, `CollectionsNewTests.cpp`,
   `InterfacesTests.cpp`.
5. New permanent suite `CopyToBoundaryTests.cpp` (section 14).
6. `test/consumer/collections_copyto.cpp` standalone fixture (section 15.2).
7. Sanitizer probe and gates (sections 15.3, 15.4).
8. Documentation (section 16).

Each step compiles and runs `SharpRuntimeTests_Collections_Core` before the
next begins.

---

## 14. Required permanent tests *(decision 16)*

New suite
`modules/collections/tests/System/Collections/CopyToBoundaryTests.cpp`, and it
must be **parameterised over every `ICollection` implementation** — the failure
mode CCF-020 describes is per-implementation divergence, so a single-collection
test would not close it. Required cases, for each of `ArrayList`, `Queue`,
`Stack`, `Hashtable`, `ListDictionaryInternal`, and both `MemberCollection`
views reached as `ICollection*`:

1. null destination → `ArgumentNullException`, including when `Count == 0`;
2. negative index → `ArgumentOutOfRangeException`, message `Non-negative number required.`;
3. index past the destination end → `ArgumentException`;
4. destination shorter than `Count` → `ArgumentException`;
5. `index == INT32_MAX` → `ArgumentException`, no overflow;
6. exact-fit destination succeeds;
7. empty collection with `index == length` succeeds;
8. leading and trailing slots outside `[index, index + Count)` are left untouched;
9. **no partial write** before a thrown capacity error;
10. element round-trip through `std::any_cast` for every collection's boxed type;
11. ordering — `Queue` FIFO, `Stack` top-to-bottom;
12. polymorphic dispatch through an `ICollection*` produces the same result as
    the concrete call;
13. the typed concrete overloads reject the same invalid inputs with the same
    exception types;
14. inherited overloads remain reachable after each derived class adds its own
    (`using` regression — probe 4);
15. the deprecated shim throws `NotSupportedException` and writes nothing
    (compiled with the deprecation warning locally suppressed).

Existing tests that must keep passing unchanged after their call-site
migration: `QueueTest.CopyTo`,
`HashtableTests.CopyTo_CopiesAllEntriesAsDictionaryEntry`,
`ICollectionTest.CopyToFillsBuffer`, and all
`ListDictionaryInternalTest.*`. No existing test may be weakened, skipped, or
recategorised.

---

## 15. Validation plan

### 15.1 Focused

`cmake --build build --target SharpRuntimeTests_Collections_Core --parallel 4`
then `build/SharpRuntimeTests_Collections_Core --gtest_color=no`. The
1,484-case baseline may only rise.

### 15.2 Public-header standalone compile fixture *(decision 17)*

`test/consumer/collections_copyto.cpp`, built through
`test/consumer/CMakeLists.txt` in `FIXTURE_COMPILE_ONLY` mode against **only**
`SharpRuntime::Collections.Core` with `-Wall -Wextra -Wpedantic -Werror`,
following the `collections_linked_list.cpp` precedent from ticket #1769. It must
exercise: each concrete collection's typed overload; the `ObjectSpan` and
`std::vector<std::any>` overloads; polymorphic dispatch through `ICollection*`;
and a `getKeysProperty()`/`getValuesProperty()` copy. Like #1769's fixture it is
**not** added to `scripts/check_selective_components.sh`'s ten-job matrix, which
is documented as exactly ten jobs in `plan.md`, `README.md`,
`docs/CMakeComponents.md`, and the tracked CI workflow.

### 15.3 Sanitizer and negative-test strategy *(decision 18)*

`build-probe-copyto/probe5_current_boundary.cpp` is re-pointed at the new API
and must report, with `-fsanitize=address,undefined` **and LeakSanitizer
active**, that all four scenarios of section 2.3 now throw the documented
exception with **no sanitizer diagnostic and no leak** — in particular the
`typemix` scenario, which currently leaks silently, must become a compile error
(it will no longer build, which is itself the recorded result).

Additional stress: 100,000-element `ArrayList` and `Hashtable` copies into
exact-fit and one-short destinations, asserting zero diagnostics and zero leaks.

### 15.4 Gates

```
python3 scripts/validate_module_boundaries.py --root .
python3 test/validate_module_boundaries_test.py
python3 scripts/generate_component_catalog.py --check
python3 scripts/db_consistency_check.py --db plan.sqlite3
scripts/check_selective_components.sh
git diff --check
scripts/local_ci_check.sh build
scripts/check_doxygen_warnings.sh
```

The 12,743-test / 37-executable floor may only rise. Doxygen must stay at or
below the 1,942-warning ceiling. Boundary validation must stay at 41 modules and
90 edges.

---

## 16. Documentation changes required at implementation closure *(decision 20)*

- `docs/ICollectionCopyToDesign.md` — add an "Implementation closure" section,
  as `docs/LinkedListNodeLifetime.md` §9 does.
- `modules/core/include/System/Array.hpp:118` and
  `modules/core/include/System/Buffer.hpp:45` — remove the
  `ArrayList::CopyTo(void*, int)` citation; that precedent no longer exists.
- `audit/AUDIT_FINDINGS_INDEX.md` — SR-AUD-358 `confirmed` → `remediated`
  (only after #1771 closes).
- `audit/AUDIT_CROSS_CUTTING_FINDINGS.md` — CCF-020 remediation status block.
- `audit/AUDIT_FINAL_REPORT.md` — post-audit remediation status paragraph.
- `plan.md`, `NEXT.md`, `README.md` — baselines and roadmap.
- `plan.sqlite3` — tickets #1771/#1772.

---

## 17. Probes executed under this ticket

All probes live in the gitignored `build-probe-copyto/` tree (matched by the
`build*` rule in `.gitignore`), built with GCC 14.2.0, `-std=c++23`, from the
repository root. No production or test file was modified.

### Probe 1 — virtual templates are impossible

```bash
g++ -std=c++23 -fsyntax-only build-probe-copyto/probe1_virtual_template.cpp
```

```
error: templates may not be ‘virtual’
    9 |     virtual void CopyTo(T* destination, int index) = 0;
```

**Result:** the task's first candidate signature
(`template<typename T> void CopyTo(T*, int)` as a virtual) cannot exist. Any
polymorphic destination must fix its element type.

### Probe 2 — removal cannot silently misbind

```bash
g++ -std=c++23 -fsyntax-only -Imodules/core/include \
    build-probe-copyto/probe2_no_silent_misbind.cpp
```

```
error: no matching function for call to ‘Coll::CopyTo(void* [4], int)’
note: candidate: ‘void Coll::CopyTo(std::vector<std::any>&, SharpRuntime::intcs)’
note:   no known conversion for argument 1 from ‘void* [4]’ to ‘std::vector<std::any>&’
note: candidate: ‘void Coll::CopyTo(System::Span<std::any>, SharpRuntime::intcs)’
note:   no known conversion for argument 1 from ‘void* [4]’ to ‘System::Span<std::any>’
```

**Result:** exit 1. There is no implicit conversion from any raw pointer to
either typed destination, so a legacy call site fails loudly rather than binding
to the wrong overload.

### Probe 3 — full prototype of the selected architecture

```bash
g++ -std=c++23 -g -O0 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Imodules/core/include \
    -o build-probe-copyto/probe3_nvi_prototype \
    build-probe-copyto/probe3_nvi_prototype.cpp \
    modules/core/src/System/{Exception,ArgumentException,ArgumentNullException,\
ArgumentOutOfRangeException,NotSupportedException,SystemException,\
IndexOutOfRangeException}.cpp
build-probe-copyto/probe3_nvi_prototype
```

Builds clean under `-Werror`. Output (LeakSanitizer active, exit 0):

```
ok  : untouched leading slot stays empty          (x3, one per implementation shape)
ok  : boxed int round-trips
ok  : boxed std::string round-trips
ok  : boxed void* round-trips
ok  : boxed non-trivial struct round-trips
ok  : Span over std::any[N] works
ok  : concrete typed overload still reachable
ok  : inherited overload not hidden by `using`
ok  : null destination -> ArgumentNullException
ok  : negative index -> ArgumentOutOfRangeException
ok  : undersized destination -> ArgumentException
ok  : index past destination end -> ArgumentException
ok  : intcs-max index -> ArgumentException, no overflow
ok  : empty collection at index == length is legal
ok  : no partial write before the throw
ok  : legacy void* shim throws instead of writing
failures=0
```

**Result:** the selected architecture compiles, dispatches polymorphically over
three different element shapes, handles a non-trivial C++ element type, produces
the full exception matrix, performs no partial write, and is clean under ASan +
UBSan + LeakSanitizer.

### Probe 4 — derived-class name hiding

```bash
g++ -std=c++23 -fsyntax-only -Imodules/core/include \
    build-probe-copyto/probe4_name_hiding.cpp
```

```
error: cannot convert ‘std::vector<std::any>’ to ‘std::vector<void*>&’
note:   initializing argument 1 of ‘void Derived::CopyTo(std::vector<void*>&, SharpRuntime::intcs)’
```

**Result:** exit 1. Without `using ICollection::CopyTo;` the inherited overloads
are hidden. The `using` declaration is mandatory in every concrete collection
that adds a typed overload, and section 14 requires a regression for it.

### Probe 5 — current boundary is still unsafe

```bash
g++ -std=c++23 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Imodules/collections/include -Imodules/core/include \
    -o build-probe-copyto/probe5_current_boundary \
    build-probe-copyto/probe5_current_boundary.cpp \
    modules/core/src/System/{Exception,ArgumentException,ArgumentNullException,\
ArgumentOutOfRangeException,InvalidOperationException,SystemException,\
IndexOutOfRangeException,NotSupportedException}.cpp
for s in null small negative typemix; do build-probe-copyto/probe5_current_boundary $s; done
```

Results are tabulated in section 2.3. Three scenarios abort with an ASan/UBSan
diagnostic (exit 141); `typemix` completes "successfully" and is caught only by
LeakSanitizer (exit 1).

### Probe 6 — public headers compile standalone

```bash
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
    -Imodules/collections/include -Imodules/core/include \
    build-probe-copyto/probe6_public_headers.cpp
```

**Result:** exit 0. All eight affected public headers, plus polymorphic use of
all six `ICollection` implementations including both `MemberCollection` views,
compile against only the `Collections.Core` + `Core.Base` include roots. This is
the baseline the §15.2 fixture must preserve.

### Probe 7 — deprecated shim under the repository's `-Werror` policy

```bash
g++ -std=c++23 -Wall -Wextra -fsyntax-only build-probe-copyto/probe7_deprecated_werror.cpp
g++ -std=c++23 -Wall -Wextra -Werror -fsyntax-only build-probe-copyto/probe7_deprecated_werror.cpp
```

Without `-Werror` (exit 0):

```
warning: ‘void Coll::CopyTo(void*, int)’ is deprecated: use CopyTo(Span<std::any>, int)
         [-Wdeprecated-declarations]
```

With `-Werror` (exit 2):

```
error: ‘void Coll::CopyTo(void*, int)’ is deprecated: use CopyTo(Span<std::any>, int)
       [-Werror=deprecated-declarations]
```

**Result:** the shim is a compile-time, self-documenting diagnostic under the
repository's own `-Werror` policy (`cmake/SharpRuntimeCommon.cmake:20`), and a
warning-plus-clear-exception elsewhere. It does **not** silently preserve the
old behaviour, and it does not preserve source compatibility for `-Werror`
consumers — which is why section 9.7 states that explicitly rather than claiming
the shim is compatible.

---

## 18. Rejected approaches, restated

| Approach | Why rejected |
|---|---|
| `template<typename T> virtual void CopyTo(T*, intcs)` | ill-formed C++ (probe 1) |
| `void CopyTo(void*, std::size_t length, intcs index)` | length fixes memory safety but not type safety; the `typemix` corruption in 2.3 survives unchanged; still requires an unchecked cast per implementation |
| Per-collection bounds checks on the existing `void*` signature | cannot know the length; leaves six divergent implementations, which is exactly CCF-020's root cause |
| Runtime `Array` abstraction (B) | needs reflection, a permanent out-of-scope area; unbounded |
| Type-erased metadata descriptor (C) | caller-supplied type identity is unverifiable; restores memory safety only |
| Keep a *working* `void*` shim (D as a copy path) | cannot validate capacity or element type; would leave SR-AUD-358 open |
| Drop copying from `ICollection` (E in full) | breaks .NET's interface shape and orphans `getKeysProperty()` consumers |
| Bare removal with no shim (F) | safe and loud (probe 2) but a strictly worse diagnostic than the deprecated shim |

---

## 19. Open risks

1. **Approval gate.** Removing a pure virtual member from a public interface is
   a compatibility-breaking change to a public header. `plan.md` lists that
   class of change under "Requires explicit user direction". #1771 must not start
   before it is approved. *This is the only blocker.*
2. **Unknown downstream override.** No repository production code overrides or
   calls the raw boundary, but CNA / mobile-eggbert are not in this checkout. If
   a downstream override exists, it fails to compile — intentionally. The
   deprecation message names the replacement.
3. **Boxing cost.** `Queue`/`Stack`/`MemberCollection` gain one `std::any`
   construction per element on the interface path. The typed concrete overloads
   avoid it entirely, and the non-generic path is legacy compatibility surface;
   accepted deliberately.
4. **Message simplification.** One capacity message replaces .NET's
   `Arg_ArrayPlusOffTooSmall` / `Argument_InvalidOffLen` /
   `ArgumentOutOfRange_IndexMustBeLessOrEqual` split. Recorded in section 11 as
   a deliberate deviation; a later parity ticket could restore the split.
5. **Overlap divergence.** .NET's `Array.Copy` defines overlapping copies;
   this design declares overlap unsupported. No repository consumer relies on it.
6. **Shim lifetime.** If #1772 is never scheduled, the deprecated overload
   lingers. It is harmless (it never writes) but it keeps a dead public member.
7. **Nine unchecked-by-this-ticket siblings.** `Array::Copy(const T*, …)` and
   `Buffer::BlockCopy(const void*, …)` keep their documented raw-pointer
   limitation. They are separate findings and are deliberately not in scope here.

---

## 20. Decision and implementation-ticket recommendation

The design is complete, is validated by seven probes, and closes SR-AUD-358 /
CCF-020 at the interface rather than per collection. SR-AUD-358 remains
`confirmed` and is annotated **design-complete**; only implementation closure may
change its status.

**Proposed, inactive:**

| Field | Value |
|---|---|
| Ticket | **#1771** |
| Key | `REMED-COLL-COPYTO` |
| Title | `P0: Implement safe ICollection CopyTo boundary` |
| Priority | P0 |
| Size | **M** |
| Findings | SR-AUD-358 / CCF-020 |
| Scope | Sections 9, 10, 13, 14, 15, 16 of this document |
| Dependencies | ticket #1770 (this design, done); **user approval of the narrow public-API break** |
| Blocked by | risk 1 in section 19 |

**Also proposed, inactive:**

| Field | Value |
|---|---|
| Ticket | **#1772** |
| Key | `REMED-COLL-COPYTO-CLEANUP` |
| Title | `P2: Remove the deprecated raw ICollection CopyTo shim` |
| Priority | P2 |
| Size | XS |
| Scope | delete the shim; update the `Array.hpp` / `Buffer.hpp` doc-comments citing it |
| Dependencies | #1771 |

Out of scope for all three tickets: JsonNode (SR-AUD-327), XML LINQ
(SR-AUD-333), SR-AUD-090, any general array or reflection redesign, the nine
existing generic `CopyTo` implementations, and every unrelated collection API.
Tickets #1767, #1768, and #1769 remain intact.

---

## 21. Implementation closure (ticket #1771, 2026-07-27)

*Sections 1-20 are the design record as written under ticket #1770 and are left
unchanged, including the retained-shim decision this section supersedes. This
section records what was actually built.*

### 21.1 Approved deviation from section 9.5: no deprecated shim

The user explicitly approved the removal of `CopyTo(void*, intcs)` **without any
compatibility overload**, and directed that no `[[deprecated]]` shim be retained.
The design's section 9.5 / 9.7 / 12 shim is therefore **not implemented**, and
alternative D of section 7 is rejected in full rather than partially adopted.

The reasoning recorded with the approval, which is stronger than the
diagnostic-quality argument that had favoured the shim:

- A retained overload that only throws lets old downstream code **compile
  successfully and fail at run time**. A compile error that names the replacement
  is a strictly safer migration signal than a deferred `NotSupportedException`.
- Under the repository's own `-Werror` policy (probe 7), calling a
  `[[deprecated]]` member is already a compile error — so for `-Werror` consumers
  the shim never preserved source compatibility anyway, while for everyone else it
  postponed a deterministic failure to run time.
- The unsafety of the old signature cannot be removed while keeping the
  signature: one pointer and one index can never carry the destination's element
  type, element count, element size, alignment, or construction state.

Everything else in sections 9, 10, 11, 13, 14, 15, and 16 is implemented as
written. Section 11's last row ("Legacy `CopyTo(void*, intcs)` →
`NotSupportedException`") is replaced by "removed; a call is a compile error",
and section 14's case 15 (shim throws) is replaced by the compile-time
`AcceptsDestination` assertions in 21.4.

### 21.2 ABI

Removing a pure virtual member changes the vtable layout of `ICollection` and of
every class deriving from it, including `IList` and `IDictionary`. Section 9.7's
"none in practice" ABI note was scoped to *this repository*, which ships no
installed package or export configuration; it is not a statement about downstream
binaries. **Every C++ consumer of sharp-runtime must be rebuilt in full.** Mixing
translation units compiled against the old and new headers is undefined
behaviour. This is stated for consumers in `docs/Migration-ICollectionCopyTo.md`.

### 21.3 Final public and protected surface

```cpp
namespace System::Collections {

using ObjectSpan = System::Span<std::any>;

namespace detail {
inline void requireValidCopyDestination(const void* data, intcs length,
                                        intcs index, intcs count);
template<typename T>
inline void requireValidCopyDestination(std::vector<T>& destination,
                                        intcs index, intcs count);
}

class ICollection : public IEnumerable {
public:
    void CopyTo(ObjectSpan destination, intcs index);
    void CopyTo(std::vector<std::any>& destination, intcs index);
protected:
    virtual void copyToCore(ObjectSpan destination, intcs index) = 0;
};

}
```

Concrete additions, all public unless marked:

| Type | Added |
|---|---|
| `ArrayList` | `using ICollection::CopyTo;`; protected `copyToCore` |
| `Queue` | `using ICollection::CopyTo;`; `void CopyTo(std::vector<void*>&, intcs)`; protected `copyToCore` |
| `Stack` | as `Queue`, top-to-bottom order |
| `Hashtable` | `using ICollection::CopyTo;`; `void CopyTo(std::vector<DictionaryEntry>&, intcs)`; protected `copyToCore`; `getCountProperty()` and the copy index normalised `int` → `intcs` |
| `ListDictionaryInternal` | as `Hashtable` (no `int` normalisation needed) |
| `ListDictionaryInternal::MemberCollection` | protected `copyToCore` only — the type is private and escapes only as `ICollection*` |

`IList.hpp` and `IDictionary.hpp` are unchanged; they inherit.

### 21.4 Known behavioural note: a null-data destination is always rejected

> **Superseded by [section 22](#22-follow-up-correction-ticket-1774-2026-07-27).**
> Left unchanged below as the historical record of what #1771 actually shipped;
> the described strictness was corrected by ticket #1774 on the same day.

Section 11's rule is implemented exactly: a destination whose data pointer is null
throws `ArgumentNullException` even when the collection is empty, matching .NET's
`ArgumentNullException` for a null `Array`. A `Span` cannot distinguish "the
caller passed nothing" from "the caller passed a zero-length container", and a
default-constructed `std::vector` typically has `data() == nullptr`, so
`collection.CopyTo(emptyVector, 0)` throws even when `getCountProperty() == 0`.
.NET accepts the equivalent `new object[0]`. This is stricter, never unsafe, and
documented in the header and in `docs/Migration-ICollectionCopyTo.md` §7; a
zero-length `ObjectSpan` over real storage *is* accepted for an empty collection.
Relaxing it would need a deliberate decision, because it trades the null-array
diagnostic for the zero-length-array one; it is not proposed here.

### 21.5 Files changed

| File | Change |
|---|---|
| `modules/collections/include/System/Collections/ICollection.hpp` | `ObjectSpan`, `detail::requireValidCopyDestination` (two overloads), the two public non-virtual `CopyTo` overloads, protected `copyToCore`; raw overload removed |
| `.../ArrayList.hpp`, `.../Queue.hpp`, `.../Stack.hpp`, `.../Hashtable.hpp`, `.../ListDictionaryInternal.hpp` | `copyToCore`, `using ICollection::CopyTo;`, typed overloads, `int` → `intcs` in `Hashtable` |
| `modules/core/include/System/Array.hpp`, `.../Buffer.hpp` | doc-comments no longer cite `ArrayList::CopyTo(void*, int)` as the raw-pointer precedent; that precedent no longer exists |
| `modules/collections/tests/.../CopyToBoundaryTests.cpp` | new permanent suite, 128 tests |
| `modules/collections/tests/.../InterfacesTests.cpp` | `MinimalCollection` re-implemented as `copyToCore`; `CopyToFillsBuffer` migrated |
| `modules/collections/tests/.../QueueStackTests.cpp` | `void* buf[4]` → `std::vector<void*> buf(4)` |
| `modules/collections/tests/.../CollectionsNewTests.cpp` | `ht.CopyTo(dest.data(), 0)` → `ht.CopyTo(dest, 0)` |
| `test/consumer/collections_copyto.cpp` | new standalone public-header fixture |
| `docs/Migration-ICollectionCopyTo.md` | new consumer migration document |

### 21.5.1 Compile-time migration assertions

`CopyToBoundaryTests.cpp` proves the removal at compile time with an
`AcceptsDestination<Collection, Destination>` detector: no `CopyTo` overload on
`ICollection`, `IList`, `IDictionary`, `ArrayList`, `Queue`, `Stack`,
`Hashtable`, `ListDictionaryInternal`, or a test implementation accepts `void*`,
`void**`, `std::any*`, `DictionaryEntry*`, `int*`, a wrongly typed vector, or a
temporary vector — while the intended destinations are all accepted.

### 21.6 Probes and validation

All probe trees are repository-local and gitignored (`build*`).

| Probe | Command | Result |
|---|---|---|
| Removed API (was probe 5) | `g++ -std=c++23 -fsyntax-only -Imodules/collections/include -Imodules/core/include build-probe-copyto/probe5_current_boundary.cpp` | exit 1, **4 errors**, one per scenario: `no matching function for call to '...CopyTo(std::nullptr_t/std::any*/void**, int)'`, each followed by `note: candidate:` lines for both surviving overloads. Captured in `build-probe-copyto/probe5_removed_api.log`. |
| Probe 8 — new boundary under sanitizers | `g++ -std=c++23 -g -O0 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Imodules/collections/include -Imodules/core/include -o build-probe-copyto/probe8_new_boundary build-probe-copyto/probe8_new_boundary.cpp modules/core/src/System/{Exception,ArgumentException,ArgumentNullException,ArgumentOutOfRangeException,InvalidOperationException,SystemException,IndexOutOfRangeException,NotSupportedException}.cpp` then `ASAN_OPTIONS=detect_leaks=1 build-probe-copyto/probe8_new_boundary` | 13 assertions, `failures=0`, exit 0, **no sanitizer diagnostic and no leak** |
| Probe 9 — permanent suite under sanitizers | same sanitizer flags over `CopyToBoundaryTests.cpp` + vendored GoogleTest | 128/128 passed, exit 0, no diagnostic, no leak |
| Probe 6 — standalone public headers | unchanged command from section 17 | exit 0 |
| Consumer fixture | `cmake -S test/consumer -B build-consumer-copyto -DFIXTURE_COMPONENT=Collections.Core -DFIXTURE_SOURCE=.../collections_copyto.cpp -DFIXTURE_COMPILE_ONLY=ON` then build; plus a linked `fixture_run` | compiles under `-Wall -Wextra -Wpedantic -Werror` against only `Collections.Core` + `Core.Base`; `fixture_run` exits 0 |

Probe 8 covers the four scenarios of section 2.3 plus non-trivial `std::any`
assignment, source destruction after copying, exactly-once destruction of
overwritten destination values, heterogeneous values, validation failures with
proof of no partial write, and 100,000-element exact-fit and one-short copies for
both `ArrayList` and `Hashtable`. The `typemix` scenario is no longer expressible
— there is no overload that accepts the wrong element type — so probe 8 runs the
safe equivalent, the same polymorphic `Hashtable` copy into the one destination
representation every implementation shares, and confirms it is leak-free.

### 21.7 Gates

| Gate | Result |
|---|---|
| `cmake --build build --parallel 4` | 0 errors, 0 warnings |
| `SharpRuntimeTests_Collections_Core` | 1,612/1,612 (was 1,484; +128) |
| `scripts/run_component_tests.sh build` | **12,871** across **37** executables (floor was 12,743) |
| `python3 scripts/validate_module_boundaries.py --root .` | 41 physical modules, 90 dependency edges — unchanged |
| `python3 test/validate_module_boundaries_test.py` | 7 tests, OK |
| `python3 scripts/generate_component_catalog.py --check` | catalogue current, no regeneration needed |
| `python3 scripts/db_consistency_check.py --db plan.sqlite3` | no consistency problems |
| `scripts/check_selective_components.sh` | all ten jobs pass, both negative fixtures still rejected |
| `git diff --check` | clean |
| `scripts/check_doxygen_warnings.sh` | Doxygen 1.9.8, **1,942** warnings — at the ceiling, unchanged from the pre-ticket baseline. `ICollection.hpp` loses one warning (`System::Type` written as `%System::Type` so Doxygen stops trying to link the permanently stubbed type); `README.md` gains one, the same unresolved-markdown-link warning that every one of its documentation links already produces. |

As in section 15.2 the new consumer fixture is deliberately **not** added to
`check_selective_components.sh`'s ten-job matrix, which is documented as exactly
ten jobs in `plan.md`, `README.md`, `docs/CMakeComponents.md`, and the tracked CI
workflow.

### 21.8 Ticket outcomes

- **#1771** — done.
- **#1772** (`REMED-COLL-COPYTO-CLEANUP`, P2/XS) — **obsolete**. Its two work
  items were both necessarily completed inside #1771: the deprecated shim it
  would have deleted was never created, and the `Array.hpp` / `Buffer.hpp`
  doc-comments citing `ArrayList::CopyTo(void*, int)` had to be corrected in the
  same change that removed the cited precedent, since leaving them would have
  documented a member that no longer exists. Marked `wontfix` with that reason
  rather than left inactive.
- **#1773** (`REMED-COLL-COPYTO-DOWNSTREAM`, P2/S) — new, **inactive**. Requires
  the CNA and mobile-eggbert sweep described in
  `docs/Migration-ICollectionCopyTo.md` §9. Neither repository is in this
  checkout, so nothing is claimed or changed about them.

---

## 22. Follow-up correction (ticket #1774, 2026-07-27)

*Sections 1-21 are the design record and #1771 closure exactly as originally
written and are left unchanged, including section 21.4's now-superseded
"known behavioural note". This section records a narrow correction found and
fixed immediately afterward, on the same branch.*

### 22.1 The defect

Section 21.4 documented, as an accepted strictness note, that a destination
with a null data pointer is rejected **even when the collection is empty**,
because `requireValidCopyDestination` checked `data == nullptr` unconditionally,
before ever looking at `length`. That is stricter than intended and stricter
than useful: `ObjectSpan` (`= System::Span<std::any>`) has no distinct
managed-null-array state to begin with (section 9.1) — a null pointer paired
with a zero length is not ".NET null", it is simply "no storage, no elements",
which is exactly what an empty destination for an empty source looks like.
Rejecting it forced every caller of an empty collection's `CopyTo` to
pre-allocate at least one destination element it would never receive, purely
to dodge a spurious `ArgumentNullException` — a burden .NET callers do not
carry (`new object[0]` is a valid destination for an empty source).

The one case that **is** genuinely malformed — a null pointer paired with a
**positive** length, i.e. a destination that claims elements it has no storage
for — was never distinguished from the harmless null-and-zero case. Both were
rejected by the same unconditional `data == nullptr` check.

### 22.2 The corrected rule

```cpp
inline void requireValidCopyDestination(const void* data, intcs length,
                                        intcs index, intcs count) {
    if (index < 0)
        throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
    if (index > length)
        throw System::ArgumentException(
            "Destination array is not long enough to copy all the items in the "
            "collection. Check array index and length.", "destination");
    if (data == nullptr && length > 0)
        throw System::ArgumentNullException("destination");
    if (length - index < count)
        throw System::ArgumentException(
            "Destination array is not long enough to copy all the items in the "
            "collection. Check array index and length.", "destination");
}
```

`data == nullptr && length == 0` is now a valid empty destination.
`data == nullptr && length > 0` remains the only null-pointer condition that is
rejected. Everything else about the boundary from sections 9, 10, 13, 14, 15,
and 21 is unchanged: the same two public non-virtual `CopyTo` overloads, the
same protected `copyToCore`, no restored `CopyTo(void*, intcs)`, no nullable
managed-array abstraction added to `ObjectSpan`.

### 22.3 Validation order

The order is now checked and tested explicitly, and differs from #1771's
implementation in one respect: the capacity-adjacent `index > length` check
and the null-data check swap relative positions so that a *malformed* pointer
is distinguished from a merely *short* one only after the index itself has
been range-checked against the destination's own length:

1. negative `index` → `ArgumentOutOfRangeException`;
2. `index` past the destination end (`index > length`) → `ArgumentException`;
3. null `data` paired with a positive `length` → `ArgumentNullException`;
4. insufficient remaining capacity (`length - index < count`) → `ArgumentException`;
5. success — `copyToCore` is dispatched to.

Splitting the former combined `index > length || length - index < count` check
into steps 2 and 4 does not change any existing test's outcome (both branches
already threw the same `ArgumentException` with the same message), but it is
what makes step 3's placement — and therefore the exact diagnosis when more
than one condition holds — observable and testable. The subtraction form of
step 4 is unchanged from #1771 and remains overflow-safe: step 2 having passed
guarantees `0 <= index <= length` before the subtraction runs.

### 22.4 Why a null pointer with a positive length is still rejected

`ObjectSpan(nullptr, 5)` is directly constructible through the public
`Span(T*, intcs)` constructor — nothing in `Span`'s invariants prevents a
caller from pairing a null pointer with a nonzero length, so this is a real,
reachable malformed state, not a hypothetical one requiring a special test
helper. It is rejected unconditionally (regardless of the source collection's
size or whether the copy would otherwise fit) because it claims storage for
elements that provably do not exist; treating it as valid would let a caller's
bug reach `copyToCore` with a pointer that must not be dereferenced.

### 22.5 Files changed

| File | Change |
|---|---|
| `modules/collections/include/System/Collections/ICollection.hpp` | `detail::requireValidCopyDestination` reordered and its null check made conditional on `length > 0`; doc-comments on the helper and on `CopyTo(ObjectSpan, intcs)` corrected |
| `modules/collections/tests/.../CopyToBoundaryTests.cpp` | two tests encoding the old unconditional-null rule replaced; new parameterised cases for empty-to-empty (both overloads), the malformed-null-with-length case, negative/past-end indices against a zero-length destination, span/vector agreement on the empty case, and a `ProbeCollection`-based proof that `copyToCore` is never reached on a validation failure; one lifetime case for a fully empty copy; two pre-existing typed-overload assertions corrected from `ArgumentNullException` to `ArgumentException` |
| `test/consumer/collections_copyto.cpp` | `rejectsInvalidDestinations()` corrected to expect `ArgumentException` for a zero-length destination against a non-empty source, plus new checks for the malformed-null-with-length case and an empty-to-empty success case |
| `docs/Migration-ICollectionCopyTo.md` | §7 exception table and the former "known strictness note" corrected |
| `README.md`, `NEXT.md`, `plan.md` | corrected references to the null-destination rule |
| `audit/AUDIT_FINDINGS_INDEX.md` | SR-AUD-358 remediation note extended to mention the correction |

### 22.6 Validation

| Check | Result |
|---|---|
| `cmake --build build --target SharpRuntimeTests_Collections_Core --parallel 4` | 0 errors, 0 warnings |
| `SharpRuntimeTests_Collections_Core` | 1,662/1,662 (was 1,612 after #1771; net +50) |
| `test/consumer/collections_copyto.cpp`, compile-only | compiles `-Wall -Wextra -Wpedantic -Werror` against only `Collections.Core` + `Core.Base` |
| `test/consumer/collections_copyto.cpp`, linked and run | exits 0 |
| `build-probe-copyto/probe10_empty_span_correction.cpp` under `-fsanitize=address,undefined` + LeakSanitizer | 10/10 assertions pass, `failures=0`, exit 0, no diagnostic, no leak |
| `scripts/check_selective_components.sh` | all ten jobs pass |
| `scripts/local_ci_check.sh build` | 12,921/12,921 tests across 37 executables (floor was 12,871), zero build warnings/errors |
| `python3 scripts/validate_module_boundaries.py` / `test/validate_module_boundaries_test.py` | 41 modules, 90 edges unchanged; 7/7 |
| `python3 scripts/generate_component_catalog.py --check` / `db_consistency_check.py` | catalogue current; no consistency problems |
| `git diff --check` | clean |
| `scripts/check_doxygen_warnings.sh` | Doxygen 1.9.8, **1,942** warnings -- at the ceiling, unchanged. One draft of this ticket's `README.md` addition briefly added a second markdown-style link to `docs/Migration-ICollectionCopyTo.md`, producing a 1,943rd warning (a second instance of the same unresolved-markdown-link class every README documentation link already produces); it was rewritten as plain backtick text pointing at the existing link, restoring the ceiling. |

Probe 10 covers: an empty source into `ObjectSpan{nullptr, 0}` and into a
default-constructed empty `std::vector<std::any>`; a non-empty source into
`ObjectSpan{nullptr, 0}` (capacity failure, not null); a null pointer with a
positive length rejected regardless of source size; a zero-length span over
real, non-null storage behaving identically to `{nullptr, 0}`; negative and
past-end indices against a zero-length destination; a non-trivial `std::any`
value surviving source destruction with exactly-once destruction on the
destination side; and polymorphic dispatch through `ICollection*`. No scenario
produced a sanitizer diagnostic, confirming that the empty-copy path never
dereferences the null pointer.

SR-AUD-358 remains `remediated`; this ticket did not reopen it. It corrects an
overly strict, previously-undocumented-as-a-defect corner of #1771's own
remediation, found immediately afterward on the same branch.
