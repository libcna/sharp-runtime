# NEXT.md

*Last verified: 2026-07-25. Branch: `feature/work`. Code baseline:
`03c7d4bb`. Native baseline: clean build and 12,586 passing tests across 36
executables. Selective baseline: failing `Text.Json` isolation assertion
because `Collections.Core` now pulls `Threading`.*

This is the cold-start handoff for the next working session. Keep it focused
on facts needed to resume: current validation state, recent architectural/API
changes, known gaps, and a short ordered task list. Historical session detail
belongs in git history and `plan.sqlite3`, not in an ever-growing transcript.

## 1. Project and phase

Sharp Runtime is a C++23 implementation of a practical subset of .NET
`System.*`, primarily for CNA and native game ports.

The project is now in **consumer-driven expansion after stabilization and
modularization**:

- The local `task` classification queue has no unclassified rows.
- All 1,736 stabilization/architecture tickets are `done`.
- The original component-boundary remediation landed, but a later collection
  port regressed one accepted isolation property.
- The full native build/test gate is green; the selective CI matrix is not.
- No database ticket records the regression yet.

Start new work only from a concrete consumer requirement, a confirmed parity
gap, or a reproducible validation finding. Create a bounded ticket before
implementation so the local database remains the durable source of detail.

## 2. Verified current state

The following passed on 2026-07-25:

```text
Module validator: 40 physical modules, 88 production dependency edges
Validator fixtures: 7 passed
Generated component catalogue: current
Build: 0 warnings, 0 errors
Tests: 12,586 passed across 36 executables
```

The exact gate was:

```bash
scripts/local_ci_check.sh build
```

The suite includes 35 component executables and
`SharpRuntimeIntegrationTests`. `SharpRuntimeTests` is only a CMake aggregate
target; there is no monolithic `./build/SharpRuntimeTests` executable.

The complete selective check currently fails after both the `Core.Base` and
`Text.Json` test binaries pass:

```text
FAIL: selective build unexpectedly configured target sharp_runtime_threading
```

Tracked CI now exists in `.github/workflows/components.yml`:

- Nine Ubuntu selective-component jobs exercise positive consumer fixtures
  and negative leakage assertions.
- A full Ubuntu compatibility job runs `scripts/local_ci_check.sh build`.
- The `Text.Json` job is expected to reproduce the current local isolation
  failure; workflow presence must not be described as a green CI result.

The local `plan.sqlite3` snapshot contains:

```text
task:   1,082 ported; 140 ignore; 14,979 legacy ignored; 0 open/TBD
ticket: 1,736 done; 0 todo/doing/blocked/needs_user
```

The database is git-ignored and is not available in a fresh clone.

## 3. What changed since the previous handoff

### Binary IO and platform portability

- `BinaryReader::ReadChar` gained incremental UTF-8 decoding (`167b0bc4`).
- `BinaryReader::ReadDecimal` gained the .NET four-int32 wire layout
  (`2505d58c`).
- `BinaryReader::ReadString` and `ReadBytes` now avoid adversarial huge
  pre-allocation on short seekable streams (`d2cc9cce`).
- Real downstream Apple Clang/Xcode 15.4 builds drove fixes for GCC-only
  warning flags, missing floating-point `from_chars`, Apple host/clock APIs,
  socket byte-order macros, BSD ICMPv4 layout, and BSD/Darwin entropy
  (`1d22a7b2` through `b797928f`).

### Physical component architecture

The work from `7a99e061` through `27e4d680`, principally `b0e944ad`:

- Moved public headers, implementations, and tests under one owning
  `modules/<module>` tree.
- Split broad Core and Collections closures into narrow physical targets.
- Added public/private/test-only dependency metadata.
- Added component-scoped test binaries and a separate integration binary.
- Added executable boundary validation, generated catalogue checks,
  selective consumer fixtures, and GitHub Actions.
- Preserved include spellings and compatibility targets.

The modularization checkpoint had 12,494 tests. Subsequent API work increased
the current baseline to 12,586 and the graph from 85 to 88 production edges.

### Consumer-driven ports after modularization

- Runtime compiler-services helpers, including `ConditionalWeakTable`,
  `RuntimeHelpers`, state-machine attributes, and compiler feature metadata
  (`84f50c5d`).
- Component-model notification, change-tracking, initialization, attribute,
  and async-completion primitives (`db0a172a`, `c97cc024`).
- `HttpMessageHandler`, `HttpMessageInvoker`, request options, and related
  HTTP primitives (`8a7077eb`).
- `IWebProxy` and `WebProxy` (`b775e773`).
- `ConcurrentBag` and `BlockingCollection` (`227111cb`).
- Generic and non-generic `TaskExtensions::Unwrap` (`0310c7d1`).
- `XmlSchemaException` and `XmlSchemaValidationException` (`5ef799e3`).

