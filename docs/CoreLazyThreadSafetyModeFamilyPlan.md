<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `Lazy<T>` thread-safety-mode boundary family — plan

Ticket #2235. Two frozen audit findings, both in
`modules/core/include/System/Lazy.hpp`, both about the same field:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-064 | medium | Mode-taking constructors silently accept an invalid `LazyThreadSafetyMode` |
| SR-AUD-066 | medium | `PublicationOnly` wrongly rejects recursive `Value()` access |

The audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. `modules/core` had **56** open findings when this family
opened; the remaining 54 are explicitly out of scope, and this is **not** a
`modules/core` namespace review.

---

## 1. Exact scope

In scope: `modules/core/include/System/Lazy.hpp` (the mode field, the two
mode-taking constructors, and the reentrancy guard in `getValueProperty()`),
`modules/core/include/System/Threading/LazyThreadSafetyMode.hpp` (read only),
and `modules/core/tests/System/LazyTests.cpp`.

Out of scope, named so they are not absorbed later:

- SR-AUD-065 (empty factory) — already **remediated** by #1867/CCF-011. Its
  `requireFactory()` check is a direct precedent for this family and must keep
  firing *before* any new mode check, because .NET checks the factory first.
- The Lazy audit report's four "other missing assertions" bullets — the
  `getModeProperty()` debug-view adaptation, `ToString`'s narrow fallback, the
  move-only / non-default-constructible `T` coverage gap, and the None-mode
  single-thread requirement. None carries an `SR-AUD-*` identifier and none is
  claimed here.
- `System::Threading::LazyInitializer` (`modules/threading`) — a different type
  in a different module, with no finding in this family.

## 2. Are these one family? — yes as a review, no as a repair

They share the type, the header, and the field: `mode_` is the only input whose
contract either finding is about. That is enough for **one bounded review**.

They do **not** share a root cause, and the inherited "both turn on
`LazyThreadSafetyMode` policy" grouping (recorded in #2229's notes) is only half
right. Measured:

- **SR-AUD-064 is a missing argument validation.** The value is outside the
  enumeration's domain. .NET rejects it; nothing about this port's design
  prevents rejecting it; no approval boundary is anywhere near it. It is
  ordinary compatible remediation of exactly the same shape as #1867 on the very
  same constructors.
- **SR-AUD-066 is a guard placed before a dispatch.** The value is *inside* the
  domain; what diverges is what the port then does with a legal
  `PublicationOnly` instance. Repairing it as .NET behaves is blocked on a real
  design decision (§4.2), which is why it is dispositioned by the finding's own
  second option and its alternative is deferred to a `needs_user` ticket.

