<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# LinkedListNode lifetime contract

*Design record for ticket #1768 (`REMED-COLL-LINKED-NODE-DESIGN`), audit
findings SR-AUD-357 / CCF-019. Recorded 2026-07-27 before any production
change.*

## 1. Problem

`System::Collections::Generic::LinkedListNode<T>` is currently a copyable pair
of a raw `std::list<T>*` and a native `std::list<T>::iterator`:

```cpp
list_t* list_ptr_ = nullptr;
iter_t  iter_;
```

Every public accessor (`getValueProperty`, `operator T`, `operator==(const T&)`,
`getNextProperty`, `getPreviousProperty`) dereferences that iterator without any
liveness information. Removing the node, clearing the list, or destroying the
`LinkedList<T>` wrapper frees the underlying `std::list` node while every copied
handle stays truthy and dereferenceable.

A focused ASan/UBSan probe
(`build-probe-linkednode/probe_sr_aud_357.cpp`, retained until equivalent
permanent coverage exists) retains `getFirstProperty()`, destroys the owning
`LinkedList<int>`, and reads the public accessor:

```
==ERROR: AddressSanitizer: heap-use-after-free on address 0x503000000050
READ of size 4 at 0x503000000050 thread T0
    #0 ... in ownerDestruction probe_sr_aud_357.cpp:36
freed by thread T0 here:
    #6 ... in System::Collections::Generic::LinkedList<int>::~LinkedList()
```

The representation also cannot express .NET's independently allocated node
object, which is why the four existing-node insertion overloads
(`AddFirst(node)`, `AddLast(node)`, `AddBefore(node, newNode)`,
`AddAfter(node, newNode)`) were deliberately omitted from the port.

## 2. Reference behaviour (verified against the local .NET source)

Read from `/rv/tmp/runtime/src/libraries/System.Collections/src/System/Collections/Generic/LinkedList.cs`
and `.../src/Resources/Strings.resx` on 2026-07-27, not from memory.

- `LinkedListNode<T>` is a separately allocated class holding
  `list`, `next`, `prev`, and `item`. `public LinkedListNode(T value)` creates a
  node that exists with **no** owning list.
- `Invalidate()` — called by `InternalRemoveNode` and by `Clear` for every node —
  sets `list = null; next = null; prev = null` and **keeps `item`**. A removed
  node therefore stays alive, keeps its value, and reports no owner.
- `Next` returns `null` when `next == null` (detached) or `next == list.head`
  (tail of the circular list). `Previous` returns `null` when `prev == null` or
  `this == list.head`. Neither throws on a detached node.
- `Value` has a getter *and* a setter, and `ValueRef` exposes `ref T`. Both work
  on a detached node.
- `List` returns the owning list or `null`.
- `ValidateNode(node)` throws `ArgumentNullException(nameof(node))` for a null
  reference, then `InvalidOperationException(SR.ExternalLinkedListNode)` —
  *"The LinkedList node does not belong to current LinkedList."* — when
  `node.list != this`. A **detached** node reaches the second case.
- `ValidateNewNode(node)` throws `ArgumentNullException(nameof(node))`, then
  `InvalidOperationException(SR.LinkedListNodeIsAttached)` — *"The LinkedList
  node already belongs to a LinkedList."* — when `node.list != null`.
  Reattaching a detached node is therefore explicitly supported.
- `RemoveFirst`/`RemoveLast` on an empty list throw
  `InvalidOperationException(SR.LinkedListEmpty)` — *"The LinkedList is
  empty."*.
- `CopyTo` validates a negative index and `index > array.Length` as
  `ArgumentOutOfRangeException`, then insufficient remaining space as
  `ArgumentException`.

.NET has no destructor question: the GC keeps a node alive as long as any
reference exists. C++ must answer that question explicitly, which is what
section 4.3 does.

## 3. Selected representation

**Independently allocated, reference-counted node objects.** The `std::list<T>`
backing store is replaced by an intrusive doubly linked chain of
`System::Collections::Generic::detail::LinkedListNodeData<T>` objects:

```cpp
template<typename T>
struct LinkedListNodeData {
    T item;
    LinkedList<T>* owner = nullptr;
    std::shared_ptr<LinkedListNodeData> next;  // forward links own the chain
    std::weak_ptr<LinkedListNodeData>   prev;  // back links never own
};
```

`LinkedListNode<T>` becomes a copyable handle holding one
`std::shared_ptr<LinkedListNodeData<T>>`; `LinkedList<T>` holds the head
`shared_ptr`, a tail `weak_ptr`, an `intcs` count, and the existing `intcs`
version counter.

