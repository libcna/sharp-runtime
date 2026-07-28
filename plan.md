# Sharp Runtime plan

*Last verified: 2026-07-28 — 41 physical components, 90 direct production
dependency edges, a clean native build, 13,069 passing tests across 37
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
- Tests: 12,991 passing across 36 component binaries plus one integration
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
| `ticket` | 1,798 rows: 1,792 `done` — including audit ticket #1766, post-audit tickets #1767, #1768, #1769, #1770, and #1771, follow-up correction ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`), ticket #1775 (`REMED-COLL-HASHTABLE-VIEWS`), ticket #1776 (`REMED-CORE-ARGNULL-MESSAGE`), ticket #1777 (`REMED-COLL-COPYTO-DOC-SYNC`), ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`), ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`), ticket #1780 (`REMED-COLL-READONLYDICT-EMPTY`), ticket #1781 (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`), ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`), ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`), ticket #1784 (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`), ticket #1786 (`REMED-COLL-VERSION-COUNTER-OVERFLOW`), and ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`) design ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`), design ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`), implementation ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`), and design ticket #1785 (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, opened inactive by #1784 and closed by adopting .NET's nested-view validation order), design ticket #1795 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, opened because #1794 is an implementation row and was deliberately not reused), and implementation ticket #1794 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, which landed #1795's design under the full four-item approval: owning `std::any` Key/Value, a mandatory `MoveNext`-time snapshot on both implementations, two `ListDictionaryInternal` parity corrections, and an acknowledged silent ABI break through two independent mechanisms) — one `wontfix` (#1772, obsoleted by #1771), five deliberately inactive `blocked` rows (#1773, the out-of-repository CNA / mobile-eggbert `CopyTo` sweep; #1788 `REMED-COLL-LINKEDLIST-VERSION-WIDEN` and #1789 `REMED-COLL-BITARRAY-VERSION-WIDEN`, both opened by #1787 and both awaiting an explicit object-size approval; #1791 `REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, opened by #1790 and awaiting the four-part approval in `docs/ListIndexerVersioningDesign.md` section 28; and #1798 `REMED-COLL-LISTDICTINTERNAL-PARITY`, opened by #1797 carrying the two previously unrecorded `ListDictionaryInternal` defects it found while establishing whether the two `IDictionary` implementations agree — a `setItem` replace branch that returns before `++version_` where .NET bumps unconditionally, and a null key that is accepted and stored where .NET and this port's `Hashtable` both throw); design ticket #1797 (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, opened because #1796 is an implementation row and was deliberately not reused) and implementation ticket #1796 (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, which landed #1797's design under the full four-item approval: owning `std::any` from `getItem`/`at`/the `const` indexer, a non-copyable `ValueReference` proxy making `table[key] = value` a tracked insert-or-replace and a bare read no longer insert, `KeyNotFoundException` in place of `std::out_of_range`, and an acknowledged silent ABI break requiring a full consumer rebuild) are both `done`; no `todo`, `doing`, or `needs_user` rows |

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
to 12,991, most recently through the post-audit remediation regressions.

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
CCF-018, tickets #1768/#1769 remediated SR-AUD-357 / CCF-019, tickets
#1770/#1771 remediated SR-AUD-358 / CCF-020, and ticket #1775 remediated
SR-AUD-363. The findings index therefore
retains 364 original findings while recording 359 as open `confirmed` and five
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

Ticket #1775 (`REMED-COLL-HASHTABLE-VIEWS`, P1, size M), opened and closed
2026-07-27 on local branch `feature/remediation-coll-hashtable-views`, then
remediated SR-AUD-363, taking the index to 359 open `confirmed` findings and
five `remediated`. `Hashtable::getKeysProperty()`/`getValuesProperty()`
returned `nullptr` although `IDictionary` documents each as returning an
`ICollection` over the keys/values, so a contract-following consumer
null-dereferenced (ASan-confirmed SEGV) while the sibling
`ListDictionaryInternal` answered identical caller code correctly; and the
raw-key entry points stringified a null key as the address text `"0"`, which
also aliased the ordinary string key `"0"`. Both properties now return a live,
caller-owned `MemberCollection` reusing the #1771/#1774 copy boundary, and
`toKey()` is the single validating conversion site every raw-key path passes
through. No public signature changed and no virtual member was added or
removed, so this is neither a source nor an ABI break. Evidence: 70 permanent
regressions parameterised over both non-generic `IDictionary` implementations,
clean under ASan + UBSan + LeakSanitizer; Collections.Core 1,732/1,732; a
`-Werror` standalone consumer fixture; and a 12,991/12,991 full gate.

Two separate pre-existing defects found during #1775 were recorded as inactive
tickets rather than folded in: #1776 (`System::ArgumentNullException(paramName)`
emits its `(Parameter 'x')` suffix twice) and #1777 (four typed `CopyTo`
doc-comments still described ticket #1771's superseded null-destination rule).
Both are now done; see below.

**Correction:** the sentence above and #1776's own opening notes described
neither as a new audit identifier under the frozen SR-AUD-001..364 numbering.
That was inaccurate for #1776: SR-AUD-089 (the null-`const char*` crash in the
same constructors) and SR-AUD-090 (the duplicate suffix itself) already
existed as `confirmed` findings within that frozen range. Ticket #1776
(`REMED-CORE-ARGNULL-MESSAGE`, P2, size XS), opened and closed 2026-07-27 on
local branch `feature/remediation-argument-null-message`, corrected
`ArgumentNullException(paramName)` to pass its raw default message straight to
the `ArgumentException(message, paramName)` base constructor — matching
.NET's own `ArgumentNullException(paramName) : base(SR.ArgumentNull_Generic,
paramName)` — so the `(Parameter 'x')` suffix is appended exactly once, and,
as a side effect of removing the unsafe local `makeMsg()` concatenation, a
null `paramName` no longer null-dereferences. `ArgumentException` and
`ArgumentOutOfRangeException` were unaffected and remain regression-tested as
such. SR-AUD-089 and SR-AUD-090 are now `remediated` in
`audit/AUDIT_FINDINGS_INDEX.md`. Evidence: 26 new permanent regressions across
`ArgumentNullExceptionTests.cpp`, `ArgumentExceptionTests.cpp`, and
`ArgumentOutOfRangeExceptionTests.cpp`; the two pre-existing exact-message
workarounds this defect forced (`DictionaryKeyAndViewContractTests.cpp` from
#1775, `LinkedListNodeLifetimeTests.cpp` from #1769) now assert the
single-suffix message directly; the `Core.Base` standalone consumer fixture
extended to cover throw/catch through `System::Exception`; and a full
`scripts/local_ci_check.sh build` gate. No public signature, virtual member,
or inheritance changed, so this is neither a source nor an ABI break.

Ticket #1777 (`REMED-COLL-COPYTO-DOC-SYNC`, P3, size XS), opened and closed
2026-07-27 on local branch `feature/remediation-copyto-docs`, then corrected
the four typed `CopyTo(std::vector<T>&, intcs)` doc-comments on `Hashtable`,
`Queue`, `Stack`, and `ListDictionaryInternal` that still cited #1771's
superseded rule (`ArgumentNullException` for any null-pointer destination).
Each now states the rule #1774 corrected: `ArgumentOutOfRangeException` for a
negative index, `ArgumentException` for insufficient capacity (including a
non-empty collection into a zero-length destination), and
`ArgumentNullException` only for a null pointer paired with a positive length.
A repository-wide search found no other current public header with the stale
text; `ICollection.hpp`, the design and migration documents, and `README.md`
were already corrected under #1774. Documentation only — no implementation,
test assertion, or public signature changed, so this is neither a source nor
an ABI break; SR-AUD-358 and CCF-020 remain `remediated` and ticket #1773
remains `blocked`. Evidence: the 225 focused `CopyTo` tests and the full
1,732-test `Collections.Core` suite are unchanged; the `-Werror` standalone
`test/consumer/collections_copyto.cpp` consumer fixture recompiles and runs
successfully; a full `scripts/local_ci_check.sh build` gate passed
13,017/13,017 tests across 37 executables with zero warnings/errors; and
Doxygen 1.9.8 stayed at exactly 1,942/1,942 — unchanged, at the ceiling.

Ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`, P2, size S), opened and
closed 2026-07-27 on local branch
`feature/remediation-coll-concurrentdict-addorupdate`, then remediated
SR-AUD-360: `ConcurrentDictionary::AddOrUpdate` (both overloads) snapshotted
the existing value, ran the update factory outside the lock, then
unconditionally overwrote the entry with the factory's result even if another
thread had mutated or removed the entry meanwhile, silently discarding the
intervening write. Real .NET's `TryUpdateInternal` gates the commit on
`EqualityComparer<TValue>.Default.Equals` against the previously observed
value and retries (re-observes, re-invokes the factory) on a mismatch. Both
overloads now loop the same way, still never holding the internal mutex
across either factory call, and require `TValue::operator==` — the same
requirement `TryUpdate` on this class already carries. No public signature
changed and no virtual member was added or removed, so this is neither a
source nor an ABI break. Selected over the only other signature-compatible
candidate, SR-AUD-362 (`FrozenDictionary::Create` duplicate keys), after
checking SR-AUD-362 against the current .NET `FrozenDictionary.cs` source and
finding its premise does not hold: .NET's own doc-comment states
last-value-wins is the intended `Create`/`ToFrozenDictionary` behavior,
contrasted explicitly with `Enumerable.ToDictionary`'s throw-on-duplicate
behavior, and sharp-runtime's current implementation already matches it.
SR-AUD-362 was left untouched, not selected, and not reopened as a second
ticket; see `audit/AUDIT_FINAL_REPORT.md`'s planning-accuracy note.
SR-AUD-359 (`ReadOnlyDictionary::Empty`) and SR-AUD-361
(`SortedSet::GetViewBetween`) were set aside per NEXT.md's own note that they
may need a public-surface design decision. Evidence: a deterministic
coordinated-thread pre-fix reproduction (gitignored
`build-probe-concurrentdict/probe1_lost_update.cpp`) matching the finding's
own `add-or-update-result=1 final=1` symptom, clean post-fix under
ASan+UBSan+ThreadSanitizer plus a 16-thread/32,000-operation TSan stress
probe; 4 new permanent regressions in `ConcurrentDictionaryTests.cpp`;
`Collections.Core` 1,736/1,736 (was 1,732); and a full
`scripts/local_ci_check.sh build` gate of 13,021/13,021 tests across 37
executables with zero warnings/errors (was 13,017).

