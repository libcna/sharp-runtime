# Sharp Runtime plan

*Last verified: 2026-07-26 — 41 physical components, 90 direct production
dependency edges, a clean native build, 12,681 passing tests across 37
executables, and a locally green ten-job selective matrix. The tracked CI
matrix covers nine fixtures; its missing direct `Collections.Blocking` fixture
is recorded as audit finding `SR-AUD-001`.*

Sharp Runtime is in a consumer-driven expansion phase. The original type
classification and stabilization queues are complete, and the full native
build/test and selective-isolation baselines are healthy. Work now proceeds
from bounded consumer requirements, confirmed parity gaps, and newly measured
validation findings.

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
- Tests: 12,681 passing across 36 component binaries plus one integration
  binary.
- Component graph: 41 physical modules and 90 direct production edges.
- Boundary validator: no cycles, duplicate public include paths, orphan
  files, unresolved includes, undeclared edges, stale edges, or visibility
  mismatches.
- Dependency allow-list: empty.
- Selective matrix: all ten positive consumers pass. The Text.Json target
  absence assertion verifies that neither `Threading` nor `TimeZone` is
  configured, and negative include-leakage fixtures remain rejected.
- Tracked CI: Ubuntu selective matrix (nine of the ten local fixtures), full
  compatibility build, and a pinned Ubuntu 24.04 Doxygen-warning-baseline job
  in `.github/workflows/components.yml`. The missing direct
  `Collections.Blocking` consumer is `SR-AUD-001`.
- Doxygen 1.9.8: 1,942 warnings. `scripts/check_doxygen_warnings.sh` enforces
  that ceiling; lower counts are accepted and a Doxygen upgrade requires a
  deliberate re-baseline.

### Local planning database

The 2026-07-25 local snapshot contains:

| Table | State |
|---|---|
| `task` | 16,201 rows: 1,082 `ported`, 140 `ignore`, 14,979 legacy `ignored`; no unclassified or `tobedecided` rows |
| `ticket` | 1,766 rows: 1,765 `done` and P1 audit ticket #1766 `doing`; no `todo`, `blocked`, or `needs_user` rows |

Because `plan.sqlite3` is git-ignored, these counts describe the maintainer
snapshot, not data shipped in a fresh clone.

## P0 completed: Collections blocking isolation

Ticket #1737 resolved the post-modular closure regression caused by
`BlockingCollection.hpp`. It created the physical `Collections.Blocking`
component, moved that header and its eight dedicated tests there, and declared
the direct public dependencies `Collections.Core`, `Core.Base`, and
`Threading`. `Collections.Core` now depends publicly only on `Core.Base` and
the `Collections` compatibility umbrella includes all four collection
components.

The repair restores lean closures for `Text.Json`, `Net.Http.Headers`,
`Net.Mime`, and `Numerics`: none configures `Threading` or `TimeZone` unless a
requested component actually requires them. The Text.Json negative assertion
and a direct `Collections.Blocking` consumer fixture guard this result. Do not
move `BlockingCollection<T>` back to `Collections.Core` or weaken that
assertion without an explicit architecture decision.

## Completed milestones

### Porting and stabilization

- Classified the indexed .NET type surface and completed the original
  porting/stabilization queue.
- Established fixed-width public API aliases, property/indexer naming, SPDX
  headers, .NET-reference review, and regression-test requirements.
- Completed native TSan, ASan, and UBSan passes during stabilization and
  fixed the production findings discovered by those runs.
- Revalidated `ConcurrentBag`, `BlockingCollection`, `ConditionalWeakTable`,
  generic task continuations, and `TaskExtensions::Unwrap` with focused TSan
  and ASan/LSan scenarios under ticket #1742.
- Added `ImmutableList<T>` predicate/action queries (`ForEach`, `Exists`,
  `Find*`, and `TrueForAll`) under ticket #1743, including empty-delegate,
  empty-list, ordering, and immutability regression coverage.
- Added default full-list `ImmutableList<T>::Sort` and `Reverse` under ticket
  #1745, leaving range and custom-comparer overloads explicitly deferred.
- Added `ImmutableList<T>::GetRange` under ticket #1746, including exact
  boundary and invalid-range regression coverage.
- Added `ImmutableList<T>::ConvertAll<TOutput>` under ticket #1747, including
  empty-source and empty-converter regression coverage.
