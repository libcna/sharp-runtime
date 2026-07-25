<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# NEXT.md

*Last verified: 2026-07-25. Branch: `feature/work`. The P0 component-boundary
repair, three P1 parity repairs, P1 portability revalidation, and eight bounded
P2 API slices are complete: 41 physical modules, 90 production dependency
edges, and 12,632 tests across 37 executables.*

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
- The full native baseline is a warning-free build with 12,632 passing tests
  across 36 component executables and one integration executable.
- `TaskT<TResult>::ContinueWith` now supports both action and result-producing
  callbacks. It runs inline on completion; `NotOn*` and `OnlyOn*` filter the
  antecedent state, while scheduler and parent-task options remain no-ops.
- `XmlWriter::WriteWhitespace` validates XML whitespace and `XText::WriteTo`
  selects it only for text directly under an `XDocument`.
- `BinaryReader` decodes UTF-8 through `ReadChar`, `ReadChars`, and
  `Read(char[])`, retaining a pending low surrogate across calls. Batch reads
  work on non-seekable streams, return partial data at clean EOF, and reject
  truncated or malformed UTF-8. Seekable `PeekChar` restores both stream and
  decoder state; it deliberately throws on non-seekable streams.
- `ImmutableList<T>` supports all three `CopyTo` overloads using a fixed-size
  `std::vector` destination. Its bounds checks distinguish invalid source
  ranges from an undersized destination and avoid signed-overflow-prone sums.
- `ImmutableList<T>::Sort(Comparison<T>)` returns an independently backed
  custom-ordered result using the established signed comparison delegate and
  rejects an empty delegate.
- MinGW-w64 GCC 14-win32/CMake 3.31.6 and Emscripten 5.0.7/CMake 3.31.6 both
  compile the post-modular `All` graph and selective `Text.Json` libraries.
  This is compile-only evidence: cross tests were deliberately disabled.
- Focused TSan scenarios for concurrent collections, `ConditionalWeakTable`,
  generic task continuations, and `TaskExtensions::Unwrap` are clean; matching
  ASan/LSan ownership scenarios, including 100 continuation teardowns, pass.

The local `plan.sqlite3` snapshot contains 16,201 classified `task` rows and
1,750 completed tickets. Ticket #1737 records the completed P0 split, tickets
#1738/#1739 the MemoryStream and generic-continuation repairs, ticket #1740 the
XML whitespace repair, #1741 the completed cross-build revalidation and
`WebProxy` portability fix, #1742 focused sanitizer evidence, and #1743 the
`ImmutableList<T>` predicate-query slice. Ticket #1744 records seekable
`BinaryReader::PeekChar`, #1745 `ImmutableList<T>::Sort`/`Reverse`, and #1746
`ImmutableList<T>::GetRange`, #1747 `ImmutableList<T>::ConvertAll`, #1748 the
UTF-8 `BinaryReader` batch-character APIs, #1749 `ImmutableList<T>` copying,
and #1750 its custom comparison sort. The database is git-ignored and is not
part of a fresh clone.

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

## P1 completion: generic task continuations

`TaskT<TResult>` now has `ContinueWith` overloads for action callbacks and
result-producing callbacks. Continuations receive a completed antecedent,
propagate their own result, fault when their callback throws, cancel when
predicate options exclude the antecedent outcome, and may be chained. Pending
callbacks retain only a weak antecedent state, and regression coverage verifies
success, fault, cancellation, filtering, chaining, and post-completion
capture release.

## P1 completion: document-level XML whitespace

`XmlWriter::WriteWhitespace` now accepts only XML whitespace (space, tab, CR,
and LF), while `XmlTextWriter` forwards the same API. `XText::WriteTo` uses it
only for direct children of `XDocument`, as .NET does; element text continues
through `WriteString`. Regression coverage guards input validation and both
serialization paths.

## P1 validation: concurrent ownership sanitizers

Focused ThreadSanitizer runs exercised `ConcurrentBag`, bounded
`BlockingCollection`, `ConditionalWeakTable`, concurrent generic-task
continuation registration, and `TaskExtensions::Unwrap` without race reports.
AddressSanitizer/LeakSanitizer passed the same ownership scenario plus a
100-iteration continuation capture-release check. These are focused native
validation runs, not a cross-platform runtime test matrix.

