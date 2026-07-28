<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# NEXT.md

*Last verified: 2026-07-28. Branch: `feature/remediation-coll-sortedset-count-race`.
The P0
component-boundary repair, three P1 parity repairs, P1 portability revalidation, and
twenty-two bounded P2 API slices are complete: 41 physical modules, 90 production
dependency edges, and 13,127 tests across 37 executables. The repository-wide,
evidence-only audit is complete under `audit/` (local ticket #1766). Remediation
tickets #1767 (enumerator lifecycle), #1768 (LinkedListNode lifetime design),
#1769 (LinkedListNode lifetime implementation), #1770 (raw `ICollection::CopyTo`
design), #1771 (raw `ICollection::CopyTo` implementation), #1774 (raw
`ICollection::CopyTo` zero-length-destination correction), #1775
(`Hashtable` `IDictionary` key/view contracts), #1776
(`ArgumentNullException` duplicate parameter suffix), #1777 (typed
`CopyTo` doc-comment sync), #1778 (`ConcurrentDictionary::AddOrUpdate`
compare-and-retry), #1779 (`ReadOnlyDictionary::Empty` const-reference
design), #1780 (`ReadOnlyDictionary::Empty` const-reference
implementation), #1781 (Doxygen 1,942-vs-1,944 count reconciliation), and
#1782 (`SortedSet::GetViewBetween` live-view design), and #1783
(`SortedSet::GetViewBetween` live-view implementation) are
complete; the
node contract is recorded in
[`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md), the copy
boundary in [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md)
(see its section 22 for the #1774 correction), and the mutable-singleton
contract in
[`docs/ReadOnlyDictionaryEmptyDesign.md`](docs/ReadOnlyDictionaryEmptyDesign.md)
(see its section 21 for the #1780 implementation), with consumer guidance in
[`docs/Migration-ICollectionCopyTo.md`](docs/Migration-ICollectionCopyTo.md).
Ticket #1775 (`REMED-COLL-HASHTABLE-VIEWS`, P1, size M) restored the
`Hashtable` `IDictionary` key and view contracts for SR-AUD-363, which is now
`remediated`. Ticket #1776 (`REMED-CORE-ARGNULL-MESSAGE`, P2, size XS) then
corrected `ArgumentNullException(paramName)` so its `(Parameter 'x')` suffix is
appended exactly once instead of twice. Ticket #1777
(`REMED-COLL-COPYTO-DOC-SYNC`, P3, size XS) then corrected the four typed
`CopyTo` doc-comments that still cited ticket #1771's superseded
null-destination rule so they state the rule ticket #1774 corrected instead.
Ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`, P2, size S) then made
`ConcurrentDictionary::AddOrUpdate` retry against the observed value instead
of unconditionally overwriting a concurrent intervening write, remediating
SR-AUD-360. Ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`, P2, size S,
design-only) then recorded the selected fix for SR-AUD-359 — changing
`ReadOnlyDictionary<K,V>::Empty()`'s return type from
`ReadOnlyDictionary<K,V>&` to `const ReadOnlyDictionary<K,V>&` — without
making the change, since it is a public signature change requiring the same
explicit approval `ICollection::CopyTo`'s removal needed. The user then
explicitly approved that change, and implementation ticket #1780
(`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS) landed it, remediating
SR-AUD-359. Ticket #1781 (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, P3, size XS,
planning-integrity only, no `SR-AUD-*` finding) then re-verified the
Doxygen warning count #1779 had flagged as possibly drifted, reconfirmed
1,942 (the documented ceiling, not 1,944), and corrected the two documents
that still stated 1,944 as a measured fact. Design-only ticket #1782
(`REMED-COLL-SORTEDSET-VIEW-DESIGN`, P2, size M) then answered SR-AUD-361 —
`SortedSet<T>::GetViewBetween` returns a detached snapshot instead of .NET's
live write-through `TreeSubSet` — selecting a shared reference-counted `State`
plus optional bounds so one `SortedSet<T>` is either an owning set or a live
bounded view, recorded in
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md).
The user then explicitly approved all three consequences, and
implementation ticket **#1783** (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L)
landed the design on 2026-07-28, so **SR-AUD-361 is now `remediated`**:
`GetViewBetween` returns a live, inclusive-bounds-enforcing, bidirectionally
write-through handle onto the same tree, and the four adjacent defects #1782
measured inside that surface are closed with it. See the "Completed SortedSet
live-view implementation" section below for the exact source, semantic, symbol,
and layout consequences.
Follow-up ticket **#1784** (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`, P1, size S)
then removed the one defect #1783 introduced and honestly reported against
itself: its lazy per-view Count cache lived in two plain `mutable intcs` fields
written by the `const` `getCountProperty()`, so concurrent read-only Count on a
single view object was a ThreadSanitizer-confirmed C++ data race. The fields are
now atomic with a release/acquire publication protocol, restoring the pre-#1783
property that concurrent readers do not race, with object layout, symbols,
public signatures, and every Count value unchanged. **SR-AUD-361 stays
`remediated`** and #1784 carries no `SR-AUD-*` identifier; see "Completed
SortedSet Count-cache race correction" below.
Ticket **#1786** (`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, size S) then
repaired the mutation counter those three tickets all relied on and none had
changed. `SortedSet<T>::State::version` was an `int32_t` incremented without
bound and compared only for **equality**, which made `++version` at `INTCS_MAX`
signed-integer overflow -- undefined behaviour, reproduced under UBSan -- and let
a wrapped counter silently revalidate a stale iterator, silently revalidate a
stale cached view `Count`, and, in a fourth defect the ticket's own description
did not list, make a view that had **never** computed its Count read its `-1`
sentinel as a warm cache. The counter and the `Iterator` snapshot are now 64-bit
unsigned, and the Count cache's 32-bit tag is stored biased by one and compared
widened so it identifies a counter value exactly. All four defects predate
#1783; **SR-AUD-361 stays `remediated`** and #1786 carries no `SR-AUD-*`
identifier. Public signatures, mangled symbols, `sizeof`, `alignof`, and every
member offset are unchanged, so no consumer rebuild is needed on its account and
no approval was required; the contract is recorded in
[`docs/SortedSetVersioningDesign.md`](docs/SortedSetVersioningDesign.md). See
"Completed SortedSet mutation-counter repair" below.
**No ticket is active.** #1771 removed the
pure virtual `CopyTo(void*, intcs)` from
`System::Collections::ICollection` under explicit user approval, so this is a
source- and ABI-breaking release for downstream consumers, which must rebuild.
#1774 then corrected #1771's validation rule so that a null-pointer destination
with a zero length (e.g. `ObjectSpan{nullptr, 0}` or a default-constructed empty
`std::vector<std::any>`) is a valid empty destination; only a null pointer
paired with a positive length is still rejected. This is a behavioral
relaxation, not a further source or ABI break. #1780's `Empty()` return-type
change is source-breaking only for the exact hazardous explicit-non-`const`-
reference/assignment pattern (confirmed absent everywhere in this repository)
and not an ABI break (header-only `INTERFACE` component, byte-identical
mangled symbol before/after); see the "Completed ReadOnlyDictionary::Empty
implementation" section below.

