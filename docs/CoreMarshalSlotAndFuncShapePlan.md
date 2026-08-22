<!-- SPDX-License-Identifier: MIT -->

# `MarshalByRefObject`, `LocalDataStoreSlot`, `Func`/`Converter` — SR-AUD-128 / SR-AUD-129 / SR-AUD-126 review

Tickets: **#2296** (review), **#2297** (SR-AUD-128, `needs_user`),
**#2298** (SR-AUD-129, `needs_user`), **#2299** (SR-AUD-126, `needs_user`),
**#2300** (the compatible partial measures, landed). Date: 2026-08-11. The audit
numbering stays frozen at 364, **no new `SR-AUD-*` identifier was created** and
**no CCF was minted**.

> **Historical snapshot.** This document preserves the 2026-08-11 review state and its
> then-current `confirmed (design-complete)` decisions. Tickets #2297, #2298 and #2299 later
> implemented and migrated all three repairs; SR-AUD-126, SR-AUD-128 and SR-AUD-129 are
> `remediated` in the authoritative audit index. Statements below about the “current tree” and
> the final disposition apply only to that dated checkpoint.

---

## 1. Verdict — three causes, not a family

They were ranked together as "remaining small Core types with no first-party
production consumer". Two thirds of that description is not even accurate, and
none of it is a cause.

| | SR-AUD-128 `MarshalByRefObject` | SR-AUD-129 `LocalDataStoreSlot` | SR-AUD-126 `Func` / `Converter` |
|---|---|---|---|
| Cause | the public shape is **wider and thinner** than .NET's — instantiable where .NET is abstract, and three members short | the type is a **door with no building**: .NET reaches it only through `Thread` APIs this port does not have, and its one value is shared by every thread | an **alias introduces no type**, so `Func<void>` *is* `Action` and the .NET category split cannot exist |
| Production consumers | **2** — `AppDomain` and `ContextBoundObject` both derive from it | **0** | **0** name the `Func`/`FuncT*` aliases; `Converter` appears in two doc-comments |
| Domain | compile-domain shape + missing runtime diagnostics | semantics and threading | compile-domain type identity |
| Faithful repair | source break **and** a vtable change | a new `Thread` API **and** a semantic change | a source break |
| Disposition | **confirmed (design-complete)** — #2297 | **confirmed (design-complete)** — #2298 | **confirmed (design-complete)** — #2299 |

The one thing they share is the shape this batch has now met four times
(SR-AUD-113/117, SR-AUD-115/116, and here): **a divergence the language can
express, whose adoption costs compatibility, is a decision; a divergence the
language cannot express is a documentation repair.** All three of these are the
first kind. That is a recurring *migration risk*, not a common cause, and it is
explicitly not grounds for a CCF — CCF-021 and CCF-022 remain unminted.

"No first-party production consumer" is also false for SR-AUD-128, which had two,
and it would license nothing even where true: all three headers are public in
`Core.Base`.

---

## 2. SR-AUD-128 — `MarshalByRefObject`

### 2.1 Reproduced, and it is a conjunction

Both halves hold against the current tree.

- **Instantiable.** `System::MarshalByRefObject obj;` compiles; the class has an
  implicitly public default constructor and only a virtual destructor. .NET
  declares the class `abstract` with a `protected` constructor. Measured
  in-repo cost of closing it: **two construction sites in two different test
  executables** — `Batch3TypeTests.cpp:90`
  (`MarshalByRefObjectNewTests.DefaultCtor_DoesNotThrow`, `Core.Base`) and
  `Task42Tests.cpp:1452` (`MarshalByRefObjectTests.Instantiation_NoThrow`,
  integration) — plus any downstream site, uninspectable here.
- **Three members absent.** `GetLifetimeService()`, virtual
  `InitializeLifetimeService()`, protected `MemberwiseClone(bool)`. Current .NET
  keeps the first two precisely so a caller gets `PlatformNotSupportedException`;
  omitting them turns an observable runtime diagnostic into a compile error at
  the call site.

### 2.2 Options, priced

