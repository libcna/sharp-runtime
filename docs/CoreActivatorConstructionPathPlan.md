<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `Activator::CreateInstance` construction path — plan

Ticket #2265. One frozen audit finding in
`modules/core/include/System/Activator.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-109 | medium | value `CreateInstance` changes forwarded constructor arguments through braced initialization |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **singleton on one template's construction
expression**, not an `Activator` review, and explicitly not a step toward
reflection — which `CLAUDE.md` records as a permanent deviation.

---

## 1. Scope

In scope: the single expression `T{std::forward<Args>(args)...}` in the
variadic value overload (`Activator.hpp:42` before the change).

Out of scope: the zero-argument `CreateInstance<T>()`, which is a separate
template and is correct — `T{}` and `T()` are both value-initialization, and for
an empty braced-init-list the language already prefers a default constructor
over an `initializer_list` one. Also out of scope: `CreateInstancePtr`, which
the finding itself calls correct and which is used here as the reference the
value form must agree with.

---

## 2. Root cause

Braces do not merely *initialize*; they change **overload resolution**. A
braced-init-list considers `initializer_list` constructors first and only falls
back to other candidates if none is viable. So for `std::vector<int>`:

| Spelling | Selected constructor | Result |
|---|---|---|
| `std::vector<int>{3, 7}` | `vector(std::initializer_list<int>)` | two elements `3, 7` |
| `std::vector<int>(3, 7)` | `vector(size_type count, const T& value)` | three elements `7, 7, 7` |

The header's own doc-comment says "Arguments forwarded to @p T's constructor",
and `CreateInstancePtr` delivers that through `std::make_unique`, which uses
`new T(args...)`. The value form did not, so the two published creation forms
disagreed — the finding's reproducer.

**Perfect forwarding was never the problem.** `std::forward` is present, correct
and untouched; §5 measures that value category, move-only arguments, explicit
constructors and constructor exception propagation are all identical under every
candidate repair. The defect is entirely in the choice of `{}` over `()`.

---

## 3. Was brace initialization the complete cause? No — measured

The finding's reproducer is one cell of a much larger table. The review question
this ticket had to answer is whether the audited repair — switching
unconditionally to parentheses — is an ordinary implementation correction, or
whether supported type categories depend on the current braced semantics.

Twelve categories were measured in a single translation unit, so no expectation
could drift between spellings. `cur` = today's braces, `A` = the audited pure-
parentheses repair, `C` = the repair implemented as #2266.

| Category | `cur` | `A` | `C` | Verdict |
|---|:--:|:--:|:--:|---|
| `vector<int>(3, 7)` | 1 | 1 | 1 | the reproducer; meaning changes, see §3.1 |
| `vector<int>(1, 2, 3)` | 1 | **0** | 1 | **A is a source break** (`initializer_list`) |
| `array<int, 3>(1, 2, 3)` | 1 | **0** | 1 | **A is a source break** (aggregate, brace elision) |
| `AggNested(1, 2, 3)` | 1 | **0** | 1 | **A is a source break** (nested aggregate) |
| `InitializerListOnly(1, 2, 3)` | 1 | **0** | 1 | **A is a source break** |
| `Agg(1, 2.5)` | 0 | **1** | 0 | **A silently accepts narrowing** |
| `int(2.5)` | 0 | **1** | 0 | **A silently accepts narrowing** |
| `string(3, 'x')` | 0 | 1 | 1 | C widens; see §3.2 |
| `Agg(1, 2)` | 1 | 1 | 1 | unchanged |
| `Both(1, 2)` | 1 | 1 | 1 | meaning changes, see §3.1 |
| `Ctor(1, 2)` | 1 | 1 | 1 | unchanged |
| `Explicit(1)` | 1 | 1 | 1 | unchanged |

**Verdict: brace initialization was not the complete cause, and four measured
categories that compile today depend on it** — including `std::array`, which is
not an exotic type. The audited repair is therefore a **public source break**,
and it additionally removes a compile-time safety property (narrowing rejection)
in two more categories. Under the SR-AUD-063 precedent — a public source break
is an approval boundary — **option A was not implemented**, and it remains
available to a future ticket that carries an approval.

### 3.1 Where the meaning of currently-compiling code changes

Three shapes compile before and after and mean something different. All three
are the defect the finding describes, and in all three the new answer is the one
`CreateInstancePtr` already produced:

| Call | Before | After (C) | Pointer form |
|---|---|---|---|
| `CreateInstance<vector<int>>(3, 7)` | `{3, 7}` | `{7, 7, 7}` | `{7, 7, 7}` |
| `CreateInstance<vector<int>>(1, 2)` | `{1, 2}` | `{2}` | `{2}` |
| `CreateInstance<Both>(1, 2)` | `initializer_list` ctor | `(int, int)` ctor | `(int, int)` ctor |

This is a behaviour correction, not a source break: nothing that compiled stops
compiling, and nothing that was accepted is now rejected. That is the boundary
this repository's approval rules draw — `docs/RemainingApprovalDecisions.md`
§C.8 was needed for the date/time parsers precisely because they began
**rejecting** text they used to accept, which is not the case here.

### 3.2 The one widening, and why it is a repair rather than a risk

`CreateInstance<std::string>(3, 'x')` is **ill-formed today**. Not because
`std::string{3, 'x'}` is inherently invalid, but because by the time the `3`
reaches the braced-init-list it is a function parameter rather than a constant
expression, so the narrowing exemption for constants that fit no longer applies
and `int` → `char` is a narrowing error. The pointer form already accepted this
call. C accepts it too, so the widening **removes** a value/pointer disagreement
rather than creating one. A widening cannot break a caller.

### 3.3 First-party call-site census

There is **no first-party production call site** of either `CreateInstance`
overload — the only callers in the repository are the seven pre-existing
`ActivatorTests` in `Batch3TypeTests.cpp`. All seven use types with ordinary
constructors or no arguments, so none of them changes behaviour, and all seven
still pass unmodified. The internal blast radius is zero; the entire risk of
this finding is downstream source compatibility, which is exactly what §3
measures.

---

## 4. The repair (#2266)

Forward to a constructor only where the type has one to forward to:

```cpp
template<typename T, typename... Args>
inline constexpr bool activatorForwardsToConstructor =
    std::is_class_v<T> && !std::is_aggregate_v<T> && std::is_constructible_v<T, Args...>;

