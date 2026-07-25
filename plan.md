# plan.md — sharp-runtime planning index
*Last updated: 2026-07-25 — modular-architecture remediation implemented and locally verified; current test baseline: 12494 passing*

sharp-runtime is a C++23 static library reimplementing a practical subset of .NET `System.*` for **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game).

Reference source: `/rv/tmp/runtime/src/libraries/` (dotnet/runtime, MIT License)

---

## Planning is now database-driven

The namespace-planning material in this file and
[plan_namespaces.md](plan_namespaces.md) is **historical** — the interactive,
per-namespace-table workflow it describes has been replaced by `plan.sqlite3`, a git-ignored
SQLite database that is the live, authoritative source of truth. The remediation plan below was
approved and implemented; its work items are tracked as tickets 1729–1736. They remain `doing`
until the changes are committed and pushed, as required by the repository ticket policy. See
`README.md`'s "Tracking: plan.sqlite3" section for the full explanation of its two tables (`task`
for .NET type classification, `ticket` for stabilization work).

```bash
# Namespace-level porting status
sqlite3 plan.sqlite3 "SELECT namespace, status, COUNT(*) FROM task GROUP BY namespace, status ORDER BY namespace;"

# Overall status counts
sqlite3 plan.sqlite3 "SELECT status, COUNT(*) FROM task GROUP BY status ORDER BY COUNT(*) DESC;"

# Stabilization ticket queue
sqlite3 plan.sqlite3 "SELECT status, priority, COUNT(*) FROM ticket GROUP BY status, priority ORDER BY priority, status;"
```

`plan_files.md`, referenced by an earlier version of this file, was never created and does not exist.

## Implemented modular-architecture remediation

**Status:** implementation and local acceptance audit complete on 2026-07-25. No production C++
behavior was changed and no public `System::*` API was removed; the work concerns physical
ownership, CMake usage requirements, dependency isolation, tests, and documentation. The final
fresh build produced zero warnings and zero errors, and 12494 tests passed exactly once across 36
executables.

The tasks are ordered. A later task may start only after its listed dependencies and acceptance
criteria are complete.

### MOD-001 — Add an executable module-boundary validator (P1) — implemented

**Goal:** turn the current manually verified module graph into an enforceable invariant before
moving files or changing targets.

**Work:**

- Inventory every `modules/<module>` directory and require it to be registered exactly once.
- Require every production `.cpp` and public header to have exactly one physical owner.
- Reject duplicate logical public include paths across module include roots.
- Parse project-local `System/...` and `SharpRuntime/...` includes and reject unresolved includes,
  undeclared cross-module edges, dependency cycles, and stale declared edges.
- Distinguish includes originating in public headers from includes used only by implementation
  sources so the validator can enforce the visibility model introduced by MOD-002.
- Add a narrowly documented allow-list mechanism for genuine link-only or generated dependencies;
  an allow-list entry must include its reason and owner.
- Run the validator from the normal local/CI validation path.

**Acceptance criteria:**

- The validator passes on the pre-refactor tree and reports the current 35 physical modules as an
  acyclic graph.
- Deliberate fixtures prove that an orphan header, duplicate include path, undeclared edge, stale
  edge, and cycle each fail validation with an actionable message.
- The validator does not modify the source tree or depend on an existing build directory.

**Depends on:** none.

### MOD-002 — Model public and private component dependencies explicitly (P1) — implemented

**Goal:** stop implementation-only include roots and usage requirements from leaking to consumers.

**Work:**

- Extend `sharp_runtime_register_module()` with explicit public and private dependency fields.
- Map public dependencies to `PUBLIC`/`INTERFACE` CMake usage requirements and implementation-only
  dependencies to `PRIVATE` link requirements, preserving static-library link closure.
- Migrate every module declaration and remove or deprecate the ambiguous catch-all dependency
  field after all callers are converted.
- Initially classify these currently implementation-only edges as private:
  `IO.Compression.Zip -> Core`, `IO.IsolatedStorage -> Storage`,
  `Net.WebSockets -> Net`, `Text.Json -> Collections`, `Timers -> Threading`, and
  `Xml -> Diagnostics`.
