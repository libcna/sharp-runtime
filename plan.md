# Sharp Runtime plan

*Last verified: 2026-07-29 — 41 physical components, 91 direct production
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
- Tests: 14,070 passing across 36 component binaries plus one integration
  binary, verified by ticket #1817 (SR-AUD-079, the canonical final-quantum
  rule) through the full repository gate, raised from the 14,002 verified by
  ticket #1816 (SR-AUD-078 / CCF-013, the in-place Base64
  encoders' write order), itself raised from the
  13,994 verified by ticket #1814 (SR-AUD-236,
  `HttpContentJsonExtensions`'s null content), itself raised from the 13,987
  verified by ticket #1812 (SR-AUD-242, `ZipArchive`'s null stream), itself
  raised from the 13,979
  verified by ticket #1811 (SR-AUD-257, the compression streams' null
  inner stream), itself raised from the 13,970
  verified by ticket #1810 (SR-AUD-132, the interpolated-string
  handler's raw destination pointer), itself raised
  from the 13,958
  verified by ticket #1807 (SR-AUD-097, AggregateException's null inner
  exception_ptr), itself raised from the 13,948
  verified by ticket #1806 (SR-AUD-338, the text stream wrappers'
  null base stream), itself raised from the 13,937
  verified by ticket #1805 (SR-AUD-341, the MemoryStream raw-buffer
  constructor), itself raised from the 13,923
  verified by ticket #1789 from a fully fresh configuration and a
  clean-first rebuild -- which the BitArray::Enumerator object-layout change
  made mandatory rather than merely prudent, exactly as #1788's LinkedList<T>
  one did -- itself raised from the 13,880 verified by ticket #1788, from the 13,840
  verified by
  ticket #1791, itself raised from the 13,790 verified by ticket #1802 and
  re-measured by ticket #1800. #1800 moved test code between files without
  adding or removing a case, so the figure was unchanged rather than stale at
  that point. (The 12,991 figure this line once carried was a stale relic; each
  remediation ticket's own section below states the count it measured.)
- Component graph: 41 physical modules and 91 direct production edges. The
  ninety-first was added by ticket #1814: `Net.Http.Json` now declares the
  `Core.Base` public dependency its header needs to throw
  `ArgumentNullException`, an edge it previously took only transitively through
  `Net.Http`.
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

The 2026-07-29 local snapshot contains:

| Table | State |
|---|---|
| `task` | 16,201 rows: 1,082 `ported`, 140 `ignore`, 14,979 legacy `ignored`; no unclassified or `tobedecided` rows |
| `ticket` | 1,804 rows: 1,801 `done` — including audit ticket #1766, post-audit tickets #1767, #1768, #1769, #1770, and #1771, follow-up correction ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`), ticket #1775 (`REMED-COLL-HASHTABLE-VIEWS`), ticket #1776 (`REMED-CORE-ARGNULL-MESSAGE`), ticket #1777 (`REMED-COLL-COPYTO-DOC-SYNC`), ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`), ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`), ticket #1780 (`REMED-COLL-READONLYDICT-EMPTY`), ticket #1781 (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`), ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`), ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`), ticket #1784 (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`), ticket #1786 (`REMED-COLL-VERSION-COUNTER-OVERFLOW`), and ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`) design ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`), design ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`), implementation ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`), and design ticket #1785 (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, opened inactive by #1784 and closed by adopting .NET's nested-view validation order), design ticket #1795 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, opened because #1794 is an implementation row and was deliberately not reused), and implementation ticket #1794 (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, which landed #1795's design under the full four-item approval: owning `std::any` Key/Value, a mandatory `MoveNext`-time snapshot on both implementations, two `ListDictionaryInternal` parity corrections, and an acknowledged silent ABI break through two independent mechanisms) — one `wontfix` (#1772, obsoleted by #1771), two deliberately inactive `blocked` rows (#1773, the out-of-repository CNA / mobile-eggbert `CopyTo` sweep; and, since 2026-07-29, #1804 `REMED-TOOLING-SEAM-DISCOVERY-VACUITY`, opened inactive by #1803 because `scripts/check_version_seam_odr.py` exits 0 when a seam *leaves* its discovery rule — measured, covered by the two consumer fixtures, and not a defect today. **This clause previously named #1803 `REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE` as the second inactive row, for the one seam — `SortedSetVersionAccess` — whose *consumer-side* unreachability had no negative fixture; #1803 is now `done`, having added `test/consumer/collections_sorted_set_version_negative.cpp` (15 sites, 9 fixtures / 66 sites in total) with no production change, and the count is taken from the database on each edit**; **this clause said "five" and still listed #1791 and #1788 as blocked until ticket #1788 corrected it: #1791 was closed earlier the same day and #1788 closed itself, so two of the five were already stale when written**; implementation ticket #1788 (`REMED-COLL-LINKEDLIST-VERSION-WIDEN`) is now `done`, having widened `LinkedList<T>`'s mutation counter and its enumerator's snapshot to 64 bits under the explicit approval that `sizeof(LinkedList<T>)` may grow 40 → 48 on LP64, with a measured silent binary break and a mandatory full consumer rebuild; implementation ticket #1789 (`REMED-COLL-BITARRAY-VERSION-WIDEN`) is likewise now `done`, having done the same for `BitArray` under its own separate approval that `sizeof(BitArray::Enumerator)` may grow 32 → 40 on LP64 while `sizeof(BitArray)` stayed 48, so **no collection retains a 2^32 enumerator-snapshot ABA horizon and `detail::NarrowMutationCounter` has no user left** — this clause listed #1789 among the inactive `blocked` rows and said "three" until #1789 itself corrected it to two, the count being taken from the database on each edit; **all three rows #1799 opened inactive are now closed**: #1802 `REMED-COLL-HASHTABLE-REMOVE-VERSION`, #1800 `REMED-COLL-VERSION-SEAM-ODR` and #1801 `REMED-TOOLING-NEGATIVE-FIXTURE-CI`, see below; **this line said "eight" while listing seven until #1802 corrected it, and the count is taken from the database on each edit**); design ticket #1797 (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, opened because #1796 is an implementation row and was deliberately not reused), design ticket #1799 (`REMED-COLL-LISTDICT-SETITEM-DESIGN`, opened for the same reason against #1798 and closed 2026-07-29 with no production change), implementation ticket #1798 (`REMED-COLL-LISTDICTINTERNAL-PARITY`, which landed #1799's design under the full three-item approval: a private `ValidatedKey` making null-key rejection structurally unskippable across all five raw-key entry points, one `setItem` upsert whose bump follows the mutation and covers replacement and equal-value replacement, deletion of the `const_cast` that made the key view's `CopyTo` publish a writable pointer to a caller's `const` object, two deliberate deviations from .NET's bump-first shape on a throwing `Add` and an absent `Remove`, and an acknowledged **silent** stale-object hazard requiring a full consumer rebuild), and implementation ticket #1796 (`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, which landed #1797's design under the full four-item approval: owning `std::any` from `getItem`/`at`/the `const` indexer, a non-copyable `ValueReference` proxy making `table[key] = value` a tracked insert-or-replace and a bare read no longer insert, `KeyNotFoundException` in place of `std::out_of_range`, and an acknowledged silent ABI break requiring a full consumer rebuild) are both `done`, as is tooling ticket #1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`, which made all **seven** — not six — negative consumer fixtures compile per site from `scripts/local_ci_check.sh`, 37 sites, after reproducing the whole-file false pass and proving the checker against a 7/7 mutation campaign; no production source, signature, symbol or layout changed); no `todo`, `doing`, or `needs_user` rows |

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

### Completed ListDictionaryInternal setter design: ticket #1799

Design ticket #1799 (`REMED-COLL-LISTDICT-SETITEM-DESIGN`, P3, size M,
`design`) is **done**. No production or test source changed. Durable record:
`docs/ListDictionaryInternalSetterDesign.md`.

**Ticket #1798 was not reused.** Its row is an *implementation* row — it performs
the corrections and is blocked on approval to do so, not on a decision about what
they are — so recording it as a completed design would log implementation work as
done when none was performed. #1798 **stays `blocked`**, now depending on #1799,
with its acceptance criteria and an exact three-item approval rewritten from the
design. Same #1795 → #1794 and #1797 → #1796 handling one and two tickets
earlier.

**Selected:** a private `ValidatedKey` boundary that every raw-key operation must
construct before the single `findNode()` locator will look at storage —
structurally unskippable, unlike `Hashtable`'s `toKey()` convention — plus one
upsert path in `setItem` that bumps on insert, on replace **and** on an
equal-value replace with the bump placed *after* the mutation (a **strong**
exception guarantee .NET's bump-first shape cannot offer), plus deletion of the
one `const_cast` that made the key view's `CopyTo` disagree with every other key
surface. **No signature, return type, parameter type or data member changes.**

**Four of #1798's own premises are corrected by measurement**, each against this
record's convenience:

1. **Six defects, not two.** #1798 names the `setItem` replace bypass and the
   accepted null key. It misses that the **key view's `CopyTo` launders away the
   caller's `const`** — `MemberCollection::copyToCore` boxes
   `const_cast<void*>(n.key)` where all four other key surfaces box
   `const void*`, so `std::any_cast<const void*>` on a `CopyTo` slot throws
   `std::bad_any_cast`, and a write through the `void*` it *does* hand out was
   reproduced as an **AddressSanitizer SEGV on read-only storage** — and misses
   that `Add`-on-duplicate and `Remove`-of-an-absent-key both diverge from .NET
   in the **opposite** direction from the setter.
2. **"Match .NET's unconditional `version++`" is the wrong instruction.** .NET
   `ListDictionaryInternal` bumps first and unconditionally, before it even
   searches, so a throwing `Add` and a no-op `Remove` both invalidate every
   outstanding enumerator; **.NET's own `Hashtable` does neither**, and .NET's
   two implementations disagree on three of ten version rows. Copying the former
   literally would introduce two new false-positive
   `InvalidOperationException`s and would contradict a currently *passing*
   assertion (`CollectionVersionCounterTests.cpp`'s `ListDictionaryAdapter` sets
   `kHasNoOpMutation = true` for an absent-key `Remove`). The selected rule is
   **advance on effective mutation**, which matches .NET on every row where
   .NET's two implementations agree and takes the `Hashtable` rule where they
   disagree.
3. **The null-key rationale is not SR-AUD-363's.** On `Hashtable`, `nullptr`
   stringified to `"0"` and *aliased* the ordinary string key `"0"`. Here keys
   are compared by raw address and **no valid object has the null address**, so
   a stored null key aliases nothing — measured. The defect is purely that the
   **two implementations of one interface disagree**, on all five raw-key entry
   points.
4. **The stale-object hazard does not crash the way #1794's and #1796's did.**
   No signature changes and every affected body is `inline` in a header, so an
   unrebuilt consumer **silently keeps the defect** — and the outcome is
   **link-order and optimisation-level dependent**: at `-O0` with the stale
   object first on the link line, a correctly *rebuilt* translation unit
   silently reverted to the old bodies, and `-flto -Wodr` diagnoses nothing.

**Measured, against the committed headers unless stated:** the ten-row version
table across the port's two `IDictionary` implementations and both .NET
references, showing **three** divergences on `ListDictionaryInternal` (replace,
equal replace, and — in the other direction — throwing `Add` and absent `Remove`)
and **one previously unrecorded divergence on `Hashtable`**; **four** enumerator
kinds silently valid after a value replacement — the dictionary enumerator, the
key view, the value view and the same through an `IDictionary&` — one of which
**enumerated the post-mutation value**, with **0 ASan and 0 UBSan reports**; six
null-key rows on which the two implementations disagree, with a null key proved
**not** to alias any real key and "absent" still distinguishable from "present
with a null value"; the key view boxing `const void*` on `Current` and `void*` on
`CopyTo`, with `std::bad_any_cast` on one and an **ASan SEGV on a write to
`.rodata`** through the other, while the same object reached through `Current`
cannot be written at all; LeakSanitizer **proved active** by a 350-byte
deliberate-leak self-test with **0 leaks** in every real scenario; **53 of 53**
`ListDictionaryInternal` mangled names byte-identical, the **19-entry vtable
identical** with `getItem` at offset 72 and `setItem` at 80, and `sizeof`
unchanged at **40 / 72 / 24 / 24** with `ValidatedKey` at 8 bytes emitting **no
symbol at all** at `-O2`; the stale-object probe at `-O0`, `-O2` and
`-flto -Wodr`; **0 added allocations** with `setItem` replace moving 1.30–1.66 ns
to 1.53–1.60 ns; and the selected design passing **33/33** contract assertions on
a compile-validated shim.

**Rejected on evidence, not argument:** literal .NET `ListDictionaryInternal`
parity; a `const std::any&` key parameter (a public source *and* ABI break on
`IDictionary` and both implementations that collides with the measured
`Add("literal", v)` address-key corruption of #1797 §13.4 while fixing no
versioning defect); scattered null checks at five entry points; split
insert/replace helpers (two places for the version rule to drift apart — the
exact mechanism of the present defect); normalising every key surface to `void*`
(would reintroduce the `const`-laundering #1793 removed, on four more surfaces);
comparing old and new values to skip an equal-value bump; **a shared proxy or
shared upsert abstraction with #1791 or #1796** (this type hands out no alias and
needs no proxy at all); and a **negative consumer fixture**, because nothing in
this design fails at compile time so one could not fail — stated explicitly so it
is not mistaken for an omission.

**Approval #1798 needs, exactly three items, per action** (design §36), none of
which carries over from #1771, #1780, #1783, #1793, #1794 or #1796: (1) a null
key becomes `ArgumentNullException("key")` on five entry points that currently
succeed; (2) a value replacement — including an equal-value one — advances the
counter, turning a currently-silent enumeration into
`InvalidOperationException`, and includes the two deliberate deviations from .NET
in §15; (3) the key view's `CopyTo` boxes `const void*`, so `any_cast<void*>`
keeps compiling and starts throwing at run time — 3 assertion lines in 2 files,
separately declinable. Plus a required acknowledgement: **a full consumer rebuild
is mandatory.**

**Validation (all unchanged, as expected for a design-only ticket):** 41 modules
/ 90 edges, validator tests 7/7, catalogue current, database consistent,
`git diff --check` clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling,
`scripts/local_ci_check.sh build` at **13,657 tests across 37 executables** with
zero warnings and zero errors, `Collections_Core` at **2,371**.
`check_selective_components.sh` **not run** (no public header or component
metadata changed); required when #1798 lands. Build directories: `build/` (reused
incrementally, `--parallel 3`), the **shared** `build-probe/` (one compiler
process per probe, `1799_` file prefix), and `build-tmp/` as `TMPDIR`. **No new
build directory was created and no compilation exceeded three jobs.**

**Three inactive follow-up tickets were opened and deliberately not begun:**
#1800 (`REMED-COLL-VERSION-SEAM-ODR`) for the pre-existing
`CollectionVersionAccess` IFNDR that #1796 reported and no ticket recorded;
#1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`) for the negative-fixture per-site
checker that exists only under the gitignored `build-probe/`; and #1802
(`REMED-COLL-HASHTABLE-REMOVE-VERSION`) for the `Hashtable` absent-key `Remove`
over-bump this ticket measured.

No new `SR-AUD-*` identifier: the audit numbering is frozen at 364 and all six
defects were found during remediation. **The defect is not marked remediated.**
Tickets #1773, #1788, #1789, #1791 and #1798 remain `blocked` and untouched;
#1790, #1792, #1793, #1794, #1795, #1796 and #1797 remain `done` and none was
reopened. CNA and mobile-eggbert were not inspected, searched, configured, built,
or modified. No push, merge, rebase, tag, or publication occurred.

No repair ticket is active.

### Completed ListDictionaryInternal setter remediation: ticket #1798

Implementation ticket #1798 (`REMED-COLL-LISTDICTINTERNAL-PARITY`, P3, size M,
`defect`) is **done**, under the three explicit per-action approvals design
ticket #1799 required. Durable record:
`docs/ListDictionaryInternalSetterDesign.md` §37. #1799 remains `done` and is
not reopened.

**Six measured defects on `System::Collections::ListDictionaryInternal` — the
second production implementer of `IDictionary` — are closed**, and every one was
reproduced again against the committed headers before a line was edited:

1. **`setItem`'s replace branch returned before `++version_`** (version `3 → 3`
   while the stored value changed), so **four** outstanding enumerator kinds
   walked to the end after a replacement with no diagnostic — the dictionary
   enumerator, the key view, the value view, and the same through an
   `IDictionary&` — and the **value view enumerated the post-mutation value**,
   with **0 ASan and 0 UBSan reports**. `setItem` is now one upsert: validate,
   locate, then replace-and-bump or insert-and-bump, with the bump **after** the
   mutation succeeds — a **strong** exception guarantee .NET's bump-first shape
   cannot offer. An **equal-value replacement bumps too**; the value is never
   compared, because equality of a `void*` is address equality and .NET compares
   neither.
2. **All five raw-key entry points accepted `nullptr`** and `setItem` **stored**
   it. A private **`ValidatedKey`** now throws
   `ArgumentNullException("key")` — message
   `Value cannot be null. (Parameter 'key')`, HResult `0x80004003` — and the
   single `findNode()` locator accepts **nothing else**, so validation is
   **structurally unskippable** rather than conventional. That is the whole
   reason the design chose it over a `Hashtable`-style `toKey()` helper, and a
   new negative fixture now **compiles that claim**: 6 of 6 sites rejected.
3. **The key view's `CopyTo` laundered away the caller's `const`** —
   `const_cast<void*>(n.key)` where all four other key surfaces box
   `const void*`, so one view had two incompatible element types and writing
   through the writable pointer the library manufactured for a key the caller
   declared `const` was an **AddressSanitizer SEGV on read-only storage**. The
   `const_cast` is deleted and its superseded rationale comment rewritten.

