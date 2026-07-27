<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration: `System::Collections::ICollection::CopyTo`

*Breaking change landed by ticket #1771 on 2026-07-27, remediating audit findings
SR-AUD-358 / CCF-020; the zero-length-destination rule in section 7 was
corrected by follow-up ticket #1774 the same day. Design record:
[`docs/ICollectionCopyToDesign.md`](ICollectionCopyToDesign.md) (see its
section 22 for the correction).*

---

## 1. What changed, in one line

`virtual void CopyTo(void* array, intcs index) = 0;` is **gone** from
`System::Collections::ICollection`. Copying now goes through a length-aware,
statically typed destination.

## 2. This is a source-breaking *and* ABI-breaking change

- **Source-breaking.** Any call of `collection.CopyTo(rawPointer, index)` and any
  `void CopyTo(void*, intcs) override` in a downstream class stops compiling. That
  is deliberate: the failure is a compile error that names the replacement
  overloads, not a runtime surprise.
- **ABI-breaking.** Removing a pure virtual member changes `ICollection`'s vtable
  layout, and every class deriving from `ICollection`, `IList`, or `IDictionary`
  inherits that change. **All C++ consumers of sharp-runtime must be fully
  rebuilt.** Linking objects compiled against the old headers with objects
  compiled against the new ones is undefined behaviour; a partial rebuild is not
  enough.
- No deprecated compatibility overload was retained. A retained overload that
  only threw would have let stale call sites compile and fail at run time; the
  compile error is the safer migration signal. (The repository builds with
  `-Werror`, under which a `[[deprecated]]` member is a compile error anyway, so
  the shim would have bought nothing while keeping a dead public member alive.)

## 3. Why the old API could not be repaired in place

A `void*` plus an index carries **no** destination element type, element size,
element count, alignment, or construction state. Through an `ICollection*` a
caller could not even learn what to allocate: `ArrayList` wrote `std::any`
(16 bytes), `Queue`/`Stack` wrote `void*` (8 bytes), and
`Hashtable`/`ListDictionaryInternal` wrote `DictionaryEntry` (32 bytes). Adding
bounds checks to each implementation could not fix that, because none of them can
discover the destination's length. The audit's sanitizer probes recorded a null
write, a heap-buffer-overflow read, an out-of-bounds write before the buffer, and
a silent 32-byte leak from an element-type mismatch — all through calls the
compiler accepted. See sections 2 and 7 of the design record.

## 4. The replacement API

```cpp
namespace System::Collections {

/** Length-aware view over already-constructed boxed-object elements. */
using ObjectSpan = System::Span<std::any>;

class ICollection : public IEnumerable {
public:
    void CopyTo(ObjectSpan destination, intcs index);             // validating, non-virtual
    void CopyTo(std::vector<std::any>& destination, intcs index); // convenience, non-virtual
protected:
    virtual void copyToCore(ObjectSpan destination, intcs index) = 0;  // the only virtual
};

}
```

The public methods validate the destination once and then dispatch, so no
implementation can skip or diverge from the shared checks, and no element is
written when validation fails.

## 5. Migrating a call site

### Old code

```cpp
void* storage = /* ... */;
collection.CopyTo(storage, index);
```

### New interface-level code

```cpp
std::vector<std::any> destination(static_cast<std::size_t>(collection.getCountProperty()) + index);
collection.CopyTo(destination, index);

// Retrieve with the boxing each collection documents (section 6).
int value = std::any_cast<int>(destination[index]);
```

An `ObjectSpan` over caller-owned storage works identically:

```cpp
std::any storage[8];
collection.CopyTo(System::Collections::ObjectSpan(storage, 8), 0);
```

### New concrete typed code

Where the concrete type is known, prefer its typed overload — it skips the boxing
and is checked by the compiler:

| Concrete collection | Destination type | Example |
|---|---|---|
| `ArrayList` | `std::vector<std::any>` | `std::vector<std::any> dest(list.getCountProperty()); list.CopyTo(dest, 0);` |
| `Queue` | `std::vector<void*>` | `std::vector<void*> dest(queue.getCountProperty()); queue.CopyTo(dest, 0);` |
| `Stack` | `std::vector<void*>` | `std::vector<void*> dest(stack.getCountProperty()); stack.CopyTo(dest, 0);` |
| `Hashtable` | `std::vector<DictionaryEntry>` | `std::vector<DictionaryEntry> dest(table.getCountProperty()); table.CopyTo(dest, 0);` |
| `ListDictionaryInternal` | `std::vector<DictionaryEntry>` | `std::vector<DictionaryEntry> dest(dict.getCountProperty()); dict.CopyTo(dest, 0);` |

`ArrayList` stores `std::any` already, so `std::vector<std::any>` *is* its typed
destination — the inherited overload is the right one.

### Migrating an implementation

A downstream class that implemented the interface moves its body to the protected
hook and drops its cast:

