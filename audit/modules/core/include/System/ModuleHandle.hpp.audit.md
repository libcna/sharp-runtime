# Audit: `modules/core/include/System/ModuleHandle.hpp`

## Metadata

- Audit status: AUDITED (48-line public handle stub, fully read with
  `RuntimeTypeHandle.hpp` and its consuming test sections).
- Validation: the combined runtime-handle filter passed 19/19 on 2026-07-26;
  the direct Batch15 runtime-handle filter passed 59/59 on 2026-07-27 and is
  fully audited in `Batch15TypesTests.cpp.audit.md`. However, standalone
  compile of a translation unit that includes `ModuleHandle.hpp`
  fails: `return type 'struct System::RuntimeTypeHandle' is incomplete` at the
  inline `ResolveTypeHandle` definition.
- Reference basis: local .NET `ModuleHandle`/`RuntimeTypeHandle` metadata-handle
  relationship and C++ public-header self-containment requirements.

## SR-AUD-111 — medium — ModuleHandle cannot be included as a standalone public header

`ModuleHandle.hpp` only forward-declares `RuntimeTypeHandle`, then defines the
body of `ResolveTypeHandle` with that incomplete return type
(`ModuleHandle.hpp:41-43`).  A C++20 consumer including this public header
directly fails before it can call any API.  The isolated
`/tmp/sharp-runtimervc-runtime-handle-audit-probe.cpp` reproduces the compiler
error.  Including `RuntimeTypeHandle.hpp` first masks the fault because that
header completes the cycle before including ModuleHandle.

The 19 passing tests do exactly that: `Batch15TypesTests.cpp` includes
`RuntimeTypeHandle.hpp` before `ModuleHandle.hpp`, while no standalone public
include fixture exists.  This is a consumer-facing compile regression even
though the no-metadata `ResolveTypeHandle` behavior itself is intentionally a
NotSupportedException stub.

### Status: REMEDIATED (#2263 review, #2264 implementation, 2026-08-11)

Reproduced exactly, at the cited line: a translation unit whose only include is
this public header fails with `return type 'struct System::RuntimeTypeHandle' is
incomplete` at `ModuleHandle.hpp:41`. The rule is [dcl.fct]/12 — a function
*definition* may not have an incomplete return type, although a *declaration*
may — so the `[[noreturn]]`/`throw` body is not what makes it ill-formed.

`ResolveTypeHandle` is now declared in-class and defined after a deferred
`#include "System/RuntimeTypeHandle.hpp"` at the bottom of the header, mirroring
the idiom `RuntimeTypeHandle.hpp:67-73` already used for the other half of the
same cycle. Both headers are `Core.Base`, so the new edge is intra-component:
the boundary validator still reports **41 modules / 92 edges** and no component
dependency was added.

**Premise refinement this report does not state.** Whether the defect was
systemic was measured rather than assumed, by compiling every `modules/core`
public header as the sole include of its own translation unit: **222 of 223 were
already self-sufficient, and this header was the only exception** — including
all five sibling handle headers. It is therefore a true singleton, and no
ordinary defect ticket was minted for a neighbouring header because there is no
affected neighbour. After the repair the sweep reads **223/223**.

Regression coverage is
`modules/core/tests/System/ModuleHandleStandaloneIncludeTests.cpp`, whose first
include — ahead of `<gtest/gtest.h>` — is `System/ModuleHandle.hpp`, carrying
three `static_assert`s and six tests. Four mutations: reverting the in-class
body, hoisting the deferred include above the class, and deleting the deferred
include are all caught; restoring the masking include order is labelled
**equivalent by design**, and is exactly why `Batch15TypesTests.cpp` (which
includes `RuntimeTypeHandle.hpp` at line 8 before this header at line 11) could
never have observed the fault.

Behaviour, layout and symbols were diffed from one probe source compiled against
both header revisions in the same include order: `sizeof`/`alignof` 1/1,
trivially-copyable/standard-layout/aggregate unchanged, the stub's message
byte-identical, the `noexcept` surface unchanged, and the `ModuleHandle` symbol
table identical (an in-class and an out-of-class `inline` member function mangle
the same). The change is purely additive to the set of translation units that
compile. +6 tests. `docs/CoreModuleHandleHeaderSelfSufficiencyPlan.md`.

## Other missing assertions and diagnostics

- No direct fixture tests `ResolveTypeHandle` through a header-isolated
consumer, arbitrary metadata tokens, exception message/category, or include
order permutations.
- All ModuleHandle values compare equal and hash zero by documented design;
there is no capability query that tells callers metadata cannot exist before a
throwing resolution call.

## Final assessment

The no-metadata behavior is explicit, but the published header has a concrete
self-containment failure.  No source or test was modified during this audit.