**Two deliberate deviations from .NET `ListDictionaryInternal`, approved and now
asserted as contract**: a throwing duplicate `Add` and a `Remove` of an absent
key do **not** bump. .NET bumps first and unconditionally on both, which would
manufacture two new false-positive `InvalidOperationException`s out of calls that
changed nothing and would contradict a currently *passing* assertion
(`CollectionVersionCounterTests.cpp`'s `kHasNoOpMutation = true`). **.NET's own
`Hashtable` does neither**, so "match .NET" was never a specification here. The
rule taken is **advance on effective mutation**, which `MutationCounter.hpp`
already documented and which both of this port's `IDictionary` implementations
now follow. `Clear` keeps its unconditional bump, matching .NET.

**Four of the design's own figures are corrected by measurement** (§37.1), each
against this record's convenience:

1. §11 said **0 existing assertions change** for the null-key row. It was
   **one** — and ticket #1796 had put it there deliberately, its comment saying
   "so #1798 has a test to flip". It was **flipped, not deleted**. Corrected
   source break: **4 assertion lines in 3 files**, not 3 in 2.
2. §22's stale-object table **understated the hazard at `-O2`**. Re-run on the
   real headers, `-O2` is **also link-order dependent**: with a stale object
   first on the link line the *rebuilt* translation unit reverts to the
   defective bodies too. The bad link order is dangerous at **both**
   optimisation levels, not only `-O0`. `-flto -Wodr` still diagnoses nothing.
3. §21.1's "7 new symbols" measured as 10 lines, two of them local string
   constants and one `_M_unhook` (from `erase` replacing `remove_if`). The
   substantive claim is unchanged: **none is a `ListDictionaryInternal` symbol**.
4. §24's **+0.2 ns per replace is not resolvable above noise** on the production
   header (1.78–2.01 ns before, 1.82–2.13 ns after, faster in two of three
   runs). Its load-bearing half holds exactly: **0 allocations added**.

**Deviation from design §28, stated so it is not mistaken for scope creep:** §28
proposed **no** negative fixture, correctly, *for the representation change* —
that one fails at run time, not compile time, and is pinned in the permanent
suite and the positive fixture instead. But §28 did not consider the design's
**other** compile-time claim, that validation is structurally unskippable.
`test/consumer/collections_dictionary_setter_negative.cpp` asserts it, because
without it the difference between the selected design and rejected alternative A
is a comment. **CI coverage, exactly:** its per-site checker lives under the
**gitignored** `build-probe/`, so the committed fixture is compiled by **no
tracked CI job** — pre-existing inactive ticket #1801, which applies equally to
the three earlier negative fixtures and which #1798 **neither widens nor
closes**. Both *positive* fixtures are compiled `-Werror` **and run** by
`check_selective_components.sh Collections.Core`.

**Interaction with inactive ticket #1800, recorded and not fixed:** the new
suite adds a **third** `CollectionVersionAccess` specialisation, spelled
**token-for-token** as the two existing `SR1794_SEAM_BODY` ones. Identical
specialisations across translation units are well-formed; the IFNDR is the
**divergence** with `CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY`.
That is pre-existing, is **not introduced, not widened and not fixed** here, and
**#1798 does not claim to close it.**

**No signature, return type, parameter type or data member changed** — measured
on the real headers, not the shim: **53 of 53** mangled names byte-identical,
the **19-entry vtable identical** with `getItem` at 72 and `setItem` at 80,
`this` still in `%rdi` with **no `sret`**, `sizeof` unchanged at **40 / 72 / 24
/ 24**, and `ValidatedKey`/`findNode` emitting **no symbol at all** at `-O2`.
**A full consumer rebuild is nevertheless mandatory**, and README.md says so:
every affected body is `inline` in a header, so a stale object links with **zero
diagnostics** and then **silently keeps the defect** — no crash, unlike #1794's
and #1796's breaks.

**Validation:** a **fresh `cmake --fresh` configuration and clean-first
rebuild** of `build/` — **631 objects, 0 predating the configure**, all 36 test
executables relinked, 0 warnings, 0 errors — then **13,723 tests across 37
executables** from that rebuilt tree (floor was 13,657), `Collections_Core`
**2,437** (was 2,371, **+66**). ASan + UBSan + LSan clean on the entire
`Collections.Core` suite and on both consumer fixtures, with LeakSanitizer
**proved active** by a 350-byte self-test; the §8.3 SEGV is now **unreachable**
because the cast that yielded the writable pointer throws first. The design's own
33-assertion contract probe re-pointed from the shim to the **production** header:
**33/33**. 41 modules / 90 edges, validator 7/7, catalogue current, database
consistent, `git diff --check` clean, Doxygen **1,940** of the 1,942 ceiling,
full `check_selective_components.sh` matrix passing.

**Build directories:** `build/` (fresh configure + clean-first, `--parallel 3`),
the **new** `build-asan/` from the closed CLAUDE.md set (`--parallel 3`,
`ccache`), the **shared** `build-probe/` and `build-consumer/` under a `1798_`
file prefix, and `build-tmp/` as `TMPDIR`. **No build-directory name outside the
closed set was invented and no compilation exceeded three jobs.**
`check_selective_components.sh` needed `TMPDIR` redirected into `build-tmp/` so
its `mktemp -d` matrix root stayed out of `/tmp`; `build-asan/` needed
`-Wno-maybe-uninitialized` for a GCC 14 `-O1` false positive inside
`<bits/std_function.h>` in a Text.RegularExpressions translation unit, unrelated
to this ticket.

**Not claimed closed:** address-based key comparison; `MoveNext`/`Reset` after
the collection is destroyed; a view or enumerator outliving its dictionary; the
silent stale-object hazard; `ValidatedKey` being unskippable within the class
but not across the codebase; the cosmetic duplicate-`Add` message divergence
(§9.5, still not required); and the real blast radius in CNA and mobile-eggbert,
**unmeasured by instruction**.

No new `SR-AUD-*` identifier: the audit numbering is frozen at 364 and all six
defects were found during remediation. `Hashtable` was **not** modified — its
absent-key `Remove` over-bump stays inactive ticket #1802, and closing it is
what would make the two implementations agree on all ten version rows. Tickets
#1800, #1801 and #1802 remain `blocked` and unbegun; #1773, #1788, #1789 and
#1791 remain `blocked` and untouched; #1790 and #1792–#1799 remain `done` and
none was reopened. CNA and mobile-eggbert were not inspected, searched,
configured, built, or modified. No push, merge, rebase, tag, or publication
occurred.

No repair ticket is active.

### Completed Hashtable Remove versioning remediation: ticket #1802

Implementation ticket #1802 (`REMED-COLL-HASHTABLE-REMOVE-VERSION`, P3, size S,
`defect`) is **done**, under the explicit per-action user approval its row
required. Durable record: `docs/HashtableValueAccessSafetyDesign.md` §35. #1796
and #1799 remain `done` and neither is reopened.

All three `System::Collections::Hashtable::Remove` overloads were
`_map.erase(key); ++version_;`, so the fail-fast mutation counter advanced
**whether or not the key was present**. Reproduced against the committed headers
before any edit: **24 defects over 43 checks**, **0 over the same 43** after.
Removing an absent key moved the counter `3 → 4` and threw
`InvalidOperationException` out of **four** enumerator kinds — the
`IDictionaryEnumerator`, the key view, the value view, and the same through an
`IDictionary&`; a full walk after one absent `Remove` yielded **0 of 3** entries
and `Reset()` threw too. `Count` and contents were correct on every row, so this
is a **false positive** — the exact opposite direction of error from #1798's,
which missed a mutation that really happened.

All three overloads now route through one private
`removeKey(const std::string&)` helper that is
`if (_map.erase(key) != 0) ++version_;`.
`std::unordered_map::erase(const key_type&)` already returns the number of
elements removed, so the correction adds **no second lookup, no `Contains`
pre-check, no second key conversion, no allocation and no lock**; the bump
follows the erase, giving a strong exception guarantee. This matches .NET
`Hashtable.Remove`, which calls `UpdateVersion()` only inside the branch that
found and cleared a bucket (`Hashtable.cs:999`), and completes the
"advance on effective mutation" rule
`docs/ListDictionaryInternalSetterDesign.md` §9.3 selected for the interface.
**With #1798 and #1802 both closed the port's two `IDictionary` implementations
agree on all ten version rows of that design's §6.1.**

`Clear()` was deliberately **not** changed and its deviation is now decided
rather than implicit: it bumps unconditionally, including on an already-empty
table, where .NET `Hashtable.Clear` early-returns under a
`_count == 0 && _occupancy == 0` guard whose `_occupancy` half has no
`std::unordered_map` analogue. The unconditional bump also errs in the
memory-safe direction and matches .NET `ListDictionaryInternal.Clear` and the
port's own sibling. It is asserted on both implementations in the permanent
suite and in the consumer fixture.

**Not an ABI break, but a silent stale-object hazard.** `sizeof(Hashtable)`
unchanged at 72, the 19-entry vtable byte-identical with `Remove` still at slot
`0x70`, `this` still in `%rdi` with no `sret`, undefined-symbol list identical.
Every affected body is `inline` in a header, so a stale object links with zero
diagnostics and silently keeps the old false positive — link-order dependent at
`-O0`, per-translation-unit at `-O2`, with `-flto -Wodr` diagnosing nothing.
**A full consumer rebuild is mandatory**, and `README.md` says so.

**Validation:** fresh `cmake --fresh` configuration and clean-first rebuild of
`build/` (**632 objects, 0 predating the configure**, 36 executables relinked, 0
warnings, 0 errors), then **13,790 tests across 37 executables** from that tree
(floor was 13,723), `Collections_Core` **2,504** (was 2,437, **+67**). A later comment-only edit (two doc-comments changed from `§9.3` to `section 9.3` so the two headers stay pure ASCII) triggered one incremental build that recompiled **11 translation units and relinked 1 executable** — the **complete** dependent set, since exactly eleven `.d` files in the tree name `Hashtable.hpp` or `IDictionary.hpp` — leaving **0** objects predating the fresh configuration; the gate below ran from that tree. ASan +
UBSan + LSan clean on the whole `Collections.Core` suite, a focused scenario
probe and both consumer fixtures, with LeakSanitizer proved active by a 350-byte
self-test reported as 383 bytes in 2 allocations. 41 modules / 90 edges,
validator 7/7, catalogue current, database consistent, `git diff --check` clean,
Doxygen **1,940** of the 1,942 ceiling, full `check_selective_components.sh`
matrix passing. Allocation counts identical on every `Remove` path; the one
measured slowdown is on the throwing null-key path and is proved by disassembly
and by a bare-throw control to be code-layout, not added work.

No new `SR-AUD-*` identifier: the numbering is frozen at 364 and the defect was
found during remediation, by #1799's probe. `ListDictionaryInternal` was **not**
modified. #1800 and #1801 remain `blocked` and unbegun — the new suite adds a
fourth `CollectionVersionAccess` specialisation spelled token-for-token as the
three existing `SR1794_SEAM_BODY` ones, so the count of *divergent* bodies is
still two, and no negative fixture was added because nothing here changes at
compile time. #1773, #1788, #1789 and #1791 remain `blocked` and untouched;
#1790 and #1792–#1799 remain `done`. CNA and mobile-eggbert were not inspected,
searched, configured, built, or modified. No push, merge, rebase, tag, or
publication occurred.

No repair ticket is active.

### Completed test-seam ODR remediation: ticket #1800

Ticket #1800 (`REMED-COLL-VERSION-SEAM-ODR`, P3, size S, `defect`) is **done**.
Durable record: `docs/CollectionVersionTestSeamDesign.md`. **No production
source, signature, symbol or object layout changed** — no file under any
`modules/*/include` was touched — so nothing in this section is a consumer
concern.

Five translation units of the one `SharpRuntimeTests_Collections_Core` program
each defined `SharpRuntime::Testing::CollectionVersionAccess` themselves, in two
divergent families (`SR1787_SEAM_BODY` with `positionVersion`, `SR1794_SEAM_BODY`
without), so three specialisations had two token-different definitions in one
program — ill-formed, **no diagnostic required**.

- **Three divergences, not the two the row named.** The **partial**
  specialisation `<detail::BasicMutationCounter<V>>` diverged as well (`read` +
  `write` against `read` alone) and is the one both collection-level bodies
  delegate to. Measured by preprocessing each unit with the build's own flags and
  hashing token sequences, not by grep.
- **The consequence was measured, not assumed.** At `-O0`, which this repository
  builds, swapping two object files on the link line changed the answer a unit
  that had spelled the correct body itself received; at `-O1` and `-O2` the two
  units disagree inside one process. `ld`, `-flto -Wodr`, ASan with
  `detect_odr_violation=2`, and UBSan each reported nothing.
- **Repair:** one authoritative header,
  `modules/collections/tests/support/CollectionVersionSeam.hpp`, holding the
  counter-level seam and all fifteen collections behind one macro; the five
  suites include it. The **richer** body became canonical, so #1787's
  near-boundary matrix keeps every capability it had.
- **Permanent guard:** `scripts/check_version_seam_odr.py` (four rules; it
  *discovers* seams rather than hard-coding them, and covers #1786's
  `SortedSetVersionAccess` too) plus `test/check_version_seam_odr_test.py` with
  12 fixtures, both wired into `scripts/local_ci_check.sh`. It exits 1 against
  the committed pre-fix sources and against an injected hypothetical suite, and 0
  against the repository. A second body inside a unit that includes the header is
  already a hard compile error, so the checker only has to cover the unit that
  does not include it.
- **One cost, reported:** four suites gained thirteen header includes, +0.38 to
  +0.42 s of front-end time each (+31 %), +1.6 s against a 336 s clean-first
  rebuild. Splitting the header would recover it; it was not done, because
  deciding which of two headers a new collection belongs in is the decision that
  produced two bodies in the first place.

Validated from a fresh configure and clean-first rebuild at three jobs: 13,790
across 37, `Collections.Core` 2,504, zero warnings and errors, 632 objects and
37 executables all post-marker, every seam COMDAT byte-identical, the post-fix
link-order probe agreeing at `-O0`/`-O1`/`-O2` in both orders, ASan/UBSan/LSan
2,504 with no diagnostic, the full selective matrix plus an explicit isolated
`Collections.Core` selective build, 41 modules / 90 edges, validator 7/7, seam
checker 12/12, catalogue current, database consistent, `git diff --check` clean,
Doxygen 1.9.8 at 1,940 of the 1,942 ceiling. TSan is not relevant and was not
run: no thread, no shared mutable state and no atomic is introduced.

No new `SR-AUD-*` identifier: the numbering is frozen at 364 and the defect was
found during remediation, by #1796. **#1801 remains `blocked` and is not closed**
— it asks for a tracked per-site checker for the six negative consumer fixtures,
and #1800's checker compiles nothing and shares none of that infrastructure.
#1773, #1788, #1789 and #1791 remain `blocked` and untouched; #1790, #1792–#1799
and #1802 remain `done` and none was reopened. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred. (**#1801 closed the same day, immediately after
#1800**, and the verified count was **seven** fixtures rather than six — see the
next section; this paragraph is left as #1800's own accurate record.)

### Completed negative-fixture CI remediation: ticket #1801

Ticket #1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`, P3, size S, `tooling`, area
*Developer experience*) is **done**. Durable record:
[`docs/NegativeConsumerFixtureValidation.md`](docs/NegativeConsumerFixtureValidation.md).
It is infrastructure only: **no production source, signature, symbol, layout,
vtable, exception contract or collection semantic changed**, and nothing under
`modules/*/include` or `modules/*/src` was touched.

Seven committed `test/consumer/*_negative.cpp` files existed to prove that
outlawed spellings are rejected by the compiler, and **no tracked job compiled
any of them**; per-site logic existed for two of the seven, under the gitignored
`build-probe/`. The independently verified inventory is **7 fixtures / 37
sites**, not the six the ticket row named — the row predates
`collections_dictionary_setter_negative.cpp` (#1798) — and three fixtures had no
per-site checker at all, while
`collections_object_model_readonlydictionary_negative.cpp` named a
`scripts/check_readonlydict_empty_negative.sh` that has never existed in any
commit.

The false pass was reproduced before anything was built: a temporary copy of the
Hashtable fixture with one marked site made legal still failed at nine other
lines, so a whole-file check reported PASS while one of eleven claims had become
false; the retained gitignored checker caught it at 10/11 and nothing tracked
did.

The selected convention is a numbered preprocessor guard per site
(`#if SHARP_RUNTIME_NEGATIVE_SITE == N`) with an inline `// NEGATIVE(<id>):
<fragment>` marker and `//     | <alternative>` continuations, chosen over
runner-generated variants, an external manifest, one file per expression, and
Clang `-verify` comments. The tracked file is compiled as-is with a `-D`, so
nothing is generated; the all-sites-off baseline must compile with **zero
diagnostics**, which is both the soundness argument for per-site attribution and
the reason no CMake change was needed.

`scripts/check_negative_consumer_fixtures.py` compiles 44 translation units
(7 baselines + 37 sites) with `-std=c++23 -Wall -Wextra -Wpedantic -Werror
-fsyntax-only`, include directories derived from the repository's own CMake
component metadata, `LC_ALL=C` for deterministic diagnostic wording, and a hard
three-job ceiling that refuses a higher request. It runs from
`scripts/local_ci_check.sh` before the configure step.
`test/check_negative_consumer_fixtures_test.py` is 37 cases in 2.1 s, including
the permanent regression proof on a real tracked fixture, and
`build-probe/1801_mutation_campaign.py` is 7/7 with exactly one problem reported
per mutated fixture.

Validated from a fresh configure and clean-first rebuild at three jobs: **7
fixtures / 37 sites / 37 rejected** in 12.5 s at peak 3 jobs, checker fixtures
37/37, mutation campaign 7/7, zero warnings and errors (346 s, 632 objects none
predating the configure), **13,790 tests across 37 executables**,
`Collections.Core` **2,504**, the full ten-component selective matrix with its
three forbidden fixtures still rejected, 41 modules / 90 edges, validator 7/7,
seam checker 12/12, catalogue current, database consistent, `git diff --check`
clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling. Sanitizers are not
applicable to a Python checker and compile-only validation, and none was built.

No new `SR-AUD-*` identifier. One residual gap is recorded rather than absorbed:
`SortedSetVersionAccess` has no consumer-side fixture, which is new inactive
ticket **#1803** (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`, `blocked`,
not begun). #1773, #1788, #1789 and #1791 remain `blocked` and untouched; #1790,
#1792–#1800 and #1802 remain `done` and none was reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
push, merge, rebase, tag, or publication occurred.

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

## Completed tracked List indexer mutation: ticket #1791

Ticket #1791 (`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, size L, `defect`,
area `Collections`) implemented the architecture design ticket #1790 selected,
under the **exact four-part approval written verbatim in
`docs/ListIndexerVersioningDesign.md` §28**, granted by the user and scoped to
#1791 only. The implementation record is §§29-39 of that document, appended below
#1790's, which is preserved unedited. No new `SR-AUD-*` identifier was issued.

Real .NET's `List<T>` index setter advances `_version` unconditionally, so
`list[i] = value` fails an in-progress enumeration fast; this port's `operator[]`
returned a plain `T&`, which no C++ mechanism can intercept, so an indexed write
was invisible to the fail-fast guard and a retained reference was a reproduced
use-after-free. The non-const indexer of `IList<T>`, `List<T>`,
`ObjectModel::Collection<T>` and `ObjectModel::ReadOnlyCollection<T>` now returns
`System::Collections::detail::ElementReference<T>`, a 16-byte prvalue proxy that
reads as `const T&` and routes every write through the mutation counter;
`getItem`/`setItem` were added as pure virtuals on `IList<T>` and implemented by
all four implementers. `list[i] = v` still compiles and now invalidates.