- Added seekable `BinaryReader::PeekChar` under ticket #1744. It returns the
  next UTF-8 character or EOF without advancing, restores the position after
  decode failure, and explicitly rejects non-seekable streams rather than
  pretending a general decoder buffer exists.
- Added `BinaryReader::ReadChars` and `Read(char[])` under ticket #1748. They
  preserve UTF-8 decoder output across calls, return a partial result at clean
  EOF, propagate truncated input, and expose supplementary scalars as UTF-16
  surrogate pairs.
- Added all three `ImmutableList<T>::CopyTo` overloads under ticket #1749,
  using checked fixed-size `std::vector` destinations, including source range
  and destination offset handling without signed-overflow-prone bounds checks.
- Added `ImmutableList<T>::Sort(Comparison<T>)` under ticket #1750. It follows
  the established signed comparison-delegate convention, rejects an empty
  delegate, and returns an independently backed sorted list.
- Added `ImmutableList<T>::Reverse(index, count)` under ticket #1751. It
  reverses only a valid subrange in an independently backed list and reuses
  the established zero-length-boundary and invalid-range contract.
- Added full-list and range `ImmutableList<T>::Sort(IComparer<T>)` overloads
  under ticket #1752. They use the established generic comparer interface and
  retain the full-list immutability and range-boundary contracts.
- Added default and `IEqualityComparer<T>` item mutations for
  `ImmutableList<T>` under ticket #1753: `Remove`, vector `RemoveRange`, and
  `Replace`. Removals process inputs sequentially, sources remain immutable,
  and a missing old value now throws `ArgumentException` as required.
- Added default and `IEqualityComparer<T>` range queries for
  `ImmutableList<T>` under ticket #1754: `IndexOf` and `LastIndexOf`. The
  forward and backward range contracts are validated separately, including the
  valid empty `LastIndexOf(..., 0, 0, ...)` case.
- Added full-list and range `ImmutableList<T>::BinarySearch(IComparer<T>)`
  overloads under ticket #1755. They respect custom ordering and return the
  complement of the absolute insertion point for a missing value.
- Added default-comparison `ImmutableList<T>::Sort(index, count)` under
  ticket #1756. It sorts only a valid range, preserves the source and outside
  elements, and accepts zero-length boundary ranges.
- Added `BigInteger` bitwise AND, OR, XOR, complement, and compound assignment
  operators under ticket #1757. The base-10^9 backing converts internally to
  a sign-extended two's-complement form so negative and large operands retain
  .NET semantics.
- Added signed `BigInteger` left/right shifts and compound assignments under
  ticket #1758. Negative counts reverse direction, while right shifts retain
  arithmetic floor semantics for negative values.
- Added `BigInteger` byte-vector construction and serialization under ticket
  #1759. The default is signed little-endian two's complement; callers can
  select unsigned and/or big-endian conversion, and output is minimal.
- Added the core `ImmutableList<T>::Builder` workflow under ticket #1760:
  `CreateBuilder`, `ToBuilder`, checked mutable mutations, and independent
  `ToImmutable` snapshots. Its vector-backed implementation intentionally
  copies rather than claiming the tree-backed .NET conversion complexity.
- Completed `UTF7Encoding` under ticket #1761 with RFC 2152 modified-Base64
  shifts over UTF-16BE units, optional-direct-character control, astral
  Unicode support, and U+FFFD recovery for malformed input. It remains
  obsolete and unsuitable for new protocols.
- Added conditional `Trace::WriteIf` and `Trace::WriteLineIf` under ticket
  #1762. They suppress output when false and retain the existing stderr
  write/newline behavior when true.
- Added explicit ProcessStartInfo child environment overrides under ticket
  #1763. They are validated before fork, passed only to the child, and retain
  inherited values not explicitly overridden.
- Added synchronous POSIX Process startup failure reporting under ticket #1764.
  Child setup and exec errors now reach Start() with the executable name and
  native error text instead of appearing later as exit code 127.
- Established the Doxygen warning baseline under ticket #1765: Doxygen 1.9.8
  emits 1,942 warnings. The dedicated check permits incremental reductions,
  rejects regressions, and avoids a mass comment-only rewrite.
- Added consumer-driven coverage across core, collections, IO, networking,
  threading/tasks, text/JSON, XML, numerics, globalization, and cryptographic
  hashing/random APIs.

### Platform work

- MinGW library cross-build audit completed under ticket #40; its post-modular
  `All` and selective `Text.Json` library revalidation passed with MinGW-w64
  GCC 14-win32 and CMake 3.31.6 under ticket #1741.
