# Audit: `modules/collections/include/System/Collections/Generic/IEnumerator.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-356 — high — collection enumerators dereference invalid Current states instead of throwing

At audit time, the generic enumerator facade forwarded `Current()` without a state precondition.  Multiple implementations indexed a vector/deque/map/list with their initial or exhausted cursor: `List`, `Queue`, `Stack`, `SortedList`, `LinkedList`, ObjectModel `Collection`/ `ReadOnlyCollection`, and the ConcurrentBag/Queue/Stack snapshot enumerators.  The direct ASan/UBSan probe called `List<int>::GetEnumerator()->Current()` before `MoveNext()` and got a heap-buffer-overflow four bytes before the backing allocation rather than `InvalidOperationException`.  The same unguarded shapes were reachable after enumeration completed.

## Missing assertions and diagnostics

- Tests cover successful iteration but do not call `Current()` before the first or after the final `MoveNext()` across the affected collection families.
- Each enumerator needs a common lifecycle diagnostic identifying before-start, ended, or invalidated state before exposing native storage.

## Remediation

**REMEDIATED by ticket #1767 on 2026-07-27.** A shared `EnumeratorState`
rejects before-start and after-end `Current` access before native storage is
touched in all ten affected implementations. Permanent regressions exercise
typed and non-generic Current bridges, normal iteration, repeated exhaustion,
and Reset. Managed generic enumerators can return a cached default outside a
valid position, but this C++ API returns `const T&`; throwing follows the
non-generic invalid-state contract and avoids inventing an unsafe reference.
The regressions pass 13/13; Collections.Core passes 1,435/1,435, the direct
ASan/UBSan replacement probe reports zero failures, and the
network-permitted repository gate passes 12,694/12,694.

## Final assessment

AUDITED. SR-AUD-356 was confirmed with reproducible evidence and is now
REMEDIATED; the original evidence is retained above.

## Post-remediation follow-up: ticket #1787 (2026-07-28)

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) repaired
the mutation counter this file's fail-fast enumerator snapshots. It carries **no
`SR-AUD-*` identifier** — the numbering is frozen at 364 and the pattern was
found during remediation, by ticket #1786's own inventory — and it reopens no
finding here. The original evidence above is retained unchanged.

The counter is now `System::Collections::detail::MutationCounter` (64-bit unsigned) and
the enumerator snapshots `detail::MutationVersion`. `sizeof`, `alignof`, and the
counter's own byte offset are all unchanged — measured, not assumed — because the
widening consumed padding the type already had at that position.

Repository-wide, three defect classes were reproduced against the committed
pre-fix headers before anything changed
(gitignored `build-probe-collversion/probe2_defects.cpp`): fourteen UBSan
signed-integer-overflow reports at `++version_`, fifteen 2^32
snapshot-reuse (ABA) reproductions, and — recorded in neither #1786's nor
#1787's description — an assignment defect that transplanted the *source's*
counter into the destination and needed no overflow at all, with six
AddressSanitizer `heap-use-after-free`/`heap-buffer-overflow` reproductions. The
full record, including the .NET comparison and the per-type layout measurements,
is `docs/CollectionVersionCounterSweep.md`.


## Newly discovered defect: ticket #1792 (2026-07-28)

Found by design ticket #1790's mutable-access inventory and **deliberately not
absorbed into it**, because it belongs to `IEnumerator<T>` and therefore reaches
**every** collection in the repository, not `List<T>`. It carries **no
`SR-AUD-*` identifier** — the numbering is frozen at 364 and this was found
during remediation, not during the audit — and it reopens no finding here. The
original evidence above is retained unchanged. **Nothing was changed in this
file**; ticket #1792 is `todo` and inactive.

```cpp
void* getCurrentProperty() const override {
    return const_cast<T*>(&Current());
}
```

`Current()` returns `const T&` precisely so that an enumerator cannot be used to
mutate what it is walking. This bridge to the non-generic
`System::Collections::IEnumerator` casts that constness away and publishes a
`void*` to the live element on a **public** interface.

Reproduced in the gitignored `build-probe-listindexer/probe1_escape-routes.log`,
with the owning collection's private counter read directly:

```
enum void* write counter 0 -> 0  value=88  fail-fast=0
```

The write landed in the collection, the mutation counter never moved, and the
outstanding enumerator's fail-fast guard stayed silent — so a consumer holding
only the non-generic interface can mutate a collection mid-enumeration,
untracked, for any collection whose enumerator derives from
`Generic::IEnumerator<T>`.

Ticket **#1792** (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M) records
it. Its first required step is a repository-wide call-site inventory by the same
compile-against-a-shim method #1790 used, because `getCurrentProperty()` is a
public virtual and a grep would understate it — as one did for `IList<T>`'s
implementers during #1790. Context:
`docs/ListIndexerVersioningDesign.md` sections 4.1 (route 6), 5.2, and 27.3.

## Design ticket #1792 closed: design-complete (2026-07-28)