| | cost | closes the finding |
|---|---|---|
| **A — protected constructor** | public source break; 2 in-repo sites migrate; downstream sites unknown | half of it |
| **B — add the three members throwing `PlatformNotSupportedException`** | additive in source, but `InitializeLifetimeService()` is **virtual**, so it adds a vtable slot to this class *and* to `AppDomain` and `ContextBoundObject`; `GetLifetimeService()` is `[Obsolete]` in .NET, which collides with the still-undecided #2289; and `MemberwiseClone(bool)` has **no base to overload** — measured, `System::Object` declares no `MemberwiseClone` at all, so it would be a new invention rather than a port | the other half |
| **A + B** | both of the above — the finding's own first prescription | **yes** |
| **D — rename to a project-specific marker** | the finding's own alternative; a wider source break than A, since the two derived classes and every downstream base-specifier change | yes |
| **C — documentation** | none; **taken** (§5) | **no** |

Three separate compatibility costs sit behind one finding, so nothing was
selected. #2297 carries them.

## 3. SR-AUD-129 — `LocalDataStoreSlot`

### 3.1 Reproduced, plus one premise correction

The single `std::any` is the whole storage and is shared by every thread;
`sizeof(LocalDataStoreSlot)` is 16. .NET's constructor is *internal* and public
callers reach a slot only through `Thread.AllocateDataSlot` /
`AllocateNamedDataSlot` / `GetData` / `SetData` / `FreeNamedDataSlot`, **none of
which exist anywhere in this repository** — so the public default constructor
and the `getData`/`setData` pair are a project-owned surface under a .NET name.
Zero production consumers; one test file.

**Premise correction — the report's third "missing assertions" bullet is
wrong.** It states "the `noexcept` setters can terminate if `std::any`
assignment throws". Measured: `std::any`'s move assignment and `reset()` are
both `noexcept` by the standard (`noexcept(a = std::move(b))` and
`noexcept(a.reset())` are both 1), and `setData`'s parameter is taken **by
value**, so any allocation for it happens at the call site, outside this
function's exception specification. `noexcept(s.setData(std::move(v)))` is 1 and
sound. **No ordinary defect ticket was filed**, unlike the genuinely unsound
`noexcept` at #2292, because there is nothing to repair.

What the report understates is the *other* half of that bullet: there is no
synchronization policy at all, so two threads touching one slot with at least
one write is a data race with undefined behaviour. That is now documented.

### 3.2 Options, priced

| | cost | closes the finding |
|---|---|---|
| **A — thread-indexed storage behind new `Thread` slot APIs** | a substantial new public API (allocate, named allocate, get, set, free) **and a behaviour change**: `getData`/`setData` stop being shared, which silently changes what any existing caller observes | yes |
| **B — non-public constructor, reachable only through those `Thread` doors** | A's cost plus a public source break — `LocalDataStoreSlot s;` stops compiling | yes, most faithfully |
| **C — demote/rename to an explicit non-thread-local `std::any` holder** | public source break; the .NET name disappears | yes |
| **D — documentation** | none; **taken** (§5) | **no** |

A behaviour change and a source break are both user decisions, and A's API
surface is large enough to need its own scope. #2298 carries them.

## 4. SR-AUD-126 — `Func` / `Converter`

### 4.1 Reproduced, and the "prevent" half is structurally impossible

`Func<void>` compiles and **is** `Action`; `Converter<T, void>` **is**
`ActionT<T>`. Not convertible — identical, because an alias template introduces
no new type. The finding's second prescription is "document and test this as a
deliberate C++ extension **while preventing APIs that require .NET parity from
accepting the substitute aliases**", and that second clause cannot be satisfied
by any alias-based design: no declaration written in terms of these names can
accept one and reject the other, because there is only one type. Preserving the
category would mean replacing every alias with a distinct class type — a whole-
API break — so the realistic choice is narrower than the finding implies.

Measured: `Converter` is declared **twice**, in `System/Converter.hpp` and in
`System/Action.hpp`, identically. Including both in one translation unit
compiles clean under `-Wall -Wextra -Wpedantic -Werror`; the two spellings name
one type. Recorded as a fact, **not filed as a defect** — nothing is wrong.

### 4.2 Options, priced

| | cost | closes the finding |
|---|---|---|
| **A — constrain the result type** | **mechanically available and measured**: a constrained alias-template parameter (`template<NonVoid R> using Func = …`) is legal C++23 here, and `Func<void>` then fails with `error: template constraint failure … constraints not satisfied`. But that is a **compile-domain public source break** at every `Func<void>` / `Converter<T, void>` downstream, and it touches all 17 `Func` aliases plus both `Converter` declarations | yes |
| **B — document as a deliberate extension** | none; **taken** (§5) — but only the first clause of the finding's prescription; the "prevent" clause is impossible per §4.1 | **no** |
| **C — distinct class types instead of aliases** | whole-API break, rejected | yes |