Three corrections to the design are recorded in §30.3: the mutable
`List<T>::ToVector()` was **removed with no public replacement** rather than
merely re-documented, because the approval forbids any ordinary public API
returning a mutable `std::vector<T>&`; `begin()`/`end()` were **kept** as the
documented STL-interop residual, so **the ticket claims the last *ordinary*
untracked write path is closed, not the last one**; and a constrained forwarding
`operator=(U&&)` was added after measuring one heap allocation per
`stringList[i] = "literal"` write with only the `T`-typed overloads.

Measured: `sizeof(List<T>)` **40 → 40**, `sizeof(Collection<T>)` **32 → 40**,
`sizeof(ReadOnlyCollection<T>)` **24 → 24**, `IList<T>` vtable **14 → 16** slots,
4 symbols removed and 18 added, source break **1 site in 1 of 631 translation
units** (the hand-written implementer, as #1790 predicted against 625), all 61
measured indexer call sites still compiling. Runtime cost is at the measurement
noise floor and nothing allocates; the real cost is that reference-based in-place
member access is gone and copy-modify-set copies the element twice.

Three residual hazards are stated rather than concealed: `begin()`/`end()` still
yield an untracked mutable `T&`; a *retained* proxy still aliases a slot across
reallocation; and **a stale object file links with no diagnostic, does not crash,
reads correct values, and silently loses mutation tracking**, measured at `-O0`
and `-O2` in both link orders — which is why the full consumer rebuild is
mandatory. `Collection<T>` also gained a fail-fast enumerator, having
version-checked nothing before, not even `Add()`.

Validation from a fresh configure plus clean-first rebuild at **three jobs** (633
objects, 0 predating the marker, 37 of 38 executables relinked, 0 warnings, 0
errors): `Collections.Core` **2,554**, full repository **13,840 across 37
executables**, negative consumer fixtures **8 / 51** all rejected plus 37/37
self-test, version-seam ODR **2 seams / 18 specialisations** plus 12/12
self-test, module graph **41 / 90**, Doxygen **1,940** of the 1,942 ceiling, the
full ten-component selective matrix and the new positive fixture passed,
ASan/UBSan/LSan `Collections.Core` 2,554 with zero reports and LSan proved active,
`git diff --check` clean, local CI gate passed. TSan was not run, for the reason
design §19 gave.

Tickets #1773, #1788, #1789 and #1803 remain `blocked` and untouched; **no shared
List/Hashtable proxy was introduced**, so #1797 §24's four measured
incompatibilities stand. #1790 and #1792–#1802 remain `done` and none was
reopened. CNA and mobile-eggbert were not inspected, searched, configured, built,
or modified, so the source-break figures here are *this repository only*. No
push, merge, rebase, tag, or publication occurred.

### Completed LinkedList mutation-counter widening: ticket #1788

Ticket **#1788** (`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, size S, `defect`,
area `Collections`) widened `System::Collections::Generic::LinkedList<T>`'s
private mutation counter — and, in the same change, its `Enumerator`'s snapshot —
from 32 to 64 bits. It was opened `blocked` by ticket #1787 and began only after
the user granted the exact approval `docs/CollectionVersionCounterSweep.md` §8.1
had asked for. **No new `SR-AUD-*` identifier**; the numbering stays frozen at
364. The implementation record is **§19** of that document, appended below
#1787's, which is preserved unedited.

**The defect, reproduced before anything changed.** #1787 removed the
signed-overflow undefined behaviour from every counter but could not widen this
one without growing a public object, so it left a 2^32 enumerator-snapshot ABA:
after 2^32 effective mutations the counter returned to a value an outstanding
`Enumerator` had captured and the equality guard silently accepted it. Because
that enumerator holds a raw `data_t*` into `shared_ptr`-owned node storage, the
consequence was a potential **use-after-free**, not merely a wrong answer, and at
~10^8 mutations/second the horizon was about **43 seconds** of hot mutation.
`build-probe/1788_prefix_defects.log` reads `guard-fired=0` three times
(`LinkedList<int>`, `LinkedList<std::string>`, and `Reset()`),
`defects-observed=3`; the identical source post-fix reads `guard-fired=1` and
`defects-observed=0`. UBSan reported **0** runtime errors on **both** sides,
confirming #1787 had already closed the UB and that this ticket closed only the
remaining logical horizon.

**Measured, not assumed.** `sizeof(LinkedList<T>)` **40 → 48** for every `T`,
`alignof` unchanged at 8, `head_`/`tail_`/`count_` offsets unchanged;
`sizeof(Enumerator)` **unchanged at 40**, because its wider snapshot landed in
padding it already had, with every other enumerator member keeping its offset;
`sizeof(LinkedListNode<T>)`, both iterators and `LinkedListNodeData<T>`
unchanged; and **0 `LinkedList` symbols added, removed or renamed** — 796 on both
sides with byte-identical name lists. The only symbol delta anywhere is five
weak inline members of the counter class swapping from the `unsigned int`
instantiation to the `unsigned long` one.

**The break is binary-only and silent, which was reproduced rather than
asserted.** No public signature, return type, parameter or `const` qualification
changed, and every in-repository call site compiles unmodified. But an object
file compiled against the old header links with a new one **with zero
diagnostics in both link orders** — and then, depending on which won the COMDAT
race, either takes an AddressSanitizer heap-buffer-overflow and a SEGV, or
silently corrupts the member following an embedded `LinkedList<T>` **with no
sanitizer report at all**, or silently loses mutation invalidation
(`guard-fired=0`); in the other order everything appears to work. A complete
consumer rebuild is therefore mandatory, and `README.md` says the linker will not
warn you.

**A weakness in #1787's own pin was found and fixed, not concealed.** Flipping
`LinkedListAdapter::kNarrowCounter` back to `true` as a mutation check failed
only *one* test. #1787's narrow branch positioned the counter at
`static_cast<Value>(snapshot)` rather than at `snapshot + 2^32` — identical for a
32-bit field, but true for a counter of any width, so it pinned nothing about the
residual its comment described. It now spells the full distance and asserts the
truncation itself, which makes it load-bearing for `BitArray` and for #1789.

Validation from a fresh configure plus a clean-first rebuild at **three jobs**
(634 objects — 630 C++, 4 C — **0 predating the fresh-configure marker**, 37 of
38 executables relinked, 0 warnings, 0 errors; the exception is the
`EXCLUDE_FROM_ALL` `build/SharpRuntimeTests`, an 85 MB historical binary outside
the gate that is now definitively stale): `Collections.Core` **2,594** (was
2,554), full repository **13,880 across 37 executables** (was 13,840), negative
consumer fixtures **8 / 51** all rejected plus 37/37 self-test (none added),
version-seam ODR **2 seams / 18 specialisations** plus 12/12 self-test (none
added — the new suite includes the one authoritative seam file), module graph
**41 / 90** unchanged, Doxygen **1,941** of the 1,942 ceiling with the single new
warning attributed to the one new `README.md` link into `docs/`, the full
ten-component selective matrix plus the new `Collections.Core` fixture passed,
ASan/UBSan/LSan `Collections.Core` **2,594 with zero reports** and LSan proved
active by a bounded self-test (336 bytes in 7 allocations, exit 1), a 200,000-node
teardown at the boundary clean under ASan, `git diff --check` clean, local CI gate
passed. **TSan was not run**: no atomic, no `mutable` cache, no hidden `const`
write, and `LinkedList<T>` claims no thread safety before or after.

Tickets #1773, #1789 and #1803 remain `blocked` and untouched — in particular
`BitArray` keeps its 2^32 residual, deliberately, because closing it grows the
**public** `BitArray::Enumerator` and that is #1789's separate approval. #1790,
#1791 and #1792–#1802 remain `done` and none was reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built or modified, and
no claim is made about whether they use `LinkedList<T>`. No push, merge, rebase,
tag, or publication occurred.

### Completed BitArray mutation-counter widening: ticket #1789

Ticket #1789 (`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, size XS, `defect`, area
`Collections`) closed the **second and last** of the two residuals ticket #1787
had to leave open, after the user granted the **exact object-size approval**
[`docs/CollectionVersionCounterSweep.md`](docs/CollectionVersionCounterSweep.md)
§8.2 asked for, scoped to #1789 only. The implementation record is **§20** of that
document; §§1–19 are #1787's and #1788's and are preserved unedited, so the record
does not pretend `BitArray` was always 64-bit. **No new `SR-AUD-*` identifier**;
the numbering stays frozen at 364.

`BitArray::version_` moved from the 32-bit `detail::NarrowMutationCounter` to the
64-bit `detail::MutationCounter`, and `BitArray::Enumerator::version_` from
`NarrowMutationVersion` to `MutationVersion` **in the same change**. Both
together, deliberately: widening the container alone would turn the guard's
comparison into a silent truncation and leave the 2^32 alias in place while the
code claimed otherwise — the failure mode §8.2 identified and refused. Nine
increment sites (`Set`, `SetAll`, `Not`, `And`, `Or`, `Xor`, `LeftShift`,
`RightShift`, `setLengthProperty`) plus the implicitly declared copy/move
assignment, and three read/compare sites, are all unchanged in spelling; the
production diff is two field declarations plus documentation. `BitArray` has no
`Clear()`, no `Add`, and a `const`-only `operator[]`, so `Set` is its sole indexed
write path.

**The defect was reproduced before any production change.** Pre-fix,
`build-probe/1789_prefix_defects.log` shows `truncated-onto-snapshot=1` and
`guard-fired=0` for `MoveNext`, for `Reset()`, and at seven laps of 2^32 —
`defects-observed=3`. The identical source post-fix reads `guard-fired=1` and
`defects-observed=0`, and the entire diff of the two logs is the counter width,
those three outcomes, and one sentinel probe reaching a larger maximum; **every
mutation-delta line and every ordinary-invalidation line is byte-identical**.
UBSan reported **0** runtime errors on both sides: `BitArray` is the one
collection whose counter was already unsigned before #1787 (`std::uint32_t`,
diverging from .NET's signed `int` at `BitArray.cs:44`), so it never had the
signed-overflow UB and this ticket closed only the remaining *logical* ABA
horizon. Unlike `LinkedList<T>`'s, the consequence was a **wrong answer rather
than a use-after-free** — the enumerator holds an index bounds-checked against the
current length on every step — which is why this was P3. At ~10^8
mutations/second 2^32 is about **43 seconds**.

Measured, not estimated: `sizeof(BitArray::Enumerator)` **32 → 40**, `alignof`
unchanged at 8, `arr_` keeping offset 8 while the snapshot at 16 widens and
`index_`/`current_`/`state_` each move by 8 — nine bytes are needed after an
eight-byte snapshot where eight are available, in any member order, exactly as
§8.2 predicted; `sizeof(BitArray)` **unchanged at 48**, because the wider counter
landed in the four bytes of tail padding the container already had, so
`PublishedObjectSizesAreUnchanged` still asserts 48 and is still telling the
truth; **0 `BitArray` symbols added, removed or renamed** (64 on each side,
byte-identical name lists), the only symbol delta anywhere being the counter
class's seven weak inline members swapping from the `<unsigned int>` to the
`<unsigned long>` instantiation. No public signature changed and every
in-repository caller compiles unmodified.

**The break is binary-only and silent, and that was measured.**
`BitArray::Enumerator` is a **public** nested class, so a consumer may name one
and store it by value. An object file compiled against the old header links with
a new one producing **no diagnostic in any of eight configurations** (`-O0`/`-O2`
× both link orders × with and without ASan+UBSan). Then, depending on which
definition won the COMDAT race, it either silently corrupts the member following
an embedded enumerator — a sentinel went from `0xFEEDFACECAFEBEED` to
`0xFEEDFACE00000002`, with **no AddressSanitizer report at all**, because the
bytes are inside the same allocation — or, at `-O2`, silently reports **zero
elements for an eight-bit array**, or aborts on a `new-delete-type-mismatch`
("allocated 32 bytes, deallocated 40") under ASan. At `-O0` one of the two link
orders looks entirely healthy. Notably the **fail-fast guard keeps firing in every
configuration**, so a consumer cannot use that as evidence it rebuilt. A complete
consumer rebuild is mandatory and `README.md` now says so in those terms.

**The adapter flip was mutation-checked.** Putting
`BitArrayAdapter::kNarrowCounter` back to `true` and rebuilding fails **two**
tests — `TheCounterHasTheWidthItsLayoutPermits` *and*
`NoStaleSnapshotBecomesValidAcrossTheOld2Pow32Distance`. Only the first would have
failed before #1788 corrected that assertion to spell the full `snapshot + 2^32`
distance (§19.11), so that correction is what made this flip load-bearing rather
than cosmetic.

**One performance figure is disclosed rather than waved through.** The
`RightShift(1)` benchmark row moved +88 ns/op (~8%) and reproduced across fourteen
paired runs — two non-overlapping ranges, so not noise. It is **not** the counter:
`BitArray::RightShift`'s generated code is instruction-for-instruction identical
on both sides (130 lines of `objdump` output each), the only codegen difference
anywhere being 32- to 64-bit `mov`s inside `BasicMutationCounter::operator++`; and
recompiling **both** sides with `-falign-loops=32 -falign-functions=64` inverts
the sign, making the post side 197 ns *faster*. It is `-O2` code alignment. Every
other row straddles zero and **allocation counts are identical in every row**.

New permanent suite `BitArrayVersionWideningTests.cpp` (**+43** cases, all
boundary positioning through #1800's one authoritative seam, which already carried
a `BitArray` specialisation, so no new specialisation body was written) and a new
tracked consumer fixture `test/consumer/collections_bitarray_version.cpp`,
compiled with `-Wall -Wextra -Wpedantic -Werror` and run. All 21
`BitArrayTests.cpp` cases and the `Batch18`/`Batch18b` gap-fills pass
**unmodified**.

Validation from `cmake --fresh` plus a clean-first rebuild at **three jobs** (635
objects, **0** predating the fresh-configure marker, 37 of 38 executables
relinked, 0 warnings, 0 errors — the exception being the `EXCLUDE_FROM_ALL`
`build/SharpRuntimeTests`, an 85 MB historical binary from 2026-07-24 outside the
gate, left untouched and still stale): `Collections.Core` **2,637** (was 2,594);
full repository **13,923 across 37 executables** (was 13,880); negative consumer
fixtures **8 / 51, every site rejected** plus 37/37 self-test, none added and none
needed since no public signature changed; version-seam ODR **2 seams / 18
specialisations** plus 12/12 self-test, none added; module graph **41 / 90**
unchanged; Doxygen **1,941** of the 1,942 ceiling, **unchanged** — the new
`README.md` entry deliberately refers to the sweep document as a code span rather
than a markdown link, because every `README.md` → `docs/` link costs one
unresolvable `\ref` warning; the full ten-component selective matrix and the new
`Collections.Core` fixture passed; ASan/UBSan/LSan `Collections.Core` **2,637 with
zero reports**, LSan proved active by a bounded self-test reporting 96 bytes in 3
allocations, and a 200,000-bit boundary-positioned walk clean; `git diff --check`
clean; the local CI gate passed. **TSan was not run** — no atomic, no `mutable`
cache, no hidden `const` write, and no thread-safety claim is made for `BitArray`
before or after; `getIsSynchronizedProperty()` still returns `false`.

**With this ticket, no collection in this repository retains a 2^32
enumerator-snapshot ABA horizon** — every one is 2^64, and
`detail::NarrowMutationCounter` has no user left (it is kept as the historical
record and as the second instantiation the counter tests pin).

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, no claim is made about whether they use `BitArray`, and **#1773
remains `blocked`**. #1803 remains `blocked` and untouched.

### Completed SortedSet seam consumer guard: ticket #1803

Ticket #1803 (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`, P3, size XS,
`tooling`, area *Developer experience*) is **done**. Durable record:
[`docs/NegativeConsumerFixtureValidation.md`](docs/NegativeConsumerFixtureValidation.md)
**§18**, appended below #1801's §§1-17, which are preserved unedited — in
particular §16.4 item 4, which is this ticket's own charge sheet and still reads
"`SortedSetVersionAccess` has no consumer-side fixture". **No new `SR-AUD-*`
identifier**; the numbering stays frozen at 364.

**This is test/tooling work only.** No production source, signature, symbol,
object layout, vtable, exception contract or collection semantic changed; not one
file under any `modules/*/include` or `modules/*/src` was touched, and CMake
metadata, the component graph and every include directory are unchanged. #1800
and #1801 stay `done` and neither was reopened; `scripts/check_version_seam_odr.py`,
`scripts/check_negative_consumer_fixtures.py`,
`modules/collections/tests/support/CollectionVersionSeam.hpp` and
`test/consumer/CMakeLists.txt` are unmodified.

**What was missing.** `SharpRuntime::Testing::SortedSetVersionAccess<T>` is
declared, never defined, and befriended by `SortedSet<T>`, so #1786's regressions
can position the shared 64-bit counter and read #1784's atomic Count cache. #1800
pinned that it has exactly one definition site — a test translation unit,
`SortedSetVersionOverflowTests.cpp` — by reading source text. What no tracked job
did was **compile a consumer that tries to use it**. `CollectionVersionAccess` had
that half, from `test/consumer/collections_mutation_version_negative.cpp`;
`SortedSetVersionAccess` did not. Nothing was broken: this was a coverage
asymmetry, and it stayed one.

**The intended restriction, now written down and pinned:** an ordinary consumer —
compiling against a component's declared public include surface, with no flag that
disables access control, including nothing under `modules/*/tests`, and not
authoring a specialisation in a namespace it does not own — can neither name a
*complete* `SortedSetVersionAccess<T>` nor reach, by any other route, the state
that seam exists to reach.

**Inventory first, fixture second.** `build-probe/1803_threat_probe.py` compiled
**29** candidate consumer expressions, one per translation unit, against the
resolved `Collections.Core` surface. Three are accepted and 26 rejected. Two of
the three acceptances are intended and are deliberately **not** sites — naming the
incomplete type, and declaring a pointer to it, obtain nothing, which is what a
forward declaration is for. The third is §18.5 below.

**`test/consumer/collections_sorted_set_version_negative.cpp`, 15 sites**, not the
two the ticket's row estimated: five prove the seam is an **incomplete type**
(`version`, `positionVersion`, `cachedTag`, an object definition, the `::Set`
member type), nine prove there is no second route to the same storage (`state_`,
the nested `State`, `bumpVersion()`, `cachedCount_`, `cachedCountVersion_`,
`countCacheTag()`, `kMaxCacheableVersion`, `kCountNotCached`, and
`Iterator::version_` — each `is private within this context`), and one proves the
defining translation unit is not reachable through any public include path. A
sibling fixture rather than two more sites in #1787's file, so `--list` output,
blame and diagnostics stay attributable to one ticket each. **One correction to
the row itself**: its acceptance criteria asks for
`SortedSetVersionAccess<SortedSet<int>>`; the seam is parameterised by the
**element** type, so the contract spelling is `SortedSetVersionAccess<int>`. The
row's spelling is also rejected and was measured, but pinning it would have pinned
a misspelling.