Design ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`, P2, size S),
opened and closed 2026-07-27 on local branch
`feature/remediation-coll-readonlydict-empty-design`, then answered SR-AUD-359,
selected over SR-AUD-361 after comparing both in detail. `ReadOnlyDictionary
<K,V>::Empty()` returns a non-`const` reference to a process-wide `static`
singleton; because the class relies on its compiler-generated copy assignment
operator, ordinary assignment through that reference silently rebinds the
singleton's private backing map for the remainder of the process. .NET's own
`Empty` is a get-only auto-property with no setter, so this is a C++-port-only
hazard, not a parity gap. SR-AUD-361 (`SortedSet::GetViewBetween`) would
instead require replacing `SortedSet<T>`'s `std::set` backing with a
tree structure supporting live, bounded, write-through sub-range views —
.NET's own `TreeSubSet` nested class is 378 lines — before any bounded
implementation ticket could be written, so it was left `confirmed` and not
selected. Recorded in
[`docs/ReadOnlyDictionaryEmptyDesign.md`](docs/ReadOnlyDictionaryEmptyDesign.md):
change `Empty()`'s return type to `const ReadOnlyDictionary<K,V>&`, the
literal C++ expression of ".NET has no setter." This is a public signature
change, so per the same approval boundary ticket #1770/#1771 used, it requires
explicit user approval; implementation was proposed as inactive ticket **#1780**
(`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS), `blocked` on that approval. No
production or test source changed under #1779. Evidence: three repository-local
ASan/UBSan-clean probes in the gitignored `build-probe-readonlydict/` tree
independently reproducing the audit's `empty-before=0`/
`empty-after-assignment=1` symptom (plus confirming the contamination is
visible process-wide, not local to one call site) and proving the proposed fix
both rejects the hazardous assignment at compile time and preserves every
existing observable behavior; the existing 17 `ReadOnlyDictionary` regression
tests rerun unchanged; and a full `scripts/local_ci_check.sh build` gate of
13,021/13,021 tests across 37 executables, zero warnings/errors (unchanged,
since no production/test source changed).