Rationale, evaluated against the alternatives:

| Candidate | Verdict |
|---|---|
| Keep `std::list<T>` and register live handles so the list can invalidate them | Rejected. A removed node cannot retain its value without a side copy, reattachment has no object to reattach, and correctness depends on a registry that must be searched per erase. |
| Keep `std::list<T>` and return a value snapshot instead of a handle | Rejected. `getValueProperty()` returns `T&` today and `AddBefore`/`AddAfter`/`Remove(node)` need node identity, not a value. |
| Independently allocated reference-counted nodes (**selected**) | Matches the .NET object model exactly: null handle, detached node with retained value, reattachment, owner-independent lifetime, and O(1) structural operations. |
| Raw `new`/`delete` nodes owned solely by the list | Rejected. It reproduces the current use-after-free the moment a handle outlives its owner. |

Forward links own and back links borrow, so the chain has no reference cycle and
teardown is iterative (section 4.3), not recursive.

## 4. The contract

### 4.1 Three states

| State | Representation | `operator bool` | `== nullptr` | `getListProperty()` |
|---|---|---|---|---|
| **Null handle** | empty `shared_ptr` | `false` | `true` | `nullptr` |
| **Detached node** | node object, `owner == nullptr` | `true` | `false` | `nullptr` |
| **Attached node** | node object, `owner == &list` | `true` | `false` | owning list |

`operator bool` means *"this handle refers to a node object"*, mirroring a
non-null `LinkedListNode<T>?` in .NET. Attachment is queried through
`getListProperty()`. This preserves every truthiness result the current header
produces: an empty list's `getFirstProperty()`, a failed `Find`, and a
default-constructed handle are null handles (`false`), an attached node is
`true`, and a handle to an already-removed node is `true` today as well — the
difference is that it is now safe rather than dangling.

### 4.2 Member behaviour

| Member | Null handle | Detached node | Attached node |
|---|---|---|---|
| `getValueProperty()` (both overloads) | throws `System::NullReferenceException` | reference to the retained value | reference to the value |
| `setValueProperty(value)` | throws `System::NullReferenceException` | stores the value | stores the value |
| `operator T()` | throws `System::NullReferenceException` | retained value | value |
| `getNextProperty()` | null handle | null handle | next node, or null handle at the tail |
| `getPreviousProperty()` | null handle | null handle | previous node, or null handle at the head |
| `getListProperty()` | `nullptr` | `nullptr` | owning list |
| `operator bool` | `false` | `true` | `true` |
| `operator==(nullptr)` | `true` | `false` | `false` |
| `operator==(const T&)` | `false` | compares the retained value | compares the value |
| `operator==(const LinkedListNode&)` | equal only to another null handle | node identity | node identity |
| copy / assignment | shares the same node object; both handles observe one node | | |

`getNextProperty()`/`getPreviousProperty()` deliberately keep returning a null
handle instead of throwing for a null receiver: that is the current header's
defensive behaviour, no repository consumer depends on a throw, and changing it
would break `node.getNextProperty().getNextProperty()` chains that are safe
today. Value access is the only path that previously reached undefined
behaviour, so it is the only path given a diagnostic.

`operator==(const LinkedListNode&)` is new and gives node identity, matching
.NET reference equality. It is required to answer "is this the same node?" once
detached nodes exist. It cannot change any existing call: no repository consumer
compares two node handles, and for `node == value` the exact-match `const T&`
overload still wins.

### 4.3 After structural events

| Event | Result for every copied handle to the affected node |
|---|---|
| `Remove(node)` / `Remove(value)` / `RemoveFirst` / `RemoveLast` | node becomes **detached**: owner `nullptr`, `Next`/`Previous` null, value retained |
| `Clear()` | every node becomes **detached**, each retaining its value |
| Removal through *another* copied handle | identical — all handles share one node object, so all observe the detached state |
| Destruction of the owning `LinkedList<T>` | the destructor performs the same detaching walk as `Clear()`; retained handles observe a detached node with its value intact and **no use-after-free** |
| Node no longer referenced by any handle | freed when the last owner (list chain or handle) releases it |

Teardown is iterative: the walk resets each node's `next` before releasing it, so
destroying a long list cannot recurse through `shared_ptr` destructors.

### 4.4 Retained value and reattachment

A removed node **retains its value** (`Invalidate()` parity) and a detached node
**can be attached again** to the same or a different list through the
existing-node overloads. This is the behaviour the C++ port previously could not
express at all.

### 4.5 Existing-node insertion overloads