**Two measurements are disclosed rather than filed under noise.**

1. **#1800's checker exits 0 on a real seam exposure.** Both checkers were run
   against identical mirror repositories (`build-probe/1803_gap_probe.py`). Give
   the seam's **primary template** a body in `SortedSet.hpp` and
   `check_version_seam_odr.py` says `OK`, silently reporting 1 seam and 17
   definitions instead of 2 and 18 — because its discovery rule is "declared and
   **not defined**", so a defined seam stops being a seam and rule 1 never fires;
   its vacuity guard fires only at **zero** seams. Making `SortedSet<T>::state_`
   public likewise exits 0. #1803's fixture fails on both, naming five sites and
   one site respectively. Nothing is wrong in the repository today and #1800 is
   **not reopened** — the two checks are complementary by design, which is the
   measurement rather than the argument. Strengthening the vacuity guard is
   inactive ticket **#1804** (`REMED-TOOLING-SEAM-DISCOVERY-VACUITY`, `blocked`).
2. **One restriction cannot be expressed, and it is not SortedSet's.** A consumer
   that reopens `namespace SharpRuntime::Testing` and writes its own explicit
   specialisation *does* obtain the access the friend declaration grants, and
   compiles clean under `-Werror` against the public headers alone — for
   `SortedSetVersionAccess<int>` and, measured identically, for
   `CollectionVersionAccess<List<int>>`. That is well-formed ISO C++; a
   `friend class X;` is open to whoever writes `X`, and no seam design in C++
   avoids it. It is unsupported and is recorded in §18.5 instead of being assumed
   away. `SortedSet.hpp`'s own doc-comment — nothing a consumer *links against*
   can observe or call the seam — remains literally true.

**Load-bearing, proved.** `build-probe/1803_mutation_campaign.py` applies **ten**
temporary header mutations, each shadowed in a mirror root whose modules are
per-file symlinks, and requires the tracked checker to fail naming **exactly** the
expected site set. **10/10 caught, 0 failures**, covering all fifteen sites; the
unmutated mirror exits 0. No tracked file was modified at any point. Two earlier
campaign runs failed because the mutation itself broke the header, and the checker
correctly reported a **baseline** failure rather than a site result — rule 7 doing
its job on the campaign is the best available evidence that "the build broke"
cannot be mistaken for "the seam was exposed".

Validation, incremental by CLAUDE.md rule 12 (no `modules/` file, no
`CMakeLists.txt` and no component metadata changed, so no object in `build/` can
be stale and a clean-first rebuild would have written a full tree of objects to
re-derive an unchanged answer): `scripts/local_ci_check.sh build` **passed**, 0
warnings, 0 errors, and it **executes the new fixture automatically** — its
negative-fixture phase now reads *9 fixtures, 66 sites, every site rejected*, 75
compiler invocations, peak 3 jobs. Full repository **13,923 tests across 37
executables**, unchanged; `Collections.Core` **2,637**, unchanged; negative
consumer fixtures **9 / 66, every site rejected** (was 8 / 51) plus **37/37**
self-test, unchanged because the checker itself was not modified; version-seam ODR
**2 seams / 18 specialisations** plus **12/12** self-test; module graph **41 /
90**; component catalogue current; database consistency clean; the full
ten-component selective matrix passed with its 3 forbidden fixtures rejected;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Sanitizers are not applicable and none was built** — the deliverable is one
compile-only fixture and documentation, with no production code, no new runtime
code, no new allocation, no new thread and no new CTest case; ASan, UBSan, LSan
and TSan cannot observe a compile-rejection contract, and #1784's and #1786's
existing `SortedSet` sanitizer coverage stands unre-measured. **No compilation
exceeded three jobs**, in any script, including inside the tracked checker, which
refuses a higher request. Build directories used: `build/` (gate), `build-probe/`
(this ticket's probes, mirrors and logs, all `1803_` prefixed), `build-tmp/`
(repository-local `TMPDIR`); **no new build directory was created**. The stale
`EXCLUDE_FROM_ALL` `build/SharpRuntimeTests` binary was neither executed, trusted,
nor deleted.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1788, #1789, #1790, #1791 and
#1792–#1802 all remain `done` and none was reopened. The one new inactive row is
**#1804**.

### Completed MemoryStream raw-buffer validation: ticket #1805

Ticket #1805 (`REMED-IO-MEMORYSTREAM-NULL-BUFFER`, P1, size S, `remediation`,
area *IO*) is **done** and **SR-AUD-341 is now `remediated`**. It is the first
ticket of the post-audit remediation phase to leave `Collections`: NEXT.md's
recommended dependency order names it, with SR-AUD-338, as one of the two
self-contained ASan/UBSan-backed public-input repairs to take after the
collection safety contracts were settled. **No new `SR-AUD-*` identifier** — the
audit numbering stays frozen at 364; the index now records **11 remediated** and
**353 confirmed** of 364.

**Selection note.** The ticket queue in `plan.sqlite3` was **empty** at the start
of this ticket: every row was `done` except #1773 and #1804, both `blocked` and
both correctly so. The remediation backlog is not empty, though — it lives in
`audit/AUDIT_FINDINGS_INDEX.md`, and converting the next roadmap item into a
ticket is how every remediation ticket since #1767 has begun. #1805 was created
that way, not discovered.

**What was wrong.** `MemoryStream(const bytecs* buffer, intcs size, bool writable)`
initialized `data_(buffer, buffer + size)` in its member-initializer list, so the
copy ran ahead of every check. Reproduced before any production change, one
process per input so a crash in one case could not hide another
(`build-probe/1805_prefix_defects.cpp`, log `build-probe/1805_prefix_defects.log`):

| Input | Pre-fix | Post-fix |
|---|---|---|
| `(nullptr, 1)` | UBSan *load of null pointer of type `const unsigned char`*, then **ASan SEGV on address 0x0**, exit 1 | `ArgumentNullException` — *Value cannot be null. (Parameter 'buffer')* |
| `(nullptr, 0)` | constructed, `length=0` | **byte-identical** |
| `(data, -1)` | `std::length_error` — *cannot create std::vector larger than max_size()* | `ArgumentOutOfRangeException` — *Non-negative number required. (Parameter 'size')* |
| `(data, 3)` | constructed, `length=3` | **byte-identical** |

**A second defect in the same constructor is disclosed rather than filed under
noise.** The finding text named only the null dereference. A negative `size` was
also unvalidated, and it did not merely produce a wrong answer: it formed
`buffer + size` — out-of-bounds pointer arithmetic, undefined in its own right —
and then constructed a vector from a reversed range, which escaped as a raw
`std::length_error`. A standard-library exception was crossing a public API whose
entire purpose is to mirror .NET's argument diagnostics. The same change closes
it, and a permanent test catches `std::length_error` *first* so the assertion
fails if it ever comes back.

**The repair.** `data_` is now initialized from a file-local
`validatedBufferCopy(buffer, size)` that validates and then copies, so nothing
invalid reaches pointer arithmetic or the vector range constructor. Null takes
precedence over a bad size, matching .NET's own ordering, in which
`ArgumentNullException.ThrowIfNull(buffer)` precedes the count check — so
`(nullptr, -1)` reports the null. An anonymous-namespace helper in the `.cpp` was
chosen over a member function so that **no header, signature, object layout,
vtable or exported symbol changed**; the sibling `UnmanagedMemoryStream.cpp` in
the same module already validates that way.

**One input is deliberately still accepted, and that decision is load-bearing.** A
null pointer paired with a size of **zero** remains valid. This port's parameter
is a pointer/length pair, not .NET's `byte[]` object: `(nullptr, 0)` is the
ordinary spelling of an empty range, `std::vector<bytecs>().data()` is permitted
to return null and does on this toolchain, and `BinaryData::ToStream()` reaches
this constructor exactly that way for empty content. The pre-fix probe's case 2
shows the input was already well defined and already produced a correct empty
stream, so rejecting it would have been a **regression, not a repair** — which is
precisely the correction ticket #1774 had to make after #1771 over-rejected the
same shape on `ICollection::CopyTo`. `UnmanagedMemoryStream` diverges from both
and rejects null unconditionally, because it *retains* the caller's pointer and
.NET's own `UnmanagedMemoryStream` rejects it unconditionally too; that
divergence is now stated in the header doc-comment rather than left to be
inferred.

**Tests: +14 permanent regressions.** Thirteen in
`modules/io/tests/System/IO/StreamTests.cpp` — null/positive (the audited input),
null/large, the parameter name in the message, null-before-negative precedence,
null/zero, null/zero still writable afterwards, `std::vector::data()` on an empty
vector, negative, the explicit no-`std::length_error` assertion, `INT32_MIN`, zero
size with a non-null source, the unchanged valid path, and the source-lifetime/copy
independence the audit report itself listed as missing. One in
`modules/io/tests/System/BinaryDataTests.cpp` pinning `BinaryData::Empty().ToStream()`,
the in-repository caller that makes the accepted `(nullptr, 0)` rule load-bearing:
delete the rule and that test fails.

**Validation.** `SharpRuntimeTests_IO` **541/541** (was 527), and the same 541
under **AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer with zero
reports** (`build-asan/1805_io_asan.log`). LeakSanitizer was **proved active**
rather than assumed: `build-probe/1805_lsan_selftest.cpp` is reported as a
4,096-byte definite leak. The first version of that self-test leaked from a
pointer still live on the stack at exit, which LSan correctly classifies as *still
reachable* and does not report — it proved nothing, and was replaced rather than
believed. Repository gate `scripts/local_ci_check.sh build`: **0 warnings, 0
errors**, **13,937 tests across 37 executables** (was 13,923), including the six
local-server `Net.Http` cases, which were network-permitted in this run. Module
graph **41 / 90**; catalogue current; database consistent; version-seam ODR
**2 seams / 18 specialisations** plus 12/12 self-test; negative consumer fixtures
**9 / 66, every site rejected** plus 37/37 self-test; the ten-component selective
matrix passed with its 3 forbidden fixtures rejected; Doxygen **1,941** of the
1,942 ceiling, **unchanged** — the header gained two `@throws` lines, which
Doxygen resolves; `git diff --check` clean.

**No consumer fixture was added**, deliberately. The existing ones exist for
contracts a consumer can only be *shown* through the public headers — ABI shape,
seam reachability, compile rejection. This ticket changes no signature and outlaws
no spelling; the GoogleTest suite exercises the identical public constructor, and
a fixture would have rebuilt the whole `IO` component to re-assert it.

**Source and ABI consequences: none.** No public signature, object layout, vtable,
inheritance or exported symbol changed, so **no consumer rebuild is required on
this ticket's account**. One behavioural note belongs in a consumer's release
notes: a caller that was catching `std::length_error` to detect a negative size no
longer catches it. That spelling was an accident of the vector range constructor,
never a contract, and no in-repository caller relied on it.

**Not closed by this ticket, and said so in the audit report:** the second bullet
of that report's "Missing assertions and diagnostics" — the absent near-limit
capacity/position diagnostic — needs a multi-gigabyte allocation to exercise and
is not part of SR-AUD-341's crash contract.

Build directories used: `build/` (gate), `build-asan/` (the pre-existing
sanitizer tree, which gained the `SharpRuntimeTests_IO` target it did not have),
`build-probe/` (this ticket's probes and logs, all `1805_` prefixed),
`build-tmp/` (repository-local `TMPDIR` for the `mktemp`-based gate, Doxygen and
selective-matrix scripts); **no new build directory was created**. **No
compilation exceeded three jobs.**

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked` and
untouched. No previously `done` ticket was reopened and no finding was reopened.

### Completed text-wrapper null-stream validation: ticket #1806

Ticket #1806 (`REMED-IO-TEXT-WRAPPER-NULL-STREAM`, P1, size S, `remediation`,
area *IO*) is **done** and **SR-AUD-338 is now `remediated`**. It is the second
of the two self-contained ASan/UBSan-backed public-input repairs NEXT.md's
recommended dependency order names, taken directly after #1805. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **12 remediated** and **352 confirmed** of 364.

**The finding named one dereference. There were five.** Measured before any
production change, one process per case so a crash could not hide another
(`build-probe/1806_prefix_defects.cpp`, logs `1806_prefix_defects.log` and
`1806_postfix_defects.log`). Pre-fix, on a null base stream:

| Call | Pre-fix | Post-fix |
|---|---|---|
| `StreamReader` `Read` / `Peek` | `-1` | `ArgumentNullException` at construction |
| `StreamReader` `ReadLine` / `ReadToEnd` | `""` | same |
| `StreamWriter::Write(std::string)` | UBSan *member access within null pointer of type `struct Stream`*, ASan **SEGV on 0x0** | same |
| `StreamWriter::Write(const char*)` | same | same |
| `StreamWriter::Flush()` | same | same |
| `StreamWriter::Close()`, `leaveOpen=false` | same | same |
| `~StreamWriter()`, `leaveOpen=false` | same | same |
| `BinaryReader(nullptr)` / `BinaryWriter(nullptr)` | `ArgumentNullException` **already** | unchanged |

The destructor case is the sharpest and is **not** in the finding text: with the
**default** `leaveOpen = false`, merely constructing a `StreamWriter` over a null
stream and letting it leave scope was fatal, because the destructor closed a
stream it did not have. No call on the object was required.

**The reader's half is not a crash, and its guards were removed rather than
kept.** `Read()`/`Peek()` returning `-1` and `ReadLine()`/`ReadToEnd()` returning
`""` are exactly what an **empty document** returns, so a programming error was
silently laundered into ordinary, plausible data — a caller could not tell "there
was no stream" from "there was nothing in it". Both constructors now throw
`ArgumentNullException("stream")`, matching .NET, whose every `Stream`-taking
constructor of both types opens with `ArgumentNullException.ThrowIfNull(stream)`,
and matching the sibling `BinaryReader`/`BinaryWriter` **in the same module**,
whose already-correct behaviour the audit called out as making the divergence
especially hazardous. With that check in place `stream_` is non-null for the
lifetime of every `StreamReader` — the only other constructor assigns a freshly
allocated `FileStream`, and nothing else writes the member — so the
`stream_ == nullptr` tests in `Peek()`, `Read()`, `Close()` and the destructor are
**gone**, rather than left behind as unreachable code implying a state that can no
longer exist.

**Tests: +11 permanent regressions** in `IOStreamTests.cpp` — reader null, reader
null with `leaveOpen=true`, the reader's parameter name, the same three for the
writer, a cross-type assertion that all four `Stream*`-wrapping types in this
module now answer the identical input identically, the reader's ordinary read
paths re-pinned after its guards were removed, the empty-stream `-1`/`""` meanings
re-pinned so they keep their one remaining legitimate sense, the writer's ordinary
write path, and a check that a rejected construction leaves a neighbouring live
stream untouched — throwing from the constructor body means `~StreamWriter()`
never runs, so the failure cannot close or delete anything.

**Validation.** `SharpRuntimeTests_IO` **552/552** (was 541), and the same 552
under **ASan + UBSan + LSan with zero reports** (`build-asan/1806_io_asan.log`);
LeakSanitizer's activity was established by #1805's self-test earlier in this same
session. Repository gate `scripts/local_ci_check.sh build`: **0 warnings, 0
errors**, **13,948 tests across 37 executables** (was 13,937), including the six
local-server `Net.Http` cases. Module graph **41 / 90**; catalogue current;
database consistent; seam ODR **2 / 18** plus 12/12; negative consumer fixtures
**9 / 66** plus 37/37; the ten-component selective matrix passed with its 3
forbidden fixtures rejected; Doxygen **1,941** of the 1,942 ceiling, unchanged;
`git diff --check` clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. One behavioural note belongs in a consumer's release
notes: code that constructed either wrapper over a null stream and relied on the
reader's silent end-of-stream now receives `ArgumentNullException` at
construction. No in-repository caller did so, and no test asserted the old
behaviour.

**Explicitly not done here.** **SR-AUD-337** — the `leaveOpen` disposal contract,
which shares these two exact files — is untouched and stays `confirmed`. Rolling
it in would have merged two unrelated contracts into one ticket.

**Two separable defects were found while doing this work and are recorded as
inactive `todo` tickets rather than folded in or concealed:**

- **#1809** (P2) — `TextWriter::Write(const char*)` forms `std::string(value)` and
  `StreamWriter::Write(const char*)` calls `std::strlen(value)`; both are
  undefined for a null pointer, and the same shape reaches `WriteLine(const char*)`
  and every `TextWriter` subclass. It is kept separate because the answer is a
  contract decision, not a guard: .NET's `TextWriter.Write(string?)` treats null as
  a **no-op**, so a throwing guard would diverge from .NET while a silent one would
  match it. The audit did not record this; that is stated plainly rather than
  backfilled into a finding.
- **#1808** (P2) — neither wrapper validates `CanRead`/`CanWrite`, where
  `StreamReader.cs:147` and `StreamWriter.cs:103` throw
  `ArgumentException(SR.Argument_StreamNotReadable / _StreamNotWritable)`. Kept
  separate because rejecting a stream that exists but is *unsuitable* is a
  different contract from rejecting one that does not exist, and because it can
  reject calls that work today — its acceptance criteria require an inventory
  before any implementation.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1806_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**. The ticket's
probe binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked` and
untouched. No previously `done` ticket and no finding was reopened.

### Completed AggregateException null-inner validation: ticket #1807

Ticket #1807 (`REMED-CORE-AGGREGATEEXCEPTION-NULL-INNER`, P1, size S,
`remediation`, area *Core*) is **done** and **SR-AUD-097 is now `remediated`** —
the third of NEXT.md's eight immediate public-input crash tickets to land, after
SR-AUD-089 (#1776), SR-AUD-341 (#1805) and SR-AUD-338 (#1806). **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **13 remediated** and **351 confirmed** of 364.

**The finding named one crash path. There were three, and two silent ones.**
`std::rethrow_exception` is undefined for a null argument and three members call
it, so a single accepted null armed all three at once. Measured before any
production change, one process per case
(`build-probe/1807_prefix_defects.cpp`, logs `1807_prefix_defects.log` and
`1807_postfix_defects.log`):

| Input / call | Pre-fix | Post-fix |
|---|---|---|
| `AggregateException(vector{null})`, `{null}`, `{valid, null}` | **ASan SEGV** in `std::rethrow_exception`, address `0xffffffffffffff80` | `ArgumentException` — *An element of innerExceptions was null.* |
| `AggregateException("m", vector{null})` then `Flatten()` | same SEGV via `collectLeaves` | rejected at construction |
| …then `GetBaseException()` | same SEGV | rejected at construction |
| …then `Handle(always-true)` | **completed**, `predicate-saw-null=1` | rejected at construction |
| `AggregateException("m", exception_ptr())` then `Unwrap()` | **completed**, `unwrapped-null=1` | `ArgumentNullException` — *(Parameter 'innerException')* |
| the two valid constructions | `One or more errors occurred. (boom)`, `count=1 message='outer'` | **byte-identical** |

The two `completed` rows are the ones worth naming. They did **not** crash: the
message-plus-collection and message-plus-single constructors never built a
message from their inner exceptions, so they stored the null quietly, and then
`Handle()` handed it to the caller's predicate and `Unwrap()` returned it. The
crash happened afterwards, in consumer code, at a `std::rethrow_exception` the
consumer wrote, with nothing left to indicate where the null had entered.

The trap address is `0xffffffffffffff80`, not `0x0` — that is what a null
`std::exception_ptr` decodes to inside libstdc++ — and it is written down so a
future reader matching this signature is not sent looking for an ordinary null
dereference.

**The two exception types are deliberately different, and a test pins them
apart.** .NET's private `AggregateException(string?, Exception[], bool)` core
constructor, through which every public collection overload funnels, throws
`ArgumentException(SR.AggregateException_ctor_InnerExceptionNull)` for a null
**element**, while `AggregateException.cs:59` opens
`AggregateException(string?, Exception)` with
`ArgumentNullException.ThrowIfNull(innerException)` for a null **argument**. Since
`ArgumentNullException` derives from `ArgumentException`, a test catching only the
base type would still pass if the two were collapsed onto one type, so one
regression asserts the collection case is **not** caught as
`ArgumentNullException`.

**Tests: +10 permanent regressions** in `ExceptionRemainingTests.cpp` — null in a
vector, null in an initializer list, null after a valid entry, the exact .NET
message text, null through the message-plus-vector constructor, null through the
message-plus-single constructor, its parameter name, the type split, an assertion
that no constructed aggregate (flat, nested, or either flattened) can hold a null
inner, and the unchanged valid paths including the empty-collection default
message.

**Validation.** `SharpRuntimeTests_Core_Base` **4,982/4,982** (was 4,972), and the
same 4,982 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1807_core_base_asan.log`). Repository gate
`scripts/local_ci_check.sh build`: **0 warnings, 0 errors**, **13,958 tests across
37 executables** (was 13,948), including the six local-server `Net.Http` cases.
Module graph **41 / 90**; catalogue current; database consistent; seam ODR
**2 / 18** plus 12/12; negative consumer fixtures **9 / 66** plus 37/37; the
ten-component selective matrix passed with its 3 forbidden fixtures rejected;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** `AggregateException` is header-only and
gains two private static helpers; no public signature, object layout, vtable or
exported symbol changed. Behavioural note: code that constructed an aggregate over
a null `std::exception_ptr` now receives an argument exception at construction
instead of crashing later. No in-repository caller did so —
`CancellationTokenSource` and `Parallel`, the only two producers, both push
`std::current_exception()` from inside a `catch (...)`, where it is never null.

**Explicitly not done here.** **SR-AUD-098** (causal diagnostics and flatten
ordering) and **SR-AUD-099** (`Handle` accepts an empty predicate) share this file
and stay `confirmed`. SR-AUD-099 belongs to **CCF-011**, which the remediation
roadmap requires be taken as a scoped family rather than one file at a time, and
SR-AUD-098 is a different contract entirely. The audit report's remaining
missing-assertion list — first-inner identity, custom-message aggregation, `Handle`
message and order preservation, predicate exceptions, nested/direct leaf order,
the `GetBaseException` value-API adaptation, and the HResult assertion — belongs
to those two findings and is not claimed as closed.

Build directories used: `build/` (gate), `build-asan/` (which gained the
`SharpRuntimeTests_Core_Base` target it did not have; 5m23s at three jobs),
`build-probe/` (all `1807_` prefixed), `build-tmp/` (repository-local `TMPDIR`);
**no new build directory was created** and **no compilation exceeded three jobs**.
The probe binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked`; #1808 and
#1809, opened by #1806, remain `todo` and unbegun. No previously `done` ticket and
no finding was reopened.

### Completed interpolated-handler pointer validation: ticket #1810

Ticket #1810 (`REMED-CORE-INTERPOLATED-HANDLER-NULL-DEST`, P1, size S,
`remediation`, area *Core*) is **done** and **SR-AUD-132 is now `remediated`** —
the fourth of NEXT.md's eight immediate public-input crash tickets. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **14 remediated** and **350 confirmed** of 364.