Zero first-party sites spell `Func<void>` or `Converter<T, void>` — measured —
and that still licenses nothing. #2299 carries the decision.

---

## 5. What was taken anyway (#2300), and what it does not close

True under every outcome of all three tickets, so it landed:

- **`MarshalByRefObject.hpp`** states that it is directly constructible where
  .NET's is abstract with a protected constructor, that two in-repo tests
  construct it so closing that is a source break, that the three .NET members
  are absent rather than unimplemented, that .NET keeps two of them so callers
  receive `PlatformNotSupportedException`, and that restoring the virtual one
  changes this class's vtable and that of both derived classes.
- **`LocalDataStoreSlot.hpp`** states that .NET's constructor is internal and
  that this port has no `Thread` slot API at all, that the one value is shared by
  every thread, that concurrent access is an unsynchronized data race, and that
  the `noexcept` specifications are themselves sound and why (§3.1). It also
  corrects the pre-existing "Use std::thread_local", which named no C++ facility
  — the specifier is `thread_local`.
- **`Func.hpp`** carries the arity-spelling note the report asks for, the
  `std::bad_function_call` note, and the `void` warning including the structural
  reason no alias-based design can restore the category split.
  **`Converter.hpp`** carries the matching warning and **`Action.hpp`** a
  one-paragraph cross-reference at its duplicate declaration.

**None of this closes any of the three findings**: the base is still
constructible, the slot is still one shared value, and `Func<void>` is still
`Action`.

### 5.1 Tests — five added to `FuncTests`, none retired

From the SR-AUD-126 report's "Other missing assertions" bullets, all pinning
undisputed behaviour: `Arities5Through7_CompileAndInvoke` and
`Arities9Through15_CompileAndInvoke` close the coverage gap the report names
(only 0–4, 8 and 16 were exercised); `EmptyFunc_ThrowsBadFunctionCall` documents
`std::function`'s own diagnostic, which is not a `System::Exception`;
`TargetExceptionPropagatesUnchanged` and `ReferenceResultIsNotCopied` cover the
exception and result-reference bullets.

**Mutation M3** (rebuilt and relinked): dropping `T11` from `FuncT11`'s
signature is caught at compile time by `Arities9Through15_CompileAndInvoke` and
by nothing else — a copy-paste slip in an arity alias is exactly the defect class
that coverage exists for, and it is a compile error rather than a failing
assertion.

**Deliberately not added:** any case asserting `Func<void>` is `Action`, any
case constructing `MarshalByRefObject` beyond the two that already exist, and
any two-thread `LocalDataStoreSlot` case. All three would pin surface that
#2297/#2298/#2299 may change.

---

## 6. Compatibility

| Dimension | #2300 |
|---|---|
| Public source | none — every header change is inside a comment |
| ABI / exported symbols | none |
| Layout / vtable | none |
| `noexcept` | none |
| Includes / component graph | none |
| Behaviour | none — no executable statement changed outside the test tree |

## 7. Validation

`build/` only, `cmake --build build --parallel 2`, maximum two jobs.
`SharpRuntimeTests_Core_Base` 5,925 → 5,930; the integration binary rebuilt and
relinked because `Action.hpp` reaches it. Two throwaway probes under
`build-probe/`, each one translation unit compiled singly and deleted once
§3.1 and §4.2 were transcribed here.

**No sanitizer run.** Classified first: every unit here is API documentation
plus tests, with no executable statement changed outside the test tree, and both
compile-domain claims (the constrained alias, the duplicate `Converter`) are
settled by a compiler diagnostic. The one memory-safety-adjacent claim — the
`LocalDataStoreSlot` race — is *documented*, not repaired, and a TSan run on a
single-threaded test would show nothing about it. **Selective components not
rerun**: no component, dependency, module boundary or catalogue entry changed.

## 8. Disposition

| Finding | Before | After | Owner |
|---|---|---|---|
| SR-AUD-128 | confirmed | **confirmed (design-complete)** | #2296 review, #2297 `needs_user` |
| SR-AUD-129 | confirmed | **confirmed (design-complete)** | #2296 review, #2298 `needs_user` |
| SR-AUD-126 | confirmed | **confirmed (design-complete)** | #2296 review, #2299 `needs_user` |

No new `SR-AUD-*` identifier. No CCF minted. No public source break taken, and
the three that these findings need are recorded as decisions rather than assumed
away. One report premise corrected (§3.1) and one report prescription shown to
be unsatisfiable (§4.1).