Ticket #1780 (`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS), opened and closed
2026-07-27 on local branch `feature/remediation-coll-readonlydict-empty`, then
implemented #1779's design after the user explicitly approved the public
return-type change, remediating SR-AUD-359: `Empty()`'s declared return type
changed from `ReadOnlyDictionary<K, V>&` to `const ReadOnlyDictionary<K, V>&`,
so assignment through its result is now a compile error instead of a silent,
process-wide rebind of the shared empty singleton's backing map. No other
member, constructor, or the class's copy/move assignment operators changed, so
ordinary, non-singleton instances remain freely assignable exactly as before;
no virtual member was added or removed (the class has none). This is a
source-breaking change only for the exact hazardous pattern of declaring an
explicit non-`const` reference to `Empty()`'s result or assigning through
it — confirmed absent everywhere in this repository — and not an ABI break: a
direct `nm`/`c++filt` comparison of the mangled `Empty()` symbol before and
after the change shows byte-identical names (the Itanium C++ ABI does not
encode a function's return type in its mangled name), and `Collections.Core`
is a header-only `INTERFACE` CMake target with no exported archive. Evidence:
pre-fix reproduction re-ran the design phase's own gitignored
`build-probe-readonlydict/probe1_mutable_empty.cpp` against the
still-unmodified production header, reconfirming the exact hazard; two new
post-fix probes compiled directly against the real, now-modified header (not a
copy) show the hazardous assignment now fails with `error: passing 'const
ReadOnlyDictionary<...>' as 'this' argument discards qualifiers`, while a
companion behavior-preservation probe runs clean under ASan+UBSan; two new
permanent regressions in `ObjectModelTests.cpp::ReadOnlyDictionaryTests` (a
`static_assert` pinning the exact return type, and
`Empty_RemainsEmptyAfterConstructingUnrelatedInstances`), with
`Empty_IsEmptyAndCached` retained verbatim; a new standalone
`test/consumer/collections_object_model_readonlydictionary.cpp` positive
fixture compiling `-Werror` and running successfully, plus a companion
`test/consumer/collections_object_model_readonlydictionary_negative.cpp`
negative fixture that fails to compile with the same diagnostic through the
repository's own consumer-fixture harness; `SharpRuntimeTests_Collections_ObjectModel`
grew from 124/124 to 125/125; and a full `scripts/local_ci_check.sh build` gate
of 13,022/13,022 tests across 37 executables, zero warnings/errors (was
13,021). Module boundaries stayed at 41 modules/90 edges; validator tests 7/7;
catalogue current; database consistent; the ten-job selective-component matrix
green; `git diff --check` clean; Doxygen re-measured at exactly 1,942/1,942 —
unchanged, at the ceiling.

While verifying the Doxygen baseline for ticket #1779, an independent
re-measurement using the repository's own canonical
`scripts/check_doxygen_warnings.sh` on the identical tree ticket #1778 left
behind returned exactly **1,942** warnings — the documented ceiling — not the
1,944 ticket #1778 recorded. A looser, non-canonical grep pattern reproduces
1,944 by additionally matching two `Inheritance graph ... not generated`
advisory lines that are not `file:line: warning:` diagnostics. This suggests
ticket #1778's figure came from a looser counting method, not a real
regression; inactive ticket **#1781**
(`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, P3, size XS) tracked re-verifying and
correcting this at pickup time. Not begun under #1779. See `NEXT.md`'s
equivalent correction note for full detail.

Ticket #1781 (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, P3, size XS), opened and
closed 2026-07-27 on local branch
`feature/remediation-docs-doxygen-count-reconcile`, then picked up that
re-verification: it re-ran `scripts/check_doxygen_warnings.sh` on a clean,
current tree three times, including once from a fully clean
`docs/generated/`, and got exactly **1,942** warnings every time, matching the
documented ceiling and confirming ticket #1779's re-measurement above still
holds with no drift since. Per the ticket's own acceptance criteria, it then
corrected ticket #1778's own `plan.sqlite3` notes and
`audit/AUDIT_PROGRESS.md`'s #1778 entry, both of which stated 1,944 as a
measured fact, with preserved-history Correction notes rather than rewriting
them; this document and `NEXT.md` already carried an accurate correction from
#1779 and needed no further content change beyond this closure paragraph and
the ticket-count line above. No production or test source changed; no
`SR-AUD-*` finding was reopened or created; this was kept a
documentation/measurement-methodology-only change, not folded into any
unrelated ticket.

Design ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`, P2, size M,
design-only), opened and closed 2026-07-27 on local branch
`feature/remediation-coll-sortedset-view-design`, then answered SR-AUD-361
without changing any production or test source.
`SortedSet<T>::GetViewBetween(lower, upper)` returns an independent snapshot
copy of the in-range elements instead of .NET's live, range-enforced,
bidirectionally write-through `TreeSubSet`, so ported C# that relies on
write-through compiles unchanged and silently mutates the wrong object.
Recorded in
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md): make
`SortedSet<T>` hold `std::shared_ptr<State>` — the `State` owning the
`std::set<T>` and the single version counter — plus `std::optional<T>` lower and
upper bounds, so one public type is either an owning full set or a bounded live
view over the same state and `GetViewBetween` keeps returning `SortedSet<T>` by
value. `std::shared_ptr` reproduces .NET's GC lifetime rule, so a view or an
iterator that outlives its set is well-defined rather than dangling; copying
preserves the object's role and assignment rebinds without mutating state
another handle observes. Four alternatives were rejected against a fourteen-row
compatibility matrix, including a dedicated `SortedSetView<T>` type (which does
not avoid the layout change, adds a return-type break on top, and breaks .NET's
structural parity in which a view **is a** `SortedSet<T>`) and retaining
snapshot semantics (a permanent, silent, undiagnosable divergence — the failure
mode ticket #1771 refused when it declined a throwing `CopyTo` shim).

**Correction:** the ticket #1779 paragraph above states that SR-AUD-361 "would
instead require replacing `SortedSet<T>`'s `std::set` backing with a tree
structure supporting live, bounded, write-through sub-range views". That premise
does not hold: `std::set` already provides `lower_bound`, `upper_bound`, and
stable iterators, and ticket #1782's working prototype demonstrates a bounded
live view on top of it. The real cost is the ownership model, the copy/move
semantics, and one required `const` removal. Recorded here rather than rewritten
in place, per this repository's preserved-narrative practice.

Evidence: six repository-local, gitignored probes in `build-probe-sortedset/`.
`probe1` independently reproduces the finding's own
`source-add-visible-in-view=0` / `view-add-visible-in-source=0` symptom and the
complete pre-fix contract, clean under ASan+UBSan+LeakSanitizer — the current
implementation is memory-safe and semantically wrong. `probe4` runs the
identical matrix against a prototype of the selected architecture with
`failures=0`, clean under the same sanitizers, including owner destruction with
surviving views and a 100,000-element scale case. `probe5` measures the
compatibility consequences rather than asserting them:
`sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104,
`sizeof(Iterator)` 24 → 40, and the Itanium mangled name changing `_ZNK…` →
`_ZN…` when `const` is dropped — unlike ticket #1780's `Empty()`, whose mangled
name was byte-identical. Four adjacent defects measured inside the same member's
surface — `GetViewBetween` requiring `operator>` although the class documents
`operator<` only, bounds not enforced after construction, nested views silently
widening, and whole-object assignment defeating the fail-fast version guard
(silently wrong dereference on copy-assign, ASan-confirmed
`heap-use-after-free` on move-assign) — are folded into the implementation
ticket rather than given new `SR-AUD-*` identifiers, the numbering being frozen
at 364. The three existing `GetViewBetween` tests and the 41
mutable-`SortedSet` tests rerun unchanged and passing; boundaries stayed at 41
modules/90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 at exactly 1,942/1,942 — unchanged,
since only `docs/*.md` and `audit/*.md` were added and Doxygen scans neither.
The full `scripts/local_ci_check.sh build` gate was run rather than omitted and
passed **13,022/13,022 tests across 37 executables** with zero build warnings
and zero errors — unchanged from ticket #1780, as expected when no production
or test source changes. `scripts/check_selective_components.sh` was not run: no
public header and no component metadata changed.

SR-AUD-361 stays **`confirmed`**, qualified `confirmed (design-complete)`, so
the index still records 355 open and nine `remediated`. Implementation is
separate ticket **#1783** (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L),
created **`blocked`** and not begun, pending explicit user approval of three
things together: removing the `const` qualifier from `GetViewBetween`, the
snapshot-to-live-view semantic change, and the object-layout change requiring
every consumer to be rebuilt — the same approval category tickets #1770/#1771
and #1779/#1780 needed. There is no in-repository source break: all three
`GetViewBetween` call sites are tests on non-`const` sets and none asserts a
snapshot property. If approval is refused, the recorded fallback keeps snapshot
semantics while fixing the four adjacent defects; it needs no approval and
closes none of SR-AUD-361. Ticket #1773 remains `blocked` and untouched.

Implementation ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L),
opened and closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-live-view`, then landed that design after
the user granted the exact approval, scoped to this ticket. **SR-AUD-361 is now
`remediated`**, so the index records 354 open and ten `remediated`.
`GetViewBetween` keeps its return type and parameters, loses its `const`
qualifier, and returns a live bounded handle onto the same tree: inclusive
bounds enforced for the life of the view, out-of-range `Add` throwing
`ArgumentOutOfRangeException("item")`, out-of-range `Remove` returning `false`,
range-scoped `Clear`, narrowing-only nested views flattened onto the root state,
.NET's exact invalid-range message, a version-cached view `Count`, range-scoped
`Min`/`Max`, one shared version counter invalidating every handle's enumerators
in both directions, and bounds-enforcing write-through set algebra whose
self-aliasing guard now compares shared state rather than object identity.
Copying preserves the object's role, assignment rebinds without disturbing other
handles, and the additive `ToSortedSet()` materializes an independent range —
the documented replacement for the old snapshot behavior. The four adjacent
defects are closed with it, still without new `SR-AUD-*` identifiers.

Measured compatibility matched every #1782 prediction exactly:
`sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104,
`sizeof(Iterator)` 24 → 40, traits preserved, and the mangled name changing
`_ZNK…` → `_ZN…`. Two limitations are recorded in design-record section 30
rather than hidden: a bounded exception-ordering divergence from .NET for a
nested call that is simultaneously inverted and widening, and a
ThreadSanitizer-measured race when concurrent threads call `getCountProperty()`
on *one* view object — documented in the header, not synchronized, since
`SortedSet<T>` claims no thread safety and this ticket adds none.

Closure evidence: 47 new permanent regressions plus a positive standalone
consumer fixture (`-Werror`, `Collections.Core` only, exits 0) and a negative
`const`-caller fixture (correctly rejected); the three pre-existing
`GetViewBetween` tests and all 41 mutable-`SortedSet` tests passing unchanged;
`SharpRuntimeTests_Collections_Core` 1,783/1,783; `scripts/local_ci_check.sh
build` at **13,069 tests across 37 executables**, up from 13,022, with zero
build warnings and zero errors; 41 modules/90 edges with no new dependency edge;
validator tests 7/7; catalogue current; database consistent; `git diff --check`
clean; Doxygen 1.9.8 at **1,937**/1,942; all ten selective components passing
with a repository-local `TMPDIR` (run this time, because a public header
changed); and a clean ASan+UBSan+LeakSanitizer campaign with LSan verified
active. Ticket #1773 stays `blocked`; CNA and mobile-eggbert were not inspected
or modified.

Ticket #1784 (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`, P1, size S), opened and
closed 2026-07-28 on local branch `feature/remediation-coll-sortedset-count-race`,
then removed the second of those two limitations. It is a **post-audit defect
with no `SR-AUD-*` identifier** (the numbering stays frozen at 364), and
**SR-AUD-361 stays `remediated`** — this corrects a defect introduced by that
finding's remediation rather than reopening it, so the index counts are
unchanged at 354 open and ten `remediated`.

#1783's judgement that the race was acceptable "since `SortedSet<T>` claims no
thread safety" is reversed here on three grounds: a C++ data race is undefined
behavior rather than a merely unhelpful result; `getCountProperty()` is `const`
and warns nobody at the call site that calling it is a write; and it was a
*regression*, because the pre-#1783 header's `const` members wrote nothing. The
.NET comparison does not transfer either — a racing `int` write is defined in
the CLR, and .NET documents that its collections support multiple concurrent
readers.

Five repair alternatives were **measured** rather than argued
(`build-probe-sortedset/probe11_cache_alternatives.cpp`): removing the cache
gives `sizeof(SortedSet<int>)` 40 → 32, a `std::mutex` 80, a `std::shared_mutex`
96, and a published `shared_ptr` snapshot 48 — every one breaking the layout
#1783 had approved — while same-width atomics stay at exactly 40 and 104. A
cache relocated into the shared `State` was rejected structurally, since
arbitrary overlapping view bounds would require an unbounded keyed map, new
allocation, and a new element-type requirement. Selected: two
`std::atomic<intcs>` fields with a release/acquire publication protocol — count
stored first (`relaxed`), version stored last (`release`), version loaded first
(`acquire`) — so the (count, version) pair can never be read torn. Two relaxed
atomics would not have sufficed. `state_->version` deliberately stays plain, and
two `static_assert`s make a padded-atomic platform a compile error rather than a
silent ABI break.

The header now states the contract in two unequal halves: concurrent **mutation**
stays unsupported and undefined, with a set and every view over it one collection
for that purpose and **no new promise of mutation safety**; concurrent
**read-only** access is race-free, because no `const` member writes an
unsynchronized field. The type is still not thread-safe — it is merely free of
*internal* races when read.

Compatibility is unchanged in every layer: `sizeof(SortedSet<int>)` 40,
`sizeof(SortedSet<std::string>)` 104, `sizeof(Iterator)` 40, `alignof` 8, all
four value-semantics traits, and the mangled `GetViewBetween` symbol are
byte-identical to #1783's stored probe output, so no consumer rebuild is needed
on this revision's account and no new user approval was required. Count keeps its
complexity — O(1) for an owning set, O(k) once per version for a view — and
allocates nothing.

Closure evidence: 29 new permanent regressions in `SortedSetCountCacheTests.cpp`
(functional Count matrix, cache-sensitive properties, and the exact
pointer-to-member type of fourteen public members, with the published sizes
behind a 64-bit guard rather than an unconditional `static_assert`); post-fix
ThreadSanitizer clean in all nine real modes with the self-test still reporting
2, and #1783's own unmodified probe going 1 race → 0; ASan+UBSan+LSan 76/76 with
LSan verified active by a deliberate-leak self-test;
`SharpRuntimeTests_Collections_Core` 1,812/1,812 (was 1,783);
`scripts/local_ci_check.sh build` at **13,098 tests across 37 executables** (was
13,069), zero warnings and errors, after which the 13,069 floor in `README.md`
and `CLAUDE.md` was raised; all ten selective components passing plus
`Collections.Core` in isolation; the extended positive consumer fixture
compiling `-Werror` and exiting 0 with the negative fixture still rejected;
41 modules/90 edges; validator tests 7/7; catalogue current; database
consistent; `git diff --check` clean; Doxygen 1.9.8 unchanged at **1,937**/1,942.

Two **inactive** tickets were opened and not begun, neither with a new
`SR-AUD-*` identifier: **#1785** (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`,
P3, XS) for the nested-view exception-ordering divergence #1783 recorded, which
needs a semantic decision rather than a bug fix; and **#1786**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, S) for the `int32_t` mutation-version
counter, which is incremented without bound and compared only for equality by
both the Count cache and `Iterator::checkVersion`. Both properties predate #1783
— they arrived with ticket 1713 — and #1784 changed only memory ordering, not
the values, the type, or the equality test.

