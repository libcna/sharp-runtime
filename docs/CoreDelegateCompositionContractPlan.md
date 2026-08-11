<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `Delegate` composition, equality and removal — review

Ticket #2270. Three frozen audit findings, all filed against
`modules/core/src/System/Delegate.cpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-118 | medium | composition accepts different concrete delegate types and converts every combined/removal result into base `Delegate` |
| SR-AUD-119 | medium | equality compares multicast entries by shared-pointer identity instead of delegate equality |
| SR-AUD-120 | medium | `Remove` cannot remove a multicast value's last matching invocation-list subsequence |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier.

This is a **bounded review of three findings that happen to live in one file**,
not a `Delegate` redesign and not a `modules/core` review. The inherited ranking
paired them only by file, and the recent SR-AUD-177/178 precedent (#2267) is the
discipline applied here: a shared file is adjacency, not causation, and has to be
disproved or confirmed by measurement.

---

## 1. Answer first

**They are not one family. They are three findings with three distinct root
causes, in two independent groups.**

| Finding | Root cause | Class | Outcome |
|---|---|---|---|
| SR-AUD-118 | the port has no representation for a delegate's *concrete type* in a composed result, so it cannot preserve one or check one | **representation design + behaviour-incompatible tightening** | design-complete; **#2271 `needs_user`** |
| SR-AUD-119 | the list-equality loop uses the wrong comparison primitive — `shared_ptr::get()` where the entries' own `Equals` already exists and already answers correctly | ordinary compatible repair (widening) | **#2272, implemented** |
| SR-AUD-120 | a whole algorithm — the multi-entry subsequence search — was never written; `Remove` only ever compares a *single* entry against the whole value | ordinary compatible repair (widening) | **#2273, implemented** |

SR-AUD-119 and SR-AUD-120 are **adjacent, not one cause**: 119 is a wrong
primitive in code that exists, 120 is missing code. They touch different
functions, and each is independently reproducible, independently repairable and
independently testable — §7 measures that neither repair changes the other's
before/after values. They are therefore two implementation tickets, not one.

**No CCF is minted.** A cross-cutting cause would have to be one defect
reachable from several places; here the only thing the three share is the file
they were filed against.

---

## 2. Method

Every value in this document was measured against the current tree by
`build-probe/2270_probe1_before.cpp` (log: `build-probe/2270_probe1_before.log`),
not taken from the audit report. The report's own probe binary lived under
`/tmp` and is gone; its recorded numbers are reproduced below independently.

The probe links the real `build/libsharp_runtime_core.a` and declares
`OtherDelegate`, a second concrete `Delegate` subclass, because the repository
itself has exactly one (`MulticastDelegate`) and the mixed-type question cannot
be asked without a second.

Also read in full: `Delegate.hpp` (245 lines), `Delegate.cpp` (166 lines),
`MulticastDelegate.hpp`, and all four fixtures that touch delegates
(`DelegateTests.cpp`, `MulticastDelegateTests.cpp`, `Batch14DelegateGCTests.cpp`,
and the `RuntimeHelpers` no-ops).

---

## 3. Measured before-state

```
118a combined_dynamic_type=N6System8DelegateE
118a combined_dynamic_type_preserved=0
118b mixed_types_accepted=1 size=2 threw=0
118c chained_step2_operand_types_equal=0 size=3
118d remove_result_dynamic_type=N6System8DelegateE preserved=0
118e cross_type_single_equals=1
119a equal_function_lists_equal=0
119a equal_function_lists_hash_equal=0
119a entry_pairs_equal=1 1
119b different_lists_equal=0 reordered_lists_equal=0
119c lambda_lists_equal=0
119d same_pointer_lists_equal=1 hash_equal=1
120a remove_multicast_subsequence_size=4 unchanged=1
120b remove_same_pointer_subsequence_size=4 unchanged=1
120c remove_entire_list_null=0 size=2
120d remove_leaving_one_size=3 is_a=0
120e longer_value_unchanged=1
120f removeall_subsequence_size=4 null=0
120g remove_single_last_occurrence_size=2
120g after_remove first=1 second=1
```

All three findings reproduce exactly as filed:
`combined_dynamic_type_preserved=0`, `mixed_types_accepted=1`,
`equal_function_lists_equal=0` and `remove_multicast_subsequence_size=4` are the
four values the audit recorded. **No premise in any of the three headlines needed
correction** — which is worth stating, because the file's recent neighbours
(#2258, #2265, #2267) each corrected one.

Three facts the findings do **not** state came out of the same run, and each one
decides a scoping question:

- **118c — `chained_step2_operand_types_equal=0`.** A chain
  `Combine(Combine(a,b),c)` presents a base `Delegate` and a `MulticastDelegate`
  to the second step. Both existing fixtures use that chain. See §4.2: it makes
  the two halves of SR-AUD-118 inseparable.
- **119a — `entry_pairs_equal=1 1`.** The individual entries of the two
  independently built lists are *already* equal to each other. The defect is
  purely that the list loop declines to ask them. See §5.
- **120a/120b — `unchanged=1` in every removal case.** `Remove` with a multicast
  value never removes anything today, not even when the entries are the
  *identical pointers* (120b). See §6.3: the repair's regression surface is
  empty.

---

## 4. SR-AUD-118 — why it is a decision, not an implementation

### 4.1 The two halves

`Combine` (`Delegate.cpp:106-122`) and multicast `Remove` (`:143-151`) both end
in `new Delegate(MulticastTag{}, …)`. Current .NET does two things this does
not:

1. `MulticastDelegate.CombineImpl` **rejects** a mismatched runtime type with
   `ArgumentException(SR.Arg_DlgtTypeMis)` before combining;
2. it **creates the same runtime delegate type** for the result.

### 4.2 The halves cannot be separated — measured

Half 1 on its own is not implementable. `typeid(*a) == typeid(*b)` would reject
step two of every chained `Combine`, because step one already returned a base
`Delegate` while the third operand is still a `MulticastDelegate` — that is
118c, and it is the exact shape of `DelegateTests.Combine…` and
`MulticastDelegateTests.Combine_InvocationListSize`, two currently green tests.
So a type check is only coherent *after* results preserve their type. The
findings' two halves are one coupled repair, and the coupling is measured rather
than assumed.

### 4.3 Why it is not implementable autonomously

Preserving the concrete type needs a representation this port does not have.
Three routes were priced; none is approval-free:

- **Route A — a new virtual factory** (`CombineImpl`/`RemoveImpl`, or a
  protected `CreateFromInvocationList`). Faithful to .NET, but it adds a virtual
  function to a **public polymorphic base**: vtable change, hence an ABI break
  for every consumer that already links `Delegate`; new protected public API; and
  a silent-degradation contract, because a subclass that does not override it
  keeps producing base results while callers now assume it does not.
- **Route B — reuse the existing virtual `Clone()`.** No vtable change, and
  `Clone()` already returns the right dynamic type. But `Clone()` is *public*, so
  `Combine` would begin invoking user-overridable code, and the combined result
  would inherit a copy of the left operand's subclass state — a semantic the port
  has never had and .NET does not describe. It also still degrades silently for a
  subclass that does not override `Clone()`.
- **Route C — leave results as base `Delegate` and check nothing.** The status
  quo; it is what the finding rejects.

On top of the representation choice, half 1 is **behaviour-incompatible in the
tightening direction**: `Delegate::Combine(a, b)` calls that succeed today would
throw `ArgumentException`. That is the same class of change as SR-AUD-178
(#2269) and the four date/time parsers, both of which the repository routes
through an explicit approval.

**Two gates, one ticket: #2271 (`needs_user`).** SR-AUD-118 becomes
`confirmed (design-complete)`.

### 4.4 One adjacent consequence, deliberately kept in #2271

118e measures that `Equals` is type-blind too: a `MulticastDelegate` and an
`OtherDelegate` wrapping the same function compare **equal**, where .NET's
`Equals` starts with `InternalEqualTypes` and answers false. This is not in the
frozen wording of 118 (composition), 119 (entry comparison) or 120 (removal), so
it would ordinarily be an ordinary post-audit ticket under the #2259 precedent.
It is folded into **#2271** instead, and named there explicitly, because it is
not separately implementable: it needs the identical type-identity decision, and
minting a second ticket for the same decision would only split one answer across
two rows. It is **not** absorbed into SR-AUD-118's frozen text.

---

## 5. SR-AUD-119 — the primitive, not the policy

`Equals` already has a value-equality path for the single-target case
(`Delegate.cpp:34-48`): two delegates wrapping the identical plain function
pointer are equal, which ticket 345 added deliberately. The multicast branch
(`:50-53`) never uses it, comparing `shared_ptr::get()` instead, and
`GetHashCode` (`:60-67`) hashes the same addresses.

119a is the proof that this is a one-primitive defect: the entries of the two
independently built `[First, Second]` lists **already answer `Equals` correctly**
(`entry_pairs_equal=1 1`), and the lists are unequal only because the loop
compares their addresses. CoreCLR's `EqualInvocationLists` calls each entry's
`Equals` — the reference behaviour the audit records.

### 5.1 The repair is a strict widening

`la[i].get() == lb[i].get()` implies `Equals` via its own `this == &other` fast
path, so **every pair equal today stays equal** and only value-equal pairs are
added. No call that returns `true` today can begin returning `false`. The
before/after table in §7 shows exactly one cell moving, in that direction.

### 5.2 The hash has to move with it

`GetHashCode` must fold each entry's **hash code**, not its address, or the
widened equality would break the hash contract the port already honours for
single targets (`MulticastDelegateTests.GetHashCode_SamePlainFunctionPointer…`).
The mixing function is left exactly as it was — the finding is about *what* is
hashed. For an entry that is not a plain function pointer, `GetHashCode()`
returns `std::hash<const Delegate*>{}(this)`, the same value the old code folded,
so **lambda-entry multicast hashes are unchanged**; only lists whose entries have
comparable targets move.

### 5.3 What 119 does *not* fix

It does not add .NET's type check to `Equals` — that is §4.4, inside #2271. The
widening cannot reach across concrete types in practice anyway: every multicast
in this port is created by `Combine`/`Remove`, both of which produce base
`Delegate` (118a, 118d), so a multicast's dynamic type is invariably
`System::Delegate`. SR-AUD-119's own frozen wording — entries compared by
pointer identity instead of delegate equality — is fully addressed.

---

## 6. SR-AUD-120 — missing code, not wrong code

### 6.1 What .NET does

`MulticastDelegate.RemoveImpl` branches on whether the *value* is itself
multicast. For a single value it scans entries backwards for the last equal one —
which this port already does. For a multicast value it scans candidate start
indices from `count - valueCount` down to `0` and deletes the **last** matching
subsequence.

### 6.2 What this port does

Nothing: the entry loop compares each single entry against the **whole** value
object, and a single-target entry can never equal a two-entry list (the size
check in `Equals` rejects it), so the result is always "not found".

### 6.3 The regression surface is empty — measured

120a–120f are every reachable removal shape with a multicast value, and every one
reports `unchanged=1` / the source's original size today. That includes 120b,
where the source's entries are the **identical `shared_ptr`s** contained in the
value. So the new branch only ever fires where the current answer is "return
source unchanged": **no existing outcome can change**. The single-value path
(120g) is untouched and pinned.

### 6.4 Boundaries taken from the reference behaviour

| Shape | Result |
|---|---|
| value longer than source | source unchanged (the candidate range is empty) |
| removal empties the list | `nullptr` |
| removal leaves one entry | that entry itself, matching the port's existing single-removal convention |
| two matching subsequences | the **last** one is removed |
| `RemoveAll` | repeats until nothing matches — inherits the fix, as the finding says |

### 6.5 120 does not need 118

The multi-entry result is built with the same
`new Delegate(MulticastTag{}, …)` the existing single-entry removal path already
uses, so this repair adds no new type-loss and takes on no new debt against
#2271: whatever representation #2271 selects will apply to both paths at once.

---

## 7. Before / after

The same probe is re-run against the repaired library after each implementation
ticket, and that ticket records its own cells here. The five `118*` cells must
stay exactly as §3 measured them in **both** passes — that is how the design gate
is shown to hold rather than asserted.

| Line | Before | Owner |
|---|---|---|
| `118a combined_dynamic_type_preserved` | 0 | #2271 — must not move |
| `118b mixed_types_accepted` | 1 | #2271 — must not move |
| `118c chained_step2_operand_types_equal` | 0 | #2271 — must not move |
| `118d remove_result_dynamic_type preserved` | 0 | #2271 — must not move |
| `118e cross_type_single_equals` | 1 | #2271 — must not move |

### 7.1 #2272 (SR-AUD-119)

*Recorded by #2272.*

### 7.2 #2273 (SR-AUD-120)

*Recorded by #2273.*

---

## 8. Compatibility statement

Both implemented repairs are confined to `Delegate.cpp` bodies and header
doc-comments.

| Property | Change |
|---|---|
| public signatures | none |
| object layout (`sizeof`/`alignof` of `Delegate`, `MulticastDelegate`) | none |
| vtable (order, count) | none |
| `noexcept` specifications | none |
| exported symbols | none added or removed |
| exception behaviour | unchanged — neither repair introduces a throw |
| accepted inputs | unchanged — no argument is newly rejected |
| answers | widened in seven measured cells (§7), all toward .NET |

No approval is required for #2272 or #2273 under the standard the repository
already applies: they widen rather than tighten, exactly as SR-AUD-177 was
classified in #2267, and unlike SR-AUD-178 no call that succeeds today begins to
throw.

In-repository blast radius is limited to the four delegate fixtures.
`RuntimeHelpers::PrepareDelegate`/`PrepareContractedDelegate` take
`const Delegate&` and are no-ops; nothing else in the tree calls `Combine`,
`Remove`, `RemoveAll`, `Equals` or `GetHashCode` on a delegate.

---

## 9. Sanitizers

Not run, deliberately. Neither repair introduces pointer arithmetic, unmanaged
lifetime or arithmetic that could be undefined: #2272 replaces one comparison
expression, and #2273 adds a bounds-guarded loop over an existing `std::vector`
whose only new operation is `erase` over a range proved in-bounds by the
`vl.size() > sl.size()` guard. The defect class here is a wrong *answer*, which a
sanitizer is silent about — the lesson already recorded for #1836/#1837. The
discriminating instrument is the before/after table in §7 plus the mutation
matrix in §10.

---

## 10. Mutations

`build-probe/2270_run_mutations.sh`. Each mutation is applied to the repaired
source, the affected object is rebuilt with two jobs, and the new tests are run.
Results are recorded by the owning ticket.

| # | Ticket | Mutation | Expected |
|---|---|---|---|
| 1 | #2272 | `Equals` list loop back to `la[i].get() != lb[i].get()` | caught (SR-AUD-119 regressions fail) |
| 2 | #2272 | `GetHashCode` folds `d.get()` again instead of `d->GetHashCode()` | caught (hash-agreement test fails) |
| 3 | #2273 | subsequence scan runs forwards, taking the **first** match | caught (last-occurrence test fails) |
| 4 | #2273 | subsequence branch drops the `vl.size() > sl.size()` guard | caught (longer-value test fails) |
| 5 | #2273 | subsequence removal returns a 1-entry multicast instead of the entry | caught (leaves-one test fails) |

---

## 11. Ticket map

| Ticket | Finding | State |
|---|---|---|
| #2270 | review | done |
| #2271 | SR-AUD-118 | `needs_user` — representation route **and** tightening approval, plus the §4.4 `Equals` type-blindness |
| #2272 | SR-AUD-119 | implementation, compatible, no approval needed |
| #2273 | SR-AUD-120 | implementation, compatible, no approval needed |
