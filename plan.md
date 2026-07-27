# Sharp Runtime plan

*Last verified: 2026-07-27 — 41 physical components, 90 direct production
dependency edges, a clean native build, 12,921 passing tests across 37
executables, and a locally green ten-job selective matrix. The tracked CI
matrix covers nine fixtures; its missing direct `Collections.Blocking` fixture
is recorded as audit finding `SR-AUD-001`.*

Sharp Runtime is in the post-audit remediation phase. The original type
classification, stabilization, and modularization queues are complete, and the
full native build/test and selective-isolation baselines are healthy. Work now
proceeds from the evidence-backed `audit/` inventory in bounded, independently
validated repair tickets. Consumer-driven API breadth remains legitimate later
work but must stay behind confirmed crash, lifetime, and public-contract
findings.

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
- Tests: 12,921 passing across 36 component binaries plus one integration
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
- Doxygen 1.9.8: 1,941 current warnings against a 1,942-warning ceiling.
  `scripts/check_doxygen_warnings.sh` enforces that ceiling; lower counts are
  accepted and a Doxygen upgrade requires a deliberate re-baseline.

### Local planning database

The 2026-07-27 local snapshot contains:

| Table | State |
|---|---|
| `task` | 16,201 rows: 1,082 `ported`, 140 `ignore`, 14,979 legacy `ignored`; no unclassified or `tobedecided` rows |
| `ticket` | 1,774 rows: 1,772 `done` — including audit ticket #1766, post-audit tickets #1767, #1768, #1769, #1770, and #1771, and follow-up correction ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`) — one `wontfix` (#1772, obsoleted by #1771), and one deliberately inactive `blocked` row (#1773, the out-of-repository CNA / mobile-eggbert `CopyTo` sweep); no `todo`, `doing`, or `needs_user` rows |

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
- Remediated SR-AUD-356 and SR-AUD-364 / CCF-018 under ticket #1767. A shared
  lifecycle state protects `Current` across ten collection enumerators;
  BitArray additionally detects every mutation. Thirteen permanent regressions,
  the direct ASan/UBSan probe, 1,435 Collections.Core tests, and the
  network-permitted 12,694-test repository gate pass; Doxygen remains below
  its ceiling at 1,941/1,942.
