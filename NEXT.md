<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# NEXT.md

*Last verified: 2026-07-27. Branch: `feature/remediation-coll-readonlydict-empty-design`.
The P0
component-boundary repair, three P1 parity repairs, P1 portability revalidation, and
twenty-two bounded P2 API slices are complete: 41 physical modules, 90 production
dependency edges, and 13,021 tests across 37 executables. The repository-wide,
evidence-only audit is complete under `audit/` (local ticket #1766). Remediation
tickets #1767 (enumerator lifecycle), #1768 (LinkedListNode lifetime design),
#1769 (LinkedListNode lifetime implementation), #1770 (raw `ICollection::CopyTo`
design), #1771 (raw `ICollection::CopyTo` implementation), #1774 (raw
`ICollection::CopyTo` zero-length-destination correction), #1775
(`Hashtable` `IDictionary` key/view contracts), #1776
(`ArgumentNullException` duplicate parameter suffix), #1777 (typed
`CopyTo` doc-comment sync), #1778 (`ConcurrentDictionary::AddOrUpdate`
compare-and-retry), and #1779 (`ReadOnlyDictionary::Empty` const-reference
design) are complete; the
node contract is recorded in
[`docs/LinkedListNodeLifetime.md`](docs/LinkedListNodeLifetime.md), the copy
boundary in [`docs/ICollectionCopyToDesign.md`](docs/ICollectionCopyToDesign.md)
(see its section 22 for the #1774 correction), and the mutable-singleton
contract in
[`docs/ReadOnlyDictionaryEmptyDesign.md`](docs/ReadOnlyDictionaryEmptyDesign.md),
with consumer guidance in
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
explicit approval `ICollection::CopyTo`'s removal needed.
**No ticket is active.** #1771 removed the
pure virtual `CopyTo(void*, intcs)` from
`System::Collections::ICollection` under explicit user approval, so this is a
source- and ABI-breaking release for downstream consumers, which must rebuild.
#1774 then corrected #1771's validation rule so that a null-pointer destination
with a zero length (e.g. `ObjectSpan{nullptr, 0}` or a default-constructed empty
`std::vector<std::any>`) is a valid empty destination; only a null pointer
paired with a positive length is still rejected. This is a behavioral
relaxation, not a further source or ABI break. Implementation ticket #1780
(`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS) is inactive and `blocked`
pending that approval; see the "Completed ReadOnlyDictionary::Empty design"
section below.

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
this ticket.

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
     stays `confirmed` until #1780 lands. The two that remain unstarted are
     SR-AUD-361 (`SortedSet::GetViewBetween` returns a detached snapshot rather
     than a live write-through view — confirmed by comparison against .NET's
     378-line `TreeSubSet` nested class while selecting between it and
     SR-AUD-359 for ticket #1779; it needs a full tree-backed rearchitecture of
     `SortedSet<T>` before any bounded implementation ticket could be written,
     so it is not yet design-first-ready in the way SR-AUD-357/SR-AUD-358/
     SR-AUD-359 were) and SR-AUD-362 (`FrozenDictionary::Create` accepts
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
7. Follow-up ticket #1774 (`REMED-COLL-COPYTO-EMPTY-SPAN`, P1, size XS) is
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