The defect recorded in the section above was designed under ticket **#1792**
(`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M), which is now **done as
a design ticket**. It carries **no `SR-AUD-*` identifier** and **reopens no
finding**: SR-AUD-356 stays `remediated`, because this defect is about what the
accessor *returns* and that finding is about the before-start/after-end lifecycle
guard, every #1767 regression of which still passes unmodified. **Nothing in this
file was changed.** The original evidence above is retained unchanged. The
durable record is `docs/IEnumeratorCurrentSafetyDesign.md`.

**Selected architecture:** the non-generic
`System::Collections::IEnumerator::getCurrentProperty()` returns **`std::any` by
value** — the direct C++ counterpart of .NET's `object IEnumerator.Current`,
which returns a value, boxes value types, and hands out no pointer. The typed
`Current()` in this file stays `const T&`, and the bridge below it converts
rather than aliases:

```cpp
std::any getCurrentProperty() const override {
    if constexpr (std::is_copy_constructible_v<T>) {
        return std::any(Current());
    } else {
        throw System::NotSupportedException(
            "The element type cannot be boxed; use the typed Current() accessor.");
    }
}
```

The `NotSupportedException` path is .NET's own documented answer for an element
type that cannot be boxed (`Generic/IEnumerator.cs`, the `ref struct` note).

**Four corrections to the note above**, all against this record's convenience:

1. **"reaches every collection in the repository" is wrong.**
   `Dictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, and `SortedDictionary<K,V>`
   implement no `IEnumerator` at all — they expose STL-style version-checked
   iterators, and none of them appears anywhere in the measured sweep. The reach
   is thirteen generic implementations plus eight non-generic ones, plus two
   hand-written test-local implementers.
2. **This file's `const_cast` is not the only one.** Four more live outside it —
   `ArrayList.hpp:755`, `Hashtable.hpp:475`, and `ListDictionaryInternal.hpp:77`
   and `:116` — so repairing only this bridge would leave every one of them.
3. **It is six defect classes, not one**, and they are not closed by the same
   measure. `const void*` closes const-correctness alone: the probe performs the
   one-line `const_cast` a consumer writes, and the write lands.
4. **The ABI is the dangerous half.** `void*`, `const void*`, and `std::any` all
   produce the byte-identical mangled name, while `this` moves from `%rdi` to
   `%rsi` under `std::any`'s sret return — a partially rebuilt consumer links
   with no diagnostic and corrupts memory.

Implementation is ticket **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, size L), opened
**blocked** on the three-part approval in design section 33 and deliberately not
begun. Permanent regressions covering today's behaviour are in
`modules/collections/tests/System/Collections/EnumeratorCurrentSafetyTests.cpp`.

## Implementation ticket #1793 closed: implemented (2026-07-28)

The design above landed under ticket **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, size L), on local branch
`feature/remediation-coll-ienumerator-current-safety`, after the user granted
design section 33's three-part approval explicitly and scoped to that ticket.
**No new `SR-AUD-*` identifier; SR-AUD-356 stays `remediated`.** The evidence in
every section above is retained unchanged.

**This file changed.** `Current()` is unchanged at `const T&`, with its validity
window written into the doc-comment for the first time. The bridge below it is
now:

```cpp
[[nodiscard]] std::any getCurrentProperty() const override {
    if constexpr (std::is_copy_constructible_v<T>) {
        return std::any(Current());
    } else {
        (void)Current();
        throw System::NotSupportedException(
            "The element type cannot be boxed; use the typed Current() accessor.");
    }
}
```

**The `(void)Current();` is a correction to the design's own sketch**, which
threw directly. `if constexpr` discards the whole else-branch's alternative, so
without that call the only use of `Current()` disappears and a before-start or
after-end read on a move-only `T` reported `NotSupportedException` where the
pre-#1793 bridge reported `InvalidOperationException` — silently converting an
existing exception path, which design section 18's ordering rule forbids. Caught
by `EnumeratorCurrentSafety.MoveOnlyStateMachineStillPrecedesTheBoxingRefusal`,
not by review.

Two further implementation findings are recorded in
`docs/IEnumeratorCurrentSafetyDesign.md` section 34.3:
`Generic::List<std::any>` cannot be instantiated at all, because `std::any` is
not equality-comparable and `List<T>`'s `Contains`/`IndexOf` need `operator==`;
and `std::any(Current())` for `T = std::any` selects `std::any`'s **copy**
constructor rather than its value-forwarding one, so the box is never nested.

The nine `EnumeratorCurrentDivergence` cases were **flipped, not deleted**, and
`SharpRuntimeTests_Collections_Core` went 2,208 → **2,229**. A clean full
rebuild against the changed virtual ABI passes **13,515 tests across 37
executables**. Object layout is `diff`-identical to the stored baseline; the
mangled name is byte-identical and the vtable slot unchanged at offset `0x20`;
an isolated stale-object probe **linked with zero diagnostics** and then took a
SEGV, which is why README.md states that a full consumer rebuild is mandatory.

The typed `Current()` reference hazard is **not** closed and is not claimed to
be: `&Current()` retained across a mutation is still a reproduced
use-after-free. Closing it would need a by-value `Current()`, which makes a
move-only `T` uninstantiable. The header now states the window and the
limitation.
