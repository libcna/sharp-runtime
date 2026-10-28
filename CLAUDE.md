<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Keep AGENTS.md and CLAUDE.md byte-for-byte identical. A shared FNA AI policy may be inserted above this repository-specific section. -->

# sharp-runtime repository instructions

## Mission and scope

`sharp-runtime` is a C++23 static library that implements a practical subset of the .NET
`System.*` namespace so ported C#/XNA game code can compile against C++ headers with minimal
changes. It underpins CNA and mobile-eggbert.

The maintainer owns scope and priorities. Agents may implement and modify sharp-runtime code,
tests, build configuration, and documentation when doing an assigned task. This includes
necessary fixes in sharp-runtime discovered while working on an authorized consumer task.
Keep changes within the assigned task and its necessary dependencies; report unrelated findings
instead of silently expanding scope.

Permission to change this repository does not authorize edits in CNA, mobile-eggbert, or any
other repository. It also does not by itself authorize commits, pushes, merges, releases, or
tags. Follow the user's request, the applicable shared policy, and the repository rules below.
The historical AI-consultant restriction in `docs/HumanDevelopmentHandoff-2026-09-30.md` is
superseded; that file is a historical handoff record.

## Sources of truth

Do not copy long-lived records into this file. Read the relevant source when a task needs it:

- `docs/StandingApprovals.md` is the single source of truth for standing approvals, public
  source/ABI changes, downstream-impact requirements, and measured environment facts.
- `docs/TestCountLedger.md` is the historical test-count ledger. Keep only the current baseline
  in this file.
- `docs/releasing.md` defines release/version procedures.
- `/rv/tmp/runtime/src/libraries/` is the local .NET reference source when present. It is a .NET
  11 preview snapshot, so behavior that may differ from released .NET versions must be described
  as snapshot-specific rather than timeless parity.
- Task-specific design, migration, and validation documents under `docs/` remain authoritative
  for their narrow subjects. Read them when touching that subject, not on every task.

If two repository documents disagree, prefer the narrower and newer source, then verify with
code, tests, and git history. Ask the maintainer when the conflict changes public behavior or
scope.

## Working discipline

- Inspect the worktree before editing. Preserve unrelated user changes and untracked files.
- Diagnose the actual problem before changing code. Avoid speculative cleanup and broad header
  refactors.
- Use existing project patterns and the narrowest module dependency that solves the problem.
- Do not weaken, delete, skip, or recategorize a failing test merely to make the gate green.
- Record behavior or source migrations in `docs/Migration-*.md` when required by
  `docs/StandingApprovals.md`.
- Report public API, source-compatibility, object-layout, vtable, exception-specification, and
  ABI effects explicitly, including “none” when verified.
- Do not commit or push unless the task or user authorizes it. After an authorized commit, push
  it immediately to the designated working branch. The default is `feature/work`; a standing
  approval may designate `next` for its recorded scope. Never push to `develop` or `master`,
  merge them, or create a tag without explicit per-action approval.

## Public API and code style

- Use C++23 and C++17 nested namespace syntax, for example
  `namespace System::Collections::Generic {`.
- Public APIs mirroring .NET `int` use `SharpRuntime::intcs`, not native `int`.
- Properties use `getXxxProperty()` and `setXxxProperty()`. Indexers use `getItem()` and
  `setItem()`.
- Do not use the compatibility `System::Linq` surface in project implementation code. Use
  standard C++ algorithms and `std::ranges`.
- Copy/adapt applicable .NET XML documentation into Doxygen `/** */` comments for public APIs.
- Project `.hpp` and `.cpp` files require the MIT SPDX line, project copyright, and .NET
  attribution. Preserve upstream headers in vendored code.
- Complex types normally use a public header plus a `.cpp` implementation; simple templates and
  value types may remain header-only.
- Keep platform headers and implementation details out of public headers. Put platform code in
  `.cpp` files behind `_WIN32`, `__EMSCRIPTEN__`, or POSIX branches.
- Do not apply naming conventions mechanically across the tree. Public spelling changes require
  the migration and compatibility process in `docs/StandingApprovals.md`.

## .NET parity and declared deviations