- Keep platform/vendor dependencies private unless a public header exposes their types. Preserve
  tinyxml2 as a public XML dependency while `XmlDocument.hpp` exposes tinyxml2 types.
- Preserve `SharpRuntime::All`, all component aliases, and the legacy `SHARP_RUNTIME` target.

**Acceptance criteria:**

- MOD-001 confirms that every public-header edge is public and every source-only edge is private
  or explicitly justified.
- A consumer linking a component cannot compile against an implementation-only component merely
  through leaked include paths.
- Full and representative selective builds succeed on the supported toolchains with no new
  compiler warnings.

**Depends on:** MOD-001.

### MOD-003 — Split the oversized Collections dependency closure (P1) — implemented

**Goal:** using a basic collection must not pull `Threading` and `ComponentModel`.

**Work:**

- Introduce a lean physical target for collection fundamentals (working name
  `SharpRuntime::Collections.Core`) containing the generic, specialized, immutable, frozen, and
  concurrent collection facilities that do not require higher layers.
- Move asynchronous enumeration contracts into
  `SharpRuntime::Collections.Async`, depending on the lean collection target and the minimal
  cancellation/threading component.
- Move the observable `System::Collections::ObjectModel` types and their notification-specific
  support into `SharpRuntime::Collections.ObjectModel`, depending on the lean collection target
  and `ComponentModel`. Keep dependency-free ObjectModel wrappers such as `ReadOnlyCollection`
  with the lean target because `List<T>` exposes them directly.
- Retain `SharpRuntime::Collections` as a compatibility umbrella exposing the same header set as
  before.
- Change `Text.Json`, `Net`, `Net.Http.Headers`, `Net.Mime`, and `Numerics` to depend on the
  narrowest collection target actually used by their public headers or sources.
- Preserve all existing include spellings and `System::*` namespaces.

**Acceptance criteria:**

- A selective `Text.Json` build no longer configures or builds `Threading` or `ComponentModel`.
- Selective `Net.Http.Headers`, `Net.Mime`, and `Numerics` builds no longer acquire unrelated
  collection subcomponents.
- Existing consumers linking `SharpRuntime::Collections` retain source compatibility.
- All collection tests run against their owning narrow target and pass.

**Depends on:** MOD-001, MOD-002.

### MOD-004 — Reduce Core to a deliberate foundation layer (P1) — implemented

**Goal:** prevent every component from compiling and exposing optional high-level Core clusters
while retaining a stable compatibility target.

**Work:**

- Produce and commit a Core ownership map grouping its headers and implementations by primitive,
  exception, memory/span, time, URI, console, runtime-compatibility, and cross-namespace
  cycle-breaking roles.
- Introduce a lean foundation target (working name `SharpRuntime::Core.Base`) and preserve
  `SharpRuntime::Core` as a compatibility umbrella.
- Extract at least the demonstrably optional URI, Console, and TimeZone clusters into explicit
  components when the MOD-001 graph proves the extraction acyclic.
- Change internal modules to depend on `Core.Base` plus only the extracted clusters they actually
  include; they must not use the compatibility umbrella internally.
- Keep `System::IO::IOException`, `System::IO::DirectoryNotFoundException`, and other
  cross-namespace foundation types in the base only when the ownership map documents the concrete
  cycle they prevent.
- Preserve all public header paths, namespaces, exception behavior, and the legacy
  `SharpRuntime::Core` consumer surface.

**Acceptance criteria:**

- The resulting graph is acyclic and MOD-001 reports no undeclared or stale edges.
- A basic selective component such as `Text.Json` does not build the extracted URI, Console, or
  TimeZone archives.
- Each exceptional cross-namespace type remaining in `Core.Base` has a documented dependency
  rationale.
- Existing code linking `SharpRuntime::Core` continues to compile without include changes.

**Depends on:** MOD-001, MOD-002, MOD-003.

### MOD-005 — Make tests respect component boundaries (P1) — implemented

**Goal:** module tests must prove the declared dependency closure instead of compiling against
`SharpRuntime::All`.

**Work:**

- Add a reusable CMake helper that creates a test target for each enabled physical component.
- Link each module test only to its owning component, GoogleTest, and explicitly declared
  test-only dependencies.