`03c7d4bb` removed a completed audit document and corrected stale test
comments; it did not change production behavior.

## 4. Active architecture regression

Commit `227111cb` placed `BlockingCollection` in `Collections.Core`.
`BlockingCollection.hpp` publicly includes `CancellationToken`,
`CancellationTokenRegistration`, and `Timeout`, so the commit also added the
public edge:

```text
Collections.Core -> Threading -> TimeZone
```

That edge is declared correctly, which is why the 40-module/88-edge boundary
validator passes. It nevertheless defeats the lean Collections closure:
`Text.Json`, `Net.Http.Headers`, `Net.Mime`, and `Numerics` all pull
`Threading` and `TimeZone` again. The Text.Json target-absence assertion
catches this and makes the selective GitHub Actions job fail.

The preferred fix is a narrow new physical component (provisional name
`Collections.Blocking`):

1. Move `BlockingCollection.hpp` and its dedicated tests out of
   `Collections.Core`. Leave the other concurrent headers in place because
   they do not include `System/Threading/*`.
2. Give it the direct dependencies proved by its headers:
   `Collections.Core`, `Core.Base`, and `Threading`.
3. Add it to the `Collections` compatibility umbrella.
4. Remove `Threading` from `Collections.Core`.
5. Add/update a selective consumer fixture, regenerate the catalogue, then
   run the full selective matrix and native gate.

Do not merely remove the negative assertion: accepting the broader closure
would reverse MOD-003/MOD-006's approved goal and needs an explicit
architecture decision.

## 5. Architecture facts to preserve

### Components and ownership

- There are 40 physical components and four compatibility surfaces:
  `Core`, `Collections`, `Xml.XPath`, and `All`.
- New internal and external consumers should use narrow physical targets such
  as `Core.Base` and `Collections.Core`.
- Public headers live under `modules/<module>/include/`.
- Implementations live under `modules/<module>/src/`.
- Module tests live under `modules/<module>/tests/`.
- Only genuinely cross-module scenarios belong in `tests/integration/`.
- Module `CMakeLists.txt` files are declarations for the root project, not
  standalone projects.

### Enforced dependency model

- Public-header edges use `PUBLIC_DEPENDENCIES`.
- Implementation-only edges use `PRIVATE_DEPENDENCIES`.
- Test-only edges use `TEST_DEPENDENCIES`.
- Optional vendor/platform libraries are attached by the owning component.
- Internal modules must not depend on the `Core`, `Collections`, or `All`
  compatibility umbrellas.
- `scripts/validate_module_boundaries.py` rejects missing, stale, cyclic, or
  incorrectly visible edges.

### Public API conventions

- .NET-sized integral values in public APIs use the corresponding
  `SharpRuntime::*cs` aliases.
- Properties use `getXxxProperty()`/`setXxxProperty()`; C# indexers use
  `getItem()`/`setItem()`.
- Mutable collection enumerators must detect structural mutation.
- Dictionary-like mutable indexing must not silently insert on a read.
- Platform-specific headers stay out of public headers; unsupported runtime
  operations throw `PlatformNotSupportedException`.
- Permanent deviations in `CLAUDE.md` are not backlog items.

## 6. Known gaps and limitations

### Confirmed, bounded parity gaps

- `MemoryStream(const bytecs*, intcs)` sets `writable_` to `false`; .NET's
  corresponding single-buffer constructor is writable.
- `TaskT<TResult>::ContinueWith` is absent. Non-generic
  `Task::ContinueWith` exists, and `TaskT` has completion synchronization, but
  not a generic continuation list/API.
- `XText::WriteTo` always calls `WriteString`; .NET uses `WriteWhitespace`
  when the parent is an `XDocument`. This port's `XmlWriter` has no
  `WriteWhitespace` primitive yet.

### Documented partial surfaces

- `ImmutableList<T>` lacks several sorting, range/copy, conversion, predicate,
  builder, and comparer APIs documented in its class comment.
- `BinaryReader` now has `ReadChar` and `ReadDecimal`, but not `PeekChar`,
  `ReadChars`, or `Read(char[])`.
- Other explicitly documented partial APIs include `BigInteger` bitwise
  operations, full UTF-7 behavior, and broader process/debugger/XML surfaces.
  Prioritize them only when a consumer needs them.

### Validation gaps

- The full selective matrix currently fails as described in §4.
- The current 40-component tree has not been revalidated with MinGW or
  Emscripten since the pre-modular ticket #40/#41 cross-builds.
- macOS portability fixes came from real downstream Xcode 15.4 builds, but
  this repository has no macOS CI job or recorded full standalone suite.