So: one review ticket (#2235), two implementation tickets (#2236, #2237), one
deferred design ticket (#2238).

## 3. Before evidence, measured 2026-08-10

`build-probe/2235_probe1_before.cpp`, compiled against the shipped
`build/libsharp_runtime_core.a` and the shipped header, one binary,
17 cases: **10 OK, 5 BAD, 1 DEVIATION** (`build-probe/2235_probe1_before.log`).

Both premises are confirmed **exactly as the findings state them**, with no
correction needed:

```
[064] Lazy<int>(mode=99)                              BAD  (got no-throw, want ArgumentOutOfRangeException)
[064] Lazy<int>(mode=-1)                              BAD  (got no-throw, ...)
[064] Lazy<int>(factory, mode=99)                     BAD  (got no-throw, ...)
[064] getModeProperty() after mode=99                 BAD  (got 99)
[064] invalid mode dispatches as PublicationOnly      BAD  (got 2 factory calls)
[066] PublicationOnly recursive Value()               DEVIATION (got InvalidOperationException)
```

The fifth row is the part of SR-AUD-064 that is worth more than the exception
type: the invalid instance is not merely tolerated, it *silently acquires
another mode's semantics*. Two `getValueProperty()` calls over a throwing
factory produced **two** factory invocations — no fault caching, which is
`PublicationOnly`'s contract and neither `None`'s nor
`ExecutionAndPublication`'s. That is the `default:` label of the switch in
`getValueProperty()` deciding a public contract by accident.

The controls that must not move are in the same binary: all three defined modes
construct, `PublicationOnly` still evaluates its factory once and returns 5
twice, recursion into a *different* instance is legal in every mode, and the
#1867 empty-factory rejection still wins the race against an invalid mode.

## 4. The two members, individually

### 4.1 SR-AUD-064 — validate the mode at construction (#2236, compatible)

.NET routes every mode-taking constructor through `LazyHelper.Create(mode,
useDefaultConstructor)`, whose `switch` has the three defined cases and

```csharp
default:
    throw new ArgumentOutOfRangeException(nameof(mode), SR.Lazy_ctor_ModeInvalid);
```

The port gains a private `requireValidMode()` with the same three cases and the
same `default:`, called from the two constructor bodies that take a
`LazyThreadSafetyMode` — `Lazy(LazyThreadSafetyMode)` and
`Lazy(F&&, LazyThreadSafetyMode)`. The other five constructors cannot produce an
invalid mode: `Lazy()`, `Lazy(T)` and `Lazy(F&&)` hard-code
`ExecutionAndPublication`, and the two `bool` forms select between two defined
values.

**Ordering.** `requireFactory()` runs first, matching
`Lazy(Func<T>, LazyThreadSafetyMode)`, which does
`ArgumentNullException.ThrowIfNull(valueFactory)` before `LazyHelper.Create`.
A probe control pins that an empty factory plus an invalid mode still reports
the factory.

**Message text.** The exception *type* is `ArgumentOutOfRangeException` and the
`paramName` is `"mode"`; both are stated by the finding and both are pinned by
tests. The message sentence mirrors .NET's `SR.Lazy_ctor_ModeInvalid`
resource, `"The mode argument specifies an invalid value."`. `/rv` is absent in
this container, so the sentence is not independently verifiable here. That is
not treated as a defect: this repository's established practice, recorded in
`ArgumentOutOfRangeException.hpp`'s own "KNOWN MINOR GAP" note and in
`CLAUDE.md`'s parity philosophy, is not to chase verbatim message text for its
own sake. The type, the `paramName` and the door are the contract.

**Consequence for the switch.** After #2236 an invalid `mode_` is unreachable —
there is no mode setter and `Lazy<T>` is neither copyable nor movable — so the
`case PublicationOnly: default:` fused label in `getValueProperty()` can no
longer decide anything. It is left fused, with a comment, because splitting it
would mean inventing a behaviour for a state that construction now forbids.

### 4.2 SR-AUD-066 — document and pin the reentrancy deviation (#2237), defer the behaviour change (#2238)

The finding states the .NET rule and then states its own two acceptable
repairs, verbatim:

> A repair must either implement the PublicationOnly rule without deadlock or
> document and deliberately expose the incompatible restriction; silently
> throwing the .NET-specific exception is not the stated behavior.

**Why option (a) is not taken autonomously.** The port serialises
`PublicationOnly` behind `publicationOnlyMutex_`, a deliberate deviation already
documented in the class doc-comment: real `PublicationOnly` runs the factory on
several threads at once and races to publish, which for an arbitrary `T` is a
C++ data race, i.e. undefined behaviour. `checkNotReentrant()` is therefore
**load-bearing for memory safety**, not merely a contract check: a recursive
`getValueProperty()` on the owning thread would re-`lock()` a non-recursive
`std::mutex`, which `[thread.mutex.requirements.mutex]` makes undefined
behaviour outright ("If `lock()` is called by a thread that already owns the
mutex, the behavior is undefined"). Removing the guard is not a repair; it is a
new defect.

Implementing .NET's actual `PublicationOnly` recursion therefore needs one of
two structural changes, and **both are user-visible design decisions**:

1. *Publish-only locking* — run the factory outside the mutex and hold it only
   across the publication, first writer wins, losers discard. This is closer to
   .NET in every respect, and it also **reverses the documented serialisation
   deviation**: factories would again run concurrently on several threads, which
   downstream code written against the documented single-call-at-a-time
   guarantee may rely on.
2. *Same-thread reentrancy* — keep the mutex for other threads, and on the
   owning thread invoke the factory again without re-locking. This needs
   `initValue` to learn a discard rule so the outer invocation does not
   overwrite the nested publication (.NET keeps the **first** published value).

Both share the decisive cost: a genuinely recursive factory then recurses
without bound. In .NET that ends in `StackOverflowException` and process death;
in C++ it is stack exhaustion, which is undefined behaviour, replacing a clean,
catchable `System::InvalidOperationException`. Trading a catchable exception for
UB on pathological input contradicts `CLAUDE.md`'s stated preference for
throwing clearly over degrading, and is not a call to make without the user.

**So option (b) is implemented, in full.** The behaviour does not move; what
moves is that it stops being silent:

- the class-level "Deviation from .NET" block gains a second, explicit
  paragraph naming `PublicationOnly` recursion, the .NET rule it does not
  implement, the `std::mutex` reason, and ticket #2238;
- `getValueProperty()`'s `@throws` clause, which already named the exception,
  gains the fact that .NET raises it for `None` and `ExecutionAndPublication`
  only;
- permanent regression tests pin the behaviour in **all three** modes, so the
  deviation cannot be changed silently in either direction, plus the controls
  that a future option-(a) implementation must not break.

**Disposition.** SR-AUD-064 → `remediated`. SR-AUD-066 → `remediated` **by the
finding's option (b)**, with the behavioural divergence stated in the index
entry as retained by design and ticket #2238 named as the reopening path. The
index entry must not read as though `PublicationOnly` recursion now matches
.NET, because it does not.

## 5. CCF relationships — none minted, none extended

- **CCF-011** (empty `std::function` at a public boundary) owns SR-AUD-065 in
  this same header. SR-AUD-064 is *adjacent*, not a member: its subject is an
  out-of-domain enum value, not an empty callable. The shared thing is the
  constructor body, which is a location, not a cause.
- **CCF-019** (borrowed raw pointers / ownership lifetime) is untouched; this
  family hands out no view or reference beyond the existing `const T&`.
- **CCF-021** (`#2131`) and **CCF-022** (`#2109`) remain unminted candidates with
  unresolved mint authority; neither is referenced, broadened or decided here.

No new cause identifier is minted. Recording the shape without an identifier
follows the #2148 and #2229 precedent.

## 6. Compatibility, ABI, layout and `noexcept`

| Property | #2236 | #2237 |
|---|---|---|
| Public signature change | none | none |
| Template parameter change | none | none |
| `noexcept` change | none — no constructor was ever `noexcept` (each constructs a `std::function`, and `requireFactory()` already threw from three of them) | none |
| Data member / `sizeof` / `alignof` change | none | none |
| Virtual function / vtable | none — `Lazy<T>` has no virtual member | none |
| Accepted-input change | **yes**: an out-of-domain `LazyThreadSafetyMode` is rejected instead of silently behaving as `PublicationOnly` | none |
| Emitted-value change | none | none |

`Lazy<T>` is a header-only class template with no out-of-line definitions, so
there is no exported symbol to compare. The only observable change in the whole
family is #2236's rejection of a value that was never a member of the
enumeration, at the constructor rather than never.

## 7. Behaviour that deliberately does not move

`ExecutionAndPublication` and `None` fault caching; `PublicationOnly` fault
retry; `getIsValueCreatedProperty()`'s lock-free read; `getModeProperty()`
returning the constructed value; the `ToString()` fallback ladder; the empty-
factory `ArgumentNullException` of #1867; `Lazy` staying non-copyable and
non-movable; and — by decision, not by omission — the
`InvalidOperationException` for recursive `Value()` in all three modes.

## 8. Ticket split

| Ticket | Status | Scope |
|---|---|---|
| #2235 | review | this plan, the before-probe, the premise verification, the split |
| #2236 | implementation | SR-AUD-064 — `requireValidMode()` in the two mode-taking constructors |
| #2237 | implementation | SR-AUD-066 — document and pin the `PublicationOnly` reentrancy deviation |
| #2238 | `needs_user` | DESIGN/APPROVAL — should `PublicationOnly` permit recursive `Value()`, by publish-only locking or same-thread reentrancy, accepting unbounded recursion? |

## 9. Test matrix

Added to `modules/core/tests/System/LazyTests.cpp`:

- #2236 — invalid mode rejected by both mode-taking constructors, for two
  distinct out-of-domain values; exception type and `paramName`; catchability as
  `System::Exception`; all three defined modes still construct through both
  constructors; both `bool` forms and all three non-mode constructors unaffected;
  empty factory still reported before the mode; a repeated throwing-construction
  loop so a leaked factory target would be visible to LeakSanitizer.
- #2237 — recursive `Value()` in each of the three modes; the exception message;
  a `PublicationOnly` factory that does *not* recurse still evaluates once and
  returns twice; a fault still retries under `PublicationOnly`; recursion into a
  *different* instance stays legal in every mode.

## 10. Sanitizer matrix

Neither member is a memory-safety or arithmetic defect: #2236 turns a defined
no-throw into a defined throw, #2237 changes no executable statement. A
sanitizer run would not discriminate either, so none is used for the verdict —
the same evidence discipline #2229 applied for the same reason. The one
sanitizer-relevant risk, a leaked `std::function` target on the new throwing
path, is covered by the LeakSanitizer-visible loop test, mirroring #1867's.

## 11. Family completion criteria

Both findings dispositioned in `audit/AUDIT_FINDINGS_INDEX.md` and in the Lazy
per-file report; #2238 recorded as `needs_user`; zero build warnings; no test
regression; the before-probe re-run after the repair with every BAD row turned
OK and the DEVIATION row unchanged and now documented.
