<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# SortedSet&lt;T&gt;::GetViewBetween live bounded-view contract

*Design record for ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`), audit
finding SR-AUD-361. Recorded 2026-07-27 before any production change. No
production or test source changed under this ticket. SR-AUD-361 remains
`confirmed` (design-complete), not `remediated`.*

---

## 1. Executive decision

`System::Collections::Generic::SortedSet<T>::GetViewBetween` must return a
**live, bounded, bidirectionally write-through view over the same underlying
tree state**, matching .NET's `TreeSubSet`, instead of the independent snapshot
copy it returns today.

The selected architecture is **Alternative D — one public type with a tagged
representation over independently owned, reference-counted tree state**:

- `SortedSet<T>` stops holding `std::set<T>` by value and instead holds
  `std::shared_ptr<State>`, where `State` owns the `std::set<T>` **and** the
  single version counter.
- A `SortedSet<T>` object is either an **owning full set** (no bounds) or a
  **bounded view** (`std::optional<T>` lower and upper bounds over the same
  `State`). One public type, one public return type, no new public class.
- `GetViewBetween` keeps returning `SortedSet<T>` **by value**; the returned
  object is a handle onto the parent's state, not a copy of its elements.
- `std::shared_ptr` reproduces .NET's GC lifetime rule exactly: a view keeps
  the state alive, so a view that outlives the set it came from is well-defined
  rather than a dangling reference.

**One public signature change is required and is not yet approved.**
`GetViewBetween` must lose its `const` qualifier, because a live view returned
from a `const SortedSet<T>&` would be a write-through handle onto a `const`
object. That is a source-breaking change of the same category as ticket
#1770/#1771's `ICollection::CopyTo` removal and ticket #1779/#1780's `Empty()`
return-type change, so implementation ticket **#1783 is created `blocked`**
pending explicit user approval (§28).

Alternative E (retain snapshot semantics and document the divergence) is
rejected: it is what the code does today, it leaves a documented .NET contract
permanently violated, and it does not fix the four adjacent defects this design
work newly measured (§4.6).

---

## 2. SR-AUD-361 evidence

The owning per-file report is
`audit/modules/collections/include/System/Collections/Generic/SortedSet.hpp.audit.md`.
Its finding text, preserved verbatim and unaltered by this ticket:

> **SR-AUD-361 — medium — GetViewBetween returns a detached snapshot rather
> than the required live bounded view**
>
> The implementation returns a separate `SortedSet` copy and explicitly
> documents the divergence. The direct probe reports
> `view-add-visible-in-source=0` and `source-add-visible-in-view=0`: mutations
> do not flow in either direction. .NET returns a range-enforced, write-through
> live view, so callers can silently mutate the wrong object.
>
> Missing assertions and diagnostics:
> - Tests exercise range membership but not bidirectional write-through, live
>   updates, or out-of-range view mutation.
> - A future implementation needs view-bound diagnostics for source/view
>   updates and violations.

The finding is `medium` in `audit/AUDIT_FINDINGS_INDEX.md` and is not a member
of any `CCF-*` cross-cutting cause.

Two earlier planning statements about this finding are **partly superseded by
this design** and are corrected here rather than rewritten in place, per this
repository's practice of preserving historical narrative:

- `NEXT.md` and `plan.md` (recorded under ticket #1779) state that SR-AUD-361
  "would require replacing `SortedSet<T>`'s `std::set` backing with a custom
  tree structure supporting live, bounded, write-through sub-range views —
  .NET's own `TreeSubSet` nested class is 378 lines — before any bounded
  implementation ticket could even be written". **That premise does not hold.**
  `std::set` is already an ordered associative container with `lower_bound`,
  `upper_bound`, and stable iterators; a bounded view needs only a shared owner
  for the container plus a pair of bounds. §7 and §10 give the full design and
  §11 the working prototype evidence; no hand-rolled red-black tree is needed.
  .NET's `TreeSubSet` is 378 lines mainly because it must re-implement
  `InOrderTreeWalk`, `BreadthFirstTreeWalk`, `FindNode`, `MinInternal`, and
  `MaxInternal` against raw `Node` pointers — work `std::set` already does.
- `SortedSet.hpp`'s own `@warning` doc-comment makes the same claim ("not
  achievable on top of `std::set` … without replacing this type's entire
  internal representation with a hand-rolled tree structure matching .NET's
  own"). It is wrong for the same reason and must be replaced by ticket #1783.

Correcting a wrong reason for deferring the work does not make the work
smaller: the ownership model, copy/move semantics, and the required `const`
removal are the real cost, and they are why this is a design-first ticket.

---

## 3. Pre-fix reproduction

All probes live in the repository-local, gitignored `build-probe-sortedset/`
tree (matched by the `build*` `.gitignore` entry). No production or test source
was modified. Build helper: `build-probe-sortedset/build.sh <probe> <mode>`,
which compiles with
`-std=c++23 -Wall -Wextra -Wpedantic` plus `-fsanitize=address,undefined`
(`asan`), `-Werror` (`werror`), or neither (`none`), against
`modules/core/include` and `modules/collections/include` and the six
`modules/core/src/System/*Exception*.cpp` support sources, so every frame in a
sanitizer report is instrumented.

### 3.1 Probe 1 — complete current-behavior matrix

```
./build-probe-sortedset/build.sh probe1_current_behavior asan
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
  ./build-probe-sortedset/probe1_current_behavior
```

Result: exit 0, `failures=0`, **no ASan/UBSan diagnostic and no leak**. The
current implementation is memory-safe; it is semantically wrong. Full log:
`build-probe-sortedset/probe1_current_behavior.log`. The load-bearing lines,
mapped to the seventeen required reproduction steps:

| # | Scenario | Observed | .NET |
|---|---|---|---|
| 1–2 | View shape over `{1..10}`, range `[3,7]` | `view-count=5`, `view-min=3`, `view-max=7`, excludes 2 and 8 | same |
| 3–4 | Parent `Add(5)` in range after view creation | **`source-add-visible-in-view=0`** | visible |
| 5–6 | Parent `Remove(4)` in range | **`source-remove-visible-in-view=0`** | visible |
| 7–8 | `view.Add(5)` | `view-add-returned=1`, **`view-add-visible-in-source=0`** | visible in source |
| 9–10 | `view.Remove(4)` | `view-remove-returned=1`, **`view-remove-visible-in-source=0`**, `parent-still-contains-4=1` | removed from source |
| 11 | `view.Add(99)`, far out of range | **`out-of-range-add-threw=0`**, `out-of-range-add-returned=1`, `out-of-range-value-now-in-view=1`, `view-max-after-out-of-range-add=99` | `ArgumentOutOfRangeException("item")` |
| 11b | `view.Remove(10)`, out of range | `out-of-range-remove-threw=0`, `out-of-range-remove-returned=0` | same (false, no throw) |
| 12 | `view.Clear()` | `clear-view-count-after=0`, **`clear-parent-count-after=7`** (unchanged from 7) | parent loses exactly the in-range elements |
| 13 | Nested `outer[3,7].GetViewBetween(4,6)` | `nested-inner-count=3` — correct by accident | same |
| 13b | Nested view that **widens** a bound | **`nested-widen-lower-threw=0`**, `nested-widen-lower-count=4`; **`nested-widen-upper-threw=0`** | `ArgumentOutOfRangeException("lowerValue"/"upperValue")` |
| 13c | `inner.Remove(5)` | `nested-inner-remove-visible-in-outer=0`, `nested-inner-remove-visible-in-parent=0` | visible in both |
| 14 | Parent destroyed, returned object survives | `after-parent-destruction-view-count=5`, sum 25, still mutable, **no ASan report** | view stays valid (GC keeps the parent alive) |
| 15 | Parent copy / move / copy-assign / move-assign | returned object completely unaffected in every case (`view-count-after-parent-move=2`, `viewMoved-count-after-parent-copy-assign=2`, …) | no C++ equivalent; view tracks the object |
| 16 | Mutating the **parent** during iteration of the returned object | **`parent-mutation-during-view-iteration-throws=0`**, all 3 elements visited | `InvalidOperationException` |
| 16b | Mutating the returned object during iteration of the **parent** | **`view-mutation-during-parent-iteration-throws=0`**, all 7 visited | `InvalidOperationException` |
| 16c | Mutating the returned object during its **own** iteration | `view-self-mutation-during-iteration-throws=1` | same |
| 17 | Element type whose ordering reverses `operator<` | `descending-view-count=3`, `descending-inverted-threw=1` — works only because the probe type defines **both** `operator<` and `operator>` consistently | ordering comes from `IComparer<T>` alone |
| — | Inverted range `GetViewBetween(7, 3)` | `ArgumentException`, message `lowerValue is greater than upperValue. (Parameter 'lowerValue')` | `Must be less than or equal to upperValue. (Parameter 'lowerValue')` |
| — | Equal bounds `[3,3]` | `equal-bounds-count=1` | same |
| — | Disjoint bounds `[100,200]` | `disjoint-range-count=0`, `disjoint-range-min=0` (`T{}`) | same |
| — | `view.UnionWith({5,6,99})` | `union-with-out-of-range-view-contains-99=1`, parent unaffected | `ArgumentOutOfRangeException`; in-range items write through |
| — | `view.IntersectWith`, `view.ExceptWith` | operate on the copy only; `intersect-parent-count=7` unchanged | write through to the parent |

The two boldface lines in rows 3–4 and 7–8 reproduce the audit's own
`view-add-visible-in-source=0` / `source-add-visible-in-view=0` evidence exactly.

### 3.2 Probe 2 — what the existing version guard actually covers

```
./build-probe-sortedset/build.sh probe2_iterator_lifetime asan
./build-probe-sortedset/probe2_iterator_lifetime safe        # exit 0
ASAN_OPTIONS=detect_leaks=0 ./build-probe-sortedset/probe2_iterator_lifetime copy-assign
ASAN_OPTIONS=detect_leaks=0 ./build-probe-sortedset/probe2_iterator_lifetime move-assign
ASAN_OPTIONS=detect_leaks=0 ./build-probe-sortedset/probe2_iterator_lifetime outlive
```

Logs: `probe2_safe.log`, `probe2_unsafe.log`.

- `safe`: `guard-add-fires=1`, `guard-remove-fires=1`, `guard-clear-fires=1`,
  `guard-duplicate-add-fires=0` (a rejected duplicate correctly does not bump
  the version). The guard works for same-object structural modification.
- `copy-assign`: `assign-guard-fired=0`, then
  `assign-stale-dereference-value=60`. Whole-object copy assignment destroys
  every node the outstanding iterator points into, but `version_` is a plain
  member that assignment **overwrites** with the source's counter instead of
  bumping, so the guard cannot fire. libstdc++'s `_Rb_tree` node-recycling
  assignment then reuses the storage, so ASan reports nothing and the iterator
  **silently yields an element of the new tree**. No diagnostic at all.
- `move-assign`: `move-assign-guard-fired=0`, then
  **ASan `heap-use-after-free`**, `READ of size 4`, freed by
  `_Rb_tree::_M_erase` during the assignment.
- `outlive`: an iterator outliving its set produces **ASan
  `stack-use-after-scope` inside `Iterator::checkVersion()` itself** — the
  guard's own raw `const SortedSet* owner_` read is the unsafe access.

`outlive` is ordinary C++ iterator-lifetime UB and is not a defect. The
`copy-assign` and `move-assign` results **are** a real gap in the guard this
class advertises, and are treated in §4.6 and §28.

### 3.3 Probe 3 — the documented element contract does not compile

```
./build-probe-sortedset/build.sh probe3_comparer_requirement werror
./build-probe-sortedset/build.sh probe3_comparer_requirement werror \
  -DSORTEDSET_PROBE_INSTANTIATE_VIEW
```

Without the macro the type works (`less-only-count=3`, `less-only-contains-5=1`,
`less-only-min=1`, `less-only-max=9`). With it, instantiating `GetViewBetween`
for a `T` that provides `operator<` **and nothing else** — exactly the contract
`SortedSet.hpp`'s own class doc-comment states — is a hard compile error:

```
SortedSet.hpp:297:19: error: no match for 'operator>' (operand types are 'const LessOnly' and 'const LessOnly')
  297 |         if (lower > upper)
SortedSet.hpp:300:77: error: no match for 'operator>' (operand types are 'const LessOnly' and 'const LessOnly')
  300 |         for (auto it = data_.lower_bound(lower); it != data_.end() && !(*it > upper); ++it)
```

`GetViewBetween` is the only member of the class that spells its comparisons
with `operator>`; every other ordering decision is delegated to `std::set`,
which uses `operator<` through `std::less<T>`.

---

## 4. Complete current behavior

### 4.1 Exact current declaration and implementation

`modules/collections/include/System/Collections/Generic/SortedSet.hpp:296-303`:

```cpp
[[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper) const {
    if (lower > upper)
        throw System::ArgumentException("lowerValue is greater than upperValue.", "lowerValue");
    SortedSet<T> view;
    for (auto it = data_.lower_bound(lower); it != data_.end() && !(*it > upper); ++it)
        view.Add(*it);
    return view;
}
```

The return is **`SortedSet<T>` by value** — not a reference, not another public
type, not a private proxy. It is a wrapper around freshly copied storage: a
default-constructed `SortedSet<T>` whose own `std::set` receives copies of the
in-range elements one `Add` at a time. Nothing links it to the source.

The member is `const`, so it is callable on a `const SortedSet<T>&` today.

### 4.2 Representation

```cpp
template<typename T>
class SortedSet {
    std::set<T> data_;
    intcs version_ = 0;
    // ... no base classes, no virtual members
};
```

Measured (`probe5_layout_symbols`, GCC 14.2.0, x86-64):
`sizeof(SortedSet<int>) = 56`, `alignof = 8`, `sizeof(std::set<int>) = 48`,
`sizeof(SortedSet<std::string>) = 56`, `sizeof(SortedSet<int>::Iterator) = 24`,
`is_polymorphic = 0`, `is_trivially_copyable = 0`,
`is_nothrow_move_constructible = 1`, `is_copy_assignable = 1`.

### 4.3 What the returned object does and does not do

Against the fourteen questions the ticket poses:

| Question | Current answer |
|---|---|
| Copies values into a new `SortedSet`? | **Yes** — element-by-element `Add`. |
| Shares comparer state? | No comparer state exists; both objects use `std::less<T>`. |
| Shares mutation state? | **No.** Separate `std::set` and separate `version_`. |
| Sees later parent insertions? | **No** (`source-add-visible-in-view=0`). |
| Sees later parent removals? | **No** (`source-remove-visible-in-view=0`). |
| Forwards view mutations to the parent? | **No** (`view-add-visible-in-source=0`). |
| Restricts additions to the bounds? | **No.** `view.Add(99)` succeeds and `Max` becomes 99. Bounds exist only for the instant of the copy. |
| Valid after parent copy/move/assign/clear/destruction? | **Yes, trivially** — it is independent. Probe 1 confirms with no sanitizer diagnostic. |
| Independent versioning? | **Yes**, and that is the defect: parent mutation cannot invalidate view enumerators. |
| Correct `Count`, `Min`, `Max`, enumeration? | Correct **at the instant of the call**, stale from the next parent mutation onward. |
| Reverse enumeration? | **The port has no `Reverse()` at all** (.NET has `IEnumerable<T> Reverse()`). |
| Nested `GetViewBetween`? | Compiles and returns another snapshot; **widening is silently accepted** where .NET throws. |
| Set operations within the bounds? | Present but bounds-unaware and non-write-through. |
| Iterator invalidation matching the parent? | **No** — three-way divergence, probe 1 rows 16/16b/16c. |
| Preserves comparer equivalence rather than `operator<` assumptions? | **No** — it uses `operator>`, which is neither `std::set`'s ordering predicate nor the documented element contract (§3.3). |

### 4.4 Documented divergence already in the header

`SortedSet.hpp:277-290` carries an explicit `@warning KNOWN DIVERGENCE FROM
.NET` block describing the snapshot behavior and asserting the fix is not
achievable on `std::set`. §2 corrects that assertion; ticket #1783 must replace
the block.

### 4.5 Test coverage of `GetViewBetween` today

Three tests, all asserting only snapshot-instant range membership:

| File:line | Assertions |
|---|---|
| `modules/collections/tests/System/Collections/Generic/LinkedListSortedSetTests.cpp:465` | `SortedSet<int> view = ss.GetViewBetween(3, 7);` then count/min/max and two negative `Contains`. |
| `modules/collections/tests/System/Collections/Generic/SortedStackTests.cpp:43` | `auto view = s.GetViewBetween(2, 4);` then count and three `Contains`. |
| `modules/collections/tests/System/Collections/Generic/Ticket1713VersionTrackingTests.cpp:108` | `auto view = s.GetViewBetween(2, 3);` count and two `Contains`; its comment explicitly documents the snapshot implementation and becomes stale under the fix. |

Focused validation of the current behavior for this ticket:
`./build/SharpRuntimeTests_Collections_Core --gtest_filter="SortedSetTests.*:GenSortedSetTest.*:SortedSetVersionTrackingTests.*"`
→ **41/41 passed**; `--gtest_filter="*GetViewBetween*"` → **3/3 passed**.

None of the three tests asserts a snapshot property that the live-view fix
would break: all three only read the view immediately after creating it. **The
existing test suite therefore requires no assertion change** under the selected
design; only the stale comment at `Ticket1713VersionTrackingTests.cpp:109` must
be corrected.

### 4.6 Adjacent defects measured during this design work

These are **not** SR-AUD-361 and are **not** new `SR-AUD-*` identifiers — the
audit numbering is frozen at SR-AUD-364. They are recorded here because they
live inside the surface ticket #1783 rewrites and would otherwise be silently
carried forward. They are folded into #1783's scope (§28), not spun out as
separate tickets.

1. **`GetViewBetween` requires `operator>`** although the class documents and
   otherwise needs only `operator<` (§3.3). A conforming element type fails to
   compile. Fixed for free by taking the predicate from `std::set::key_comp()`.
2. **Bounds are not enforced after construction.** `view.Add(99)` succeeds
   (probe 1 row 11). Even under snapshot semantics this contradicts the
   `@return`/`@param` documentation of a "range" object.
3. **Nested views may silently widen** (probe 1 row 13b) where .NET throws
   `ArgumentOutOfRangeException`.
4. **Whole-object assignment defeats the fail-fast version guard**, producing a
   silently wrong dereference on copy-assign and an ASan-confirmed
   `heap-use-after-free` on move-assign (§3.2). The selected ownership model
   eliminates both as a consequence, not as a special case (§14, §11.4).

Additionally, the exception **message** for an inverted range diverges from
.NET (`lowerValue is greater than upperValue.` vs `Must be less than or equal
to upperValue.`); the type and parameter name already match.

---

## 5. .NET behavior

Read from the local current .NET sources, not from memory:

- `/rv/tmp/runtime/src/libraries/System.Collections/src/System/Collections/Generic/SortedSet.cs` (2,015 lines)
- `/rv/tmp/runtime/src/libraries/System.Collections/src/System/Collections/Generic/SortedSet.TreeSubSet.cs` (379 lines)
- `/rv/tmp/runtime/src/libraries/System.Collections/src/Resources/Strings.resx`
- `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/Resources/Strings.resx`

| Question | .NET answer | Source |
|---|---|---|
| Public return type | `public virtual SortedSet<T> GetViewBetween(T? lowerValue, T? upperValue)` | `SortedSet.cs:1508` |
| Cached or new per call | **New per call**: `return new TreeSubSet(this, lowerValue, upperValue, true, true);` | `SortedSet.cs:1514` |
| How the view references the tree | `TreeSubSet : SortedSet<T>` holds `private readonly SortedSet<T> _underlying;` and re-roots via `root = _underlying.FindRange(_min, _max, _lBoundActive, _uBoundActive)` | `TreeSubSet.cs:17,46,318` |
| Bound inclusivity | **Inclusive both ends**: `IsWithinRange` returns false only when `Compare(_min, item) > 0` or `Compare(_max, item) < 0` | `TreeSubSet.cs:112-122` |
| Lower/upper validation | `if (Comparer.Compare(lowerValue, upperValue) > 0) throw new ArgumentException(SR.SortedSet_LowerValueGreaterThanUpperValue, nameof(lowerValue));` | `SortedSet.cs:1510-1513` |
| `lowerValue > upperValue` message | `"Must be less than or equal to upperValue."`, parameter `lowerValue` | `Strings.resx:138-140` |
| Parent → view propagation | `VersionCheckImpl`: when `version != _underlying.version`, the subset re-roots and adopts the parent's version | `TreeSubSet.cs:313-328` |
| View → parent propagation | `AddIfNotPresent` calls `_underlying.AddIfNotPresent(item)`; `DoRemove` calls `_underlying.Remove(item)` | `TreeSubSet.cs:59,84` |
| Out-of-range `Add` | `throw new ArgumentOutOfRangeException(nameof(item))` — parameter name `item`, default message | `TreeSubSet.cs:54-57` |
| Out-of-range `Remove` | `return false` — no throw, parent untouched | `TreeSubSet.cs:79-82` |
| `Clear` | Breadth-first collects the in-range items and calls `_underlying.Remove` for each; the parent keeps everything outside the range | `TreeSubSet.cs:92-110` |
| `Count` caching | `Count` calls `VersionCheck(updateCount: true)`; the subset recomputes by `InOrderTreeWalk` only when `_countVersion != _underlying.version` | `SortedSet.cs:266-273`, `TreeSubSet.cs:322-327` |
| `Min` / `Max` | `MinInternal` / `MaxInternal` are overridden to walk within the bounds and return `default(T)` when the view is empty | `TreeSubSet.cs:124-183`, `SortedSet.cs:1457,1478` |
| Nested views | A view may only **narrow**: widening either bound throws `ArgumentOutOfRangeException(nameof(lowerValue))` / `(nameof(upperValue))`; otherwise it delegates to `_underlying.GetViewBetween`, so nesting is always **flattened to depth 1** | `TreeSubSet.cs:342-353` |
| Set operations | Non-virtual base methods routed through virtual `Add`/`Remove`/`Contains`, so on a view they enforce bounds and write through; `UnionWith`/`IntersectWith` call `VersionCheck()` first when `this is TreeSubSet` | `SortedSet.cs:843-851,983-1000,1065,1103` |
| Enumeration and version checks | `Enumerator` captures `_tree` and `_version`; `MoveNext` calls `_tree.VersionCheck()` then throws `InvalidOperationException(SR.InvalidOperation_EnumFailedVersion)` on mismatch. `Initialize`/`MoveNext` skip out-of-range nodes via `_tree.IsWithinRange` | `SortedSet.cs:1829-1930` |
| Enumerator message | `"Collection was modified; enumeration operation may not execute."` | `Strings.resx:2647-2649` |
| Reverse enumeration | `public IEnumerable<T> Reverse()` builds `new Enumerator(this, reverse: true)`; on a view it is bounded like the forward one | `SortedSet.cs:1499-1506` |
| Synchronization / thread safety | **None**: `bool ICollection.IsSynchronized => false`, `object ICollection.SyncRoot => this` | `SortedSet.cs:279-282` |
| Owner no longer referenced | The view's `_underlying` field is a strong reference, so the GC **cannot** collect the parent while any view is reachable; the view stays fully functional | `TreeSubSet.cs:17,41` |
| Comparer propagation | `TreeSubSet` constructs `: base(Underlying.Comparer)` — the view always uses the parent's comparer | `TreeSubSet.cs:39` |
| Copy construction from a view | `new SortedSet<T>(collection)` explicitly **excludes** `TreeSubSet` from its `DeepClone` fast path and falls back to enumerate-sort-dedupe | `SortedSet.cs:88` |
| Serialization on a view | `GetObjectData` / `OnDeserialization` throw `PlatformNotSupportedException` | `TreeSubSet.cs:365-375` |
| Exception ordering | Range validity is checked **before** any allocation; on a view, bound checks precede narrowing | `SortedSet.cs:1510`, `TreeSubSet.cs:344-351` |

### 5.1 Which .NET behaviors this port can and cannot reproduce

**(1) Directly reproducible.** Bound inclusivity; the invalid-range exception
type, message, and parameter name; bidirectional write-through; out-of-range
`Add` throwing `ArgumentOutOfRangeException("item")`; out-of-range `Remove`
returning `false`; range-scoped `Clear`; lazy version-gated `Count`; bounded
`Min`/`Max` returning `T{}` when empty; nested-view narrowing-only validation
with flattening; bounds-enforcing write-through set algebra; a single shared
version counter driving fail-fast enumeration; comparer propagation; and the
"no synchronization" contract.

**(2) Relies on managed GC or object identity.** Only one behavior: *a view
keeps its parent alive*. `std::shared_ptr<State>` reproduces it exactly, with
one deliberate refinement — what stays alive is the **tree state**, not the
parent `SortedSet` *object*. In .NET those are the same thing; in C++ the
object is a value that can be copied, moved, assigned, and destroyed
independently of its storage. §12 defines the consequences.

**(3) Requires a different safe C++ ownership model.** Copy, move, assignment,
and destruction of a `SortedSet<T>` **object** have no .NET counterpart at all,
because .NET `SortedSet<T>` is a reference type with no copy or assignment
operator. §12 and §13 define them from first principles rather than by
analogy. Likewise, `TreeSubSet`'s virtual-override mechanism cannot be used:
`GetViewBetween` returns by value, and returning a derived type by a base value
would slice it. The tagged representation in §10 is the C++ equivalent.

**(4) Intentional sharp-runtime deviations.** Four, all recorded in §26:
serialization hooks (absent by permanent project deviation, so
`PlatformNotSupportedException` on a view has nothing to attach to);
`Reverse()` (absent from the port and deliberately not added here);
`IComparer<T>` construction (absent from the port — ordering is
`std::less<T>`, so "comparer propagation" reduces to "the view uses the same
`std::set` and therefore the same `key_comp()`"); and `T?`/`default(T)` nullable
bound arguments (C++ references cannot be null, so both bounds are always
active, matching `GetViewBetween`'s own `lowerBoundActive: true,
upperBoundActive: true`).

---

## 6. Affected-surface inventory

`SortedSet<T>` is a standalone class template: **no base classes and no virtual
members**, so the "collection interfaces implemented by `SortedSet<T>`" list is
empty. It implements none of `ICollection`, `IEnumerable<T>`, `ISet<T>`, or
`IReadOnlyCollection<T>`, unlike .NET's
`SortedSet<T> : ISet<T>, ICollection<T>, ICollection, IReadOnlyCollection<T>,
IReadOnlySet<T>, ISerializable, IDeserializationCallback`. That absence is
what makes a tagged single-type representation feasible at all.

| Surface | Line | Change under the selected design |
|---|---|---|
| `SortedSet()` | 61 | Body changes: allocate the shared `State`. Signature unchanged. |
| `explicit SortedSet(std::initializer_list<T>)` | 67 | Body changes. Signature unchanged. |
| Copy constructor | implicit | **Becomes user-declared.** Owning set → deep clone (today's behavior); view → another handle. |
| Move constructor | implicit | **Becomes user-declared**, `noexcept`; leaves the source a valid empty owning set. |
| Copy assignment | implicit | **Becomes user-declared.** Rebinds this handle; never mutates state another handle observes. |
| Move assignment | implicit | **Becomes user-declared**, `noexcept`. |
| Destructor | implicit | Stays implicit; `shared_ptr` releases the state, which survives while any view or iterator holds it. |
| `GetViewBetween` | 296 | **Loses `const`**; returns a live bounded handle; validates via `key_comp()`; rejects nested widening. |
| `Add` | 106 | On a view, rejects out-of-range with `ArgumentOutOfRangeException("item")`; writes to shared state. |
| `Remove` | 119 | On a view, returns `false` for out-of-range; writes to shared state. |
| `Clear` | 141 | On a view, erases only `[lower, upper]` from the shared state. |
| `Contains` | 132 | On a view, returns `false` for out-of-range. |
| `getCountProperty` | 75 | O(1) for an owning set; version-cached O(k) for a view. |
| `getIsEmptyProperty` | 81 | Delegates to `getCountProperty() == 0`. |
| `getMinProperty` / `getMaxProperty` | 89 / 97 | Range-scoped; `T{}` when the view is empty. |
| Lower/upper bound operations | — | No public member exists; internally `std::set::lower_bound`/`upper_bound` become the range primitives. |
| `Iterator` (nested class) | 39 | Holds `shared_ptr<const State>` + current + end + version; no raw owner pointer. |
| `begin()` / `end()` | 320 / 322 | Range-scoped for a view. Signatures unchanged. |
| `Reverse` | — | **Does not exist.** Explicitly out of scope (§26). |
| `UnionWith`, `IntersectWith`, `ExceptWith`, `SymmetricExceptWith` | 149–201 | Bounds-enforcing and write-through on a view; **existing `&other == this` self-aliasing guards must be strengthened to shared-state comparison** (§18). |
| `IsSubsetOf`, `IsSupersetOf`, `IsProperSubsetOf`, `IsProperSupersetOf`, `SetEquals`, `Overlaps` | 210–270 | Must compare in-range elements only; `SetEquals`'s `data_ == other.data_` must become an element-wise range comparison. |
| `ToVector` | 311 | Range-scoped for a view. |
| Comparer access | — | No public accessor exists; not added. |
| Serialization hooks | — | None exist; not added (permanent project deviation). |
| `ToSortedSet()` | — | **New**, additive: materializes an independent owning set (§10). |
| `getIsViewProperty()` | — | **New**, additive: lets callers and tests distinguish the two roles. |

### Consumers and tests inside sharp-runtime

`System/Collections/Generic/SortedSet.hpp` is included by exactly **three
files, all tests** (§4.5). There is **no production consumer anywhere in this
repository**, no `test/consumer/` fixture, and no other module header. The
module owner is `Collections.Core` (`modules/collections/CMakeLists.txt`),
whose only public dependency is `Core.Base`; the design adds no new include and
therefore **no new dependency edge** — the graph stays at 41 modules / 90 edges.
`System::Collections::Immutable::ImmutableSortedSet` is unrelated: it uses
`std::set` directly and does not include this header.

---

## 7. C++ ownership constraints

1. **`GetViewBetween` returns by value.** Any design in which the view is a
   *derived* type is impossible without changing the return type, because
   returning a derived object through a base value slices it. This alone rules
   out a literal port of `TreeSubSet`.
2. **`SortedSet<T>` is a value type today** — copyable, movable, assignable,
   destructible, and stored by value in every existing call site. Turning the
   whole type into a reference/handle type would silently convert every
   existing `SortedSet<T> b = a;` into an alias. Unacceptable.
3. **Elements must be owned independently of any one object.** For a view to
   remain valid while its parent object is copied, moved, assigned, or
   destroyed, the `std::set<T>` cannot live inside the parent object. This is
   the single structural change every live-view alternative shares, and it is
   why **no live-view design can preserve object layout** (§17).
4. **`std::set` iterators are stable** across insert and across erase of other
   elements, so a bounded range can be expressed as a pair of positions
   recomputed on demand; no custom tree is required.
5. **`std::set` owns its ordering predicate**, retrievable by value via
   `key_comp()`. Any comparison the view performs must use that predicate, not
   `operator<` or `operator>` written by hand, or the view's notion of "in
   range" can disagree with the container's notion of "sorted".
6. **There is no GC.** Lifetime must be explicit. `std::shared_ptr` gives
   exactly the reachability rule .NET's strong `_underlying` field gives, at the
   cost of one control block per set.
7. **No virtual members may be added.** `SortedSet<T>` is non-polymorphic
   today (`is_polymorphic = 0`); adding a vptr would change layout further,
   break value semantics under slicing, and violate the project's "no broad
   public header refactor" rule for no benefit.

---

## 8. Alternatives considered

### Alternative A — shared tree state for sets *and* views, uniformly

Every `SortedSet<T>` holds `shared_ptr<State>`; copying **always** shares.
Views are the same object with bounds.

*Rejected.* It converts `SortedSet<T>` from a value type into a handle type for
every existing user. `SortedSet<int> b = a; b.Add(x);` would mutate `a` — a
silent, un-diagnosable semantic break in code that never mentions
`GetViewBetween`. It is also *further* from .NET than the selected design: .NET
has no copy operation at all, so there is no .NET behavior that A reproduces
and D does not. Its only advantage over D is that copy semantics become
uniform, which is not worth breaking every unrelated consumer.

### Alternative B — parent-owned tree with weak or non-owning views

The parent owns a `shared_ptr<State>`; views hold `weak_ptr<State>` and throw
`InvalidOperationException` (or `ObjectDisposedException`) once the parent dies.

*Can it be safe?* Yes — `weak_ptr::lock()` makes parent death **deterministically
detectable** with no dangling pointer, and the moved-from/reassigned cases are
detectable too. B is memory-safe; it is not, however, simpler or more faithful.

*Rejected* for three reasons. (i) It diverges from .NET precisely where D
matches: in .NET a view keeps its parent alive and never becomes invalid, and
returning a view from a factory whose set is a local is legal, idiomatic managed
code. Under B that pattern throws at first use. (ii) Every single member —
`Count`, `Min`, `Max`, `Contains`, `Add`, `Remove`, `Clear`, `begin`, `end`,
every set operation — needs a liveness check and a new failure mode, roughly
doubling the exception matrix for a state .NET cannot even reach. (iii) It buys
nothing D lacks: D never dangles either, because the state outlives every
handle. B trades a strictly-safe behavior for a throwing one.

### Alternative C — dedicated public `SortedSetView<T>` type

`GetViewBetween` returns a distinct `SortedSetView<T>` (or an internal proxy).

*Genuine advantages.* Roles are explicit in the type system; copy semantics are
unambiguous (a view type is documented as a handle, a set type as a value); no
`if (isView())` branch inside `SortedSet<T>`; and a view can be made
non-assignable or non-default-constructible if desired.

*Rejected.* (i) **It does not avoid the layout change** — the view still needs
the set's storage to be independently owned, so `SortedSet<T>`'s data members
change anyway. C pays D's whole compatibility cost and adds a return-type break
on top. (ii) The return type changes, breaking
`SortedSet<int> view = ss.GetViewBetween(3, 7);` — one in-repository test uses
exactly that spelling, and downstream usage cannot be inspected. (iii) It
breaks .NET parity structurally: in .NET a view **is a** `SortedSet<T>` and can
be passed to `UnionWith`, `IsSubsetOf`, `SetEquals`, or any `SortedSet<T>`
parameter. `SortedSetView<T>` would need either an implicit conversion (which
re-materializes a snapshot at every boundary, reintroducing the very defect) or
a duplicated set-algebra API on a second type. (iv) It adds a new public header
and public type to `Collections.Core`.

### Alternative D — one public type with a tagged representation ***(SELECTED)***

`SortedSet<T>` holds `shared_ptr<State>` plus optional bounds and is either an
owning full set or a bounded view. `GetViewBetween` keeps its return type.

*Costs, stated honestly.* (i) Copy semantics depend on the object's role. This
is stated as one rule — *copying preserves the role: an owning set copies its
elements, a view copies its reference* — but it is still a runtime-dependent
behavior, and it is the single most surprising thing about the design. (ii)
About ten members gain an `isView()` branch. (iii) `sizeof` changes, up for
large `T` (§17). (iv) One pointer indirection on every operation.

*Benefits.* It is the only alternative that keeps the public return type, keeps
every existing call site compiling and every existing assertion passing (§4.5),
keeps a view usable everywhere a `SortedSet<T>` is expected, and matches .NET's
own model of "the view is a `SortedSet<T>`". It also fixes all four adjacent
defects of §4.6 as a by-product.

### Alternative E — retain snapshot semantics and document the divergence

*Evaluated honestly, and rejected.* Its cost is not zero, and it is not merely
"the finding stays open":

- Ported C# that relies on write-through — `set.GetViewBetween(a,b).Add(x)`,
  `view.Clear()` to delete a range, `foreach` over a view while the set is
  mutated — compiles unchanged and produces a **silently different result**.
  There is no compile error, no exception, and no diagnostic. This is exactly
  the failure mode ticket #1771 rejected when it declined to keep a throwing
  `CopyTo` shim ("removal makes each call a compile error naming the
  replacement").
- The header already documents the divergence and has done so for the whole
  life of the finding; documentation demonstrably has not prevented the audit
  from classifying it as a confirmed medium defect.
- It leaves the four adjacent defects of §4.6 in place, including one
  (`operator>`) that makes a documented-conforming element type fail to
  compile and one (assignment defeating the version guard) with an
  ASan-confirmed use-after-free.
- The cost of *deferring* rises: every new consumer written against snapshot
  behavior increases the eventual migration burden.

A reduced variant — **E′: keep snapshot semantics but fix the four adjacent
defects and sharpen the documentation** — is a legitimate fallback if the
`const` removal in §28 is refused. It closes none of SR-AUD-361 but is strictly
better than the status quo. It is recorded as the rollback target in §21.

---

## 9. Compatibility matrix

Ratings: ✅ preserved / ⚠️ changed but manageable / ❌ broken.

| Criterion | A (uniform shared) | B (weak views) | C (view type) | **D (tagged)** | E (snapshot) |
|---|---|---|---|---|---|
| Memory safety | ✅ | ✅ | ✅ | ✅ | ✅ |
| Ownership / lifetime model | ⚠️ implicit sharing | ⚠️ views expire | ✅ explicit | ✅ explicit, role-based | ✅ trivial |
| Parent → view propagation | ✅ | ✅ | ✅ | ✅ | ❌ |
| View → parent propagation | ✅ | ✅ | ✅ | ✅ | ❌ |
| Bounds enforcement | ✅ | ✅ | ✅ | ✅ | ❌ |
| Copy behavior | ❌ every set becomes an alias | ⚠️ | ✅ | ⚠️ role-dependent | ✅ |
| Move behavior | ✅ | ⚠️ | ✅ | ✅ | ✅ |
| Iterator validity | ✅ | ⚠️ extra expiry mode | ✅ | ✅ (also fixes §4.6.4) | ⚠️ §4.6.4 unfixed |
| Comparer support | ✅ | ✅ | ✅ | ✅ (fixes §4.6.1) | ❌ §4.6.1 unfixed |
| Public source compatibility | ✅ | ⚠️ `const` removal | ❌ return type **and** `const` | ⚠️ `const` removal only | ✅ |
| ABI / mangled symbols | ⚠️ | ⚠️ | ❌ | ⚠️ one mangling change | ✅ |
| Object layout | ❌ | ❌ | ❌ | ❌ | ✅ |
| Implementation complexity | Low | High | High (two types) | Medium | None |
| Performance | Same | +`lock()` per op | Same as D | +1 indirection; O(k) view `Count` | Best per-op, O(k) per call |
| Module dependencies | none added | none added | none added | **none added** | none |
| Testability | Poor (aliasing hard to pin) | Medium | Good | Good | Good |
| Migration burden | Catastrophic | High | High | Medium | None |
| **.NET parity** | Partial | Partial | Partial | **Full** | None |

---

## 10. Selected architecture

`SortedSet<T>` becomes a handle onto reference-counted tree state, tagged by
the presence of bounds.

```
                      ┌──────────────────────────────┐
   SortedSet<T> parent│ shared_ptr<State> ───────────┼──┐
   (owning: no bounds)│ optional<T> lower_ = {}      │  │
                      │ optional<T> upper_ = {}      │  │   ┌────────────────┐
                      └──────────────────────────────┘  ├──▶│ State          │
                      ┌──────────────────────────────┐  │   │  std::set<T>   │
   SortedSet<T> view  │ shared_ptr<State> ───────────┼──┤   │  intcs version │
   (bounded: [3,7])   │ optional<T> lower_ = 3       │  │   └────────────────┘
                      │ optional<T> upper_ = 7       │  │           ▲
                      └──────────────────────────────┘  │           │
                      ┌──────────────────────────────┐  │           │
   SortedSet<T> view2 │ shared_ptr<State> ───────────┼──┘           │
   (bounded: [4,6],   │ optional<T> lower_ = 4       │              │
    nested → flattened│ optional<T> upper_ = 6       │  Iterator ───┘
    to depth 1)       └──────────────────────────────┘  (also holds a
                                                         shared_ptr)
```

- **One shared `version`.** Any structural modification through the parent or
  through any view bumps it, so every outstanding iterator on any of them
  fail-fasts — reproducing .NET's single `_underlying.version`.
- **Views are flattened.** A view of a view refers to the same root `State`
  with intersected bounds, exactly as `TreeSubSet::GetViewBetween` delegates to
  `_underlying.GetViewBetween`. Nesting depth is always 1.
- **Iterators hold their own `shared_ptr<const State>`**, so an iterator can
  outlive the object it came from without reading freed memory.

---

## 11. Proposed public declarations

Precise enough that ticket #1783 does not redesign anything. Doc-comments are
elided here; #1783 must supply full Doxygen blocks per `CLAUDE.md` §3.

```cpp
namespace System::Collections::Generic {

using SharpRuntime::intcs;

template<typename T>
class SortedSet {
    struct State {
        std::set<T> data;
        intcs version = 0;
    };

    std::shared_ptr<State> state_;
    std::optional<T> lower_;                 // absent => lower bound inactive
    std::optional<T> upper_;                 // absent => upper bound inactive
    mutable intcs cachedCount_ = -1;         // .NET TreeSubSet::count
    mutable intcs cachedCountVersion_ = -1;  // .NET TreeSubSet::_countVersion

    using SetIterator = typename std::set<T>::const_iterator;

    // std::set::key_comp() returns BY VALUE. Binding it to a const reference
    // returns a reference to a temporary (-Wreturn-local-addr, observed while
    // prototyping). Copy the predicate; never alias it.
    [[nodiscard]] typename std::set<T>::key_compare comparer() const;
    [[nodiscard]] SetIterator rangeBegin() const;   // lower_bound(*lower_) or begin()
    [[nodiscard]] SetIterator rangeEnd()   const;   // upper_bound(*upper_) or end()

    SortedSet(std::shared_ptr<State> state,
              std::optional<T> lower,
              std::optional<T> upper);            // private view constructor

public:
    class Iterator {
        std::shared_ptr<const State> state_;
        SetIterator it_;
        SetIterator end_;
        intcs version_ = 0;
        void checkVersion() const;
    public:
        Iterator(std::shared_ptr<const State> state, SetIterator it, SetIterator end);
        const T& operator*()  const;
        const T* operator->() const;
        Iterator& operator++();
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
    };

    SortedSet();
    explicit SortedSet(std::initializer_list<T> items);

    SortedSet(const SortedSet& other);                        // role-preserving
    SortedSet(SortedSet&& other) noexcept;
    SortedSet& operator=(const SortedSet& other);             // rebinds
    SortedSet& operator=(SortedSet&& other) noexcept;
    ~SortedSet() = default;

    // --- unchanged signatures, view-aware bodies -------------------------
    [[nodiscard]] intcs getCountProperty() const;
    [[nodiscard]] bool  getIsEmptyProperty() const;
    [[nodiscard]] T     getMinProperty() const;
    [[nodiscard]] T     getMaxProperty() const;
    bool Add(const T& item);
    bool Remove(const T& item);
    [[nodiscard]] bool Contains(const T& item) const;
    void Clear();
    void UnionWith(const SortedSet<T>& other);
    void IntersectWith(const SortedSet<T>& other);
    void ExceptWith(const SortedSet<T>& other);
    void SymmetricExceptWith(const SortedSet<T>& other);
    [[nodiscard]] bool IsSubsetOf(const SortedSet<T>& other) const;
    [[nodiscard]] bool IsSupersetOf(const SortedSet<T>& other) const;
    [[nodiscard]] bool IsProperSubsetOf(const SortedSet<T>& other) const;
    [[nodiscard]] bool IsProperSupersetOf(const SortedSet<T>& other) const;
    [[nodiscard]] bool SetEquals(const SortedSet<T>& other) const;
    [[nodiscard]] bool Overlaps(const SortedSet<T>& other) const;
    [[nodiscard]] std::vector<T> ToVector() const;
    Iterator begin() const;
    Iterator end()   const;

    // --- THE ONE BREAKING SIGNATURE CHANGE: `const` is removed -----------
    [[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper);

    // --- additive, non-breaking -----------------------------------------
    [[nodiscard]] bool getIsViewProperty() const;
    [[nodiscard]] bool IsWithinRange(const T& item) const;
    [[nodiscard]] SortedSet<T> ToSortedSet() const;   // materialize, detached
};

} // namespace System::Collections::Generic
```

`ToSortedSet()` is the C++ spelling of .NET's `new SortedSet<T>(view)` — the
supported way to obtain the old snapshot behavior deliberately (§24). A
collection constructor `SortedSet(const SortedSet&)` cannot express it because
it collides with the copy constructor.

New standard includes required: `<memory>`, `<optional>`, `<iterator>`, plus
`System/ArgumentOutOfRangeException.hpp`. All are `Core.Base` or standard
library; **no new module dependency edge** (§6).

---

## 12. Ownership and lifetime model

1. **The `State` is the owner of the elements.** No `SortedSet<T>` object owns
   elements directly. Every object — owning set or view — holds a
   `shared_ptr<State>`.
2. **A `State` lives exactly as long as at least one handle references it**,
   where a handle is an owning set, a view, or an `Iterator`. This is the C++
   expression of .NET's rule that `TreeSubSet._underlying` is a strong
   reference.
3. **An owning set is a `SortedSet<T>` with no active bounds**; a view is one
   with at least one active bound. `GetViewBetween` always activates both, so
   in practice a view has both. The inactive-bound representation is retained
   so a future `Head()`/`Tail()` (which .NET's `_lBoundActive`/`_uBoundActive`
   also anticipates) needs no representation change.
4. **A view never owns a distinct copy of any element**, so there is no
   synchronisation problem, no staleness window, and no reconciliation step.
5. **Destroying an owning set does not destroy the elements** while a view or
   iterator survives. The state is simply no longer reachable through that
   object. This is well-defined and matches .NET. It also means an *orphaned*
   state can exist — reachable only through views — which is correct, not a
   leak: measured leak-free at 100k elements (§15).
6. **There are no ownership cycles.** `State` holds only elements; it never
   holds a `SortedSet<T>`. `shared_ptr` alone is sufficient; no `weak_ptr` is
   required anywhere.

---

## 13. Parent copy/move/assignment semantics

The single governing rule:

> **Copying preserves the object's role; assignment rebinds the assigned handle
> and never mutates state that another handle observes.**

| Operation | Behavior | Effect on existing views | Effect on existing iterators |
|---|---|---|---|
| Copy-construct an **owning set** | Deep clone into a fresh `State` (today's value semantics, preserved exactly) | none — they still observe the original state | none |
| Copy-construct a **view** | Shares the same `State` and copies the bounds → another handle onto the same range | none | none |
| Move-construct (either role) | Transfers the `shared_ptr` and bounds; the source becomes a **valid, empty owning set** (matching today's observed `parent-after-move-count=0`) | none — the `State` is untouched, so views keep working and now observe mutations made through the destination object | none |
| Copy-assign | Equivalent to destroying this handle and copy-constructing it from `other` (copy-and-move-assign idiom, self-assignment guarded) | **none** — views onto the previous state keep that state alive and keep observing it | iterators into the previous state remain **valid** and continue to observe its pre-assignment elements |
| Move-assign | Same rebinding, `noexcept`; the source becomes a valid empty owning set | none | as above |
| Destructor | Releases one reference to the `State` | none | none |

Two deliberate decisions inside that table:

**(a) Assignment rebinds rather than mutating in place.** The alternative —
overwriting the existing `State`'s contents so views follow the parent's new
value — was considered and rejected: it makes `a = b` silently change what an
unrelated view observes (action at a distance), and it has no .NET counterpart
to justify the surprise.

**(b) Iterators into a reassigned object keep working on the previous
contents** rather than fail-fasting. This is a strict improvement over today,
where the same code silently yields an element of the *new* tree (copy-assign)
or is an ASan-confirmed use-after-free (move-assign) — §3.2. Bumping the
outgoing state's version to force a fail-fast was considered and rejected: the
outgoing state's *contents* did not change, so it would spuriously invalidate
enumerations held by unrelated views of that same state. Fail-fast stays scoped
to genuine structural modification of the state being enumerated.

---

## 14. View copy/move/assignment semantics

- **Copying a view** yields another handle onto the same `State` with the same
  bounds. Mutating either handle is visible through the other and through the
  parent. This is the C++ equivalent of `var v2 = view;` in C#.
- **Moving a view** transfers the handle and bounds; the moved-from view
  becomes a valid, empty **owning** set (it is no longer a view), matching
  today's `view-after-move-count=0`.
- **Assigning to a view** rebinds it by the same rule as §13: `view = other`
  makes `view` behave like `other` (a handle if `other` is a view, an
  independent owning set if `other` is an owning set). It does **not** attempt
  to replace the viewed range's contents, and it never throws.
- **Nested and overlapping views** are ordinary additional handles. Overlapping
  views agree instantly because there is only one `State` (measured:
  `overlap-b-sees-a-removal=1`, `overlap-a-sees-b-add=1`).
- **A view's bounds are immutable** after construction. There is no setter, so
  a view cannot silently widen.

---

## 15. Bounds and exception matrix

Ordering is always `state_->data.key_comp()` — never `operator<` or
`operator>` spelled by hand. `cmp(a, b)` means "a orders before b".

| Operation | Condition | Result |
|---|---|---|
| `GetViewBetween(lower, upper)` on any object | `cmp(upper, lower)` | `ArgumentException("Must be less than or equal to upperValue.", "lowerValue")` — .NET's exact message; the base appends `(Parameter 'lowerValue')` exactly once (post-#1776) |
| `GetViewBetween` on a **view** | `cmp(lower, *lower_)` (widens the lower bound) | `ArgumentOutOfRangeException("lowerValue")` |
| `GetViewBetween` on a **view** | `cmp(*upper_, upper)` (widens the upper bound) | `ArgumentOutOfRangeException("upperValue")` |
| `GetViewBetween` | valid, including `lower == upper` and a range disjoint from the contents | a live view; a disjoint range is a valid **empty** view that still enforces its bounds |
| Checking order | invalid-range check **before** the widening checks, and both before any allocation | matches `SortedSet.cs:1510` / `TreeSubSet.cs:344` |
| `IsWithinRange(item)` | `lower_` active and `cmp(item, *lower_)` → false; `upper_` active and `cmp(*upper_, item)` → false; else true | inclusive both ends, `TreeSubSet.cs:112-122` |
| `Add(item)` on a **view** | `!IsWithinRange(item)` | `ArgumentOutOfRangeException("item")`, nothing written |
| `Add(item)` on a **view** | in range, absent | inserts into the shared state, bumps the version, returns `true` |
| `Add(item)` on a **view** | in range, present | returns `false`, version unchanged |
| `Add(item)` on an **owning set** | — | unchanged from today |
| `Remove(item)` on a **view** | `!IsWithinRange(item)` | returns `false`, **no throw**, parent untouched |
| `Remove(item)` on a **view** | in range | erases from the shared state, bumps the version |
| `Contains(item)` on a **view** | `!IsWithinRange(item)` | `false` |
| `Clear()` on a **view** | — | erases exactly `[rangeBegin, rangeEnd)` from the shared state; elements outside the bounds are untouched; version bumped only if something was erased |
| `Clear()` on an **owning set** | — | clears the shared state; version bumped only if it was non-empty |
| `getCountProperty()` on a **view** | — | `std::distance(rangeBegin(), rangeEnd())`, cached against the shared version (.NET's `_countVersion`) |
| `getMinProperty()` / `getMaxProperty()` on an empty view | — | `T{}` (`default(T)`), never throws |

Exception **ordering** on `GetViewBetween`: invalid range first, then lower
widening, then upper widening. No state is observed or allocated before the
first check.

---

## 16. Mutation propagation matrix

`P` = owning set, `V1`/`V2` = views over `P`'s state, `N` = a view nested
inside `V1`. `≡` means the same underlying `State`.

| Mutation | Seen by `P` | Seen by `V1` | Seen by `V2` | Seen by `N` | Invalidates outstanding iterators of |
|---|---|---|---|---|---|
| `P.Add(x)`, `x` in `V1`'s range | yes | yes | if in range | if in range | `P`, `V1`, `V2`, `N` — all |
| `P.Add(x)`, `x` outside every view range | yes | no | no | no | all (single shared version, exactly like .NET) |
| `P.Remove(x)` | yes | if was in range | if was in range | if was in range | all |
| `P.Clear()` | yes | yes (becomes empty) | yes | yes | all |
| `V1.Add(x)`, in range | yes | yes | if in range | if in range | all |
| `V1.Add(x)`, out of range | — | — | — | — | none (throws, no mutation) |
| `V1.Remove(x)`, in range | yes | yes | if was in range | if was in range | all |
| `V1.Remove(x)`, out of range | no | no | no | no | none (returns `false`) |
| `V1.Clear()` | yes, loses only `V1`'s range | yes | partially, where ranges overlap | partially | all |
| `N.Remove(x)` | yes | yes | if in range | yes | all |
| `P = other` (assignment) | `P` rebinds | **no change** | **no change** | **no change** | none (§13(b)) |
| `P` destroyed | — | no change, state survives | no change | no change | none |
| `P` moved | destination observes it | no change | no change | no change | none |

The "invalidates all" column is the deliberate .NET behavior: `SortedSet.cs`'s
enumerator compares against a single `version` shared through
`TreeSubSet.VersionCheckImpl`, so a mutation anywhere in the tree fail-fasts
every enumeration of the tree or any of its views.

---

## 17. Enumeration and versioning rules

1. `begin()` returns `Iterator(state_, rangeBegin(), rangeEnd())`; `end()`
   returns `Iterator(state_, rangeEnd(), rangeEnd())`. For an owning set these
   are `data.begin()` / `data.end()`, so today's behavior is unchanged.
2. `Iterator` captures `state_->version` at construction. `operator*`,
   `operator->`, and `operator++` compare it and throw
   `System::InvalidOperationException("Collection was modified; enumeration
   operation may not execute.")` on mismatch — the exact current message, which
   is also .NET's `SR.InvalidOperation_EnumFailedVersion`.
3. `operator==`/`operator!=` compare only the underlying `std::set` iterator, as
   today, so range-`for` termination is unchanged.
4. **One version counter per `State`.** Mutation through the parent invalidates
   view enumerations and vice versa (probe 4:
   `parent-mutation-during-view-iteration-throws=1`,
   `view-mutation-during-parent-iteration-throws=1`), closing the two
   divergences of probe 1 rows 16/16b.
5. A rejected duplicate `Add` and a `Remove` of an absent element do **not**
   bump the version, so they do not invalidate an in-flight enumeration. This
   preserves today's `guard-duplicate-add-fires=0` behavior.
6. Because `Iterator` holds `shared_ptr<const State>`, an iterator that outlives
   the object it came from is **safe and well-defined**, and assignment to that
   object detaches rather than dangles (§13(b)). Both are strict improvements
   over §3.2's measured behavior.
7. **Reverse iteration.** `Reverse()` does not exist in this port and is **not**
   added by ticket #1783 (§26). Should it ever be added, the rule is fixed
   here: reverse enumeration of a view walks `[rangeBegin, rangeEnd)` backwards
   from `std::prev(rangeEnd())`, under the same single-version guard.

---

## 18. Set-operation rules

1. On a view, every element mutation routes through the bounds-enforcing
   `Add`/`Remove`, so `UnionWith` with an out-of-range element throws
   `ArgumentOutOfRangeException("item")` and in-range elements write through —
   matching .NET, which routes the non-virtual base methods through virtual
   `Add`/`Remove`/`Contains`.
2. Read-only predicates (`IsSubsetOf`, `IsSupersetOf`, `IsProperSubsetOf`,
   `IsProperSupersetOf`, `SetEquals`, `Overlaps`) consider **only in-range
   elements** on both sides. `SetEquals`'s current `data_ == other.data_`
   whole-container comparison must become an element-wise comparison of the two
   ranges using `key_comp()` equivalence (`!cmp(a,b) && !cmp(b,a)`), not
   `operator==`.
3. **A new self-aliasing hazard is created by this design and must be handled.**
   Today's `ExceptWith`/`SymmetricExceptWith` guard `&other == this` — object
   identity. With shared state, `p.ExceptWith(viewOfP)` aliases at the *state*
   level while the two objects differ, so iterating `other`'s range while
   erasing from the same `std::set` is the exact iterator-invalidation UB
   ticket 324 fixed for `HashSet<T>`. The rule for #1783:

   - If `other.state_ == this->state_` **and** the bounds are equal, apply the
     existing identity shortcut (`Clear()` for `ExceptWith` and
     `SymmetricExceptWith`; no-op for `IntersectWith`).
   - Otherwise, whenever `other.state_ == this->state_`, materialize `other`'s
     in-range elements into a `std::vector<T>` **before** mutating.
   - The prototype takes the conservative route of always materializing
     `other`'s range first, which is correct for every case at the cost of one
     vector; #1783 may narrow it to the aliasing case only.

   Measured working: `except-with-own-view-parent-count=2` with elements 1 and
   5 retained, `symmetric-except-own-view-count=0`, `except-self-count=0`,
   `symmetric-except-self-count=0`.
4. `UnionWith`/`IntersectWith` on a view have no `VersionCheck()` analogue to
   call: the port re-reads the shared state on every access, so .NET's explicit
   `if (treeSubset != null) VersionCheck();` is unnecessary rather than omitted.

---

## 19. Thread-safety contract

Unchanged and explicitly restated: **`SortedSet<T>` offers no thread-safety
guarantee**, matching .NET (`ICollection.IsSynchronized => false`). Two
clarifications the shared representation makes necessary:

- A set and every view derived from it are **one collection** for concurrency
  purposes. Concurrent access to a parent and one of its views is exactly as
  unsafe as concurrent access to a single set.
- `shared_ptr` reference-count updates are atomic, so **lifetime** management is
  race-free even if contents are not. Copying or destroying handles on
  different threads does not corrupt the control block. Nothing stronger is
  claimed.

Because the design claims **no** concurrency property beyond the existing
contract, no ThreadSanitizer campaign is required for #1783 (§23).

---

## 20. Implementation phases

Ticket #1783 should land in this order, keeping the build green at each step:

1. **Representation.** Introduce `State`, move `data_`/`version_` into it, add
   `state_`, `lower_`, `upper_`, and the count cache. Implement the five
   special members. Keep `GetViewBetween` returning a materialized set for now.
   *Gate:* the existing 41 SortedSet tests pass unchanged.
2. **Range primitives.** `comparer()`, `rangeBegin()`, `rangeEnd()`,
   `IsWithinRange`, `getIsViewProperty()`. Route `getCountProperty`,
   `getMinProperty`, `getMaxProperty`, `ToVector`, `begin`, `end` through them.
   *Gate:* unchanged behavior for owning sets.
3. **`Iterator` rework.** `shared_ptr<const State>` plus a range end.
   *Gate:* the four `SortedSetVersionTrackingTests` cases pass unchanged.
4. **Live view.** Drop `const` from `GetViewBetween`, return the bounded
   handle, add the invalid-range and nested-widening validation with .NET's
   messages and parameter names.
   *Gate:* the three existing `GetViewBetween` tests pass unchanged (§4.5).
5. **Bounds-enforcing mutation.** View-aware `Add`, `Remove`, `Contains`,
   `Clear`.
6. **Set algebra.** View-aware set operations plus the strengthened
   shared-state self-aliasing guards of §18.
7. **`ToSortedSet()`** and the migration documentation.
8. **Documentation.** Replace the `@warning KNOWN DIVERGENCE` block; correct
   the stale comment at `Ticket1713VersionTrackingTests.cpp:109`; update the
   class doc-comment's element requirement; add `README.md` breaking-change
   guidance (taking care not to add a second markdown link to an
   already-linked document, which measurably adds a Doxygen warning).
9. **Permanent tests, consumer fixture, full gate** (§21–§23).

---

## 21. Permanent test plan

A new dedicated file
`modules/collections/tests/System/Collections/Generic/SortedSetLiveViewTests.cpp`
(the pattern established by `LinkedListNodeLifetimeTests.cpp` and
`CopyToBoundaryTests.cpp`), keeping the existing three `GetViewBetween` tests in
place unchanged as the "still works" baseline. Required cases, one assertion
group each:

1. **Parent → view**: in-range `Add` and `Remove` on the parent are visible in
   the view, including `Count`, `Min`, `Max`, and enumeration.
2. **View → parent**: in-range `Add` and `Remove` on the view are visible in
   the parent.
3. **Out-of-range invisibility**: a parent mutation outside the bounds changes
   nothing observable through the view.
4. **Out-of-range `Add`**: throws `ArgumentOutOfRangeException`, parameter name
   `item`, and writes nothing.
5. **Out-of-range `Remove`**: returns `false`, throws nothing, leaves the parent
   intact.
6. **Out-of-range `Contains`**: `false` even when the parent holds the element.
7. **`Clear` on a view**: removes exactly the range from the parent and nothing
   else.
8. **Bounds inclusivity**: both endpoints are members; `[x,x]` is a valid
   one-element range.
9. **Invalid range**: exact exception type, parameter name, and the .NET message
   (asserted as an exact string, since #1776 made the suffix single).
10. **Disjoint range**: a valid empty view that still enforces its bounds, and
    whose in-range `Add` writes through.
11. **Nested narrowing**: correct contents and write-through.
12. **Nested widening**: `ArgumentOutOfRangeException` with parameter name
    `lowerValue` and `upperValue` respectively.
13. **Overlapping views**: mutation through one is visible through the other.
14. **Owner destruction**: a view outliving its parent stays readable and
    mutable.
15. **Iterator outliving its set**: safe and yields the pre-existing contents.
16. **Parent copy** is a deep clone; the copy's mutations are invisible to the
    original and to its views.
17. **View copy** is a handle; its mutations are visible through the original.
18. **Parent move**: views follow the state; the moved-from object is a valid
    empty owning set.
19. **View move**: keeps viewness; the moved-from object is a valid empty owning
    set.
20. **Copy-assign and move-assign a parent that has views**: views and iterators
    are undisturbed, and the reassigned parent is detached.
21. **Enumeration invalidation, all three directions**: self, parent-during-view,
    view-during-parent.
22. **Rejected duplicate `Add` / absent `Remove`** do not invalidate an in-flight
    enumeration.
23. **Set algebra through a view**: `UnionWith` bounds violation;
    `IntersectWith`/`ExceptWith`/`SymmetricExceptWith` write-through sparing
    out-of-range elements.
24. **Shared-state self-aliasing**: `p.ExceptWith(viewOfP)`,
    `view.SymmetricExceptWith(sameView)`, plus the existing `ExceptWith(self)`
    and `SymmetricExceptWith(self)` regressions.
25. **Range-aware `SetEquals`/`Overlaps`/`IsSubsetOf`**.
26. **`ToSortedSet()`** produces a detached owning set.
27. **`getIsViewProperty()`** is false for every constructor and true for every
    `GetViewBetween` result, including nested.
28. **Element type with `operator<` only** instantiates `GetViewBetween` — a
    compile-level regression for §4.6.1, expressed as a file-scope
    instantiation plus a runtime assertion.
29. **Non-trivial element type** (`std::string`) across the whole matrix.
30. **Scale**: 100,000 elements, a 20,001-element view, cached `Count`,
    enumeration sum, and range `Clear`.

Existing suites that must keep passing unchanged: `SortedSetTests.*`,
`GenSortedSetTest.*`, `SortedSetVersionTrackingTests.*` (41 today).

### Rollback strategy

Each phase in §20 is independently revertable, and phases 1–3 are behavior
preserving. If a defect is found after phase 4, reverting phases 4–6 restores
snapshot semantics while keeping the safer representation — which is exactly
fallback **E′** of §8: the adjacent defects of §4.6 stay fixed, SR-AUD-361
reopens. Because the whole change is one header plus one test file, `git revert`
of the implementation commit is a complete rollback with no data or schema
migration.

---

## 22. Sanitizer plan

| Scenario | Sanitizers | Why |
|---|---|---|
| The full new test file | ASan + UBSan + LeakSanitizer | Ownership change; the state may be reachable only through views |
| Owner destroyed while views and iterators survive | ASan + LSan | The central lifetime claim of §12 |
| Parent copy / move / copy-assign / move-assign with live views and iterators | ASan + LSan | The §13 rebinding rules, and the §3.2 regressions |
| Iterator outliving its set | ASan | Replaces today's measured `stack-use-after-scope` |
| Nested and overlapping views mutating the same state | ASan + UBSan | Iterator invalidation across handles |
| Set algebra with shared-state aliasing | ASan | §18's new hazard — the ticket-324 failure mode |
| 100,000-element view: build, enumerate, range-`Clear`, teardown | ASan + LSan | Orphaned-state teardown at scale |
| **ThreadSanitizer** | **not required** | §19 claims no concurrency property beyond the existing contract, and the design adds no shared mutable global. Per the repository's rule, TSan is run only when such a property is claimed. |

Verify LeakSanitizer is actually active with a deliberate-leak self-test, as
ticket #1775 did — under this sandbox's `ptrace` policy LSan has previously
failed to initialise silently.

---

## 23. Consumer-fixture plan

Add `test/consumer/collections_sorted_set_view.cpp`, a standalone fixture
linking **only** `SharpRuntime::Collections.Core`, compiled
`-Wall -Wextra -Wpedantic -Werror` through the existing
`test/consumer/CMakeLists.txt` harness. It must construct a set, take a view,
mutate in both directions, take a nested view, outlive the parent, and exit 0 —
proving the header is self-sufficient and that a narrow consumer needs no new
component.

Add a companion **negative** fixture
`test/consumer/collections_sorted_set_view_negative.cpp`, following the
`collections_object_model_readonlydictionary_negative.cpp` precedent, asserting
that `GetViewBetween` on a `const SortedSet<T>&` **fails to compile** — the
visible face of the one approved signature change.

Run `scripts/check_selective_components.sh` with a repository-local `TMPDIR`
(the script's `mktemp` build trees otherwise land in `/tmp`, violating the
build policy).

---

## 24. Migration guidance

For consumers of this repository, including CNA and mobile-eggbert, **neither
of which is in this checkout and neither of which has been inspected**:

1. **Full rebuild is mandatory.** `SortedSet<T>`'s data members change, so
   object files compiled against the old header are layout-incompatible with
   new ones (§25).
2. **Audit every `GetViewBetween` call site** for a snapshot assumption. The
   dangerous patterns are: mutating the result and expecting the source to be
   unaffected; holding the result and expecting it to remain a point-in-time
   copy; and adding out-of-range elements to the result.
3. **To keep snapshot behavior deliberately**, replace
   `auto snap = set.GetViewBetween(a, b);`
   with
   `auto snap = set.GetViewBetween(a, b).ToSortedSet();`.
   This is the exact analogue of .NET's `new SortedSet<T>(view)`.
4. **A `const` set can no longer produce a view.** `constSet.GetViewBetween(...)`
   becomes a compile error naming the non-`const` overload. Take a non-`const`
   reference, or copy the set first and take the view from the copy.
5. **Copying the result of `GetViewBetween` copies the handle, not the
   elements.** `SortedSet<int> v2 = view;` gives a second handle. Use
   `ToSortedSet()` for an independent set.
6. **Enumerating a view now fail-fasts when the source is mutated**, matching
   .NET. Code that mutated the source while walking a view previously
   "worked"; it will now throw `InvalidOperationException`.
7. **Ticket #1773 is unrelated and stays blocked.** It covers the
   `ICollection::CopyTo` ABI sweep from ticket #1771 only. A downstream sweep
   for this change is a separate future item and is **not** created by ticket
   #1782.

---

## 25. Compatibility analysis

Separated into the five layers the ticket requires. Returning the same public
type is explicitly **not** treated as implying no impact.

### 25.1 Public source compatibility — ⚠️ one narrow break

- `GetViewBetween`'s parameter list and return type are **unchanged**.
- It **loses its `const` qualifier**. Every call on a non-`const` set still
  compiles; a call on a `const SortedSet<T>&` becomes a compile error. All
  three in-repository call sites use non-`const` sets (§4.5), so **no
  in-repository source break**. Downstream cannot be inspected.
- The five special members become user-declared. No call site changes, but the
  type stops being aggregate-initializable in any new way and copy semantics
  change for views (§14).
- Two additive members (`getIsViewProperty`, `ToSortedSet`) and one additive
  query (`IsWithinRange`); additions cannot break existing code.
- `SortedSet<T>` is **not** explicitly instantiated anywhere; it is a
  header-only class template in the `Collections.Core` `INTERFACE` target.

### 25.2 Binary symbol compatibility — ⚠️ mangled names change

Measured with `nm`/`c++filt` on `probe5_layout_symbols.o`
(`build-probe-sortedset/probe5_symbols_mangled.log`):

```
W _ZNK6System11Collections7Generic9SortedSetIiE14GetViewBetweenERKiS5_   (today, const)
W _ZN18SortedSetPrototype9SortedSetIiE14GetViewBetweenERKiS3_            (proposed, non-const)
```

The Itanium C++ ABI encodes cv-qualification of the implicit object parameter,
so dropping `const` changes `_ZNK…` to `_ZN…`. Unlike ticket #1780's `Empty()`
— whose mangled name was byte-identical because return types are not encoded —
**this is a genuine mangled-name change**. It is not a link break in practice:
the class is header-only with weak/COMDAT emission per translation unit and the
`Collections.Core` target produces no archive, so every translation unit that
uses the member emits the new symbol when recompiled. It *is* a link break for
any pre-built object file that references the old symbol.

### 25.3 Object-layout compatibility — ❌ broken

Measured (`build-probe-sortedset/probe5_layout_symbols.log`, GCC 14.2.0,
x86-64):

| Type | Today | Proposed |
|---|---:|---:|
| `sizeof(SortedSet<int>)` | 56 | **40** |
| `sizeof(SortedSet<std::string>)` | 56 | **104** |
| `sizeof(SortedSet<int>::Iterator)` | 24 | **40** |
| `alignof` | 8 | 8 |
| `is_polymorphic` | 0 | 0 (unchanged — no vptr added) |
| `is_trivially_copyable` | 0 | 0 |
| `is_nothrow_move_constructible` | 1 | 1 (preserved) |
| `is_copy_assignable` | 1 | 1 (preserved) |

The size for `int` **shrinks** (the 48-byte inline `std::set` is replaced by a
16-byte `shared_ptr`) and for `std::string` **grows** (two
`std::optional<std::string>` bounds cost 80 bytes). The growth scales with
`sizeof(T)`; storing the bounds behind a single `shared_ptr<const Bounds>`
would make the size `T`-independent at the cost of one allocation per view and
one indirection per bounds check. That optimization is **deferred, not
selected**: views are comparatively rare, and allocation-free bounds checks are
on every hot path.

Any object file compiled against the old header is layout-incompatible with one
compiled against the new header. Mixing them is an ODR violation with no
diagnostic.

### 25.4 Semantic compatibility — ❌ intentionally broken

This is the point of the change. Existing code that relies on snapshot
independence compiles unchanged and behaves differently. §24 lists the
patterns; `ToSortedSet()` is the documented replacement. **No in-repository
caller relies on snapshot independence** — the only three call sites read the
view immediately and assert nothing that changes (§4.5).

### 25.5 Practical rebuild recommendation

Full clean rebuild of every consumer, plus the §24 call-site audit. This is the
same rebuild expectation ticket #1771's ABI break already established for this
release line; consumers still on the pre-#1771 revision must rebuild anyway.

---

## 26. Performance implications and explicit exclusions

### Performance

| Operation | Today | Proposed |
|---|---|---|
| `Add`/`Remove`/`Contains` on an owning set | O(log n) | O(log n) + one pointer indirection |
| `Add`/`Remove`/`Contains` on a view | O(log k) on the copy | O(log n) + 1–2 comparator calls |
| `getCountProperty()` on an owning set | O(1) | O(1) |
| `getCountProperty()` on a view | O(1) on the copy | **O(k)** on first call per version, then cached (matches .NET's `_countVersion`) |
| `getMinProperty()`/`getMaxProperty()` on a view | O(1) on the copy | O(log n) — better than .NET's tree walk |
| `GetViewBetween` itself | **O(k log k)** — copies every in-range element | **O(1)** — no traversal, no allocation of elements |
| Enumerating a view | O(k) | O(log n) to position + O(k) |
| Memory per view | k elements | 1 `shared_ptr` + 2 bounds |
| Construction of an owning set | no allocation beyond the tree | **+1 control-block allocation** |

Net: `GetViewBetween` goes from O(k log k) with k allocations to O(1) with
none; the only regression is one heap allocation per owning set and O(k) for
the first `Count` of a view after each mutation. Measured at scale: a
100,000-element set, a 20,001-element view, cached `Count`, full enumeration,
and range `Clear` all run clean under ASan+UBSan+LSan.

### Explicit exclusions (not done by #1783)

- **`Reverse()`** is not added. It is absent from the port today; adding it is
  unrelated API breadth. §17.7 fixes its semantics if it is ever added.
- **`IComparer<T>` construction** is not added. Ordering stays `std::less<T>`;
  "comparer propagation" is satisfied because the view uses the parent's
  `std::set` and therefore its `key_comp()`.
- **Serialization hooks** are not added (permanent project deviation).
- **`CopyTo`** is not added; `ToVector()` remains the copy-out route. The
  ticket #1771/#1774 `ICollection` copy boundary is untouched.
- **Collection interfaces** (`ISet<T>`, `ICollection<T>`, `IEnumerable<T>`) are
  not implemented. Doing so would require virtual members, which §7.7 rules out.
- **`SortedDictionary`, `SortedList`, `ImmutableSortedSet`, `HashSet`** are not
  touched. `ImmutableSortedSet` in particular does not use this header.
- **SR-AUD-362 production behavior** and its conservative correction note are
  not touched.
- **Ticket #1773** stays blocked and untouched; CNA and mobile-eggbert are not
  inspected.
- **No new third-party dependency**, no module split, no CI matrix change, no
  repository-wide formatting.

---

## 27. Risks

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| 1 | The `const` removal breaks an unknown downstream call site | Medium | It is a **compile error naming the replacement**, never a silent behavior change — the same rationale ticket #1771 used to refuse a throwing shim. Gated on explicit approval (§28). |
| 2 | Downstream code silently relies on snapshot independence | **High** | The one genuinely silent risk. No compile error is possible. Mitigated by §24's call-site audit instruction, `ToSortedSet()`, and a `README.md` breaking-change entry — not eliminated. |
| 3 | Role-dependent copy semantics surprise a reader | Medium | Stated as one rule (§13), exposed by `getIsViewProperty()`, and pinned by tests 16–19 (§21). |
| 4 | The new shared-state self-aliasing hazard in set algebra (§18) | Medium | Explicitly designed, prototyped, and covered by test 24. It is the ticket-324 failure mode in a new guise; missing it would be a real regression. |
| 5 | `sizeof(SortedSet<std::string>)` grows 56 → 104 | Low | Measured, documented, and reversible via the deferred `shared_ptr<Bounds>` variant (§25.3). |
| 6 | One extra heap allocation per owning set | Low | Measured; negligible next to the `std::set` node allocations that follow. |
| 7 | Orphaned state (reachable only through views) looks like a leak | Low | It is correct behavior; LeakSanitizer coverage at 100k elements proves it is released (§22). |
| 8 | The lazy `Count` cache goes stale under an unforeseen mutation path | Medium | The cache is keyed on the single shared version, which every mutating path bumps; test 30 exercises it across an invalidation. |
| 9 | Scope creep into `Reverse()`, `IComparer<T>`, or the collection interfaces | Medium | Explicitly excluded in §26. |
| 10 | Iterators surviving reassignment of their set observe pre-assignment data | Low | Deliberate (§13(b)), well-defined, and strictly better than today's silent-wrong-value / use-after-free. Documented. |

---

## 28. Proposed implementation ticket

**#1783 — `REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L, status `blocked`.**

*Title:* Implement live SortedSet GetViewBetween views.
*Finding:* SR-AUD-361.

Size **L**, not M: one header is substantially rewritten, ~30 permanent
regressions and two consumer fixtures are added, and the change is semantically
breaking — comparable to ticket #1769 (`REMED-COLL-LINKED-NODE`, size L), which
made the same class of ownership change to `LinkedListNode<T>`. Priority stays
**P2**, inherited from SR-AUD-361's `medium` severity and matching the P2 used
for the other medium Collections contract findings (#1778, #1779, #1780).

### Exact approval required

> **Approve removing the `const` qualifier from
> `System::Collections::Generic::SortedSet<T>::GetViewBetween(const T&, const T&)`,
> and approve the accompanying semantic change from a detached snapshot to a
> live, bidirectionally write-through bounded view, together with the
> `SortedSet<T>` object-layout change (`sizeof(SortedSet<int>)` 56 → 40,
> `sizeof(SortedSet<std::string>)` 56 → 104) that requires every consumer,
> including CNA and mobile-eggbert, to be rebuilt.**

This is the same approval category as ticket #1770/#1771's
`ICollection::CopyTo` removal and ticket #1779/#1780's `Empty()` return-type
change. Ticket #1783 must not begin until it is granted.

If the approval is refused, the fallback is **E′** (§8): keep snapshot
semantics, fix the four adjacent defects of §4.6, and sharpen the header
documentation. E′ needs no approval — it changes no signature and no layout —
but it closes none of SR-AUD-361, which would stay `confirmed` indefinitely.

### In scope for #1783

Everything in §11 and §20, the permanent tests of §21, the sanitizer plan of
§22, the consumer fixtures of §23, the documentation updates of §20.8, and the
four adjacent defects of §4.6 (which live inside the rewritten surface and are
fixed as a consequence of the design, not as separate work).

### Out of scope for #1783

Everything in §26.

### Closure gate for #1783

Warning-free `cmake --build build --parallel 4`;
`scripts/run_component_tests.sh build` with no regression below the 13,022-test
floor; `python3 scripts/validate_module_boundaries.py --root .` at 41 modules /
90 edges; `python3 test/validate_module_boundaries_test.py`;
`python3 scripts/generate_component_catalog.py --check`;
`python3 scripts/db_consistency_check.py --db plan.sqlite3`; `git diff --check`;
`scripts/check_doxygen_warnings.sh` at or below 1,942;
`scripts/check_selective_components.sh` with a repository-local `TMPDIR`; and a
network-permitted `scripts/local_ci_check.sh build`.

---

## 29. Probe index

Every command and its result, for reproduction. All artifacts are in the
gitignored `build-probe-sortedset/` tree; none is a tracked file.

| Probe | Command | Result |
|---|---|---|
| `probe1_current_behavior.cpp` | `build.sh probe1_current_behavior asan` then `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ./probe1_current_behavior` | exit 0, `failures=0`, no diagnostic, no leak. Full pre-fix matrix → §3.1 |
| `probe2_iterator_lifetime.cpp` | `build.sh probe2_iterator_lifetime asan`; then `safe`, `copy-assign`, `move-assign`, `outlive` | `safe` exit 0; `copy-assign` silently wrong value 60, **no diagnostic**; `move-assign` **ASan heap-use-after-free**; `outlive` **ASan stack-use-after-scope in `checkVersion()`** → §3.2 |
| `probe3_comparer_requirement.cpp` | `build.sh probe3_comparer_requirement werror` and again with `-DSORTEDSET_PROBE_INSTANTIATE_VIEW` | without: compiles `-Werror`, runs, exit 0. with: **two `no match for 'operator>'` errors** at `SortedSet.hpp:297` and `:300` → §3.3 |
| `SortedSetPrototype.hpp` + `probe4_prototype.cpp` | `build.sh probe4_prototype asan -I build-probe-sortedset` then `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ./probe4_prototype` | exit 0, **`failures=0`**, no diagnostic, no leak, including the 100,000-element scale case. Every §15/§16/§17/§18 rule verified → §11–§18 |
| `probe5_layout_symbols.cpp` | `build.sh probe5_layout_symbols werror -I build-probe-sortedset`; then `g++ -c … && nm -C` | Layout and `is_*` trait table of §25.3; mangled-name comparison of §25.2 |
| `probe6_public_header_standalone.cpp` | `build.sh probe6_public_header_standalone werror` | The production header compiles standalone under `-Wall -Wextra -Wpedantic -Werror` and runs, exit 0 — the baseline #1783 must preserve |

The prototype found one real design defect during development that the
implementation must avoid: `std::set::key_comp()` returns **by value**, so
binding it to a `const` reference is `-Wreturn-local-addr` (a reference to a
temporary). Recorded inline in §11.