Added, matching the .NET signatures and returning `void`:

```cpp
void AddFirst(LinkedListNode<T> node);
void AddLast(LinkedListNode<T> node);
void AddBefore(LinkedListNode<T> node, LinkedListNode<T> newNode);
void AddAfter(LinkedListNode<T> node, LinkedListNode<T> newNode);
```

A detached node is created with a new **explicit** constructor
`explicit LinkedListNode(const T& value)`. .NET's constructor is not explicit;
C++ requires `explicit` so that `AddLast(value)` and `AddLast(node)` never
become ambiguous. `AddLast(LinkedListNode<T>)` is an exact match for a node
argument and is not viable for a value argument, so the value overloads keep
their current resolution.

### 4.6 Validation and exceptions

- `validateNode` — null handle → `System::ArgumentNullException("node")`;
  otherwise `getListProperty() != this` (which includes every detached node and
  every foreign-list node) → `System::InvalidOperationException("The LinkedList
  node does not belong to current LinkedList.")`.
- `validateNewNode` — null handle → `System::ArgumentNullException("node")`;
  already attached to any list → `System::InvalidOperationException("The
  LinkedList node already belongs to a LinkedList.")`.
- `RemoveFirst`/`RemoveLast` on an empty list keep
  `System::InvalidOperationException("The LinkedList is empty.")`.
- `CopyTo` keeps its current, already .NET-correct three-step validation.

All message strings are the exact current .NET resource values.

### 4.7 `LinkedList<T>` copy and move

Both are now user-defined, because the default member-wise copy of a
`shared_ptr` head would alias one node chain into two lists:

- **Copy** — deep copy. The destination allocates fresh nodes with copied
  values; handles into the source keep pointing at the source's nodes. This is
  the observable behaviour of the current `std::list` member-wise copy.
- **Move** — the chain is transferred and every node's `owner` is repointed at
  the destination, so handles obtained from the source continue to report the
  correct owner. The source becomes empty. The re-owner walk is O(n); a
  `std::list` move is O(1). This cost is accepted deliberately: it is the price
  of deterministic node ownership, moves of a `LinkedList<T>` are rare, and the
  alternative (a shared owner box) adds an allocation and an indirection to
  every membership check.

### 4.8 Public STL `begin()`/`end()`

Preserved as a compatible migration. `begin()`/`end()` return a new
`LinkedList<T>::iterator` / `const_iterator` — a full bidirectional iterator
(`value_type`, `difference_type`, `pointer`, `reference`,
`iterator_category = std::bidirectional_iterator_tag`, `iterator_concept`,
default-constructible, `*`, `->`, `++`, `--`, `==`, `!=`) over the node chain.

- Range-`for`, `std::ranges`, and iterator-based algorithms keep working.
- Code written with `auto` is unaffected.
- Only code that spelled the type as `std::list<T>::iterator` would need to
  change; there is no such consumer in this repository and the header never
  documented the concrete type beyond "STL interop".
- The documented invalidation contract is unchanged: raw STL iterators follow
  plain `std::list` rules (erasing a node invalidates that node's iterator
  only) and are *not* the safe handle. `LinkedListNode<T>` is the safe handle.

### 4.9 Enumerator

`GetEnumerator()` keeps returning `IEnumerator<T>*` and keeps ticket #1767's
lifecycle guard unchanged: `System::Collections::detail::EnumeratorState`
rejects `Current` before the first successful `MoveNext` and after enumeration
ends, `requireUnmodified` fails fast on structural modification, and `Reset`
restores the start position. Only the cursor changes from a native
`std::list` iterator to a node pointer. Ticket #1767 must remain intact.

## 5. Consumers and compatibility

Repository consumers of `LinkedList<T>`/`LinkedListNode<T>` outside the header
itself (2026-07-27, excluding `audit/`, `vendor/`, and build trees):

- `modules/collections/tests/System/Collections/Generic/LinkedListSortedSetTests.cpp`
  — 30 LinkedList cases;
- `modules/collections/tests/System/Collections/Generic/ListTests.cpp`
  — 10 `GenLinkedList*` cases;
- `modules/collections/tests/System/Collections/EnumeratorLifecycleTests.cpp`
  — the ticket #1767 `EnumeratorLifecycleTest.LinkedList` case.

There is no production consumer inside the repository, and `docs/` does not
document the node representation. Downstream (CNA / mobile-eggbert) source
compatibility is therefore the governing constraint:

