<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `ModuleHandle.hpp` public-header self-sufficiency — plan

Ticket #2263. One frozen audit finding in
`modules/core/include/System/ModuleHandle.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-111 | medium | `ModuleHandle` cannot be included as a standalone public header |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **singleton on one member-function
definition**, not a `ModuleHandle` review and not a header-hygiene programme.

---

## 1. Scope

In scope: the placement of `ModuleHandle::ResolveTypeHandle`'s *body*, and the
one include needed to make that placement legal.

Out of scope, deliberately: the `ModuleHandle` API surface itself. Every method
on this type is an intentional no-metadata stub, which the audit report states
and this ticket does not revisit. `ResolveTypeHandle` throwing
`NotSupportedException` is the correct end state per the permanent reflection
deviation in `CLAUDE.md`; the finding is about the header failing to *compile*,
not about what it does once it does.

---

## 2. Root cause

`ModuleHandle.hpp` forward-declares `RuntimeTypeHandle` (line 13) and then, at
line 41, **defines** `ResolveTypeHandle` with that still-incomplete type as its
return type:

```cpp
struct RuntimeTypeHandle;            // forward declaration only
// ...
[[noreturn]] RuntimeTypeHandle ResolveTypeHandle(intcs typeToken) const {
    throw System::NotSupportedException("...");
}
```

[dcl.fct]/12 forbids exactly this: the return type of a function **definition**
may not be an incomplete class type. A *declaration* with the same return type
is well-formed, which is what makes the repair a one-line move rather than a
redesign. The `[[noreturn]]`/`throw` body is irrelevant to the rule — the
constraint is on the declared return type, not on whether the function can
return.

The two headers form a genuine cycle: `ModuleHandle::ResolveTypeHandle` returns
a `RuntimeTypeHandle` by value and `RuntimeTypeHandle::GetModuleHandle` returns
a `ModuleHandle` by value. `RuntimeTypeHandle.hpp` had already solved its half
correctly — it declares `GetModuleHandle` in-class, then includes
`ModuleHandle.hpp` *after* its own class is complete and defines the body there
(`RuntimeTypeHandle.hpp:67-73`). `ModuleHandle.hpp` did not mirror that, so the
cycle was resolvable from one entry point only.

That asymmetry is also the masking the finding describes: including
`RuntimeTypeHandle.hpp` first completes `RuntimeTypeHandle` before
`ModuleHandle.hpp` is ever reached, so the illegal definition compiles.

---

## 3. Before evidence, measured 2026-08-11

A standalone translation unit whose only include is the public header:

```
$ g++ -std=c++23 -fsyntax-only -I<module include roots> probe.cpp
modules/core/include/System/ModuleHandle.hpp:41:94: error: return type
    'struct System::RuntimeTypeHandle' is incomplete
```

This reproduces the finding exactly, including its cited line. The audit's
original probe path was under `/tmp`; this one is `build-probe/`, per the
build-resource policy.

### 3.1 Premise refinement — how widespread is this?

The finding does not say whether the defect is systemic. It was measured rather
than assumed, over **every** public header in the module — each compiled as the
sole include of its own translation unit:

| Sweep | Headers | Self-sufficient | Not self-sufficient |
|---|---|---|---|
| `modules/core/include/**/*.hpp`, before | 223 | 222 | **1** (`System/ModuleHandle.hpp`) |
| `modules/core/include/**/*.hpp`, after | 223 | **223** | 0 |

The five sibling handle headers (`RuntimeTypeHandle`, `RuntimeMethodHandle`,
`RuntimeFieldHandle`, `RuntimeArgumentHandle`, `RuntimeType`) are all clean.

**Consequence for this batch:** SR-AUD-111 is a true singleton, so no ordinary
new defect ticket is minted for a neighbouring header — there is no neighbour to
mint one for. Had the sweep found others, they would have become ordinary
project tickets rather than an extension of the frozen finding, following the
#2259 precedent.

### 3.2 What the existing tests do and do not pin

`Batch15TypesTests.cpp` includes `RuntimeTypeHandle.hpp` at line 8 and
`ModuleHandle.hpp` at line 11 — the masking order. Its five `ModuleHandleTests`
therefore exercise the type's behaviour but can never observe the compile
defect. `RuntimeTypeHandleTests.cpp` enters from the already-working side. No
translation unit in the repository, test or consumer, named this header first.

---

## 4. The repair (#2264)

`ResolveTypeHandle` is declared in-class and defined after a deferred include,
mirroring `RuntimeTypeHandle.hpp`'s existing idiom so the cycle is resolvable
from **either** entry point:

```cpp
    [[noreturn]] RuntimeTypeHandle ResolveTypeHandle(intcs typeToken) const;   // declaration
};
// ...
#include "System/RuntimeTypeHandle.hpp"                                       // deferred