## P1 completion: post-modular cross-build revalidation

Ticket #1741 revalidated library-only `All` and selective `Text.Json` graphs
with MinGW-w64 GCC 14-win32/CMake 3.31.6 and Emscripten 5.0.7/CMake 3.31.6.
Emscripten exposed a `WebProxy` DNS-comparison helper whose only call site is
excluded on that platform; its definition is now excluded too, preserving the
`-Werror` build. The native `WebProxyTests` regression filter passes 11 tests.
Cross-platform runtime tests were deliberately not built or run.

## P2 completion: `ImmutableList<T>` predicate queries

`ImmutableList<T>` now provides `ForEach`, `Exists`, `Find`, `FindAll`,
`FindIndex`, `FindLast`, `FindLastIndex`, and `TrueForAll`. Query methods
preserve the source list, return .NET-compatible default/index results when no
element matches, and reject an empty delegate with `ArgumentNullException`.
Four focused tests cover ordering, immutability, empty-list semantics, and
delegate validation.

## P2 completion: seekable `BinaryReader::PeekChar`

`PeekChar` records a seekable stream position, decodes one UTF-8 character,
and restores the position before returning it. It returns `-1` at EOF and also
restores the position before propagating invalid or truncated UTF-8 errors.
Without a general character decoder buffer the port cannot implement that
contract on non-seekable streams, so it throws `NotSupportedException` there.
Five regressions cover UTF-8, EOF, invalid/truncated input, and the explicit
non-seekable limitation.

## P2 completion: `BinaryReader` batch character APIs

`ReadChars(int)` and `Read(char[], offset, count)` now decode the default
UTF-8 stream into UTF-16 code units. They return a partial result at clean EOF
but propagate truncated UTF-8, preserve a pending low surrogate across
`ReadChar`/batch-call boundaries, and work on non-seekable streams. Six
regressions cover mixed UTF-8, buffer offsets, clean and truncated EOF,
supplementary characters, pending-state peeking, and argument validation.

## P2 completion: `ImmutableList<T>` ordering

`ImmutableList<T>::Sort()` and `Reverse()` now return independently backed,
reordered lists using the default `T::operator<` and full-list order. Both
leave the source unchanged; range and custom-comparer overloads remain
deferred. Three regressions cover ordering, immutability, and empty/singleton
lists.

## P2 completion: `ImmutableList<T>::GetRange`

`GetRange(index, count)` returns an independently backed, ordered immutable
slice. It accepts zero-length ranges at either valid boundary and reuses the
same `ArgumentOutOfRangeException` checks as `RemoveRange`. Three regressions
cover slice content, source immutability, boundary empties, and invalid ranges.

## P2 completion: `ImmutableList<T>::ConvertAll`

`ConvertAll<TOutput>` converts each source value in order into an independently
backed immutable list. It preserves an empty source and rejects an empty
converter with `ArgumentNullException`; three regressions cover those cases.

## P2 completion: `ImmutableList<T>::CopyTo`

All three `CopyTo` overloads now copy to a fixed-size `std::vector<T>`
destination: full list, destination offset, and source/destination range.
They preserve order and source immutability, allow valid empty end ranges,
and distinguish invalid indices/ranges from an undersized destination. Five
regressions cover each overload, boundary behavior, and validation.

## P2 completion: `ImmutableList<T>::Sort(Comparison<T>)`

`Sort` now accepts the project's signed `Comparison<T>` delegate convention:
negative is before, zero equivalent, positive after. It returns an
independently backed result and rejects an empty delegate with
`ArgumentNullException`; two regressions cover custom ordering and validation.

## Recommended next bounded tasks

All currently planned P1 work is complete. Choose one consumer-driven P2
slice, create a ticket, and keep the changes isolated:

1. **`ImmutableList<T>` breadth.** Select one real consumer-needed group from
   builder support, range operations, or remaining comparer overloads.
2. **Other documented partial surfaces.** Examples include `BigInteger`
   bitwise operations, fuller UTF-7 behavior, and wider debugger/process/XML
   surfaces.
3. **Doxygen warning baseline.** Establish a reproducible warning count, then
   keep touched public APIs from increasing it; do not do a mass comment-only
   rewrite.

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