- Repository CI is Ubuntu-only.
- The latest concurrent additions (`ConcurrentBag`, `BlockingCollection`,
  `TaskExtensions::Unwrap`, and `ConditionalWeakTable`) have not had a
  dedicated post-port sanitizer pass.
- Doxygen still has a pre-existing warning backlog; unlike compiler warnings,
  it is not yet a zero-warning gate.

### Permanent deviations

- Reflection, GC internals, serialization infrastructure, P/Invoke, and
  late-bound delegate invocation remain out of scope.
- Symmetric/asymmetric encryption, TLS, and X.509 remain out of scope.
- `Decimal`, `Int128`, and `UInt128` retain their accepted `__int128`
  dependency and are unsupported by MSVC.
- Some platform operations intentionally compile but throw
  `PlatformNotSupportedException`; see `CLAUDE.md`.

## 7. Useful commands

```bash
# Inspect state
git status --short --branch
git log --oneline -12

# Full local gate: validator, catalogue, clean build, all tests
scripts/local_ci_check.sh build

# Configure/build explicitly
cmake -S . -B build \
  -DSHARP_RUNTIME_COMPONENTS=All \
  -DSHARP_RUNTIME_BUILD_TESTS=ON
cmake --build build --target SharpRuntimeTests --parallel 4

# Run all component/integration executables exactly once
scripts/run_component_tests.sh build

# Run one suite
./build/SharpRuntimeTests_Threading_Tasks \
  --gtest_filter="TaskExtensionsTests.*"

# Validate only module metadata and generated docs
python3 scripts/validate_module_boundaries.py
python3 test/validate_module_boundaries_test.py
python3 scripts/generate_component_catalog.py --check

# Reproduce the active Text.Json isolation regression
scripts/check_selective_components.sh

# Inspect planning queues
sqlite3 plan.sqlite3 \
  "SELECT status, COUNT(*) FROM task GROUP BY status ORDER BY status;"
sqlite3 plan.sqlite3 \
  "SELECT ticket_no, priority, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 10;"

# Generate API documentation
mkdir -p docs/generated
doxygen Doxyfile
```

HTTP/socket/ping tests need permission to use local networking. A sandbox that
denies `socket()` can make those tests fail even when the code baseline is
healthy.

## 8. Recommended next bounded tasks

Choose one, create a ticket, and keep its changes isolated.

1. **Restore the Collections boundary** — create a ticket and implement the
   `Collections.Blocking` split from §4. The acceptance gate is all nine
   selective jobs plus the full native baseline.

2. **`MemoryStream` writability parity** — smallest confirmed behavior fix.
   Audit callers, change the constructor default, and add a regression test.

3. **Post-modular MinGW/Emscripten build verification** — re-run the known
   toolchains against `All` plus a selective component and record exact
   results. Do not imply cross-platform runtime-test coverage from a
   library-only build.

4. **Focused sanitizer pass for the newest concurrent types** — TSan
   `ConcurrentBag`/`BlockingCollection` and ASan/LSan task/weak-ownership
   teardown.

5. **`TaskT<TResult>::ContinueWith`** — medium-sized API completion with
   success/fault/cancel, option-filtering, chaining, and lifetime tests.

6. **`XmlWriter::WriteWhitespace` plus `XText` parity** — scoped XML API
   addition, not a one-line behavior change.

7. **One consumer-requested partial surface** — select a bounded
   `ImmutableList`, `BinaryReader`, or other documented gap rather than a
   broad completeness sweep.

The longer roadmap and definition of done are in `plan.md`.

## 9. Guardrails

- Do not restart completed project-wide naming or integer-alias rollouts.
- Do not add compatibility aliases for deliberately removed public names
  without an explicit decision.
- Do not add cross-platform CI jobs, dependencies, or broad public-header
  refactors without user direction.
- Do not implement permanent out-of-scope reflection/GC/TLS/crypto areas.
- Do not optimize without a measurement that demonstrates a real bottleneck.
- Do not weaken component dependencies by linking internal code to an
  umbrella target.
- Do not treat the aggregate `SharpRuntimeTests` build target as an
  executable.
- Push only to `feature/work`; never merge/push to `develop` or `master`, or
  create tags, without explicit per-action approval.

## 10. Cold resume

From a fresh context:

1. Read `CLAUDE.md`, this file, and `plan.md`.
2. Run `git status --short --branch`.
3. Run `scripts/local_ci_check.sh build`.
4. Reproduce the selective failure with
   `scripts/check_selective_components.sh`.
5. Query `plan.sqlite3` for an existing `todo`/`doing` ticket; if none exists,
   create one for the §4 Collections split.
6. Inspect only the collection modules, component registration/fixtures, and
   affected tests needed for that remediation.
7. Update the measured baseline and this handoff after verified work lands.
