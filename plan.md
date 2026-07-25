# Sharp Runtime plan

*Last verified: 2026-07-25 — code baseline `03c7d4bb`; 40 physical
components, 88 direct production dependency edges, clean build, and 12,586
passing tests across 36 executables; Text.Json selective isolation currently
fails because `Collections.Core` pulls `Threading`.*

Sharp Runtime is in a consumer-driven expansion phase. The original type
classification and stabilization queues are complete, and the full native
build/test baseline is healthy. A post-modular API addition has, however,
reintroduced an unwanted dependency closure and currently breaks one
selective CI job. Restoring that boundary is the first priority.

## Sources of truth

Planning is deliberately split:

- `plan.md` is the versioned roadmap and milestone index.
- `NEXT.md` is the current cold-start handoff and ordered list of bounded next
  tasks.
- `plan.sqlite3` is the local, git-ignored detailed database:
  - `task` classifies .NET types.
  - `ticket` records concrete stabilization and architecture work.
- `CLAUDE.md` defines non-negotiable implementation and validation rules.
- `prompt.md` defines the database workflow.

The old namespace-table workflow and `plan_namespaces.md` were retired and
removed in commit `528d9ab7`. `plan_files.md` was referenced historically but
was never created. Neither file should be linked as current documentation.

## Current measured state

### Code and validation

- Native Linux/GCC build: zero errors and zero warnings.
- Tests: 12,586 passing across 35 component binaries plus one integration
  binary.
- Component graph: 40 physical modules and 88 direct production edges.
- Boundary validator: no cycles, duplicate public include paths, orphan
  files, unresolved includes, undeclared edges, stale edges, or visibility
  mismatches.
- Dependency allow-list: empty.
- Selective matrix: Core.Base and the Text.Json test binary pass, but the
  Text.Json isolation assertion fails because `sharp_runtime_threading` is
  configured unexpectedly. The full nine-job matrix is therefore not green.
- Tracked CI: Ubuntu selective matrix and full compatibility build in
  `.github/workflows/components.yml`; its Text.Json job is expected to
  reproduce the local isolation failure.

### Local planning database

The 2026-07-25 local snapshot contains:

| Table | State |
|---|---|
| `task` | 16,201 rows: 1,082 `ported`, 140 `ignore`, 14,979 legacy `ignored`; no unclassified or `tobedecided` rows |
| `ticket` | 1,736 rows, all `done`; no `todo`, `doing`, `blocked`, or `needs_user` rows |

Because `plan.sqlite3` is git-ignored, these counts describe the maintainer
snapshot, not data shipped in a fresh clone.

## Active architecture regression

`scripts/check_selective_components.sh` currently stops in the `Text.Json`
job with:

```text
FAIL: selective build unexpectedly configured target sharp_runtime_threading
```

The cause is commit `227111cb`: `BlockingCollection.hpp` publicly includes
`CancellationToken`, `CancellationTokenRegistration`, and `Timeout`, so the
commit added `Collections.Core -> Threading`. Since `Threading -> TimeZone`,
every consumer that needs only `Collections.Core` now also configures both
archives.

Affected closures include the four isolation examples that motivated the
Collections split:

| Requested component | Current unwanted additions |
|---|---|
| `Text.Json` | `Threading`, `TimeZone` |
| `Net.Http.Headers` | `Threading`, `TimeZone` |
| `Net.Mime` | `Threading`, `TimeZone` |
| `Numerics` | `Threading`, `TimeZone` |

The boundary validator still passes because the new edge is declared
correctly; this is a product-level closure regression, not an undeclared-edge
error.

**Recommended remediation:** create a narrow physical component (provisional
name `Collections.Blocking`), move `BlockingCollection.hpp` and its dedicated
tests there, give it public dependencies on `Collections.Core`, `Core.Base`,
and `Threading`, and include it in the `Collections` compatibility umbrella.
The other concurrent collection headers do not include `System/Threading/*`
and can remain in `Collections.Core`, avoiding an unnecessary compatibility
move. Then remove `Threading` from `Collections.Core`, add/update consumer
fixtures, regenerate the catalogue, and run the full selective matrix plus
native gate.

Keeping the broader closure and weakening the negative fixture is possible
but contradicts MOD-003/MOD-006's accepted isolation goal. It should be done
only after an explicit architecture decision.

## Completed milestones

### Porting and stabilization

- Classified the indexed .NET type surface and completed the original
  porting/stabilization queue.
- Established fixed-width public API aliases, property/indexer naming, SPDX
  headers, .NET-reference review, and regression-test requirements.
- Completed native TSan, ASan, and UBSan passes during stabilization and
  fixed the production findings discovered by those runs.
- Added consumer-driven coverage across core, collections, IO, networking,
  threading/tasks, text/JSON, XML, numerics, globalization, and cryptographic
  hashing/random APIs.

### Platform work

- MinGW library cross-build audit completed under ticket #40.
- Emscripten library cross-build audit completed under ticket #41.
- Real downstream Apple Clang/Xcode 15.4 builds drove the portability fixes
  in commits `1d22a7b2` through `b797928f`.

These results predate or only partially overlap the final component
architecture. They are evidence of portability, not a current cross-platform
test matrix.

### Modular architecture

The remediation plan MOD-001 through MOD-008 was implemented in
`b0e944ad`, documented in `27e4d680`, and closed as tickets 1729–1736.
It delivered:

- One physical owner for every production header, source, and module test.
- Explicit public, private, and test-only dependency visibility.
- Narrow `Core.Base`, `Collections.Core`, `Collections.Async`, and
  `Collections.ObjectModel` targets while preserving compatibility umbrellas.