```cpp
// Old
void CopyTo(void* array, intcs index) override {
    auto* dest = static_cast<MyElement*>(array);
    for (std::size_t i = 0; i < items_.size(); ++i) dest[index + i] = items_[i];
}

// New
protected:
    void copyToCore(System::Collections::ObjectSpan destination,
                    SharpRuntime::intcs index) override {
        for (std::size_t i = 0; i < items_.size(); ++i)
            destination[index + static_cast<SharpRuntime::intcs>(i)] = std::any(items_[i]);
    }
```

`copyToCore` is called only after the destination has been validated, so it must
not re-validate and must not throw for argument reasons. If the class also adds a
typed public overload, it must write `using ICollection::CopyTo;` — a derived
`CopyTo` declaration otherwise hides every inherited one.

**Follow the compile error.** Every removed call site produces
`error: no matching function for call to '...CopyTo(<raw pointer>, int)'`
followed by `note: candidate:` lines listing the surviving overloads, so the
compiler points at the exact replacement. There is no implicit conversion from
`void*`, `void**`, `std::any*`, or `DictionaryEntry*` to either destination type,
so a silent misbind to the wrong overload is impossible.

## 6. What each collection puts in a slot

| Collection | Boxed value | Retrieve with |
|---|---|---|
| `ArrayList` | the stored `std::any`, copied unchanged | `std::any_cast<T>` for whatever was stored |
| `Queue`, `Stack` | `std::any(void*)` | `std::any_cast<void*>` |
| `Hashtable`, `ListDictionaryInternal` | `std::any(DictionaryEntry)` | `std::any_cast<DictionaryEntry>` |
| `ListDictionaryInternal` keys/values views | `std::any(void*)` | `std::any_cast<void*>` |

An empty `std::any` is this port's boxed `null`.

## 7. Exceptions

*Corrected by ticket #1774 (2026-07-27): the null-destination check is now
conditional on a positive destination length. See the note at the end of this
section for what changed and why.*

Checked in this exact order:

| # | Condition | Exception |
|---|---|---|
| 1 | Negative `index` | `System::ArgumentOutOfRangeException("index", "Non-negative number required.")` |
| 2 | `index` past the destination end | `System::ArgumentException(..., "destination")` |
| 3 | Destination has a null pointer **and** a positive length | `System::ArgumentNullException("destination")` |
| 4 | Destination too short for `getCountProperty()` elements from `index` | `System::ArgumentException(..., "destination")` |
| — | none of the above | success; `copyToCore` runs |

Validation always precedes the copy, so a rejected call never performs a partial
write. `index + Count` is never computed; the capacity test is the subtraction
`length - index < count`, so a large index cannot overflow past the check.

**A null pointer with a zero length is a valid empty destination.**
`ObjectSpan` (`= System::Span<std::any>`) has no distinct managed-null-array
state — `ObjectSpan{nullptr, 0}` and a default-constructed empty
`std::vector<std::any>` (whose `data()` is typically null) both mean "no
storage, no elements", exactly like .NET's `new object[0]`. Copying an empty
collection into either now succeeds with no exception, matching row "—" above:

```cpp
std::vector<std::any> destination;      // default-constructed, empty
emptyCollection.CopyTo(destination, 0); // succeeds
emptyCollection.CopyTo(System::Collections::ObjectSpan(), 0); // succeeds
```

A **non-empty** collection copied into a zero-length destination still fails,
but now on capacity (row 4, `ArgumentException`), not on nullness — the
destination is valid, it is simply too small. Only a null pointer paired with
a **positive** length (`ObjectSpan(nullptr, 5)`, a destination claiming
elements it has no storage for) is malformed and throws
`ArgumentNullException` (row 3), regardless of the source collection's size.

## 8. Not supported, by design

Rank, non-zero lower bounds, runtime element-type compatibility, and overlapping
source/destination storage are not representable at this boundary. They are
consequences of this port's permanent reflection/`System::Type` deviation, not new
gaps; `ObjectSpan` is always rank 1 with lower bound 0, and element-type
mismatches are compile errors rather than `ArrayTypeMismatchException` /
`InvalidCastException`.

## 9. Downstream consumers

**CNA** and **mobile-eggbert** are not present in this checkout, so nothing about
their current usage is claimed here. Each must be checked in its own repository:

1. search for `CopyTo(` calls reaching `System::Collections::ICollection`,
   `IList`, `IDictionary`, `ArrayList`, `Queue`, `Stack`, `Hashtable`, or
   `ListDictionaryInternal`;
2. search for classes deriving from those interfaces that override
   `CopyTo(void*, int)`;
3. migrate per sections 5 and 6;
4. **rebuild fully** against the new `ICollection` vtable — see section 2;
5. re-run that consumer's own tests.

This is tracked as ticket **#1773** (`REMED-COLL-COPYTO-DOWNSTREAM`), left
inactive in `plan.sqlite3` because those repositories are outside this one.
