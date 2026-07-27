# Audit: `modules/collections/include/System/Collections/Generic/LinkedList.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-357 — high — copied LinkedListNode handles retain dangling list iterators after node or owner destruction

`LinkedListNode<T>` is a copyable pair of a raw `std::list<T>*` and native iterator.  Removing the node or destroying its owner leaves any copied handle truthy and dereferenceable.  A direct ASan/UBSan probe retains `getFirstProperty()`, destroys the `LinkedList<int>`, then reads the node; ASan reports heap-use-after-free in the public accessor.  The raw-list representation also cannot preserve .NET's independently allocated/detached node lifetime model.

## Missing assertions and diagnostics

- Tests do not retain nodes across Remove/Clear/owner destruction or assert a deterministic detached-node result.
- Ownership/liveness state must be observable before any node value, next, previous, comparison, or implicit conversion dereference.

## Post-audit remediation of SR-AUD-357 (tickets #1768/#1769, 2026-07-27)

**Status: REMEDIATED.** The original audit evidence above is retained
unchanged; this section records the bounded repair that followed it.

Design ticket #1768 recorded the lifetime contract in
`docs/LinkedListNodeLifetime.md` after comparing the C++ surface with the local
current-.NET `LinkedList.cs` and `Strings.resx`. Implementation ticket #1769
replaced the copyable `std::list<T>*`/iterator pair with independently
allocated, reference-counted node objects. A `LinkedListNode<T>` handle is now
a null handle, a detached node, or an attached node. `Remove`, `Clear`, removal
through another copied handle, and destruction of the owning `LinkedList<T>`
all detach the affected nodes while retaining their values; the destructor runs
the same iterative detaching walk as `Clear`, so no retained handle can reach
freed storage. Null-handle value access throws
`System::NullReferenceException` instead of dereferencing.

The repair also supplied the previously impossible .NET surface: the explicit
detached-node constructor, the `Value` setter, the `List` accessor, node
identity comparison, and the four existing-node `AddFirst`/`AddLast`/
`AddBefore`/`AddAfter` overloads with `validateNewNode` rejecting an already
attached node using the exact .NET message. `begin()`/`end()` migrated to a
bidirectional `LinkedList<T>::iterator` with the same range-`for`,
`std::ranges`, algorithm, and invalidation behaviour.

Evidence: the 49-case permanent `LinkedListNodeLifetimeTests` suite covers the
missing assertions listed above — retained handles across `Remove`, `Clear`,
and owner destruction, and an observable ownership/liveness state before any
value, next, previous, comparison, or conversion access. The direct ASan/UBSan
reproduction that produced the heap-use-after-free above now reports
`failures=0` with no sanitizer diagnostic, including a 200,000-node teardown
and leak checking. `SharpRuntimeTests_Collections_Core` passed 1,484/1,484 and
the network-permitted `scripts/local_ci_check.sh build` gate passed
12,743/12,743 tests across 37 executables with zero warnings.

## Related enumerator remediation

Ticket #1767 remediated this file's SR-AUD-356 enumerator-lifecycle aspect:
`Current` now uses the shared before-start/after-end guard, and `Reset`
restores the native iterator to the beginning. The permanent family regression
and direct ASan/UBSan replacement probe pass. That ticket deliberately did not
change `LinkedListNode` ownership; SR-AUD-357 was remediated separately by the
design-first tickets #1768/#1769 recorded above. The enumerator's lifecycle
guard, `Reset` behaviour, and version check are unchanged by that repair — only
its cursor type moved from a native `std::list` iterator to a node pointer.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