The goal is maximum practical parity: public shape, semantics, defaults, exceptions, messages,
formatting, and edge cases should match the applicable .NET reference as closely as C++ permits.
Compare non-trivial implementations with the reference source. Where the reference is clear,
derive the behavior from it; where it is ambiguous or platform-specific, measure or ask instead
of guessing.

These are declared boundaries, not invitations to silently invent substitutes:

- Reflection and runtime type activation are out of scope. Required compatibility surfaces are
  stubs or throw according to the existing contract.
- `System::GC` is a compatibility surface; ownership is implemented with C++ RAII and smart
  pointers.
- Most delegate aliases are single-target `std::function` values. `System::Delegate` and
  `System::MulticastAction` are the explicit multicast implementations. Late-bound
  `DynamicInvoke` remains unsupported.
- There is no general object serializer. `SerializationInfo` is an in-process name/value store
  used by explicitly implemented constructors and `GetObjectData`; it is not `BinaryFormatter`
  or an object-graph serializer.
- P/Invoke and general interop are out of scope. Symmetric/asymmetric cryptography, X.509, and
  TLS are out of scope; existing hash, HMAC, and PBKDF2 support remains in scope.
- Public string indices, lengths, and counts use UTF-8 storage bytes, not UTF-16 code units.
- Unicode normalization follows invariant-globalization behavior: validation remains real, but
  normalization itself does not transform text.
- `TimeZoneInfo` does not expose full tzdata rule structures; do not infer structural rule
  equality by sampling offsets.
- `Decimal`, `Int128`, and `UInt128` require compiler-provided native 128-bit integers. Respect
  `SHARP_RUNTIME_HAS_NATIVE_INT128`; do not add a reduced or hand-rolled replacement.

Unsupported runtime operations must still compile on intended toolchains and throw a clear
`PlatformNotSupportedException` rather than fail the build or silently return a false result.
The verified runtime baseline is Linux. Windows, macOS, MinGW, and Emscripten compile coverage
must not be described as equivalent runtime test coverage.

Current notable platform limits are:

| Subsystem | Implemented runtime platforms |
|---|---|
| Sockets and `RandomAccess` | Windows and POSIX; unavailable operations throw on Emscripten |
| `AppDomain` base directory | Windows, macOS, Linux/POSIX; Emscripten uses a virtual-FS fallback |
| `TimeZoneInfo` | Windows and POSIX; Emscripten has limited UTC/local behavior |
| `Diagnostics::Process` and POSIX signals | POSIX |
| Network-interface enumeration | Linux |
| `FileSystemWatcher` | Linux/inotify |

## Architecture invariants

- Every module owns its public headers, sources, tests, dependencies, and platform setup in
  `modules/<module>/CMakeLists.txt`. `scripts/validate_module_boundaries.py` enforces ownership
  and dependency declarations.
- Internal code depends on narrow physical targets such as `Core.Base` and `Collections.Core`,
  never compatibility umbrellas such as `Core`, `Collections`, or `All`.
- Public-header dependencies are `PUBLIC_DEPENDENCIES`; implementation-only dependencies are
  `PRIVATE_DEPENDENCIES`; test-only dependencies are `TEST_DEPENDENCIES`.
- `BlockingCollection<T>` belongs to `Collections.Blocking`. Do not move its threading
  requirements into `Collections.Core` or weaken the Text.Json isolation fixture.
- A collection with fail-fast enumeration uses
  `System::Collections::detail::MutationCounter` and snapshots
  `detail::MutationVersion`. Do not introduce a bare signed counter or a new use of
  `detail::NarrowMutationCounter`. See `docs/CollectionVersionCounterSweep.md`.
- Test-only access seams have exactly one definition in the designated shared test-support file.
  A new seam needs both the ODR check and a negative consumer fixture. See
  `docs/CollectionVersionTestSeamDesign.md` and
  `docs/NegativeConsumerFixtureValidation.md`.
- Negative consumer fixtures must compile cleanly with no site selected and test each forbidden
  spelling independently through numbered `SHARP_RUNTIME_NEGATIVE_SITE` branches. Never replace
  this with a whole-file failure assertion.
- Vendored libraries are upstream code and are not reformatted or subjected to project naming
  rules. The only standing tinyxml2 deviations are documented in `docs/VendoredTinyXml2.md`;
  unrelated vendor edits require explicit scope.

