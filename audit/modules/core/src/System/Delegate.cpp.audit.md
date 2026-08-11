# Audit: `modules/core/src/System/Delegate.cpp`

## Metadata

- Audit status: AUDITED (166-line implementation, fully read).
- Validation: direct delegate suites passed 70/70 and the standalone probe
  completed on 2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-delegate-audit-probe` is compiled
  against `build/libsharp_runtime_core.a` and prints the four values recorded
  in the companion declaration audit.
- Reference basis: local .NET `Delegate.cs:18-51,158-183` and CoreCLR
  `MulticastDelegate.CoreCLR.cs:212-292,315-390`.

## SR-AUD-118 — medium — Delegate composition accepts different concrete delegate types and converts every combined/removal result into base Delegate

`Combine` neither compares dynamic types nor asks a concrete delegate to create
the result; it always constructs `new Delegate(MulticastTag{}, ...)`
(`Delegate.cpp:106-122`).  Multicast `Remove` uses the same base construction
(`:143-150`).  Current .NET rejects mismatched delegate types before combining
or removing, and `MulticastDelegate.CombineImpl` creates the same runtime
delegate type.  The probe combines `MulticastDelegate` objects and prints
`combined_dynamic_type_preserved=0`; it also combines a `MulticastDelegate`
with a distinct C++ subclass and prints `mixed_types_accepted=1` rather than
an argument error.

The 70 green direct tests verify only target invocation and list lengths.  The
MulticastDelegate fixture even labels Combine as producing a shared base
Delegate, so it does not assert the required dynamic type or mismatch failure.

### Status: CONFIRMED (design-complete) — #2270 review, #2271 `needs_user` (2026-08-11)

Reproduced exactly as filed, against the current tree rather than the audit's
own probe (which lived under `/tmp` and is gone):
`combined_dynamic_type_preserved=0`, `mixed_types_accepted=1`,
`remove_result_dynamic_type` also base `Delegate`. No premise needed
correction.

**The finding's two halves cannot be separated, and that is measured.** A
same-type guard alone is not implementable: `Combine(Combine(a,b),c)` presents a
base `Delegate` and a `MulticastDelegate` to its second step
(`chained_step2_operand_types_equal=0`), which is the exact shape of two
currently green fixtures, so `typeid` equality would reject a chain the port
supports today. A type check is coherent only once results preserve their type.

**Two independent gates, neither of which this session may pass.** (1) A
*representation* decision: preserving the concrete type needs either a new
virtual factory on a public polymorphic base — a vtable change, hence an ABI
break, plus a silent-degradation contract for any subclass that does not
override it — or the reuse of the existing public virtual `Clone()`, which makes
`Combine` invoke user-overridable code and copies the left operand's subclass
state into the result. (2) An *approval*: rejecting a mismatch makes
`Delegate::Combine` calls that succeed today throw `ArgumentException`, the same
tightening class as SR-AUD-178/#2269 and the date/time parsers.

One adjacent consequence is carried by #2271 rather than by a separate ticket,
because it needs the identical decision and is not separately implementable:
`Equals` is type-blind too (`cross_type_single_equals=1`), where .NET's
`MulticastDelegate.Equals` begins with `InternalEqualTypes`. It is **not**
absorbed into this finding's frozen text.

SR-AUD-119 and SR-AUD-120 were reviewed with this finding and are **not** part of
its cause; see their own sections and `docs/CoreDelegateCompositionContractPlan.md`
§1. No CCF is minted — a shared file is adjacency, not causation.

## SR-AUD-119 — medium — Delegate equality compares multicast entries by shared-pointer identity instead of delegate equality

Single plain function pointers receive a special value-equality path
(`Delegate.cpp:34-48`), but multicast lists compare only `shared_ptr::get()`
values (`:50-53`) and hash those addresses (`:60-67`).  CoreCLR's
`EqualInvocationLists` calls each entry's `Equals`, so independently-created
but equal delegates form equal invocation lists.  The probe combines two
separate `[First, Second]` lists and prints `equal_function_lists_equal=0`.

This also leaves the eventual equality/hash repair coupled: pointer-folded
hashes will not represent the required value-equal lists.  Existing tests only
compare two lists built from the exact same entry pointers.

### Status: REMEDIATED (#2270 review, #2272 implementation, 2026-08-11)

Reproduced exactly: two independently built `[First, Second]` lists compared
unequal and hashed differently. The finding's diagnosis is confirmed and refined
by one measurement it does not state — the **entries already answered `Equals`
correctly** (`entry_pairs_equal=1 1`). The value-equality path for a single plain
function pointer has existed since ticket 345; only the list loop declined to use
it. This is therefore a wrong comparison *primitive*, not a missing policy, and
the repair is one loop body.

`Equals` now compares `la[i]` against `lb[i]` with `Equals`, matching CoreCLR's
`EqualInvocationLists`. It is a **strict widening**: pointer-identical entries
short-circuit through `Equals`'s own `this == &other` fast path, so no pair equal
today can become unequal. Measured after: `equal_function_lists_equal=1`,
`equal_function_lists_hash_equal=1`, while `different_lists_equal`,
`reordered_lists_equal` and `lambda_lists_equal` all stay `0` and
`same_pointer_lists_equal` stays `1`.

The finding's own coupling note is honoured in the same change: `GetHashCode`
folds each entry's hash code instead of its address, or the widened equality
would have broken the hash contract the single-target path already honours. The
mixing function is deliberately untouched — only the hashed value moved — and an
entry with no comparable target still hashes as `std::hash<const Delegate*>{}
(this)`, exactly the address the old code folded, so **lambda-entry multicast
hashes are unchanged**.

Not fixed here, by design: `Equals` still does not compare concrete types
(.NET's `InternalEqualTypes`). That belongs to SR-AUD-118/#2271. It is
unreachable across types in practice regardless — every multicast in this port is
produced by `Combine`/`Remove`, both of which yield dynamic type
`System::Delegate`.

+13 tests (`DelegateInvocationListEqualityTests`), covering the widening, the
five answers that must not move, the closure limitation, and the subclass. Two
mutations, both caught. No signature, layout, vtable, `noexcept` or symbol
change; the repair is confined to `Delegate.cpp` bodies plus two header
doc-comments that had described the pointer-identity contract this change
replaces.

## SR-AUD-120 — medium — Delegate.Remove cannot remove a multicast value's last matching invocation-list subsequence

For a multicast source, `Remove` scans individual entries and compares each to
the whole `value` object (`Delegate.cpp:143-151`).  It never searches a
multi-entry value as a trailing sequence.  Current CoreCLR scans backward over
the candidate invocation-list range and removes the last equal subsequence
(`MulticastDelegate.CoreCLR.cs:369-390`).  The probe removes `[First, Second]`
from `[First, Second, First, Second]` and prints
`remove_multicast_subsequence_size=4` instead of the expected remaining size
two; `RemoveAll` inherits the fault.

### Status: REMEDIATED (#2270 review, #2273 implementation, 2026-08-11)

Reproduced exactly, and the *reason* the entry loop cannot match was measured
rather than assumed: a single-target entry compared against a multi-entry value
is rejected by `Equals` on length alone, so the answer is always "not found".
That also fixes the repair's regression surface at **empty** — every reachable
multicast-value removal shape returns the source unchanged today, including
`remove_same_pointer_subsequence_size=4`, where the source's entries are the
*identical* `shared_ptr`s the value holds. The new branch can therefore only fire
where the current answer is "unchanged": **no existing outcome can change.**

`Remove` now branches on whether the value is itself multicast, as
`MulticastDelegate.RemoveImpl` does. For a multicast value it scans candidate
start positions from the last one backwards and deletes the **last** matching
contiguous subsequence. Measured after: `remove_multicast_subsequence_size` and
`remove_same_pointer_subsequence_size` both `4 → 2`, `remove_entire_list_null`
`0 → 1`, `remove_leaving_one_size/is_a` `3/0 → 1/1`, and
`removeall_subsequence_size/null` `4/0 → 0/1`, so `RemoveAll` inherits the fix
exactly as this finding says it inherits the fault. `longer_value_unchanged` and
the whole single-target path are unchanged.

Boundaries: a value longer than the source leaves the source unchanged; emptying
the list returns `nullptr`; a remainder of one entry is returned as that entry
itself, the convention the existing single-entry removal path already used. The
`vl.size() > sl.size()` guard is load-bearing — without it the unsigned start
index underflows — and its boundary is pinned by a mutation rather than by
removing it, which would be undefined behaviour rather than a measurement.

**Independent of SR-AUD-118.** The multi-entry result is built with the same
`new Delegate(MulticastTag{}, …)` the existing single-entry path already uses, so
this adds no new type-loss and no new debt against #2271: whichever
representation #2271 selects will apply to both paths at once.

+17 tests (`DelegateSubsequenceRemovalTests`). Four mutations executed, all
caught, plus one documented as deliberately not executed. No signature, layout,
vtable, `noexcept` or symbol change.

## Other missing assertions and diagnostics

- No test covers same-type enforcement, dynamic result preservation, equal
  independently allocated multicast entries, multi-entry Remove/RemoveAll,
  or mismatch diagnostics.
- `GetInvocationList` has the documented `shared_from_this` requirement but
  lacks a checked/diagnosed stack-object route.
- The function-pointer-to-`void*` hash conversion is implementation-dependent
  C++ territory; no portability build asserts it on the supported compilers.

## Final assessment

Basic invocation works, but the composition/equality/removal contract diverges
on reachable ordinary delegate values.  No source or test was modified during
this audit.