**Correction (ticket #1776, 2026-07-27):** the #1775-era notes below and in
`plan.md`/`audit/AUDIT_FINAL_REPORT.md` describing ticket #1776 as a fresh,
post-audit discovery with no covering `SR-AUD-*` identifier were inaccurate.
`ArgumentNullException(paramName)`'s duplicate parameter suffix and its
null-`const char*` crash were already recorded as `confirmed` findings
SR-AUD-090 and SR-AUD-089 respectively, within the frozen SR-AUD-001..364
range, before ticket #1775 opened #1776. Ticket #1776's fix resolves both;
they are now `remediated` in `audit/AUDIT_FINDINGS_INDEX.md` and
`audit/modules/core/include/System/ArgumentNullException.hpp.audit.md`. This
note is left in place rather than silently correcting the original text below,
per this repository's practice of preserving historical audit narrative.*

This is the cold-start handoff for the next working session. Keep it focused
on verified facts, remaining bounded work, and commands needed to resume.
Historical session detail belongs in git history and `plan.sqlite3`.

## Planning audit and 500-hour remediation tranche

The 2026-07-27 post-audit planning review confirms that the evidence inventory
is usable, but the remediation backlog has not yet been converted into an
executable ticket queue. This section is the authoritative next-session
ordering and supersedes the still-ambiguous choice near the end of the older
handoff text below.

### Independently rechecked state

- The configured native build completed successfully with no compiler
  diagnostics.
- The build currently exposes 12,921 GoogleTest cases across 37 executables.
  `SharpRuntimeTests_Collections_Core` passed 1,662/1,662 after ticket #1774
  (was 1,612/1,612 after #1771).
- Database consistency, component-boundary validation (41 physical modules,
  90 production dependency edges), generated-catalogue freshness, and
  `git diff --check` passed.
- The audit contains all 1,748 frozen-scope mirrored reports when hidden paths
  such as `audit/.github/` are included. The findings index contains every
  identifier from SR-AUD-001 through SR-AUD-364 exactly once, with no duplicate
  or missing identifier, and every referenced report path exists.
- The open inventory is 360 findings: 88 high, 261 medium, and 11 low.
  SR-AUD-356, SR-AUD-357, SR-AUD-358, and SR-AUD-364 are the four remediated
  findings.
- Ticket #1774's repository gate is the latest complete one: a warning-free full
  build and 12,921/12,921 tests across 37 executables. The last
  network-permitted `scripts/local_ci_check.sh build` run, which additionally
  exercised the six local-server `Net.Http` cases, was ticket #1769's
  12,743/12,743.
- The worktree was clean after the checks. No ticket or production change was
  created by the planning review.

### Planning/documentation defects to repair

These are planning-integrity tasks, not newly classified runtime findings:

1. ~~`plan.sqlite3` has 1,767 tickets and all are `done`; there is no `todo` or
   `doing` ticket from which an autonomous session can resume.~~ Repaired:
   design ticket #1768 and implementation ticket #1769 now carry the active
   remediation queue.
2. ~~`CLAUDE.md` and `README.md` still state the old 12,681-test floor although
   the current measured floor is 12,694.~~ Repaired under ticket #1769, which
   raised the measured floor to 12,743 and synchronized both documents.
3. `CLAUDE.md` says that only `feature/work` may be pushed, while the completed
   audit/remediation workflow uses dedicated `feature/audit` and
   `feature/remediation-*` branches. Clarify whether bounded branches are
   local-only and land through `feature/work`, or update the branch policy
   before any future push.
4. `plan.md` still describes the project as being primarily in a
   consumer-driven expansion phase. Post-audit remediation is now the active
   priority; optional P2 breadth must remain behind confirmed safety defects.
5. This file calls itself a concise cold-start handoff but has grown to more
   than 1,300 lines and duplicates most of `AUDIT_FINDINGS_INDEX.md`. A later
   documentation-only ticket should reduce it to current state, active queue,
   validation commands, and links to authoritative audit history.
6. The twelve index entries SR-AUD-286 through SR-AUD-297 contain Markdown
   fragments for headings that do not exist in their owning Text reports.
   Either promote the report bullets to matching headings or remove the
   fragments while preserving the report links.
7. `AUDIT_PROGRESS.md` says there are two open documented-adaptation questions
   but does not give their identifiers or owning reports. Identify them or
   remove the unsupported roll-up.
8. The inventory has severity but no explicit severity rubric, compatibility
   risk, dependency, or effort field. Each remediation ticket must add those
   planning dimensions rather than treating every same-severity finding as
   interchangeable.

### Completed LinkedListNode remediation: tickets #1768 and #1769

**`P0: Define LinkedListNode detached lifetime contract`**
(`REMED-COLL-LINKED-NODE-DESIGN`, SR-AUD-357 / CCF-019, estimated 8 hours,
size S) is recorded in
[`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md). It answers
all fourteen required questions against the local current-.NET
`LinkedList.cs`/`Strings.resx` sources rather than from memory, and no
production code changed under it.

Selected representation: **independently allocated, reference-counted node
objects**, replacing the copyable raw `std::list<T>` pointer/iterator pair.
`LinkedListNode<T>` becomes a handle over a shared node object with three
states — null handle, detached node (owner cleared, value retained), attached
node. `Remove`, `Clear`, removal through another copied handle, and destruction
of the owning `LinkedList<T>` all produce the detached state instead of a
dangling iterator; the destructor performs the same detaching walk as `Clear`.
Detached nodes can be reattached through the four newly added existing-node
insertion overloads, and `validateNewNode` rejects an already-attached node
with the exact .NET message. `begin()`/`end()` migrate to a bidirectional
`LinkedList<T>::iterator` with the same range-`for`, `std::ranges`, and
invalidation contract. No public member is removed or renamed, so this is not a
broad public API break.

The design was compatible, so ticket #1769 (`REMED-COLL-LINKED-NODE`, size L)
implemented it and is complete. `SR-AUD-357` is now `remediated` and CCF-019 is
partially remediated; its JsonNode (SR-AUD-327) and XML LINQ (SR-AUD-333)
members stay open by design. Closure evidence:

- 49 permanent regressions in `LinkedListNodeLifetimeTests.cpp` covering the
  null/detached/attached matrix, copied handles, removal through either handle,
  `Clear`, owner destruction, reattachment, duplicate and cross-list
  attachment, first/middle/last/single-node cases, list and handle copy/move,
  iteration, and the exact exception types and messages;
- `SharpRuntimeTests_Collections_Core` 1,484/1,484;
- the local `build-probe-linkednode` ASan/UBSan/LeakSanitizer reproduction of
  the audit's use-after-free: `failures=0`, no diagnostic, including a
  200,000-node teardown;
- `test/consumer/collections_linked_list.cpp` compiled `-Werror` against only
  `SharpRuntime::Collections.Core` and run successfully;
- network-permitted `scripts/local_ci_check.sh build`: 12,743/12,743 tests
  across 37 executables, zero warnings/errors;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, `git diff --check` clean, Doxygen 1,941/1,942 with no
  new warning from the touched headers.

### Completed raw-CopyTo design: ticket #1770

**`P0: Define safe typed ICollection CopyTo boundary`**
(`REMED-COLL-COPYTO-DESIGN`, SR-AUD-358 / CCF-020, size S) is recorded in
[`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md). It answers
all twenty required decisions against the local current-.NET `ICollection.cs`,
`ArrayList.cs`, `Queue.cs`, `Stack.cs`, `Hashtable.cs`,
`ListDictionaryInternal.cs`, `Array.cs`, and `Strings.resx` sources rather than
from memory, and no production or test source changed under it. SR-AUD-358 stayed
`confirmed` until implementation ticket #1771 closed it (see below).

Two facts beyond the audit summary were established by direct probe:

- The six `ICollection` implementations disagree on the destination element type
  (`std::any*`, `void**`, `DictionaryEntry*` — sizes 16/8/32), so a caller
  holding an `ICollection*` cannot allocate a correct destination even in
  principle. A per-collection bounds patch cannot close the finding.
- An element-type mismatch through the interface produces **no crash**:
  `Hashtable::CopyTo` into `std::vector<void*>` storage is caught only by
  LeakSanitizer (32-byte direct leak). The other three scenarios reproduce the
  recorded ASan/UBSan aborts (null-destination SEGV, undersized
  heap-buffer-overflow read, negative-index heap-buffer-overflow write).

Selected: a length-aware, statically typed `System::Span<std::any>` destination
behind a **non-virtual interface** — public validating `CopyTo` overloads on
`ICollection` plus one protected `copyToCore` hook per implementation, so
capacity and element type are validated exactly once before any implementation
writes. Concrete collections keep typed `std::vector<void*>` /
`std::vector<DictionaryEntry>` overloads. `CopyTo(void*, intcs)` leaves the
virtual interface and remains briefly as a deprecated, never-writing shim.
.NET's rank, non-zero-lower-bound, and element-type-mismatch diagnostics are
intentionally unsupported: they need a runtime `Array` object and a working
`System::Type`, both permanently out of scope, so **no prerequisite
array/reflection ticket is created**. No new dependency edge: `Span` and the
exception headers are Core.Base, already `Collections.Core`'s only public
dependency.

Seven repository-local probes back the design (commands and output in section 17
of the design document): virtual templates are ill-formed; removal of the legacy
overload cannot silently misbind; the full prototype builds `-Werror` and runs
clean under ASan + UBSan + LeakSanitizer with `failures=0`; derived-class name
hiding makes `using ICollection::CopyTo;` mandatory; the current boundary is
still unsafe; the eight affected public headers compile standalone against
`Collections.Core` + `Core.Base`; and a retained `[[deprecated]]` overload is a
compile error under the repository's own `-Werror` policy.

### Completed raw-CopyTo implementation: ticket #1771

**Approved and done.** The user explicitly approved the public source- and
ABI-breaking change on 2026-07-27, including the instruction **not** to retain a
compatibility overload, so implementation ticket **#1771**
(`REMED-COLL-COPYTO`, P0, size M) landed the boundary. SR-AUD-358 is now
`remediated` and CCF-020 is closed; the index records 360 open findings and four
`remediated`.

`virtual void CopyTo(void* array, intcs index) = 0;` is **removed** from
`System::Collections::ICollection`. The public surface is now non-virtual
`CopyTo(ObjectSpan, intcs)` and `CopyTo(std::vector<std::any>&, intcs)` over one
protected pure virtual `copyToCore(ObjectSpan, intcs)` hook, with
`detail::requireValidCopyDestination` as the single validation site shared by
every implementation and by the typed `std::vector<void*>` /
`std::vector<DictionaryEntry>` overloads on `Queue`/`Stack` and
`Hashtable`/`ListDictionaryInternal`. `Hashtable`'s `getCountProperty()` and copy
index moved from `int` to `intcs`.

One decision in the design record was superseded by the approval and is recorded
in section 21 of the design document: the deprecated, never-writing
`CopyTo(void*, intcs)` shim was **not** retained. A shim would let stale
downstream code compile and fail at run time, whereas removal makes each call a
compile error naming the replacement. **This is an ABI break** — a pure virtual
member left the interface, so every `ICollection`/`IList`/`IDictionary` vtable
changed and all C++ consumers must be rebuilt in full. Consumer guidance is in
[`docs/Migration-ICollectionCopyTo.md`](docs/Migration-ICollectionCopyTo.md) and
summarised in `README.md`'s "Breaking changes" section.

Closure evidence:

- 128 permanent regressions in `CopyToBoundaryTests.cpp`, parameterised over
  every `ICollection` implementation including both private `MemberCollection`
  views, plus compile-time `AcceptsDestination` assertions proving no overload
  accepts a raw pointer or a wrongly typed vector;
- the same 128 tests pass under ASan + UBSan + LeakSanitizer;
- the replacement probe `build-probe-copyto/probe8_new_boundary.cpp` runs the
  four originally unsafe scenarios plus non-trivial-value, heterogeneous, and
  100,000-element cases with `failures=0`, no sanitizer diagnostic, and no leak;
- the old probe's raw calls now produce four captured `no matching function`
  compile errors naming the replacements
  (`build-probe-copyto/probe5_removed_api.log`);
- `SharpRuntimeTests_Collections_Core` 1,612/1,612; full gate 12,871/12,871
  across 37 executables with zero warnings/errors;
- a `-Werror` standalone `Collections.Core` consumer fixture
  (`test/consumer/collections_copyto.cpp`) that compiles and runs successfully;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, selective matrix green, `git diff --check` clean, Doxygen
  1,942/1,942 -- at the ceiling and unchanged from before the ticket, since the
  README link to the migration document adds one instance of the pre-existing
  unresolved-markdown-link warning every README documentation link produces,
  offsetting one removed from `ICollection.hpp`.

Follow-ups: cleanup ticket **#1772** (`REMED-COLL-COPYTO-CLEANUP`) is `wontfix`
because both of its items were necessarily completed inside #1771 — the shim it
would have deleted was never created, and the `Array.hpp` / `Buffer.hpp`
doc-comments citing `ArrayList::CopyTo(void*, int)` could not be left citing a
removed member. New ticket **#1773** (`REMED-COLL-COPYTO-DOWNSTREAM`, P2, size S)
is **inactive**: it requires the CNA and mobile-eggbert `CopyTo` sweep and full
rebuild described in `docs/Migration-ICollectionCopyTo.md` §9. Neither repository
is in this checkout, so nothing is asserted about their current usage.

### Completed raw-CopyTo correction: ticket #1774

**Done**, opened and closed 2026-07-27 on the same branch immediately after
#1771. `detail::requireValidCopyDestination` rejected every null-pointer
destination outright, including a valid empty `ObjectSpan{nullptr, 0}` or a
default-constructed empty `std::vector<std::any>` copied from an empty
collection — stricter than intended, since `ObjectSpan` has no distinct
managed-null-array state and a null-and-zero-length destination is simply "no
storage, no elements", matching .NET's `new object[0]`. The corrected rule and
its exact checking order (negative index, index past the destination end,
null data with a *positive* length, insufficient capacity, success) are
recorded in [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md)
section 22. A non-empty collection copied into a zero-length destination still
fails, but on capacity, not nullness; a null pointer paired with a positive
length remains rejected regardless of source size. SR-AUD-358 stays
`remediated` — this did not reopen it, and it did not restore
`CopyTo(void*, intcs)`, redesign `ObjectSpan`, or touch individual
`copyToCore` bodies.

Closure evidence: `CopyToBoundaryTests.cpp` gained parameterised cases for the
empty-to-empty success paths (both overloads), the malformed
null-with-positive-length case, negative/past-end indices against a
zero-length destination, and a `ProbeCollection`-based proof that a validation
failure never reaches `copyToCore`; two pre-existing typed-overload assertions
were corrected from `ArgumentNullException` to `ArgumentException`; the
standalone `test/consumer/collections_copyto.cpp` fixture was corrected,
compiles `-Werror`, and runs successfully; and the new standalone probe
`build-probe-copyto/probe10_empty_span_correction.cpp` passes 10/10 assertions
under ASan + UBSan + LeakSanitizer with no diagnostic and no leak.
`SharpRuntimeTests_Collections_Core` grew from 1,612/1,612 to 1,662/1,662
(net +50); the full `scripts/local_ci_check.sh build` gate passed
12,921/12,921 tests across 37 executables with zero build warnings/errors
(was 12,871); boundaries stayed at 41 modules/90 edges, validator tests 7/7,
catalogue current, database consistent, the ten-job selective matrix green,
`git diff --check` clean, and Doxygen 1.9.8 stayed at exactly 1,942/1,942 --
unchanged from the #1771 ceiling, since the one new markdown link this ticket
added to `README.md` was written without link syntax specifically to avoid
adding a second instance of the pre-existing unresolved-markdown-link warning.

### Completed Hashtable key/view remediation: ticket #1775

**`P1: Restore the Hashtable IDictionary key and view contracts`**
(`REMED-COLL-HASHTABLE-VIEWS`, SR-AUD-363, size M) is **done**, opened and
closed 2026-07-27 on local branch `feature/remediation-coll-hashtable-views`.

It was selected from the "Recommended dependency order" below, whose step 1 now
has exactly one remaining bullet — SR-AUD-359 through SR-AUD-363 — because the
enumerator lifecycle (#1767), node lifetime (#1768/#1769), and raw-output
(#1770/#1771/#1774) contracts are all stable. All five of those findings are
`medium`, so the tie was broken on the documented rule of preferring a coherent
interface/ownership defect: SR-AUD-363 is the last non-generic `IDictionary`
contract hole, and it reuses the very `ICollection` copy boundary #1771 built.

Bounded scope — the two defects the finding records, and nothing else:

1. `Hashtable::getKeysProperty()` / `getValuesProperty()` return `nullptr`
   although `IDictionary` documents each as returning an `ICollection` over the
   dictionary's keys/values. The sibling `ListDictionaryInternal` in the same
   component already returns a real `MemberCollection`, so identical
   `IDictionary*` caller code is safe against one implementation and fatal
   against the other.
2. The raw-key entry points stringify a null key as the address text `"0"`, so
   `Add(nullptr, value)`, `setItem`, `getItem`, `Contains`, and `Remove` accept
   a null key instead of throwing `ArgumentNullException`, and that stringified
   null key aliases the ordinary string key `"0"` accepted by the
   `Add(std::string, std::any)` overload.

Explicitly excluded: SR-AUD-359 (`ReadOnlyDictionary::Empty`), SR-AUD-360
(`ConcurrentDictionary::AddOrUpdate`), SR-AUD-361 (`SortedSet::GetViewBetween`),
SR-AUD-362 (`FrozenDictionary::Create` duplicates), and every remediated
finding. No public signature changed and no virtual member was added or removed,
so unlike #1771 this is **not** a source- or ABI-breaking change; ticket #1773
stayed blocked and untouched.

Three facts beyond the audit summary were established by direct probe before
any change (`build-probe-hashtable/probe1_current_boundary.cpp`, gitignored):

- the null view is not merely a missing feature — a consumer that follows the
  `IDictionary` documentation is an ASan-confirmed SEGV plus a UBSan
  `member access within null pointer of type 'struct ICollection'`, while
  `ListDictionaryInternal` answers the *identical* caller code correctly;
- the stringified null key `"0"` aliases the ordinary string key `"0"`: after
  `Add(nullptr, v)`, `ContainsKey("0")` is true and `Add("0", …)` is rejected
  as a duplicate of an entry the caller never added;
- a third null-key entry point exists — `Remove(const char*)` reached
  `std::string`'s null construction and terminated with a `std::logic_error`
  invisible to code catching `System::Exception&`.

Selected repair: both view properties return a live, **caller-owned**
`MemberCollection` whose `Count`, `SyncRoot`, `IsSynchronized`,
`GetEnumerator`, and `copyToCore` delegate to the owning table — the
`ListDictionaryInternal::MemberCollection` precedent already in this component,
and the closest available match to .NET's `KeyCollection`/`ValueCollection`.
.NET can hand back a *cached* view because the GC owns it; this port has no GC,
so a returned reference type is caller-owned exactly as `GetEnumerator()` is
throughout the port. The views reuse the #1771/#1774 copy boundary unchanged.
`toKey()` became the single validating conversion site every raw-key path
passes through, so no entry point can skip the null check — the same
structurally-unskippable shape `detail::requireValidCopyDestination` gave the
copy boundary. `IDictionary`'s own `@return` documentation, which claimed the
concrete dictionary manages the view lifetime (something neither implementation
nor any caller did), now states the implemented rule.

Closure evidence:

- 70 permanent regressions in `DictionaryKeyAndViewContractTests.cpp`; because
  the root cause is an *interface* defect, the view cases are parameterised
  over both non-generic `IDictionary` implementations, so neither can regress
  to a null or snapshot view;
- the same 70 tests pass under ASan + UBSan + LeakSanitizer with no diagnostic
  and no leak (leak detection verified active by a deliberate-leak self-test);
- the replacement probe `build-probe-hashtable/probe2_fixed_boundary.cpp` runs
  the previously fatal scenarios plus liveness, non-trivial values, a
  20,000-entry table, and destruction order with `failures=0`;
- `SharpRuntimeTests_Collections_Core` 1,732/1,732 (was 1,662);
- `test/consumer/collections_dictionary_views.cpp` compiles `-Werror` against
  only `SharpRuntime::Collections.Core` and runs successfully;
- network-permitted `scripts/local_ci_check.sh build`: 12,991/12,991 tests
  across 37 executables (was 12,921), zero warnings/errors, with the six
  local-server `Net.Http` cases passing;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, the ten-job selective matrix green, `git diff --check`
  clean, and Doxygen 1.9.8 at exactly 1,942/1,942 — unchanged, at the ceiling.

Two separate **pre-existing** defects found while implementing this are
recorded as inactive tickets rather than folded in, and are deliberately *not*
new `SR-AUD-*` identifiers, since the audit numbering is frozen at 364:

- **#1776** (`REMED-CORE-ARGNULL-MESSAGE`, P2, XS) —
  `System::ArgumentNullException(paramName)` emits its `(Parameter 'x')` suffix
  **twice**: its own `makeMsg()` appends it and the
  `ArgumentException(message, paramName)` base appends it again. Every
  `ArgumentNullException(paramName)` call site inherits the doubled text,
  including the copy boundary's `ArgumentNullException("destination")`. #1775's
  assertions therefore check the exception type, `getParamNameProperty()`, and
  the leading `"Value cannot be null."` text rather than an exact string, so
  they stay correct once #1776 lands.
- **#1777** (`REMED-COLL-COPYTO-DOC-SYNC`, P3, XS) — the typed
  `CopyTo(std::vector<T>&, intcs)` doc-comments on `Hashtable` (:87),
  `Queue` (:73), `Stack` (:73), and `ListDictionaryInternal` (:165) still said
  `@throws ArgumentNullException if destination has no storage`, which ticket
  #1774 superseded. Documentation only; it did **not** reopen SR-AUD-358 or
  CCF-020. **Done** — see below.

### Completed ArgumentNullException message remediation: ticket #1776

**`P2: fix duplicated ArgumentNullException parameter suffix`**
(`REMED-CORE-ARGNULL-MESSAGE`, size XS) is **done**, opened and closed
2026-07-27 on local branch `feature/remediation-argument-null-message`.

Root cause: `ArgumentNullException(paramName)`'s private `makeMsg()` helper
composed `"Value cannot be null. (Parameter 'x')"` and passed that
already-suffixed text to the `ArgumentException(message, paramName)` base
constructor, whose own `appendParamName()` appended the identical suffix a
second time, producing e.g.
`"Value cannot be null. (Parameter 'destination') (Parameter 'destination')"`.
Because `makeMsg()` concatenated the raw C-string before the base
constructor's null guard could run, a null `paramName` also reached
`std::char_traits<char>::length(nullptr)` first — this is audit finding
SR-AUD-089, filed alongside SR-AUD-090 (the duplicate suffix itself) against
the same file during the original audit. See the correction note above:
#1776 was inaccurately recorded as covering no audit finding when it opened.

Fix: the paramName-only constructors now pass the raw, unsuffixed default
message straight through, exactly matching .NET's own
`ArgumentNullException(paramName) : base(SR.ArgumentNull_Generic, paramName)`.
The base constructor is now the single site that both appends the suffix
(once) and null-guards the C-string overload, which resolves SR-AUD-090
directly and resolves SR-AUD-089 as a natural consequence, not a separate
change. `getParamNameProperty()`, HResult (`E_POINTER`), the
`(paramName, message)` and `(message, innerException)` overloads, and sibling
`ArgumentException`/`ArgumentOutOfRangeException` are all unchanged and
regression-tested as such. No public signature, virtual member, or
inheritance changed, so this is neither a source nor an ABI break.

Evidence:

- 26 new permanent regressions: 20 in `ArgumentNullExceptionTests.cpp`
  (exact message per constructor overload, single-occurrence suffix counts,
  empty and punctuated parameter names, copy/move, catch-through
  `ArgumentNullException&`/`ArgumentException&`/`System::Exception&`, and a
  direct null-`const char*` non-crash regression for SR-AUD-089), 3 in
  `ArgumentExceptionTests.cpp`, and 3 in `ArgumentOutOfRangeExceptionTests.cpp`
  pinning that those sibling types were never affected;
- the two pre-existing exact-message workarounds this defect forced now
  assert the single-suffix message directly:
  `DictionaryKeyAndViewContractTests.cpp`'s `expectNullKeyRejected` (from
  #1775) and `LinkedListNodeLifetimeTests.cpp`'s `ExpectArgumentNullMessage`
  (from #1769);
- `SharpRuntimeTests_Core_Base` 4,972/4,972 and
  `SharpRuntimeTests_Collections_Core` 1,732/1,732;
- the `Core.Base` standalone public-header consumer fixture
  (`test/consumer/core_base.cpp`) extended to construct, throw, and catch an
  `ArgumentNullException` through `System::Exception`, verifying the parameter
  name and single-suffix message, compiling and running under `-Werror`;
- network-permitted `scripts/local_ci_check.sh build`: 13,017/13,017 tests
  across 37 executables (was 12,991), zero warnings/errors;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, the ten-job selective matrix green, `git diff --check`
  clean, and Doxygen 1.9.8 at exactly 1,942/1,942 — unchanged, at the ceiling.

This is a pure message-composition fix with no allocation, ownership, or
string-lifetime change, so a dedicated sanitizer campaign was not run beyond
the existing focused-suite coverage.

### Completed CopyTo doc-sync remediation: ticket #1777

**`P3: correct stale typed CopyTo documentation`**
(`REMED-COLL-COPYTO-DOC-SYNC`, size XS) is **done**, opened and closed
2026-07-27 on local branch `feature/remediation-copyto-docs`.

Root cause: the typed `CopyTo(std::vector<T>&, intcs)` doc-comments on
`Hashtable::CopyTo(std::vector<DictionaryEntry>&, intcs)`,
`Queue::CopyTo(std::vector<void*>&, intcs)`,
`Stack::CopyTo(std::vector<void*>&, intcs)`, and
`ListDictionaryInternal::CopyTo(std::vector<DictionaryEntry>&, intcs)` each
carried `@throws System::ArgumentNullException if @p destination has no
storage.` — ticket #1771's original rule, superseded by #1774. A repository-wide
search for the null-destination/zero-length/insufficient-capacity language
(`CopyTo`, `null destination`, `zero-length destination`, `ObjectSpan`,
`ArgumentNullException`, and the concrete destination vector types) across
`modules/`, `docs/`, `README.md`, `NEXT.md`, and `plan.md` confirmed these were
the only four current public headers with the stale text; `ICollection.hpp`'s
own `CopyTo(ObjectSpan, intcs)` doc-comment, `docs/ICollectionCopyToDesign.md`
section 22, `docs/Migration-ICollectionCopyTo.md` §7, and `README.md` were
already corrected under #1774 and needed no further change. `List<T>::CopyTo`,
`LinkedList<T>`'s node-handle `ArgumentNullException` documentation, and
`ImmutableList<T>`'s delegate-null documentation are unrelated APIs that do not
route through `detail::requireValidCopyDestination` and were left untouched.

Fix: each of the four doc-comments now states the corrected contract —
`ArgumentOutOfRangeException` for a negative index, `ArgumentException` for a
destination that cannot hold `getCountProperty()` elements starting at
`index` (including a non-empty collection copied into a zero-length
destination), and `ArgumentNullException` only for a null pointer paired with
a positive length, with a null pointer at zero length called out explicitly as
a valid empty destination. No implementation, test assertion, signature, or
behavior changed. SR-AUD-358 and CCF-020 remain `remediated` and were not
reopened; ticket #1773 remains `blocked`.

Evidence:

- `SharpRuntimeTests_Collections_Core` `--gtest_filter="*CopyTo*"`: 225/225,
  unchanged; full suite 1,732/1,732, unchanged;
- `test/consumer/collections_copyto.cpp` recompiled and rerun against only
  `SharpRuntime::Collections.Core` + `Core.Base`, `-Wall -Wextra -Wpedantic
  -Werror`: compiles and exits 0;
- network-permitted `scripts/local_ci_check.sh build`: 13,017/13,017 tests
  across 37 executables, zero warnings/errors — unchanged, no regression;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, the ten-job selective matrix green, `git diff --check`
  clean, and Doxygen 1.9.8 at exactly 1,942/1,942 — unchanged, at the ceiling.

### Completed ConcurrentDictionary AddOrUpdate remediation: ticket #1778

**`P2: Retry ConcurrentDictionary AddOrUpdate against the observed value
instead of losing concurrent updates`** (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`,
SR-AUD-360, size S) is **done**, opened and closed 2026-07-27 on local branch
`feature/remediation-coll-concurrentdict-addorupdate`.

Selected from the "Recommended dependency order" step 1 remaining bullet
(SR-AUD-359 through SR-AUD-362, since SR-AUD-363 closed under #1775). NEXT.md
itself flags SR-AUD-359 (`ReadOnlyDictionary::Empty`) and SR-AUD-361
(`SortedSet::GetViewBetween`) as possibly needing a public-surface design
decision, so per the Approval boundary they were set aside without a design
ticket rather than selected. Between the two remaining signature-compatible
candidates, SR-AUD-362 (`FrozenDictionary::Create` duplicate keys) was checked
against the current .NET reference
(`/rv/tmp/runtime/src/libraries/System.Collections.Immutable/src/System/Collections/Frozen/FrozenDictionary.cs`)
before selection: its own doc-comment states last-value-wins is the
*intended* `Create`/`ToFrozenDictionary` behavior, explicitly contrasted with
`Enumerable.ToDictionary`'s throw-on-duplicate behavior, and
`GetExistingFrozenOrNewDictionary`/`CreateFromDictionary` deliberately use the
indexer instead of `Add` to realize exactly that. sharp-runtime's current
`FrozenDictionary::Create` already matches this. SR-AUD-362's premise
therefore does not hold against the actual .NET source; it was left
untouched and not reopened as a second ticket — see
`audit/AUDIT_FINAL_REPORT.md`'s planning-accuracy note for the full
correction record.

SR-AUD-360 was independently confirmed as a real defect against
`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Concurrent/ConcurrentDictionary.cs`
(`AddOrUpdate` and `TryUpdateInternal`): real .NET gates the commit on
`EqualityComparer<TValue>.Default.Equals` against the previously observed
value and retries (re-observes, re-invokes the factory) on a mismatch.
`ConcurrentDictionary::AddOrUpdate`'s own doc-comment already documented the
unconditional-overwrite behavior as a deliberate simplification, but
`TryUpdate` on the same class already requires `TValue::operator==`, so
extending that requirement to `AddOrUpdate` is consistent with existing
project convention, not a new constraint category. No public signature
changed and no virtual member was added or removed, so this is neither a
source nor an ABI break; it proceeded without additional user approval under
the repository's compatible-bug-fix rule.

Fix: both `AddOrUpdate` overloads now loop — after computing the new value
outside the lock (the factory is still never invoked with the internal mutex
held, preserving the documented reentrancy/deadlock-avoidance guarantee), the
commit re-acquires the lock, re-reads the entry, and writes only if it still
equals the previously observed value; otherwise the whole operation retries
against the newly observed state. A key absent at the initial observation
that is concurrently added by another thread falls through to the update
branch on retry rather than double-adding.

Pre-fix reproduction (gitignored
`build-probe-concurrentdict/probe1_lost_update.cpp`, ASan+UBSan and
separately ThreadSanitizer): a coordinated two-thread repro blocks the update
factory after it observes `0`, writes `10` through the indexer while the
factory is blocked, then releases it. Pre-fix: `add-or-update-result final=1`
(5/5 runs), matching the finding's own `add-or-update-result=1 final=1`
reproduction exactly. Post-fix: `final=11` (20/20 runs under ASan+UBSan; 5/5
under TSan), clean, no sanitizer diagnostic. A second stress probe
(`build-probe-concurrentdict/probe2_stress.cpp`, 16 threads x 2,000
`AddOrUpdate` calls on one shared key) is clean under TSan with no data race
and the exact expected total (32,000).

Closure evidence:

- 4 new permanent regressions in `ConcurrentDictionaryTests.cpp`: deterministic
  coordinated intervening-write repros for both overloads (matching the
  pre-fix probe's shape), a key-added-concurrently retry case, and an
  8-thread/500-iteration-per-thread contention stress case asserting the
  final counter reflects every increment;
- `ConcurrentDictionaryTest` suite (26/26) passing consistently across five
  repeated runs;
- `SharpRuntimeTests_Collections_Core` 1,736/1,736 (was 1,732);
- network-permitted `scripts/local_ci_check.sh build`: 13,021/13,021 tests
  across 37 executables (was 13,017), zero warnings/errors, six local-server
  `Net.Http` cases passing;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, the ten-job selective matrix green, `git diff --check`
  clean;
- a dedicated public-header consumer fixture was not added: no public
  signature or type surface changed, and the header already compiles as part
  of the regular `Collections.Core` build/test target, so a new standalone
  fixture would not exercise anything the existing build does not already
  cover;
- Doxygen: measured independently at exactly 1,944 warnings both with and
  without this ticket's diff applied (confirmed via `git stash`), i.e. this
  ticket adds **zero** new warnings. This is 2 warnings above the "1,942"
  ceiling recorded elsewhere in this document and in `CLAUDE.md`; that drift
  predates this ticket (it is present on a clean pre-ticket tree) and is not
  attributable to this change. Not investigated or corrected here — flagged
  for a future documentation-integrity ticket, consistent with this session's
  one-active-ticket rule and the transparency requirement not to conceal it.

No repair ticket is active.

**Correction (ticket #1779, 2026-07-27):** the paragraph above states ticket
#1778 "measured independently at exactly 1,944 warnings." While verifying the
Doxygen baseline during ticket #1779's own validation pass, an independent
re-measurement on the identical tree ticket #1778 left behind (no header or
source file changed since) using the repository's own canonical
`scripts/check_doxygen_warnings.sh` — Doxygen 1.9.8, `doxygen Doxyfile`,
`grep -c ': warning:'`, the exact colon-qualified pattern the tracked script
uses — returned **1,942** warnings, exactly at the documented ceiling, three
times including once from a fully clean `docs/generated/`. A looser
`grep -c 'warning:'` (no leading colon) on the same run returns 1,944: it
additionally matches two lines that are not `file:line: warning:` diagnostics
at all, but bare `warning: Inheritance graph for ... not generated, too many
nodes` advisory lines (for `System::Attribute` and `System::SystemException`)
that begin with the word `warning:` with no preceding colon-qualified
location — exactly the two extra matches accounting for 1,944 vs 1,942. This
strongly suggests ticket #1778's 1,944 figure came from a looser counting
method than the tracked script uses, not from an actual regression, and that
the 1,942 ceiling documented in `CLAUDE.md`/`README.md`/`plan.md` remains the
correct, current, canonical count as of this ticket. This correction is
recorded here rather than silently rewriting the paragraph above, per this
repository's practice of preserving historical narrative (see the #1776 and
#1778 corrections elsewhere in this file). No inactive ticket previously
tracked this discrepancy; inactive ticket **#1781**
(`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, P3, size XS) now does, to re-verify at
pickup time (an intervening ticket could change the count again) before
correcting any other documents that repeat the 1,944 figure. Not begun under
this ticket. **Ticket #1781 has since completed this re-verification** — see
"Completed Doxygen count reconciliation: ticket #1781" below — and
reconfirmed 1,942 with no intervening drift.

### Completed ReadOnlyDictionary::Empty design: ticket #1779

**`P2: Design a safe const-reference contract for ReadOnlyDictionary::Empty`**
(`REMED-COLL-READONLYDICT-EMPTY-DESIGN`, SR-AUD-359, size S, design-only) is
**done**, opened and closed 2026-07-27 on local branch
`feature/remediation-coll-readonlydict-empty-design`. It made no production or
test-source change.

Selected from the two remaining findings NEXT.md flagged as possibly needing a
public-surface design decision (SR-AUD-359 `ReadOnlyDictionary::Empty` and
SR-AUD-361 `SortedSet::GetViewBetween`), compared in detail before selection.
Both are medium-severity confirmed findings affecting a public contract, but
SR-AUD-359's fix is a bounded one-line return-type change with a fully
compatible design (no in-repository source break found), while SR-AUD-361
would require replacing `SortedSet<T>`'s `std::set` backing with a custom tree
structure supporting live, bounded, write-through sub-range views — .NET's own
`TreeSubSet` nested class is 378 lines — before any bounded implementation
ticket could even be written, an architecture change far beyond a single
ticket and already flagged as such in `SortedSet.hpp`'s own doc-comment.
SR-AUD-361 was left untouched, confirmed, and not selected.

`ReadOnlyDictionary<K,V>::Empty()` returns a non-`const` reference to a
process-wide function-local `static` singleton. Because the class relies on
its compiler-generated copy assignment operator, ordinary assignment through
that reference (`Empty() = someOtherInstance;`) silently rebinds the
singleton's private backing map for the remainder of the process, corrupting
every past and future caller of `Empty()` for that `<K,V>` instantiation.
.NET's own `Empty` is a get-only auto-property with no setter (`CS0200` on
assignment), so the C++ port introduced this hazard by translating a get-only
property into a mutable reference-returning static method.

Selected design (recorded in full, with three evaluated alternatives and a
compatibility matrix, in
[`docs/ReadOnlyDictionaryEmptyDesign.md`](docs/ReadOnlyDictionaryEmptyDesign.md)):
change `Empty()`'s return type from `ReadOnlyDictionary<K,V>&` to
`const ReadOnlyDictionary<K,V>&`. This is not a compromise — it is the literal
C++ expression of ".NET has no setter," fully closing the finding while
preserving every other observable behavior, including the existing
singleton-identity regression test (`&empty1 == &empty2`). Returning `Empty()`
by value instead (rejected Alternative B) would have broken that exact test
and the class's own documented "shared, permanently-empty instance" contract
for no additional safety benefit; deleting the class's assignment operators
entirely (rejected Alternative C) would have been broader than the finding's
bounded scope, restricting ordinary non-singleton instances the finding never
implicated.

This is a public API signature change (the qualifier of a returned reference),
so per this repository's established approval boundary — the same one applied
to ticket #1770/#1771's `ICollection::CopyTo` removal — it requires explicit
user approval before implementation, even though this design finds no actual
in-repository source break (the sole in-repo caller uses `auto&`, which
deduces correctly either way) and no ABI break (the class is a header-only
template with no vtable and no exported linkage symbol). Implementation is
proposed as separate, inactive ticket **#1780**
(`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS), marked `blocked` on that
approval.

Reproduction (repository-local, gitignored `build-probe-readonlydict/` tree,
excluded by the repository's `build*` `.gitignore` entry):

- `probe1_mutable_empty.cpp`, compiled `-std=c++23 -Wall -Wextra -Wpedantic
  -fsanitize=address,undefined` against `Core.Base` and run with
  `ASAN_OPTIONS=detect_leaks=0`, independently reproduces the audit's own
  `empty-before=0` / `empty-after-assignment=1` symptom, and adds two facts
  beyond the original evidence: an unrelated second call site observes the
  contamination (`second-caller-observes=1`), and `&empty == &empty2` confirms
  it is the identical process-wide singleton object (`same-instance=1`), not a
  copy — this is real, silent, global data corruption for every consumer of
  `Empty()`, not a mistake local to one call site. No sanitizer diagnostic
  fires (it is a logic defect, not a memory-safety one: the assignment is
  valid, well-defined C++, simply semantically wrong for an advertised
  immutable singleton).
- `probe2_fix_rejects_assignment.cpp`, compiled against a modified copy of the
  header (`ReadOnlyDictionary_fixed.hpp`, production source untouched) with
  only `Empty()`'s return type changed: the same hazardous assignment now
  fails to compile (`error: passing 'const ReadOnlyDictionary<...>' as 'this'
  argument discards qualifiers`).
- `probe3_fix_preserves_behavior.cpp`, compiled and run clean under
  ASan+UBSan (`all-assertions-passed=1`) against the same fixed header:
  confirms singleton identity, emptiness, normal construction, `ContainsKey`,
  indexer access, and independent local copy-construction all remain exactly
  as they behave today.

Closure evidence:

- the three probes above, all `-Wall -Wextra -Wpedantic` clean;
- the existing 17 `ReadOnlyDictionary` regression tests (10 in
  `ObjectModelTests.cpp`, 7 in `ObjectModelBatch18Tests.cpp`) rerun unchanged
  and passing, confirming the design work introduced no regression;
  `SharpRuntimeTests_Collections_ObjectModel` 124/124;
- a full local gate: `python3 scripts/validate_module_boundaries.py`,
  `python3 test/validate_module_boundaries_test.py` (7/7),
  `python3 scripts/generate_component_catalog.py --check`,
  `python3 scripts/db_consistency_check.py --db plan.sqlite3`, and
  `git diff --check` all pass; boundaries unchanged at 41 modules/90 edges;
  `scripts/local_ci_check.sh build`: 13,021/13,021 tests across 37
  executables, zero warnings/errors — unchanged, since no production or test
  source changed;
- Doxygen 1.9.8 independently re-measured at 1,942 warnings (see the
  correction note above this section) — unchanged, since this ticket added
  only `docs/*.md` and `audit/*.md` files, which Doxygen does not scan.

**SR-AUD-362 reconciliation (not the active ticket, done alongside #1779's
housekeeping):** per this run's instruction to inspect SR-AUD-362's status
without repairing it as production code, its per-file audit report
(`audit/modules/collections/include/System/Collections/Frozen/FrozenDictionary.hpp.audit.md`)
and `audit/AUDIT_FINDINGS_INDEX.md` row now carry a Correction note
cross-referencing ticket #1778's planning-accuracy finding that
`FrozenDictionary::Create`'s last-value-wins behavior matches the current
.NET reference's own documented intent. The repository's findings-index
status vocabulary supports only `confirmed`/`remediated` — no
`not-a-defect`/`false-positive` status exists — so SR-AUD-362 is deliberately
left `confirmed` rather than assigned an invented status, but it must not be
read as an active, un-investigated defect. It is not counted as `remediated`
and no code changed.

No repair ticket is active.

### Completed ReadOnlyDictionary::Empty implementation: ticket #1780

**`P2: Return Empty() by const reference to close the mutable-singleton
hazard`** (`REMED-COLL-READONLYDICT-EMPTY`, SR-AUD-359, size XS) is **done**,
opened and closed 2026-07-27 on local branch
`feature/remediation-coll-readonlydict-empty`, after the user explicitly
approved implementing #1779's design as a public API signature change.

`ReadOnlyDictionary<K,V>::Empty()`'s declared return type changed from
`ReadOnlyDictionary<K, V>&` to `const ReadOnlyDictionary<K, V>&`, exactly as
#1779 selected — no redesign. No other member, constructor, or the class's
copy/move assignment operators changed, so ordinary, non-singleton instances
remain freely assignable exactly as before; no virtual member was added or
removed (the class has none, so this is not an ABI break in that sense
either).

Pre-fix reproduction re-ran #1779's own gitignored
`build-probe-readonlydict/probe1_mutable_empty.cpp` directly against the
still-unmodified production header immediately before the change, reconfirming
`empty-before=0`/`empty-after-assignment=1`/`second-caller-observes=1`/
`same-instance=1`. Post-fix, two new probes were compiled against the real,
now-modified production header (not a copy, unlike #1779's design-phase
probes, which necessarily used a modified copy since production source could
not change under a design-only ticket):
`probe4_production_header_rejects_assignment.cpp` fails to compile with
`error: passing 'const ReadOnlyDictionary<...>' as 'this' argument discards
qualifiers`, and `probe5_production_header_preserves_behavior.cpp` runs clean
under ASan+UBSan with `all-assertions-passed=1`, confirming singleton
identity, emptiness, normal construction, `ContainsKey`, indexer access, and
independent copy-construction are all unaffected.

**ABI investigation:** a direct `nm -C`/`c++filt` comparison of `Empty()`'s
mangled symbol in `build/SharpRuntimeTests_Collections_ObjectModel`, before
and after the change, shows byte-identical symbol names — only load addresses
differ, an artifact of the added test code shifting layout. This is expected:
the Itanium C++ ABI does not encode a function's return type in its mangled
name, `Empty()` is emitted as a weak/COMDAT symbol (one per instantiating
translation unit, deduplicated by the linker), and `Collections.Core` is
registered as an `INTERFACE` (header-only) CMake target with no static or
shared archive of its own. Conclusion: no binary symbol break; the only
compatibility impact is source-level, and only for the exact hazardous
explicit-non-`const`-reference/assignment pattern, confirmed absent everywhere
in this repository (§5 of the design document). Because the type is
header-only, any translation unit that includes this header must recompile to
gain the new compile-time protection — the ordinary rebuild expectation for
any header-only library change, not a new risk this ticket introduces.

Closure evidence:

- two new permanent regressions in
  `ObjectModelTests.cpp::ReadOnlyDictionaryTests`: `Empty_ReturnTypeIsConstReference`
  (a file-scope `static_assert`, since GoogleTest cannot assert a compile-time
  property) and `Empty_RemainsEmptyAfterConstructingUnrelatedInstances`, which
  constructs and copies several unrelated instances and reconfirms `Empty()`
  stays empty and identity-stable throughout; `Empty_IsEmptyAndCached` is
  retained verbatim;
- `SharpRuntimeTests_Collections_ObjectModel` grew from 124/124 to 125/125;
- a new standalone `Collections.Core` public-header consumer fixture
  (`test/consumer/collections_object_model_readonlydictionary.cpp`) compiles
  `-Wall -Wextra -Wpedantic -Werror` and runs successfully, and a companion
  negative-compile fixture
  (`test/consumer/collections_object_model_readonlydictionary_negative.cpp`)
  fails to compile with the same diagnostic through the repository's own
  `test/consumer/CMakeLists.txt` harness;
- network-permitted `scripts/local_ci_check.sh build`: 13,022/13,022 tests
  across 37 executables (was 13,021), zero warnings/errors;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, the ten-job selective matrix green, `git diff --check`
  clean, and Doxygen 1.9.8 re-measured at exactly 1,942/1,942 — unchanged, at
  the ceiling.

No repair ticket is active.

### Completed Doxygen count reconciliation: ticket #1781

**`P3: Reconcile ticket #1778's 1,944 Doxygen count against the canonical
1,942 script measurement`** (`REMED-DOCS-DOXYGEN-COUNT-RECONCILE`, size XS,
planning-integrity only, no `SR-AUD-*` finding) is **done**, opened and
closed 2026-07-27 on local branch
`feature/remediation-docs-doxygen-count-reconcile`, in a session scoped
exclusively to this ticket.

This ticket was itself opened inactive while verifying the Doxygen baseline
during design ticket #1779's validation pass (see the correction note under
"Completed ConcurrentDictionary AddOrUpdate remediation: ticket #1778"
above), to re-verify at pickup time — since an intervening ticket could
change the count again — before correcting any other document that repeats
the stale 1,944 figure as a measured fact.

Re-ran `scripts/check_doxygen_warnings.sh` (Doxygen 1.9.8, `doxygen Doxyfile`,
`grep -c ': warning:'`, the exact colon-qualified pattern the tracked script
uses) on a clean, current tree three times, including once from a fully
clean `docs/generated/`: **1,942** warnings every time, exactly at the
documented ceiling and unchanged since ticket #1779's own re-measurement —
no intervening drift. This is a pure re-verification; no header or source
file changed between #1779 and this ticket's measurement.

Per the ticket's acceptance criteria, since the canonical count reproduced
1,942 (not 1,944), the two documents that still stated 1,944 as a measured
fact were corrected with preserved-history Correction notes rather than
having their original text silently rewritten, matching this repository's
established practice (see the #1776, #1778, and #1779 corrections elsewhere
in this file):

- `plan.sqlite3`'s ticket #1778 row `notes` field gained an appended
  Correction paragraph;
- `audit/AUDIT_PROGRESS.md`'s ticket #1778 entry gained an appended
  Correction paragraph.

`NEXT.md` (this file) and `plan.md` already carried an accurate correction
recorded under ticket #1779 and needed no further content change beyond this
closure section, this file's own header/summary lines, and `plan.md`'s
ticket-count summary line and closure paragraph. `audit/AUDIT_FINAL_REPORT.md`
and `audit/AUDIT_FINDINGS_INDEX.md` were checked and do not contain the 1,944
figure, so neither needed correction.

No production or test source changed; no `SR-AUD-*` finding was reopened,
remediated, or created; this was kept a documentation/measurement-methodology
-only change and was not folded into any unrelated ticket, per the ticket's
own bounded acceptance criteria. Validation: `scripts/check_doxygen_warnings.sh`
passed (1,942/1,942 against the 1,942 ceiling); `python3
scripts/db_consistency_check.py --db plan.sqlite3` passed with no consistency
problems; `git diff --check` clean. Because no production or test source
changed, a full `scripts/local_ci_check.sh build` rerun was not required and
was not performed — the existing 13,022/13,022 gate from ticket #1780 stands
unchanged; module boundaries, the validator suite, and the generated
catalogue were likewise left unchanged and were not re-run, since nothing
that would affect them changed. CNA and mobile-eggbert were not inspected or
modified. No push, merge, rebase, tag, or publication occurred.

No repair ticket is active.

### Completed SortedSet live-view design: ticket #1782

**`P2: Define live SortedSet GetViewBetween semantics`**
(`REMED-COLL-SORTEDSET-VIEW-DESIGN`, SR-AUD-361, size M, design-only) is
**done**, opened and closed 2026-07-27 on local branch
`feature/remediation-coll-sortedset-view-design`. It made no production or
test-source change. SR-AUD-361 stays **`confirmed`**, now qualified
`confirmed (design-complete)`; it is **not** `remediated`, and the index counts
are unchanged at 355 open / 9 remediated.

`SortedSet<T>::GetViewBetween(lower, upper)` returns an independent `SortedSet<T>`
snapshot copy of the in-range elements instead of .NET's live, range-enforced,
bidirectionally write-through `TreeSubSet`. Ported C# that relies on
write-through — `set.GetViewBetween(a,b).Add(x)`, `view.Clear()` to delete a
range, enumerating a view while the set is mutated — compiles unchanged and
silently produces a different result.

Selected architecture, recorded in full with four rejected alternatives, a
fourteen-row compatibility matrix, all thirty-five required design decisions,
and a six-probe evidence index, in
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md):
`SortedSet<T>` holds `std::shared_ptr<State>` — the `State` owning the
`std::set<T>` **and** the single version counter — plus `std::optional<T>` lower
and upper bounds, so one public type is either an **owning full set** (no
bounds) or a **bounded live view** over the same state. `GetViewBetween` keeps
returning `SortedSet<T>` **by value**; the returned object is a handle, not a
copy. `std::shared_ptr` reproduces .NET's GC lifetime rule exactly, so a view or
an iterator outliving the set it came from is well-defined rather than dangling.
The governing rule for the special members is *copying preserves the object's
role — an owning set copies its elements, a view copies its reference — and
assignment rebinds the assigned handle without mutating state another handle
observes.*

A dedicated `SortedSetView<T>` type (Alternative C) was rejected: it does not
avoid the layout change, because the set's storage must become independently
owned either way; it breaks the return type on top of that; and it breaks .NET's
structural parity, in which a view **is a** `SortedSet<T>` accepted by every
`SortedSet<T>` parameter. Retaining snapshot semantics (Alternative E) was
evaluated honestly and rejected: its cost is a permanent, silent, undiagnosable
divergence, exactly the failure mode ticket #1771 refused when it declined a
throwing `CopyTo` shim.

**Planning-accuracy correction (2026-07-27, ticket #1782):** the ticket #1779
selection note below, `plan.md`, and `SortedSet.hpp`'s own `@warning` block each
state that SR-AUD-361 "would require replacing `SortedSet<T>`'s `std::set`
backing with a custom tree structure supporting live, bounded, write-through
sub-range views — .NET's own `TreeSubSet` nested class is 378 lines — before any
bounded implementation ticket could even be written." **That premise does not
hold.** `std::set` already provides `lower_bound`, `upper_bound`, and stable
iterators, so a bounded view needs only a shared owner for the container plus a
pair of bounds; the working prototype demonstrates it. .NET's `TreeSubSet` is
long mainly because it re-implements tree walks against raw `Node` pointers —
work `std::set` already does. The real cost is the ownership model, the
copy/move semantics, and one required `const` removal, which is why this stayed
design-first. Recorded here rather than rewritten into the earlier text, per
this file's practice of preserving historical narrative (see the #1776, #1778,
and #1779 corrections above).

Reproduction (repository-local, gitignored `build-probe-sortedset/` tree,
excluded by the `build*` `.gitignore` entry; §29 of the design record indexes
every command):

- `probe1_current_behavior.cpp`, ASan+UBSan+LSan, exit 0, `failures=0`, **no
  diagnostic and no leak**: the current implementation is memory-safe and
  semantically wrong. It reproduces the finding's own
  `source-add-visible-in-view=0` / `view-add-visible-in-source=0` symptom and
  adds the complete pre-fix contract — bounds are not enforced after
  construction (`view.Add(99)` on a `[3,7]` range succeeds and moves `Max` to
  99), `Clear()` on the view leaves the parent untouched, a nested view may
  silently **widen** where .NET throws, and mutating the parent during
  enumeration of the view does **not** throw where .NET does.
- `probe2_iterator_lifetime.cpp`: the advertised fail-fast guard fires for
  `Add`/`Remove`/`Clear` but **not** for whole-object assignment, because
  `version_` is a plain member that assignment overwrites instead of bumping —
  copy-assignment yields a silently wrong dereference with no diagnostic, and
  move-assignment is an **ASan-confirmed `heap-use-after-free`**.
- `probe3_comparer_requirement.cpp`: `GetViewBetween` is the only member
  spelling its comparisons with `operator>`, so an element type providing
  `operator<` alone — exactly the contract the class doc-comment states — is a
  hard compile error at `SortedSet.hpp:297` and `:300`.
- `SortedSetPrototype.hpp` + `probe4_prototype.cpp`: a working prototype of the
  selected architecture passes the identical scenario matrix with
  `failures=0`, clean under ASan+UBSan+LeakSanitizer, including owner
  destruction with surviving views, overlapping and nested views, three-way
  iterator invalidation, shared-state self-aliasing in set algebra, and a
  100,000-element scale case.
- `probe5_layout_symbols.cpp`: measured compatibility, not asserted —
  `sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104,
  `sizeof(Iterator)` 24 → 40, `is_polymorphic` 0 both ways; and the Itanium
  mangled name changes `_ZNK…` → `_ZN…` when `const` is dropped, **unlike**
  ticket #1780's `Empty()`, whose mangled name was byte-identical.
- `probe6_public_header_standalone.cpp`: the production header compiles
  standalone under `-Wall -Wextra -Wpedantic -Werror` and runs — the baseline
  #1783 must preserve.

Four adjacent defects (the three `probe1`/`probe2`/`probe3` items above plus the
divergent invalid-range message) live inside the surface #1783 rewrites. They
are folded into #1783's scope and deliberately receive **no new `SR-AUD-*`
identifier**, since the audit numbering is frozen at 364.

Implementation is separate ticket **#1783**
(`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L), **`blocked`** and not begun.
**The exact approval required** is to approve, together: removing the `const`
qualifier from `SortedSet<T>::GetViewBetween(const T&, const T&)`; the semantic
change from a detached snapshot to a live bidirectionally write-through bounded
view; and the `SortedSet<T>` object-layout change that requires every consumer,
including CNA and mobile-eggbert, to be rebuilt. This is the same approval
category tickets #1770/#1771 and #1779/#1780 needed. There is **no
in-repository source break** — all three `GetViewBetween` call sites are tests
using non-`const` sets, and none asserts a snapshot property, so no existing
assertion changes. If approval is refused, the recorded fallback is **E′**: keep
snapshot semantics but fix the four adjacent defects; it needs no approval and
closes none of SR-AUD-361.

Closure evidence:

- the six probes above, all `-Wall -Wextra -Wpedantic` clean;
- the three existing `GetViewBetween` tests (3/3) and the 41 mutable-`SortedSet`
  tests (`SortedSetTests.*:GenSortedSetTest.*:SortedSetVersionTrackingTests.*`,
  41/41) rerun unchanged and passing;
- `python3 scripts/validate_module_boundaries.py --root .` at 41 modules/90
  edges, `python3 test/validate_module_boundaries_test.py` 7/7,
  `python3 scripts/generate_component_catalog.py --check`,
  `python3 scripts/db_consistency_check.py --db plan.sqlite3`, and
  `git diff --check` all pass;
- `scripts/check_doxygen_warnings.sh` at exactly 1,942/1,942 — unchanged, at the
  ceiling, since this ticket added only `docs/*.md` and `audit/*.md`, which
  Doxygen does not scan (`Doxyfile`'s `INPUT` is `modules README.md`);
- the full `scripts/local_ci_check.sh build` gate was run rather than omitted,
  even though no production or test source changed and ticket #1781's
  documentation-only closure had precedent for skipping it: **13,022/13,022
  tests across 37 executables**, incremental build with zero warnings and zero
  errors (the script exits non-zero on any diagnostic; it exited 0), unchanged
  from ticket #1780's gate. The run reused the existing `build` tree
  incrementally at `--parallel 4`;
- `scripts/check_selective_components.sh` was **not** run: no public header and
  no component metadata changed, which is the condition that requires it, and
  the module graph is unchanged at 41 modules/90 edges.

Ticket #1773 remains `blocked` and untouched. CNA and mobile-eggbert were not
inspected or modified. No push, merge, rebase, tag, or publication occurred.

No repair ticket is active.

### Completed SortedSet live-view implementation: ticket #1783

**`P2: Implement live SortedSet GetViewBetween views`**
(`REMED-COLL-SORTEDSET-LIVE-VIEW`, SR-AUD-361, size L) is **done**, opened and
closed 2026-07-28 on local branch `feature/remediation-coll-sortedset-live-view`.
The user granted the exact approval ticket #1782 required, scoped to this ticket
only, so **SR-AUD-361 is now `remediated`** and the index counts move to
**354 open / 10 remediated**. The paragraph above, recording #1782's
design-complete state, is left in place rather than rewritten.

**The approved public change.**

```cpp
// before
[[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper) const;
// after
[[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper);
```

Same return type, same parameters, `const` removed. What changed underneath is
larger: the result is a **live bounded handle** onto the same tree instead of a
detached snapshot.

**Final internal representation** — exactly design section 11:
`std::shared_ptr<State>` where `State` owns `std::set<T> data` and
`intcs version`; `std::optional<T> lower_`/`upper_` (absent = bound inactive);
`mutable intcs cachedCount_`/`cachedCountVersion_` for a view's lazy Count.
`Iterator` holds `std::shared_ptr<const State>`, its position, the range end,
and a version snapshot. One public type is either an **owning full set** (no
bounds) or a **bounded view**; `getIsViewProperty()` tells them apart.

**Behavior now matching .NET's `TreeSubSet`:** bidirectional write-through;
inclusive bounds enforced for the whole life of the view (out-of-range `Add`
throws `ArgumentOutOfRangeException("item")` and writes nothing, out-of-range
`Remove` returns `false`, out-of-range `Contains` returns `false`); `Clear` on a
view removes exactly its range; nested views may only narrow
(`ArgumentOutOfRangeException("lowerValue"/"upperValue")`) and are flattened onto
the root state; the invalid-range `ArgumentException` carries .NET's exact
message `Must be less than or equal to upperValue. (Parameter 'lowerValue')`;
`Count` on a view is version-cached; `Min`/`Max` are range-scoped and return
`T{}` when empty; one shared version counter makes a mutation anywhere invalidate
every handle's outstanding enumerators; and set algebra is bounds-enforcing,
write-through, and guards the new shared-state self-aliasing hazard by comparing
shared state rather than object identity.

**Ownership rules.** Copying preserves the object's role: an owning set
deep-clones into independent state, a view copies its handle and bounds.
Assignment rebinds the assigned handle and never mutates state another handle
observes, so views and iterators onto the abandoned state keep it alive and keep
observing its pre-assignment contents. Move leaves the source a valid, empty
owning set. `ToSortedSet()` is the additive way to materialize an independent
range — the C++ spelling of .NET's `new SortedSet<T>(view)`.

**Four adjacent defects closed with it**, no new `SR-AUD-*` identifiers (the
numbering stays frozen at 364): ordering comes from `std::set::key_comp()` by
value, so an `operator<`-only element type instantiates `GetViewBetween` (probe 3
with `-DSORTEDSET_PROBE_INSTANTIATE_VIEW` now compiles `-Werror` and runs, where
it previously failed with two `no match for 'operator>'` errors); bounds are
enforced after construction; nested widening throws; and assignment can no longer
defeat the version guard — probe 2's `copy-assign` yields the correct
pre-assignment element instead of a silently wrong one, and its `move-assign`
**ASan `heap-use-after-free`** and `outlive` **ASan `stack-use-after-scope`** are
both gone.

**Compatibility, measured not assumed.**

| Layer | Effect |
|---|---|
| Public source | Only `const` callers break, as a compile error naming the non-`const` member. All three in-repository call sites use non-`const` sets, re-verified after implementation: **no in-repository source break**, no existing assertion changed. |
| Semantic | Intentionally broken — the silent one. Mutating the result now mutates the source; copying it copies the handle. `ToSortedSet()` is the documented replacement. |
| Binary symbol | `_ZNK6System11Collections7Generic9SortedSetIiE14GetViewBetweenERKiS5_` → `_ZN6System11Collections7Generic9SortedSetIiE14GetViewBetweenERKiS5_`, confirmed with `nm`. |
| Object layout | `sizeof(SortedSet<int>)` 56 → **40**, `sizeof(SortedSet<std::string>)` 56 → **104**, `sizeof(Iterator)` 24 → **40**; `alignof` 8, `is_polymorphic` 0, `is_trivially_copyable` 0, `is_nothrow_move_constructible` 1, `is_copy_assignable` 1 all preserved. Every design prediction matched exactly. |
| Practical | Every consumer must be **fully rebuilt** when it adopts this revision; mixing object files across it is an ODR violation with no diagnostic. |

**Two limitations recorded rather than hidden** (design-record section 30). For a
nested call that is simultaneously inverted *and* widening, this port follows the
design's ordering rule (invalid range first) where .NET's override structure
checks widening first — both throw, but with different exception types; the
design's own claim that its order "matches" .NET is corrected there. And a
ThreadSanitizer probe found that concurrent `getCountProperty()` on **one** view
object races on the lazy Count cache that mirrors .NET's
`TreeSubSet._countVersion`. That is documented in the header, not synchronized:
`SortedSet<T>` claims no thread safety and this ticket adds none. Concurrent
read-only access through **distinct** handles over one shared state, including
concurrent handle creation and destruction, is race-free.

Closure evidence:

- 47 permanent regressions in
  `modules/collections/tests/System/Collections/Generic/SortedSetLiveViewTests.cpp`,
  including an `operator<`-only element type, a custom (descending) ordering, a
  copy-counting element type proving `GetViewBetween` copies no elements, and a
  100,000-element scale case; the three pre-existing `GetViewBetween` tests and
  all 41 mutable-`SortedSet` cases pass **unchanged**, with only the stale
  snapshot comment at `Ticket1713VersionTrackingTests.cpp:109` corrected;
- `test/consumer/collections_sorted_set_view.cpp` compiles `-Wall -Wextra
  -Wpedantic -Werror` against only `Collections.Core` (compile-only and linked)
  and exits 0; `test/consumer/collections_sorted_set_view_negative.cpp` is
  correctly **rejected** with `passing 'const …SortedSet<int>' as 'this'
  argument discards qualifiers`;
- ASan+UBSan+LeakSanitizer over the whole new suite (47/47, no diagnostic, no
  leak) and over a post-fix behavior probe (82 assertions, `failures=0`), with
  LSan verified active by a deliberate-leak self-test (232 bytes in 5
  allocations reported); ThreadSanitizer run in three modes with TSan itself
  verified active;
- `scripts/local_ci_check.sh build`: **13,069 tests across 37 executables**, up
  from 13,022, zero build warnings and zero errors;
  `SharpRuntimeTests_Collections_Core` 1,783/1,783 (was 1,736);
- `python3 scripts/validate_module_boundaries.py --root .` at 41 modules/90
  edges with no new dependency edge, `python3
  test/validate_module_boundaries_test.py` 7/7, `python3
  scripts/generate_component_catalog.py --check`, `python3
  scripts/db_consistency_check.py --db plan.sqlite3`, and `git diff --check` all
  pass;
- `scripts/check_doxygen_warnings.sh` at **1,937**/1,942 — five below the
  ceiling: documenting the previously undocumented `Iterator` members removed
  six warnings, and the new `README.md` link to `docs/SortedSetLiveViewDesign.md`
  added one, since `Doxyfile`'s `INPUT` is `modules README.md` and therefore
  cannot resolve a reference into `docs/` (the same warning the four
  pre-existing `docs/` links already produce). The 1,942 ceiling is deliberately
  left in place;
- `scripts/check_selective_components.sh` **was** run this time, with a
  repository-local `TMPDIR`, because a public header changed: all ten components
  pass.

Build directories used, all repository-local and gitignored: the existing
`build` tree (incremental, `--parallel 4`), `build-probe-sortedset` (#1782's
probe tree, extended with the post-fix probes), `build-asan-sortedset` (new,
ASan+UBSan build of the new suite only — deliberately not a whole-repository
sanitizer tree), `build-consumer-sortedsetview`,
`build-consumer-sortedsetview-run`, `build-consumer-sortedsetview-neg`, and
`build-tmp` as the repository-local `TMPDIR`. No build tree was created under
`/tmp`, `/var/tmp`, or `/dev/shm`, and no existing build tree was cleaned or
recreated.

Ticket #1773 remains `blocked` and untouched — it covers the out-of-repository
`ICollection::CopyTo` sweep only, and a downstream sweep for this change is a
separate future item that was not created. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified; both intentionally remain
on an older sharp-runtime revision. No push, merge, rebase, tag, or publication
occurred.

No repair ticket is active.

### Completed SortedSet Count-cache race correction: ticket #1784

**`P1: Eliminate SortedSet live-view Count cache data race`**
(`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`, size S) is **done**, opened and closed
2026-07-28 on local branch `feature/remediation-coll-sortedset-count-race`.

This is a **post-audit defect found by ticket #1783's own ThreadSanitizer
probe**, not an audit finding: it carries **no new `SR-AUD-*` identifier**,
since the numbering is frozen at 364. **SR-AUD-361 stays `remediated`** — this
corrects a defect introduced by that finding's remediation and does not reopen
it. The paragraph above recording #1783's completion is left in place, including
its own honest report that the race existed.

Root cause: #1783 gave a bounded view the lazy Count cache .NET's `TreeSubSet`
keeps (`count`/`_countVersion`), held in two plain `mutable intcs` fields that
the **`const`** `getCountProperty()` wrote. Two threads reading Count through
one unchanged view object performed conflicting non-atomic accesses — a C++
data race and undefined behaviour, even though both calls are observationally
read-only. #1783 classified this as acceptable because "`SortedSet<T>` claims no
thread safety"; **that classification was wrong and is reversed here**. A data
race is UB, not an unhelpful result; the operation is `const` and warns nobody
at the call site; and it is a *regression*, since the pre-#1783 header's `const`
members wrote nothing. The .NET comparison does not carry over either: a racing
`int` write in the CLR is defined and merely stale, and .NET documents that its
collections *do* support multiple concurrent readers.

Pre-fix reproduction, gitignored `build-probe-sortedset/probe10_tsan_count_race.cpp`,
ten modes under `-fsanitize=thread`, none of which ever mutates concurrently:
`known-race` self-test **2 races** (TSan is active), `same-view-count`
**1 race**, `readonly-enumeration` **1**, `nested-views` **2**,
`overlapping-views` **2**; and clean pre-fix already for `copied-handles-count`,
`independent-sets`, `fullset-count`, `sequential-count`, `view-churn`.
`fullset-count` being clean pins the defect as view-specific, since the
owning-set path returns `state_->data.size()` and never touches the cache. The
diagnostic is `Read of size 4 … SortedSet.hpp:315` against
`Previous write of size 4 … SortedSet.hpp:317`, both inside
`getCountProperty() const`.

Five alternatives were **measured**, not argued
(`build-probe-sortedset/probe11_cache_alternatives.cpp` reproduces the real
member sequence and varies only the cache): removing the cache gives
`sizeof(SortedSet<int>)` 40 → **32**, a `std::mutex` **80**, a
`std::shared_mutex` **96**, a published `shared_ptr` snapshot **48** — all
breaking #1783's approved layout — while same-width atomics stay at exactly
**40**/**104**. A cache moved into the shared `State` was rejected structurally:
views have arbitrary overlapping bounds, so one shared count cache would need an
unbounded keyed map, new allocation, and a new element-type requirement.

Selected: **two `std::atomic<intcs>` fields with a release/acquire publication
protocol** — count stored first (`relaxed`), version stored last (`release`),
version loaded first (`acquire`). A reader that observes the new version
therefore also observes the matching count, so the pair can never be read torn;
two relaxed atomics would *not* have sufficed. With no concurrent mutation every
racing thread computes the same value for the same version, so duplicate
publication is harmless and no compare-exchange, retry, or lock is needed.
`state_->version` deliberately stays plain. Two `static_assert`s pin
`sizeof`/`alignof` so a platform that pads its atomics fails to compile rather
than silently re-breaking the ABI.

**The contract, now stated exactly in the header**, in two unequal halves:
concurrent **mutation** is unsupported and undefined, and a set plus every view
over it are one collection for that purpose — *no new promise of concurrent
mutation safety is introduced*; concurrent **read-only** access is race-free,
because no `const` member writes an unsynchronized field. The second half is
what .NET documents for its own collections and what this header had before
#1783. The type is still **not thread-safe**; it is merely free of *internal*
races when read.

**Compatibility: nothing changed.** `sizeof(SortedSet<int>)` **40**,
`sizeof(SortedSet<std::string>)` **104**, `sizeof(Iterator)` **40**, `alignof`
8, and all four value-semantics traits are byte-identical to #1783's stored
probe output; the mangled `GetViewBetween` symbol is unchanged. No public
signature, no semantic change, no consumer rebuild needed on this revision's
account, and no new user approval was required. Count stays O(1) for an owning
set and O(k) once per version for a view; the added cost is one acquire load and
one release store, both plain `mov` on x86-64, with no allocation.

Closure evidence:

- 29 permanent regressions in
  `modules/collections/tests/System/Collections/Generic/SortedSetCountCacheTests.cpp`
  covering the functional Count matrix, the cache-sensitive properties, and
  source/layout compatibility (the exact pointer-to-member type of fourteen
  public members; the published sizes behind a 64-bit guard rather than an
  unconditional `static_assert`, since this repository keeps no permanent
  architecture-specific `sizeof` assertions);
- post-fix ThreadSanitizer: **0 reports in all nine real modes**, with the
  self-test still reporting 2, and #1783's own unmodified `probe9`
  `shared-view-count` going **1 race → 0**;
- ASan + UBSan + LeakSanitizer over both permanent suites: **76/76**, no
  diagnostic, no leak, with LSan verified active by a deliberate-leak self-test
  (4,112 bytes in 102 allocations reported);
- `SharpRuntimeTests_Collections_Core` **1,812/1,812** (was 1,783);
- `scripts/local_ci_check.sh build`: **13,098 tests across 37 executables**
  (was 13,069), zero build warnings and zero errors. `README.md` and `CLAUDE.md`
  were raised from the 13,069 floor to 13,098 only after this gate measured it;
- `scripts/check_selective_components.sh` full ten-component matrix passes, and
  `Collections.Core collections_sorted_set_view.cpp` additionally passes in
  isolation (1,812 tests); the extended positive consumer fixture compiles
  `-Wall -Wextra -Wpedantic -Werror` and exits 0, and the negative fixture is
  still correctly rejected;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, `git diff --check` clean;
- `scripts/check_doxygen_warnings.sh` at **1,937**/1,942 — unchanged from the
  pre-ticket count, five below the ceiling.

Build directories used, all repository-local and gitignored: the existing
`build` tree (incremental, `--parallel 4`), `build-probe-sortedset` (extended
with `probe10`/`probe11` and a TSan build helper), `build-asan-sortedset`
(extended with `build_1784.sh` and an LSan self-test), `build-consumer-1784`,
and `build-tmp` as the repository-local `TMPDIR`. No build tree was created
under `/tmp`, `/var/tmp`, or `/dev/shm`, and no existing build tree was cleaned
or recreated.

Two **inactive** tickets were opened and deliberately not begun, neither
carrying a new `SR-AUD-*` identifier:

- **#1785** (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, P3, XS) — the
  nested-view exception-ordering divergence #1783 recorded in design section
  30.4. A nested call that is simultaneously inverted *and* widening throws
  `ArgumentException` here and `ArgumentOutOfRangeException` in .NET, because
  design #1782 intentionally validates the argument pair before validating it
  against object state. Changing that is a semantic decision, not a bug fix.
  #1784 was explicitly scoped not to touch it.
  *(Now **done**, closed 2026-07-28 under explicit user approval of acceptance
  branch (b) — .NET's order. See "Completed SortedSet nested-view exception
  ordering" below. The paragraph above is left as #1784 wrote it.)*
- **#1786** (`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, S) — `State::version` is
  `int32_t`, incremented without bound, and compared only for **equality** by
  both the Count cache and `Iterator::checkVersion`. After `INT32_MAX` mutations
  the increment is signed overflow (UB in C++, defined wrapping in .NET), and a
  wrap to a previously observed value would silently accept a stale cache or
  iterator. Both properties **predate #1783** (they arrived with ticket 1713)
  and #1784 changed only the memory ordering, not the values, the type, or the
  equality test. Likely repository-wide across every collection ticket 1713
  touched, so the assessment must precede any change.

Ticket #1773 remains `blocked` and untouched. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred.

No repair ticket is active.

### Completed SortedSet mutation-counter repair: ticket #1786

**`P3: Assess the int32 mutation-version counter for overflow and reuse`**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW`, size S) is **done**, opened inactive by
#1784 and closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-version-overflow`. It carries **no new
`SR-AUD-*` identifier** (numbering frozen at 364), does **not** reopen
SR-AUD-361, and is **not** a regression from #1783 or #1784: the counter, its
type, and its increment all arrived with ticket 1713.

The ticket was opened as an assessment. The assessment concluded that a fully
compatible repair exists, so it was implemented in the same ticket rather than
deferred; the full record is
[`docs/SortedSetVersioningDesign.md`](docs/SortedSetVersioningDesign.md).

**Four defects, all reproduced against the real production header before
anything changed** (gitignored `build-probe-sortedset/probe12_version_overflow.cpp`,
one source built against both the committed pre-fix header and the working tree,
positioning the counter with GCC's `-fno-access-control` rather than performing
billions of mutations):

1. `++state_->version` at `INTCS_MAX` is **signed-integer overflow**. UBSan:
   `SortedSet.hpp:425:20: runtime error: signed integer overflow: 2147483647 + 1
   cannot be represented in type 'int'` inside `SortedSet<int>::Add`. Threshold
   2^31 − 1 mutations -- about 20 seconds of hot looping.
2. 2^32 mutations later the counter returns to a value an outstanding `Iterator`
   captured and the guard **silently accepts the stale iterator** (probe:
   dereference yielded 10, no throw), enumerating a container that changed under
   it.
3. The same wrap **silently revalidates a stale cached view `Count`** (probe:
   answered 4 where the range held 3). Deterministically constructible, since
   `Add`+`Remove` is two increments.
4. **Not in the ticket's description, and the worst of the four:**
   `kCountNotCached` was `-1`, which the counter itself reaches after 2^32 − 1
   mutations, so a view that had **never** computed its Count read its cache as
   warm and answered `0` where the range held 5. Unlike 2 and 3 it needs no
   prior observation -- every view over that state is wrong for its whole life.

The header's own claim that the counter "never legitimately holds this value"
was false, not merely optimistic.

**.NET comparison, read from the current sources.** `SortedSet<T>.version` is
`int`, incremented unchecked, compared for equality only, and `TreeSubSet`
initialises `version = -1; _countVersion = -1;` -- **the same sentinel with the
same latent collision**. So .NET has defects 2, 3, and 4 as *defined-but-wrong*
behaviour. Matching .NET's width therefore does **not** make the C++ code
correct: the CLR defines signed overflow as wrapping, C++ makes it undefined.
sharp-runtime deliberately exceeds .NET here.

**Selected:** the shared counter and the `Iterator` snapshot become
`SharpRuntime::ulongcs` (64-bit unsigned), so every increment is defined for
every representable prior value and a repeat needs 2^64 mutations -- over 580
years of uninterrupted mutation. The widening is **free**: the counter is not a
member of `SortedSet<T>`, and `Iterator` already had four bytes of tail padding.
The Count cache's tag **cannot** be widened -- measured member offsets show
`sizeof(SortedSet<int>)` 40 and `sizeof(SortedSet<std::string>)` 104 have no
spare byte, and an exact count needs 31 of the 64 available bits -- so the tag
is instead stored **biased by one and compared widened**: a never-filled cache
holds 0 and `version + 1` is never 0, a tag filled at V matches only while the
counter is exactly V, and once the counter outgrows the tag nothing matches and
the cache stops being written, so a view's `Count` becomes an O(k)
recomputation. Six alternatives were evaluated and rejected with reasons,
including State renewal, which cannot preserve #1783's live-view graph because
rebinding only the owning set would split it.

**A real performance regression was found, measured, and removed.** The first
implementation used an explicit horizon *branch* on the read path; with
loop-invariant hoisting defeated by a compiler barrier it cost **+1 ns on every
`Count` call, including an owning full set's**, which cannot even reach that
branch, because it enlarged the inlined body. Two variant headers isolated it
(counter widening alone: no cost; shipped cache minus the branch: no cost). The
biased tag delivers the same exactness with no branch and no regression.

Closure evidence:

- 29 permanent regressions in
  `modules/collections/tests/System/Collections/Generic/SortedSetVersionOverflowTests.cpp`
  covering ordinary counter behaviour, the old int32 boundary, iterator
  invalidation across the old wrap point, the Count cache's sentinel/reach/
  exactness, `std::string` and `operator<`-only element types, and source/layout
  compatibility. Near-boundary cases position the counter through a **test-only
  friend seam** (`SharpRuntime::Testing::SortedSetVersionAccess<T>`, declared and
  befriended in the header, defined only in that test file) rather than through
  a production hook; no test performs more than 300 mutations;
- UBSan: the signed-overflow report pre-fix, **0 diagnostics** post-fix across
  all six probe modes;
- ASan + UBSan + LeakSanitizer over all three permanent SortedSet suites:
  **105/105**, no diagnostic, no leak, LSan verified active by a deliberate-leak
  self-test (4,112 bytes in 102 allocations);
- ThreadSanitizer: #1784's ten-mode probe **0 races** in all nine real modes,
  #1783's unmodified `probe9` `shared-view-count` **0**, and a new six-mode
  probe covering the recompute-past-the-tag path **0 races** in all five real
  modes -- with both self-tests still reporting races, so the zeroes are
  evidence. No mode ever mutates concurrently;
- performance: every benchmarked operation within run-to-run noise, warm view
  `Count` slightly faster (1.03 → 0.76 ns); memory per set and per iterator
  unchanged; no allocation added anywhere;
- `SharpRuntimeTests_Collections_Core` **1,841/1,841** (was 1,812), with all 47
  `SortedSetLiveViewTests`, all 29 `SortedSetCountCacheTests`, and all 41
  pre-existing SortedSet cases passing and **no assertion edited**;
- `scripts/local_ci_check.sh build`: **13,127 tests across 37 executables**
  (was 13,098), zero build warnings and zero errors. `README.md` and `CLAUDE.md`
  were raised from the 13,098 floor to 13,127 only after this gate measured it;
- ABI/layout: `sizeof` 40/104/40, `alignof` 8, all four value-semantics traits,
  and the mangled `GetViewBetween` symbol byte-identical to #1784's stored probe
  output; a dedicated offset probe additionally confirms **every member offset**
  of `SortedSet<T>` and `Iterator` is unchanged. The only difference is the
  *width* of two private fields, one of which lands in existing padding;
- both consumer fixtures behave as before: the positive one compiles
  `-Wall -Wextra -Wpedantic -Werror` and exits 0, the negative `const` caller is
  still correctly rejected;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, `git diff --check` clean;
- `scripts/check_doxygen_warnings.sh` at **1,937**/1,942 -- unchanged from the
  pre-ticket count;
- `scripts/check_selective_components.sh` full ten-component matrix passes, and
  `Collections.Core collections_sorted_set_view.cpp` additionally passes in
  isolation (1,841 tests).

Build directories used, all repository-local and gitignored: the existing
`build` tree (incremental, `--parallel 4`), `build-probe-sortedset` (extended
with probes 12–17, a pre-fix header checkout, and a pre-fix build helper),
`build-asan-sortedset` (extended with `build_1786.sh`), `build-consumer-1786`,
`build-consumer-1786-neg`, and `build-tmp` as the repository-local `TMPDIR`. No
build tree was created under `/tmp`, `/var/tmp`, or `/dev/shm`, and no existing
build tree was cleaned or recreated.

**One inactive ticket was opened and deliberately not begun.** **#1787**
(`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, M) -- fourteen other
collections carry the identical `intcs version_` counter, incremented without
bound and compared for equality only, so defects 1 and 2 apply to all of them
(defects 3 and 4 are specific to `SortedSet<T>`'s Count cache). #1786's stored
acceptance criteria asked for a repository-wide implementation; the instruction
governing this session scoped #1786 to `SortedSet<T>` and required the rest to
be a separate inactive ticket. That divergence is recorded in the design
document rather than silently absorbed, and the full inventory the criteria
asked for is delivered there. Measured sizes suggest none of the fourteen can be
assumed to have `SortedSet<T>::Iterator`'s spare padding, so #1787 must measure
member offsets per type and some may need layout approval.

Ticket #1785 remains `todo` and untouched; no exception ordering changed.
Ticket #1773 remains `blocked` and untouched. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred.

**Ticket #1787 is now done — see the next section.**

### Completed repository-wide mutation-counter sweep: ticket #1787

**`P3: Apply the mutation-counter overflow repair to the remaining fourteen
collections`** (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, size M) is **done**,
opened inactive by #1786 and closed 2026-07-28 on local branch
`feature/remediation-coll-version-counter-sweep`. It carries **no new
`SR-AUD-*` identifier** (numbering frozen at 364), reopens no audit finding, and
is not a regression from #1783, #1784, or #1786: the counters, their type, and
their increments all arrived with ticket 1713. The full record is
[`docs/CollectionVersionCounterSweep.md`](docs/CollectionVersionCounterSweep.md).

**Three corrections to the inherited scope, recorded rather than absorbed:**

1. The count is **sixteen** counter-carrying types, not the fifteen #1786's §16
   inventory listed. `BitArray` was missed, and it is also the one type whose
   counter was already `std::uint32_t` rather than `intcs` — which is exactly why
   it never had the signed-overflow undefined behaviour, confirmed by its
   producing no UBSan report where all fourteen others did. #1786's claim that
   "all fifteen declare it `intcs`" was therefore wrong on two counts.
2. A **third defect class** was found that appears in neither ticket's
   description and is the worst of the three: the implicitly declared copy/move
   assignment operator transplanted the *source's* counter into the destination,
   so an enumerator outstanding over the destination saw no change even though
   the assignment had just destroyed every element it could refer to. It needs
   **no overflow at all** — the two counters merely have to be equal, which two
   collections that have taken the same number of mutations routinely are. Six of
   the fourteen affected types reproduced as AddressSanitizer
   `heap-use-after-free` or `heap-buffer-overflow` (`HashSet`, `Dictionary`,
   `SortedDictionary`, `OrderedDictionary`, `Hashtable`,
   `ListDictionaryInternal`) rather than merely as wrong answers.
   `LinkedList<T>` was immune, because ticket #1769 had already given it a
   bumping `operator=`; `SortedSet<T>` is immune because its `Iterator`
   co-owns the shared `State` through a `shared_ptr`.
3. #1786 asserted its defects 3 and 4 (a stale cached `Count`, a colliding
   sentinel) were specific to `SortedSet<T>`. That is now **confirmed with
   evidence** rather than repeated: no other collection caches anything against
   its counter or reserves a counter value, asserted per type at five counter
   positions including the maximum.

**Pre-fix evidence**, all taken against the *committed* headers before anything
changed (gitignored `build-probe-collversion/`, one probe source built against
both header revisions, counters positioned with `-fno-access-control` rather
than by performing billions of mutations):

- **14** UBSan signed-integer-overflow reports, one per collection, e.g.
  `Generic/List.hpp:116:68: runtime error: signed integer overflow: 2147483647 +
  1 cannot be represented in type 'int'`;
- **15** iterator/enumerator ABA reproductions — every one of the fifteen guards
  stops firing once the counter is positioned 2^32 forward;
- **8** assignment-alias reproductions on the index-based collections;
- **6** ASan memory errors on the node-based ones.

**.NET comparison, read from the current sources.** Every corresponding .NET
collection uses `int`, unchecked, equality-compared, with no sentinel outside
`SortedSet`'s `TreeSubSet` — and `Hashtable.cs:704-708` says so explicitly:
"Version might become negative when version is int.MaxValue, but the oddity will
be still be correct." That is true in C#, where signed overflow is defined, and
is exactly how fourteen instances of undefined behaviour reached this
repository. .NET has **no analogue of the assignment defect at all**, because
its collections are reference types; its `Clone` methods copy the counter, which
is the *copy-construction* case and is what this port now matches exactly.

**Selected:** the new `System::Collections::detail::BasicMutationCounter`, whose
increment is unsigned (defined for every representable prior value) and whose
**assignment advances the destination instead of taking the source's value**,
with copy construction still inheriting it. Thirteen collections take the 64-bit
`MutationCounter`. `LinkedList<T>` and `BitArray` take the 32-bit
`NarrowMutationCounter`, because widening them grows a public object —
`sizeof(LinkedList<int>)` 40 → 48 and `sizeof(BitArray::Enumerator)` 32 → 40,
arithmetically unavoidable in any member order, since both are exactly packed
with zero spare bytes at the counter's position. Eight alternatives were
evaluated and rejected with reasons, including per-mutation generation tokens, a
saturating counter (silently invalid: once saturated a snapshot never fail-fasts
again), checked exhaustion (a branch on every mutation; #1786 measured a real
+1 ns from exactly one such branch), and moving the counter behind a
`shared_ptr` the way `SortedSet<T>` does.

Closure evidence:

- **336** permanent regressions in
  `modules/collections/tests/System/Collections/CollectionVersionCounterTests.cpp`
  — two typed suites over 11 enumerator-shaped and 4 iterator-shaped adapters
  plus 12 targeted cases with 30+ `static_assert`s — covering the ticket-1713
  fail-fast contract, the old int32 boundary at seven distinct values, counter
  exhaustion, the 2^32 alias at three multiples, all four assignment paths,
  copy/move, empty and single-element collections, public-interface iteration
  through `ICollection*`/`IEnumerator*`, `std::string` and `operator<`-only
  element types, and source/layout/trait compatibility. Near-boundary cases
  reach every counter through **one** test-only friend seam,
  `SharpRuntime::Testing::CollectionVersionAccess<T>`, declared in the new
  header and defined only in that test file — generalising #1786's per-type
  seam. No test performs more than a few dozen real mutations;
- **no existing assertion was edited**, and all 1,841 pre-existing
  Collections.Core tests still pass, which is itself evidence that the
  assignment repair breaks no established behaviour;
- UBSan: 14 signed-overflow reports pre-fix, **0 diagnostics** post-fix across
  all six probe modes. ASan: 6 memory errors pre-fix, **0** post-fix;
- ASan + UBSan + LeakSanitizer over the permanent suites: **349/349**, no
  diagnostic, no leak. LSan is verified active twice over — it caught a **real
  24-byte leak in the first draft of this ticket's own test** (a caller-owned
  `Hashtable::getKeysProperty()` view), and the repository's deliberate-leak
  self-test still reports 4,112 bytes in 102 allocations;
- ThreadSanitizer: **0 races** in all three real modes (four threads enumerating
  one shared instance of twelve collections; independent instances mutating
  freely; concurrent copy construction from a shared const source), with the
  deliberate-race self-test still reporting 2. This ticket adds no atomic, no
  `mutable` cache, and no hidden `const` write, so the probe exists to
  substantiate that rather than assert it. **No concurrent-mutation safety is
  claimed** — no mode ever mutates from two threads at once;
- layout: every affected container's and enumerator's `sizeof`, `alignof`, and
  **counter offset** unchanged, measured per type. The claim is deliberately not
  "`sizeof` is unchanged" alone: the counter's width grew into padding the type
  already had at that exact position, and where no such padding existed the
  widening was **not performed**;
- symbols: **0 removed or renamed**, 10 new *weak* inline definitions for the
  new counter class's constructors, `operator++`, and conversion operator —
  emitted by the consumer's own translation unit exactly as
  `detail::EnumeratorState`'s already are, since `Collections.Core` is an
  `INTERFACE` target with no archive;
- performance: every benchmarked path within run-to-run noise. The one apparent
  7% outlier (`Dictionary` copy assignment) was re-measured three more times and
  the ranges overlap, with a ~12 ns spread on both sides;
- `SharpRuntimeTests_Collections_Core` **2,177/2,177** (was 1,841);
- `scripts/local_ci_check.sh build`: **13,463 tests across 37 executables**
  (was 13,127), zero build warnings and errors, exit 0, no test disabled or
  filtered. `README.md` and `CLAUDE.md` were raised from the 13,127 floor only
  after this gate measured it;
- both new consumer fixtures behave as intended: the positive one compiles
  `-Wall -Wextra -Wpedantic -Werror` against only `Collections.Core` and exits
  0; the negative one is correctly rejected with `error: incomplete type
  'SharpRuntime::Testing::CollectionVersionAccess<…>' used in nested name
  specifier`, proving the seam gives a consumer nothing;
- boundaries 41 modules/90 edges, validator tests 7/7, catalogue current,
  database consistent, `git diff --check` clean;
- `scripts/check_doxygen_warnings.sh` at **1,938**/1,942, one more than the
  pre-ticket 1,937. The full warning list was diffed against `HEAD`: exactly one
  addition, zero removals, and it is the single new `README.md` markdown link
  into `docs/`, which `Doxyfile` does not scan. Six such README links already
  existed and each costs the same warning; the ceiling is untouched. Disclosed
  rather than described as unchanged;
- `scripts/check_selective_components.sh` full ten-component matrix passes, and
  `Collections.Core collections_mutation_version.cpp` additionally passes in
  isolation (2,177 tests).

Build directories used, all repository-local and gitignored, all at **at most
four compilation jobs**: the existing `build` tree (incremental,
`--parallel 4`), the new `build-probe-collversion` (probes 1–5, a pre-fix header
checkout, and two build helpers), the new `build-asan-collversion`
(`build_1787.sh`), `build-consumer-1787`, `build-consumer-1787-neg`, and
`build-tmp` as the repository-local `TMPDIR`. `scripts/check_selective_components.sh`
uses `mktemp -d` internally and was run with `TMPDIR` redirected into
`build-tmp/selective`; it already caps its own builds at `--parallel 4`. No
build tree was created under `/tmp`, `/var/tmp`, or `/dev/shm`, and no existing
build tree was cleaned or recreated.

**Three tickets were opened and deliberately not begun.** **#1788**
(`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, S) is **blocked** pending explicit
approval that `sizeof(LinkedList<T>)` may grow 40 → 48 on LP64. **#1789**
(`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, XS) is **blocked** pending explicit
approval that `sizeof(BitArray::Enumerator)` may grow 32 → 40. They are
deliberately **two** tickets: they share the symptom and nothing else — one
grows a container, the other a public enumerator, the ABI blast radius differs,
and a user might reasonably approve one and not the other. **#1790**
(`REMED-COLL-LIST-INDEXER-VERSION`, P3, L) is inactive `todo` and records the
separate, pre-existing, non-versioning divergence that `List<T>::operator[]`
returns a plain `T&` and so cannot bump the counter the way .NET's index setter
does — documented in `List.hpp` since ticket 1713, neither introduced nor
worsened here.

Ticket #1785 remains `todo` and untouched; no exception ordering changed.
Ticket #1773 remains `blocked` and untouched. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred. No compilation used more than four jobs.

**Ticket #1790 is now design-complete — see the next section.**

### Completed List<T> indexer versioning design: ticket #1790

Ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`, P3, size L, category `parity`)
is **done as a design ticket**, closed 2026-07-28 on local branch
`feature/remediation-coll-list-indexer-design`. It carries **no `SR-AUD-*`
identifier** — the numbering stays frozen at 364. It **changed no production
behaviour, no public signature, no object layout, and no exception**; the one
production edit is a doc-comment correction in `List.hpp`. The durable record is
`docs/ListIndexerVersioningDesign.md`.

**The answer to the ticket's own question: no fully source-compatible correction
exists.** A plain `T&` cannot be intercepted — once the caller holds it, no C++
mechanism notifies the collection of a write through it. Every closing
alternative changes what the non-const indexer returns, which is a public source
break. Acceptance-criteria route (a) — record the divergence as permanent — was
rejected because the same `T&` is a *reproduced use-after-free*, not merely a
fail-fast divergence.

The selected architecture is a tracked proxy,
`System::Collections::detail::ElementReference<T>`, returned by the non-const
indexer: it reads as `const T&`, intercepts every write, and advances the
counter. It is chosen because it is the only alternative that closes the write
path while keeping **`list[i] = v`**, the exact spelling C# uses, compiling.

**Three of the ticket's own premises were corrected rather than inherited.**

1. **The indexer is not the widest hole.** The non-const `ToVector()` hands out
   the whole backing `std::vector<T>&`, so a caller can `push_back`, `erase`, or
   `clear` through it — a **structural** mutation the fail-fast guard never
   sees, strictly wider than the indexer, which can only replace an element.
   Reproduced (`Count` went to 0 with an enumerator outstanding and the guard
   silent) and previously undocumented anywhere.
2. **The migration premise was wrong for this repository.** The ticket recorded
   `operator[]` as "the single most call-site-heavy method in this repository".
   Measured across **all 625 translation units** by compiling against a
   `[[deprecated]]`-tagged shim, the non-const indexer has **61 call sites, all
   in two test files**, and **no library source in the repository includes
   `List.hpp` at all**. The CNA/mobile-eggbert burden is real, unmeasured, and
   out of scope — it is explicitly *not* claimed to be small.
3. **`IList<T>` has four implementers, not one:** `List<T>`,
   `ObjectModel::Collection<T>`, `ObjectModel::ReadOnlyCollection<T>`, and a
   hand-written one in the test suite — the last being direct evidence that
   consumer code implements the interface by hand. A grep for `public IList<`
   finds only the first; the other three spell it `public Generic::IList<T>`.
   They were found by compiling the repository against the candidate header, not
   by searching. `Collection<T>` **has no mutation counter at all**, which is why
   Phase 2 carries a second object-size consequence.

Evidence, all repository-local and gitignored under `build-probe-listindexer/`:
a five-mode reproduction probe recording that a native iterator, a fail-fast
enumerator, an equal-value write, and a write through `IList<T>&` all leave the
counter at rest while `Add()` correctly invalidates; **four AddressSanitizer
heap-use-after-free reports** for references retained across reallocation (read
and write), across `Clear()`, and across move assignment; 0 UBSan diagnostics on
every non-lifetime mode; a 24-case expression matrix compiled per candidate; and
four generated header shims compiled against the whole repository at
`-fsyntax-only`. Measured source break: the **refined proxy breaks 1 site in 1
of 625 translation units** (the hand-written implementer — migration, not
call-site breakage), against **8 sites in 3 units** for the rejected
value/setter alternative, of which only 2 are genuine call-site breaks. Those
figures are close, and the decision is explicitly *not* made on them: it is made
because the value alternative deletes `list[i] = v` from the API. Layout
measured: `sizeof(List<T>)` **40 → 40** unchanged, `sizeof(Collection<T>)`
**32 → 40**, and the proxy is a 16-byte prvalue.

Closure evidence: **14 new permanent regressions** in
`modules/collections/tests/System/Collections/Generic/ListIndexerVersionTests.cpp`,
deliberately split into a `Contract` suite (8 cases that must survive #1791
unchanged) and a `Divergence` suite (6 cases, each asserting today's behaviour
with .NET's named in a comment, carrying `static_assert`s that #1791 physically
cannot land without editing); `SharpRuntimeTests_Collections_Core`
**2,191/2,191** (was 2,177), no existing assertion edited;
`scripts/local_ci_check.sh build` at **13,477 tests across 37 executables** (was
13,463), zero warnings, zero errors; module boundaries unchanged at 41 modules /
90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 at **1,938**/1,942 — unchanged, since
`docs/` is not scanned, tests are excluded, and the `List.hpp` comment edit
introduced no new warning.

Build directories used, all repository-local and gitignored, all at **at most
four compilation jobs**: the existing `build` tree (incremental,
`--parallel 4`; re-configured once with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`,
which changes no compile flag and forced no recompilation), the new
`build-probe-listindexer` (probes, four generated shims, and the two sweep
scripts, whose parallelism is a hard-coded constant 4 with no `nproc` or
hardware-concurrency call anywhere), and `build-tmp` as the repository-local
`TMPDIR`. `scripts/check_selective_components.sh` and
`scripts/local_ci_check.sh` both use `mktemp` internally and were run with
`TMPDIR` redirected into `build-tmp`; both already cap their own builds at
`--parallel 4`. No build tree was created under `/tmp`, `/var/tmp`, or
`/dev/shm`, and no existing build tree was cleaned or recreated.

**Two tickets were opened and deliberately not begun.** **#1791**
(`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, L) is **blocked**. It has two
phases: **Phase 1 needs no approval** (add tracked `getItem`/`setItem` to
`List<T>` — a pure addition that gives callers a correct path but does not close
the defect), and **Phase 2 is blocked** pending the exact four-part approval in
design section 28 — a public source break to `List<T>::operator[]`, a public
source break to the `IList<T>` interface affecting every implementer including
hand-written consumer ones, an object-layout change to `Collection<T>`
(`sizeof` 32 → 40, full consumer rebuild), and acknowledgement that CNA's and
mobile-eggbert's usage is unmeasured. The approvals granted for #1771, #1780,
and #1783 explicitly do **not** carry over. The unavoidable cost, stated rather
than buried: `list[i].member` and `list[i].method()` stop compiling for
value-type elements, because `operator.` cannot be overloaded.

**#1792** (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, M) is inactive `todo`
and records a **newly discovered, previously unrecorded defect** found by
#1790's inventory and deliberately not absorbed into it:
`Generic::IEnumerator<T>::getCurrentProperty()` does
`return const_cast<T*>(&Current());`, publishing a mutable `void*` to the live
element on a public interface. `Current()` returns `const T&` precisely so an
enumerator cannot mutate what it walks. Reproduced: the write landed, the
counter never moved, and the guard stayed silent. It affects **every** collection
in the repository, not `List<T>`, which is why it is its own ticket. No new
`SR-AUD-*` identifier.

Ticket #1785 remains `todo` and untouched. Tickets #1788 and #1789 remain
`blocked` and untouched; no counter was widened. Ticket #1773 remains `blocked`
and untouched. CNA and mobile-eggbert were not inspected, searched, configured,
built, or modified. No push, merge, rebase, tag, or publication occurred. No
compilation used more than four jobs.

**Ticket #1792 is now done — see the next section.**

### Completed enumerator Current safety design: ticket #1792

Ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M, category
`defect`) is **done as a design ticket**, closed 2026-07-28 on local branch
`feature/remediation-coll-ienumerator-current-design`. It carries **no
`SR-AUD-*` identifier** — the numbering stays frozen at 364. It **changed no
production behaviour, no public signature, no object layout, and no exception**;
it edits no production source at all. The durable record is
[`docs/IEnumeratorCurrentSafetyDesign.md`](docs/IEnumeratorCurrentSafetyDesign.md).

**The answer to the ticket's own question: the divergence is remediable and is
*not* recorded as deliberate.** `System::Collections::IEnumerator::getCurrentProperty()`
returns `void*`, and the generic bridge fills it with
`const_cast<T*>(&Current())`, so a consumer holding nothing but the public
non-generic interface obtains a writable, untyped, unbounded-lifetime pointer
into the live storage of the collection it is walking — including collections
whose own members refuse to be mutated. The selected architecture is
**Alternative B: the non-generic accessor returns `std::any` by value**, the
direct C++ counterpart of .NET's `object IEnumerator.Current`, which returns a
value, boxes value types, and hands out no pointer at all. The typed
`Generic::IEnumerator<T>::Current()` is **unchanged** at `const T&`.

**`const void*` was measured and rejected as a fix.** It closes const-correctness
and nothing else: the probe performs the one-line `const_cast` a determined
consumer writes, and the write lands. So does the richer read-only descriptor
candidate. Only the two candidates that stop returning an address into live
storage — a boxed value, or an enumerator-owned copy — actually close the write
path.

**Four of the ticket's own premises were corrected rather than inherited.**

1. **The defect does not reach "every collection in the repository."** The
   ticket description, this file's #1790 section, and the per-file audit note all
   said it does. `Dictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, and
   `SortedDictionary<K,V>` implement **no `IEnumerator` at all** — they expose
   STL-style version-checked iterators. The measured reach is **thirteen**
   generic enumerator implementations plus **eight** non-generic ones, plus two
   hand-written test-local implementers.
2. **The bridge's `const_cast` is neither the only one nor the worst.**
   **Four** further `const_cast`s live outside it — `ArrayList`,
   `Hashtable`'s member view, and `ListDictionaryInternal` twice — so repairing
   only the bridge would leave every one of them. One of them publishes a
   writable pointer to a **live `std::unordered_map` key**.
3. **It is not one defect but six**, with different scopes and different fixes:
   const-correctness, mutation/version bypass, type safety, lifetime, ownership
   ambiguity, and generic/non-generic inconsistency. `const void*` closes one of
   the six; a descriptor closes two; an enumerator-owned copy closes four;
   `std::any` closes all six.
4. **The most dangerous property is the ABI, not the source break.** Under the
   Itanium C++ ABI a non-template function's return type is not part of its
   mangled name: `void*`, `const void*`, and `std::any` all produce the
   byte-identical symbol, while the calling convention differs — `this` moves
   from `%rdi` to `%rsi` under `std::any`'s sret return. **A partially rebuilt
   consumer links with no diagnostic and then corrupts memory.** No candidate
   avoids this, so it is a requirement on the release rather than an argument
   between alternatives.

Evidence, all repository-local and gitignored under `build-probe-ienumerator/`:
a six-mode reproduction probe recording that a write through the `void*` changed
live storage with the mutation counter at rest and the fail-fast guard silent for
`List`, `Queue`, `Stack`, `LinkedList`, `SortedList`, and
`ObjectModel::Collection` (which has **no counter at all**), while `Add()`
correctly invalidates; a `ReadOnlyCollection<T>` whose non-const indexer throws
`NotSupportedException` and whose enumerator nonetheless mutated both the wrapper
and the caller's shared backing vector; a `Hashtable` key view whose write
rewrote a live hash-map key in place so that, with 64 entries, the entry became
unreachable by **both** its old and its new key while `Count` still reported it;
**four AddressSanitizer `heap-use-after-free` reports** for pointers retained
across reallocation, `Clear()`, the collection's destruction, and the
enumerator's destruction, plus two non-faulting stale-aliasing shapes across
`MoveNext()` and `Reset()`; 0 UBSan diagnostics on every mode and 0 LSan leaks;
a bounded, sanitizer-controlled type-erasure probe showing a same-width wrong
cast is silently wrong with no diagnostic; a five-candidate side-by-side
allocation and layout measurement; disassembly of the calling-convention change;
and a 626-translation-unit deprecation sweep measuring **28** non-generic, **4**
bridge, and **27** typed call sites (0 compile failures). Three fully migrated
header shims recompiled against the whole repository break **6**, **7**, and
**6** translation units at **12**, **14**, and **12** distinct sites — and **zero
library sources under any of them**, so the design's proposed bodies are
compile-validated rather than sketched.

Closure evidence: **17 new permanent regressions** in
`modules/collections/tests/System/Collections/EnumeratorCurrentSafetyTests.cpp`,
deliberately split into a `Contract` suite (8 cases that must survive #1793
unchanged) and a `Divergence` suite (9 cases, each asserting today's behaviour
with .NET's named in a comment, carrying `static_assert`s that #1793 physically
cannot land without editing); `SharpRuntimeTests_Collections_Core`
**2,208/2,208** (was 2,191), no existing assertion edited;
`scripts/local_ci_check.sh build` at **13,494 tests across 37 executables** (was
13,477), zero warnings, zero errors; module boundaries unchanged at 41 modules /
90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; all ten selective components plus every consumer
fixture passing; Doxygen 1.9.8 at **1,938**/1,942 — unchanged, since `docs/` is
not scanned and tests are excluded.

Build directories used, all repository-local and gitignored, all at **at most
four compilation jobs**: the existing `build` tree (incremental,
`--parallel 4`), the new `build-probe-ienumerator` (probes, the ABI object
files, four generated shims, and the two sweep scripts, whose parallelism is a
hard-coded constant 4 with no `nproc` or hardware-concurrency call anywhere), and
`build-tmp` as the repository-local `TMPDIR`.
`scripts/check_selective_components.sh` and `scripts/local_ci_check.sh` both use
`mktemp` internally and were run with `TMPDIR` redirected into `build-tmp`; both
already cap their own builds at `--parallel 4`. No build tree was created under
`/tmp`, `/var/tmp`, or `/dev/shm`, and no existing build tree was cleaned or
recreated.

**One ticket was opened and deliberately not begun** — **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, L), which is now
**done**; see the section after next. As opened by #1792 it was **blocked**. It has
two phases: **Phase 1 needs no approval** (write the ownership, lifetime, and
validity rules into both headers and correct the misleading "cast to the
appropriate type" comment — which does not close the defect), and **Phase 2 is
blocked** pending the exact three-part approval in design section 33 — a public
source break to `System::Collections::IEnumerator`, a public source break to
`Generic::IEnumerator<T>` adding a `NotSupportedException` path for element types
that cannot be copied, and acknowledgement of the **silent ABI break** requiring
a full consumer rebuild. There is **no object-layout change**, so this approval
is narrower than #1788's, #1789's, or #1791 Phase 2's in that respect and wider
in the ABI one. The approvals granted for #1771, #1780, and #1783 explicitly do
**not** carry over.

**#1793 should be implemented before #1791**, and the two must not be merged.
They are independent defects on disjoint surfaces — #1791 changes
`IList<T>::operator[]`, #1793 changes `IEnumerator::getCurrentProperty()`, and
neither repairs the other. #1793 goes first because it needs no object-layout
change while #1791 Phase 2 grows `ObjectModel::Collection<T>` from 32 to 40
bytes, and because #1793's break is loud everywhere it is a read and impossible
where it is a write, while #1791's silently changes what `list[i]` *means* in
expressions that still compile.

Two residual limitations are stated rather than buried. **The typed
`Current()` reference hazard is not closed** — `&Current()` retained across a
mutation is still a reproduced use-after-free; closing it would need a by-value
`Current()`, which makes move-only `T` uninstantiable. And
**`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` keep returning
`const void*` into live storage**; they are already const-correct, so the
const-correctness class does not apply, but the type-safety, lifetime, and
ownership classes do. Both are recorded in design sections 30 and 15 as
deliberate exclusions with the reasoning attached.

Ticket #1785 remains `todo` and untouched. Tickets #1788 and #1789 remain
`blocked` and untouched. Ticket #1791 remains `blocked` and untouched; no indexer
changed. Ticket #1773 remains `blocked` and untouched. CNA and mobile-eggbert
were not inspected, searched, configured, built, or modified. No push, merge,
rebase, tag, or publication occurred. No compilation used more than four jobs.

**Ticket #1793 is now done — see the next section.**

### Completed enumerator Current safety implementation: ticket #1793

Ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, size L,
category `defect`) is **done**, closed 2026-07-28 on local branch
`feature/remediation-coll-ienumerator-current-safety`. It carries **no
`SR-AUD-*` identifier** — the numbering stays frozen at 364, and SR-AUD-356
remains `remediated` from ticket #1767 rather than being reopened. The user
granted design section 33's three-part approval explicitly and scoped to this
ticket; the approvals for #1771, #1780, and #1783 did not carry over, and this
one does not carry to #1791, #1788, or #1789. Both phases landed together,
because Phase 1's documentation would have contradicted the headers if it had
landed separately. The durable record is
[`docs/IEnumeratorCurrentSafetyDesign.md`](docs/IEnumeratorCurrentSafetyDesign.md)
section 34; everything above that section is #1792's design record, unchanged —
the unsafe pointer and its reproductions stay on the record exactly as measured.

**What changed.** `System::Collections::IEnumerator::getCurrentProperty()`
returns an **owning `std::any` by value** instead of a mutable `void*`, the
direct C++ counterpart of .NET's `object IEnumerator.Current`.
`Generic::IEnumerator<T>::Current()` is **unchanged** at `const T&`; its
inherited bridge is now `std::any(Current())`, throwing
`System::NotSupportedException` for an element type that cannot be copied —
.NET's own documented answer for a `ref struct` element type. All **four**
`const_cast`s outside the bridge are gone, and both `mutable` members
(`Hashtable::Enumerator::current_`, `BitArray::Enumerator::current_`) are
ordinary members again. Eight production non-generic overrides, the one bridge
covering thirteen production generic implementations, two hand-written
test-local implementers, and the three in-library call sites migrated — the
exact counts #1792 measured, confirmed by compilation. **Zero library sources
broke**, as the design's shim sweep predicted.

**The defect was reconfirmed before any production edit.** The #1792 probes were
re-run and their output preserved under
`build-probe-ienumerator/prefix1793/`: 15 defects across six modes, including a
`List<int>` element rewritten with the mutation counter at rest and the
fail-fast guard silent, a `ReadOnlyCollection<T>` mutated through its own
enumerator into the caller's shared backing vector, a `Hashtable` key rewritten
in place so that with 64 entries the entry was unreachable by **both** its old
and its new key while `Count` still reported it, **four AddressSanitizer
`heap-use-after-free` reports**, and a same-width wrong cast that was silently
wrong with no diagnostic from any sanitizer.

**Four corrections to the design's section 14 sketch**, all recorded in
section 34.3 and two of them caught only by running the new suite:

1. **The `if constexpr` else-branch had to call `Current()` and discard it.**
   The sketch throws directly, which discards the only use of `Current()` — so
   for a move-only `T` a before-start or after-end read reported
   `NotSupportedException` where the pre-#1793 bridge reported
   `InvalidOperationException`, silently converting an existing exception path.
   The implemented bridge runs the state machine first, as design section 18's
   ordering rule requires.
2. **`Generic::List<std::any>` cannot be instantiated at all** — `std::any` is
   not equality-comparable and `List<T>`'s `Contains`/`IndexOf` need
   `operator==` — so the nested-box case is tested through a hand-written
   `IEnumerator<std::any>` implementer instead.
3. **`std::any(Current())` for `T = std::any` selects `std::any`'s copy
   constructor**, not its value-forwarding one, so the result is a copy of the
   element's box and never a nested box. Now pinned by three explicit tests.
4. **`Stack(ICollection&)` and `Queue(ICollection&)` gained a throwing path**:
   they `std::any_cast<void*>` each element, so a source that does not enumerate
   `void*` elements now raises `std::bad_any_cast` where the old code silently
   stored a pointer into that source's live storage. No caller in this
   repository constructs either from an `ICollection`.

**The Divergence suite was flipped, not deleted.** All nine cases still exist,
renamed `EnumeratorCurrentSafety`, each asserting the opposite outcome on the
same collection through the same accessor with a `WAS …` comment naming what it
replaced. The `static_assert`s stay load-bearing in the other direction: they
now pin `std::any`, so a revert to `void*` cannot land silently either. The
eight `Contract` cases are untouched. Twenty-one further cases cover ownership,
lifetime across `MoveNext`/`Reset`/reallocation/`Clear`/both destructions, the
move-only type, `std::bad_any_cast`, nested boxing, `shared_ptr` and raw-pointer
handle semantics, the mutation counter, and every remaining non-generic
implementation family.

Closure evidence, all repository-local: `SharpRuntimeTests_Collections_Core`
**2,229/2,229** (was 2,208); a **clean full rebuild** in a dedicated
`build-abi-1793` tree at **13,515 tests across 37 executables** (was 13,494),
zero warnings, zero errors; ASan+UBSan clean on all six migrated lifetime
shapes, where four were `heap-use-after-free` before; **0 LSan leaks, with
LeakSanitizer proved active by a self-test that leaks 289 bytes and exits
non-zero**; TSan deliberately not run, because this change adds no atomic, no
`mutable` cache and no hidden `const` write and in fact *removes* two `mutable`
members; object layout `diff`-identical against the stored baseline; the
mangled name `_ZNK…18getCurrentPropertyEv` byte-identical and the vtable slot
unchanged at offset `0x20`, confirmed on the real repository objects; a
**stale-object probe in which an old caller and a new implementation linked with
zero diagnostics** and the mismatched call then took a SEGV with UBSan reporting
an invalid vptr; a positive consumer fixture compiling under
`-Wall -Wextra -Wpedantic -Werror` and exiting 0; a negative fixture rejected at
all **6** marked sites; module boundaries unchanged at 41 modules / 90 edges;
validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 at **1,939**/1,942 — one more than the
canonical 1,938, because `Doxyfile`'s `INPUT` does not cover `docs/` and every
`README.md` link into it resolves as an unresolved `\ref`; this ticket's
Breaking-changes entry adds exactly one such link, as its two neighbours already
do. Explained in design record section 34.8.

**Allocation, measured rather than assumed** (full round trip, replaced global
`operator new`): 0 for `int`, a raw pointer, and an already boxed `int`; **1**
for a small SSO `std::string` and a `std::shared_ptr`; 2 for a 64-char
`std::string` and a `DictionaryEntry`. The middle row corrects design section
22, which predicted 0 for "every type ≤ one pointer": libstdc++'s `std::any`
small-buffer optimisation admits only types that *fit in* a `void*`, and both
are larger than that regardless of their contents. A non-trivial element costs
exactly 1 copy and 1 destroy per read, live count balanced at 0. The typed
`Current()` path is unchanged and allocation-free; wall-clock ratios of the
boxed path against it, with an `asm volatile` barrier per iteration, are 2.4×
for `int` and 8.5× for a 64-char `std::string`. **No gate enforces those
ratios**, and they are not a regression threshold.

**A full consumer rebuild is mandatory and the linker will not say so.** The
symbol is byte-identical before and after and the calling convention is not, so
a partially rebuilt consumer links silently and corrupts memory. README.md
carries the behaviour-change entry, the migration table, and that warning.

Three residual limitations are stated rather than buried, all unchanged from
the design's risk register. **The typed `Current()` reference hazard is not
closed** — `&Current()` retained across a mutation is still a reproduced
use-after-free; closing it needs a by-value `Current()`, which makes move-only
`T` uninstantiable. Its validity window is now written into the header for the
first time, together with the statement that #1793 did not close it.
**`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` keep returning
`const void*` into live storage**; they are const-correct, so no write path
exists through them, but the type-safety, lifetime, and ownership-ambiguity
classes remain open there. A warning now sits on that interface pointing at the
design record, and it is opened as ticket **#1794**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M), **blocked** and
deliberately not begun — it is a second public source break plus a second silent
ABI break, needing its own two-part approval that #1793's does not supply.
**CNA's and mobile-eggbert's
usage remains unmeasured**; both were not inspected, searched, configured,
built, or modified, and ticket #1773 remains `blocked`.

Build directories used, all repository-local, gitignored, and at **at most four
compilation jobs**: the existing `build` tree (incremental, `--parallel 4`,
development only), the new dedicated `build-abi-1793` tree (**clean full
rebuild**, `--parallel 4`, the ABI gate), `build-probe-ienumerator` (the
re-run pre-fix probes, the new post-fix probe, and the stale-object ABI probe —
single-translation-unit compiles, one process each), `build-consumer-1793-neg`
(the negative fixture, `--parallel 4`, required to fail), and `build-tmp` as the
repository-local `TMPDIR`. `scripts/check_selective_components.sh` and
`scripts/check_doxygen_warnings.sh` use `mktemp` internally and were run with
`TMPDIR` redirected into `build-tmp`; the former already caps its own builds at
`--parallel 4`. No build tree was created under `/tmp`, `/var/tmp`, or
`/dev/shm`.

Ticket #1785 remains `todo` and untouched. Tickets #1788 and #1789 remain
`blocked` and untouched. Ticket #1791 remains `blocked` and untouched; no
indexer changed. Ticket #1773 remains `blocked` and untouched. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
push, merge, rebase, tag, or publication occurred. No compilation used more than
four jobs.

No repair ticket is active.

### Completed SortedSet nested-view exception ordering: ticket #1785

Ticket #1785 (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, P3, size XS,
category `design`) is **done**, closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-nested-order`. It carries **no `SR-AUD-*`
identifier** — the numbering stays frozen at 364 — and it does **not** reopen
SR-AUD-361, which stays `remediated`. The user explicitly approved acceptance
branch **(b)**: adopt .NET's ordering. That approval was scoped to #1785 alone
and does not carry to #1788, #1789, #1791, or #1794. The durable record is
[`docs/SortedSetLiveViewDesign.md`](docs/SortedSetLiveViewDesign.md) section 33;
sections 1–32 are unchanged apart from two supersession markers inside §15 that
point at §33.

**What changed: one `if` moved.** `SortedSet<T>::GetViewBetween` now validates
in .NET's order — lower widening, then upper widening, then the
lower-versus-upper relationship — instead of the order design #1782 selected and
#1783 shipped. Nothing else in the body changed.

**The .NET control flow, read from source rather than from #1783's report.**
`SortedSet.TreeSubSet.cs:342-353` tests `Comparer.Compare(_min, lowerValue) > 0`
and then `Comparer.Compare(_max, upperValue) < 0` against *its own* bounds, and
only then delegates to `_underlying.GetViewBetween`, which is
`SortedSet.cs:1508-1515` — the sole home of the
`SR.SortedSet_LowerValueGreaterThanUpperValue` check. So the widening tests are
in the caller and are unconditionally first; the lower bound precedes the upper;
and `_underlying` is the **root** set, which is why nesting flattens to depth 1
and the delegated call never re-enters the override.

**Observable difference, measured on both sides.**
`build-probe-sortedset/probe18_nested_exception_order.cpp` printed the full
matrix before and after the edit (`probe18_prefix.log`, `probe18_postfix.log`).
**Exactly 7 of 32 outcome rows changed**, all of them nested calls that are
*simultaneously* widening and inverted — for example `view[3,7]` asked for
`(2, 1)` is now `ArgumentOutOfRangeException("lowerValue")` where it was
`ArgumentException("Must be less than or equal to upperValue.", "lowerValue")`,
and `view[3,7]` asked for `(12, 9)` is now
`ArgumentOutOfRangeException("upperValue")`. Every success, every widening-only
failure, every inverted-only failure, and **every top-level (owning-set) call**
is byte-identical before and after: an owning set activates neither bound, so it
still reaches only the base check.

One combination is **arithmetically unreachable** and is proved so rather than
asserted: a view's bounds satisfy `!cmp(*upper_, *lower_)`, so widening both ends
gives `lower < *lower_ <= *upper_ < upper`, which cannot also be inverted.

**Compatibility.** No public signature, return type, `const` qualification,
mangled symbol, vtable (there are no virtual members), `sizeof`, `alignof`, or
member offset changed; ownership, live write-through, bounds inclusivity, nested
flattening, Count caching, iterator invalidation, the thread-safety contract,
and the O(1)-in-element-copies allocation behaviour are all untouched. A
rejected call still allocates nothing and bumps no version. This is an
observable semantic correction **only** for a nested request that is both
widening and inverted; consumers need ordinary recompilation of the changed
header and nothing more. Every in-repository `GetViewBetween` caller was
reviewed — six test files plus the two consumer fixtures, and no production
`src/` caller exists — and **none** asserted a doubly-invalid nested call, so
none relied on the old precedence.

**Tests.** `SortedSetNestedViewOrderTests.cpp` adds **23** permanent cases: the
complete matrix with exact exception type, parameter name, full message text,
and HResult; an exhaustive `(lower, upper)` grid over `[-2, 12]²` checked against
.NET's decision procedure transcribed independently as an oracle; the
unreachability proof; a descending custom comparer, an `operator<`-only element
type, and `std::string`; nesting to depth three; and the no-op guarantees —
nothing mutated, no version bumped, every view still fully usable after 1,500
consecutive failed constructions. `SortedSetLiveViewTests.cpp`'s 47 live-view
regressions are deliberately not duplicated.

Closure evidence, all repository-local: `SharpRuntimeTests_Collections_Core`
**2,252/2,252** (was 2,229); `scripts/local_ci_check.sh build` at **13,538 tests
across 37 executables** (was 13,515), zero warnings, zero errors;
ASan+UBSan+LSan over four SortedSet suites, **128 tests, 0 diagnostics, 0
leaks**, with LeakSanitizer proved active by a self-test reporting 4,112 leaked
bytes; TSan deliberately **not** run, because this change adds no shared mutable
state, no `const` write, and no new field — #1784's TSan campaign remains the
governing evidence; the SortedSet consumer fixture extended with a nested
precedence case and compiling under `-Wall -Wextra -Wpedantic -Werror`, exiting
0; module boundaries unchanged at 41 modules / 90 edges; validator tests 7/7;
catalogue current; database consistent; `git diff --check` clean; Doxygen 1.9.8
**unchanged at 1,939**/1,942; all ten selective components plus `Collections.Core
collections_sorted_set_view.cpp` in isolation.

Build directories used, all repository-local, gitignored, and at **at most four
compilation jobs**: the existing `build` tree (incremental, `--parallel 4`),
`build-probe-sortedset` (extended with `probe18` — a single-translation-unit
compile, one process), `build-asan-sortedset` (extended with `build_1785.sh` and
a re-run LSan self-test — single compiles), the new `build-consumer-1785`
(`--parallel 4`), and `build-tmp` as the repository-local `TMPDIR`.
`scripts/check_selective_components.sh` and `scripts/check_doxygen_warnings.sh`
use `mktemp` internally and were run with `TMPDIR` redirected into `build-tmp`;
the former already caps its own builds at `--parallel 4`. No build tree was
created under `/tmp`, `/var/tmp`, or `/dev/shm`. **`build-abi-1793` was removed**
after confirming it is repository-local, gitignored, held only build output, and
that its #1793 results are already recorded here and in `plan.md` — **1.46 GiB
reclaimed**; its two evidence logs (`build-abi-1793.log`,
`build-abi-1793-configure.log`) were kept.

Tickets #1788, #1789, #1791, and #1794 remain `blocked` and untouched. Ticket
#1773 remains `blocked` and untouched. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred. No compilation used more than four jobs.

No repair ticket is active.

### Nominal 500-hour first remediation programme

The 500-hour figure is credible for a first risk-reduction tranche, not for
closing all 362 findings. The following allocation includes implementation,
permanent regression tests, focused sanitizer/stress evidence, ticket
documentation, and normal focused/component validation:

| Work package | Bounded scope | Hours |
|---|---|---:|
| Planning truth and ticket setup | Synchronize baselines/phase/branch policy, repair twelve broken anchors, identify the two open questions, add severity/effort rules, and seed the bounded queue | 20 |
| LinkedListNode lifetime | Design plus compatible implementation for SR-AUD-357 / CCF-019 | 40 |
| Raw ICollection output boundary | ADR, consumer inventory, compile fixtures, and implementation for SR-AUD-358 / CCF-020. **Delivered in full**: design ticket #1770 plus implementation ticket #1771, which landed the user-approved source/ABI break | 20 |
| Eight immediate public-input crash tickets | SR-AUD-089 (**delivered**, ticket #1776), SR-AUD-097, SR-AUD-132, SR-AUD-236, SR-AUD-242, SR-AUD-257, SR-AUD-338, and SR-AUD-341 | 80 |
| Seven bounds/copy safety tickets | SR-AUD-041, SR-AUD-043, SR-AUD-044, SR-AUD-047, SR-AUD-049, SR-AUD-051, SR-AUD-054, SR-AUD-067, SR-AUD-071 through SR-AUD-073, SR-AUD-078, and SR-AUD-084, grouped only by demonstrated shared contract | 90 |
| Six defined-arithmetic tickets | SR-AUD-008, SR-AUD-019, SR-AUD-020, SR-AUD-025 through SR-AUD-027, SR-AUD-057, SR-AUD-060, SR-AUD-131, and SR-AUD-135 | 70 |
| Eight ownership/use-after-free tickets | SR-AUD-187, SR-AUD-221, SR-AUD-230, SR-AUD-237, SR-AUD-245, SR-AUD-247, SR-AUD-263, and SR-AUD-310 | 90 |
| Five concurrency/entropy tickets | SR-AUD-010/SR-AUD-050, SR-AUD-203/SR-AUD-204, SR-AUD-207, SR-AUD-210 through SR-AUD-212, and SR-AUD-216/SR-AUD-218 | 60 |
| Integrated closure and risk reserve | Final sanitizer/stress passes, network-permitted full gate, selective consumers, documentation/index reconciliation, and estimate variance | 30 |
| **Total** | | **500** |

Treat the hours as a capacity boundary with roughly +/-30% uncertainty, not a
promise to batch unrelated findings until the allocation is consumed. Recheck
the remaining estimate after every five completed tickets. The nominal tranche
can reasonably close roughly 40-50 high findings; SR-AUD-358 is closed by
tickets #1770 and #1771 and is one of the four `remediated` findings.

The preliminary whole-backlog estimate is 1,600-2,400 engineering hours:
after the first 500-hour tranche, roughly forty high findings and essentially
the full 261-medium/11-low inventory remain. A deliberately chosen
consumer-risk stopping point may be smaller, but 500 hours must not be
presented as complete remediation of all 362 open findings.

## Current state

- The CNA-style deep audit is complete. `audit/AUDIT_SCOPE.md` fixes its
  1,748-file first-party scope and the mirrored
  `audit/<source-path>.audit.md` convention; `AUDIT_FINAL_REPORT.md`,
  `AUDIT_MANIFEST.md`, and `AUDIT_PROGRESS.md` record the reconciled evidence
  and remediation backlog.
- Post-audit remediation ticket #1767 completed on
  `feature/remediation-coll-enum`. It remediates SR-AUD-356 and SR-AUD-364 /
  CCF-018 with one guarded lifecycle state across ten collection enumerators
  and BitArray mutation invalidation. Design ticket #1768 then recorded the
  SR-AUD-357 / CCF-019 LinkedListNode lifetime contract in
  `docs/LinkedListNodeLifetime.md`, and implementation ticket #1769 completed it
  on `feature/remediation-coll-linked-node`. Design-only ticket #1770 then
  recorded the SR-AUD-358 / CCF-020 raw-`CopyTo` contract in
  `docs/ICollectionCopyToDesign.md` on `feature/remediation-coll-copyto`,
  changing no production or test source. Implementation ticket #1771 then landed
  the user-approved source/ABI break on the same branch: the pure virtual
  `CopyTo(void*, intcs)` is removed from `ICollection`. SR-AUD-358 is
  `remediated` and no repair ticket is active.
- Initial audit validation passed boundary validation, catalogue freshness, and
  a zero-warning native build. It could not complete the full suite in this
  sandbox because the six local-server `Net.Http` cases fail immediately with
  `Socket::Socket: socket() failed`; this matches the documented requirement
  for local-network permission. The tests remained enabled. Ticket #1767's
  network-permitted closure rerun subsequently passed all 12,694 tests,
  including the six HTTP cases.
- Final audit reconciliation completed all configured build targets with
  `gmake -C build -j4`; plan-database consistency, module-boundary validation
  (41 physical modules, 90 production edges), and `git diff --check` passed.
- All 1,748 audit reports are complete and confirmed three hundred sixty-four
  findings at audit closure. The index now records 360 open `confirmed`
  findings and four `remediated` findings. The final
  142-file `Collections` shard passed 1,422/1,422 focused tests and adds SR-AUD-356 through
  SR-AUD-364: invalid enumerator Current paths can reach ASan-confirmed out-of-bounds reads;
  retained LinkedListNode handles use freed storage; raw ICollection CopyTo can ASan-crash;
  ReadOnlyDictionary.Empty is assignable; ConcurrentDictionary loses updates; SortedSet views
  are snapshots; FrozenDictionary overwrites duplicate keys; Hashtable accepts null keys and
  returns null views; and BitArray enumeration has no valid-state/mutation guard. The preceding
  119-file `Xml` shard passed 377/377 focused tests and adds SR-AUD-348 through SR-AUD-355:
  XmlReader reads after Close; XmlWriter emits malformed XML for invalid names; invalid InnerXml
  silently removes children; cross-parent DOM removal detaches unrelated children; XmlDocument
  node events never fire; HasNamespace misses outer scopes; XmlConvert rejects XSD durations; and
  XPath leaves adjacent text/CDATA as separate nodes. The preceding 84-file `IO` shard passed
  527/527 focused tests and adds SR-AUD-337 through SR-AUD-347:
  leaveOpen text wrappers remain usable after Close; null text streams silently EOF or ASan-crash;
  FileSystemWatcher misroutes events after live Path changes and ignores NotifyFilter; RandomAccess
  accepts invalid metadata and suppresses invalid-descriptor failures; MemoryStream null construction
  ASan-crashes; FileStream bypasses requested access and retains closed metadata; in-memory text and
  UnmanagedMemoryStream disposal is incomplete; FileInfo deletes an empty directory; and FileInfo/
  DirectoryInfo empty paths leak raw filesystem errors. The preceding 38-file `Xml.Linq` shard passed
  92/92 and adds SR-AUD-333 through SR-AUD-336: retained children dereference an ASan-confirmed
  dangling raw parent; namespaces are lost in parse/save; special XML lexical delimiters corrupt output;
  and registered XObject events never notify. The preceding 50-file `Security.Cryptography` shard passed 80/80 and adds
  SR-AUD-331 through SR-AUD-332. Earlier work
  confirms that tracked CI omits the
  direct `Collections.Blocking` selective fixture; the boundary validator has
  narrow negative-fixture coverage; `BlockingCollection<T>` has a
  fractional-negative timeout parity gap; the source inventory does not
  implement its advertised plan cross-reference; the .NET indexer defaults to
  destructively writing a different checkout; DateTime/DateTimeOffset/TimeOnly
  have confirmed constructor validation and parser false-success defects;
  TimeSpan parses overflowed day counts as wrapped durations; `Random::Shared`
  and `Guid::NewGuid` are not safe for concurrent use despite their public
  contracts; `Guid::NewGuid`/`CreateVersion7` also use a predictable standard
  PRNG rather than .NET's OS CSPRNG; Version
  serializes undefined fields as `-1` in `ToString(fieldCount)`; ThreadPool's
  raw work-item overload exposes callers to a detached use-after-free,
  registered waits defer a null WaitHandle crash to a worker, and ThreadPool
  configuration setters claim success without changing any setting; Timer
  accepts an empty callback without an error and TimeProvider reports a
  successful post-disposal timer Change; Thread permits an empty start callback
  to terminate asynchronously, gives external threads the same managed ID, and
  discards the Start(void*) parameter; its Batch8 running-state assertion is
  tautological; ThreadStartException exposes runtime-internal constructors as
  ordinary public C++ API; the principal Threading CurrentThread ID test
  discards its result; CancellationToken accepts an empty callback and permits
  a public null state to crash; PeriodicTimer truncates a fractional period and
  lets two consumers take one tick; recursive Monitor.Wait can deadlock its
  signaler; and ReaderWriterLockSlim has a TSan-confirmed Dispose/entry race,
  permits disposal while held, admits readers ahead of queued writers, and
  reflects invalid recursion-policy values unlike .NET; Mutex Close remains
  usable; Semaphore/SemaphoreSlim Release can overflow before their full-count
  check; SemaphoreSlim/ManualResetEventSlim have TSan-confirmed disposal races;
  Auto/ManualResetEvent Close remains usable and neither event can take part in
  local WaitHandle multi-waits; Barrier deadlocks a legal post-phase phase
  read and races ParticipantCount; CountdownEvent Reset(0) leaves a waiter
  blocked and its disposal races Wait; SpinWait accepts invalid timeout and
  empty callback inputs rather than managed argument errors; and the
  cryptographic `GetInt32` full signed-domain path reaches
  implementation-defined conversion and signed-overflow-prone arithmetic; and
  the nominal `SynchronizationContext::Send` test has no observable assertion;
  and file-backed compression tests overwrite or recursively remove fixed
  `/tmp` paths, making them non-isolated; and `String::Format` mishandles
  escaped/malformed braces while bounded substring `LastIndexOf` can return a
  match extending outside the requested range; `Char::Parse` accepts overlong
  invalid UTF-8; Object and HashCode tests incorrectly require distinct values
  to have distinct/nonzero hashes; `Int128` invokes undefined signed negation for
  `MinValue` parsing/formatting; `UInt128` invokes undefined shifts for counts
  of 128 or more; audited 8/16/32/64/128-bit formatters accept unknown formats
  (and 128-bit variants leak `std::stoi`); Byte/SByte/Int16/UInt16/Int32/UInt32/
  Int64/UInt64/UInt128 do not validate inverted Clamp bounds; SByte/Int16/
  UInt16/UInt32/UInt64/UInt128 omit the integral binary `B`/`b` format; and
  SByte/Int16 return false from `IsPositive(0)` despite .NET's generic-math
  zero rule, while their tests assert that incorrect result; and IntPtr
  Add/Subtract execute signed-overflow UB at native-width extrema rather than
  defined unchecked wrap; `Convert` silently wraps several signed/narrowing
  integral overloads, converts NaN to spurious integers, and accepts malformed
  Base64 padding while rejecting permitted whitespace; and `Single`/`Double`
  accept invalid `Round` precision, reject subnormal powers of two, map
  `ILogB(NaN)` to the zero sentinel, lose exact Pi-turn results, and expose a
  C++ subset for default parsing/formatting. `Single::IsPositive` also rejects
  a positive-sign NaN. Decimal rejects valid default whitespace/grouping,
  reports numeric range overflow as `FormatException`, truncates excess parser
  precision and OA Currency values, accepts invalid rounding enums, and erases
  its observable negative-zero sign. Math/MathF also retain invalid rounding
  enums; Math leaks the native `ILogB(NaN)` sentinel and misses double base-log
  special cases, while MathF accepts inverted Clamp bounds and changes ties-to-
  even results when the C++ rounding environment is altered. BitConverter's
  typed vector decoders have ASan-confirmed before/after-buffer reads for
  negative or short input. `Numerics::BitOperations` passes independent
  32/64-bit bit-operation checks, but its omission of current .NET `Crc32C`
  and an exact signed-64 `TrailingZeroCount` overload is an API-baseline
  decision, not yet a confirmed defect. `DivisionRounding` matches all five
  .NET values but has no consumer by documented design. `TotalOrderIeee754Comparer`
  correctly orders raw Half/float/double bit patterns but lacks .NET's
  `IEqualityComparer<T>` contract, preventing use where total-order equality
  is required. `HashCode::AddBytes` converts a negative public ReadOnlySpan
  length to an enormous unsigned raw read; ASan confirms an overflow, with
  Span's now-confirmed negative-length constructor as the enabling cause.
  Span/ReadOnlySpan also corrupt overlapping nontrivial ranges because all
  CopyTo/TryCopyTo paths use forward `std::copy`; static MemoryExtensions
  CopyTo repeats that overlap defect and ASan confirms that it writes past a
  shorter destination. `SpanSplitEnumerator` also treats an empty exact sequence
  as a repeating zero-length separator, creating an infinite range iteration.
  MemoryExtensions default sort/binary search/sequence comparison use C++
  operators rather than .NET comparison semantics, mishandling NaN; its
  whitespace trim treats UTF-8 bytes with locale `std::isspace` and retains
  U+00A0. `Memory`/`ReadOnlyMemory` extend the malformed-length and
  overlap-copy defects; `ReadOnlyMemory::Slice(INT_MIN)` additionally reaches
  signed-overflow UB before its intended range exception. `Guid` span parsing
  also extends the malformed-length raw-read risk. `Array` vector copy extends
  overlap corruption; its raw-pointer copy accepts negative metadata and
  byte-copies nontrivial values, while its float ordering, empty-callback
  diagnostics, and `MaxLength` constant diverge from .NET. `ArraySegment`
  extends the overlap and malformed-length patterns; default-segment operations
  can silently succeed or dereference null, and vector `CopyTo` resizes a short
  destination. The four Core span-formatting/parsing adapters and three
  focused test files add no confirmed implementation defect: their 33/33
  focused test filter passed, while their reports identify missing
  pre-populated failure-result, non-null-provider, UTF-8/error-taxonomy, and
  short-buffer assertions. The direct observable fixture is a new medium
  test-contract finding: it returns no unsubscription handle and permits
  notifications after completion, while no production observable exists. The
  adjacent async-disposal/APM, cloning, comparison, and custom-formatting
  interfaces add no new confirmed defect; their 12/12 focused filter exposes
  only assertion gaps. Equality and disposal interfaces add no confirmed
  defect under their 22/22 filter, but their reports distinguish explicit
  disposal from misleading shared-pointer-reset and counter-only assertions.
  The IConvertible/DBNull checkpoint adds no defect after Core.Base 9/9 and
  integration 11/11 filters; its reports retain the culture-invariant adapter
  and singleton reference boundary as explicit assumptions. `Index`/`Range`
  add new high SR-AUD-057: their deliberately unvalidated .NET offset path uses
  signed C++ arithmetic, so an end-based `INT_MAX` index with `INT_MIN` length
  hits UBSan-confirmed overflow. `Nullable<T>` extends SR-AUD-046: its raw
  comparator/equality path makes NaN compare equal to finite values or unequal
  to itself, rather than using .NET default comparer/equality semantics.
  WeakReference's shared-pointer adaptation adds no new defect under its 23/23
  filter; TrackResurrection remains explicitly nonfunctional by design.
  `ValueType` is now confirmed as a medium parity defect: the C++ base is
  publicly constructible and defaults to identity/address semantics, while
  current .NET makes it abstract and provides fieldwise default value semantics;
  the direct tests lock in that fallback.
  `SequencePosition` is also a medium parity defect: its publicly mutable
  `void*`/integer components can be rewritten after construction even though
  .NET reserves its private readonly parts for the position creator; all six
  focused tests pass without protecting that boundary.
  `ArrayBufferWriter<T>` adds SR-AUD-070: its `std::vector` growth and clear
  paths silently require a default-constructible element type, so a valid
  unconstrained .NET-style generic payload fails to compile at `GetSpan`.
  `MemoryPool<T>` adds high SR-AUD-071: disposed owners return an empty view
  rather than throwing, while a retained pre-dispose Memory carries a stale
  length over freed vector storage and ASan confirms native null dereference.
  `IBufferWriter<T>` and `IMemoryOwner<T>` add no standalone defect, but their
  reports retain missing nonempty-view, old-view invalidation, post-dispose,
  and polymorphic conformance assertions.
  `ReadOnlySequence<T>` adds high SR-AUD-072/073: its raw pointer constructor
  dereferences a null nonzero source, while `TryGet` accepts before-start or
  negative forged positions and exposes pre-slice data or ASan-confirmed
  out-of-bounds memory. SR-AUD-074 records default sequence enumeration of one
  empty segment rather than none.
  `SequenceReader<T>` adds SR-AUD-075: false `TryRead` and `TryPeek` leave
  caller output unchanged instead of assigning default, allowing stale values
  to be reused despite the returned false result.
  `BinaryPrimitives` adds no confirmed implementation defect; its report
  records missing `Try*`, floating payload, 128-bit, big-endian CI, and MSVC
  API-baseline evidence.
  `ArrayPool<T>` adds SR-AUD-076: its configured `Create` factory silently
  ignores both limits and accepts zero values, while .NET requires positive
  values and realizes them through pool buckets.
  `MemoryManager<T>` and `IPinnable` add no new classified defect: manager-
  backed `Memory<T>` is an explicit unsupported storage adaptation, while the
  reports retain missing pin/lifecycle conformance assertions.
  `SearchValues<T>` adds SR-AUD-077: its documented equality-only generic
  contract actually requires `std::hash<T>` through `unordered_set`, so an
  equality-only value type fails to compile.
  `SequenceReaderExtensions` adds no confirmed defect in its signed contiguous
  byte subset; its report preserves unsigned, multi-segment, false-output,
  big-endian, union-punning, and include-hygiene evidence gaps.
  `Base64` adds high SR-AUD-078: its in-place encoder overwrites an unread
  trailing source remainder after encoding a full triple, silently corrupting
  four-/five-byte input. SR-AUD-079 through SR-AUD-081 record acceptance of
  noncanonical padding bits, padded input in a non-final streaming call, and
  incorrect consumption of whitespace after padding.
  `Base64Url` independently extends the same in-place corruption and
  noncanonical-final-bit findings, and SR-AUD-082 records its unsupported
  rejection of current .NET's optional `=`/`%` final padding.
  `StandardFormat` adds SR-AUD-083: its `ToString` serializes default or
  zero-symbol values as embedded-NUL C++ strings rather than .NET's empty
  string; the mixed test file checks default state but not the rendering.
  `Utf8Formatter` adds no new confirmed defect in its documented bool/integer
  subset after its 25/25 direct filter, but reports retain signed-minimum,
  exact-short-buffer, format-alias, all-overload, and differential-vector gaps.
  `Utf8Parser` adds high SR-AUD-084: its default and grouped `Int64.MinValue`
  paths negate a signed minimum under UBSan; SR-AUD-085 retains stale output on
  false, and SR-AUD-086 rejects valid leading-plus integer input.
  `ReadOnlySequenceSegment` adds SR-AUD-087: linked segment nodes cannot form a
  C++ `ReadOnlySequence`, contrary to the header's multi-segment claim; the
  companion extensions pass only their contiguous 11-test subset.
  `MemoryHandle` adds SR-AUD-088: its comments promise scope-based RAII cleanup
  but its implicit destructor never calls `Dispose`, so a scoped pinned handle
  does not unpin.
  The full Buffers module is now audited (40/40): its remaining direct fixture
  filter passes 54/54, but `EnumeratorTests.cpp` calls default-sequence
  `MoveNext()` without asserting its result, leaving SR-AUD-074 unobserved;
  ArrayBufferWriter and BinaryPrimitives reports preserve their generic,
  byte-exact, and cross-platform assertion/diagnostic gaps.
  The argument-exception family passes 64/64 direct tests but adds high
  SR-AUD-089: `ArgumentNullException(const char*)` null-dereferences a null
  parameter name in string assembly. Its non-null parameter path also doubles
  the message suffix (SR-AUD-090), and `ArgumentOutOfRangeException` generic
  comparison/equality guards silently impose `std::to_string` despite their
  declared comparison-only contract (SR-AUD-091). `ArgumentException` extends
  SR-AUD-048 by accepting UTF-8 U+00A0 as non-whitespace; CCF-015 now records
  that shared byte-`std::isspace` cause.
  Base `Exception`/`SystemException` source and declarations are now audited:
  their selected 62/62 test filter passes, but C++ default `Exception` returns
  an empty message where current .NET produces a nonempty fallback diagnostic
  (new medium SR-AUD-092), and two direct tests lock that result in.
  The complete `ExceptionTests.cpp`/`ExceptionNewTests.cpp` audit passes its
  twelve-suite 124/124 filter but documents weak default-message, null C-string,
  exact-suffix, Unicode-whitespace, and generic-template assertions rather
  than treating the shared green fixture as evidence of those boundaries.
  `ArithmeticException`, `DivideByZeroException`, and `OverflowException` have
  no new classified implementation fault under their 7/7 focused and 124/124
  shared evidence, but their reports identify missing specific-HResult,
  inner-exception, null-C-string, and checked-arithmetic integration coverage.
  `InvalidOperationException`, `NotImplementedException`,
  `NotSupportedException`, `NullReferenceException`, and
  `ObjectDisposedException` add no new classified defect under their shared
  124/124 evidence; their audit reports preserve exact message/HResult,
  null-C-string, inner-exception, and state-transition assertion gaps.
  `ArrayTypeMismatchException`, `FieldAccessException`,
  `IndexOutOfRangeException`, `OutOfMemoryException`, and
  `InsufficientMemoryException` are now audited; ArrayTypeMismatch adds medium
  SR-AUD-093 because every inline constructor inherits `COR_E_SYSTEM` rather
  than assigning .NET's `COR_E_ARRAYTYPEMISMATCH` value.
  `MemberAccessException`, `MethodAccessException`, `MissingMemberException`,
  `MissingFieldException`, and `MissingMethodException` now have five mirrored
  audits. Their complete plural/singular 61/61 filter confirms ordinary constructor, inheritance,
  exact ASCII diagnostic, and derived-HResult paths; no standalone defect was
  found. The reports retain untested empty/UTF-8-name, inner-pointer identity,
  and native reflection-boundary diagnostics.
  `ApplicationException`, `AppDomainUnloadedException`,
  `BadImageFormatException`, `CannotUnloadAppDomainException`, and
  `DataMisalignedException` add five mirrored audits and medium SR-AUD-094:
  every inline constructor omits its derived HResult assignment. A direct probe
  finds one inherited `COR_E_EXCEPTION` and four inherited `COR_E_SYSTEM`
  values instead of their five documented codes despite the green 43/43 family
  filter; CCF-016 links the recurring exception-HResult audit gap to SR-AUD-093.
  `TypeLoadException`, `TypeAccessException`, `TypeUnloadedException`,
  `DllNotFoundException`, and `EntryPointNotFoundException` add five mirrored
  audits. Their full 48/48 family filter confirms the three base/sibling HResult
  implementations, but the Dll and entry-point derivatives retain
  `COR_E_TYPELOAD` rather than their documented distinct codes (medium
  SR-AUD-095); the shared probe records both and CCF-016 extends accordingly.
  `AccessViolationException`, `ContextMarshalException`,
  `InsufficientExecutionStackException`, `InvalidCastException` (header and
  source), and `InvalidProgramException` add six mirrored audits. A focused
  32/32 filter confirms correct HResults for the latter three, but the first
  two retain `COR_E_SYSTEM` rather than `E_POINTER` and
  `COR_E_CONTEXTMARSHAL` (medium SR-AUD-096); CCF-016 now captures this pair.
  `MulticastNotSupportedException`, `NotFiniteNumberException`,
  `PlatformNotSupportedException`, `RankException`, and
  `StackOverflowException` (header and source) add six mirrored audits. Their
  focused 29/29 filter and local .NET source confirm all reviewed HResults;
  no new defect was classified, while reports preserve missing special-float,
  all-overload HResult, inner-exception, and real runtime-integration evidence.
  `AggregateException` now has a mirrored audit. Its 13/13 focused tests are
  green, but null inner `exception_ptr` causes probe-confirmed segfault
  (high SR-AUD-097); custom constructors, `Handle`, and `Flatten` lose .NET
  causal message/first-inner/order behavior (SR-AUD-098); and empty `Handle`
  predicates defer to native `bad_function_call` (SR-AUD-099, extending
  CCF-011).
  `DuplicateWaitObjectException`, `ExecutionEngineException`,
  `FormatException` (header/source), `TimeoutException` (header/source),
  `UnauthorizedAccessException` (header/source), and
  `TypeInitializationException` add nine mirrored audits. The 38/38 filter
  confirms the latter eight normal HResult paths; DuplicateWaitObject retains
  generic `COR_E_ARGUMENT` and a divergent wait-array default diagnostic
  (medium SR-AUD-100), extending CCF-016.
  `System::IO::IOException` (header/source), `DirectoryNotFoundException`
  (header/source), and `Security::Cryptography::CryptographicException` add
  five mirrored audits. Their direct test filter selects 0 tests. Existing
  HResults are probe-correct, but the ports omit IOException custom-HResult,
  DirectoryNotFound path-plus-inner, and CryptographicException composite-format
  public overloads (medium SR-AUD-101).
  `AppContext`, `AppDomain` (declaration and implementation), `AppDomainSetup`,
  and its dedicated fixture add five reports. Their combined 11/11 filter is
  green, but the isolated probe shows named AppContext data cannot configure
  BaseDirectory or compatibility switches (medium SR-AUD-102); AppDomain
  discards public data/switch state instead of delegating to AppContext
  (SR-AUD-103); and `ApplyPolicy` accepts representable empty/NUL identity
  strings that .NET rejects (SR-AUD-104). No production or test source changed.
  The existing `Environment` declaration, implementation, and complete 99-test
  fixture reports are strengthened. Its direct filter is green, but a reproducible probe
  confirms Unix special-folder XDG/option/error divergence (SR-AUD-105), empty
  environment values being deleted rather than represented (SR-AUD-106), a
  real 4,866-byte cwd becoming empty through a fixed buffer (SR-AUD-107), and
  raw command-line joins losing quote/space argument boundaries (SR-AUD-108).
  No production or test source changed.
  `GC`, its three directly represented support types, two compatibility
  forwarding headers, and dedicated fixture add seven reports. The complete 61/61 direct fixture is
  green; all reviewed zero/no-op behavior is an explicit RAII/no-tracing-GC
  adaptation, and GC notification waits correctly return `NotApplicable`.
  No new classified defect or source/test change resulted.
  `Activator`, `RuntimeTypeHandle`, `RuntimeType`, and their two dedicated
  fixtures add five reports. The 16/16 runtime-type filter is green, but a
  direct construction probe confirms Activator's braced value construction
  changes initializer-list-capable arguments (medium SR-AUD-109), and the
  public `RuntimeType` enum collides semantically with .NET's unrelated
  internal reflection class (SR-AUD-110). No production or test source changed.
  `ModuleHandle`, `RuntimeArgumentHandle`, `RuntimeFieldHandle`, and
  `RuntimeMethodHandle` add four reports. Their focused existing tests pass
  19/19, but a standalone public-header compile fails because ModuleHandle
  defines `ResolveTypeHandle` before `RuntimeTypeHandle` is complete (medium
  SR-AUD-111); the test suite masks it by include order. The other reviewed
  no-metadata/no-varargs adapters are explicit. No production or test source changed.
  `ArgIterator`, `TypedReference`, and the complete Batch12 arg-handle fixture
  add three reports. Its direct 11/11 filter is green, but five ArgIterator
  tests call methods through raw reinterpreted character storage without an
  ArgIterator lifetime (medium SR-AUD-112). TypedReference's intrinsic/
  reflection omission remains explicit. No production or test source changed.
  `AssemblyLoadEventArgs`, ThreadStatic/STA/MTA marker attributes, and their
  three dedicated fixtures add seven reports. The marker filter passes 18/18,
  but ThreadStaticAttribute has no C++ field-attachment or `thread_local`
  mechanism despite its per-thread-value contract (medium SR-AUD-113).
  Assembly-load payload and STA/MTA no-effect adaptations are explicit. No
  production or test source changed.
  The Attribute base, targets/usage value objects, ten related marker/value
  headers, and eight full fixtures add twenty-one reports; their focused filter
  passes 77/77. `Attribute` remains publicly constructible and performs
  address-based equality/hash rather than current .NET's abstract fieldwise
  contract (SR-AUD-114). `ObsoleteAttribute` cannot attach to a declaration or
  issue its promised diagnostic (SR-AUD-115), and it collapses nullable string
  properties into empty strings (SR-AUD-116). Deprecated LoaderOptimization
  values have Doxygen-only, not C++ compiler, deprecation (low SR-AUD-117).
  Context/serialization/params/reflection marker limits are explicit permanent
  adaptations. No production or test source changed.
  Delegate, MulticastDelegate, MulticastAction, implementation, and four full
  fixtures add eight reports. Delegate-specific filters pass 70/70 and the
  mixed Batch14 filter 25/25, but a compiled probe confirms that composition
  loses concrete delegate type and accepts mismatches (SR-AUD-118), multicast
  equality uses entry pointer identity rather than delegate equality
  (SR-AUD-119), and Remove cannot remove a multi-entry final subsequence
  (SR-AUD-120). MulticastAction's token-based event-field adaptation has no
  independent reproduced fault. No production or test source changed.
  EventArgs/EventHandler declaration and source plus their direct fixtures add
  six reports; the focused filter passes 32/32. EventHandler nevertheless
  stores an empty callback and later raises native `std::bad_function_call`
  (SR-AUD-121, CCF-011), while its `const TEventArgs&` callback type rejects a
  handler that must mutate event data (SR-AUD-122). No production or test
  source changed.
  Resolve/unhandled-exception event arguments and aliases plus three full
  fixtures add seven reports; their selected filter passes 33/33.
  ResolveEventHandler nevertheless requires a string and cannot represent the
  nullable .NET “not resolved” outcome independently of an empty name
  (SR-AUD-123). The string/reflection and exception-pointer sender adaptations
  are explicit; AppDomain event dispatch remains the SR-AUD-103 stub. No
  production or test source changed.
  ApplicationId/ApplicationIdentity and their fixtures add four reports; the
  focused filter passes 22/22. ApplicationId loses the byte/null-aware identity
  model and required nonempty-name validation (SR-AUD-124), while its ToString
  omits public-key token and uses a non-.NET grammar (SR-AUD-125).
  ApplicationIdentity is a documented legacy/reflection adaptation. No
  production or test source changed.
  Converter/Predicate/Func and their fixtures add six reports; the focused
  filter passes 17/17. `Func<void>` and `Converter<T, void>` nevertheless
  compile and are type-identical to `Action` forms, although current .NET
  keeps Action distinct because `void` cannot be a generic type argument
  (SR-AUD-126, reproduced by C++ and C# probes). No production or test source
  changed.
  DateTimeKind and DayOfWeek match their .NET values; their focused sections
  remain within not-yet-complete `SystemTypesRemainingTests.cpp`. CrashReason
  and its full direct fixture add four reports under a green 17/17 combined
  filter, but the public top-level `System::CrashReason` incorrectly exposes
  an internal nested NativeAOT enum that no production source consumes
  (SR-AUD-127). No production or test source changed.
  ContextBoundObject, MarshalByRefObject, LocalDataStoreSlot, and two direct
  fixtures add five reports; their selected filter passes 14/14. C++ permits a
  direct MarshalByRefObject despite .NET's abstract base and omits its legacy
  throwing members (SR-AUD-128), while a child write replaces a parent's
  LocalDataStoreSlot value and no C++ Thread slot API exists (SR-AUD-129).
  Existing Batch3 coverage locks the invalid base construction; no production
  or test source changed.
  Diagnostics::Stopwatch and its full direct fixture add two reports under a
  green 20/20 filter. It publishes 100-ns/10 MHz timestamps even though .NET
  Unix exposes raw 1 GHz monotonic units (SR-AUD-130), and
  GetElapsedTime(INT64_MIN, INT64_MAX) reaches UBSan-confirmed signed overflow
  (SR-AUD-131). No production or test source changed.
  TryWriteInterpolatedStringHandler and its full direct fixture add two reports
  under a green 13/13 filter. A positive-length null destination reaches an
  ASan-confirmed write crash (SR-AUD-132); normal formatting ignores format and
  emits C++ spellings such as `1`, `255`, and `3.140000` instead of .NET text
  (SR-AUD-133). No production or test source changed.
  Header-only Linq and its full direct fixture add two reports under a green
  45/45 filter. Empty callbacks silently succeed on empty vectors but later
  throw `std::bad_function_call` (SR-AUD-134); `Sum(INT_MAX, 1)` has
  UBSan-confirmed signed overflow (SR-AUD-135); and raw float logic extends
  SR-AUD-046 by rejecting/duplicating NaN and missing a late NaN minimum. No
  production or test source changed.
  Void and UnitySerializationHolder plus both full direct fixtures add four
  reports under a green 12/12 singular-suite filter. C# rejects the ordinary
  Void construction/text/generic use that C++ documents and tests
  (SR-AUD-136); UnitySerializationHolder exposes an invented raw-code/data
  object instead of .NET's internal serialization-only boundary (SR-AUD-137).
  No production or test source changed.
  Six complete exception fixture sources (Arithmetic, Overflow, Format,
  NotImplemented, NotSupported, and PlatformNotSupported) add six reports;
  their exact 36/36 filter passes. No new production finding is classified,
  but reports record HResult, inner-cause, null/UTF-8, and real consumer-route
  assertion gaps; PlatformNotSupported's older header report is corrected to
  reflect its existing three-constructor HResult coverage. No source or test
  was changed.
  Six more exception fixtures (CannotUnloadAppDomain, DataMisaligned,
  ContextMarshal, ExecutionEngine, MemberAccess, MulticastNotSupported) add
  six reports under a selected 31/31 filter; no new production finding. The
  first three document their existing SR-AUD-094/096 HResult coverage gaps,
  while the last three assert their expected HResults. No source or test was
  changed.
  Six runtime exception fixtures (ArrayTypeMismatch, Rank, OutOfMemory,
  NullReference, SystemException, TypeUnloaded) add six reports under a green
  40/40 filter; no new production finding. ArrayTypeMismatch's direct text
  coverage leaves SR-AUD-093's HResult gap untested, while the other five
  fixtures verify their key HResult paths. No source or test was changed.
  One complete BadImageFormat fixture and three complete shared exception
  fixtures add four reports under a green 33/33 selected filter. Their
  AppDomainUnloaded/BadImageFormat/DllNotFound/DuplicateWaitObject/
  EntryPointNotFound cases omit the known HResult diagnostics, leaving
  SR-AUD-094, SR-AUD-095, and SR-AUD-100 unguarded; no new production finding
  or source/test change resulted.
  Six complete member/type-access exception fixtures add six reports under a
  green 58/58 filter. MissingField/MissingMember/MissingMethod, MethodAccess,
  and InvalidOperation directly protect their derived HResults; TypeAccess
  tests its default HResult and header review confirms the other constructors.
  No production finding or source/test change resulted.
  TypeLoad and TypeInitialization fixture sources add two reports under a
  green 25/25 filter. TypeLoad directly checks its default HResult while the
  audited header covers remaining overloads; TypeInitialization checks its
  sole constructor's HResult and retained cause. No production finding or
  source/test change resulted.
  UriBuilder and its full direct fixture add two reports under a green 27/27
  filter, but C++/current-.NET probes confirm four medium defects:
  constructor copying fuses credentials, relative string input renders invalid
  `:///` text, raw equality/hash disagree with Uri identity, and Scheme/IPv6
  Host setters skip required normalization or validation (SR-AUD-138–141).
  No production or test source changed.
  Uri's header, implementation, and full direct fixture add three reports
  under a green 57/57 filter, but C++/current-.NET probes confirm four medium
  defects: raw case/default-port identity, opaque `mailto` port loss,
  query/fragment/network-path base-resolution errors, and acceptance of
  malformed IPv6 or invalid UriKind input (SR-AUD-142–145). No production or
  test source changed.
  UriParser and its complete direct fixture add two reports under a green 14/14
  filter. Current-.NET and C++ probes confirm that custom-parser registration/
  participation is absent and protected hooks are public stubs, while
  IsKnownScheme accepts malformed input instead of throwing (SR-AUD-146–147).
  No production or test source changed.
  UriTypeConverter, UriFormatException, and both complete direct fixtures add
  four reports under a green 13/13 filter. New medium SR-AUD-148 records that
  converter empty text throws where current .NET returns null; the non-nullable
  C++ result and direct fixture lock the incompatible behavior. No production
  or test source changed.
  Seven URI value-type headers and their seven complete direct fixtures add
  fourteen reports under a green 38/38 filter. New medium findings record that
  UriCreationOptions has no Uri consumer, UriPartial has no GetLeftPart, and
  UriHostNameType has no CheckHostName classifier (SR-AUD-149–151); other
  audited enum values match .NET. No production or test source changed.
  The Uri module README adds the final 27th eligible URI report; its Core.Base
  dependency claim is accurate, but it does not expose the documented URI
  adaptation limits. No new finding or source/test change resulted.
  Architecture, OSPlatform, RuntimeInformation declaration/implementation, and
  their shared fixture add five reports under a green 11/11 filter. New medium
  findings record missing default OSPlatform, omitted FrameworkDescription/
  RuntimeIdentifier, and Windows OSArchitecture aliasing process architecture
  (SR-AUD-152–154). No production or test source changed.
  NativeMemory and its complete direct fixture add two reports under a green
  20/20 filter. The reviewed POSIX AlignedRealloc old-size bounded-copy repair
  is correct; no new defect, but reports record AllocZeroed overflow,
  reallocation, and near-limit alignment coverage gaps. No source or test
  changed.
  ExceptionDispatchInfo and its complete direct fixture add two reports under a
  green 4/4 filter. Current-.NET/C++ probes confirm medium SR-AUD-155: null
  exception_ptr is accepted then deferred to undefined rethrow behavior instead
  of immediate ArgumentNullException. No production or test source changed.
  GCSettings and the complete shared Runtime fixture add two reports under a
  green 82/82 filter (with the direct GC subset 9/9). Current-.NET source/C++
  probes confirm medium SR-AUD-156: both setters retain invalid enum values,
  including caller-set NoGCRegion, instead of validating their public domains.
  No production or test source changed.
  AmbiguousImplementationException and ExternalException add two reports under
  a green shared 7/7 filter. Current-.NET/C++ probes confirm medium
  SR-AUD-157 through SR-AUD-159: both retain COR_E_SYSTEM instead of their
  derived HResults; Ambiguous has the wrong catch hierarchy and lacks a causal
  constructor; External lacks error-code construction, ErrorCode, and its
  specialized formatted diagnostic. No production or test source changed.
  Four small CompilerServices metadata headers add four reports under a green
  9/9 filter. Their compiler-unconsumed native adaptation is documented; one
  low parity finding, SR-AUD-160, remains because the direct test locks
  post-construction mutation of IsOptional where current .NET permits only
  initialization-time assignment. No production or test source changed.
  Nine remaining small compiler-marker and derived state-machine headers add
  nine reports; their shared StateMachine 2/2 and marker 1/1 fixtures were
  already green. Each makes its compiler-unconsumed C++ adaptation explicit,
  and no production consumer or new finding was identified. No production or
  test source changed.
  MethodImplOptions, MethodCodeType, and MethodImplAttribute add three reports
  under a green 10/10 targeted group. Values and representable constructor
  state match current .NET; reports record untested flags/unknown raw values
  and explicit non-consumption by C++ code generation. No new finding or
  production/test source change.
  ConditionalWeakTable adds one report under a green direct 7/7 filter.
  C++/managed probes confirm medium SR-AUD-161 and SR-AUD-162: snapshot Reset
  rewinds and retains non-current values where current .NET retains only
  Current, and unconstrained templates admit scalar tables that managed class
  constraints reject. No production or test source changed.
  RuntimeHelpers adds one report under a green direct 5/5 filter. Implemented
  native identity, cleanup, subarray, and reference-content operations are
  coherent; CLR metadata and stack/CER methods explicitly throw or state their
  no-op native adaptation. No new finding or production/test source change.
  VersioningAttributes adds one report under a green 11/11 fixture group.
  Current-.NET/C++ probes confirm medium SR-AUD-163 and SR-AUD-164: the public
  OSPlatformAttribute hierarchy/common consumer is absent, and nullable or
  mutable metadata is flattened into immutable constructor strings. No
  production or test source changed.
  InteropAttributes adds one report under a green 23/23 fixture group.
  C++/managed probes confirm medium SR-AUD-165 through SR-AUD-168: the
  UnmanagedType surface is wrong/incomplete, StructLayout/DllImport defaults
  diverge, MarshalAs/COM metadata is truncated/retyped, and the declarative
  attributes have no native ABI, marshalling, or P/Invoke consumer. No
  production or test source changed.
  The POSIX signal header/source/test group adds five reports under a green
  9/9 direct filter. Safe helpers confirm high SR-AUD-169, SR-AUD-171, and
  SR-AUD-172 plus medium SR-AUD-170: registration destroys instead of
  preserving a prior signal disposition, a non-cancelled SIGTSTP stops the
  process unlike current .NET's Unix policy, the supposedly non-blocking
  self-pipe can block in the raw handler, and supported positive raw Unix
  signal values are rejected. No production or test source changed.
  SerializationInfo and StreamingContext add two reports under a green 3/3
  shared test filter and a standalone warnings-as-errors compile. Their
  intentionally empty surfaces are the explicit permanent legacy-
  serialization adaptation recorded in CLAUDE.md, and no production consumer
  exists; no new finding or source/test change resulted.
  Runtime's CMake registration and README add the final two Runtime reports:
  their Core.Base/Collections.Core dependency declaration matches the
  generated catalogue, and module-boundary validation passes with 41 physical
  modules/90 edges. All 518 eligible Runtime files are now mirrored; no new
  finding or source/test change resulted.
  Uri's final CMake report completes all 28 eligible Uri files. Its static
  target and Core.Base-only public edge agree with the generated catalogue;
  boundary validation still passes with 41 physical modules/90 edges. No new
  finding or source/test change resulted.
  CharUnicodeInfo, UnicodeCategory, and CharTests2 add three reports under a
  green 63/63 direct filter. C++/managed probes confirm medium SR-AUD-173 and
  SR-AUD-174: numeric APIs recognize only ASCII/a few Latin-1 values, while
  C-locale category predicates label common assigned BMP code points as
  OtherNotAssigned. The documented non-BMP mapping remains an explicit
  adaptation. No production or test source changed.
  BFloat16 adds one report under a green 67/67 BitConverter fixture. A
  bit-level probe confirms medium SR-AUD-175 and SR-AUD-176: float conversion
  truncates rather than round-to-nearest-even, and the public type is a narrow
  bit wrapper missing most current .NET numeric/parse/format/conversion
  surface. No production or test source changed.
  NumberStyles and the shared integer parser add two reports under a green
  43/43 direct filter. C++/managed probes confirm medium SR-AUD-177 and
  SR-AUD-178: valid integer `AllowExponent` is ignored, and undefined or
  incompatible style masks are silently accepted instead of throwing
  `ArgumentException`. No production or test source changed.
  Experimental Property/ReadonlyProperty, PortableFromChars, Prop, and
  SharpRuntimeHelper add five reports under a green 8/8 integration filter.
  Probes confirm high SR-AUD-180 (old-Apple fallback ignores its range end),
  medium SR-AUD-179 (assignment returns stale cache), and low SR-AUD-181
  (documented experimental auto/custom macros do not compile). No production
  or test source changed.
  EnvironmentVariableTarget and generic IEqualityComparer add two reports
  under green 99/99 Environment and 4/4 comparer filters. Their public
  ordinal/interface contracts and direct consumers are coherent; the comparer
  fixture remains pending as a complete collections-source audit. No new
  finding or source/test change resulted.
  StringNormalizationExtensions and NormalizationForm add two reports under a
  green 5/5 focused filter, but every direct case is ASCII/empty. C++/managed
  FormC probes confirm medium SR-AUD-182: decomposed `e` + U+0301 is reported
  normalized and preserved as `65CC81`, where .NET reports false and composes
  it to `C3A9`. The source documents the Unicode-table stub but leaves the
  counterpart API public and otherwise unsupported only by behavior. No
  production or test source changed.
  The four direct UnauthorizedAccessException, ObjectDisposedException,
  TimeoutException, and StackOverflowException fixture sources add four
  reports under a green 41/41 focused filter (four StackOverflow cases are in
  a previously audited companion source). Normal construction/HResult coverage
  is coherent; the reports document missing causal-inner, null/UTF-8,
  non-default-HResult, producer-integration, and real-overflow assertions. No
  new finding or source/test change resulted.
  Int32NewTests adds one fully reviewed report under a green 9/9 focused
  filter. Its `MinValue` magnitude, normal comparison, and fixed hash paths
  agree with .NET; the audit records tautological hash, boundary/tie,
  parse/format, and shared SR-AUD-021/022 assertion gaps. No new finding or
  source/test change resulted.
  NotFiniteNumberExceptionTests adds one report under a green 9/9 filter;
  direct C++/managed evidence confirms `COR_E_NOTFINITENUMBER` (`80131528`)
  across all six construction routes. No new finding; the audit retains NaN
  payload/sign, inner identity, null/UTF-8, and real-producer assertion gaps.
  CultureInvariantFormattingTests adds one report under a green 1/1 run using
  installed `en_US.utf8`; reviewed stream paths explicitly imbue
  `std::locale::classic()` and retain invariant output. Its report records the
  host-locale skip path plus custom-facet and concurrency coverage gaps. No
  new finding or source/test change resulted.
  Batch13BufferTests adds one report under a green 10/10 generic
  BlockCopy/unsigned-MemoryCopy filter. It guards checked primitive vector,
  overlap, and capacity paths, but does not assert high SR-AUD-067 raw negative
  metadata or SR-AUD-051 nontrivial generic-vector lifetime corruption. No new
  finding or source/test change resulted.
  Threading AsyncCallback, EventResetMode, WaitHandle, and EventWaitHandle add
  four reports under green 9/9 valid-mode/multi-wait tests. C++/managed probes
  confirm medium SR-AUD-183: empty/null WaitAll/WaitAny collections and invalid
  timeouts silently return success/timeout or can loop; SR-AUD-184: invalid
  EventResetMode values are accepted and yield incoherent behavior instead of
  an argument error. No production or test source changed.
  Dedicated Threading AsyncCallbackTests and Core's mixed MiscNewTests add two
  reports under green 4/4 and 6/6 filters. They confirm normal marker/callback
  construction but leave real APM transitions, callback failure, and async
  wait/lifetime behavior unasserted. No new finding or source/test change
  resulted.
  Core Batch4Tests adds one report under a green 21/21 six-suite filter. It
  covers normal Resolve/APM/exception/GC metadata paths but omits SR-AUD-123's
  nullable resolution, SR-AUD-094/096 HResults, and real APM/GC telemetry
  integration. No new finding or source/test change resulted.
  Core Batch11ArrayTests adds one report under a green 38/38 fifteen-suite
  range/copy/search/read-only filter. It provides broad normal integer vector
  coverage but omits existing SR-AUD-044/046/051/052 raw/overlap/nontrivial,
  float-ordering, and callable-diagnostic paths. No new finding or source/test
  change resulted.
  Core Batch15TypesTests adds one report under a green 59/59 seven-suite
  filter. It covers ordinary Math/exception/type-handle paths but locks in
  SR-AUD-068's constructible identity `ValueType` and masks SR-AUD-111 through
  its include order. No new finding or source/test change resulted.
  IO BinaryData plus its Core and IO direct fixtures add three reports under
  green 33/33 and 15/15 filters. C++/managed evidence confirms medium
  SR-AUD-185: invalid UTF-8 byte `FF` remains raw C++ text instead of U+FFFD;
  SR-AUD-186: ReadOnlyMemory construction snapshots bytes rather than wrapping
  them. The IO fixture's nonnegative hash assertion extends low SR-AUD-018. No
  production or test source changed.
  Threading WaitCallback, WaitOrTimerCallback, and LockRecursionPolicy add
  three reports under a green 4/4 related registered-wait/ordinal filter. The
  aliases are coherent native pointer/function adaptations, but direct
  state/timeout/error/lifetime coverage is still absent. No new finding or
  source/test change resulted.
  `Progress<T>` adds SR-AUD-058: empty event-style callbacks are accepted then
  later throw `std::bad_function_call`, unlike .NET event null-add behavior.
  FormattableString extends SR-AUD-015: brace replacement reinterprets inserted
  values, breaks escaping, and retains missing indices; its factory also has a
  low-severity false empty-format exception claim (SR-AUD-059). CharEnumerator
  adds no confirmed defect under its 11/11 state-machine filter; the MDArray
  constants-only surface also passed its 2/2 direct filter. `ValueTuple` and
  its direct tests passed their combined 53/53 filter, but extend SR-AUD-046:
  raw comparison makes a NaN item compare equal to finite data and raw equality
  makes a NaN tuple unequal to itself instead of using .NET default comparer /
  equality-comparer behavior. `DateOnly` source/header/tests passed 119/119,
  yet its day-number/day/month/year extreme-input paths hit UBSan-confirmed
  signed overflow before range handling (SR-AUD-060), and its ISO parser
  accepts arbitrary trailing text (SR-AUD-061). `StringComparer` adds no new
  implementation defect under its 42/42 filter, but its case-sensitive hash
  test extends SR-AUD-018 by forbidding a valid collision. `Tuple` and both
  direct suites passed 94/94, but raw NaN comparison extends SR-AUD-046,
  `tupleHashCombine` has UBSan-confirmed signed overflow (SR-AUD-062), and
  public mutable tuple fields violate .NET Tuple immutability (SR-AUD-063). See
  `Lazy<T>` passes its 38/38 focused filter yet accepts invalid modes,
  defers empty factories to `bad_function_call`, and wrongly throws for
  PublicationOnly recursion (SR-AUD-064 through SR-AUD-066). `Buffer` passes
  38/38 direct tests but raw BlockCopy turns negative count into ASan-confirmed
  unbounded `memmove` (SR-AUD-067), while generic typed-vector byte copying
  extends SR-AUD-051 with a string-vector double-free. See
  `audit/AUDIT_FINDINGS_INDEX.md`.

- The current Threading checkpoint adds two medium findings: `AsyncLocal<T>`
  calls its notification before committing the new thread-local value, so the
  callback sees stale ambient data (SR-AUD-214); and the documented minimal
  `ExecutionContext::Run` adapter treats a null context as normal where .NET
  throws `InvalidOperationException` (SR-AUD-215).  The focused native suite
  passed 10/10; direct C++/.NET probes provide the behavioral evidence.  These
  are audit records only, not production changes.

- Subsequent Threading review adds TSan-confirmed races in lock-free
  `LazyInitializer` target publication and `ThreadLocal<T>::Dispose`, and an
  ASan-confirmed dangling `SynchronizationContext::Current` raw pointer.
  `ThreadLocal<T>` also defers empty factories, permits disposed
  `IsValueCreated`, and ignores its `trackAllValues` option; default
  `SynchronizationContext::Send` silently drops an empty callback.  These are
  SR-AUD-216 through SR-AUD-222; focused native tests remain green (16/16 and
  6/6) because they omit those boundaries.  Audit-only, no source/test change.

- All 72 eligible files in `modules/threading` now have mirrored audit reports.
  Component-boundary validation reports 41 physical modules and 90 dependency
  edges, its generated catalogue is current, and the Threading target builds.
  No production or test code changed in this audit phase.

- All five eligible files in `modules/security-cryptography-random` now have
  mirrored reports. Its component graph/catalogue validation and static-target
  build pass; existing SR-AUD-012 remains the module's only confirmed finding.

- TimeZone audit has begun. Legacy `TimeZone::CurrentTimeZone` snapshots one
  base offset and always reports no DST, so under New York it gives the wrong
  January offset/DST result (SR-AUD-223); its exception HResults match .NET.

- `TimeZoneInfo` adds SR-AUD-224 through SR-AUD-229: stale failed TryFind
  output; missing factory/adjustment validation; case-sensitive IDs; reduced
  rule identity; and current DST data incorrectly stored as BaseUtcOffset.
  Focused fixtures pass 99/99 but do not cover these boundaries.

- All 12 eligible files in `modules/time-zone` now have mirrored reports. The
  component graph/catalogue validation and static target build pass; the full
  TimeZone fixture passes 114/114. No production/test change occurred.

- All 21 eligible files in `modules/threading-tasks` now have mirrored reports.
  Its complete fixture passes 171/171, but ASan confirms that
  `TaskCanceledException` exposes a dangling raw Task pointer after its input
  expires (SR-AUD-230). Direct C++/.NET probes also confirm delayed
  `bad_function_call` instead of immediate missing-delegate rejection
  (SR-AUD-231) and acceptance of invalid parallel maximum degrees
  (SR-AUD-232). Audit-only; no production/test change occurred.

- All seven eligible files in `modules/threading-channels` now have mirrored
  reports; its full fixture passes 39/39. Direct C++/.NET comparisons show
  zero capacity acts as a buffer of one (SR-AUD-233), ReadAsync leaks a close
  error rather than wrapping it in `ChannelClosedException` (SR-AUD-234), and
  an invalid FullMode can overfill a bounded channel (SR-AUD-235). Audit-only;
  no production/test change occurred.

- All five eligible files in `modules/collections-async` now have mirrored
  reports. Its six smoke tests pass; synchronously adapted shared-pointer
  enumeration/disposal is explicitly documented, with no additional
  evidence-backed finding. Audit-only; no production/test change occurred.

- All four eligible files in `modules/storage` now have mirrored reports.
  StoragePaths smoke tests pass 2/2, component-boundary validation reports
  41/90, and its static target builds. Its platform-specific root-selection
  policy has no direct managed equivalent and produced no new evidence-backed
  finding. Audit-only; no production/test change occurred.

- All seven eligible files in `modules/net-mime` now have mirrored reports;
  the full fixture passes 26/26. Direct parser/setter comparisons agree with
  .NET for trailing separators and an empty Boundary; its documented practical
  MIME grammar/encoded-word limits create no additional evidence-backed
  finding. Audit-only; no production/test change occurred.

- All eight eligible files in `modules/net-http-json` now have mirrored
  reports. Six content-only tests pass; the two loopback-client cases remain
  environment-blocked at socket creation. ASan confirms null HttpContent
  dereference in `ReadFromJson` (SR-AUD-236), where current .NET throws
  `ArgumentNullException`. Audit-only; no production/test change occurred.

- All eleven eligible files in `modules/collections-object-model` now have
  mirrored reports. ASan confirms a `ReadOnlyObservableCollection` destroyed
  while its shared source survives leaves a captured `this` callback, and a
  later source mutation is stack-use-after-scope (SR-AUD-237). Audit-only; no
  production/test change occurred.

- All eight eligible files in `modules/timers` now have mirrored reports; its
  complete fixture passes 9/9. Native/current-.NET probes confirm that a
  throwing `Elapsed` handler aborts the C++ process (SR-AUD-238) and that an
  Elapsed handler receives null rather than the raising Timer (SR-AUD-239).
  Audit-only; no production/test change occurred.

- All eight eligible files in `modules/net-security` now have mirrored
  reports; its complete fixture passes 13/13. The generated TLS cipher-suite
  values match current .NET 310/310, while UBSan confirms signed-overflow UB
  in the hash of a valid 255-byte ALPN protocol (SR-AUD-240). Audit-only; no
  production/test change occurred.

- All ten eligible files in `modules/io-isolated-storage` now have mirrored
  reports; its library and the dependent IO fixture pass 527/527. A temporary
  root probe confirms that an absolute POSIX path escapes the C++ isolated
  store root, unlike current .NET's separator normalization (SR-AUD-241).
  Audit-only; no production/test change occurred.

- All ten eligible files in `modules/io-compression-zip` now have mirrored
  reports; its library builds and focused ZIP integration passes 38/38. A
  native null `Stream` causes Read-mode SIGSEGV and Create silently drops its
  output instead of throwing `ArgumentNullException` (SR-AUD-242). Audit-only;
  no production/test change occurred.

- All thirteen eligible files in `modules/console` now have mirrored reports;
  its complete fixture passes 123/123. Native/current-.NET probes confirm that
  invalid ConsoleColor values are accepted (SR-AUD-243) and negative cursor
  positions are stored/emitted rather than rejected (SR-AUD-244). Audit-only;
  no production/test change occurred.

- All fourteen eligible files in `modules/text-regular-expressions` now have
  mirrored reports. ASan confirms a Match continuation captures raw Regex
  `this` and calls it after Regex destruction (SR-AUD-245), while current .NET
  continues safely after GC. Audit-only; no production/test change occurred.

- All sixteen eligible files in `modules/security` now have mirrored reports;
  its complete fixture passes 38/38. Native/current-.NET comparison confirms
  bytewise native role matching rejects Unicode `ÄDMIN`/`ädmin` even though
  managed `OrdinalIgnoreCase` accepts it (SR-AUD-246). VerificationException
  and CAS/transparency attributes remain documented ignored-surface
  placeholders. Audit-only; no production/test change occurred.

- All 426 eligible files in `modules/core` now have mirrored reports. The
  complete Core.Base fixture passes 4,946/4,946; the seven final declaration/
  fixture reports add no independent finding but record missing regressions for
  existing String and DateTime defects. Audit-only; no production/test change
  occurred.

- All seventeen eligible files in `modules/net-websockets` now have mirrored
  reports. Its target builds; 22/24 tests pass while two loopback cases are
  blocked by sandbox socket policy. ASan confirms raw `ClientWebSocket`
  lifetime UAF (SR-AUD-247); C++/.NET probes add request-header injection,
  invalid subprotocol acceptance, dropped causal exception, ignored
  cancellation, and inert keep-alive options (SR-AUD-248..252). Audit-only; no
  production/test change occurred.

- All twenty-four eligible files in `modules/component-model` now have mirrored
  reports; its dedicated fixture passes 98/98. The supported metadata,
  notifications, change/init interfaces, and async-completion adapters are
  coherent. DataAnnotations and PropertyDescriptorCollection remain explicit
  ignored metadata/stub surfaces. Audit-only; no production/test change
  occurred.

- All twenty-three eligible files in `modules/net-network-information` now
  have mirrored reports. Its target builds and 28/39 tests pass; eleven live
  interface/ICMP cases are blocked by sandbox permissions. Native/current-.NET
  probes confirm delayed Ping async validation, sliced failure causes, and
  fabricated default reply options (SR-AUD-253..255). Audit-only; no
  production/test change occurred.

- All twenty-five eligible files in `modules/io-compression` now have mirrored
  reports; its existing target passes 22/22. ASan/UBSan confirms unbounded
  negative-length raw zlib input and a null inner-stream dereference; native/
  current-.NET probes add invalid mode/post-close behavior and inert compression
  strategy/options constructors (SR-AUD-256..259). Audit-only; no production/
  test change occurred.

- All twenty-five eligible files in `modules/io-hashing` now have mirrored
  reports; its target passes 96/96. ASan/UBSan confirms raw positive-null
  buffer dereferences; a native probe confirms Adler/CRC negative lengths
  silently succeed; and source comparison confirms XXH's named little-endian
  helpers are host-endian object copies (SR-AUD-260..262). Audit-only; no
  production/test change occurred.

- All twenty-nine eligible files in `modules/net-sockets` now have mirrored
  reports. Its target builds but only 54/88 tests pass because 34 native
  socket/send cases are sandbox-blocked. Native probes confirm negative packet
  count coercion and silent invalid NetworkStream descriptor I/O; source/.NET
  comparison adds raw Socket task lifetime and Tcp/Udp IPv4/validation gaps
  (SR-AUD-263..267). Audit-only; no production/test change occurred.

- All thirty-four eligible files in `modules/diagnostics` now have mirrored
  reports; its target passes 159/159. Direct native probes confirm Process
  negative-timeout, destruction/zombie, restart, EINTR, and detached-tree
  defects; source review identifies multithreaded-fork child risk; and TSan
  confirms the Debug provider race (SR-AUD-268..275). Audit-only; no
  production/test change occurred.

- All twenty-eight eligible files in `modules/numerics` now have mirrored
  reports; its target passes 299/299. Direct probes confirm that degenerate
  Vector2/3/4 and Plane normalization returns finite zero instead of current
  .NET NaN propagation (SR-AUD-276); Complex exposes the wrong `Abs` return
  type and an incompatible default text form (SR-AUD-277); and generic-math
  static interface declarations fail only at consumer linkage (SR-AUD-278).
  Current .NET source confirms zero aspect/orthographic dimensions are not
  validation defects. Audit-only; no production/test change occurred.

- All fifty-three eligible files in `modules/globalization` now have mirrored
  reports; its target passes 676/676. Direct probes confirm byte/code-point
  treatment where .NET requires grapheme text elements (SR-AUD-279), a
  constructible Gregorian-fallback Calendar base (SR-AUD-281), an inert
  `IdnMapping::AllowUnassigned` setting (SR-AUD-282), silently ASCII-only
  comparison/casing behavior (SR-AUD-283/284), and fabricated unknown
  culture/region metadata (SR-AUD-285). TSan confirms that CurrentCulture and
  CurrentUICulture are racy process globals rather than per-thread/task state
  (SR-AUD-280). Audit-only; no production/test change occurred. Resume the
  next coherent module shard.

- All thirty-nine eligible files in `modules/text` now have mirrored reports;
  its target passes 233/233. ASan confirms unsafe signed raw decoding and
  null-fallback dereference (SR-AUD-286/287); TSan confirms mutable shared
  factory state (SR-AUD-288); UBSan confirms `StringBuilder::CopyTo` signed
  capacity overflow (SR-AUD-295). Behavioral probes add Latin-1,
  UTF-16-count, preamble/fallback/partial-unit, Rune Unicode, byte-indexed
  StringBuilder, Web encoder, CompositeFormat, and EncodingInfo gaps
  (SR-AUD-289..294, SR-AUD-296..299). Audit-only; no production/test change
  occurred. Resume the next coherent module shard.

- All fifty eligible files in `modules/net` now have mirrored reports; its
  target passes 238/238. ASan/UBSan and behavioral probes confirm unchecked
  SocketAddress decoding (SR-AUD-300), IPv6 scope narrowing/loss
  (SR-AUD-301/303), permissive endpoint parsing (SR-AUD-302), DNS fast-path
  validation/filtering gaps (SR-AUD-304), cookie domain isolation, constructor
  state, unbounded storage, and indexing failures (SR-AUD-305..308), and
  incomplete WebUtility HTML behavior (SR-AUD-309). Audit-only; no
  production/test change occurred.

- All thirty-two eligible files in `modules/net-http` now have mirrored
  reports. Its target builds and 126/132 tests pass; all six loopback failures
  are blocked at sandbox socket creation. ASan confirms an async HttpClient
  use-after-free; direct probes confirm permissive URL/status parsing,
  re-sendable request messages, CR/LF HTTP/MIME serialization,
  case-sensitive headers, invalid response fields, false StringContent charset
  metadata, and unbounded/weak terminal response framing (SR-AUD-310..318).
  Audit-only; no production/test change occurred. Resume `Net.Http.Headers`.

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
- The local ten-job selective consumer matrix, including a direct
  `Collections.Blocking` consumer, previously passed. The tracked GitHub Actions
  matrix currently covers only nine fixtures and omits that direct consumer;
  this is recorded as `SR-AUD-001`. Text.Json retains its target absence and
  negative include-leakage assertions.
- The full native baseline is a warning-free build with 12,694 passing tests
  across 36 component executables and one integration executable.
- Doxygen 1.9.8 currently emits 1,941 warnings against the tracked
  1,942-warning ceiling. Run `scripts/check_doxygen_warnings.sh` to prevent
  increases; lower totals are accepted, and a dedicated Ubuntu 24.04 CI job
  enforces the same limit. A Doxygen-version change requires a deliberate
  re-baseline.
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
- `ImmutableList<T>::Reverse(index, count)` reverses only the requested valid
  range in an independently backed list; zero-length boundary ranges are
  allowed and invalid ranges throw `ArgumentOutOfRangeException`.
- `ImmutableList<T>` supports full-list and range `Sort(IComparer<T>)` through
  the established generic comparer interface. C++ references cannot be null,
  so the parameterless overload remains the default-comparer route.
- `ImmutableList<T>` supports equality-based `Remove`, vector `RemoveRange`,
  and `Replace` operations through the default equality operator or an
  `IEqualityComparer<T>`. `RemoveRange` processes input values sequentially,
  and `Replace` throws `ArgumentException` when the old value is absent.
- `ImmutableList<T>` supports default and comparer-based range `IndexOf` and
  `LastIndexOf` lookups. They validate their distinct forward/backward range
  contracts, including the valid empty `LastIndexOf(..., 0, 0, ...)` case.
- `ImmutableList<T>` supports full-list and range `BinarySearch` with
  `IComparer<T>`. A miss returns the complement of the absolute insertion
  point, including for valid empty ranges.
- `ImmutableList<T>::Sort(index, count)` sorts only a valid requested range
  with the default comparison. Zero-length boundary ranges are valid and the
  source plus outside elements remain unchanged.
- `BigInteger` supports `&, |, ^, ~` and their compound assignments using
  infinite two's-complement semantics, including negative and beyond-native
  integer values. It also supports signed left and arithmetic right shifts,
  plus minimal byte-vector conversion with signed/unsigned and little/big-endian
  options.
- `ImmutableList<T>` provides `CreateBuilder()` and `ToBuilder()` with core
  mutable operations and independent `ToImmutable()` snapshots. The current
  vector backend copies source/snapshot contents rather than claiming .NET's
  tree-backed O(1) conversion characteristics.
- `UTF7Encoding` implements RFC 2152 modified-Base64 conversion for BMP and
  astral Unicode, including optional direct characters and U+FFFD recovery for
  malformed shifts. UTF-7 remains obsolete and unsuitable for new protocols.
- `Trace::WriteIf` and `Trace::WriteLineIf` now conditionally preserve the
  existing stderr write/newline behavior; category and listener surfaces stay
  intentionally deferred.
- `ProcessStartInfo` now supplies explicit child-only environment overrides;
  unspecified values inherit, empty values remain empty, and invalid variable
  names are rejected before forking.
- POSIX `Process::Start` now reports child setup and exec failures synchronously
  with the executable path and native error text, rather than returning a
  process that later exits with code 127.
- MinGW-w64 GCC 14-win32/CMake 3.31.6 and Emscripten 5.0.7/CMake 3.31.6 both
  compile the post-modular `All` graph and selective `Text.Json` libraries.
  This is compile-only evidence: cross tests were deliberately disabled.
- Focused TSan scenarios for concurrent collections, `ConditionalWeakTable`,
  generic task continuations, and `TaskExtensions::Unwrap` are clean; matching
  ASan/LSan ownership scenarios, including 100 continuation teardowns, pass.

The local `plan.sqlite3` snapshot contains 16,201 classified `task` rows and
1,774 ticket rows: 1,772 completed, including closed audit ticket #1766,
post-audit tickets #1767, #1768, #1769, #1770, and #1771, and follow-up
correction ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`), one `wontfix` row
(#1772, obsoleted by #1771), and one deliberately inactive `blocked` row
(#1773, the out-of-repository CNA / mobile-eggbert `CopyTo` sweep). No ticket is
active. Ticket #1737 records the
completed P0 split, tickets
#1738/#1739 the MemoryStream and generic-continuation repairs, ticket #1740 the
XML whitespace repair, #1741 the completed cross-build revalidation and
`WebProxy` portability fix, #1742 focused sanitizer evidence, and #1743 the
`ImmutableList<T>` predicate-query slice. Ticket #1744 records seekable
`BinaryReader::PeekChar`, #1745 `ImmutableList<T>::Sort`/`Reverse`, and #1746
`ImmutableList<T>::GetRange`, #1747 `ImmutableList<T>::ConvertAll`, #1748 the
UTF-8 `BinaryReader` batch-character APIs, #1749 `ImmutableList<T>` copying,
and #1750 its custom comparison sort, and #1751 its range reverse. The
database also records #1752 for its `IComparer<T>` sorting overloads, #1753
for its equality-based item mutations, #1754 for its equality-based range
queries, #1755 for its comparer-aware binary search, #1756 for its default
range sort, #1757 for `BigInteger` bitwise operators, #1758 for signed
`BigInteger` shifts, #1759 for byte-vector conversion, and #1760 for the
`ImmutableList<T>` Builder core, #1761 for RFC 2152 UTF-7, and #1762 for
conditional Trace writes, #1763 for ProcessStartInfo environment overrides, and
#1764 for synchronous Process startup-failure reporting, and #1765 for the
Doxygen warning baseline. The baseline is 1,942 warnings under Doxygen 1.9.8,
enforced by `scripts/check_doxygen_warnings.sh` without a mass comment-only
rewrite; the database is git-ignored and is not part of a fresh clone.

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

`ImmutableList<T>::Sort()` and `Reverse()` return independently backed,
reordered lists using the default `T::operator<` and full-list order. Both
leave the source unchanged; range sort remains deferred. The later tickets add
custom-comparison sort and range reverse.

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

## P2 completion: `ImmutableList<T>::Reverse(index, count)`

Range reverse now returns an independently backed list with only the requested
range reordered. It accepts zero-length ranges at either valid boundary and
throws `ArgumentOutOfRangeException` for invalid index/count combinations;
three regressions cover ordering, immutability, boundaries, and validation.

## P2 completion: `ImmutableList<T>::Sort(IComparer<T>)`

Full-list and range sort now accept the project's `IComparer<T>` interface.
They preserve source immutability and range boundaries; because the C++ API
uses a reference, a null comparer is not representable and parameterless
`Sort()` remains the default-comparer route. Two regressions cover descending
full-list/range order and invalid range validation.

## Post-audit remediation roadmap

Ticket #1766 is closed as an evidence-only audit.  The next work is *not* a
return to the old consumer-driven P2 queue: it is a separately authorised
remediation phase against the 364-item audit inventory (91 high, 262 medium,
11 low).  The first bounded repair, ticket #1767 on
`feature/remediation-coll-enum`, is complete. It covers SR-AUD-356 and
SR-AUD-364 / CCF-018 with the project's safe C++ reference-returning
enumerator lifecycle and current .NET BitArray mutation semantics.
`feature/audit` remains the audit-handoff snapshot.

### Completed first remediation ticket

Ticket #1767 makes `Current` throw `InvalidOperationException` before
the first successful `MoveNext` and after enumeration ends across List, Queue,
Stack, SortedList, LinkedList, ObjectModel Collection/ReadOnlyCollection, and
ConcurrentBag/Queue/Stack snapshot enumerators. BitArray enforces the same
lifecycle and rejects enumeration after collection mutation. Its 13/13
permanent regressions, 1,435/1,435 Collections.Core target, direct ASan/UBSan
probe, warning-free configured build, 41-module/90-edge boundary check,
database/catalogue/diff controls, and network-permitted 12,694/12,694 full
gate pass. Doxygen is below its ceiling at 1,941/1,942. LeakSanitizer alone
could not initialize under the sandbox's
`ptrace` policy; the probe is clean with ASan and UBSan active. The scope excludes
LinkedListNode ownership (SR-AUD-357) and the raw ICollection CopyTo redesign
(SR-AUD-358).

### Completed second remediation batch

Design ticket #1768 and implementation ticket #1769 remediated SR-AUD-357
(CCF-019). `LinkedListNode<T>` now refers to an independently allocated,
reference-counted node with an explicit null/detached/attached state, so
`Remove`, `Clear`, removal through another copied handle, and destruction of the
owning `LinkedList<T>` detach the node and retain its value instead of leaving a
dangling `std::list` iterator. The repair also added the .NET existing-node
insertion overloads, the detached-node constructor, the `Value` setter, the
`List` accessor, node identity comparison, defined list copy/move semantics, and
a bidirectional `LinkedList<T>::iterator` replacing the exposed `std::list`
iterator. Ticket #1767's enumerator lifecycle guard is unchanged. Its 49
permanent regressions, 1,484/1,484 Collections.Core target, clean direct
ASan/UBSan/LeakSanitizer probe, `-Werror` standalone Collections.Core consumer
fixture, warning-free build, 41-module/90-edge boundary check,
validator-test/catalogue/database/selective/diff controls, and
network-permitted 12,743/12,743 full gate all pass; Doxygen stays at
1,941/1,942. The scope excludes the raw ICollection CopyTo redesign
(SR-AUD-358) and the JsonNode/XML LINQ members of CCF-019.

### Completed third remediation batch

Design-only ticket #1770 recorded the SR-AUD-358 / CCF-020 raw-`CopyTo`
contract in `docs/ICollectionCopyToDesign.md` and changed no production or test
source, so there is no test-count, boundary, catalogue, or Doxygen delta. Its
evidence is seven repository-local compile/sanitizer probes, summarised in
"Completed raw-CopyTo design: ticket #1770" above and reproducible from section
17 of the design record. SR-AUD-358 stays `confirmed`; CCF-020 is marked
design-complete. The scope excludes every implementation change, JsonNode
(SR-AUD-327), XML LINQ (SR-AUD-333), and SR-AUD-090.

### Opening a remediation ticket

Every repair ticket should name its owning `SR-AUD-*` finding(s), link the
per-file reports and any `CCF-*` shared cause, and state the exact public
contract that is being restored.  It must also identify affected public
headers, compatibility/migration risk, focused tests, sanitizer or native
probe where applicable, and the completion gate.  Preserve the original audit
probe as a regression until an equivalent permanent test exists; a patch that
only makes an ad-hoc probe quiet is not closure.

Keep tickets small and independently reviewable.  A shared root cause may
justify coordinated changes, but it does not justify a catch-all rewrite of a
module or an unrelated API cleanup.  Update `audit/AUDIT_FINDINGS_INDEX.md`,
the selected per-file reports, `plan.md`, and this handoff when the status or
scope changes; retain the evidence and do not delete a finding simply because
one reproduction changes shape.

### Recommended dependency order

1. **Plan the collection safety contracts before patching their symptoms.**
   These are the immediate high-priority handoff items from the final shard:

   - Completed `REMED-COLL-ENUM` ticket #1767 covers SR-AUD-356 and
     SR-AUD-364 / CCF-018 with one lifecycle policy and permanent regressions
     across all affected storage categories.
   - Design ticket #1768 (`REMED-COLL-LINKED-NODE-DESIGN`) covers SR-AUD-357's
     `LinkedListNode` owner/iterator lifetime and is recorded in
     `docs/LinkedListNodeLifetime.md`; implementation ticket #1769
     (`REMED-COLL-LINKED-NODE`) carries it out.  CCF-019 also contains JsonNode
     and XML LINQ instances, but they remain separate repair tickets
     unless a deliberately shared lifetime abstraction is introduced and its
     public compatibility is reviewed.
   - SR-AUD-358 / CCF-020 was a design-first item and is now closed.  Design
     ticket #1770 (`REMED-COLL-COPYTO-DESIGN`) recorded the selected boundary in
     `docs/ICollectionCopyToDesign.md`: a length-aware, statically typed
     `System::Span<std::any>` destination behind a non-virtual `ICollection`,
     with one protected `copyToCore` hook per implementation and typed
     `std::vector` overloads on the concrete collections.  Implementation ticket
     #1771 (`REMED-COLL-COPYTO`, P0, size M) landed it after the user explicitly
     approved the source/ABI break, including the instruction not to retain a
     compatibility overload; cleanup ticket #1772 is `wontfix` because its work
     had to happen inside #1771.  Do not reopen the finding or re-litigate the
     removal.
   - Then take SR-AUD-359 through SR-AUD-363 as small semantic/concurrency
     tickets after the lifetime and raw-output decisions have a stable
     contract.  They are not substitutes for the three safety boundaries.
     SR-AUD-363 is **done** (ticket #1775, the Hashtable `IDictionary` key and
     view contracts) and SR-AUD-360 is **done** (ticket #1778, the
     `ConcurrentDictionary::AddOrUpdate` compare-and-retry loop).  SR-AUD-359
     (`ReadOnlyDictionary::Empty` is an assignable process-static singleton) is
     now **design-complete**: design ticket #1779
     (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`) recorded the selected fix — a
     `const`-reference return type — in
     `docs/ReadOnlyDictionaryEmptyDesign.md`; implementation ticket #1780
     (`REMED-COLL-READONLYDICT-EMPTY`) is inactive and `blocked` pending
     explicit approval of the public return-type change, the same approval
     category SR-AUD-358's `ICollection::CopyTo` removal needed. SR-AUD-359
     stays `confirmed` until #1780 lands. SR-AUD-361
     (`SortedSet::GetViewBetween` returns a detached snapshot rather
     than a live write-through view) was described here as "not yet
     design-first-ready … it needs a full tree-backed rearchitecture of
     `SortedSet<T>` before any bounded implementation ticket could be written".
     **Corrected by ticket #1782 (2026-07-27):** that premise does not hold —
     `std::set` already supplies `lower_bound`, `upper_bound`, and stable
     iterators, so no hand-rolled tree is needed. SR-AUD-361 is now
     **design-complete**: design ticket #1782
     (`REMED-COLL-SORTEDSET-VIEW-DESIGN`) recorded the selected architecture — a
     shared reference-counted `State` plus optional bounds, one public type,
     unchanged return type — in `docs/SortedSetLiveViewDesign.md`;
     implementation ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L)
     is inactive and `blocked` pending explicit approval of the `const` removal
     on `GetViewBetween`, the snapshot-to-live-view semantic change, and the
     object-layout change — the same approval category SR-AUD-358's
     `ICollection::CopyTo` removal and SR-AUD-359's `Empty()` return type
     needed. SR-AUD-361 stays `confirmed` until #1783 lands. The one that
     remains unstarted is SR-AUD-362 (`FrozenDictionary::Create` accepts
     duplicate keys last-value-wins — reviewed under ticket #1778 and again
     under #1779's SR-AUD-362 reconciliation: this matches the current .NET
     reference's own documented intent and is not actually a defect; left
     `confirmed` since no `not-a-defect` status exists in this repository's
     index, but not an active fix target).

2. **Take self-contained ASan/UBSan-backed public-input failures next.**
   SR-AUD-338 and SR-AUD-341 (null text-stream and `MemoryStream`
   construction boundaries) are suitable examples because the audit has
   focused reproductions and the affected ownership/input boundary is much
   smaller than the collection interface redesign.  Similar candidates must
   remain one public contract per ticket; validate null, empty, valid, and
   disposal/error paths rather than adding only the crashing input.

3. **Repair shared high-severity causes by a scoped family plan, not by a
   repository-wide sweep.**  CCF-004 (defined arithmetic at native/fixed-width
   boundaries), CCF-005 (conversion special values and range validation),
   CCF-009 (process-wide PRNG concurrency), CCF-013 (in-place Base64 write
   order), and CCF-019 are high-leverage groups.  First enumerate the exact
   listed `SR-AUD-*` members and their current tests, then split work along
   public type/module boundaries.  In particular, preserve documented .NET
   checked versus unchecked behavior; replacing all arithmetic with one
   generic "safe" helper without that distinction would create parity
   regressions.

4. **Follow with contained parity and diagnostics work.**  Date/time grammar
   validation (CCF-002), numeric formatting/parsing policy (CCF-003 and
   CCF-006 through CCF-008), floating comparison semantics (CCF-010),
   formatting/UTF/try-output contracts (CCF-012 and CCF-014 through CCF-017),
   and the remaining medium/low findings can be scheduled after their owning
   high-severity blockers.  Assertion-only gaps found by the audit should be
   strengthened alongside the relevant repair, not claimed as an independent
   product fix unless they hide a confirmed behavioral defect.

5. **Only resume the former P2 candidates after the remediation queue has a
   deliberately chosen stopping point.**  Wider debugger/process/XML
   surfaces, advanced `ImmutableList<T>::Builder` operations, and incremental
   Doxygen reduction remain legitimate later work, but must not displace
   confirmed crash, lifetime, or public-contract findings.

### Required evidence for each repair

At a minimum, a remediation ticket must add or improve permanent assertions
for the normal path, the audited failure path, boundary inputs, and observable
exception/result semantics.  Memory/lifetime tickets also require the focused
ASan/UBSan reproduction to be clean after the fix; concurrency tickets require
an appropriate bounded race/stress check.  Run the smallest relevant test
target during development, then its owning component target, the configured
build, component-boundary validation, plan-database consistency, and
`git diff --check` before marking a ticket done.  Record exact commands and
results, including any platform limitation, in the ticket handoff.

The sandbox's local-network restriction is not a waiver: the six enabled
`Net.Http` local-server tests currently fail at `Socket::Socket: socket()
failed`.  A network-permitted full `scripts/local_ci_check.sh build` run is a
required closure gate for the first remediation batch and again whenever a
repair affects networking, shared runtime infrastructure, or the final batch.
Do not disable, filter out, or recategorize those tests to manufacture green
evidence.

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
- Keep the audit evidence intact. Continue later repairs on dedicated bounded
  branches; do not merge to `develop`/`master` or create tags without explicit
  approval.

## Cold resume

1. Read `CLAUDE.md`, this file, `plan.md`, `audit/AUDIT_FINAL_REPORT.md`,
   `audit/AUDIT_FINDINGS_INDEX.md`, and
   `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`.  The first three audit documents
   are the authoritative closure, inventory, and shared-cause record.
2. Inspect `git status --short --branch`, `audit/AUDIT_PROGRESS.md`, and the
   selected finding's mirrored reports and current implementation/tests.  Do
   not search for a new audit shard: the 1,748-file audit is complete.
3. Tickets #1767, #1768, #1769, #1770, #1771, #1774, #1775, #1776, and #1777
   are complete and no ticket is active; preserve their permanent regressions,
   the two recorded designs, and the retained audit evidence.
4. The LinkedListNode lifetime contract is recorded in
   `docs/LinkedListNodeLifetime.md` and implemented. Do not redesign it, do not
   reopen SR-AUD-357, and keep `LinkedListNodeLifetimeTests.cpp` and
   `test/consumer/collections_linked_list.cpp` in place. Those 49 permanent
   regressions are the durable replacement for the ASan/UBSan probe; the probe
   itself lived in the gitignored `build-probe-linkednode/` directory and is
   reproducible from section 9 of the design record, not from a tracked file.
5. The SR-AUD-358 / CCF-020 `ICollection::CopyTo` boundary is recorded in
   `docs/ICollectionCopyToDesign.md` by completed design ticket #1770
   (`REMED-COLL-COPYTO-DESIGN`, P0, size S) and implemented by ticket #1771;
   section 21 of that document is the implementation closure, including the one
   approved deviation (no deprecated shim) and the ABI consequences. Do not
   redesign it and do not reopen the inventory: all six implementations, three
   test call sites, zero production callers, the .NET reference behaviour, six
   alternatives, the exception matrix, and the probes are already recorded there.
   The probes lived in the gitignored `build-probe-copyto/` directory and are
   reproducible from sections 17 and 21, not from a tracked file.
6. Implementation ticket #1771 (`REMED-COLL-COPYTO`, P0, size M) is **done**.
   It removed the pure virtual `CopyTo(void*, intcs)` from `ICollection` under
   explicit user approval of the source- and ABI-breaking change, with no
   retained compatibility overload. Do not reintroduce one, do not reopen
   SR-AUD-358, and keep `CopyToBoundaryTests.cpp` and
   `test/consumer/collections_copyto.cpp` in place: those 128 permanent
   regressions plus the compile-time `AcceptsDestination` assertions are the
   durable replacement for the probes, which live in the gitignored
   `build-probe-copyto/` directory and are reproducible from sections 17 and 21
   of the design record. Cleanup ticket #1772 is `wontfix` (its work happened
   inside #1771). The only open follow-up is ticket #1773
   (`REMED-COLL-COPYTO-DOWNSTREAM`, P2, size S), which stays **blocked**:
   deferred until CNA and mobile-eggbert intentionally upgrade to a
   sharp-runtime revision containing the `ICollection` `CopyTo` ABI change.
   No downstream usage or compatibility claim has been made yet. It would
   sweep those repositories for `CopyTo` calls and rebuild them against the new
   vtable, per `docs/Migration-ICollectionCopyTo.md` §9. Neither repository is
   in this checkout, so do not guess at their usage, inspect them, or modify
   them from here.
7. Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) is
   **done** and is the current end of the collections thread. Every mutation
   counter in `modules/collections/include/` now uses
   `System::Collections::detail::MutationCounter` (or, for `LinkedList<T>` and
   `BitArray`, `detail::NarrowMutationCounter`); do not reintroduce a bare
   `intcs version_`, and read the contract note in `CLAUDE.md`'s architecture
   invariants before adding a counter to a new collection. Keep
   `modules/collections/tests/System/Collections/CollectionVersionCounterTests.cpp`,
   `test/consumer/collections_mutation_version.cpp`, and
   `test/consumer/collections_mutation_version_negative.cpp` in place: those 336
   permanent regressions plus the two fixtures are the durable replacement for
   the probes, which live in the gitignored `build-probe-collversion/` directory
   and are reproducible from sections 4, 12, 14, and 15 of
   `docs/CollectionVersionCounterSweep.md`, not from a tracked file. The
   assignment repair is a deliberate behaviour strengthening recorded in
   `README.md`'s breaking-changes section — do not "restore" the old
   counter-copying assignment. Two follow-ups are **blocked** on an explicit
   object-size approval and must not be begun without it: #1788
   (`sizeof(LinkedList<T>)` 40 → 48) and #1789
   (`sizeof(BitArray::Enumerator)` 32 → 40). #1790
   (`REMED-COLL-LIST-INDEXER-VERSION`) is inactive `todo` and is a separate
   non-versioning parity decision, not part of the sweep.
8. Follow-up ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`, P1, size XS) is
   **done**. It corrected `detail::requireValidCopyDestination` so a null
   pointer is rejected only when paired with a *positive* length — a
   null-pointer destination with a zero length (`ObjectSpan{nullptr, 0}`, a
   default-constructed empty `std::vector<std::any>`) is now a valid empty
   destination, matching .NET's `new object[0]`. Do not reintroduce the old
   unconditional null check, do not restore `CopyTo(void*, intcs)`, and keep
   the extended `CopyToBoundaryTests.cpp` cases and
   `build-probe-copyto/probe10_empty_span_correction.cpp` in place: they are
   the durable evidence for the corrected rule, recorded in section 22 of
   `docs/ICollectionCopyToDesign.md`. SR-AUD-358 remains `remediated`.