## Build-resource policy

### Two compilation jobs maximum

Every build, link, rebuild, sanitizer build, compile probe, consumer fixture, dependency build,
and script that compiles internally may use at most two parallel jobs. Use an explicit bound,
for example:

```bash
cmake --build build --parallel 2
ninja -C build -j2
make -C build -j2
ctest --test-dir build -j2
```

Never use bare/unbounded `-j`, an implicit `--parallel`, detected CPU counts such as `nproc`, or
scripts that default to all cores. Use `SHARP_RUNTIME_BUILD_JOBS=2` or the script's explicit job
option when needed. One job is always allowed and preferred for memory-heavy work. More than two
jobs requires explicit approval for that individual action.

### Reuse build trees and protect the SSD

Use incremental builds and existing build trees. Do not clean, delete, or reconfigure a usable
tree merely to obtain a fresh build. The allowed build-directory names are a closed set:

| Directory | Purpose |
|---|---|
| `build/` | default build and repository gate |
| `build-modular/` | modular/selective component build |
| `build-asan/`, `build-ubsan/`, `build-tsan/` | sanitizer builds |
| `build-probe/` | disposable probes and ABI experiments |
| `build-consumer/` | standalone consumer fixtures |
| `build-tmp/` | repository-local temporary area and `TMPDIR` |
| `cmake-build-debug/` | IDE build |

Do not create per-ticket, per-branch, or dated build directories. Never create a build tree under
`/tmp`, `/var/tmp`, or `/dev/shm`; redirect temporary build-producing scripts through
`build-tmp/`. Retain existing `ccache` configuration. Remove disposable probe artifacts after
their evidence is recorded, but do not delete a build directory another session may use.

## Validation gate

Validation must be proportional while developing, but the repository gate before an authorized
commit remains strict:

1. `cmake --build build --parallel 2` completes with zero errors and zero warnings.
2. `scripts/run_component_tests.sh build` reports zero failures and zero skips, with no test-count
   regression.
3. Relevant boundary, negative-fixture, sanitizer, platform, documentation, or local-CI checks
   run when the change can affect them.

The latest fully verified runtime test baseline is **18,232 tests across 42 executables: 18,232
passed, 0 failed, 0 skipped**, measured 2026-10-28 with the supported XNA sample fixture view and
the original Yacht SOAP service on a private loopback port. The fixture runner is
`scripts/run_component_tests_with_soap_fixture.py`; set `XNA_SAMPLES_ROOT` for the five real XNA
serialization fixtures. The 2026-09-30 checkpoint additionally recorded graph 44/109, test-only
seams 5/22, negative fixtures 55/284, 230 first-party production translation units clean under
Clang 19.1.7 with `-Werror`, and 2,674 Doxygen warnings under the ceiling of 2,675. Do not silently
replace the current test baseline with a partial run; update it only from a complete, reproducible
gate. Historical checkpoints belong in `docs/TestCountLedger.md`.

When repairing a test that encoded an environment version, retain its defect-detection power and
demonstrate that with a mutation or equivalent negative control. A passing but weakened test is
not a repair.

## Completion and reporting

A type is “done” only when its intended public surface is present, non-trivial behavior is
implemented or explicitly declared out of scope, documentation and SPDX requirements are met,
the relevant .NET logic has been compared, and the validation gate is green.

Final reports must state:

- what changed and why;
- validation performed and any check not run;
- every build directory used and the maximum parallel job count;
- public API/source/ABI/layout/vtable/exception impact;
- remaining limitations or follow-up work.

Use these status terms consistently:

- **DONE** — implemented and tested on the verified native baseline, with no known gap.
- **PARTIAL** — usable but has a documented API or behavior gap.
- **PLATFORM-LIMITED** — compiles on intended toolchains, but named runtime operations throw on
  unsupported platforms.
- **STUB** — the compatibility surface exists but is intentionally unimplemented or throwing.

## Common commands

```bash
# Incremental build
cmake --build build --parallel 2

# Full component test gate
scripts/run_component_tests.sh build

# Module-boundary validation
python3 scripts/validate_module_boundaries.py

# Negative consumer fixtures
SHARP_RUNTIME_BUILD_JOBS=2 python3 scripts/check_negative_consumer_fixtures.py
```