Ticket #1786 (`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, size S), opened
inactive by #1784, was then completed on local branch
`feature/remediation-coll-sortedset-version-overflow`. It carries **no new
`SR-AUD-*` identifier**, does not reopen SR-AUD-361, and is not a regression
from #1783 or #1784. Opened as an assessment; the assessment found a fully
compatible repair, so it was implemented in the same ticket. Four defects were
reproduced against the real production header before anything changed, using a
single probe source built against both the committed pre-fix header and the
working tree and positioning the counter with `-fno-access-control` rather than
performing billions of mutations: `++state_->version` at `INTCS_MAX` is
signed-integer overflow, reported by UBSan as
`signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`
inside `Add`; a counter wrapped 2^32 mutations on silently revalidates a stale
`Iterator`; the same wrap silently revalidates a stale cached view `Count`; and
— a fourth defect the ticket's own description did not list, and the worst of
them — the `-1` `kCountNotCached` sentinel is itself a reachable counter value,
so a view that had **never** computed its Count read its cache as warm and
answered 0. .NET's own `SortedSet` has defects two, three, and four as
*defined-but-wrong* behaviour, since the CLR defines signed overflow as wrapping
where C++ makes it undefined; matching .NET's integer width would therefore not
have made the C++ code correct, and this port deliberately exceeds it. The
shared counter and the `Iterator` snapshot are now `SharpRuntime::ulongcs`,
which is free because the counter is not a member of `SortedSet<T>` and
`Iterator` already had four bytes of tail padding; the Count cache's 32-bit tag
cannot be widened without breaking the approved layout, so it is stored biased
by one and compared widened, which identifies a counter value exactly, cannot be
produced by a never-filled cache, and stops the cache being written once the
counter outgrows it. A first implementation used an explicit horizon branch and
measurably cost +1 ns on every `Count` call, including an owning full set's; two
variant headers isolated the branch as the cause and the biased tag removed it.
Closure evidence: 29 new permanent regressions in
`SortedSetVersionOverflowTests.cpp`, whose near-boundary cases reach the counter
through a test-only friend seam rather than a production hook;
`SharpRuntimeTests_Collections_Core` 1,841/1,841 (was 1,812) with no assertion
edited; `scripts/local_ci_check.sh build` at **13,127 tests across 37
executables** (was 13,098), zero warnings and errors, after which the 13,098
floor in `README.md` and `CLAUDE.md` was raised; UBSan clean post-fix;
ASan+UBSan+LSan 105/105 with LSan verified active; ThreadSanitizer clean across
#1783's, #1784's, and a new six-mode probe covering the recompute path, with
both self-tests still reporting races; `sizeof`, `alignof`, all traits, the
mangled `GetViewBetween` symbol, and **every member offset** unchanged; both
consumer fixtures behaving as before; 41 modules/90 edges; validator tests 7/7;
catalogue current; database consistent; `git diff --check` clean; Doxygen 1.9.8
unchanged at **1,937**/1,942; all ten selective components plus
`Collections.Core` in isolation. The contract is recorded in
`docs/SortedSetVersioningDesign.md`, with a pointer from
`docs/SortedSetLiveViewDesign.md` section 32.

One further ticket was opened by #1786 and not begun, again with no
`SR-AUD-*` identifier: **#1787**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, M), covering the other
collections that carry the identical `intcs version_` counter. #1786's
stored acceptance criteria asked for a repository-wide implementation; the
instruction governing that working session scoped #1786 to `SortedSet<T>` and
required the remainder to become a separate inactive ticket. The divergence is
recorded in the design document rather than silently absorbed, and the full
inventory the criteria asked for is delivered there. **#1787 is now done — see
below.**

### Completed repository-wide mutation-counter sweep: ticket #1787

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) is
**done**, closed 2026-07-28 on local branch
`feature/remediation-coll-version-counter-sweep`, with no new `SR-AUD-*`
identifier and no audit finding reopened. The full record is
[`docs/CollectionVersionCounterSweep.md`](docs/CollectionVersionCounterSweep.md).

It corrected #1786's inventory in three ways rather than inheriting it. The
count is **sixteen** counter-carrying types, not fifteen: `BitArray` was
missed, and it is also the one whose counter was already `std::uint32_t` rather
than `intcs`, so it never had the signed-overflow undefined behaviour. And a
**third defect class** was found that appears in neither ticket's description
and is more serious than either recorded one: the implicitly declared copy/move
assignment operator transplanted the *source's* counter into the destination, so
an enumerator outstanding over the destination survived having every element it
could refer to destroyed. It needs **no overflow at all** — the counters merely
have to match — and six of the fourteen affected types reproduced as
AddressSanitizer `heap-use-after-free` or `heap-buffer-overflow` rather than as
wrong answers. `LinkedList<T>` was immune, because ticket #1769 had already
given it a bumping `operator=`.

Pre-fix evidence, all against the committed headers before anything changed
(gitignored `build-probe-collversion/`, one probe source built against both
revisions): **14** UBSan signed-overflow reports, one per collection; **15**
iterator/enumerator ABA reproductions at the 2^32 alias distance; **8**
assignment-alias reproductions; **6** ASan memory errors.

The repair replaces each bare integer with the new
`System::Collections::detail::BasicMutationCounter`, whose increment is unsigned
and whose **assignment advances the destination instead of taking the source's
value**, while copy construction still inherits it (matching .NET's
`ArrayList.Clone`/`Hashtable.Clone`). Thirteen collections took the 64-bit
`MutationCounter`; `LinkedList<T>` and `BitArray` took the 32-bit
`NarrowMutationCounter` because widening them grows a public object — measured,
arithmetically unavoidable in any member order — so both keep a documented,
test-pinned 2^32 residual and both have a **blocked** ticket stating the exact
approval required.

Closure evidence: **336** new permanent regressions in
`CollectionVersionCounterTests.cpp`, whose near-boundary cases reach every
counter through **one** test-only friend seam
(`SharpRuntime::Testing::CollectionVersionAccess<T>`) generalising #1786's;
`SharpRuntimeTests_Collections_Core` **2,177/2,177** (was 1,841) with no
existing assertion edited; `scripts/local_ci_check.sh build` at **13,463 tests
across 37 executables** (was 13,127), zero warnings and errors, after which the
13,127 floor in `README.md` and `CLAUDE.md` was raised; UBSan and ASan clean
post-fix on every probe mode; ASan+UBSan+LSan **349/349** with LSan verified
active twice over — it caught a real 24-byte leak in this ticket's own first
draft test; ThreadSanitizer **0 races** in three real modes with the self-test
still reporting 2; every `sizeof`, `alignof`, and counter offset unchanged, with
**0 symbols removed or renamed** and 10 new weak inline definitions for the new
counter class; a new positive consumer fixture compiling `-Werror` and exiting 0
and a new negative one correctly rejecting the test seam as an incomplete type;
41 modules/90 edges; validator tests 7/7; catalogue current; database
consistent; `git diff --check` clean; Doxygen 1.9.8 at **1,938**/1,942 — one
warning more than the pre-ticket 1,937, attributable entirely to the single new
`README.md` markdown link into `docs/`, which `Doxyfile` does not scan (the six
existing README links into `docs/` each cost the same, and the ceiling is
unchanged); all ten selective components plus `Collections.Core` in isolation;
performance within run-to-run noise on every benchmarked path.

Three tickets were opened and deliberately not begun: **#1788**
(`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, S, **blocked**) needs approval for
`sizeof(LinkedList<T>)` 40 → 48; **#1789**
(`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, XS, **blocked**) needs approval for
`sizeof(BitArray::Enumerator)` 32 → 40; and **#1790**
(`REMED-COLL-LIST-INDEXER-VERSION`, P3, L, `todo`) records the separate,
pre-existing, non-versioning divergence that `List<T>::operator[]` returns a
plain `T&` and so cannot bump the counter the way .NET's index setter does.
#1788 and #1789 are deliberately **not** one ticket: they share the symptom and
nothing else, and a user might reasonably approve one and not the other.

Tickets #1785 and #1773 remain untouched and inactive. **Ticket #1790 is now
design-complete — see the next section.**