**This one is a write.** `TryWriteInterpolatedStringHandler(nullptr, 1)` passed
the capacity check in `appendRaw` — `1 >= 1` — and reached
`std::memcpy(dest_ + pos_, ...)`, an AddressSanitizer-confirmed **write to the
zero page**, with UBSan reporting *null pointer passed as argument 1, which is
declared to never be null*. That is the most severe shape among the remaining
public-input findings, which is why it was taken ahead of them. Measured before
any production change, one process per case
(`build-probe/1810_prefix_defects.cpp`, logs `1810_prefix_defects.log` and
`1810_postfix_defects.log`):

| Input | Pre-fix | Post-fix |
|---|---|---|
| `handler(nullptr, 1).AppendLiteral("x")`, both constructors | **ASan SEGV on 0x0** in `memcpy`, exit 1 | `ArgumentNullException` *(Parameter 'destination')* |
| `AppendLiteral((const char*)nullptr)` | **ASan SEGV** in `strlen`, exit 1 | `ArgumentNullException` *(Parameter 'value')* |
| `handler(nullptr, 0).AppendLiteral("x")` | refused, `success=0` | **byte-identical** |
| fill the buffer then append one more | refused, `written=8 success=0` | **byte-identical** |
| ordinary use | `written=4 success=1 string='x=42'` | **byte-identical** |

**What .NET gets for free, this port must check.** The .NET counterpart takes a
`Span<char>`, which cannot represent a nonempty null destination at all, so there
is no .NET validation to copy — the check restores by validation what the .NET
type gets from its parameter type. A null paired with a capacity of **zero** stays
valid, per the rule tickets #1774 and #1805 settled for the same pointer/length
shape; the probe shows it already behaved correctly.

**The null-literal policy is decided here, not inherited.** The finding's closing
sentence asked for exactly that. In .NET the handler is compiler-generated and
`AppendLiteral` receives only literal text, so no .NET behaviour applies.
`AppendLiteral(const char*)` throws rather than treating null as empty: the
`std::string` overload cannot be null — `""` is already how an empty literal is
spelled — and the `bool` result already means "did it fit", so succeeding silently
would give that result a second meaning and hide the caller's bug. One regression
asserts `""` and `nullptr` behave differently, so a later change cannot quietly
collapse them.

**Two further defects in the same members are closed by the same change**, both
taken from the audit report's own "Other missing assertions" list rather than
found anew:

- the capacity test was `pos_ + len > destLen_`, a `size_t` sum that can **wrap**
  and let an oversized append pass the very check meant to stop it. It is now
  `len > destLen_ - pos_`, which cannot wrap because `pos_ <= destLen_` is an
  invariant of the class;
- `std::memcpy` is undefined for a null pointer even at zero length, and so is
  `std::string(nullptr, 0)` in `getString()`. Both are reachable — `dest_` is null
  for a zero-capacity handler and `data` is null for an empty `std::string` — so
  `appendRaw` returns early at `len == 0` and `getString()` guards the null.

**Tests: +12 permanent regressions** in
`TryWriteInterpolatedStringHandlerTests.cpp`, including one that pins the
four-argument constructor's `shouldAppend` out-parameter as deliberately
**unwritten** on rejection: an exception reports a destination that does not
exist, where `shouldAppend = false` reports one that exists and is too small, and
conflating them would make the failure indistinguishable from an ordinary
short-buffer result.

**Validation.** `SharpRuntimeTests_Core_Base` **4,994/4,994** (was 4,982), and the
same 4,994 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1810_core_base_asan.log`). Repository gate: **0 warnings, 0 errors**,
**13,970 tests across 37 executables** (was 13,958). Module graph **41 / 90**;
catalogue current; database consistent; the ten-component selective matrix passed;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** Header-only, one new private static helper;
no public signature, object layout, vtable or exported symbol changed. Neither
rejected input had any in-repository caller.

**Explicitly not done here.** **SR-AUD-133** — `AppendFormatted` discards its
format string and substitutes hardcoded C++ spellings, so `true` renders as `1`,
`255` with `"X2"` as `255`, and `3.14` as `3.140000` — shares this file and stays
`confirmed`. It asks for format/provider-aware formatting *or* an explicit
renaming of the surface to a documented primitive formatter, which is a design
decision about what this type is, not a safety repair. The report's observation
that the class is an ordinary C++ object rather than a compiler-generated
`ref struct`, with nothing preventing it from being copied or escaping, belongs
with it.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1810_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**. The probe
binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked`; #1808 and
#1809 remain `todo` and unbegun. No previously `done` ticket and no finding was
reopened.

### Completed compression-stream null-inner validation: ticket #1811

Ticket #1811 (`REMED-IO-COMPRESSION-NULL-INNER-STREAM`, P1, size S,
`remediation`, area *IO*) is **done** and **SR-AUD-257 is now `remediated`** —
the fifth of NEXT.md's eight immediate public-input crash tickets. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **15 remediated** and **349 confirmed** of 364.

`DeflateStream`, `GZipStream` and `ZLibStream` all take a raw `Stream*` and none
validated it. Measured before any production change, one process per case, across
all three types symmetrically rather than only the one the finding named
(`build-probe/1811_prefix_defects.cpp`, logs `1811_prefix_defects.log` and
`1811_postfix_defects.log`):

| Cases | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1–3 | `T(nullptr, Compress, true)` then a 256 KiB incompressible `Write` | **ASan SEGV on 0x0**, all three types | `ArgumentNullException` at construction |
| 4–6 | `T(nullptr, Decompress, true)` then `Read` | **ASan SEGV on 0x0**, all three types | same |
| 7–12 | `T(nullptr, Compress, false)` then `Close()`, and destruction alone | completed | same |
| 13 | the valid path over a real `MemoryStream` | `length=6` | **byte-identical** |

The finding named `Write`. The `Decompress`-mode `Read` path crashes identically
and is the second half of the same defect; it is recorded rather than absorbed.

**The payload shape is why the check belongs at construction.** A small
compressible write does not reproduce this at all — zlib absorbs it into the
64 KiB deflate buffer and never touches the inner stream. That is what the
finding's phrase "a sufficiently large incompressible Write" is about, and it is
the argument against fixing this on the write path: a write-path guard would
still leave a constructed object whose inner stream does not exist. The check
also sits **before** `deflateInit2`/`inflateInit2`, which allocate state only
`deflateEnd`/`inflateEnd` release, so a rejected construction allocates no
compressor state.

**Cases 7–12 did not crash, and the reason is recorded so it is not "tidied
away" later.** `Close()` tests `if (produced > 0 && inner_)` and
`if (!leaveOpen_ && inner_)`. Those look exactly like the guards ticket #1806
removed from `StreamReader` as unreachable — but here they are **reachable**:
`Close()` itself assigns `inner_ = nullptr` after closing a non-`leaveOpen` inner
stream, so a null `inner_` is a genuine post-close state in these three classes.
They stay, and a comment in each constructor plus a note on the ticket row says
why. Two tickets in the same session reached opposite conclusions about
identical-looking code, for a concrete reason, and that is worth writing down.

**Tests: +9 permanent regressions** in `CompressionTests.cpp` — both compression
modes rejected for each of the three types, the owning (`leaveOpen = false`) shape
whose destructor and `Close()` also touch the inner stream, the parameter name,
and a large incompressible round-trip that exercises the exact flush path the
crash came from.

**Validation.** `SharpRuntimeTests_IO_Compression` **31/31** (was 22), and the
same 31 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1811_io_compression_asan.log`). Repository gate: **0 warnings, 0
errors**, **13,979 tests across 37 executables** (was 13,970). Module graph
**41 / 90**; catalogue current; database consistent; the ten-component selective
matrix passed, including the `IO.Compression` and `IO.Compression.Zip` consumer
fixtures that build this component in isolation; Doxygen **1,941** of the 1,942
ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. Behavioural note: constructing any of the three over a
null stream now throws instead of deferring a crash to first use. No
in-repository caller did so.

**Explicitly not done here.** **SR-AUD-258** — invalid `CompressionMode` values
silently accepted (a native `(CompressionMode)42` constructs, creates a deflater,
and reports both `CanRead` and `CanWrite` false, where .NET throws
`ArgumentException`), and post-`Close` operations returning silently — shares
these files and stays `confirmed`.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1811_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**. The probe
binaries were deleted once their logs were transcribed.

**CNA and mobile-eggbert were not inspected, searched, configured, built or
modified**, and **#1773 remains `blocked`**. #1804 remains `blocked`; #1808 and
#1809 remain `todo` and unbegun. No previously `done` ticket and no finding was
reopened.

## Session summary — 2026-07-29, five public-input crash remediations

Tickets **#1805, #1806, #1807, #1810 and #1811** are `done`, each remediating a
high-severity public-input crash finding: SR-AUD-341, SR-AUD-338, SR-AUD-097,
SR-AUD-132 and SR-AUD-257 respectively. Two new inactive `todo` tickets, **#1808**
and **#1809**, were opened from defects found during #1806 and were not folded
into it. The audit identifier range stays **frozen at 364**; the findings index
now records **15 remediated and 349 confirmed** of 364, up from 10 and 354.

The ticket queue was **empty** when the session began — every row `done` except
the correctly `blocked` #1773 and #1804. The backlog lives in
`audit/AUDIT_FINDINGS_INDEX.md`, and each ticket was created by converting the
next item from `NEXT.md`'s recommended dependency order, which is how every
remediation ticket since #1767 has begun.

In **all five**, the defect proved larger than the finding described: five
`StreamWriter` dereferences rather than one, three `AggregateException` crash
paths plus two silent hand-offs to the caller, a `Read` half of SR-AUD-257 that
was never named, a `std::length_error` leak alongside SR-AUD-341's null read, and
a `size_t` capacity wrap alongside SR-AUD-132's zero-page write. Every extra
defect is disclosed in the owning audit report rather than absorbed silently.

Baselines after the session: **13,979 tests across 37 executables** (from 13,923),
0 warnings, 0 errors, 41 modules / 90 edges, Doxygen **1,941** of the 1,942
ceiling unchanged throughout, negative consumer fixtures 9 / 66 with every site
rejected, version-seam ODR 2 seams / 18 specialisations.

Full detail — per-ticket measurements, scope boundaries, environment notes and
the recommended next ticket — is in `NEXT.md`'s "CONTEXT-REFRESH handoff" section
and in the per-ticket sections above.

### Completed ZipArchive null-stream validation: ticket #1812

Ticket #1812 (`REMED-IO-ZIP-NULL-STREAM`, P1, size S, `remediation`, area *IO*)
is **done** and **SR-AUD-242 is now `remediated`** — the sixth of NEXT.md's eight
immediate public-input crash tickets. **No new `SR-AUD-*` identifier**; the
numbering stays frozen at 364, and the index now records **16 remediated** and
**348 confirmed** of 364.

`ZipArchive`'s public `Stream*` constructor stored the pointer with no null
check. Measured before any production change, one process per case
(`build-probe/1812_prefix_defects.cpp`, logs `1812_prefix_defects.log` and
`1812_postfix_defects.log`):

| Case | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `ZipArchive(nullptr, Read)` | **ASan SEGV on 0x0**, exit 1 | `ArgumentNullException` |
| 2 | `ZipArchive(nullptr, Update)` | **ASan SEGV on 0x0**, exit 1 | same |
| 3 | `ZipArchive(nullptr, Create)` | constructed | same |
| 4 | `ZipArchive(nullptr, Create)` + `CreateEntry` + write + `Dispose()` | completed — **and delivered nothing** | same |
| 5 | `ZipArchive(nullptr, Create)` destruction alone | completed | same |
| 6 | `ZipArchive(nullptr, (ZipArchiveMode)42)` | constructed | rejected on the null, before the mode |
| 7 | the valid Create path over a real `MemoryStream` | `length=146` | **byte-identical** |
| 8 | the valid Read path over case 7's archive | `entries=1 name=payload.txt length=5` | **byte-identical** |

**The two halves of this finding are not symmetric, and that asymmetry is the
argument for an unconditional guard.** Read and Update dereference the pointer
inside the constructor itself, so they fail loudly and immediately. Create never
crashes: it stores the null pointer and then every call the caller makes
succeeds — `CreateEntry`, the entry write stream, `Dispose()`. The finalized
archive lands in `state_->memBuf`, and `Dispose()`'s write-back is gated on
`state_->stream != nullptr`, so the archive is discarded in silence. Case 4 wrote
a complete one-entry archive and delivered it nowhere, with no diagnostic of any
kind. Silent data loss is the worse of the two failure modes, so the check covers
every mode rather than only the ones that segfault.

The check sits **first**, before any state is populated: nothing opens a reader
and nothing fills a buffer on the rejected path, and `state_` is a `shared_ptr`
that unwinds on its own. This matches .NET, whose `Stream`-taking `ZipArchive`
constructors all funnel into `ZipArchive(Stream, ZipArchiveMode, bool,
Encoding?)` and open with `ArgumentNullException.ThrowIfNull(stream)`.

**The path-based constructor overload is deliberately unchanged** and a
regression pins that: an unopenable path still raises
`System::IO::InvalidDataException` from the reader, not the new guard.

**One separate defect was found and deliberately not folded in.** Probe case 9 —
added *after* the fix, because a null stream no longer reaches the mode at all —
constructs `ZipArchive(&realStream, (ZipArchiveMode)42)` successfully, where
.NET's `ValidateMode` throws `ArgumentOutOfRangeException(nameof(mode))`. It is a
different public contract, carries **no `SR-AUD-*` identifier** (the audit
recorded invalid mode values only as a missing-test note, never as a finding),
and is now inactive ticket **#1813**, which is explicitly told to inventory the
sibling enum surfaces — including SR-AUD-258's `CompressionMode` half and
`ZipFile::Open`'s own `ZipArchiveMode` parameter — before writing a guard.

**Tests: +8 permanent regressions** in the ZIP integration fixture
(`tests/integration/System/IO/Compression/CompressionTests.cpp`) — all three
modes, the defaulted-mode overload, the parameter name, repeatability of the
rejected construction, an unaffected valid Create/Read round-trip, and the
path-based overload.

**Validation.** The ZIP fixture is **44/44** (was 36), and the same 44 under
**ASan + UBSan + LSan with zero reports** (`build-asan/1812_zip_asan.log`, which
required building `SharpRuntimeIntegrationTests` in `build-asan/` for the first
time, at three jobs). Repository gate: **0 warnings, 0 errors**, **13,987 tests
across 37 executables** (was 13,979). Module graph **41 / 90**; catalogue
current; database consistent; the ten-component selective matrix passed,
including the `IO.Compression.Zip` fixture that builds this component in
isolation; Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check`
clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. Behavioural note: constructing a `ZipArchive` over a
null stream now throws instead of crashing (Read/Update) or silently discarding
output (Create). No in-repository caller did so.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1812_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### Completed HttpContent JSON null-content validation: ticket #1814