| Surface | Change | Risk |
|---|---|---|
| `LinkedListNode<T>` default ctor, `operator bool`, `== nullptr`, `!= nullptr`, `== T`, `!= T`, `operator T`, `getValueProperty`, `getNextProperty`, `getPreviousProperty` | signatures unchanged | none for compiling code; behaviour changes only where it was undefined |
| `LinkedList<T>` `getCountProperty`, `getFirstProperty`, `getLastProperty`, `AddFirst(T)`, `AddLast(T)`, `AddBefore(node,T)`, `AddAfter(node,T)`, `Remove*`, `Clear`, `Contains`, `Find`, `FindLast`, `CopyTo`, `GetEnumerator` | signatures unchanged | none |
| `begin()`/`end()` | iterator type changes from `std::list<T>::iterator` to `LinkedList<T>::iterator` | only for code that spells the concrete type |
| `LinkedList<T>` copy/move | now user-defined with the same observable copy semantics | none for copy; move additionally repoints owners |
| New: `explicit LinkedListNode(const T&)`, `setValueProperty`, `getListProperty`, node-identity `operator==`/`!=`, four existing-node insertion overloads | additive | none |

No public member is removed or renamed, so this is **not** a broad public API
break and needs no separate approval.

## 6. Ownership and dependencies

The header stays at
`modules/collections/include/System/Collections/Generic/LinkedList.hpp`, owned by
the `Collections.Core` physical module. New includes are `<memory>` (standard
library) and `System/NullReferenceException.hpp`, which `Core.Base` already
provides as a public dependency of `Collections.Core`. The component graph stays
at 41 physical modules and 90 production dependency edges, and no CMake metadata
changes.

## 7. Verification plan

### 7.1 Standalone public-header compile fixture

`test/consumer/collections_linked_list.cpp` exercises the selected
representation — detached construction, attachment, retained value after removal,
owner destruction, node identity, and range-`for` — against **only**
`SharpRuntime::Collections.Core`, compiled with `-Wall -Wextra -Wpedantic
-Werror` through `test/consumer/CMakeLists.txt` in `FIXTURE_COMPILE_ONLY` mode.
It is deliberately **not** added to `scripts/check_selective_components.sh`'s
default matrix: that matrix is documented as exactly ten jobs in `plan.md`,
`README.md`, `docs/CMakeComponents.md`, and the tracked CI workflow, and
SR-AUD-001 already tracks a divergence there. The fixture is run explicitly.

### 7.2 Permanent regression tests

A new focused suite,
`modules/collections/tests/System/Collections/Generic/LinkedListNodeLifetimeTests.cpp`,
must cover: default/null handle behaviour; attached-node behaviour; two copied
handles to one node; removal through the original handle; removal through
another copied handle; value retention after removal; `Next`/`Previous` after
detachment; `Clear`; owner destruction with retained handles; reattachment of a
detached node; duplicate attachment attempts; cross-list `Remove`/`AddBefore`/
`AddAfter`/existing-node insertion; first, last, middle, and single-node cases;
`LinkedList` copy and move plus node handle copy and move; `operator bool`;
range/STL iteration including `--` and `const` iteration; every existing-node
insertion overload; and the exact exception types and messages.

The existing 40 LinkedList cases and the ticket #1767 lifecycle case must
continue to pass unchanged. No existing test may be weakened, skipped, or
recategorised.

### 7.3 Sanitizer reproduction

`build-probe-linkednode/probe_sr_aud_357.cpp` (owner destruction, removal
through a copied handle, `Clear`, and neighbour navigation after detachment),
built with `-fsanitize=address,undefined`, must report `failures=0` and no
sanitizer diagnostic before SR-AUD-357 may be marked `remediated`. The probe is
retained until the permanent suite covers the same states, per the audit
handoff rule.

### 7.4 Gates

`SharpRuntimeTests_Collections_Core`, then
`python3 scripts/validate_module_boundaries.py --root .`,
`python3 test/validate_module_boundaries_test.py`,
`python3 scripts/generate_component_catalog.py --check`,
`python3 scripts/db_consistency_check.py --db plan.sqlite3`,
`scripts/check_selective_components.sh`, `git diff --check`, and finally
`scripts/local_ci_check.sh build`. The 12,694-test / 37-executable floor may
only rise. The Doxygen warning count must stay at or below the 1,942 ceiling.

## 8. Decision

The design is compatible and implementable without an unapproved public API
break. Implementation proceeds as ticket #1769
(`REMED-COLL-LINKED-NODE`, size L). Out of scope for both tickets: SR-AUD-358 /
CCF-020 (`ICollection::CopyTo`), and the JsonNode and XML LINQ members of
CCF-019, which need their own compatibility review.