namespace System {
[[noreturn]] inline RuntimeTypeHandle
ModuleHandle::ResolveTypeHandle([[maybe_unused]] intcs typeToken) const {
    throw System::NotSupportedException("ModuleHandle.ResolveTypeHandle is not supported.");
}
}
```

Why this and not the alternatives:

- **Including `RuntimeTypeHandle.hpp` at the top instead** does not work and is
  not a matter of taste. Under `#pragma once`, entering from `ModuleHandle.hpp`
  would reach `RuntimeTypeHandle.hpp`, whose own trailing
  `#include "System/ModuleHandle.hpp"` is then a no-op, so
  `RuntimeTypeHandle::GetModuleHandle`'s body would compile with `ModuleHandle`
  still incomplete. This was measured as mutation M2 in §7 and fails exactly
  there.
- **Changing the return type** would break the .NET-shaped signature.
- **Fixing the test's include order** would hide the finding rather than repair
  it, and is explicitly excluded.

### 4.1 Include-graph and boundary effect

`System/ModuleHandle.hpp` and `System/RuntimeTypeHandle.hpp` are both in
`modules/core/include/System/` and both belong to component **`Core.Base`**. The
new edge is therefore *intra-component*: no module boundary is crossed, no
component dependency is added, and no include cycle is introduced that was not
already there and already resolved by the same idiom.

`scripts/validate_module_boundaries.py`: **41 physical modules, 92 dependency
edges** — identical to the inherited value. The generated catalogue is unchanged
and selective-components does not need to run for this ticket (§8).

The one first-party header that includes `ModuleHandle.hpp` is
`modules/runtime/include/System/Runtime/CompilerServices/RuntimeHelpers.hpp`;
it now transitively receives `RuntimeTypeHandle.hpp`, a 75-line header whose own
includes are `<cstdint>` and the shared helper. It was compiled standalone as
part of §7's order matrix.

---

## 5. Compatibility, ABI, layout and `noexcept`

Measured before/after from one probe source compiled against both header
revisions in the *same* (masking) include order, so nothing can drift:

| Property | Before | After |
|---|---|---|
| `sizeof(ModuleHandle)` / `alignof` | 1 / 1 | 1 / 1 |
| trivially copyable / standard layout / aggregate | 1 / 1 / 1 | 1 / 1 / 1 |
| `getMDStreamVersionProperty()` | 0 | 0 |
| `GetHashCode()` | 0 | 0 |
| `==` / `!=` / `Equals` vs `EmptyHandle` | 1 / 0 / 1 | 1 / 0 / 1 |
| `ResolveTypeHandle(42)` | throws `NotSupportedException` | throws `NotSupportedException` |
| exception message | `ModuleHandle.ResolveTypeHandle is not supported.` | *(byte-identical)* |
| `noexcept` (md, Equals, hash, `==`, `!=`, Resolve) | 1,1,1,1,1,0 | 1,1,1,1,1,0 |
| `ModuleHandle` symbols in the object file | — | **identical**, diffed |

A member function defined in-class and one defined out-of-class with `inline`
are both implicitly inline and mangle identically
(`_ZNK6System12ModuleHandle17ResolveTypeHandleEi`, weak in both revisions), so
there is no signature, layout, vtable, `noexcept` or symbol change. The change
is **purely additive to the set of translation units that compile**: every TU
that compiled before still compiles and behaves identically, and TUs that
previously failed now succeed.