Ticket #1814 (`REMED-NET-HTTP-JSON-NULL-CONTENT`, P1, size S, `remediation`,
area *Net*) is **done** and **SR-AUD-236 is now `remediated`** — the seventh of
NEXT.md's eight immediate public-input crash tickets. **No new `SR-AUD-*`
identifier**; the numbering stays frozen at 364, and the index now records
**17 remediated** and **347 confirmed** of 364.

`HttpContentJsonExtensions::ReadFromJson` dereferenced its
`std::shared_ptr<HttpContent>` with no validity check, and `ReadFromJsonAsync`
delegated to it from inside the task body. Measured before any production change,
one process per case (`build-probe/1814_prefix_defects.cpp`, logs
`1814_prefix_defects.log` and `1814_postfix_defects.log`):

| Case | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `ReadFromJson(empty)` | UBSan *member access within null pointer* at `HttpContentJsonExtensions.hpp:32`, then **ASan SEGV on 0x0** | `ArgumentNullException` |
| 2 | `ReadFromJsonAsync(empty)` then `getResultProperty()` | same pair, on worker thread **T1** | same, thrown by the call itself |
| 3 | `ReadFromJsonAsync(empty)` and **never awaited** | same pair, on worker thread **T1** — the process still died | same |
| 4 | the valid synchronous path | `answer=42` | **byte-identical** |
| 5 | the valid asynchronous path | `answer=7` | **byte-identical** |

**Case 3 is why the async guard sits before the task, not inside it.** The finding
describes `ReadFromJsonAsync` as turning null input into "a deferred task crash".
Measured, it is worse than deferred: the dereference happened on the `std::async`
worker thread, so a caller that started the task and walked away still lost the
whole process to a SEGV on thread T1. A guard placed only inside `ReadFromJson`
would have converted that into an exception stored on a task the caller never
observes — quieter, still wrong.

.NET prevents exactly this by code layout, and this port now copies it: the public
`ReadFromJsonAsync` overloads in `HttpContentJsonExtensions.cs` are **not** `async`
methods. Each runs `ArgumentNullException.ThrowIfNull(content)` and only then calls
the separate `ReadFromJsonAsyncCore`, so a null argument throws at the call site. A
named regression pins that placement, not just the exception type.

**One component-metadata consequence — the first in this remediation series.**
`Net.Http.Json` is an `INTERFACE` (header-only) component, so the guard lives in a
public header, which must include `System/ArgumentNullException.hpp`.
`Net.Http.Json` previously reached `Core.Base` only transitively through
`Net.Http`, and the boundary validator correctly rejected the undeclared public
edge. `modules/net-http-json/CMakeLists.txt` now declares `Core.Base` in
`PUBLIC_DEPENDENCIES` and `docs/ComponentCatalog.md` was regenerated. **The
production graph moves from 90 to 91 direct edges**; the module count is unchanged
at 41. No consumer include path, target name or link line changes as a result, and
the ten-component selective matrix still passes.

**Tests: +7 permanent regressions** in `HttpContentJsonExtensionsTests.cpp` — both
entry points rejected, the parameter name on both, the synchronous-throw placement
of the async guard, repeatability, content whose body is the JSON literal `null`
(an empty `shared_ptr` and a document that parses to null are different things and
must stay different), and empty content still reaching the parser rather than
being absorbed by the new guard.

**Validation.** `SharpRuntimeTests_Net_Http_Json` **15/15** (was 8), and the same
15 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1814_net_http_json_asan.log`). Repository gate: **0 warnings, 0
errors**, **13,994 tests across 37 executables** (was 13,987). Module graph **41 /
91**; catalogue regenerated and current; database consistent; the ten-component
selective matrix passed; Doxygen **1,941** of the 1,942 ceiling, unchanged;
`git diff --check` clean.

**Source and ABI consequences: none.** No public signature, object layout, vtable
or exported symbol changed. Behavioural note: passing an empty `shared_ptr` to
either method now throws instead of crashing the process. No in-repository caller
did so.

**Deliberately out of scope.** `HttpClientJsonExtensions`'s
`GetFromJsonAsync`/`DeleteFromJsonAsync` do guard their response content, but map a
null content body to the JSON literal `"null"`
(`content ? content->ReadAsString() : "null"`) rather than to a diagnostic. That is
a different contract on a different type, carries no `SR-AUD-*` identifier, and was
left exactly as found.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1814_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### Completed Base64 family plan and in-place write order: tickets #1815 and #1816

With the eight immediate public-input crash findings closed, the roadmap's item 3
applies: cross-cutting causes get a **scoped family plan** first, not a
file-by-file sweep. **CCF-013** was taken first because it is the smallest
coherent cause and the only one of the five that is a *correctness* defect rather
than a parity difference — the current output is simply wrong.

**Ticket #1815** (`REMED-BUFFERS-BASE64-FAMILY-PLAN`, P2, size S, design-only) is
**done** and recorded in
[`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md). No production source
changed under it. It establishes, from evidence rather than memory:

- CCF-013 has exactly **one** member finding, SR-AUD-078, spanning **two** public
  headers, and the cause explicitly requires one repair covering both;
- four adjacent `confirmed` findings — SR-AUD-079, SR-AUD-080, SR-AUD-081,
  SR-AUD-082 — live in the *same two headers* and share the same shape, but are
  **not** CCF-013 members and are not renamed into it;
- the pre-existing in-place encode tests covered `dataLength` **2 and 3 only**,
  which are precisely the two shapes that *cannot* exhibit the defect (length 3
  has no remainder, length 2 has no full pack) — that is why the suite was green;
- #1817, #1818 and #1819 all edit the same final-quantum branch of one
  `decodeCore` and are therefore **sequenced**, while #1820 touches the Base64Url
  decode table instead and is deliberately **unordered** against them;
- **none** of the six tickets needs the public-signature/layout approval category,
  though #1817 and #1818 do narrow the accepted input set, which each must state
  in its own record.

**Ticket #1816** (`REMED-BUFFERS-BASE64-INPLACE-ORDER`, P1, size S) is **done**
and **SR-AUD-078 is now `remediated`**, which **closes CCF-013**. **No new
`SR-AUD-*` identifier**; the numbering stays frozen at 364, and the index now
records **18 remediated** and **346 confirmed** of 364.

`Base64::EncodeToUtf8InPlace` and `Base64Url::TryEncodeToUtf8InPlace` both encoded
the full 3-byte packs backwards and only then read the trailing one/two-byte
remainder. Encoding pack `i` reads source `3i..3i+2` and writes output
`4i..4i+3`, and `4i >= 3i`, so a pack can only overwrite source bytes belonging to
packs *after* it — which makes a last-to-first walk correct for every full pack
but **not** for the remainder, which is the last pack of all and was handled after
the loop. The remainder is now encoded **first**. That is .NET's own order:
`Base64Helper/Base64EncoderHelper.cs`'s shared
`EncodeToUtf8InPlace<TBase64Encoder>` encodes the leftover pack before its
backwards loop, under the comment *"encode last pack to avoid conditional in the
main loop"*.

**Measured before any production change** (`build-probe/1816_prefix_defects.cpp`,
logs `1816_prefix_defects.log` and `1816_postfix_defects.log`) — every
`dataLength` from 0 to 24, both types, each in-place result compared against *the
same type's own out-of-place encoder*, with a sentinel byte immediately past the
encoded output:

| | Pre-fix | Post-fix |
|---|---|---|
| Cases wrong | **28 of 50** | **0 of 50** |
| Lengths affected, per type | 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 | none |
| Status returned | `Done` / `true` in **all** 50 | unchanged |
| Sentinel past the output | never touched | never touched |

**The scope was larger than the finding's "4/5-byte source lengths"** — it is
every length with both a full pack and a remainder, which the finding's own text
does say and the sweep makes concrete. **The sentinel result is the other half of
the story**: this was silent corruption *inside* the declared output, never an
overrun, which is why no sanitizer had ever flagged it and why only a
differential sweep could find it.

**Tests: +8 permanent regressions**, four per header — the audit's own 4-byte
reproduction (`'A','B','C',0` → `QUJDAA==` / `QUJDAA`, not `QUJDRA==` /
`QUJDRA`), the 5-byte case, a **7-byte** case proving the defect was never limited
to 4 and 5, and the 0..24 sweep asserting equality with the out-of-place encoder
and an untouched sentinel at every length.

**Validation.** `SharpRuntimeTests_Buffers` **473/473** (was 465), and the same
473 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1816_buffers_asan.log`). Repository gate: **0 warnings, 0 errors**,
**14,002 tests across 37 executables** (was 13,994). Module graph **41 / 91**;
catalogue current; database consistent; the ten-component selective matrix
passed; Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check`
clean.

**Source and ABI consequences: none.** Both functions are `static` members of
header-only classes; no signature, layout or exported symbol changed.
**Behavioural note for consumers: any output previously produced in place for a
length with both a full pack and a remainder was wrong and is now correct**, so a
consumer that stored or transmitted such output stored corrupted data. No
in-repository caller uses these APIs outside their tests.

**Left open on purpose.** SR-AUD-079, SR-AUD-080, SR-AUD-081 and SR-AUD-082 stay
`confirmed` and are tickets **#1817–#1820**, ordered by the plan. #1815 also
opened **#1821** for a defect found while planning and not folded in: .NET's
helper short-circuits an empty buffer to `Done` *before* its length check, while
this port returns `DestinationTooSmall`/`false` for an empty buffer with a
positive `dataLength` (`build-probe/1815_empty_buffer_probe.log`). #1821 is framed
as a **decision**, not a foregone fix — reporting `Done` for a request to encode
five bytes into a zero-byte buffer is arguably the worse contract.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1815_`/`1816_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new
build directory was created** and **no compilation exceeded three jobs**.

### Completed Base64 canonical final-quantum rule: ticket #1817

Ticket #1817 (`REMED-BUFFERS-BASE64-CANONICAL-FINAL-BITS`, P2, size S,
`remediation`, area *Buffers*) is **done** and **SR-AUD-079 is now `remediated`**.
It is the second ticket of the Base64 family plan
([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket #1815) and the
first of its three sequenced `decodeCore` tickets. **No new `SR-AUD-*`
identifier**; the numbering stays frozen at 364, and the index now records
**19 remediated** and **345 confirmed** of 364.

Neither header required the unused low bits of the final quantum to be zero:

- a quantum carrying **one** byte (`XX==` padded, `XX` unpadded) uses only the top
  two bits of the second sextet, so its **low four bits** must be zero;
- a quantum carrying **two** bytes (`XXX=` padded, `XXX` unpadded) uses only the
  top four bits of the third sextet, so its **low two bits** must be zero.

Measured before and after (`build-probe/1817_defects.cpp`, with the pre-fix log
built against stashed headers so the two runs use the same source):

| Input | Type | Pre-fix | Post-fix |
|---|---|---|---|
| `AB==` | Base64 | `Done`, 1 byte, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AAB=` | Base64 | `Done`, 2 bytes, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AB` | Base64Url | `Done`, 1 byte, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AAB` | Base64Url | `Done`, 2 bytes, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| 12 canonical spellings, both types | — | accepted | **unchanged** |

**The validator had to change in the same ticket, not a later one.** Decoder and
validator agreed before and after; a validator more permissive than its own decoder
is the worse outcome, because it tells a caller an input is safe to decode when it
is not. Base64Url's `validateCore` only *counted* symbols and never kept their
values, so it now retains the trailing sextets in order to apply the rule at all.

**The canonical check runs before the destination-size check**, deliberately:
canonicity is a property of the input alone and must not depend on how much room
the caller provided. Canonical input is unaffected either way, so no existing
`DestinationTooSmall` outcome changes.

**This narrows the accepted input set**, in the direction of .NET parity. Input
that used to decode successfully is now `InvalidData`. All 104 pre-existing
`Base64*` tests still pass unmodified, so nothing in this repository relied on the
old acceptance.

**Tests: +12 permanent regressions**, six per header — the noncanonical one- and
two-byte quanta rejected by the decoder, `IsValid` and its `decodedLength` overload
agreeing with the decoder, the `char` overloads inheriting the rule through the
shared core, six canonical spellings still decoding to the same bytes, and a 0..24
round trip proving everything this repository's own encoder produces is still
accepted.

**Validation.** `SharpRuntimeTests_Buffers` **485/485** (was 473), and the same 485
under **ASan + UBSan + LSan with zero reports**
(`build-asan/1817_buffers_asan.log`). Repository gate: **0 warnings, 0 errors**,
**14,014 tests across 37 executables** (was 14,002). Module graph **41 / 91**;
catalogue current; database consistent; the ten-component selective matrix passed;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.** No signature, layout or exported symbol
changed; only the accepted input set did.

**Still open in this family**, in the plan's order: **#1818** (SR-AUD-080, padding
accepted while `isFinalBlock` is false), **#1819** (SR-AUD-081, trailing whitespace
wrongly consumed), **#1820** (SR-AUD-082, Base64Url rejects optional final
padding), and **#1821** (the empty-buffer status divergence, no `SR-AUD-*`).

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1817_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### Completed Base64 non-final padding rule: ticket #1818

Ticket #1818 (`REMED-BUFFERS-BASE64-NONFINAL-PADDING`, P2, size S, area
*Buffers*) is **done** and **SR-AUD-080 is `remediated`**. It is the third ticket
of the Base64 family plan ([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md),
ticket #1815) and the second of its three sequenced `decodeCore` tickets. **No new
`SR-AUD-*` identifier**; numbering stays frozen at 364, and the index now records
**20 remediated** and **344 confirmed** of 364.

`Base64::decodeCore` consulted `isFinalBlock` only *after* an incomplete unpadded
group, so a **complete padded** group decoded to `Done` regardless of the flag —
telling a chunked caller that a terminal quantum was ordinary intermediate data.

Current .NET rejects padding in a non-final call along two independent paths:
`Base64DecoderHelper.DecodeFrom` sets `skipLastChunk = isFinalBlock ? 4 : 0`, so
with the flag clear the whole source runs through the four-element loop where `'='`
is unmapped and the padding-aware tail is unreachable; and
`DecodeWithWhiteSpaceBlockwise` forces its per-block `localIsFinalBlock` back to
false whenever the caller's flag is false. The repair is that one rule, applied at
the **first** padding character — which is what keeps `bytesConsumed`/`bytesWritten`
on the last completed quantum boundary and stops a too-small destination from
masking the rejection.

**The finding understated its surface.** It named one input; six of the seven
non-final shapes probed were wrong (`build-probe/1818_defects.cpp`, logs
`1818_prefix_defects.log` / `1818_postfix_defects.log`): the bare padded quantum, a
padded quantum after a complete one, the single-`=` spelling, padding in a
non-terminal position, and padding split by whitespace.

**Two residual divergences are recorded, not fixed** — both in the cursor reported
*alongside* `InvalidData`, neither changing a status or a decoded byte. They are
inactive ticket **#1822**, with no `SR-AUD-*` identifier.

**This narrows the accepted input set.** Every `isFinalBlock == true` outcome is
byte-for-byte unchanged, `IsValid` is unaffected (it has no `isFinalBlock`
parameter and *is* the final-block decoder's validator), and unpadded incomplete
quanta keep `NeedMoreData`.

**Tests: +7 permanent regressions.** `SharpRuntimeTests_Buffers` **492/492** (was
485), and the same 492 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1818_buffers_asan.log`). Repository gate: **0 warnings, 0 errors**,
**14,021 tests across 37 executables** (was 14,014). Module graph **41 / 91**;
catalogue current; database consistent; the ten-component selective matrix passed;
Doxygen **1,941** of the 1,942 ceiling, unchanged; `git diff --check` clean.

**Source and ABI consequences: none.**

**Still open in this family**, in the plan's order: **#1819** (SR-AUD-081, trailing
whitespace wrongly consumed), **#1820** (SR-AUD-082, Base64Url rejects optional
final padding), **#1821** (the empty-buffer status divergence) and **#1822** (the
`InvalidData` cursor), the last two with no `SR-AUD-*`.

Build directories used: `build/` (gate), `build-asan/`, `build-probe/` (all
`1818_` prefixed), `build-tmp/` (repository-local `TMPDIR`); **no new build
directory was created** and **no compilation exceeded three jobs**.

### SR-AUD-081 is a false positive: ticket #1819