template<typename T, typename... Args>
[[nodiscard]] static T CreateInstance(Args&&... args) {
    if constexpr (detail::activatorForwardsToConstructor<T, Args...>) {
        return T(std::forward<Args>(args)...);
    } else {
        return T{std::forward<Args>(args)...};
    }
}
```

Each conjunct is load-bearing, and §6 measures that removing either of the first
two is caught:

- **`!std::is_aggregate_v<T>`** — an aggregate has no constructor to forward to,
  so "forward to the constructor" is vacuous for one. Braces are also the only
  spelling that expresses brace elision, and the only one that still rejects
  narrowing. Dropping this conjunct routes aggregates through parentheses and
  loses both.
- **`std::is_class_v<T>`** — keeps scalars on braces, so
  `CreateInstance<int>(2.5)` stays the compile error it has always been.
- **`std::is_constructible_v<T, Args...>`** — the fallback that keeps every
  `initializer_list` call working.

The header also gained explicit `<type_traits>` and `<utility>` includes. It
previously relied on `<memory>` transitively supplying `std::forward`, which is
the third bullet of this report's own "Other missing assertions" section; the
sweep in `docs/CoreModuleHandleHeaderSelfSufficiencyPlan.md` §3.1 measured that
the header does compile standalone today, so this is hygiene rather than a fix,
and `<type_traits>` is required by the new predicate in any case.

### 4.1 The repaired contract, stated exactly

> Wherever `CreateInstance` and `CreateInstancePtr` are **both** well-formed,
> they now construct identically.

Measured on every comparable row of §3: `vector(3,7)`, `vector(1,2)`,
`Both(1,2)`, `Ctor(1,2)`, `Explicit(5)`, `Agg(1,2)` and `string(3,'x')` all
agree. Where only the value form is well-formed — aggregate brace elision and
`initializer_list` calls — it keeps today's behaviour exactly. That asymmetry is
**pre-existing and unchanged**: `std::make_unique` uses parentheses, so it
already rejected `array<int,3>(1,2,3)`, `AggNested(1,2,3)`, `vector<int>(1,2,3)`
and `OnlyInit(1,2,3)`, verified by real instantiation rather than by a detection
idiom (see §6.1). Option A would have "resolved" that asymmetry by deleting the
capability from the value form.

---

## 5. Compatibility, ABI and forwarding

| Property | Effect |
|---|---|
| Compile-domain losses | **none** — measured over all twelve categories |
| Compile-domain widenings | one, `string(3, 'x')`, which the pointer form already accepted |
| Behaviour changes | the three §3.1 rows, each the defect the finding names |
| Value category (`int&` / `const int&` / `int&&`) | identical |
| Move-only arguments | identical |
| Explicit constructors | identical (both spellings are direct-initialization) |
| Constructor exception propagation | identical |
| Narrowing rejection (aggregates, scalars) | **preserved** |
| Layout / vtable / `noexcept` / symbols | not applicable — header-only templates, no object representation |

`Activator` has no data members and no virtual functions; both overloads are
`static` templates. There is no object layout, vtable or exported symbol to
change.

---

## 6. Test and mutation matrix (#2266)

`modules/core/tests/System/ActivatorConstructionPathTests.cpp` — 14 tests, +14
over the 7 pre-existing `ActivatorTests`, which are retained unmodified and all
still pass. Coverage: the reproducer; value/pointer agreement; constructor over
`initializer_list`; `std::array`, nested-aggregate and plain-aggregate
initialization; `initializer_list` elements; the selection predicate itself over
twelve categories; value category; move-only arguments; explicit constructors;
constructor exceptions; and the §3.2 widening.

**A SFINAE probe on `CreateInstance` was written, and discarded as vacuous.**
Both overloads are unconstrained templates, so an ill-formed body is a hard
error rather than a substitution failure: the detection idiom reports *every*
call as well-formed. The compiler caught this because the two
narrowing-stays-rejected assertions failed. The compile domain is therefore
pinned two other ways — every preserved shape is really instantiated in the test
file, so losing one stops the file compiling, and the two shapes that must stay
**rejected** are pinned per-site by
`test/consumer/core_activator_construction_negative.cpp` (component
`Core.Base`, 2 sites), which is the only mechanism that can attribute a compile
rejection to its own source line.

| # | Mutation | Caught by | Result |
|---|---|---|---|
| M1 | Revert to unconditional braces (the defect) | test TU fails to compile at the §3.2 call; and at run time the reproducer reads `size=2 first=3` instead of `size=3 first=7` | **caught** |
| M2 | Unconditional parentheses (the audited option A) | test TU fails on `std::array` — *"array must be initialized with a brace-enclosed initializer"* — **and both** negative sites start compiling | **caught** |
| M3 | Drop `!is_aggregate_v<T>` | predicate assertion, **and** negative site 1 (aggregate narrowing) starts compiling while site 2 stays rejected | **caught** |
| M4 | Drop `is_class_v<T>` | predicate assertion, **and** negative site 2 (scalar narrowing) starts compiling while site 1 stays rejected | **caught** |

M3 and M4 each flip **exactly one** negative site, which is the evidence that
both conjuncts are independently load-bearing and that the two sites are not
redundant. M2 is the measured proof, from the repository's own tooling, that the
audited repair is a source break.

### 6.1 A measurement error caught during the review, recorded

The first draft of the review's evidence used
`decltype(std::make_unique<T>(...))` to decide whether the pointer form accepted
a category. That is unsound for the same reason the discarded SFINAE probe is:
`std::make_unique` is constrained only on `T` not being an array, so the failure
occurs in its body and is a hard error. It reported `array<int,3>(1,2,3)` as
accepted. Every claim in §4.1 about the pointer form's domain was therefore
re-measured by **real instantiation**, which reports it rejected. Detection
idioms were not trusted anywhere in this review without that check.

---

## 7. Sanitizers — not applicable, deliberately

No sanitizer run. The defect is constructor **selection**, fully determined at
compile time; both the before and after states are well-defined C++ that
constructs a valid object. There is no undefined behaviour, no lifetime question
and no memory error for a sanitizer to observe. `MoveOnlyArgument` and the
`std::vector`/`std::string` cases all run under the ordinary test binary.

---

## 8. CCF relationships — none minted, none extended

CCF-011 stays closed, CCF-019 stays open and untouched, CCF-021 and CCF-022 stay
unminted. This finding shares a *technique* with nothing else remediated in this
batch: SR-AUD-111 in the same batch is also a compile-domain defect in a
`modules/core` public header, and that is **adjacency, not a common cause** —
one is a violation of [dcl.fct]/12 in a header's include structure, the other is
an overload-resolution choice inside a template body. A shared batch is not a
family.

---

## 9. What remains open

The audited repair — unconditional parentheses — is **not** implemented and is
**not** owed. §3 measures it as a public source break across four categories
plus a loss of narrowing rejection in two more; under the SR-AUD-063 precedent
that needs an explicit approval. It is recorded here so a future ticket can pick
it up with one, but note that #2266 already repairs the finding's actual
complaint, so option A would now buy only strict symmetry with `make_unique`'s
narrower domain, at the cost of breaking `std::array` callers. No ticket is
opened for it.

---

## 10. Outcome, measured 2026-08-11

- `CreateInstance<std::vector<int>>(3, 7)`: `{3, 7}` → **`{7, 7, 7}`**.
- Value and pointer forms: **agree wherever both are well-formed**.
- Compile-domain losses: **zero**, measured over twelve categories.
- All 7 pre-existing `ActivatorTests` pass unmodified.
- Build: 0 errors, 0 warnings, `--parallel 2`.
- +14 tests; +1 negative fixture, +2 negative sites (15/126 → 16/128).
- 4 mutations, all caught, none equivalent.
- SR-AUD-109: `confirmed` → `remediated`.