- Component-scoped test executables and a separate integration executable.
- Automated boundary validation, catalogue generation, isolated consumers,
  negative fixtures, and GitHub Actions coverage.
- Generated component documentation in `docs/ComponentCatalog.md`.

The graph landed with 85 production edges and has since grown to 88. The
validator and generated catalogue remain green, but the
`Collections.Core -> Threading` edge is the active closure regression
described above.

### Post-modular API expansion

The first consumer-driven ports after modularization added:

- Runtime compiler-services helpers, including `ConditionalWeakTable` and
  `RuntimeHelpers`.
- Component-model notification, initialization, and async-completion
  metadata.
- HTTP handler/invoker/request-option primitives and web-proxy APIs.
- `ConcurrentBag` and `BlockingCollection`.
- `TaskExtensions::Unwrap` for generic and non-generic nested tasks.
- XML schema exception types.

The verified test baseline grew from 12,494 at the modularization checkpoint
to 12,586.

## Candidate roadmap

No implementation is active yet. Create or reopen a `ticket` row with
acceptance criteria and a validation command before changing code.

### P0 — Restore the accepted component boundary

1. **Split the threading-dependent blocking collection from
   `Collections.Core`.**
   Implement the `Collections.Blocking` remediation described in the active
   regression section. Acceptance requires the Text.Json negative assertion,
   all nine selective jobs, the boundary/catalogue checks, and the 12,586-test
   native floor to pass.

### P1 — Confirmed parity and correctness gaps

2. **Fix `MemoryStream(buffer, size)` writability parity.**
   The constructor currently sets `writable_` to `false`; the corresponding
   .NET single-buffer constructor is writable. Audit callers that may depend
   on the current behavior, change the default, and add a regression test that
   fails before the fix.

3. **Add `TaskT<TResult>::ContinueWith`.**
   Non-generic `Task::ContinueWith` exists, and `TaskT` already has completion
   synchronization used by `TaskExtensions::Unwrap`, but the generic
   continuation list/API is still absent. Preserve weak ownership and verify
   success, fault, cancellation, option filtering, chaining, and leak-free
   teardown.

4. **Close the `XText::WriteTo` whitespace distinction.**
   `XText` always calls `XmlWriter::WriteString`; .NET calls
   `WriteWhitespace` for text directly under `XDocument`. This requires a
   deliberate `XmlWriter` API addition and tests before changing `XText`.

### P1 — Revalidation after architectural and concurrency changes

5. **Re-run MinGW and Emscripten library builds on the post-modular tree.**
   The recorded cross-builds predate the 40-component graph. Validate an
   `All` build and at least one selective build, record exact toolchain
   versions, and distinguish compile success from runtime test coverage.

6. **Run focused sanitizer passes over new concurrent code.**
   Prioritize `ConcurrentBag`, `BlockingCollection`,
   `TaskExtensions::Unwrap`, and `ConditionalWeakTable`. Use TSan for
   synchronization paths and ASan/LSan for continuation/ownership teardown;
   keep any sanitizer-only test adaptations separate from production fixes.

### P2 — Consumer-driven API breadth

7. **Choose a bounded `ImmutableList<T>` slice.**
   Its documented omissions include sorting/reversing, copy/range/conversion,
   predicate search, builder support, and comparer overloads. Do not attempt
   the entire surface in one change; select methods required by a real
   consumer and port them against the .NET reference.

8. **Complete only demanded `BinaryReader` character APIs.**
   `ReadChar` and `ReadDecimal` are implemented, while `PeekChar`,
   `ReadChars`, and `Read(char[])` remain deliberately absent. Add them only
   when a consumer needs them, preserving decoder state and truncated-input
   behavior.

9. **Review other documented partial surfaces by demand.**
   Examples include `BigInteger` bitwise operations, full UTF-7 behavior,
   debugger/process breadth, and richer XML reader/writer functionality.
   A documented partial API is not automatically higher priority than a
   consumer-visible bug.

### P2 — Developer experience

10. **Reduce the Doxygen warning backlog incrementally.**
   Establish a reproducible baseline first, then require touched public APIs
   not to regress it. Avoid a mass comment-only rewrite.

11. **Decide whether distribution support is wanted.**
    The repository currently supports `add_subdirectory`; it has no installed
    package/export configuration and no standalone sample application. Add
    install rules, package config, or a sample only after the desired consumer
    workflow is selected.

### Requires explicit user direction

- Adding Windows, macOS, or Emscripten jobs to the repository CI matrix.
- Introducing a new third-party dependency.
- Broad public-header renames or compatibility-breaking refactors.
- Expanding permanent out-of-scope areas such as reflection, TLS/X.509, or
  symmetric/asymmetric encryption.

## Definition of done for future work

A task is complete only when all applicable items hold:

1. The expected behavior is verified against the actual .NET source under
   `/rv/tmp/runtime/src/libraries/`, not memory or an old audit statement.
2. The change has one clear physical module owner and declares only necessary
   public/private/test dependency edges.
3. A regression test demonstrates any corrected behavior; existing tests are
   not weakened to hide a failure.
4. `scripts/local_ci_check.sh build` passes with zero warnings/errors and the
   test count does not decrease without a documented reason.
5. Boundary/catalogue checks pass, and selective fixtures are updated when a
   component surface or dependency changes.
6. Concurrency, lifetime, or low-level memory changes receive the relevant
   sanitizer pass.
7. `README.md`, `NEXT.md`, component documentation, and `plan.sqlite3` are
   updated when their stated facts change.
8. The focused change is committed and pushed only according to the branch
   policy in `CLAUDE.md`.