Ticket **#1819** (`REMED-BUFFERS-BASE64-PADDED-WHITESPACE-CURSOR`, P2, size XS,
area *Buffers*) is **done, classified FALSE POSITIVE**. **No production source
changed.** It is the fourth ticket of the Base64 family plan
([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket #1815); see that
plan's new §8.

**The finding's premise is inverted.** SR-AUD-081 states that *"the current .NET
Base64 test base specifies that whitespace after end/padding is not included in
consumed bytes"*. That test base specifies the opposite, in three places: its member
data is named `BasicDecodingWithExtraWhitespaceShouldBeCountedInConsumedBytes_MemberData`
and yields `{ "AQ==" + whitespace(i), 4 + i, 1 }`; its second half yields
`{ s+s+s+s, s.Length * 4, 12 }` for seven whitespace placements including a trailing
one; and `DecodingWithWhiteSpaceSplitFinalQuantumAndIsFinalBlockFalse` asserts
`bytesConsumed == base64Data.Length` for `"AQ\r\nQ=\r\n"`, whitespace after the
padding included.

For the finding's own `"QQ== \n"`, .NET reports **6** consumed — exactly what this
port reports. Traced through `Base64DecoderHelper.DecodeFrom`: `SrcLength` rounds the
source to 4, the `if (srcLength != source.Length)` guard sends the call to
`InvalidDataExit`, and `InvalidDataFallback` then finds the remainder to be all
whitespace, executes `bytesConsumed += source.Length` and returns `Done`.

**Measured**: `build-probe/1819_defects.cpp` (log `build-probe/1819_defects.log`)
replays .NET's own vectors with .NET's own expected values on both the UTF-8 and the
`char` overload. **27 of 27 whitespace-consumption vectors match.** The same run
independently re-confirmed ticket #1818 against .NET's *own* tests
(`"AAA="` → `InvalidData`, 0, 0; `"AAAA"` → `Done`, 4, 3; `"AQ\r\nQ="` →
`InvalidData`, 0, 0) — a stronger check than the traced expectations #1818 closed on.

**SR-AUD-081 stays `confirmed`** because the findings-index vocabulary has no
false-positive value — the same treatment SR-AUD-362 received under #1779 — and now
carries a Correction note in the index row and in the owning report. It must not be
read as an open defect. The index counts are unchanged at **20 remediated / 344
confirmed of 364**.

**Tests: +4 permanent regressions** pinning the .NET-verified behaviour so the
inverted premise cannot be re-applied later in good faith — the `"AQ==" + i` shape,
the seven whitespace placements repeated four times, the whitespace-split final
quantum with and without trailing whitespace, and the `char` overload.
`SharpRuntimeTests_Buffers` **496/496** (was 492); repository gate **0 warnings, 0
errors, 14,025 tests across 37 executables** (was 14,021).

**What the run *did* find** is ticket **#1822**, upgraded from two traced instances
to four **.NET-test-pinned** ones, from P3 to **P2**, and from `InvalidData` only to
`DestinationTooSmall` as well: on a non-`Done` return .NET advances `bytesConsumed`
past whitespace to the first non-whitespace character at or after the last completed
quantum boundary, and this port stops at the boundary. One case sits outside that
rule and needs its own decision — `"QQ==QUJD"` with `isFinalBlock` true, where
`DecodeWithWhiteSpaceBlockwise` reverts its counters to `0,0`. No `SR-AUD-*`
identifier; numbering stays frozen at 364.

### Completed Base64Url optional-padding acceptance: ticket #1820

Ticket **#1820** (`REMED-BUFFERS-BASE64URL-OPTIONAL-PADDING`, P2, size S, area
*Buffers*) is **done** and **SR-AUD-082 is now `remediated`** — the last `confirmed`
finding in `Base64Url.hpp`, and the fifth and final repair ticket of the Base64 family
plan ([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket #1815). No new
`SR-AUD-*` identifier; numbering stays frozen at 364, and the index now records
**21 remediated** and **343 confirmed** of 364.

Base64Url's decode table mapped `'='` and `'%'` to `-1` and `decodeCore` rejected
either immediately, so `IsValid` rejected them too and `YQ==` / `YQ%%` were
unreadable. Base64Url omits padding on **output**, and this port still does — but
.NET's decoder and validator deliberately **accept** it:
`Base64UrlDecoderByte.IsValidPadding` is `padChar is EncodingPad or UrlEncodingPad`
and `Base64UrlByteValidatable.IsEncodingPad` is the same test. The port's header
documented only how it encodes and never claimed a stricter decode adaptation.

**The finding predicted a table change; the table did not need one.** Like .NET,
padding is now recognised by a test on the **raw character**, so kDecTable stays the
pure sextet alphabet and a padding character can never be mistaken for a value. Not
changing the table is the more faithful port.

**The grammar** is `Base64UrlByteValidatable.ValidateAndDecodeLength`: with
`remainder` symbols in the trailing incomplete quantum and `padCount` final pads,
padding is valid only when `remainder != 0` and `remainder + padCount <= 4`, at most
two pads, `remainder == 1` never decodable. Two symbols admit one **or** two pads,
three symbols admit exactly one, a complete quantum admits none; whitespace may sit
before, between and after the pads. Padding is also `InvalidData` when `isFinalBlock`
is false — #1818's rule, which .NET's own `DecodingInvalidBytesPadding` asserts here.
`validateCore` gained the same branch in the same change: a validator **stricter**
than its own decoder is as harmful as a more permissive one, because it declares a
decodable input unusable.

**Measured against 62 vectors, every one taken from a named current-.NET test rather
than traced** (`build-probe/1820_defects.cpp`, logs `1820_prefix_defects.log` and
`1820_postfix_defects.log`): **18 of 62 differed before, 0 of 62 after**, with the two
overloads and the two validators agreeing on every line throughout. All 18 were
rejections that should have been acceptances. **Nothing .NET rejects was accepted
before this change, and nothing is now** — padding before the last quantum, more than
two pads, a pad after a complete quantum or a one-symbol remainder, three symbols plus
two pads, data after padding, and noncanonical final bits under padding are all still
`InvalidData`, with the exact cursor .NET reports.

**This is a widening change**: it only adds accepted input, which is why the family
plan deliberately left it unordered against the narrowing tickets #1817–#1819.
Unpadded input of every remainder size decodes exactly as before and the encoder still
emits no padding, both confirmed by a 0..24 round trip.

**One correction to the ticket's own note**: it suggested rewording
`invalidDataMessage()` because it mentions padding despite a Base64Url surface. That
would be a **divergence** — .NET's `Base64Url` throws `FormatException` with
`SR.Format_BadBase64Char`, verbatim the string this port already uses. The message is
left alone.

**Tests: +8 permanent regressions.** `SharpRuntimeTests_Buffers` **504/504** (was
496), and the same 504 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1820_buffers_asan.log`); the probe is clean under the same three
(`build-probe/1820_asan.log`). Repository gate: **0 warnings, 0 errors, 14,033 tests
across 37 executables** (was 14,025). Module graph **41 / 91**.

**Source and ABI consequences: none.**

**The Base64 family is now closed except for two decisions**: **#1821** (the
empty-buffer status divergence in the in-place encoders) and **#1822** (the cursor
reported alongside a non-`Done` status), neither of which carries an `SR-AUD-*`
identifier.

### Completed non-Done decode cursor alignment: ticket #1822

Ticket **#1822** (`REMED-BUFFERS-BASE64-INVALIDDATA-CURSOR`, P2, size S, area
*Buffers*) is **done**. It carries **no `SR-AUD-*` identifier** by design — the audit
numbering stays frozen at 364 and the index counts are unchanged at **21 remediated /
343 confirmed**. It was opened inactive by #1818 with two *traced* instances and
upgraded by #1819 to four **.NET-test-pinned** ones, from P3 to P2, and from
`InvalidData` alone to `DestinationTooSmall` as well.

**The defect.** On a non-`Done` return both `decodeCore`s reported the boundary of the
last **completed** quantum. Current .NET reports the first **non-whitespace** character
at or after that boundary, because it reaches its `InvalidData` and
`DestinationTooSmall` exits through `InvalidDataFallback`, which skips the failing
region's leading whitespace and adds it to `bytesConsumed` before re-entering the
decoder.

**Pinned by .NET's own tests**, not by tracing:
`DecodingWithValidDataBeforeWhiteSpaceSplitFinalQuantum` asserts `bytesConsumed` of
**9, 10 and 15** and then that slicing the input at that cursor leaves exactly
`"AQ\r\nQ="`; `DecodingWithEmbeddedWhiteSpaceIntoSmallDestination_TrailingWhiteSpacesAreConsumed`
asserts `input[consumed] == 'j'` — index **44** of a 48-byte input with a 6-byte
destination — which is what makes this a rule about *every* non-`Done` status rather
than about `InvalidData`.

**The fix** is one `failCursor` per header, updated whenever a non-whitespace character
is read with no quantum pending, applied through a small `fail()` helper at every
`InvalidData`/`DestinationTooSmall` return. Base64Url gets it too, because .NET's
`DecodeFrom` and `InvalidDataFallback` are **one generic helper** shared by both
decoders.

**`NeedMoreData` is deliberately excluded.** .NET returns it from `NeedMoreDataExit`,
which the fallback never runs for, so its cursor stays on the quantum boundary —
`"QUJD QQ"` with `isFinalBlock` false is **4**, not 5. Applying the rule there would
have introduced a new divergence while fixing an old one.

**Measured** over 41 vectors across both types (`build-probe/1822_defects.cpp`, logs
`1822_prefix_defects.log` and `1822_postfix_defects.log`): **9 differed before, 0
after**, with the two overloads agreeing on every line. Re-running #1819's and #1820's
probes against the new rule gives **0 of 27** and **0 of 62** differences, so no
previously verified cursor moved.

**One case is a deliberate deviation**, decided explicitly as the ticket required.
.NET's `DecodeWithWhiteSpaceBlockwise` *reverts* its block counters when non-whitespace
follows a block's padding, reporting `0,0` for `"QQ==QUJD"` while having already
written the byte into the caller's destination. This port keeps reporting what it
actually wrote. The .NET behaviour is pinned by none of its own tests, and reporting
fewer bytes written than were physically written is the worse contract for a caller
that inspects the buffer. Two vectors pin the deviation so that it stays deliberate,
including the invariant `bytesWritten <= bytesConsumed`.

**Tests: +8 permanent regressions** — the three `DecodingWithValidDataBefore…` cursors
plus the remainder-slice assertion, the `DestinationTooSmall` cursor with its decoded
bytes and its untouched sentinel, five failing shapes on both overloads, three
`NeedMoreData` shapes keeping the boundary, and the deviation, for Base64; and the
`InvalidData`, `NeedMoreData` and `DestinationTooSmall` cursors for Base64Url.
`SharpRuntimeTests_Buffers` **512/512** (was 504), and the same 512 under **ASan +
UBSan + LSan with zero reports** (`build-asan/1822_buffers_asan.log`); the probe is
clean under the same three (`build-probe/1822_asan.log`). Repository gate: **0
warnings, 0 errors, 14,041 tests across 37 executables** (was 14,033). Module graph
**41 / 91**.

**No status and no decoded byte changes for any input; every `Done` cursor is
unchanged. Source and ABI consequences: none.**

### Completed in-place encoder empty-buffer decision: ticket #1821

Ticket **#1821** (`REMED-BUFFERS-BASE64-EMPTY-BUFFER-STATUS`, P3, size XS, area
*Buffers*) is **done**, and with it the entire Base64 family. It carries **no
`SR-AUD-*` identifier** by design; the index counts are unchanged at **21 remediated /
343 confirmed** of 364.

.NET's `Base64Helper.EncodeToUtf8InPlace` — the **one** helper its `Base64` and
`Base64Url` in-place encoders share — opens with
`if (buffer.IsEmpty) { bytesWritten = 0; return OperationStatus.Done; }` **before**
`GetMaxEncodedLength(dataLength)` and before the destination-size check. This port had
no such branch.

**Ticket #1815 recorded four diverging shapes; there are eight**, and the four it
missed are the more consequential ones (`build-probe/1821_defects.cpp`, logs
`1821_prefix_defects.log` and `1821_postfix_defects.log`):

| buffer | `dataLength` | Before | .NET / after |
|---|---|---|---|
| empty | 0 | `Done`, 0 | unchanged |
| empty | 1, 5 | `DestinationTooSmall` / `false` | `Done` / `true`, 0 |
| empty | −1, −1000 | **throws `ArgumentOutOfRangeException`** | `Done` / `true`, 0 |
| empty | 1610612734 | **throws `ArgumentOutOfRangeException`** | `Done` / `true`, 0 |

**The decision, and the reasoning against it.** #1815 framed this as a genuine
question: reporting `Done` for a request to encode five bytes into a zero-byte buffer
is arguably the worse contract. It was decided in favour of .NET's behaviour because
(1) `buffer` **is** the source, so a caller cannot act on `DestinationTooSmall` by
supplying a larger destination — that status names a remedy that does not exist, so it
is a different answer, not a more informative one; (2) the input is
self-contradictory, claiming `dataLength` bytes of raw data at the start of a
zero-length buffer, so there is no *correct* answer to give, only a *portable* one; and
(3) the **ordering** matters as much as the branch — without it, an empty buffer with a
negative or over-large `dataLength` **threw** where .NET returns `Done`, and an
exception where the reference implementation succeeds is a harder divergence for ported
code than a wrong status. That half of the divergence had not been recorded before this
ticket measured it.

**A non-empty buffer is untouched**: correct encodes, `DestinationTooSmall`,
`dataLength` 0, and the `ArgumentOutOfRangeException` on a negative or over-large
`dataLength` all behave exactly as before.

**Tests: +5 permanent regressions** across both types — the empty buffer succeeding for
six `dataLength` values with the byte past the span untouched, the short-circuit
running before validation for three invalid `dataLength` values, and the non-empty
buffer's four outcomes pinned unchanged. `SharpRuntimeTests_Buffers` **517/517** (was
512), and the same 517 under **ASan + UBSan + LSan with zero reports**
(`build-asan/1821_buffers_asan.log`); the probe is clean under the same three
(`build-probe/1821_asan.log`). Repository gate: **0 warnings, 0 errors, 14,046 tests
across 37 executables** (was 14,041). Module graph **41 / 91**.

**Source and ABI consequences: none.**

### Autonomous batch handoff, 2026-07-29 (Base64 family closure)

Five tickets completed on `feature/remediation-batch-base64-followup`: **#1818**
(SR-AUD-080, non-final padding), **#1819** (SR-AUD-081, **false positive**), **#1820**
(SR-AUD-082, optional Base64Url padding), **#1822** (the non-`Done` decode cursor, no
`SR-AUD-*`), and **#1821** (the empty-buffer in-place encode decision, no `SR-AUD-*`).

The Base64 family plan ([`docs/Base64FamilyPlan.md`](docs/Base64FamilyPlan.md), ticket
#1815) is **fully executed** and `Base64.hpp`/`Base64Url.hpp` carry no `confirmed`
`SR-AUD-*` finding.

Baselines at batch end: **14,046 tests across 37 executables**, 0 warnings, 0 errors;
findings index **21 remediated / 343 confirmed of 364**; module graph **41 / 91**;
Doxygen **1,941** of the 1,942 ceiling; **9 negative fixtures / 66 sites**; **2 version
seams / 18 specialisations**; selective-component matrix passed.

Four incorrect premises were corrected by appending, never by rewriting: SR-AUD-080
understated its surface, SR-AUD-081's premise is inverted, SR-AUD-082 predicted the
wrong repair and asked for a message change that would have been a divergence, and this
repository's own family plan was wrong in its §2 and §5.

Ready queue: **#1808**, **#1809** (same two headers — plan them together) and **#1813**,
all compatible and needing no approval. Blocked: **#1773** (CNA / mobile-eggbert
migration, untouched) and **#1804**. Next recommended family after those: **CCF-004**,
which needs a #1815-quality plan first.

Full detail, including build directories, the three-job parallelism record and the
`build-asan`/`build-probe` disk accounting, is in `NEXT.md` under
"Autonomous batch handoff, 2026-07-29 (Base64 family closure)".

### Completed text-wrapper input contract plan: ticket #1823

**`P2: scope the text-wrapper input contract before implementing #1808 or #1809`**
(`REMED-IO-TEXT-WRAPPER-CONTRACT-PLAN`, P2, size M, design-only) is recorded in
[`docs/TextWrapperInputContractPlan.md`](docs/TextWrapperInputContractPlan.md). No
production source changed under it. It carries **no `SR-AUD-*` identifier**; the audit
numbering stays frozen at 364, and `SR-AUD-337`/`SR-AUD-338` keep the statuses they had.

Tickets #1808 and #1809 name the same headers and were opened inactive by #1806 with an
explicit instruction to inventory before implementing. This ticket did that inventory
and measured every affected path *before* any production change, with one process per
case under ASan + UBSan + LSan (`build-probe/1823_prefix_defects.cpp`, log
`build-probe/1823_prefix_defects.log`).

**Three of the four premises in the two tickets were understated, and are corrected by
appending rather than by rewriting:**

1. **#1808 said the writer failure "surfaces later as a `NotSupportedException` from
   the first `Write`, or not at all if nothing is ever written."** For `FileStream` it
   surfaces **never, even when data is written**: a `StreamWriter` over a
   `FileAccess::Read` `FileStream` accepts `Write` and `Flush` with no exception, and
   the file is unchanged afterwards (cases 5, 15). `FileStream::Write` checks neither
   `canWrite_` nor the stream state, so `std::fstream::write` sets `badbit` and the
   bytes are dropped in silence. That is data loss, not a late diagnostic, and it is
   now ticket **#1825**.
2. **#1808 assumed one contract for both directions.** The two halves have opposite
   compatibility, and the cause is one line: `Stream::getCanWriteProperty()` **defaults
   to `false`** where .NET's `Stream.CanWrite` is abstract. A `CanRead` guard rejects
   only streams that positively declare themselves unreadable; a `CanWrite` guard also
   rejects every custom stream that implements `Write()` and never overrode the
   property — case 8 writes `"hello"` successfully through exactly such a stream today.
   #1808 is therefore rescoped to the reader half and the writer half becomes blocked
   ticket **#1824**.
3. **#1809 listed two failure modes; there are three, and it named the mildest two.**
   `Console::Write(nullptr)` sets `badbit` on `std::cout` **permanently**, so every
   subsequent console write in the process silently produces nothing (cases 26, 27) —
   no crash, no exception, no message. `StreamWriter` gives an ASan `SEGV` in `strlen`
   (cases 22, 23) and `TextWriter`/`StringWriter` a libstdc++ `std::logic_error` (cases
   20, 21, 24, 25), which is also the wrong hierarchy: `std::`, not `System::`.

**Selected contracts.** Null `const char*` follows .NET's null-**string** rule exactly —
`Write` is a no-op, `WriteLine` writes only the line terminator, nothing ever throws
(`TextWriter.cs:277-283`, `502-509`) — across `TextWriter`, `StreamWriter` and
`System::Console`. `StreamReader` rejects `!getCanReadProperty()` with
`ArgumentException("Stream was not readable.")`, byte-identical to `BinaryReader.cpp:25`
and to `StreamReader.cs:147`, after the null check and before member initialisation.

**Ticket split**: #1809 (compatible) → #1808 (compatible, reader half) → #1825
(compatible, `FileStream` access flags) → #1824 (**blocked on approval**) with #1826
opened inactive for `MemoryStream::getCanReadProperty()` ignoring `isOpen_`.

The family also has **no asynchronous path at all** — `grep -rn "Async"` over all six
text-wrapper headers returns nothing — no synchronized wrapper, no null writer, and no
adapters beyond the four subclasses, all of which is stated in §1.1 rather than assumed.

### Completed null `const char*` text contract: ticket #1809

**`P2: a null const char* is undefined behaviour across the TextWriter Write family`**
(`REMED-IO-TEXTWRITER-NULL-CSTRING`, P2, size S) is complete. It carries **no
`SR-AUD-*` identifier**; the audit did not record it, which is stated plainly rather
than backfilled, and the numbering stays frozen at 364.

Design ticket #1823 decided the contract for the whole family first, because #1809 was
a contract decision and not a guard. Its measurement found **three** structurally
different current failures where the ticket named two, and the unnamed one is the worst:

| Surface | Before | After |
|---|---|---|
| `TextWriter`/`StringWriter` `Write`/`WriteLine(const char*)` | libstdc++ `std::logic_error` — a `std::` exception, not a `System::` one, so `catch (const System::Exception&)` missed it | no-op / line terminator only |
| `StreamWriter::Write`/`WriteLine(const char*)` | AddressSanitizer **SEGV on `0x0`** inside `strlen` | no-op / line terminator only |
| `Console::Write`/`WriteLine(const char*)` | `badbit` on `std::cout` set **permanently**, silently disabling every later console write in the process | no-op / line terminator only, `std::cout` still `good()` |

**The contract is .NET's own rule for a null *string*.** These `const char*` overloads
have no .NET counterpart — they exist only so a string literal binds to the string
overload instead of `Write(bool)` (`TextWriter.hpp:30-37`) — so they take the behaviour
.NET gives the thing they spell: `TextWriter.cs:277-283` makes `Write(string?)` a
no-op, `TextWriter.cs:502-509` makes `WriteLine(string?)` write only the line
terminator, and neither ever throws. A guard that threw `ArgumentNullException` would
have been a divergence, exactly as the ticket's own description warned.

`StreamWriter` needed its own test: it is the one override of the base overload in this
repository, so fixing only `TextWriter` would have left the crash reachable by virtual
dispatch — including through `TextWriter::WriteLine(const char*)`, which forwards to
the virtual `Write`.

Closure evidence: **14 permanent regressions** (10 in `IOStreamTests.cpp`, 4 in
`Batch10ConsoleTests.cpp`) covering null, empty and ordinary input on each of the five
surfaces, the terminator-only `WriteLine` rule, a cross-type assertion that
`StringWriter` and `StreamWriter` answer null identically, an inertness check that a
null write between two ordinary writes leaves them contiguous, and three `Console`
cases asserting `std::cout.good()` afterwards — the sticky-`badbit` regression stated
directly. `SharpRuntimeTests_IO` **562/562** (was 552), `SharpRuntimeTests_Console`
**127/127** (was 123), both clean under ASan + UBSan + LSan
(`build-asan/1809_io_asan.log`, `build-asan/1809_console_asan.log`; activation proven
in `build-asan/1809_asan_activation.log`). Repository gate: 0 warnings, 0 errors,
**14,060 tests across 37 executables**.

Source and ABI consequences: none. No public signature, virtual, vtable, object layout
or exported symbol changed. `TextWriter`'s two overloads are `inline` in a public
header, so an unrebuilt consumer keeps the old inlined body — a stale-inline
consideration, not an ABI break, and benign here because the old body only ever failed
on the input the new one handles.

### Completed StreamReader direction validation: ticket #1808

**`P2: StreamReader does not validate CanRead on the base stream`**
(`REMED-IO-TEXT-WRAPPER-STREAM-DIRECTION`, P2, size S) is complete **for the reader
half**. No `SR-AUD-*` identifier; `SR-AUD-337` stays `confirmed`, `SR-AUD-338` stays
`remediated`, numbering frozen at 364.

`StreamReader(Stream*, bool)` now rejects a stream that exists but declares itself
unreadable, with `ArgumentException("Stream was not readable.")` — message only, no
`paramName` — after the null check. That is `StreamReader.cs:145-148` /
`Argument_StreamNotReadable` exactly, and byte-identical to `BinaryReader.cpp:25`,
which already did it.

**The defect is SR-AUD-338's laundering one level further out.** Measured on a
`FileStream(path, FileMode::Append)` (`FileAccess::Write` only, so `CanRead` is
false): `Read()` returned `-1` and `ReadToEnd()` returned `""`, so a stream that can
never be read was indistinguishable from an empty document
(`build-probe/1823_prefix_defects.log` cases 6 and 7).

**Rescoped to one half, with the reason measured rather than assumed.** #1808 was
opened covering both directions and required an inventory first, so that "the check
cannot reject a stream that is in fact usable". The inventory (#1823,
`docs/TextWrapperInputContractPlan.md` §5) found the two directions have **opposite**
compatibility, from one line — `Stream::getCanWriteProperty()` defaults to `false`
and `getCanReadProperty()` to `true`, where .NET makes both abstract. A `CanRead`
guard rejects only self-declared-unreadable streams; a `CanWrite` guard would also
reject every custom stream that implements `Write()` and never overrode the property,
one of which writes `"hello"` successfully today. The writer half is therefore blocked
ticket **#1824**, awaiting the approval a mandatory downstream migration needs.

Closure evidence: 10 permanent regressions, `SharpRuntimeTests_IO` **572/572** (was
562), clean under ASan + UBSan + LSan (`build-asan/1808_io_asan.log`). Repository
gate: 0 warnings, 0 errors, **14,070 tests across 37 executables**. No public
signature, virtual, vtable, layout or symbol changed.

Two further defects exposed by the same measurement are their own tickets rather than
folded in: **#1825** (`FileStream::Write` silently discards data written to a
read-only handle) and **#1826** (`MemoryStream::getCanReadProperty()` ignores
`isOpen_`).