`[[noreturn]]` is applied to the first declaration, as
[dcl.attr.noreturn] requires, and repeated on the definition in the same
translation unit, which is permitted.

---

## 6. CCF relationships — none minted, none extended

CCF-011 stays closed, CCF-019 stays open and untouched, CCF-021 and CCF-022 stay
unminted. A single header whose author did not mirror a neighbouring header's
idiom is one defect, not a cross-cutting family: the §3.1 sweep measured that
222 of the other 223 headers do not share it, which is the evidence that would
have been required to argue recurrence. Adjacency to the other `Runtime*Handle`
headers is not membership.

---

## 7. Regression mechanism and mutation matrix (#2264)

`modules/core/tests/System/ModuleHandleStandaloneIncludeTests.cpp` is a new
suite whose **first** include — ahead of `<gtest/gtest.h>` and every other
`System` header — is `System/ModuleHandle.hpp`. It carries three
`static_assert`s pinning that both handle types are complete and that
`ResolveTypeHandle` still returns `RuntimeTypeHandle` by value, plus six runtime
tests. The file's header comment forbids reordering the include, because that is
precisely the masking the finding is about.

A consumer fixture was considered and rejected as the wrong instrument here: the
`Core.Base` slot in the selective matrix is already held by `core_base.cpp`,
which includes other `System` headers first, so pinning *this* contract there
would mean restructuring an unrelated fixture and forcing a selective-components
rerun for a defect that is entirely intra-component and fully observable from a
single translation unit.

Mutations, each rebuilt and recompiled against the fixture:

| # | Mutation | Expected | Result |
|---|---|---|---|
| M1 | Restore the original in-class definition | rejected | **caught** — header error plus 2 of the 3 `static_assert`s |
| M2 | Keep the deferred include but hoist it above the class | rejected | **caught** — fails at `RuntimeTypeHandle.hpp:71`, exactly as §4 predicts |
| M4 | Out-of-line declaration but delete the deferred include | rejected | **caught** |
| M3 | Original header **and** the masking include order in the fixture | compiles | **compiles — equivalent by design** |

M3 is labelled equivalent deliberately and is the point of the exercise: it
shows the defect is invisible to any suite that includes `RuntimeTypeHandle.hpp`
first, so the fixture's value lies entirely in its include-order discipline, not
in its assertions. It is the mutation that justifies the comment forbidding a
reorder.

Order matrix, all with `-Wall -Wextra -Wpedantic -Werror`, all passing after the
repair (all failed or were untested before): `ModuleHandle.hpp` alone;
`RuntimeTypeHandle.hpp` alone; both, `ModuleHandle` first; both,
`RuntimeTypeHandle` first (the pre-existing masking order); and
`RuntimeHelpers.hpp`, the real first-party includer.

---

## 8. Sanitizers and selective-components — not applicable, deliberately

No sanitizer run: the defect is a translation-time constraint violation with no
runtime component, and the repair executes no new code. `ResolveTypeHandle`'s
body is byte-for-byte the statement it always was.

No selective-components run: that matrix exists to prove component isolation,
and this ticket adds no component edge, changes no `PUBLIC_DEPENDENCIES`,
`PRIVATE_DEPENDENCIES` or `TEST_DEPENDENCIES`, and leaves the boundary validator
at the same 41/92 with the catalogue current. The new include is between two
headers of the same component.

---

## 9. Outcome, measured 2026-08-11

- Standalone include of `System/ModuleHandle.hpp`: **fails → compiles**.
- `modules/core` public-header self-sufficiency: **222/223 → 223/223**.
- Build: 0 errors, 0 warnings, `--parallel 2`.
- +6 tests, all passing; 4 mutations, 3 caught and 1 labelled equivalent.
- No signature, layout, vtable, `noexcept` or symbol change; symbol tables
  diffed identical.
- SR-AUD-111: `confirmed` → `remediated`.