- Remediated SR-AUD-357 / CCF-019 under design ticket #1768 and implementation
  ticket #1769. `LinkedListNode<T>` now refers to an independently allocated,
  reference-counted node with an explicit null/detached/attached state, so
  removal, `Clear`, and destruction of the owning `LinkedList<T>` detach the
  node and retain its value instead of leaving a dangling `std::list` iterator.
  The repair also added the .NET existing-node insertion overloads, the
  detached-node constructor, `Value` setter, `List` accessor, node identity
  comparison, defined list copy/move semantics, and a bidirectional
  `LinkedList<T>::iterator`. Forty-nine permanent regressions, a clean
  ASan/UBSan/LeakSanitizer probe, a `-Werror` standalone `Collections.Core`
  consumer fixture, 1,484 Collections.Core tests, and the network-permitted
  12,743-test repository gate pass; Doxygen stays at 1,941/1,942. The contract
  is recorded in [`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md).
- Completed the SR-AUD-358 / CCF-020 raw-`CopyTo` design under ticket #1770, a
  design-only ticket that changed no production or test source. The selected
  contract — a length-aware, statically typed `System::Span<std::any>`
  destination behind a non-virtual `ICollection`, so capacity and element type
  are validated exactly once before any implementation writes — is recorded in
  [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md). Seven
  repository-local probes back it, including a clean ASan/UBSan/LeakSanitizer
  `-Werror` prototype and a re-verification that the current boundary still
  produces three sanitizer aborts plus one silent, LeakSanitizer-only
  element-type corruption. SR-AUD-358 stays `confirmed`; implementation is
  inactive ticket #1771, gated on explicit approval of the narrow public-API
  break.
- Remediated SR-AUD-358 / CCF-020 under implementation ticket #1771, after the
  user explicitly approved the public source- and ABI-breaking change.
  `CopyTo(void*, intcs)` is removed from `System::Collections::ICollection` and
  replaced by non-virtual, validating `CopyTo(ObjectSpan, intcs)` and
  `CopyTo(std::vector<std::any>&, intcs)` over one protected pure virtual
  `copyToCore(ObjectSpan, intcs)` hook, with checked typed
  `std::vector<void*>` / `std::vector<DictionaryEntry>` overloads on the
  concrete collections and `using ICollection::CopyTo;` where one is added. No
  deprecated shim was retained, so a stale call site is a compile error naming
  the replacement rather than a run-time throw; because a pure virtual member
  was removed, every consumer must rebuild. 128 permanent regressions across
  every implementation, a clean ASan/UBSan/LeakSanitizer probe and suite run, a
  `-Werror` standalone `Collections.Core` consumer fixture, 1,612
  Collections.Core tests, and the 12,871-test repository gate pass; Doxygen
  stays at 1,942/1,942 -- the new README link to the migration document
  produces the same unresolved-markdown-link warning that every other README
  documentation link already produces, offsetting one warning removed from
  ICollection.hpp. Consumer guidance is in
  [`docs/Migration-ICollectionCopyTo.md`](docs/Migration-ICollectionCopyTo.md).
- Corrected a follow-on defect in #1771's own validation rule under ticket
  #1774: `detail::requireValidCopyDestination` had rejected every null-pointer
  destination outright, including a valid empty `ObjectSpan{nullptr, 0}` or a
  default-constructed empty `std::vector<std::any>` copied from an empty
  collection. The rule now rejects a null pointer only when paired with a
  positive length; a non-empty collection copied into a zero-length
  destination still fails, but on capacity, not nullness. SR-AUD-358 remains
  `remediated`. `CopyToBoundaryTests.cpp` grew to 1,662 Collections.Core tests
  and a new standalone probe
  (`build-probe-copyto/probe10_empty_span_correction.cpp`) is clean under
  ASan/UBSan/LeakSanitizer; the 12,921-test repository gate passes with zero
  build warnings/errors and Doxygen stays at 1,942/1,942. Recorded in section 22
  of [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md).
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
to 12,921, most recently through the post-audit remediation regressions.

## Completed repository audit

Ticket #1766 was a P1, evidence-only, repository-wide audit. It mirrors every
tracked first-party text-like source, test, build, CI, and relevant
documentation file under `audit/`, following the CNA audit format. Its scope,
exclusions, final manifest, findings index, and handoff state are maintained in
that directory. The audit was deliberately not a repair stream: confirmed
defects, missing assertions, weak diagnostics, and parity gaps become
evidence-backed follow-up tickets only after the manifest is reconciled.
The 2026-07-27 audit closure has all 1,748 of 1,748 mirrored reports complete
and three hundred sixty-four findings confirmed at closure;
`audit/AUDIT_FINAL_REPORT.md`
and `audit/AUDIT_PROGRESS.md` are the authoritative handoff. The final
142-file Collections shard passed 1,422/1,422 and adds SR-AUD-356 through
SR-AUD-364 for unsafe enumerator lifecycle, LinkedListNode lifetime, raw CopyTo
storage, mutable ReadOnlyDictionary.Empty, concurrent update loss, non-live
SortedSet views, FrozenDictionary duplicates, Hashtable contracts, and BitArray
enumeration. Final reconciliation built all configured targets and passed the
database/boundary/diff controls. Audit-only work made no production or
test-source changes; begin post-audit remediation only through separately
approved, bounded tickets.

## Candidate roadmap

The audit is complete. Ticket #1767 remediated SR-AUD-356 and SR-AUD-364 /
CCF-018, tickets #1768/#1769 remediated SR-AUD-357 / CCF-019, and tickets
#1770/#1771 remediated SR-AUD-358 / CCF-020. The findings index therefore
retains 364 original findings while recording 360 as open `confirmed` and four
as `remediated`. Post-audit remediation is the active priority; optional P2
breadth stays behind confirmed safety defects.

Design ticket #1768 selected the SR-AUD-357 / CCF-019 `LinkedListNode`
lifetime contract — independently allocated, reference-counted nodes with an
explicit null/detached/attached state model — and recorded it in
[`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md).
Implementation ticket #1769 (`REMED-COLL-LINKED-NODE`) completed it, taking the
index to 361 open `confirmed` findings and three `remediated` at that point.

Design ticket #1770 then answered SR-AUD-358 / CCF-020. It inventoried all six
`ICollection` implementations, the three test call sites, and the zero
production callers of the raw boundary; compared them with the current .NET
`ICollection.CopyTo(Array, int)` sources; and selected a length-aware, statically
typed `System::Span<std::any>` destination behind a non-virtual `ICollection`,
recorded in [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md).
No production change was made under #1770, so SR-AUD-358 stayed `confirmed`
until implementation closed.

The user approved the narrow public-API break on 2026-07-27, and implementation
ticket #1771 (`REMED-COLL-COPYTO`) landed it: the pure virtual
`CopyTo(void*, intcs)` is removed from `System::Collections::ICollection`, so
SR-AUD-358 is now `remediated` and the index records 360 open findings and four
`remediated`. Cleanup ticket #1772 (`REMED-COLL-COPYTO-CLEANUP`) is `wontfix`:
both of its items had to be completed inside #1771, because the deprecated shim
it would have deleted was never created and the `Array.hpp` / `Buffer.hpp`
doc-comments citing `ArrayList::CopyTo(void*, int)` could not be left pointing at
a member that no longer exists. The only follow-up is inactive ticket #1773
(`REMED-COLL-COPYTO-DOWNSTREAM`, P2, size S), the CNA and mobile-eggbert sweep
described in [`docs/Migration-ICollectionCopyTo.md`](docs/Migration-ICollectionCopyTo.md);
neither repository is in this checkout, so nothing is claimed about their current
usage.

Follow-up ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`, P1, size XS), opened and
closed 2026-07-27 on the same branch, then corrected a narrow defect found in
#1771's own validation rule: `detail::requireValidCopyDestination` rejected
every null-pointer destination outright, including a valid empty
`ObjectSpan{nullptr, 0}` or default-constructed empty `std::vector<std::any>`
copied from an empty collection. The rule now rejects a null pointer only when
the destination's length is positive; a non-empty collection copied into a
zero-length destination still fails, but on capacity, not nullness. SR-AUD-358
remains `remediated` — this did not reopen it. See
[`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md) section 22.
No repair ticket is active.

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