### Completed FileStream access-flag validation: ticket #1825

**`P1: FileStream::Write silently discards data written to a read-only handle`**
(`REMED-IO-FILESTREAM-ACCESS-FLAGS`, P1, size S) is complete. It carries **no
`SR-AUD-*` identifier**; the audit did not record this defect, which is stated plainly
rather than backfilled, and the numbering stays frozen at 364.

`FileStream::Read`, `Write` and `WriteByte` inspected only `file_.is_open()`, never
`canRead_`/`canWrite_`. This was **data loss, not a late diagnostic**: an
`std::fstream` opened without `std::ios::out` accepts `write()`, sets `badbit` and
returns, and nothing inspected either the flag or the stream state, so the bytes were
dropped with no diagnostic anywhere. All three operations now test the flag **after**
the existing `is_open()` check and **before** the buffer/offset/count validation,
throwing `NotSupportedException` with .NET's `NotSupported_UnreadableStream` /
`_UnwritableStream` messages — the order and the wording of
`Strategies/OSFileStreamStrategy.cs:208-217` and `232-241`, and the same messages
`MemoryStream` and `UnmanagedMemoryStream` already threw for the same condition.
`FileStream` was the last stream in the module that did not.

**A fourth premise corrected, this one in #1825's own text.** The ticket said all three
operations "check only `file_.is_open()`". For `WriteByte` that was too generous: it had
**no validation at all**, so writing a byte to a *closed* `FileStream` was accepted in
silence while the `Write()` sibling beside it already threw
(`build-probe/1825_prefix_defects.log` case 4, a case the ticket did not predict). .NET
has no such gap because `OSFileStreamStrategy.cs:226-227` defines `WriteByte` in terms
of `Write(ReadOnlySpan<byte>)` and inherits both checks.

| Case | Before | After |
|---|---|---|
| `Write` on `FileAccess::Read` | accepted; file still `"seed"` | `NotSupportedException` |
| `WriteByte` on `FileAccess::Read` | accepted; file still `"seed"` | `NotSupportedException` |
| `Read` on `FileMode::Append` | `n=0`, indistinguishable from EOF | `NotSupportedException` |
| `WriteByte` after `Close()` | **accepted in silence** | `ObjectDisposedException` |
| `Write`/`Read` after `Close()` | `ObjectDisposedException` | unchanged |
| the valid read, write and `WriteByte` paths | correct | unchanged, byte-identical |
| closed **and** unwritable | `ObjectDisposedException` | unchanged — pinned by a test |

**Compatible narrowing, needing no approval.** Every newly rejected input already
failed: a write to a read-only handle never reached the file, and a read of a write-only
handle always returned 0. The only change is that the caller is now told.

Closure evidence: 7 permanent regressions, `SharpRuntimeTests_IO` **579/579** (was
572), clean under ASan + UBSan + LSan with activation proven rather than assumed
(`build-probe/1825_postfix_defects.log`, `build-asan/1825_io_asan.log`). Repository
gate: 0 warnings, 0 errors, **14,077 tests across 37 executables**;
`scripts/local_ci_check.sh build` passed. No public signature, virtual, vtable, object
layout or mangled symbol changed.

### Completed ZipArchive mode-range validation: ticket #1813

**`P2: ZipArchive silently accepts an out-of-range ZipArchiveMode value`**
(`REMED-IO-ZIP-INVALID-MODE`, P2, size S) is complete. It carries **no `SR-AUD-*`
identifier**; the audit recorded invalid mode values only as a missing-test note in
`ZipArchive.hpp.audit.md`'s "Other missing assertions" section, never as a finding, and
the numbering stays frozen at 364.

Both constructors tested the mode only with `== Read` / `== Create` / `== Update`, so a
value outside the enumerator set took none of the branches and produced a **zombie
archive**: no reader opened, no stream retained, no entries reported, nothing written
back. `validateZipArchiveMode()` now rejects any such value with
`ArgumentOutOfRangeException("mode")` — `ZipArchive.cs:979`'s
`default: throw new ArgumentOutOfRangeException(nameof(mode))`, verbatim.

**The severity is worse than the ticket said.** #1813 described an archive that "reports
no entries and writes nothing back", which reads as inert. It is not:
`build-probe/1813_prefix_defects.log` **case 9** builds a complete one-entry archive over
a perfectly good `MemoryStream` — `CreateEntry("lost.txt")`, `"DATA"` written to the entry
stream, `Dispose()` — with every step accepted, and the stream receives **0 bytes**. That
is the same silent-data-loss shape ticket #1812 removed from the null-stream path, reached
here with a valid stream. **Case 14** shows why `CreateEntry` does not catch it: that
method rejects only `mode == Read`.

**This defect is invisible to a sanitizer, which is worth recording.** `enum class
ZipArchiveMode` has the implicit fixed underlying type `int`, so holding 42, −1, `INT_MAX`
or `INT_MIN` in it is well-formed C++ with a well-defined value, not undefined behaviour.
UBSan reported nothing for cases 1–5 (measured, not assumed). Only an explicit range check
finds this class of defect — a useful counterexample to the batch's default sanitizer
strategy.

| Case | Before | After |
|---|---|---|
| 1–5 — stream ctor with 42, 3, −1, `INT_MAX`, `INT_MIN` | all construct successfully | `ArgumentOutOfRangeException("mode")` |
| 6 — **path** ctor with 42 | constructs successfully | same |
| 7 — `ZipFile::Open(path, 42)` | constructs successfully | same, inherited |
| 8 — `nullptr` **and** mode 42 | `ArgumentNullException("stream")` | unchanged — order pinned |
| 9 — mode 42 + `CreateEntry` + write + `Dispose` | **accepted; 0 bytes delivered** | cannot start |
| 10, 11, 12 — the valid Create, Read and Update paths | 146 bytes, 1 entry, 2 entries | unchanged, byte-identical |
| 13, 14 — destructor and observable state on mode 42 | zombie archive survives | cannot be constructed |

**Inventory result for the acceptance criterion.** `ZipFile::Open` is a bare forwarder to
the path constructor (`ZipFile.cpp:17`), so it is fixed **transitively** rather than
needing its own guard — and that is pinned by its own test, so a future refactor that
stops forwarding cannot silently lose the check. Validation order is .NET's in both
overloads: after the null-stream check for the `Stream*` ctor (`ZipArchive.cs:135`), and
before the path is stored or the file system touched for the path ctor
(`ZipFile.Create.cs:473-479`, which rejects the range before opening its `FileStream`).

**Explicitly excluded, and now blocked ticket #1827.** `ValidateMode` has a second half
(`ZipArchive.cs:962-975`) that rejects a stream whose *capabilities* contradict the mode.
This port validates none of it, and the guard cannot be added compatibly for the same
one-line reason that blocks #1824: `System::IO::Stream::getCanWriteProperty()` **defaults
to `false`** (`Stream.hpp:62`) where .NET's `Stream.CanWrite` is abstract, so a Create-mode
capability guard would reject every custom stream that implements `Write()` without
overriding the property. #1827 is opened `blocked`, with a note that it and #1824 share a
root cause and may deserve one design covering the `Stream.hpp` default itself.

**Compatible narrowing, needing no approval.** Every newly rejected input already
produced an unusable archive; no in-range value changed behaviour.

Closure evidence: 14 permanent regressions, `ZipArchiveTests` **42/42**,
`SharpRuntimeIntegrationTests` **857/857** (was 843), clean under ASan + UBSan + LSan with
0 reports (`build-probe/1813_postfix_defects.log`,
`build-asan/1813_integration_asan.log`). Repository gate: 0 warnings, 0 errors, **14,091
tests across 37 executables**; `scripts/local_ci_check.sh build` passed. No public
signature, virtual, vtable, object layout or mangled symbol changed.

### Completed MemoryStream disposed-CanRead fix: ticket #1826

**`P3: MemoryStream::getCanReadProperty() ignores the disposed state`**
(`REMED-IO-MEMORYSTREAM-CANREAD-DISPOSED`, P3, size XS) is complete. **No `SR-AUD-*`
identifier**; numbering stays frozen at 364.

`MemoryStream` did not override `getCanReadProperty()` at all, inheriting `Stream`'s base
default of `true`, so it kept claiming to be readable after `Close()`. It now returns
`isOpen_` (`MemoryStream.cs:99`). `getCanWriteProperty()` is deliberately left returning
`writable_` (`MemoryStream.cs:103`); the resulting asymmetry is **.NET's own** and is
documented in the header and pinned by a test citing both line numbers, so it cannot later
be normalised into a divergence.

**The predicted interaction, confirmed.** A `StreamReader` over a closed `MemoryStream` was
accepted at construction and failed only on first read (`prefix` case 5); it now throws
`ArgumentException("Stream was not readable.")` from the constructor, where .NET reports it.
Ticket #1808's guard existed for exactly this and the property had been defeating it.

**Inventory result.** Of nine `Stream` subclasses, `MemoryStream` was the only one
overriding `CanWrite`/`CanSeek` but not `CanRead`. `BufferedStream` folds `closed_` in and
delegates to its inner stream; `UnmanagedMemoryStream` folds `isOpen_` in; `NetworkStream`
folds `fd_ >= 0` in; `FileStream` folds neither, unobservably after #1825.

**Separate defect found → inactive ticket #1828.** The three zlib wrappers answer
`CanRead`/`CanWrite` from `mode_` alone where `DeflateStream.cs:171-195` folds in both the
disposed state and the inner stream's capability (cases 7–9). Case 10 records that #1826
made this **visible** rather than causing it: `CanRead=1 inner-CanRead=1` before (accidentally
consistent), `CanRead=1 inner-CanRead=0` after (a contradiction). #1828 is blocked because
its delegation half meets `Stream.hpp:62`'s default-`false` `getCanWriteProperty()` — the
root cause it now shares with #1824 and #1827.

Closure evidence: 7 permanent regressions, `MemoryStreamTests` 64/64,
`SharpRuntimeTests_IO` **586/586** (was 579), clean under ASan + UBSan + LSan with 0 reports.
Repository gate: 0 warnings, 0 errors, **14,098 tests across 37 executables**;
`scripts/local_ci_check.sh build` passed. No member added, layout unchanged, no vtable slot
added.

### Completed CCF-004 family plan: ticket #1829

**`P1: plan the CCF-004 defined-arithmetic family before implementing any of its eight
findings`** (`REMED-CORE-CCF004-PLAN`, P1, size M, design-only) is recorded in
[`docs/DefinedArithmeticBoundaryPlan.md`](docs/DefinedArithmeticBoundaryPlan.md). No
production source changed under it. **No `SR-AUD-*` identifier**; all eight members keep
status `confirmed` and numbering stays frozen at 364.

All eight members were re-reproduced under UBSan on 2026-07-29 rather than taken from the
audit's wording (`build-probe/1829_ccf004_survey.cpp`, 16 cases, one process each,
`-fno-sanitize-recover`). Every one still reproduces.

**Three survey findings that change the work:**

1. **The members split three ways** — 6 defined-wrap sites whose current result is
   already the intended value and whose repair changes nothing observable; 1
   validate-first ordering fix; and 2 that produce a **wrong answer** today
   (`TimeSpan::TryParse` returns `parsed=1` with `ticks=-7695280436664713216` for a
   positive input, and the `DateOnly` arithmetic). Only the last two need a compatibility
   argument, and it is the one already accepted for #1817/#1818/#1825.
2. **SR-AUD-060 is seven sites, not four** — the overflow cascades into `jdnToDate` at
   `DateOnly.cpp:35/37/39`, so cases 8 and 9 each report four UB operations.
3. **A methodology trap** — the first survey run called SR-AUD-049, SR-AUD-060 and
   SR-AUD-008 already fixed, which is false: a probe linked against `build/` cannot see
   any `.cpp`-side site, and `-O1` constant-folds an inlined header overflow so it emits
   no check. Link against `build-asan/` at `-O0`.

**No new shared infrastructure is needed.** .NET's idiom (`DateOnly.cs:73-81`, `:121-132`)
already exists correctly at `ReadOnlyMemory.hpp:120-131`; each remaining application is a
local one-line change, and a proposed `SafeArithmetic` helper should be rejected.

Implementation split: **#1830–#1837**, all `todo`, all compatible, none requiring
approval. #1836 and #1837 should not be taken first.


### Autonomous batch handoff, 2026-07-29 (text/IO remediation and the CCF-004 family plan)

Six tickets on `feature/remediation-batch-text-io-ccf004`: **#1825** (FileStream access
flags), **#1813** (ZipArchive mode range), **#1826** (MemoryStream disposed CanRead),
**#1829** (CCF-004 family plan, design-only), **#1830** (Index/Range defined arithmetic,
**SR-AUD-057 remediated**) and **#1832** (IntPtr defined wrap, **SR-AUD-025 remediated**).

Baselines, all verified rather than carried forward: repository gate **14,113 tests across
37 executables**, 0 warnings; audit **23 remediated / 341 confirmed / 364**; module graph
41 / 91; canonical Doxygen **1,941** against the 1,942 ceiling; negative fixtures 9 / 66;
version seams 2 / 18.

Five premises were corrected by measurement, including two in the batch's own documents:
SR-AUD-060 is seven sites not four, SR-AUD-057 is two sites not one, and #1829's own first
survey run wrongly reported three of eight members as already fixed. The two methodology
rules that follow — link probes against `build-asan/` at `-O0`, and enumerate sites with
the *recovering* sanitizer build — are recorded in
`docs/DefinedArithmeticBoundaryPlan.md` §3 and §12.

Ready queue: **#1831**, **#1833**, **#1834**, **#1835** (all compatible, no approval), then
the two class C tickets **#1836** and **#1837** last. Blocked: **#1773** (untouched),
**#1804**, and **#1824** / **#1827** / **#1828**, which share one root cause —
`Stream.hpp:62`'s default-`false` `getCanWriteProperty()` — and are better served by one
design covering that line than by four per-type guards.

Full detail, including build directories, the three-job parallelism record and the
`build-probe` disk accounting, is in `NEXT.md` under "CONTEXT-REFRESH handoff — 2026-07-29,
text/IO + CCF-004 batch".
