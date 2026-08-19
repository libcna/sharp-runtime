<!-- SPDX-License-Identifier: MIT -->
# Migration — deep tree construction is linear (#1896)

Ticket **#1896** (CCF-019, probes J19d / X27d), landed 2026-08-19 on a per-action approval
(`docs/StandingApprovals.md` SA-13), which **withdrew** the refusal recorded on 2026-07-31.

**No public signature, layout, vtable or behaviour changed.** Every input accepted before is
accepted now, every input rejected before is rejected now, with the same exception type and text.
The only difference is how long it takes.

## The measurement

Same binary, same flags (`-O2`), before and after
(`build-probe/1896_probe1_before.log`, `..._after.log`):

| depth | JSON before | JSON after | XML before | XML after |
|---|---|---|---|---|
| 20,000 | 0.994 s | **0.009 s** | 4.668 s | **0.015 s** |
| 50,000 | 7.884 s | **0.023 s** | 34.547 s | **0.029 s** |
| 100,000 | 42.583 s | **0.045 s** | 120.028 s | **0.051 s** |

Quadratic → linear. **946×** at 100,000 levels for JSON, **2,353×** for XML.

## The approved layout growth was NOT taken, and that is the ticket's own premise corrected

#1896 was blocked on an object-layout approval — the plan was for `JsonNode` and `XContainer` to
cache a root or a depth. **The approval was granted and then turned out not to be needed.**
`sizeof` is unchanged on every type this ticket touches, no member was added, and no virtual was
added either (a virtual would have been a vtable change, a heavier approval than this needs).

The guards ask *"is `this` an ancestor-or-self of `parent`?"*, and two facts answer that in O(1)
whenever the answer is no:

* the node being attached is **parentless** — the existing already-has-a-parent check has thrown
  otherwise — so it is the root of its own subtree;
* it can therefore only be an ancestor of `parent` if it **has** a subtree.

So a direct `parent == this` comparison catches self-attachment, and a **childless** node cannot
contain anything, so the walk is skipped. Both container counts are O(1) vector sizes, and both
container headers were already included at both sites.

**The guard is faster, not weaker.** It rejects exactly what it rejected before — the `shared_ptr`
reference cycle it exists to prevent stays prevented — and every rejection path is asserted:
already-parented, self-attach on an *empty* container, self-attach on a non-empty one, an ancestor
under its own descendant, the same 64 levels deep, and (for JSON) an *object* ancestor rather than
an array.

**The trap, and it is the one mutation most likely to be written by accident:** an empty container
short-circuits the walk, so `n == this` must be tested **outside** the emptiness guard. Moving it
inside makes self-attachment legal again. That is mutations M1 and M4, and both are caught.

## There were TWO quadratic sources, and only one was in the ticket

Fixing the guard took XML from 120.0 s to **89.3 s** — a real improvement and still quadratic. A
test asserting only "faster" would have called that a success.

The second source is **#2199's own ancestor walk**, added earlier the same day: `NotifyChanging`
visits every ancestor on every mutation *even when nothing is subscribed*. **.NET has exactly that
shape** — `XObject.cs:424-427` skips annotation-less ancestors cheaply but still visits each one —
so .NET's XLinq is quadratic here too.

The fix is a **process-wide count of live registrations**. If it is zero, no walk can find anything,
so `NotifyChanging` returns `false` immediately. This is **exact, not approximate**: no handler is
ever skipped, because none exists. When registrations do exist the walk runs in full and behaviour
is identical to .NET's; only the nobody-is-listening case is faster.

The count is decremented by `remove_*` **and by the registration block's destructor**. Without the
latter, destroying an observed object would leave the count permanently non-zero and every tree in
the process would pay the full walk for ever — a silent, permanent regression of the very defect
this ticket fixes. That is mutation M7, and it is caught by a case built for it.

## Tests

`Pin2119_TheProgrammaticHalfOfTheResidualSURVIVESAndHasAnOwner` is **inverted**, as it said it must
be: it recorded the last surviving row of `OwnedTreeLifetimeContractPlan.md`'s deep-nesting group
and said *"#1896 landing is a visible change here rather than a silent one."*

Five cases added across the two modules. The timing cases use 40,000 levels with a generous bound
rather than a benchmark threshold — a quadratic implementation misses it by three orders of
magnitude, so it discriminates without flaking under gate load.

## Mutation testing

Eight mutations, **all caught — but two only after the tests that were supposed to catch them were
strengthened, and the reason is the most useful thing in this ticket.**

| # | Mutation | Caught by |
|---|---|---|
| M1 | JSON: move the `parent == this` check *inside* the emptiness guard | the cycle-guard case + a pre-existing `JsonArrayTests.Add_Self_Throws` |
| M2 | JSON: `hasChildren()` always false | the cycle-guard case + `JsonNodeMutationConsistencyTests` |
| M3 | JSON: `hasChildren()` ignores objects | the cycle-guard case, whose fifth arm exists for it |
| M4 | XML: move the self-check inside the guard | the self-insertion case + `XLinqMutationConsistencyTests` |
| M5 | XML: invert the guard | the self-insertion case + `XLinqLifetimeTests.SelfInsert…` |
| M6 | the notification short-circuit always skips | ~20 `Fix2199_*` cases |
| M7 | the registration count is never decremented on destroy | the destroy case, **after strengthening** |
| M8 | the count is not decremented on `remove_*` | the same, **after strengthening** |

**M7 and M8 were first reported NOT CAUGHT, and that was the honest result.** A leaked count does
not corrupt anything — the tree still builds correctly and every assertion still passes. It only
makes every tree in the process pay the full ancestor walk **for ever**, which is a silent,
permanent regression of the exact defect this ticket fixes. The suite merely got slower, and one run
hit the 240-second mutation timeout — and *a mutation caught only as a timeout is not caught by
name*.

The fix was to make the two cases **time** the build rather than merely run it. The bound is
2 seconds, and both margins are measured rather than guessed: 40,000 levels costs **~40 ms** linear
(50× of headroom under the bound) and **13,870 ms** under M7 (7× over it). So it discriminates
without being a benchmark that flakes under gate load — the same reasoning #2326 applied when
rescaling its resolution tests.

One mutation (M2, first spelling) was **invalid as written** — it left an orphaned function and
failed to compile — and was reformulated rather than counted.