### Completed List<T> indexer versioning design: ticket #1790

Ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`, P3, size L, category `parity`)
is **done as a design ticket**, closed 2026-07-28 on local branch
`feature/remediation-coll-list-indexer-design`, with **no `SR-AUD-*`
identifier** — the numbering stays frozen at 364. It changed no production
behaviour, signature, layout, or exception; the single production edit is a
doc-comment correction in `List.hpp`. The durable record is
`docs/ListIndexerVersioningDesign.md`.

It answers the ticket's own question with a **no**: there is no fully
source-compatible correction. A plain `T&` cannot be intercepted, because C++
provides no mechanism by which a collection learns of a write through a
reference it previously returned. Acceptance-criteria route (a) — declare the
divergence permanent — was rejected, because the same `T&` is a **reproduced
use-after-free** (four AddressSanitizer heap-use-after-free reports: across a
reallocating `Add()` on both read and write, across `Clear()`, and across move
assignment), not merely a fail-fast divergence. The selected architecture is a
tracked proxy, `System::Collections::detail::ElementReference<T>`, chosen
because it is the only alternative that closes the write path while keeping
`list[i] = v` — the exact spelling C# uses — compiling.

Three of the ticket's own premises were corrected rather than inherited. The
indexer is **not** the widest hole: the non-const `ToVector()` hands out the
whole backing `std::vector<T>&`, permitting `push_back`/`clear` — a *structural*
mutation the guard never sees, previously undocumented anywhere. The migration
premise was wrong for this repository: measured across **all 625 translation
units** by compiling against a `[[deprecated]]`-tagged shim, the non-const
indexer has **61 call sites, all in two test files**, and **no library source
includes `List.hpp` at all** — while the CNA/mobile-eggbert burden stays real,
unmeasured, and out of scope. And `IList<T>` has **four** implementers, not one
(`List<T>`, `ObjectModel::Collection<T>`, `ObjectModel::ReadOnlyCollection<T>`,
and a hand-written one in the test suite), found by compiling against the
candidate header rather than by grepping — a `grep` for `public IList<` finds
only the first.

Measured source break, from four generated shims compiled against the whole
repository at `-fsyntax-only`: the refined proxy breaks **1 site in 1 of 625
translation units** — the hand-written implementer, i.e. migration — against
**8 sites in 3 units** for the rejected value/setter alternative, of which only
2 are genuine call-site breaks. The two figures are close and the decision is
explicitly **not** made on them; it is made because the value alternative
deletes `list[i] = v` from the API. Layout measured: `sizeof(List<T>)`
**40 → 40** unchanged, `sizeof(ObjectModel::Collection<T>)` **32 → 40**, proxy
16 bytes. The unavoidable cost, stated rather than buried: `list[i].member` and
`list[i].method()` stop compiling for value-type elements, because `operator.`
cannot be overloaded.

Closure evidence: **14 new permanent regressions** in
`ListIndexerVersionTests.cpp`, split into a `Contract` suite (must survive
#1791) and a `Divergence` suite (each case asserting today's behaviour with
.NET's named alongside, carrying `static_assert`s #1791 cannot land without
editing); `SharpRuntimeTests_Collections_Core` **2,191/2,191** (was 2,177) with
no existing assertion edited; `scripts/local_ci_check.sh build` at **13,477
tests across 37 executables** (was 13,463), zero warnings and errors; 41
modules/90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 unchanged at **1,938**/1,942.

Two tickets were opened and deliberately not begun. **#1791**
(`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, L, **blocked**) carries the
implementation in two phases: Phase 1 (tracked `getItem`/`setItem`, a pure
addition) needs **no** approval but does not close the defect; Phase 2 needs the
exact four-part approval in design section 28 — public source breaks to
`List<T>::operator[]` and to the `IList<T>` interface, an object-layout change
to `Collection<T>`, and acknowledgement that CNA's usage is unmeasured. The
#1771, #1780, and #1783 approvals do **not** carry over. **#1792**
(`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, M, `todo`) records a **newly
discovered defect**: `Generic::IEnumerator<T>::getCurrentProperty()` does
`const_cast<T*>(&Current())` and publishes a mutable `void*` to the live element
on a public interface, so a write through it mutates a collection mid-walk with
the counter at rest. It affects **every** collection, not `List<T>`, which is
why it is its own ticket and not absorbed. No new `SR-AUD-*` identifier.
**Correction (ticket #1792, 2026-07-28): the "every collection" claim in the
sentence above is wrong** — `Dictionary`, `HashSet`, `SortedSet`, and
`SortedDictionary` implement no `IEnumerator` at all. See the next section.

Tickets #1785, #1788, #1789, and #1773 remain untouched and inactive.
**Ticket #1792 is now done — see the next section.**

### Completed enumerator Current safety design: ticket #1792

Design ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M,
category `defect`), opened inactive by #1790, was completed on local branch
`feature/remediation-coll-ienumerator-current-design`. It carries **no
`SR-AUD-*` identifier** and reopens no finding. It **changed no production
behaviour, no public signature, no object layout, and no exception** — it edits
no production source at all. The record is
[`docs/IEnumeratorCurrentSafetyDesign.md`](docs/IEnumeratorCurrentSafetyDesign.md).

Opened as "remove the `const_cast` or document the divergence as deliberate", it
answers the first: the divergence is remediable and is **not** recorded as
permanent. The selected architecture is **`std::any` returned by value from the
non-generic accessor** — the direct C++ counterpart of .NET's `object
IEnumerator.Current`, which returns a value, boxes value types, and hands out no
pointer. `Generic::IEnumerator<T>::Current()` stays `const T&`.

It corrected four of its own premises rather than inheriting them. **The defect
does not reach every collection**: the four hash- and tree-backed generic
collections expose STL-style iterators and no `IEnumerator`, so the measured
reach is thirteen generic plus eight non-generic implementations plus two
hand-written test-local ones. **The bridge's `const_cast` is not the only one**:
four more live outside it, one of which publishes a writable pointer to a live
`std::unordered_map` key. **It is six distinct defect classes, not one**, closed
by different measures — and `const void*` was *measured* to close only the first
of them, because a one-line `const_cast` restores the write. **The most dangerous
property is the ABI**: `void*`, `const void*`, and `std::any` all produce the
byte-identical mangled name while `this` moves from `%rdi` to `%rsi`, so a
partially rebuilt consumer links with no diagnostic and corrupts memory.

Evidence: four ASan `heap-use-after-free` reports plus two non-faulting
stale-aliasing shapes; a `ReadOnlyCollection<T>` mutated through its own
enumerator, reaching the caller's shared backing vector; a `Hashtable` entry made
unreachable by both its old and its new key while `Count` still reported it; 0
UBSan diagnostics and 0 LSan leaks; a five-candidate allocation and layout
comparison; calling-convention disassembly; a 626-translation-unit deprecation
sweep measuring 28 non-generic, 4 bridge, and 27 typed call sites with 0 compile
failures; and three fully migrated header shims breaking 6, 7, and 6 translation
units at 12, 14, and 12 sites with **zero library sources broken under any of
them**, so the proposed bodies are compile-validated.

Closure evidence: **17 new permanent regressions** in
`EnumeratorCurrentSafetyTests.cpp`, split into a `Contract` suite (8 cases that
must survive #1793) and a `Divergence` suite (9 cases carrying `static_assert`s
#1793 cannot land without editing); `SharpRuntimeTests_Collections_Core`
**2,208/2,208** (was 2,191) with no existing assertion edited;
`scripts/local_ci_check.sh build` at **13,494 tests across 37 executables** (was
13,477), zero warnings and errors; 41 modules/90 edges; validator tests 7/7;
catalogue current; database consistent; `git diff --check` clean; all ten
selective components passing; Doxygen 1.9.8 unchanged at **1,938**/1,942.

One ticket was opened and deliberately not begun — **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, L), which is now
**done**; see the next section. As opened it was **blocked**, in two
phases. Phase 1 (write the ownership/lifetime/validity rules into both headers)
needs **no** approval and does not close the defect. Phase 2 needs the exact
three-part approval in design section 33 — public source breaks to
`System::Collections::IEnumerator` and to `Generic::IEnumerator<T>`, and
acknowledgement of the silent ABI break requiring a full consumer rebuild. There
is **no** object-layout change. The #1771, #1780, and #1783 approvals do **not**
carry over. **#1793 should land before #1791**, and the two must not be merged:
they are independent defects on disjoint surfaces and neither repairs the other.

Two residual limitations are recorded rather than buried: the typed `Current()`
reference hazard is **not** closed (closing it needs a by-value `Current()`,
which makes move-only `T` uninstantiable), and
`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` keep returning
`const void*` into live storage, which is a separate follow-on rather than a
widening of this approval.

Tickets #1785, #1788, #1789, #1791, and #1773 remain untouched and inactive.

### Completed enumerator Current safety implementation: ticket #1793

Implementation ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`,
P2, size L, category `defect`), opened blocked by #1792, was completed on local
branch `feature/remediation-coll-ienumerator-current-safety` after the user
granted design section 33's three-part approval explicitly and scoped to it. It
carries **no `SR-AUD-*` identifier** and reopens no finding; SR-AUD-356 stays
`remediated` from ticket #1767. Both phases landed together. The record is
[`docs/IEnumeratorCurrentSafetyDesign.md`](docs/IEnumeratorCurrentSafetyDesign.md)
section 34, appended below #1792's design record rather than rewriting it.

`System::Collections::IEnumerator::getCurrentProperty()` now returns an owning
`std::any` **by value** instead of a mutable `void*`.
`Generic::IEnumerator<T>::Current()` is unchanged at `const T&`; its inherited
bridge boxes a copy and throws `System::NotSupportedException` for an element
type that cannot be copied, matching .NET's documented `ref struct` answer. All
four `const_cast`s outside the bridge are gone and both `mutable` members are
ordinary again. Eight production non-generic overrides, the bridge, two
test-local implementers, and the three in-library call sites migrated; **zero
library sources broke**, exactly as the design's shim sweep predicted.

The defect was reconfirmed before any production edit, with the output preserved
under `build-probe-ienumerator/prefix1793/`: 15 defects across six modes, four
ASan `heap-use-after-free` reports, a `ReadOnlyCollection<T>` mutated through
its own enumerator into the caller's shared vector, a `Hashtable` entry made
unreachable by both its old and its new key while `Count` still reported it, and
a same-width wrong cast silently wrong with no sanitizer diagnostic.

Four corrections to the design's section 14 sketch are recorded in section 34.3,
two of them caught only by running the new suite: the `if constexpr`
else-branch had to call `Current()` and discard it, or a move-only `T` would
have reported `NotSupportedException` where the pre-#1793 bridge reported
`InvalidOperationException`; `Generic::List<std::any>` cannot be instantiated at
all, because `std::any` is not equality-comparable; `std::any(Current())` for
`T = std::any` selects the copy constructor, so the box is never nested; and the
non-generic `Stack`/`Queue` `ICollection` constructors gained a
`std::bad_any_cast` path.

