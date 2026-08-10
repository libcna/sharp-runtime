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
