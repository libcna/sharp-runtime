<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# NEXT.md

*Last verified: 2026-07-25. Branch: `feature/work`. The P0 component-boundary
repair and first P1 parity repair are complete: 41 physical modules, 90
production dependency edges, and 12,588 tests across 37 executables.*

This is the cold-start handoff for the next working session. Keep it focused
on verified facts, remaining bounded work, and commands needed to resume.
Historical session detail belongs in git history and `plan.sqlite3`.

## Current state

- `Collections.Blocking` owns `BlockingCollection<T>` and its eight tests.
  It depends publicly on `Collections.Core`, `Core.Base`, and `Threading`.
- `Collections.Core` owns the remaining synchronous/non-blocking collection
  surface and depends publicly only on `Core.Base`.
- `SharpRuntime::Collections` remains the compatibility umbrella over Core,
  Blocking, Async, and ObjectModel collections. Public include paths and
  namespaces did not change.
- `Text.Json` now configures only `Core.Base`, `Buffers`, `Text`,
  `Collections.Core`, and `Text.Json`; it excludes `Threading` and `TimeZone`.
- The module validator reports 41 physical modules and 90 production edges;
  the dependency allow-list is empty. The generated catalogue is current.
- The ten-job selective consumer matrix, including a direct
  `Collections.Blocking` consumer, is green. Text.Json retains its target
  absence and negative include-leakage assertions.
- The full native baseline is a warning-free build with 12,588 passing tests
  across 36 component executables and one integration executable.

The local `plan.sqlite3` snapshot contains 16,201 classified `task` rows and
1,738 completed tickets. Ticket #1737 records the completed P0 split and
ticket #1738 records the MemoryStream repair. The
database is git-ignored and is not part of a fresh clone.

## P0 completion: restore Collections isolation

The `BlockingCollection` port had made `Collections.Core` publicly depend on
`Threading`, which caused ordinary consumers such as `Text.Json`,
`Net.Http.Headers`, `Net.Mime`, and `Numerics` to configure both `Threading`
and `TimeZone`. The repair moved only `BlockingCollection.hpp` and its
dedicated tests into `modules/collections-blocking`.

Do not move `BlockingCollection<T>` back into `Collections.Core` or weaken the
Text.Json negative assertion. The narrow component is intentional: consumers
that need blocking, cancellation, and timeout semantics select
`Collections.Blocking`; unrelated collections consumers keep the lean closure.

## P1 completion: MemoryStream buffer constructor

`MemoryStream(buffer, size)` now follows .NET's single-buffer constructor and
is writable by default. The port retains its copying ownership model. Callers
that require a read-only stream pass `false` explicitly: this preserves the
contracts of `BinaryData::ToStream()` and read-mode `ZipArchiveEntry::Open()`.
Regression tests cover writing, resizing, and BinaryData's protected read-only
stream.

## Recommended next bounded tasks

Choose one, create a ticket, and keep the changes isolated.

1. **`TaskT<TResult>::ContinueWith`.** Add the missing generic continuation
   API with success, fault, cancellation, option filtering, chaining, and
   lifetime tests.
2. **`XmlWriter::WriteWhitespace` plus `XText` parity.** `XText::WriteTo`
   currently always writes a string; match the document-child whitespace rule
   through a deliberate writer API addition.
3. **Post-modular portability evidence.** Re-run documented MinGW and
   Emscripten library configurations for `All` and one selective component;
   record exact toolchains and distinguish build evidence from runtime tests.
4. **Focused sanitizers.** Run TSan for `ConcurrentBag`/`BlockingCollection`
   and ASan/LSan for task/weak-ownership teardown. Keep test adaptations
   separate from production fixes.

`ImmutableList<T>` breadth, remaining `BinaryReader` character APIs,
`BigInteger` bitwise operations, fuller UTF-7 behavior, and wider
debugger/process/XML surfaces remain consumer-driven P2 work.

## Useful commands

```bash
# Inspect repository and planning state
git status --short --branch
sqlite3 plan.sqlite3 \
  "SELECT ticket_no, priority, status, title FROM ticket WHERE status IN ('todo', 'doing') ORDER BY priority, ticket_no;"

# Full local gate: validator, catalogue, warning-free build, all tests
scripts/local_ci_check.sh build

# Selective closure and consumer isolation checks
scripts/check_selective_components.sh

# Focused new component check
scripts/check_selective_components.sh Collections.Blocking blocking_collection.cpp

# Metadata/catalogue only
python3 scripts/validate_module_boundaries.py
python3 test/validate_module_boundaries_test.py
python3 scripts/generate_component_catalog.py --check
```

HTTP, socket, and ping tests require permission for local network operations.

## Guardrails

- Preserve narrow physical dependencies; internal modules must not link the
  `Core`, `Collections`, or `All` compatibility umbrellas.
- Public-header edges are `PUBLIC_DEPENDENCIES`, source-only edges are
  `PRIVATE_DEPENDENCIES`, and test-only edges are `TEST_DEPENDENCIES`.
- Do not restart completed naming, integral-alias, or project-wide
  classification work.
- Do not add cross-platform CI, dependencies, or broad public-header refactors
  without direction.
- Push only to `feature/work`; do not merge to `develop`/`master` or create
  tags without explicit approval.

## Cold resume

1. Read `CLAUDE.md`, this file, and `plan.md`.
2. Inspect `git status --short --branch` and open tickets.
3. Run `scripts/local_ci_check.sh build` and
   `scripts/check_selective_components.sh` before starting new work.
4. Create a bounded ticket for the selected task, implement it, then update
   the measured baseline and this handoff.