Closure evidence: the nine `Divergence` cases were **flipped, not deleted** —
renamed `EnumeratorCurrentSafety`, each asserting the opposite outcome through
the same accessor, with the `static_assert`s now pinning `std::any` so a revert
cannot land silently; `SharpRuntimeTests_Collections_Core` **2,229/2,229** (was
2,208); a **clean full rebuild** in a dedicated `build-abi-1793` tree at
**13,515 tests across 37 executables** (was 13,494), zero warnings and errors;
ASan+UBSan clean on all six migrated lifetime shapes; 0 LSan leaks with
LeakSanitizer proved active by a deliberate-leak self-test; TSan deliberately
not run, with the reason stated; object layout `diff`-identical to the stored
baseline; the mangled name byte-identical and the vtable slot unchanged at
offset `0x20`; a stale-object probe in which an old caller and a new
implementation **linked with zero diagnostics** and then took a SEGV; a positive
consumer fixture passing under `-Werror` and a negative fixture rejected at all
six marked sites; 41 modules/90 edges; validator tests 7/7; catalogue current;
database consistent; `git diff --check` clean; Doxygen 1.9.8 at **1,939**/1,942
— one above the canonical 1,938, because `Doxyfile` does not scan `docs/` and the
new `README.md` link into it resolves as an unresolved `\ref`, exactly as its two
neighbouring entries already do (design record section 34.8).

Allocation was measured rather than assumed: 0 for `int`, a raw pointer, and an
already boxed `int`; **1** for a small SSO `std::string` and a
`std::shared_ptr`; 2 for a 64-char `std::string` and a `DictionaryEntry`. The
middle row corrects design section 22, which predicted 0 for any type at most
one pointer wide — libstdc++'s `std::any` small-buffer optimisation admits only
types that *fit in* a `void*`. A full consumer rebuild is mandatory and the
linker will not say so; README.md carries that warning with the migration table.