- Emscripten's earlier library audit is ticket #41. Its post-modular `All` and
  selective `Text.Json` revalidation passed with Emscripten 5.0.7 and CMake
  3.31.6 under ticket #1741, which also corrected an Emscripten-only unused
  `WebProxy` DNS helper warning promoted by `-Werror`.
- Both cross-build validations are compile evidence only; GoogleTest/runtime
  tests were not cross-built or run.
- Real downstream Apple Clang/Xcode 15.4 builds drove the portability fixes
  in commits `1d22a7b2` through `b797928f`.

These library builds use the final 41-component architecture. They are
evidence of portability, not a current cross-platform test matrix.

### Modular architecture

The remediation plan MOD-001 through MOD-008 was implemented in
`b0e944ad`, documented in `27e4d680`, and closed as tickets 1729–1736.
It delivered:

- One physical owner for every production header, source, and module test.
- Explicit public, private, and test-only dependency visibility.
- Narrow `Core.Base`, `Collections.Core`, `Collections.Blocking`,
  `Collections.Async`, and `Collections.ObjectModel` targets while preserving
  compatibility umbrellas.
- Component-scoped test executables and a separate integration executable.
- Automated boundary validation, catalogue generation, isolated consumers,
  negative fixtures, and GitHub Actions coverage.
- Generated component documentation in `docs/ComponentCatalog.md`.

The graph landed with 85 production edges and has since grown to 90. The
validator, generated catalogue, native gate, and selective matrix are green.

### Post-modular API expansion

The first consumer-driven ports after modularization added:

- Runtime compiler-services helpers, including `ConditionalWeakTable` and
  `RuntimeHelpers`.
- Component-model notification, initialization, and async-completion
  metadata.
- HTTP handler/invoker/request-option primitives and web-proxy APIs.
- `ConcurrentBag` and `BlockingCollection`.
- `MemoryStream(buffer, size)` writability parity, while `BinaryData::ToStream`
  and read-mode ZIP entry streams retain their explicit read-only contracts.
- `TaskT<TResult>::ContinueWith` for action and result-producing callbacks,
  including terminal-state filtering, chaining, and weak-ownership teardown.
- `TaskExtensions::Unwrap` for generic and non-generic nested tasks.
- `XmlWriter::WriteWhitespace` and `XText::WriteTo`'s document-level
  whitespace distinction.
- XML schema exception types.

The verified test baseline grew from 12,494 at the modularization checkpoint
to 12,681.

## Active repository audit

Ticket #1766 is a P1, evidence-only, repository-wide audit. It mirrors every
tracked first-party text-like source, test, build, CI, and relevant
documentation file under `audit/`, following the CNA audit format. Its scope,
exclusions, live manifest, findings index, and resume state are maintained in
that directory. The audit is deliberately not a repair stream: confirmed
defects, missing assertions, weak diagnostics, and parity gaps become
evidence-backed follow-up tickets only after the manifest is reconciled.
The 2026-07-27 checkpoint has 630 of 1,748 mirrored reports complete and
one hundred ninety-seven confirmed findings; `audit/AUDIT_PROGRESS.md` is the authoritative
live count.

## Candidate roadmap

No production implementation is active while ticket #1766 is in progress.
After it completes, create or reopen a repair `ticket` row with acceptance
criteria and a validation command before changing code.

### P2 — Consumer-driven API breadth

1. **Extend `ImmutableList<T>::Builder` only from a concrete consumer need.**
   Core construction, mutation, query, and snapshot behavior is implemented.
   Advanced query, sorting, and copy overloads remain separate bounded work;
   preserve the documented vector-backed snapshot semantics.

2. **Extend `BinaryReader` only from a concrete consumer need.**
   `ReadChar`, `ReadChars`, `Read(char[])`, `ReadDecimal`, and seekable
   `PeekChar` are implemented. Any further encoding or buffering breadth must
   preserve decoder state, supplementary UTF-8 handling, and truncated-input
   behavior.

3. **Review other documented partial surfaces by demand.**
   Examples include debugger/process breadth and richer XML reader/writer
   functionality.
   A documented partial API is not automatically higher priority than a
   consumer-visible bug.

### P2 — Developer experience

4. **Reduce the Doxygen warning backlog incrementally.**
   The reproducible Doxygen 1.9.8 baseline is 1,942 warnings. Require touched
   public APIs not to regress it; avoid a mass comment-only rewrite.

5. **Decide whether distribution support is wanted.**
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