- Keep genuinely cross-module scenarios under `tests/integration` in a separate integration
  target.
- Stop forcing `All` merely because `SHARP_RUNTIME_BUILD_TESTS=ON`; a selective configuration
  should build tests for the requested component closure only.
- Preserve a convenient repository-wide target/CTest invocation that runs all module and
  integration tests.

**Acceptance criteria:**

- `SHARP_RUNTIME_COMPONENTS=Text.Json` with tests enabled does not enable unrelated networking,
  XML, compression, crypto, or collection subcomponents.
- A test that includes an undeclared module header fails to compile in its owning test target.
- The repository-wide suite still discovers and runs every existing test exactly once.

**Depends on:** MOD-002, MOD-003, MOD-004.

### MOD-006 — Add a selective-component CI matrix and negative isolation fixtures (P1) — implemented

**Goal:** continuously verify that optional components and external libraries remain isolated.

**Work:**

- Add clean configure/build/test jobs for representative leaves, including at least Core base,
  Text.Json, Net.Http.Headers, Net.WebSockets, both compression implementations,
  IO.IsolatedStorage, Security.Cryptography.Random, and Xml.Linq.
- Add small consumer fixtures proving that requested public headers compile and link with only
  the documented component target.
- Add negative fixtures proving that headers from an unrequested sibling or implementation-only
  dependency are unavailable.
- Verify that JSON-only and other lightweight configurations do not configure ZLIB, miniz,
  tinyxml2, SDL, socket, or platform-crypto dependencies unless required.

**Acceptance criteria:**

- The matrix starts from clean build directories and passes without relying on `SharpRuntime::All`.
- Removing a required dependency or making a private dependency leak is caught by at least one
  automated job.
- The full-build job remains present as a separate compatibility check.

**Depends on:** MOD-001 through MOD-005.

### MOD-007 — Document the final ownership and migration model (P2) — implemented

**Goal:** make the narrower graph understandable to maintainers and consumers.

**Work:**

- Update `README.md` and `docs/CMakeComponents.md` with the final component catalogue, public and
  private dependency meanings, external-library mapping, and selective-test commands.
- Document compatibility umbrellas (`Core`, `Collections`, `All`) separately from lean physical
  targets intended for new internal and external consumers.
- Record the rationale for intentional exceptions such as Core-owned IO exceptions,
  public tinyxml2 exposure, and the shared Xml/XPath archive.
- Add a maintainer checklist for assigning new headers, sources, tests, dependencies, and vendor
  libraries without weakening module boundaries.

**Acceptance criteria:**

- Every exported component has documented ownership, direct public dependencies, optional
  external dependencies, and one minimal usage example.
- Documentation dependency tables are checked against the registered graph or generated from the
  same metadata so they cannot silently drift.

**Depends on:** MOD-003 through MOD-006.

### MOD-008 — Perform the final modularity acceptance audit (P1) — implemented

**Goal:** close the remediation only after the architecture is measurably isolated and compatible.

**Work and acceptance criteria:**

- Run MOD-001 and record zero cycles, duplicate include paths, orphan files, undeclared edges,
  stale edges, and visibility mismatches.
- Run the full build/test suite and all MOD-006 selective jobs from clean directories.
- Compare dependency closures before and after the work, explicitly confirming that Text.Json,
  Net.Http.Headers, Net.Mime, and Numerics no longer pull unrelated threading/component-model
  modules.
- Verify compatibility targets and existing include spellings with consumer fixtures.
- Confirm that no public runtime behavior or API was removed as an incidental consequence of
  physical relocation.
- Record the final module/edge counts and intentional exceptions in
  `docs/CMakeComponents.md`.

**Depends on:** MOD-001 through MOD-007.

## Legend (task.status — see CLAUDE.md and README.md for the full definitions)

| Status | Meaning |
|--------|---------|
| `ported` | Implemented in sharp-runtime, meets the full porting checklist |
| `todo` / `''` | Needs to be ported/classified |
| `ignore` / `ignored` | Out of scope for sharp-runtime |
| `tobedecided` | Genuinely ambiguous — needs a human architecture decision |

`in_progress` is **not** a valid status value in the current workflow (it was used by an older,
now-retired interactive process — see `plan_namespaces.md`).