Three residual limitations stand, unchanged from the design's risk register: the
typed `Current()` reference hazard is **not** closed (its validity window is now
in the header, together with the statement that #1793 did not close it);
`IDictionaryEnumerator`'s `const void*` accessors are untouched, with a warning
now on that interface and a ticket of their own — **#1794**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M, `blocked`, not begun); and CNA's and
mobile-eggbert's usage remains unmeasured, with #1773 still `blocked`.

Tickets #1785, #1788, #1789, #1791, and #1773 remain untouched and inactive. No
repair ticket is active.

### Completed SortedSet nested-view exception ordering: ticket #1785

Ticket #1785 (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, P3, size XS,
category `design`) is **done**, closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-nested-order` after the user explicitly
approved acceptance branch **(b)** — adopt .NET's ordering — scoped to this
ticket alone. It carries **no `SR-AUD-*` identifier** (numbering frozen at 364)
and reopens nothing; SR-AUD-361 stays `remediated`. The record is
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md) section 33,
appended below sections 1–32 rather than rewriting them; §15's original ordering
rule is preserved verbatim under two supersession markers pointing at §33.

`SortedSet<T>::GetViewBetween` now validates in .NET's order — lower widening,
then upper widening, then the lower-versus-upper relationship — where #1782
selected and #1783 shipped the reverse. Exactly one `if` moved.
`SortedSet.TreeSubSet.cs:342-353` performs both widening tests against its own
bounds and only then delegates to `SortedSet.cs:1508-1515`, which owns the
`SR.SortedSet_LowerValueGreaterThanUpperValue` check; the widening tests are
therefore unconditionally first, the lower bound precedes the upper, and
`_underlying` is the root set, which is why nesting flattens to depth 1.

A probe printed the whole matrix before and after
(`build-probe-sortedset/probe18_{prefix,postfix}.log`): **exactly 7 of 32 outcome
rows changed**, every one a nested call that is *simultaneously* widening and
inverted. `view[3,7]` asked for `(2, 1)` is now
`ArgumentOutOfRangeException("lowerValue")` and `(12, 9)` is now
`ArgumentOutOfRangeException("upperValue")`, both formerly
`ArgumentException("Must be less than or equal to upperValue.", "lowerValue")`.
Every success, every widening-only failure, every inverted-only failure, and
every **top-level** call is byte-identical: an owning set activates neither
bound. Widening *both* ends while inverted is arithmetically unreachable and is
proved so by an exhaustive grid rather than asserted.

Nothing else moved: no public signature, return type, `const` qualification,
mangled symbol, vtable, `sizeof`, `alignof`, or member offset; ownership, live
write-through, bounds inclusivity, nested flattening, Count caching, iterator
invalidation, the thread-safety contract, and the allocation behaviour are all
unchanged, and a rejected call still bumps no version. Every in-repository
caller was reviewed — six test files and two consumer fixtures, no production
`src/` caller — and none asserted a doubly-invalid nested call.

Closure evidence: `SortedSetNestedViewOrderTests.cpp` adds **23** permanent
cases, including an exhaustive `(lower, upper)` grid checked against .NET's
decision procedure transcribed as an independent oracle;
`SharpRuntimeTests_Collections_Core` **2,252/2,252** (was 2,229);
`scripts/local_ci_check.sh build` at **13,538 tests across 37 executables** (was
13,515), zero warnings and errors; ASan+UBSan+LSan over four SortedSet suites at
**128 tests, 0 diagnostics, 0 leaks**, with LeakSanitizer proved active by a
deliberate-leak self-test; TSan deliberately not run, with the reason stated;
the SortedSet consumer fixture extended and passing under `-Werror`; 41
modules/90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 **unchanged at 1,939**/1,942; all ten
selective components plus `Collections.Core collections_sorted_set_view.cpp` in
isolation. The disposable `build-abi-1793` tree from #1793 was removed after
confirming its results are already recorded here — **1.46 GiB reclaimed** — with
its two evidence logs kept.

Tickets #1788, #1789, #1791, #1794, and #1773 remain untouched and inactive. No
repair ticket is active.

SR-AUD-362 (`FrozenDictionary::Create` duplicate keys) was reconciled
conservatively alongside #1779, per that finding's own instruction to inspect
rather than repair it: its per-file audit report and
`audit/AUDIT_FINDINGS_INDEX.md` row now carry a Correction note
cross-referencing ticket #1778's finding that its premise does not hold
against the current .NET reference. The repository's index status vocabulary
supports only `confirmed`/`remediated`, so it stays `confirmed` rather than
being assigned an invented status — but it must not be read as an active,
un-investigated defect, and it is not counted as `remediated`.

No repair ticket is active.

### Completed IDictionaryEnumerator key/value design: ticket #1795

Design ticket #1795 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, P3, size M,
category `design`) was completed on local branch
`feature/remediation-coll-idictenumerator-keyvalue-design` on 2026-07-28, with
**no production or test-source change**. Durable record:
[`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`](docs/IDictionaryEnumeratorKeyValueSafetyDesign.md).

**Ticket #1794 was not reused.** Its row is an *implementation* row — it migrates
the accessors and is blocked on approval to do so, not on a decision about what
to do. Converting it into a completed design ticket to reuse the number would
have recorded implementation work as done when none was. It stays `blocked`, and
#1795 is the design.

Selected: **Entry-canonical owning accessors with a mandatory `MoveNext`-time
snapshot.** `getEntryProperty()` stays `DictionaryEntry` by value and becomes
canonical; `getKeyProperty()`/`getValueProperty()` return an owning `std::any`
by value equal by construction to that entry's members; `getCurrentProperty()`
keeps #1793's signature and boxes the `DictionaryEntry` on **both**
implementations, matching .NET's `Current => Entry`; and every implementation
must answer every accessor from state the enumerator owns.

**The return-type change alone is not the fix.** Neither accessor
version-checks, so both dereference a container iterator a mutation may have
invalidated. On `ListDictionaryInternal` that makes even `getEntryProperty()` and
the already-migrated `getCurrentProperty()` AddressSanitizer
`heap-use-after-free` after `Clear()` or destruction. .NET's own
`HashtableEnumerator` snapshots `_currentKey`/`_currentValue` at `MoveNext` and
never reads `_buckets` from an accessor; the design adopts that.

Three premises in #1794's own description are contradicted by measurement and
corrected in the design record. The most important: *"there is no write path
through them"* is **false for `Hashtable`** — `getValueProperty()` returns a
pointer to the live map's non-`const` `std::any`, so `const_cast` + assignment is
well-formed, defined C++ that rewrites dictionary storage with the mutation
counter unmoved and a second enumerator silent; and `getKeyProperty()` reaches
the `const std::string` key, producing at 64 entries an entry `Count` still
reports that **no lookup can return by either its old or its new key**.

Two previously unrecorded `ListDictionaryInternal` parity defects were found and
decided: its `getCurrentProperty()` boxes the key where .NET is
`Current => Entry`, and it disagrees with itself about `const` on a value.

Measured: 10 unique call sites across 628 translation units (0 compile
failures); a fully migrated three-header shim breaks **1 of 628** at 1 line and
**2 of 2,252** permanent tests, both being the two parity defects above; 20
pre-fix defects with **8 ASan `heap-use-after-free`** reports, 0 UBSan
diagnostics on the corruption, and three fatal scenarios that complete *silently*
under UBSan alone; 42 post-fix assertions clean under ASan+UBSan+LSan; the
mangled name **byte-identical** and the vtable slot unchanged at `0x30` while
`this` moves `%rdi` → `%rsi`; a stale caller and new implementation **link with
zero diagnostics** then SEGV with a UBSan invalid-vptr report; a **second**
stale-object vector — `ListDictionaryInternal::NodeEnumerator` 40 → 72 bytes with
an `inline` `GetEnumerator()`, reproduced as ASan `heap-use-after-free` — noting
`NodeEnumerator` is a **private nested class**, so this is not a public layout
change; allocations 0 → 1/2 for a `Hashtable` key and **0 → 0** for
`ListDictionaryInternal`, with `Entry` and `Current` unchanged.

Seven alternatives were evaluated with a compatibility matrix. Alternative F
(enumerator-owned copies behind an unchanged `const void*`) was **measured** —
0 of 628 translation units break, no calling-convention change — and rejected as
the selected design because it leaves type safety and implementation divergence
entirely open and reintroduces the enumerator/collection desynchronisation
#1793 removed. It is the documented fallback if the approval is declined and
must never be called a remediation.

**#1794 stays `blocked`**, now depending on #1795, with a rewritten four-item
approval: a public source break to `IDictionaryEnumerator`; two observable
`ListDictionaryInternal` behaviour changes; acknowledgement of a silent ABI break
through **two** independent mechanisms requiring a full consumer rebuild; and
item 2 separately declinable. #1793's, #1771's, #1780's and #1783's approvals do
not carry over. Recommended order: **#1794 before #1791**; the two migrations
must not be merged.

Validation: boundaries 41 modules / 90 edges, validator tests 7/7, catalogue
current, database consistent, `git diff --check` clean, Doxygen 1.9.8 at
**1,939/1,942** unchanged, and `scripts/local_ci_check.sh build`
**13,538/13,538 across 37 executables** with zero warnings/errors —
all unchanged, as expected for a design-only ticket. No new `SR-AUD-*`
identifier; **SR-AUD-356 stays `remediated`** and CCF-018 is not reopened. The
defect is **not** marked remediated. Tickets #1773, #1788, #1789, #1791, and
#1794 remain `blocked` and untouched.

### Completed IDictionaryEnumerator key/value safety implementation: ticket #1794

Implementation ticket #1794 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M,
category `defect`) is **done**, on local branch
`feature/remediation-coll-idictenumerator-keyvalue-safety`, landing the
architecture #1795 selected under the §33 approval granted **in full** (items 1,
2a, 2b, 3).

`getKeyProperty()` and `getValueProperty()` return an **owning `std::any` by
value**; `getEntryProperty()` is canonical and unchanged; `getCurrentProperty()`
keeps #1793's signature and now boxes the `DictionaryEntry` on **both**
implementations. The load-bearing half is the **mandatory `MoveNext()`-time
snapshot**: `ListDictionaryInternal::NodeEnumerator` gained a `DictionaryEntry
current_` (40 → 72 bytes, a **private nested** class), and **no accessor on
either implementation dereferences a container iterator** — which is what closes
the lifetime class, since no accessor version-checks and .NET's own
`HashtableEnumerator` snapshots for exactly this reason.

Re-measured before any source changed: `defects=20` on the write paths,
identical to #1795; and **nine** ASan `heap-use-after-free` reports across
sixteen lifetime scenarios — **correcting #1795 §8.2's prose, which said
"eight" where its own table listed nine**. Three of the nine needed no caller
misuse at all. Three further corrections to the design record are written into
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37.1: §24 never measured
`MoveNext()`, and the snapshot costs `ListDictionaryInternal::MoveNext()`
**2.8 → 23.9 ns per position** while `Hashtable::MoveNext()` is unchanged;
§12.3's predicted 2,250/2,252 became **2,252/2,252** once both predicted
assertions were updated; and §22's synthetic ABI numbers were **re-measured on
the real interface**, where every prediction held.

Both **silent ABI mechanisms were reproduced end to end on the real headers**:
byte-identical mangled names and unchanged vtable slots (`0x30`, `0x38`) with
`this` displaced `%rdi` → `%rsi` behind a hidden `sret` — links with zero
diagnostics, then SEGV, UBSan invalid vptr, and a bogus
`System::InvalidOperationException` out of garbage; and the `NodeEnumerator`
40 → 72 growth against an `inline` `GetEnumerator()` — links clean, then ASan
`heap-use-after-free`. **A full consumer rebuild is mandatory**; README.md
carries the entry with per-shape migration guidance.

Validation: **+64 permanent tests** in a new suite parameterised over both
implementations; the three pinned assertions **updated, not deleted**; post-fix
probe on real headers at 42 assertions with **0 ASan/UBSan/LSan diagnostics and
0 leaks**; the new suite under ASan+UBSan+LSan at 78 tests clean, leak detection
proved active by the 284-byte self-test; **TSan not run, precondition verified**
(no `mutable` member, every accessor `const`, every `current_` write inside the
non-`const` `MoveNext()`/`Reset()`); pre-fix caller source no longer compiles;
positive consumer fixture clean under `-Werror` and passing, negative fixture
rejected at every marked site; boundaries 41 modules / 90 edges; validator tests
7/7; catalogue current; database consistent; `git diff --check` clean;
`scripts/check_selective_components.sh` run with a repository-local `TMPDIR`;
Doxygen 1.9.8 at **1,940/1,942**, the single new warning identified as the
unresolvable `\ref` for the new `README.md` link into `docs/`. The full gate ran
from a dedicated clean **`build-abi-1794`** tree at **13,602 tests across 37
executables**, zero warnings/errors, `SharpRuntimeTests_Collections_Core` at
**2,316**.

**SR-AUD-356 and CCF-018 are recorded as remediated by this ticket.** No new
`SR-AUD-*` identifier. Left open and stated rather than buried:
`MoveNext()`/`Reset()` after collection destruction remain undefined, and the two
**pre-existing** `Hashtable` write escapes outside this interface now have their
own inactive ticket **#1796** (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, P3,
`blocked`) instead of only a risk note. Tickets #1773, #1788, #1789, #1791 and
#1796 remain `blocked` and untouched.

### Completed Hashtable value-access design: ticket #1797

Design ticket #1797 (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, P3, size M,
`design`) is **done**. No production or test source changed. Durable record:
[`docs/HashtableValueAccessSafetyDesign.md`](docs/HashtableValueAccessSafetyDesign.md).

**Ticket #1796 was not reused.** Its row is an *implementation* row — it closes
the escapes and is blocked on approval to perform that change, not on a decision
about what it is — so recording it as a completed design would log implementation
work as done when none was performed. #1796 **stays `blocked`**, now depending on
#1797, with acceptance criteria and an exact four-item approval rewritten from
the design. This is the same #1795 → #1794 handling one ticket earlier.

**Four of #1796's own premises are corrected by measurement**, each against this
record's convenience:

1. **Four escape routes, not two.** #1796 names `operator[]` and `getItem()`. It
   misses `at()`, which returns a `const std::any&` **into live map storage**
   where `const_cast` + assignment is well-formed, fully defined C++ that rewrote
   the stored value with the counter unmoved; and it misses `setItem`/`Add`'s
   non-`const` `void*` value parameter, a type hole on the input side.
2. **Rehash does *not* dangle a retained alias.** `std::unordered_map` is
   node-based, and the address of a stored value was **unchanged across 8,000
   insertions**. The hazard is `Remove`, `Clear`, copy assignment, move
   assignment and destruction — **nine ASan `heap-use-after-free` reports across
   fourteen scenarios**, 0 LSan leaks with detection proved active by a 317-byte
   self-test.
3. **The worst defect is one #1796 never mentions.** `operator[]` on an *absent*
   key performs a **structural insert** without bumping, so a bare read changes
   `Count`. Measured at 4,008 entries, an outstanding enumerator then visited
   **2,045 distinct keys**, reached **6 of its 8** pre-mutation seed keys, threw
   nothing, and produced **no report from ASan, UBSan or LSan**. All sixteen
   reproduced defects are silent under UBSan alone.
4. **The sibling `IDictionary` implementation has its own defects.**
   `ListDictionaryInternal::setItem`'s *replace* branch returns before
   `++version_` where .NET does `version++` first unconditionally, and it accepts
   and **stores a null key** where .NET and this port's `Hashtable` both throw.
   Filed as **new inactive ticket #1798** (`REMED-COLL-LISTDICTINTERNAL-PARITY`,
   P3, `blocked`), not absorbed.

**Selected: owning reads, tracked writes, no public alias into storage.**
`getItem` → `std::any` by value; `operator[]` → a non-copyable
`Hashtable::ValueReference` proxy whose read conversion returns `std::any` **by
value**, plus a new `const` by-value overload; `at()` → by value throwing
`KeyNotFoundException` instead of `std::out_of_range`. Two proxy details are
load-bearing rather than stylistic, and both were found by measurement:
`std::any`'s template converting constructor outranks a *copyable* proxy's own
conversion operator, so `std::any b = h[k];` silently boxes the **proxy** and
throws `std::bad_any_cast` at run time with nothing wrong at compile time; and a
conversion returning `const std::any&` makes `const std::any& r = h[k];` trip
GCC 14's `-Wdangling-reference`, which every module here compiles with `-Werror`.

**Measured:** 12 call sites across 629 translation units, all in tests, with
`operator[]` at **0** sites; **3** units / 5 sites broken by Phase 2, all in the
test suite — fewer than migrating `getItem` alone (6 units), because the sibling
implementer is migrated in the same change; `at()` → by value breaks **0**;
mangled name **byte-identical** and vtable slot unchanged at `0x38` while `this`
moves `%rdi → %rsi` behind a hidden `sret`, reproduced as a stale caller that
**links with zero diagnostics then SEGVs** with 14 UBSan misaligned-address
reports; `sizeof(Hashtable)` **unchanged at 72** — this is *not* a layout break
in #1788/#1789/#1791's sense; reads cost **0 allocations for an `int`**, 1 for an
SSO string, 2 for a large one, 1.2 → 5.4/15.7/27.7 ns.

**The obvious tidy-up is rejected on evidence**: migrating `setItem`/`Add`'s
raw-key value parameter to `const std::any&` makes `Add("literal", v)` store the
entry under the **stringified address of the literal**, because the standard
`const char*` → `const void*` conversion beats the user-defined
`const char*` → `std::string` one — and it compiles clean under `-Werror`.

**Alternative A′ (`const std::any*`) is the documented fallback**, measured as
**byte-identical machine code** to today. It leaves the alias-lifetime class
entirely open and must never be recorded as a remediation. A shared proxy with
#1791 is explicitly rejected on four measured incompatibilities; recommended
order is **#1796 before #1791**, and the migrations must not be merged.

Validation, all unchanged as expected for a design-only ticket: 41 modules / 90
edges, validator tests 7/7, catalogue current, database consistent,
`git diff --check` clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling,
`scripts/local_ci_check.sh build` at **13,602 tests across 37 executables**.
`check_selective_components.sh` not run (no public header or component metadata
changed); required when #1796 Phase 2 lands. Build directories: `build/`
(reused, `--parallel 3`) and the **shared** `build-probe/` (one compiler process
per probe; `MAX_JOBS = 3` in the two Python sweeps). **No compilation exceeded
three jobs** — the ceiling was lowered from four to three during this ticket at
the user's instruction, and `scripts/local_ci_check.sh` and
`scripts/check_selective_components.sh` were corrected from their hard-coded
`--parallel 4` in the same change.

Tickets #1773, #1788, #1789, #1791 and #1796 remain `blocked` and untouched;
#1798 is newly opened `blocked` and deliberately not begun; #1790, #1792, #1793,
#1794 and #1795 remain `done`. #1793 and #1794 were not reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified.

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

---

## Post-audit remediation batch — ticket #1796, `Hashtable` value-access escapes closed (2026-07-28)

Implementation ticket **#1796** (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, P3, size
M, category `defect`, area Collections) is **`done`** on branch
`feature/remediation-coll-hashtable-value-access`. It implements design ticket
**#1797** exactly; the durable record is
`docs/HashtableValueAccessSafetyDesign.md`, whose new §34 is the
implementation-complete section. **No new `SR-AUD-*` identifier** — the audit
numbering stays frozen at 364, and every defect closed here was found during
remediation.

**The user granted design §32's four-item approval explicitly and per action**,
in this ticket's own instruction: (1) the public source break, (2) the one silent
semantic change, (3) the silent ABI break requiring a full consumer rebuild, and
(4) the changed exception type on `at()`.

**All four escape routes are closed** — the two #1796 was named after and the two
#1797 found:

| Member | Was | Now |
|---|---|---|
| `IDictionary::getItem(const void*) const` | `void*` into live storage, from a `const` member | **`std::any` by value** |
| `Hashtable::operator[](const std::string&)` | `std::any&`, and a bare read **inserted** | **`ValueReference` proxy** — tracked write, owning read, no insert on read |
| `Hashtable::operator[](const std::string&) const` | did not exist | **`std::any` by value** |
| `Hashtable::at(const std::string&) const` | `const std::any&`, `std::out_of_range` | **`std::any` by value, `KeyNotFoundException`** |
| `Hashtable::setItem(const std::string&, const std::any&)` | did not exist | **new typed tracked setter** |
| `ListDictionaryInternal::getItem` | `void*` | **`std::any`**, boxing the same caller pointer — mechanical only |
| `setItem`/`Add` raw-key `void*` *value* parameter | — | **unchanged, deliberately** (design §13.4) |

**Two corrections to #1797's own record, both against convenience.** The Phase 2
source break is **3 translation units and seven source lines, not five** —
#1797's "5 sites" counted distinct compiler *diagnostics*, and
`ListDictionaryInternalTests.cpp`'s three `int*`-shaped `getItem` comparisons
share one GoogleTest template instantiation, so two of them produced no
diagnostic of their own yet still had to be edited. And **zero `test/consumer/`
fixtures needed migration, not three**: all five pre-existing `Collections.Core`
fixtures compile and run unmodified. Everything else in #1797 reproduced exactly.

**Post-fix evidence.** The nine AddressSanitizer `heap-use-after-free` reports
across fourteen lifetime scenarios are **0**; UBSan **0**; LeakSanitizer **0**
with detection proved active by a 318-byte / 2-allocation deliberate-leak
self-test. Rerun in #1797's exact experiment shape — 8 seed keys, one outstanding
enumerator, 4,000 missing-key reads through `operator[]` — `Count` goes
**8 → 8** where it went **8 → 4,008**, and the enumerator walks **8 of 8**
distinct keys with 0 duplicates where it walked **2,045** and reached **6 of 8**
seeds while throwing nothing and producing no sanitizer report.

**The ABI break was reproduced end to end against the real production
declarations**, with the old headers extracted from git rather than approximated:
mangled name **byte-identical**, vtable slot **unchanged at `0x38`**, no symbol
added or removed, `this` moving `%rdi → %rsi` behind a hidden `sret`. A stale
caller **links with `exit=0` and then segfaults with `exit=139`**, preceded by 14
UBSan misaligned-address diagnostics naming the caller's key pointer used as
`this`. `sizeof(Hashtable)` is **unchanged at 72** and
`sizeof(ListDictionaryInternal)` at **40** — not an object-layout break;
`ValueReference` is 40 bytes and is never stored by the collection. `README.md`
carries the mandatory-full-rebuild breaking-change entry.

Permanent coverage:
`modules/collections/tests/System/Collections/HashtableValueAccessSafetyTests.cpp`,
**55 tests**, parameterised over both `IDictionary` implementations wherever the
assertion is about the interface, clean under ASan + UBSan + LSan. Consumer
fixtures `test/consumer/collections_hashtable_value_access.cpp` (compiled **and
run** against `Collections.Core` alone under `-Wall -Wextra -Wpedantic -Werror`)
and `..._negative.cpp` (**11 of 11** marked alias spellings rejected, verified
per-site by `build-probe/1796_check_negative.py` rather than by the file merely
failing to compile).

Validation: `scripts/local_ci_check.sh build` at **13,657 tests across 37
executables**, zero warnings and zero errors, from a tree **reconfigured from
scratch (`cmake --fresh`) and rebuilt with `--clean-first`** for the silent ABI
break — **626 translation units recompiled, 37 executables relinked, and zero
object files on disk predating the fresh configuration**.
`SharpRuntimeTests_Collections_Core` at **2,371** (was 2,316; +55, exactly the
new suite). 41 physical modules / 90 dependency edges, validator tests 7/7,
catalogue current, `scripts/db_consistency_check.py` clean, `git diff --check`
clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling (unchanged), and
`scripts/check_selective_components.sh` **run in full** with a repository-local
`TMPDIR` because public headers changed. `scripts/__pycache__` absent; every
Python tool run with `PYTHONDONTWRITEBYTECODE=1`.

**No new build directory was created.** `CLAUDE.md` rule 10 closes the name set,
so the mandatory clean build reconfigured `build/` itself rather than adding a
`build-abi-1796/` — the per-ticket habit #1794's `build-abi-1794` exemplified and
#1797 ended. Directories used: `build/`, the shared `build-probe/` (this ticket's
artefacts under a `1796_` file prefix), `build-consumer/`, and `build-tmp/` as
`TMPDIR`. **No compilation exceeded three jobs.**

**Still open and explicitly not claimed closed:** `setItem`/`Add`'s raw-key
`void*` *value* parameter (deliberate, design §13.4, with the
`Add("literal", v)` address-key corruption that is the reason); accessor use
after the *collection* is destroyed; a `ValueReference` outliving its table (the
port-wide borrow rule — documented on the class, not enforced);
`const std::any& r = h[k];` still compiling and now meaning a snapshot (the one
silent meaning change, documented in `README.md` with the instruction not to
write it). A **pre-existing, unrelated** finding was observed and **recorded
rather than fixed**: `CollectionVersionAccess<Hashtable>` and
`CollectionVersionAccess<ListDictionaryInternal>` are explicitly specialised with
*different* bodies in two translation units of one binary
(`CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY` has
`positionVersion`; `DictionaryEnumeratorKeyValueSafetyTests.cpp`'s
`SR1794_SEAM_BODY` does not), which is IFNDR. It predates this ticket, is benign
in practice, and #1796 deliberately did not make it worse — the new suite spells
its specialisation token-for-token identically to the `SR1794` one. Fixing it is
outside #1796's approval.

Tickets #1773, #1788, #1789, #1791 and #1798 remain `blocked` and untouched;
**#1791 was not implemented and no shared List/Hashtable proxy was introduced**,
so #1797 §24's four measured incompatibilities and its recommended
**#1796-before-#1791** order stand. #1790, #1792, #1793, #1794, #1795 and #1797
remain `done`, and none of them was reopened. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified, so the source-break figures
here are *this repository only*. No push, merge, rebase, tag, or publication
occurred.
