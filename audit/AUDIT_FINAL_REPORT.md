# Sharp Runtime audit final report

## Outcome

The evidence-only repository audit is complete. Every one of the 1,748
eligible tracked first-party text-like files has exactly one mirrored report
under `audit/<source-path>.audit.md`; the 1,699 runtime-module files are also
fully covered. No production source or test was changed during this phase.

At audit closure, the findings index recorded 364 confirmed issues. It now
retains all 364 entries while marking 354 `confirmed` and ten `remediated`
(SR-AUD-089, SR-AUD-090, SR-AUD-356, SR-AUD-357, SR-AUD-358, SR-AUD-359,
SR-AUD-360, SR-AUD-361, SR-AUD-363, and SR-AUD-364):

| Severity | Count |
|---|---:|
| High | 91 |
| Medium | 262 |
| Low | 11 |
| **Total** | **364** |

The index is the remediation inventory, while per-file reports contain the
source-level evidence, missing assertions/diagnostics, and focused target for
each finding. `AUDIT_CROSS_CUTTING_FINDINGS.md` identifies causes that require
coordinated repairs rather than isolated symptom patches.

## Closure evidence

- Mirror reconciliation: 1,748 reports for 1,748 eligible files; no missing
  source-path mirror and every newly completed report has an `AUDITED` status.
- Focused audit validations: `SharpRuntimeTests_Collections_Core` 1,422/1,422;
  `SharpRuntimeTests_Xml` 377/377; `SharpRuntimeTests_IO` 527/527;
  `SharpRuntimeTests_Xml_Linq` 92/92; and
  `SharpRuntimeTests_Security_Cryptography` 80/80 at their respective
  checkpoints.
- Full configured build: `gmake -C build -j4` completed all registered
  backend/runtime libraries and test executables during final reconciliation.
- Audit controls: `python3 scripts/db_consistency_check.py --db plan.sqlite3`,
  `python3 scripts/validate_module_boundaries.py --root .`, and
  `git diff --check` passed during final reconciliation and are required again
  for each remediation change.
- Direct ASan/UBSan probes establish memory-safety findings including
  SR-AUD-338, SR-AUD-341, SR-AUD-356, SR-AUD-357, and SR-AUD-358. Functional
  probes establish the associated public-contract findings cited by the
  per-file reports.

During the audit phase, the broad local CI gate was environment-limited: six
`Net.Http` local-server tests fail at socket creation in this sandbox
(`Socket::Socket: socket() failed`). The tests remained enabled. The first
post-audit remediation batch later satisfied this prerequisite in a
network-permitted run; see the status below.

## Repair handoff

Do not repair from this report en masse. Create small, independently validated
tickets that preserve public compatibility and retain the audit evidence. Start
with high-severity memory safety and lifecycle boundaries, then repair grouped
root causes from the cross-cutting report. In particular, Collections findings
SR-AUD-356 through SR-AUD-358 need a design review because they affect shared
enumerator and raw-polymorphic APIs; patching one concrete collection would
leave sibling public paths unsafe.

The audit phase is closed. The next phase is user-approved post-audit
remediation planning, not further source changes under ticket #1766.

## Post-audit remediation status

Ticket #1767 completed the first bounded remediation batch on 2026-07-27.
SR-AUD-356 and SR-AUD-364 / CCF-018 are marked `remediated`; their original
audit evidence remains in place. A shared lifecycle guard now prevents invalid
`Current` access across the ten affected collection enumerators, and
`BitArray` uses mutation-version checks.

Closure evidence is 13/13 permanent focused regressions, 1,435/1,435
Collections.Core tests, a clean direct ASan/UBSan probe, and
`scripts/local_ci_check.sh build` in a network-permitted environment:
12,694/12,694 tests across 37 executables with zero build warnings/errors.
Boundary validation remains 41 physical modules and 90 dependency edges;
catalogue, database consistency, and diff checks pass. Doxygen 1.9.8 reports
1,941 warnings against the 1,942-warning ceiling. LeakSanitizer alone could
not initialize under the sandbox's `ptrace` policy, so its probe was rerun
with leak detection disabled while AddressSanitizer and UndefinedBehaviorSanitizer
remained active.

Design ticket #1768 and implementation ticket #1769 completed the second
bounded batch on 2026-07-27. SR-AUD-357 is marked `remediated` and CCF-019 is
partially remediated; the original audit evidence remains in place.
`LinkedListNode<T>` now refers to an independently allocated, reference-counted
node with an explicit null/detached/attached state, so removal, `Clear`, and
destruction of the owning `LinkedList<T>` detach the node and retain its value
instead of leaving a dangling `std::list` iterator. The selected contract is
recorded in `docs/LinkedListNodeLifetime.md`.

Closure evidence is 49 permanent regressions in
`LinkedListNodeLifetimeTests.cpp`, 1,484/1,484 Collections.Core tests, a
standalone `Collections.Core` public-header consumer fixture compiled with
`-Werror`, a direct ASan/UBSan/LeakSanitizer probe reporting `failures=0` with
no diagnostic, and a network-permitted `scripts/local_ci_check.sh build` run of
12,743/12,743 tests across 37 executables with zero build warnings/errors.
Boundary validation is unchanged at 41 modules and 90 edges; validator-test,
catalogue, database, selective-component, and diff checks pass, and Doxygen
1.9.8 remains at 1,941 warnings with no new warning from the touched headers.
The JsonNode (SR-AUD-327) and XML LINQ (SR-AUD-333) members of CCF-019 remain
open by design.

Design ticket #1770 completed the third bounded batch on 2026-07-27 and made no
production change. SR-AUD-358 / CCF-020 was left **design-complete but still
`confirmed`** at that point, so the findings index then recorded 361 open
findings and three `remediated`. The selected contract is recorded in
`docs/ICollectionCopyToDesign.md` — a length-aware, statically typed
`System::Span<std::any>` destination behind a non-virtual `ICollection`, so the
destination's capacity and element type are validated exactly once before any
implementation writes, with `CopyTo(void*, intcs)` leaving the virtual interface
and remaining briefly as a deprecated, never-writing shim. Seven repository-local
compile/sanitizer probes back the design: virtual templates are ill-formed;
removal cannot silently misbind; the full prototype is clean under
ASan/UBSan/LeakSanitizer with `-Werror`; derived-class name hiding requires
`using ICollection::CopyTo;`; the current boundary still reproduces three
sanitizer aborts plus one silent LeakSanitizer-only element-type corruption; the
affected public headers compile standalone against `Collections.Core` +
`Core.Base`; and a retained deprecated overload is a compile error under the
repository's own `-Werror` policy. Implementation is proposed as inactive ticket
#1771, gated on explicit user approval of the narrow public-API break.

Implementation ticket #1771 closed that batch on 2026-07-27, after the user
explicitly approved the public source- and ABI-breaking change. SR-AUD-358 /
CCF-020 is now `remediated`, so the findings index records **360 open findings
and four `remediated`**; the original audit evidence remains in place.
`virtual void CopyTo(void*, intcs) = 0` is removed from `ICollection` and
replaced by non-virtual, validating `CopyTo(ObjectSpan, intcs)` and
`CopyTo(std::vector<std::any>&, intcs)` over a single protected pure virtual
`copyToCore(ObjectSpan, intcs)` hook, with checked typed
`std::vector<void*>` / `std::vector<DictionaryEntry>` overloads on the concrete
collections. One departure from the design record is recorded in section 21 of
`docs/ICollectionCopyToDesign.md`: the deprecated, never-writing shim was **not**
retained, so a stale call site fails to compile rather than throwing at run time.
Because a pure virtual member was removed, every `ICollection`/`IList`/
`IDictionary` vtable changes and all consumers must be rebuilt; the consumer
guidance is in `docs/Migration-ICollectionCopyTo.md`, and the CNA /
mobile-eggbert sweep is inactive ticket #1773, as neither repository is in this
checkout.

Closure evidence is 128 permanent regressions in `CopyToBoundaryTests.cpp`
parameterised over every `ICollection` implementation (also 128/128 under
ASan + UBSan + LeakSanitizer), 1,612/1,612 Collections.Core tests, a standalone
`Collections.Core` public-header consumer fixture compiled with `-Werror` and
executed successfully, a replacement ASan/UBSan/LeakSanitizer probe reporting
`failures=0` with no diagnostic and no leak on the four scenarios that previously
crashed or leaked, a captured compile log showing that the old raw calls now
produce four `no matching function` errors naming the replacements, and a
`scripts/local_ci_check.sh`-equivalent run of 12,871/12,871 tests across 37
executables with zero build warnings/errors. Boundary validation is unchanged at
41 modules and 90 edges; validator-test, catalogue, database, selective-component,
and diff checks pass, and Doxygen 1.9.8 reports 1,942 warnings, exactly at the
1,942 ceiling and unchanged from the pre-ticket baseline: the README link to the
new migration document adds one instance of the pre-existing unresolved-
markdown-link warning that every README documentation link produces, offsetting
one warning removed from `ICollection.hpp`.

Follow-up ticket #1774 corrected a narrow defect in #1771's own validation rule
on the same day, still 2026-07-27: `detail::requireValidCopyDestination`
rejected every null-pointer destination outright, including a valid empty
`ObjectSpan{nullptr, 0}` or default-constructed empty `std::vector<std::any>`
copied from an empty collection. The rule now rejects a null pointer only when
paired with a positive length; a non-empty collection copied into a
zero-length destination still fails, but on capacity, not nullness. SR-AUD-358
and CCF-020 remain `remediated` — this did not reopen either finding. Evidence:
`CopyToBoundaryTests.cpp` grew to 1,662 Collections.Core tests, and the new
standalone probe `build-probe-copyto/probe10_empty_span_correction.cpp` passes
under ASan + UBSan + LeakSanitizer with zero diagnostics and zero leaks.
Recorded in section 22 of `docs/ICollectionCopyToDesign.md`.

Ticket #1775 completed the fourth bounded batch on 2026-07-27 and remediates
SR-AUD-363; the findings index now records **359 open findings and five
`remediated`**. The original audit evidence remains in place. `Hashtable`
violated two public `IDictionary` contracts: `getKeysProperty()` and
`getValuesProperty()` returned `nullptr` although the interface documents each
as returning an `ICollection` over the keys/values, and the raw-key entry
points stringified a null key as the address text `"0"`.

Two facts beyond the original evidence were established by direct probe before
the repair. First, the null view is an ASan-confirmed SEGV plus a UBSan
`member access within null pointer of type 'struct ICollection'` for a consumer
that follows the interface documentation, while the sibling
`ListDictionaryInternal` answers the *identical* caller code correctly — so
this is an interface defect with divergent implementations, not a
Hashtable-local omission. Second, the stringified null key aliases the ordinary
string key `"0"` accepted by the `Add(const std::string&, const std::any&)`
overload, and a third null-key entry point, `Remove(const char*)`, terminated
on `std::string`'s null construction with a `std::logic_error` invisible to
code catching `System::Exception&`.

Both properties now return a live, caller-owned `MemberCollection` whose
`Count`, `SyncRoot`, `IsSynchronized`, `GetEnumerator`, and `copyToCore`
delegate to the owning table, following the
`ListDictionaryInternal::MemberCollection` precedent and matching .NET's
`KeyCollection`/`ValueCollection`; the views reuse the #1771/#1774 copy
boundary unchanged. `toKey()` is now the single validating conversion site
every raw-key path passes through, so no entry point can skip the null check
and no non-null address stringifies to `"0"`. `IDictionary`'s own `@return`
documentation was corrected from "lifetime managed by the concrete dictionary",
which neither implementation nor any caller did, to the implemented rule:
non-null, live, and caller-owned. No public signature changed and no virtual
member was added or removed, so unlike #1771 this is neither a source nor an
ABI break.

Closure evidence is 70 permanent regressions in
`DictionaryKeyAndViewContractTests.cpp` whose view cases are parameterised over
both non-generic `IDictionary` implementations (also 70/70 under ASan + UBSan +
LeakSanitizer with no diagnostic and no leak), a 33-assertion replacement probe
reporting `failures=0` on the previously fatal scenarios plus liveness,
non-trivial values, a 20,000-entry table and destruction order,
1,732/1,732 Collections.Core tests, a standalone `Collections.Core`
public-header consumer fixture compiled with `-Werror` and executed
successfully, and a network-permitted `scripts/local_ci_check.sh build` run of
12,991/12,991 tests across 37 executables with zero build warnings/errors.
Boundary validation is unchanged at 41 modules and 90 edges; validator-test,
catalogue, database, selective-component, and diff checks pass, and Doxygen
1.9.8 remains at exactly 1,942 warnings, at the ceiling and unchanged.

Two separate pre-existing defects found while implementing #1775 were recorded
as inactive tickets rather than folded into it: ticket #1776
(`System::ArgumentNullException(paramName)` emits its `(Parameter 'x')` suffix
twice, because its own `makeMsg()` appends it and the
`ArgumentException(message, paramName)` base appends it again) and ticket #1777
(four typed `CopyTo` doc-comments still describe ticket #1771's superseded
null-destination rule). #1775's assertions deliberately checked exception type,
parameter name, and leading message text rather than an exact string, so they
stayed correct once #1776 landed.

**Correction:** the paragraph above, and #1776's own opening notes, described
#1776 as **not** a new audit finding, on the premise that SR-AUD-001 through
SR-AUD-364 were frozen at closure with nothing covering this defect. That
premise was wrong: SR-AUD-089 (a null-`const char*` crash in the same
constructors) and SR-AUD-090 (the duplicate suffix itself) already existed as
`confirmed` findings inside that frozen range, filed against this exact file
during the original audit. This correction is recorded here rather than
silently rewritten into the paragraph above, per this repository's practice of
preserving historical narrative; see the ticket #1776 entry below for the
remediation this produced.

Ticket #1776 completed a fifth bounded batch on 2026-07-27 on local branch
`feature/remediation-argument-null-message` and remediates SR-AUD-089 and
SR-AUD-090; the findings index now records **357 open findings and seven
`remediated`**. The original audit evidence in
`audit/modules/core/include/System/ArgumentNullException.hpp.audit.md` remains
in place. Root cause: `ArgumentNullException(paramName)`'s private `makeMsg()`
helper composed `"Value cannot be null. (Parameter 'x')"` and passed that
already-suffixed text to the `ArgumentException(message, paramName)` base
constructor, whose own `appendParamName()` appended the identical suffix a
second time; because `makeMsg()` concatenated the raw C-string before the base
constructor's null guard could run, a null `paramName` also reached
`std::char_traits<char>::length(nullptr)` first (SR-AUD-089). The fix passes
the raw, unsuffixed default message straight to
`ArgumentException(message, paramName)`, exactly matching .NET's own
`ArgumentNullException(paramName) : base(SR.ArgumentNull_Generic, paramName)`,
so the base constructor is the single site that both appends the suffix once
and null-guards the C-string overload. `getParamNameProperty()`, HResult
(`E_POINTER`), the `(paramName, message)` and `(message, innerException)`
overloads, and sibling `ArgumentException`/`ArgumentOutOfRangeException`
behavior are unchanged; no public signature, virtual member, or inheritance
changed, so this is neither a source nor an ABI break.

Closure evidence is 26 permanent regressions across `ArgumentNullExceptionTests.cpp`
(20, covering every constructor overload's exact message, single-suffix
occurrence counts, empty/punctuated parameter names, copy/move, catch-through
`ArgumentNullException&`/`ArgumentException&`/`System::Exception&`, and a
direct null-`const char*` non-crash regression for SR-AUD-089),
`ArgumentExceptionTests.cpp` (3), and `ArgumentOutOfRangeExceptionTests.cpp`
(3) pinning that those sibling types were never affected; the two pre-existing
exact-message workarounds this defect forced now assert the single-suffix
message directly (`DictionaryKeyAndViewContractTests.cpp`'s
`expectNullKeyRejected` from #1775, `LinkedListNodeLifetimeTests.cpp`'s
`ExpectArgumentNullMessage` from #1769); `SharpRuntimeTests_Core_Base`
4,972/4,972 and `SharpRuntimeTests_Collections_Core` 1,732/1,732; the
`Core.Base` standalone public-header consumer fixture extended to construct,
throw, and catch an `ArgumentNullException` through `System::Exception`,
compiling and running under `-Werror`; and a network-permitted
`scripts/local_ci_check.sh build` run. This is a pure message-composition fix
with no allocation, ownership, or string-lifetime change, so a dedicated
sanitizer campaign was not run beyond the existing focused-suite coverage.
Ticket #1777 (`REMED-COLL-COPYTO-DOC-SYNC`, P3, size XS) subsequently closed on
2026-07-27 on local branch `feature/remediation-copyto-docs`: it corrected the
four typed `CopyTo` doc-comments that still cited ticket #1771's superseded
null-destination rule so they state the rule ticket #1774 corrected instead.
Documentation only; SR-AUD-358 and CCF-020 remain `remediated` and were not
reopened. It is not a new `SR-AUD-*` identifier and does not change the
counts above.

Ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`, P2, size S) completed a
sixth bounded batch on 2026-07-27 on local branch
`feature/remediation-coll-concurrentdict-addorupdate` and remediates
SR-AUD-360; the findings index now records **356 open findings and eight
`remediated`**. The original audit evidence in
`audit/modules/collections/include/System/Collections/Concurrent/ConcurrentDictionary.hpp.audit.md`
remains in place. `ConcurrentDictionary::AddOrUpdate` (both the addValue-constant
and addFactory overloads) snapshotted the existing value, invoked the update
factory outside the lock, then unconditionally overwrote the entry with the
factory's result regardless of whether another thread had mutated or removed
the entry in the meantime -- silently discarding the intervening write. Real
.NET's `TryUpdateInternal` instead gates the commit on
`EqualityComparer<TValue>.Default.Equals(currentValue, observedValue)` and, on
a mismatch, retries the whole operation: re-observes the current value and
re-invokes the factory against it. sharp-runtime's existing doc-comment on
this method already recorded the deviation as a deliberate simplification
("this port's ConcurrentDictionary doesn't assume TValue equality-
comparability"), but `TryUpdate` on the same class already requires
`operator==`, so extending that same requirement to `AddOrUpdate` closes the
gap without introducing a new constraint category to the type.

Both overloads now loop: after computing the new value outside the lock (the
factory is still never invoked with the internal mutex held, preserving the
existing no-lock-across-user-code reentrancy/deadlock-avoidance guarantee
documented on `GetOrAdd`), the commit re-acquires the lock, re-reads the
entry, and writes only if it still equals the previously observed value;
otherwise the operation retries against the newly observed state. A key
absent at the initial observation that is concurrently added by another
thread falls through to the update branch on retry rather than double-adding.
No public signature changed and no virtual member was added or removed, so
this is neither a source nor an ABI break, and it proceeded without additional
user approval under the repository's compatible-bug-fix rule.

Pre-fix reproduction (gitignored
`build-probe-concurrentdict/probe1_lost_update.cpp`, compiled directly with
`-fsanitize=address,undefined` and separately with `-fsanitize=thread`)
deterministically reproduced the finding's own `add-or-update-result=1
final=1` symptom across 5/5 runs: a coordinated second thread writes `10`
through the indexer while the update factory is blocked after observing `0`;
the pre-fix implementation discarded that write and produced `final=1`.
Post-fix, the same probe produced the correct `final=11` across 20/20 ASan+UBSan
runs and 5/5 ThreadSanitizer runs, with no sanitizer diagnostic. A second
stress probe (`build-probe-concurrentdict/probe2_stress.cpp`, 16 threads each
issuing 2,000 `AddOrUpdate` calls against one shared key) passed cleanly under
ThreadSanitizer with no data race and the exact expected total (32,000),
confirming the retry loop introduces no new synchronization defect.

Closure evidence is 4 new permanent regressions in
`ConcurrentDictionaryTests.cpp`: a deterministic coordinated intervening-write
repro for each `AddOrUpdate` overload (matching the pre-fix probe's shape), a
key-added-concurrently retry case, and an 8-thread/500-iteration-per-thread
contention stress case asserting the final counter reflects every increment.
The full `ConcurrentDictionaryTest` suite (26/26) passed consistently across
five repeated runs; `SharpRuntimeTests_Collections_Core` grew from 1,732/1,732
to 1,736/1,736; and a network-permitted `scripts/local_ci_check.sh build` run
passed 13,021/13,021 tests across 37 executables with zero build
warnings/errors (was 13,017), including the six local-server `Net.Http` cases.
Boundary validation is unchanged at 41 modules and 90 edges; validator-test,
catalogue, database, selective-component, and diff checks pass. A dedicated
public-header consumer fixture was not added: no public signature or type
surface changed, and the header already compiles as part of the regular
`Collections.Core` build and test target, so a new standalone fixture would
not exercise anything the existing build does not already cover.

**Planning-accuracy note (2026-07-27, discovered while selecting ticket
#1778):** SR-AUD-362 (`FrozenDictionary::Create` "silently overwrites
duplicate keys") was reviewed against the current .NET reference
(`/rv/tmp/runtime/src/libraries/System.Collections.Immutable/src/System/Collections/Frozen/FrozenDictionary.cs`)
while choosing between it and SR-AUD-360 as the next signature-compatible
candidate. .NET's own doc-comment on `FrozenDictionary.Create` and
`ToFrozenDictionary` states that last-value-wins is the *intended* behavior,
explicitly contrasted with `Enumerable.ToDictionary`'s throw-on-duplicate
behavior ("If the same key appears multiple times in the input, the latter
one in the sequence takes precedence. This differs from
`Enumerable.ToDictionary`, with which multiple duplicate keys will result in
an exception."), and `GetExistingFrozenOrNewDictionary`/`CreateFromDictionary`
deliberately use the indexer rather than `Add` "to avoid throwing and to
overwrite existing entries such that last one wins." sharp-runtime's current
`FrozenDictionary::Create` already implements exactly this. SR-AUD-362's
premise -- that .NET's factory "rejects duplicate keys" -- does not hold
against the actual .NET source. This finding was left untouched: not
selected, not reopened as a new ticket (the active-ticket-queue rule permits
only one ticket per session), and its `confirmed` status in the index was not
changed, since correcting it is a documentation-only action distinct from
this ticket's SR-AUD-360 remediation. It is recorded here for future
correction rather than silently acted on.

Ticket #1778 remains the only new inactive-follow-up-free closure; no separate
defect was discovered during its implementation.

Design ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`, P2, size S)
completed a seventh bounded batch on 2026-07-27 on local branch
`feature/remediation-coll-readonlydict-empty-design` and answered SR-AUD-359
without changing any production or test source: `ReadOnlyDictionary<K,V>::
Empty()` returned a non-`const` reference to a process-wide `static`
singleton, so ordinary assignment through it silently rebound the singleton's
private backing map for the remainder of the process, matching the finding's
own `empty-before=0`/`empty-after-assignment=1` reproduction plus two new
facts: an unrelated second call site observed the contamination, and the
identical singleton object (not a copy) was confirmed corrupted. Recorded in
`docs/ReadOnlyDictionaryEmptyDesign.md`: change `Empty()`'s return type to
`const ReadOnlyDictionary<K, V>&`, the literal C++ expression of .NET's
get-only `Empty` property, which has no setter. This is a public signature
change, so per the same approval boundary ticket #1770/#1771 used, implementation
was proposed as separate, inactive ticket #1780, `blocked` pending explicit
user approval; SR-AUD-359 stayed `confirmed` at #1779's close.

Implementation ticket #1780 (`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS)
completed an eighth bounded batch on 2026-07-27 on local branch
`feature/remediation-coll-readonlydict-empty`, after the user explicitly
approved the public return-type change, and remediates SR-AUD-359; the
findings index now records **355 open findings and nine `remediated`**. The
original audit evidence in
`audit/modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp.audit.md`
remains in place. `Empty()`'s declared return type changed from
`ReadOnlyDictionary<K, V>&` to `const ReadOnlyDictionary<K, V>&` exactly as
#1779 designed; no other member, constructor, or the class's copy/move
assignment operators changed, so ordinary, non-singleton instances remain
freely assignable. No public signature other than `Empty()`'s return type
changed and no virtual member was added or removed (the class has none), so
this is a source-breaking change only for the exact hazardous pattern of
declaring an explicit non-`const` reference to hold `Empty()`'s result or
assigning through it -- confirmed absent everywhere in this repository -- and
not an ABI break: `Collections.Core` is an `INTERFACE` (header-only) CMake
target with no exported archive, `Empty()` is emitted as a weak/COMDAT inline
symbol per translation unit, and a direct `nm`/`c++filt` comparison of the
mangled symbol before and after the change shows byte-identical symbol names
(the Itanium C++ ABI does not encode a function's return type in its mangled
name).

Pre-fix reproduction re-ran the design phase's own gitignored
`build-probe-readonlydict/probe1_mutable_empty.cpp` directly against the
still-unmodified production header and reconfirmed
`empty-before=0`/`empty-after-assignment=1`/`second-caller-observes=1`/
`same-instance=1`. Post-fix, two new probes were compiled against the real,
now-modified production header (not a copy, unlike #1779's design-phase
probes): `probe4_production_header_rejects_assignment.cpp` fails to compile
with `error: passing 'const ReadOnlyDictionary<...>' as 'this' argument
discards qualifiers`, and `probe5_production_header_preserves_behavior.cpp`
runs clean under ASan+UBSan with `all-assertions-passed=1`, confirming
singleton identity, emptiness, normal construction, `ContainsKey`, indexer
access, and independent copy-construction are all unaffected.

Closure evidence: two new permanent regressions in
`ObjectModelTests.cpp::ReadOnlyDictionaryTests` -- a `static_assert` pinning
the exact `const ReadOnlyDictionary<K,V>&` return type and
`Empty_RemainsEmptyAfterConstructingUnrelatedInstances`, which constructs and
copies several unrelated instances and reconfirms `Empty()` stays empty and
identity-stable throughout -- while the existing `Empty_IsEmptyAndCached` case
is retained verbatim; a new standalone `Collections.Core` public-header
consumer fixture
(`test/consumer/collections_object_model_readonlydictionary.cpp`) compiles
`-Wall -Wextra -Wpedantic -Werror` and runs successfully, and a companion
negative-compile fixture
(`test/consumer/collections_object_model_readonlydictionary_negative.cpp`)
fails to compile with the same `discards qualifiers` diagnostic through the
repository's own `test/consumer/CMakeLists.txt` harness;
`SharpRuntimeTests_Collections_ObjectModel` grew from 124/124 to 125/125; and
`scripts/local_ci_check.sh build` passed 13,022/13,022 tests across 37
executables with zero build warnings/errors (was 13,021). Module boundaries
remain 41 modules/90 edges; validator-test (7/7), catalogue, database, the
ten-job selective-component matrix, and `git diff --check` all pass, and
Doxygen 1.9.8 stays at exactly 1,942/1,942 -- unchanged, at the ceiling, since
no touched header gained or lost a documented public member.

Design ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`, P2, size M) completed
a ninth bounded batch on 2026-07-27 on local branch
`feature/remediation-coll-sortedset-view-design` and answered SR-AUD-361
without changing any production or test source. **SR-AUD-361 stays
`confirmed`**, now qualified `confirmed (design-complete)`; the counts above are
unchanged at **355 open findings and nine `remediated`**. The original audit
evidence in
`audit/modules/collections/include/System/Collections/Generic/SortedSet.hpp.audit.md`
remains in place.

`SortedSet<T>::GetViewBetween` returns an independent snapshot copy of the
in-range elements instead of .NET's live, range-enforced, bidirectionally
write-through `TreeSubSet`. A repository-local, gitignored `build-probe-sortedset/`
probe reproduced the finding's own `source-add-visible-in-view=0` /
`view-add-visible-in-source=0` symptom and established the complete pre-fix
contract under ASan+UBSan+LeakSanitizer with **no diagnostic and no leak** --
the current implementation is memory-safe and semantically wrong.

The selected architecture, recorded in `docs/SortedSetLiveViewDesign.md`, is
one public type with a tagged representation: `SortedSet<T>` holds
`std::shared_ptr<State>` (the `State` owning the `std::set<T>` and the single
version counter) plus `std::optional<T>` lower and upper bounds, so an object is
either an owning full set or a bounded live view over the same state.
`GetViewBetween` keeps returning `SortedSet<T>` **by value**; `std::shared_ptr`
reproduces .NET's GC lifetime rule exactly, so a view or an iterator that
outlives the set it came from is well-defined rather than dangling. Four other
alternatives were evaluated against a fourteen-row compatibility matrix and
rejected, including a dedicated `SortedSetView<T>` type -- which does not avoid
the layout change, breaks the return type on top of it, and breaks .NET's
structural parity in which a view **is a** `SortedSet<T>`. A working prototype
passes the identical scenario matrix with `failures=0`, clean under
ASan+UBSan+LeakSanitizer including a 100,000-element scale case.

**Planning-accuracy correction (2026-07-27, ticket #1782).** `NEXT.md`,
`plan.md`, and `SortedSet.hpp`'s own `@warning` block each state that a live
view is not achievable on `std::set` without replacing the internal
representation with a hand-rolled tree matching .NET's own, and ticket #1779
used that premise to defer SR-AUD-361 in favour of SR-AUD-359. **The premise
does not hold.** `std::set` already provides `lower_bound`, `upper_bound`, and
stable iterators; a bounded view needs only a shared owner for the container
plus a pair of bounds, as the prototype demonstrates. .NET's `TreeSubSet` is 378
lines mainly because it re-implements tree walks against raw `Node` pointers --
work `std::set` already does. The real cost is the ownership model, the
copy/move semantics, and one required `const` removal, which is why the item
remained design-first. This correction is recorded here rather than rewritten
into the earlier text, per this repository's practice of preserving historical
narrative (see the #1776, #1778, and #1779 corrections above).

Four adjacent defects inside the same member's surface were measured and are
folded into the implementation ticket's scope rather than receiving new
`SR-AUD-*` identifiers, since the audit numbering is frozen at 364:
`GetViewBetween` is the only member spelling its comparisons with `operator>`,
so an element type providing `operator<` alone -- the contract the class's own
doc-comment states -- fails to compile; the returned object does not enforce its
bounds after construction (`view.Add(99)` on a `[3,7]` range succeeds); a nested
`GetViewBetween` may silently widen a bound where .NET throws; and whole-object
assignment defeats the class's advertised fail-fast version guard, producing a
silently wrong dereference on copy-assignment and an ASan-confirmed
`heap-use-after-free` on move-assignment.

Implementation is proposed as separate ticket **#1783**
(`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L), created **`blocked`** pending
explicit user approval of three things together: removing the `const` qualifier
from `GetViewBetween` (measured: the Itanium mangled name changes `_ZNK…` to
`_ZN…`, unlike ticket #1780's `Empty()`, whose name was byte-identical), the
semantic snapshot-to-live-view change, and the `SortedSet<T>` object-layout
change (measured `sizeof(SortedSet<int>)` 56 → 40,
`sizeof(SortedSet<std::string>)` 56 → 104). This is the same approval category
tickets #1770/#1771 and #1779/#1780 required. No in-repository source break
exists: all three `GetViewBetween` call sites are tests using non-`const` sets,
and none asserts a snapshot property, so no existing assertion changes.

Closure evidence for #1782: six repository-local probes (§29 of the design
record), the three existing `GetViewBetween` tests and the 41 mutable-`SortedSet`
tests rerun unchanged and passing, module boundaries unchanged at 41 modules/90
edges, validator-test (7/7), catalogue, database consistency, `git diff --check`,
and Doxygen 1.9.8 at exactly 1,942/1,942 -- unchanged, since this ticket added
only `docs/*.md` and `audit/*.md`, which Doxygen does not scan. The full
`scripts/local_ci_check.sh build` gate was run rather than omitted and passed
13,022/13,022 tests across 37 executables with zero build warnings/errors,
unchanged from ticket #1780. `scripts/check_selective_components.sh` was not
run: no public header and no component metadata changed, which is the condition
that requires it.

## Tenth remediation batch — SR-AUD-361 closed by ticket #1783 (2026-07-28)

Implementation ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L)
completed a tenth bounded batch on 2026-07-28 on local branch
`feature/remediation-coll-sortedset-live-view`, landing the architecture ticket
#1782 selected. **SR-AUD-361 moves from `confirmed (design-complete)` to
`remediated`**; the findings index now records **354 open findings and ten
`remediated`**. The original audit evidence in
`audit/modules/collections/include/System/Collections/Generic/SortedSet.hpp.audit.md`
and the whole #1782 design record are preserved unaltered; a clearly separated
remediation note is appended to each.

The user granted the exact approval design section 28 required, scoped to this
ticket: removing the `const` qualifier from
`SortedSet<T>::GetViewBetween(const T&, const T&)`, the semantic change from a
detached snapshot to a live bidirectionally write-through bounded view, and the
`SortedSet<T>` object-layout change that requires every consumer to be rebuilt.

`SortedSet<T>` now holds `std::shared_ptr<State>` plus `std::optional<T>` lower
and upper bounds, so one public type is either an owning full set or a bounded
live view; `GetViewBetween` still returns `SortedSet<T>` by value, but the
returned object is a handle onto the same tree. The finding's own symptom is
inverted where it was measured: the post-fix probe reports
`source-add-visible-in-view=1` and `view-add-visible-in-source=1`, against the
original evidence's 0 in both directions.

The four adjacent defects the #1782 batch measured are closed with it, without
new `SR-AUD-*` identifiers — the numbering stays frozen at 364. Ordering is now
taken from `std::set::key_comp()`, so probe 3's `operator<`-only element type
compiles `-Werror` and runs where it previously failed with two `no match for
'operator>'` errors; bounds are enforced after construction; nested views may
only narrow; and assignment rebinds the handle rather than overwriting the
version counter, so probe 2's `copy-assign` silently wrong dereference now
yields the correct pre-assignment element and its `move-assign`
**ASan-confirmed `heap-use-after-free`** and `outlive` **ASan
`stack-use-after-scope`** both exit 0 with no diagnostic.

Two limitations are recorded rather than hidden, both in design-record section
30. For a nested call that is simultaneously inverted and widening, this port
follows the design's ordering rule (invalid range first) where .NET's override
structure checks widening first — an observable but bounded divergence, since
both throw. And a ThreadSanitizer probe found that concurrent
`getCountProperty()` on *one* view object races on the lazy Count cache
mirroring .NET's `TreeSubSet._countVersion`; that is documented in the header
instead of synchronized, because `SortedSet<T>` claims no thread safety and this
ticket adds none. Concurrent read-only access through *distinct* handles over
one shared state, including concurrent handle creation and destruction, is
race-free (0 TSan reports over 8 threads and 400 handles, with TSan confirmed
active by a deliberate-race self-test).

Closure evidence for #1783: 47 new permanent regressions in
`modules/collections/tests/System/Collections/Generic/SortedSetLiveViewTests.cpp`
plus two standalone consumer fixtures (positive, `-Werror`, `Collections.Core`
only, exits 0; negative, a `const` caller, correctly rejected);
`SharpRuntimeTests_Collections_Core` 1,783/1,783 with all 41 pre-existing
SortedSet cases passing and no assertion edited; the full
`scripts/local_ci_check.sh build` gate at **13,069 tests across 37 executables**
with zero build warnings and zero errors, up from 13,022; module boundaries
unchanged at 41 modules / 90 edges with no new dependency edge; validator tests
7/7; catalogue current; database consistent; `git diff --check` clean; Doxygen
1.9.8 at **1,937** warnings against the 1,942 ceiling; all ten selective
components passing with a repository-local `TMPDIR`; the whole new suite clean
under ASan+UBSan+LeakSanitizer with LSan verified active by a deliberate-leak
self-test; and a post-fix behavior probe with 82 assertions and `failures=0`.

Ticket #1773 (the out-of-repository CNA / mobile-eggbert `ICollection::CopyTo`
sweep) remains `blocked` and untouched. Neither downstream repository was
inspected, searched, configured, built, or modified; both intentionally remain
on an older sharp-runtime revision and must perform a full rebuild and a
`GetViewBetween` call-site audit whenever they choose to upgrade.

## Post-remediation race correction: ticket #1784 (2026-07-28)

*The #1783 record above is preserved unaltered, including its own report that
the Count-cache race existed and was left unsynchronized. **SR-AUD-361 stays
`remediated`**, the index counts stay at 354 open / ten `remediated`, and this
ticket carries **no new `SR-AUD-*` identifier** — the numbering stays frozen at
364. It corrects a defect introduced by that finding's remediation.*

Ticket #1784 (`REMED-COLL-SORTEDSET-VIEW-COUNT-RACE`, P1, size S), opened and
closed on local branch `feature/remediation-coll-sortedset-count-race`, reverses
#1783's judgement that the Count-cache race was acceptable "since
`SortedSet<T>` claims no thread safety". Three reasons: a C++ data race is
**undefined behaviour**, not a stale-value nuisance; `getCountProperty()` is
`const` and gives a caller no signal that reading Count performs a write; and it
was a **regression**, because the pre-#1783 header's `const` members wrote
nothing. The .NET parallel #1783 relied on does not transfer — a racing `int`
write is defined in the CLR, and .NET documents that its collections support
multiple concurrent **readers** as long as none mutates, which is precisely the
half of the contract #1783 broke.

Reproduced before any production change with a ten-mode ThreadSanitizer probe
(`build-probe-sortedset/probe10_tsan_count_race.cpp`) that never mutates
concurrently, so no report can originate from the unsupported case. Pre-fix:
`same-view-count` **1 race**, `readonly-enumeration` **1**, `nested-views`
**2**, `overlapping-views` **2**, with the `known-race` self-test reporting 2 to
prove TSan active, and `fullset-count`, `copied-handles-count`,
`independent-sets`, `sequential-count`, and `view-churn` already clean.
`fullset-count` being clean pins the defect as **view-specific**: the owning-set
path returns `state_->data.size()` and never touches the cache. Diagnostic:
`Read of size 4 … SortedSet.hpp:315` against
`Previous write of size 4 … SortedSet.hpp:317`, both in `getCountProperty() const`.

Five alternatives were **measured**
(`build-probe-sortedset/probe11_cache_alternatives.cpp`): removing the cache
gives `sizeof(SortedSet<int>)` 40 → 32, a `std::mutex` 80, a `std::shared_mutex`
96, a published `shared_ptr` snapshot 48 — all breaking the layout #1783 had
approved — while same-width atomics stay at exactly 40 (104 for the
`std::string` specialization). A cache moved into the shared `State` was
rejected structurally: arbitrary overlapping view bounds would need an unbounded
keyed map, new allocation, and a new element-type requirement. Selected: two
`std::atomic<intcs>` fields with a **release/acquire publication protocol** —
count stored first (`relaxed`), version stored last (`release`), version loaded
first (`acquire`) — so the (count, version) pair can never be read torn. Two
relaxed atomics would not have sufficed. `state_->version` deliberately stays
plain, and two `static_assert`s make a padded-atomic platform a compile error
rather than a silent ABI break.

The header now states the contract as two unequal halves: concurrent
**mutation** remains unsupported and undefined, with a set and every view over
it one collection for that purpose and **no new promise of mutation safety**;
concurrent **read-only** access is race-free, because no `const` member writes
an unsynchronized field. The type is still not thread-safe — it is merely free
of *internal* races when read.

Post-fix: **0 ThreadSanitizer reports in all nine real modes**, self-test still
reporting 2, and #1783's own unmodified `probe9` `shared-view-count` going
**1 race → 0**. `sizeof(SortedSet<int>)` 40, `sizeof(SortedSet<std::string>)`
104, `sizeof(Iterator)` 40, `alignof` 8, all four value-semantics traits, and
the mangled `GetViewBetween` symbol are **byte-identical** to #1783's stored
probe output, so this revision needs no consumer rebuild of its own account and
required no new user approval.

Evidence: 29 new permanent regressions in
`modules/collections/tests/System/Collections/Generic/SortedSetCountCacheTests.cpp`;
`SharpRuntimeTests_Collections_Core` **1,812/1,812** (was 1,783, with all 47
`SortedSetLiveViewTests` and all 41 pre-existing SortedSet cases passing and no
assertion edited); `scripts/local_ci_check.sh build` at **13,098 tests across 37
executables** (was 13,069) with zero build warnings and zero errors, after which
the recorded floor in `README.md` and `CLAUDE.md` was raised; ASan+UBSan+LSan
**76/76** with LSan verified active by a deliberate-leak self-test (4,112 bytes
in 102 allocations); boundaries 41 modules / 90 edges; validator tests 7/7;
catalogue current; database consistent; `git diff --check` clean; Doxygen 1.9.8
**unchanged at 1,937** against the 1,942 ceiling; all ten selective components
plus `Collections.Core` in isolation; the extended positive consumer fixture
compiling `-Wall -Wextra -Wpedantic -Werror` and exiting 0, with the negative
`const`-caller fixture still correctly rejected.

The **exception-ordering** divergence recorded in the #1783 material above is
deliberately untouched and is now inactive ticket **#1785**
(`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, P3), not begun. A separate
pre-existing issue found during #1784's required overflow analysis —
`State::version` is `int32_t`, incremented without bound, and compared only for
**equality** by both the Count cache and `Iterator::checkVersion` — is inactive
ticket **#1786** (`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3). Both predate
#1783; neither receives a new `SR-AUD-*` identifier.

Ticket #1773 remains `blocked` and untouched. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase,
tag, or publication occurred.

---

## Post-remediation mutation-counter repair: ticket #1786 (2026-07-28)

*The original audit evidence and the #1782/#1783/#1784 notes above are preserved
verbatim and unaltered. **SR-AUD-361 stays `remediated`.** This ticket repairs a
defect that **predates** that finding's remediation — the counter, its type, and
its increment all arrived with ticket 1713 — so it does not reopen the live-view
finding, does not change the findings-index counts (354 open, ten `remediated`),
and carries **no new `SR-AUD-*` identifier**; the audit numbering stays frozen
at 364.*

Ticket #1786 (`REMED-COLL-VERSION-COUNTER-OVERFLOW`, P3, size S) was opened
inactive by #1784's required overflow analysis and has now been completed on
local branch `feature/remediation-coll-sortedset-version-overflow`. It was
opened as an assessment; the assessment established that a fully
source-, symbol-, and layout-compatible repair exists, so it was implemented in
the same ticket. The contract is recorded in
`docs/SortedSetVersioningDesign.md`, with a pointer from
`docs/SortedSetLiveViewDesign.md` section 32.

`SortedSet<T>::State::version` was a `SharpRuntime::intcs` — `int32_t` — that
started at 0, was only ever incremented, and was compared for **equality alone**
by both `Iterator::checkVersion` and the per-view Count cache. Four defects
follow, all reproduced against the real production header before anything
changed, with a single probe source built against both the committed pre-fix
header and the working tree and positioning the counter through GCC's
`-fno-access-control` rather than by performing billions of mutations
(`build-probe-sortedset/probe12_version_overflow.cpp`, logs
`probe12_prefix_*.log` / `probe12_postfix_*.log`, defects observed 4 → 0):

1. `++state_->version` at `INTCS_MAX` is **signed-integer overflow**, undefined
   behaviour in C++. UBSan: `SortedSet.hpp:425:20: runtime error: signed integer
   overflow: 2147483647 + 1 cannot be represented in type 'int'` inside
   `SortedSet<int>::Add`.
2. A counter wrapped 2^32 mutations on returns to a value an outstanding
   `Iterator` captured, and the guard **silently accepts the stale iterator** —
   which then enumerates a container that changed under it, since
   `std::set::insert` does not invalidate iterators.
3. The same wrap **silently revalidates a stale cached view `Count`**: the probe
   answered 4 where the range held 3. Deterministically constructible, because
   `Add`+`Remove` is two increments.
4. **Not in the ticket's description, and the most serious of the four:**
   `kCountNotCached` was `-1`, a value the counter itself reaches after 2^32 − 1
   effective mutations, so a view that had **never** computed its Count read its
   cache as warm and answered 0 where the range held 5. Unlike defects 2 and 3
   this needs no prior observation. The header's own claim that the counter
   "never legitimately holds this value" was false, not merely optimistic.

.NET's own `SortedSet<T>` carries defects 2, 3, and 4 as *defined-but-wrong*
behaviour: its counter is `int`, incremented unchecked, compared for equality
only, and `TreeSubSet` initialises `version = -1; _countVersion = -1;` — the
same sentinel with the same latent collision. Because the CLR defines signed
overflow as wrapping where C++ makes it undefined, matching .NET's integer width
would **not** have made the C++ code correct, and this port deliberately exceeds
the reference here.

Repair: the shared counter and the `Iterator` snapshot become
`SharpRuntime::ulongcs` (64-bit unsigned), so every increment is defined for
every representable prior value and a repeat needs 2^64 effective mutations —
over 580 years of uninterrupted mutation. The widening is free: the counter is
not a member of `SortedSet<T>`, and `Iterator` already carried four bytes of
tail padding. The Count cache's 32-bit tag **cannot** be widened — measured
member offsets show `sizeof(SortedSet<int>)` 40 and
`sizeof(SortedSet<std::string>)` 104 have no spare byte, and an exact count
needs 31 of the 64 bits available — so it is instead stored **biased by one and
compared widened**, which identifies a counter value exactly, cannot be produced
by a never-filled cache, and stops the cache being written once the counter
outgrows it, at which point a view's `Count` becomes an O(k) recomputation:
slower, never wrong. Six alternatives were evaluated with reasons, including
State renewal, which cannot preserve #1783's live-view graph because rebinding
only the owning set would split it. A first implementation used an explicit
horizon *branch* and measurably cost +1 ns on every `Count` call, including an
owning full set's; two variant headers isolated the branch as the sole cause and
the biased tag removed it.

Closure gates: 29 new permanent regressions in
`SortedSetVersionOverflowTests.cpp`, whose near-boundary cases reach the counter
through a portable test-only friend seam declared and befriended in the header
and defined only in that test file, never a production hook;
`SharpRuntimeTests_Collections_Core` **1,841/1,841** (was 1,812, with all 47
`SortedSetLiveViewTests`, all 29 `SortedSetCountCacheTests`, and all 41
pre-existing SortedSet cases passing and no assertion edited);
`scripts/local_ci_check.sh build` at **13,127 tests across 37 executables** (was
13,098) with zero warnings and zero errors, after which the 13,098 floor in
`README.md` and `CLAUDE.md` was raised; UBSan clean post-fix across all six
probe modes; ASan+UBSan+LeakSanitizer 105/105 over all three permanent SortedSet
suites with LSan verified active by a deliberate-leak self-test;
ThreadSanitizer clean across #1783's probe, #1784's ten-mode probe, and a new
six-mode probe covering the recompute-past-the-tag path, with both self-tests
still reporting races so the zeroes are evidence, and with no mode ever mutating
concurrently; `sizeof` 40/104/40, `alignof` 8, all four value-semantics traits,
and the mangled `GetViewBetween` symbol byte-identical to #1784's stored probe
output, plus a dedicated probe confirming **every member offset** of
`SortedSet<T>` and `Iterator` is unchanged; both consumer fixtures behaving as
before; boundaries 41 modules / 90 edges; validator tests 7/7; catalogue
current; database consistent; `git diff --check` clean; Doxygen **unchanged at
1,937** against the 1,942 ceiling; all ten selective components plus
`Collections.Core` in isolation. Performance is within run-to-run noise on every
benchmarked operation, with warm view `Count` slightly faster and no allocation
added anywhere.

Because no public signature, mangled symbol, or object layout changed, this
revision needs **no consumer rebuild on its own account** and required **no new
user approval**.

One further **inactive** ticket was opened and not begun, with no `SR-AUD-*`
identifier: **#1787** (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, M).
Fourteen other collections carry the identical `intcs version_` counter,
incremented without bound and compared for equality only, so defects 1 and 2
apply to all of them; defects 3 and 4 are specific to `SortedSet<T>`'s Count
cache. #1786's stored acceptance criteria asked for a repository-wide
implementation, and the instruction governing that working session scoped #1786
to `SortedSet<T>` and required the remainder to become a separate inactive
ticket; the divergence is recorded in the design document rather than silently
absorbed, and the full inventory the criteria asked for is delivered there.

Ticket #1785 remains `todo` and untouched — this ticket changed no exception
behaviour whatsoever — and ticket #1773 remains `blocked` and untouched.

## Post-remediation repository-wide mutation-counter sweep: ticket #1787 (2026-07-28)

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) is
**done**, closed on local branch
`feature/remediation-coll-version-counter-sweep`. It carries **no new
`SR-AUD-*` identifier** — the audit numbering stays frozen at 364 and this
pattern was found during remediation, by ticket #1786's own required inventory,
not during the audit. It reopens no finding: SR-AUD-361 stays `remediated`, and
it is **not** a member of CCF-004 for the reasons already appended to that cause
(the counter is a private implementation detail, not a public .NET-shaped
arithmetic boundary, and its real defect is snapshot reuse rather than a wrong
result). The full record is `docs/CollectionVersionCounterSweep.md`.

**It corrected #1786's inventory in three ways.** There are **sixteen**
counter-carrying types in `modules/collections/include/`, not fifteen: `BitArray`
was missed, and it is also the one type whose counter was already
`std::uint32_t` rather than `intcs` — which is exactly why it never had the
signed-overflow undefined behaviour, confirmed by its producing no UBSan report
where all fourteen others did. #1786's assertion that "all fifteen declare it
`intcs`" was wrong on both counts. And #1786's claim that its defects 3 and 4
(a stale cached `Count`, a colliding `-1` sentinel) are specific to
`SortedSet<T>` is now **confirmed with per-type evidence** rather than repeated.

**A third defect class was found that appears in neither ticket's description
and is the most serious of the three.** The implicitly declared copy/move
assignment operator transplanted the *source's* counter into the destination, so
an enumerator outstanding over the destination saw no change even though the
assignment had just destroyed every element it could refer to. It needs **no
overflow at all**: the two counters merely have to be equal, which two
collections that have taken the same number of effective mutations routinely
are. Six of the fourteen affected types reproduced as AddressSanitizer
`heap-use-after-free` or `heap-buffer-overflow` — `HashSet<T>`,
`Dictionary<K,V>`, `SortedDictionary<K,V>`, `OrderedDictionary<K,V>`,
`Hashtable`, and `ListDictionaryInternal` — rather than merely as wrong answers.
`LinkedList<T>` was immune, because ticket #1769 had already given it a bumping
`operator=`, and `SortedSet<T>` is immune because its `Iterator` co-owns the
shared `State` through a `shared_ptr`, so an assignment cannot free the state it
observes.

Pre-fix evidence, all taken against the *committed* headers before anything
changed (gitignored `build-probe-collversion/`, one probe source built against
both header revisions, counters positioned with `-fno-access-control` rather
than by performing billions of mutations): **14** UBSan signed-integer-overflow
reports, one per collection; **15** iterator/enumerator ABA reproductions at the
2^32 alias distance; **8** assignment-alias reproductions; **6** ASan memory
errors.

The repair replaces each bare integer field with the new
`System::Collections::detail::BasicMutationCounter`, whose increment is unsigned
— defined for every representable prior value — and whose **assignment advances
the destination instead of taking the source's value**, while copy construction
still inherits it, matching .NET's `ArrayList.Clone` and `Hashtable.Clone`.
Thirteen collections took the 64-bit `MutationCounter`; `LinkedList<T>` and
`BitArray` took the 32-bit `NarrowMutationCounter`, because widening them grows
a public object (`sizeof(LinkedList<int>)` 40 → 48,
`sizeof(BitArray::Enumerator)` 32 → 40) and both are exactly packed with zero
spare bytes at the counter's position, arithmetically unavoidable in any member
order. Both still lose the undefined behaviour and the assignment transplant,
and both keep a documented, test-pinned 2^32 snapshot-reuse residual.

Closure evidence: **336** permanent regressions in
`modules/collections/tests/System/Collections/CollectionVersionCounterTests.cpp`
reaching every counter through **one** test-only friend seam
(`SharpRuntime::Testing::CollectionVersionAccess<T>`, generalising #1786's
per-type seam and proven inaccessible to a consumer by a negative compile
fixture); no existing assertion edited and all 1,841 pre-existing
Collections.Core tests still passing; `SharpRuntimeTests_Collections_Core`
**2,177/2,177** (was 1,841); `scripts/local_ci_check.sh build` at **13,463 tests
across 37 executables** (was 13,127) with zero warnings and errors and no test
disabled or filtered; UBSan and ASan **0 diagnostics** post-fix on every probe
mode; ASan+UBSan+LSan **349/349** with LSan verified active twice over — it
caught a real 24-byte leak in this ticket's own first-draft test;
ThreadSanitizer **0 races** in three real modes with the deliberate-race
self-test still reporting 2, and **no concurrent-mutation safety claimed**;
every `sizeof`, `alignof`, and counter offset unchanged with **0 symbols removed
or renamed** and 10 new weak inline definitions for the new counter class; both
new consumer fixtures behaving as intended; 41 modules/90 edges; validator tests
7/7; catalogue current; database consistent; `git diff --check` clean; Doxygen
1.9.8 at **1,938**/1,942 — one warning more than the pre-ticket 1,937, diffed
line by line and attributable entirely to the single new `README.md` markdown
link into `docs/`, which `Doxyfile` does not scan, exactly as the six existing
such links already are; all ten selective components plus `Collections.Core` in
isolation; performance within run-to-run noise on every benchmarked path.

Because no public signature, mangled symbol, or object layout changed, this
revision needs **no consumer rebuild on its own account**. It does change
observable *behaviour* for one shape — enumerating a collection after that
collection was wholesale assigned now throws instead of reading destroyed
storage — which is recorded in `README.md`'s breaking-changes section.

Three tickets were opened and deliberately not begun, none with an `SR-AUD-*`
identifier: **#1788** (`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, S,
**blocked**) pending explicit approval that `sizeof(LinkedList<T>)` may grow
40 → 48 on LP64; **#1789** (`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, XS,
**blocked**) pending explicit approval that `sizeof(BitArray::Enumerator)` may
grow 32 → 40; and **#1790** (`REMED-COLL-LIST-INDEXER-VERSION`, P3, L, `todo`)
recording the separate, pre-existing, non-versioning divergence that
`List<T>::operator[]` returns a plain `T&` and so cannot bump the counter the way
.NET's index setter does. #1788 and #1789 are deliberately two tickets rather
than one: they share the symptom and nothing else, one grows a container and the
other a public enumerator, and a user might reasonably approve one and not the
other.

Ticket #1785 remains `todo` and untouched — this ticket changed no exception
behaviour whatsoever — and ticket #1773 remains `blocked` and untouched. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
compilation used more than four parallel jobs, and no push, merge, rebase, tag,
or publication occurred.


## Post-audit design batch — ticket #1790, `List<T>` indexer versioning (2026-07-28)

Design ticket #1790 (`REMED-COLL-LIST-INDEXER-VERSION`, P3, size L, category
`parity`) is **done as a design ticket**, on local branch
`feature/remediation-coll-list-indexer-design`. It carries **no `SR-AUD-*`
identifier and reopens none** — the audit numbering stays frozen at 364, the
findings index is unchanged, and every earlier finding keeps its status
(SR-AUD-361 stays `remediated`). It changed **no production behaviour, no public
signature, no object layout, and no exception**; the single production edit is a
doc-comment correction in `List.hpp`. The durable record is
`docs/ListIndexerVersioningDesign.md`.

The finding under investigation was recorded by ticket #1787 as Category D — a
divergence deliberately *not* absorbed into that ticket's counter-arithmetic
scope. Its answer is **no fully source-compatible correction exists**: a plain
`T&` returned by `operator[]` cannot be intercepted, because C++ provides no
mechanism by which a collection learns of a write through a reference it
previously handed out. Acceptance-criteria route (a) — declare the divergence
permanent — was **rejected**, because the same `T&` is a *reproduced
use-after-free* (four AddressSanitizer heap-use-after-free reports: across a
reallocating `Add()` on both read and write, across `Clear()`, and across move
assignment), not merely a fail-fast divergence. Route (b), a tracked proxy
return, is selected and handed to implementation ticket #1791.

**Three of the originating ticket's own premises were corrected rather than
inherited**, each against this record's convenience:

1. `List<T>::operator[]` is **not** the widest untracked route. The non-const
   `ToVector()` hands out the whole backing `std::vector<T>&`, permitting
   `push_back`/`erase`/`clear` — a **structural** mutation the fail-fast guard
   never sees, strictly wider than the indexer. Reproduced, and previously
   undocumented anywhere in the repository.
2. The claim that `operator[]` is "the single most call-site-heavy method in
   this repository" is **wrong for this repository**. Measured across **all 625
   translation units** by compiling against a `[[deprecated]]`-tagged shim, it
   has **61 call sites, all in two test files**, and **no library source
   includes `List.hpp` at all**. The CNA/mobile-eggbert burden remains real,
   unmeasured, and out of scope by instruction.
3. `IList<T>` has **four** implementers, not one — `List<T>`,
   `ObjectModel::Collection<T>`, `ObjectModel::ReadOnlyCollection<T>`, and a
   hand-written one in the test suite. Three were missed by grep (they spell it
   `public Generic::IList<T>`) and were found only by compiling the repository
   against the candidate header.

Evidence is repository-local and gitignored under `build-probe-listindexer/`: a
five-mode reproduction probe, four AddressSanitizer reports, 0 UBSan
diagnostics on every non-lifetime mode, a 24-case expression matrix, and four
generated header shims each compiled against the whole repository at
`-fsyntax-only`. Measured source break: the selected refined proxy breaks **1
site in 1 of 625 translation units**, against **8 sites in 3 units** for the
rejected value/setter alternative. Those figures are close and the decision is
explicitly **not** based on them — it is based on the value alternative deleting
`list[i] = v`, the exact spelling C# uses, from the API. Layout measured:
`sizeof(List<T>)` **40 → 40** unchanged, `sizeof(ObjectModel::Collection<T>)`
**32 → 40**.

Closure evidence: 14 new permanent regressions in
`ListIndexerVersionTests.cpp`, split into a `Contract` suite (must survive
#1791) and a `Divergence` suite whose `static_assert`s #1791 cannot land without
editing; `SharpRuntimeTests_Collections_Core` **2,191/2,191** (was 2,177) with
no existing assertion edited; `scripts/local_ci_check.sh build` at **13,477
tests across 37 executables** (was 13,463) with zero warnings and errors; 41
modules / 90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 unchanged at **1,938**/1,942.

**Two tickets were opened and deliberately not begun, neither with an
`SR-AUD-*` identifier.** **#1791**
(`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, L, **blocked**) carries the
implementation in two phases: Phase 1 (tracked `getItem`/`setItem`, a pure
addition) needs no approval but does not close the defect; Phase 2 is blocked
pending the exact four-part approval in design section 28 — public source breaks
to `List<T>::operator[]` and to the `IList<T>` interface (affecting every
implementer, including hand-written consumer ones), an object-layout change to
`ObjectModel::Collection<T>` requiring a full consumer rebuild, and
acknowledgement that CNA's and mobile-eggbert's usage is unmeasured. The
approvals granted for #1771, #1780, and #1783 do **not** carry over. The
unavoidable cost is recorded rather than buried: `list[i].member` and
`list[i].method()` stop compiling for value-type elements, because `operator.`
cannot be overloaded.

**#1792** (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, M, `todo`) records a
**newly discovered defect** found by #1790's inventory and deliberately not
absorbed into it: `Generic::IEnumerator<T>::getCurrentProperty()` does
`return const_cast<T*>(&Current());`, publishing a mutable `void*` to the live
element on a public interface, so a write through it mutates a collection
mid-enumeration with the counter at rest and the guard silent. It affects
**every** collection in the repository, not `List<T>`. **Corrected by #1792 itself
(2026-07-28): the "every collection" claim is wrong — see the next section.**

Tickets #1785 remains `todo` and untouched; #1788 and #1789 remain `blocked` and
untouched, and no counter was widened; #1773 remains `blocked` and untouched.
CNA and mobile-eggbert were not inspected, searched, configured, built, or
modified. No compilation used more than four parallel jobs, and no push, merge,
rebase, tag, or publication occurred.

## Post-audit design batch — ticket #1792, enumerator `Current` safety (2026-07-28)

Design ticket #1792 (`REMED-COLL-ENUMERATOR-CURRENT-CONSTCAST`, P3, size M,
category `defect`), opened inactive by #1790, is **done as a design ticket**,
closed on local branch `feature/remediation-coll-ienumerator-current-design`. It
carries **no `SR-AUD-*` identifier** — the numbering stays frozen at 364 — and it
**reopens no finding**. SR-AUD-356 stays `remediated`: this defect is about what
the accessor *returns*, not about the before-start/after-end lifecycle guard that
finding covers, and every #1767 regression still passes unmodified. The findings
index is unchanged. It **changed no production behaviour, no public signature, no
object layout, and no exception**, and edits no production source at all. The
durable record is `docs/IEnumeratorCurrentSafetyDesign.md`.

`System::Collections::IEnumerator::getCurrentProperty()` returns `void*` and the
generic bridge fills it with `const_cast<T*>(&Current())`, so a consumer holding
nothing but the public non-generic interface obtains a writable, untyped,
unbounded-lifetime pointer into the live storage of the collection it is walking.
The selected architecture is **`std::any` returned by value** — the direct C++
counterpart of .NET's `object IEnumerator.Current`, which returns a value, boxes
value types, and hands out no pointer. `Generic::IEnumerator<T>::Current()` stays
`const T&`.

**Four premises recorded in this file's #1790 section were corrected**, and the
corrections are against this record's own convenience.

1. The claim that the defect "affects **every** collection in the repository" is
   **wrong**. `Dictionary<K,V>`, `HashSet<T>`, `SortedSet<T>`, and
   `SortedDictionary<K,V>` implement no `IEnumerator` at all — they expose
   STL-style version-checked iterators. The measured reach is thirteen generic
   enumerator implementations plus eight non-generic ones, plus two hand-written
   test-local implementers. The paragraph above is left in place, with this
   correction attached, rather than rewritten.
2. The bridge's `const_cast` is not the only one. **Four** further `const_cast`s
   live outside it — `ArrayList::Enumerator`, `Hashtable`'s member-view
   enumerator, and `ListDictionaryInternal` twice — so repairing only the bridge
   would leave every one of them. One publishes a writable pointer to a **live
   `std::unordered_map` key**.
3. It is **six** distinct defect classes, not one: const-correctness,
   mutation/version bypass, type safety, lifetime, ownership ambiguity, and
   generic/non-generic inconsistency. They are closed by different measures, and
   `const void*` was *measured* to close only the first — a one-line `const_cast`
   restores the write.
4. The most dangerous property is the **ABI**, not the source break. `void*`,
   `const void*`, and `std::any` all produce the byte-identical mangled name,
   while `this` moves from `%rdi` to `%rsi` under `std::any`'s sret return, so a
   partially rebuilt consumer links with **no diagnostic** and then corrupts
   memory.

Evidence, all repository-local and gitignored under `build-probe-ienumerator/`,
produced against the committed pre-fix headers with nothing modified: four
AddressSanitizer `heap-use-after-free` reports (pointer retained across
reallocation, `Clear()`, the collection's destruction, and the enumerator's
destruction) plus two non-faulting stale-aliasing shapes across `MoveNext()` and
`Reset()`; a `ReadOnlyCollection<T>` whose non-const indexer throws
`NotSupportedException` and whose enumerator nonetheless mutated both the wrapper
and the caller's shared backing vector; a `Hashtable` entry made unreachable by
**both** its old and its new key while `Count` still reported it; 0 UBSan
diagnostics on every mode and 0 LSan leaks; a bounded, sanitizer-controlled
type-erasure probe; a five-candidate allocation and layout comparison;
calling-convention disassembly; a 626-translation-unit deprecation sweep
measuring 28 non-generic, 4 bridge, and 27 typed call sites with 0 compile
failures; and three fully migrated header shims breaking 6, 7, and 6 translation
units at 12, 14, and 12 sites, with **zero library sources broken under any of
them**, so the design's proposed bodies are compile-validated rather than
sketched.

Closure evidence: **17 new permanent regressions** in
`modules/collections/tests/System/Collections/EnumeratorCurrentSafetyTests.cpp`,
split into a `Contract` suite (8 cases that must survive the implementation) and
a `Divergence` suite (9 cases carrying `static_assert`s the implementation cannot
land without editing); `SharpRuntimeTests_Collections_Core` **2,208/2,208** (was
2,191) with no existing assertion edited; `scripts/local_ci_check.sh build` at
**13,494 tests across 37 executables** (was 13,477), zero warnings and errors; 41
modules / 90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; all ten selective components and every consumer fixture
passing; Doxygen 1.9.8 unchanged at **1,938**/1,942.

**One ticket was opened and deliberately not begun** — **#1793**
(`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`, P2, L), which is now
**done**; see the batch below. As opened by #1792 it was **blocked**, in two
phases: Phase 1 (write the ownership, lifetime, and validity rules into both
headers) needs **no** approval and does not close the defect; Phase 2 needs the
exact three-part approval in design section 33 — public source breaks to
`System::Collections::IEnumerator` and to `Generic::IEnumerator<T>`, and
acknowledgement of the silent ABI break requiring a full consumer rebuild. There
is **no** object-layout change, so this approval is narrower than #1788's,
#1789's, or #1791 Phase 2's in that respect and wider in the ABI one. The
approvals granted for #1771, #1780, and #1783 do **not** carry over. #1793 should
land **before** #1791, and the two must not be merged: they are independent
defects on disjoint surfaces and neither repairs the other.

Two residual limitations are recorded rather than buried: the typed `Current()`
reference hazard is **not** closed, and
`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` keep returning
`const void*` into live storage — a separate follow-on rather than a widening of
this approval.

Ticket #1785 remains `todo` and untouched; #1788, #1789, and #1791 remain
`blocked` and untouched; #1773 remains `blocked` and untouched. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
compilation used more than four parallel jobs, and no push, merge, rebase, tag,
or publication occurred.

## Post-audit remediation batch — ticket #1793, safe enumerator `Current` contract (2026-07-28)

Implementation ticket #1793 (`REMED-COLL-IENUMERATOR-CURRENT-SAFETY-IMPLEMENT`,
P2, size L, category `defect`), opened blocked by design ticket #1792, is
**done** on local branch
`feature/remediation-coll-ienumerator-current-safety`. It carries **no new
`SR-AUD-*` identifier** — the numbering stays frozen at 364 — and it **reopens
no finding**. **SR-AUD-356 stays `remediated`**, unchanged: that finding is
about enumerators dereferencing invalid `Current` *states*, which ticket #1767
closed and which every one of its regressions still proves; #1793 is about what
a *valid* `Current` hands back, which is a different defect found during
remediation rather than during the audit.

The user granted design section 33's three-part approval explicitly and scoped
to this ticket — a public source break to `System::Collections::IEnumerator`, a
public source break to `Generic::IEnumerator<T>` adding a
`NotSupportedException` path for element types that cannot be copied, and
acknowledgement of a **silent ABI break** requiring a full consumer rebuild. It
does not carry to #1791, #1788, or #1789.

**What changed.** `getCurrentProperty()` returns an owning `std::any` **by
value** instead of a mutable `void*`, the direct C++ counterpart of .NET's
`object IEnumerator.Current`. `Generic::IEnumerator<T>::Current()` is unchanged
at `const T&`. All **four** `const_cast`s outside the bridge are gone and both
`mutable` members are ordinary members again. Eight production non-generic
overrides, the one bridge covering thirteen production generic implementations,
two hand-written test-local implementers, and the three in-library call sites
migrated. **Zero library sources broke**, exactly as the design's shim sweep
predicted.

**The defect was reconfirmed before any production edit**, with output preserved
under `build-probe-ienumerator/prefix1793/`: 15 defects across six modes, four
ASan `heap-use-after-free` reports, a `ReadOnlyCollection<T>` mutated through
its own enumerator into the caller's shared backing vector, a `Hashtable` entry
made unreachable by **both** its old and its new key while `Count` still
reported it, and a same-width wrong cast silently wrong with no diagnostic from
any sanitizer.

**Four corrections to the design's section 14 sketch**, recorded in section
34.3, two of them caught only by running the new suite: the `if constexpr`
else-branch had to call `Current()` and discard it, or a move-only `T` would
have reported `NotSupportedException` where the pre-#1793 bridge reported
`InvalidOperationException`, silently converting an existing exception path;
`Generic::List<std::any>` cannot be instantiated at all, because `std::any` is
not equality-comparable and `List<T>`'s `Contains`/`IndexOf` need `operator==`;
`std::any(Current())` for `T = std::any` selects `std::any`'s copy constructor,
so the box is never nested; and the non-generic `Stack`/`Queue` `ICollection`
constructors gained a `std::bad_any_cast` path where the old code silently
stored a pointer into the source's live storage.

Closure evidence: the nine `EnumeratorCurrentDivergence` cases were **flipped,
not deleted** — renamed `EnumeratorCurrentSafety`, each asserting the opposite
outcome on the same collection through the same accessor, with the
`static_assert`s now pinning `std::any` so a revert cannot land silently either;
twenty-one further cases added; `SharpRuntimeTests_Collections_Core`
**2,229/2,229** (was 2,208); a **clean full rebuild** in a dedicated
`build-abi-1793` tree at **13,515 tests across 37 executables** (was 13,494),
zero warnings and zero errors; ASan+UBSan clean on all six migrated lifetime
shapes, four of which were `heap-use-after-free` before; **0 LSan leaks**, with
LeakSanitizer proved active by a self-test that leaks 289 bytes and exits
non-zero; **TSan deliberately not run**, because this change adds no atomic, no
`mutable` cache, and no hidden `const` write and in fact removes two `mutable`
members; object layout `diff`-identical against the stored baseline; the mangled
name `_ZNK…18getCurrentPropertyEv` byte-identical and the vtable slot unchanged
at offset `0x20`, confirmed on the real repository objects; a **stale-object
probe in which an old caller and a new implementation linked with zero
diagnostics** and the mismatched call then took a SEGV with UBSan reporting an
invalid vptr; a positive consumer fixture compiling under
`-Wall -Wextra -Wpedantic -Werror` and exiting 0; a negative fixture rejected at
all **6** marked sites; 41 modules / 90 edges; validator tests 7/7; catalogue
current; database consistent; `git diff --check` clean; Doxygen 1.9.8 at
**1,939**/1,942 — one above the canonical 1,938, because `Doxyfile`'s `INPUT`
does not cover `docs/` and every `README.md` link into it resolves as an
unresolved `\ref`; this ticket's Breaking-changes entry adds exactly one such
link, as its two neighbours already do (design record section 34.8).

Allocation was measured rather than assumed: 0 for `int`, a raw pointer, and an
already boxed `int`; **1** for a small SSO `std::string` and a
`std::shared_ptr`; 2 for a 64-char `std::string` and a `DictionaryEntry`. That
middle row corrects design section 22's prediction of 0 for any type at most one
pointer wide — libstdc++'s `std::any` small-buffer optimisation admits only
types that *fit in* a `void*`. A non-trivial element costs exactly 1 copy and 1
destroy per read, live count balanced at 0.

Three residual limitations stand, unchanged from the design's risk register and
stated rather than buried. **The typed `Current()` reference hazard is not
closed** — `&Current()` retained across a mutation is still a reproduced
use-after-free; its validity window is now written into the header for the first
time, together with the explicit statement that #1793 did not close it.
**`IDictionaryEnumerator::getKeyProperty()`/`getValueProperty()` keep returning
`const void*` into live storage**; they are const-correct, so no write path
exists through them, but the type-safety, lifetime, and ownership-ambiguity
classes remain open there. A warning now sits on that interface pointing at the
design record, and it is opened as ticket **#1794**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3, size M), **blocked** and
deliberately not begun: a second public source break plus a second silent ABI
break, needing its own two-part approval that #1793's does not supply.
**CNA's and mobile-eggbert's usage remains unmeasured.**

Ticket #1785 remains `todo` and untouched; #1788, #1789, and #1791 remain
`blocked` and untouched; #1773 remains `blocked` and untouched. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
compilation used more than four parallel jobs, and no push, merge, rebase, tag,
or publication occurred.

---

## Post-remediation parity correction — ticket #1785, SortedSet nested-view exception ordering (2026-07-28)

Ticket **#1785** (`REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER`, P3, size XS,
category `design`) is **done**, closed 2026-07-28 on local branch
`feature/remediation-coll-sortedset-nested-order`. It carries **no `SR-AUD-*`
identifier** — the numbering stays frozen at 364, and this was found during
remediation rather than during the audit — and it does **not reopen
SR-AUD-361**, which stays `remediated` from ticket #1783. It is the fourth
post-remediation follow-up inside that finding's surface, after #1784 (Count
cache race), #1786 (mutation counter), and #1787 (repository-wide counter
sweep). The user explicitly approved acceptance branch **(b)** of the ticket's
stored criteria — adopt .NET's ordering — scoped to #1785 alone.

**The question the ticket existed to settle.** Design §30.4 of
`docs/SortedSetLiveViewDesign.md` recorded, honestly and at implementation time,
that §15's claim of matching `SortedSet.cs:1510` / `TreeSubSet.cs:344` was
false: .NET checks nested **widening first** and only then delegates to the
ordinary invalid-range check, whereas #1783 shipped the reverse under §15's rule
that an argument pair should be validated for mutual consistency before being
validated against object state. Both orders throw; neither loses data or
corrupts state; the difference is visible only in the exception *type* and
*parameter* of a nested call that is simultaneously widening and inverted. That
made it a semantic decision, not a defect — hence a `design` ticket at P3.

**The .NET control flow, re-read from source rather than taken from the earlier
report.** `SortedSet.TreeSubSet.cs:342-353` throws
`ArgumentOutOfRangeException(nameof(lowerValue))` when
`_lBoundActive && Comparer.Compare(_min, lowerValue) > 0`, then
`ArgumentOutOfRangeException(nameof(upperValue))` when
`_uBoundActive && Comparer.Compare(_max, upperValue) < 0`, and only then returns
`(TreeSubSet)_underlying.GetViewBetween(lowerValue, upperValue)`.
`SortedSet.cs:1508-1515` is where the
`SR.SortedSet_LowerValueGreaterThanUpperValue` `ArgumentException` lives. Three
consequences: the widening tests are in the caller and therefore unconditionally
first; the lower bound precedes the upper; and `_underlying` is the **root** set,
which is why a nested view flattens to depth 1 instead of chaining.

**What shipped.** One `if` moved inside one existing inline body. The port now
checks lower widening, then upper widening, then `cmp(upper, lower)`. An owning
full set activates neither bound (`std::optional` standing in for
`_lBoundActive`/`_uBoundActive`), so it still reaches only the base check and its
behaviour is bit-for-bit unchanged. Ordering still routes exclusively through
`state_->data.key_comp()`; no `operator>`, `operator<=`, `operator>=`, or
natural-order comparison was introduced.

**Measured both ways.** `build-probe-sortedset/probe18_nested_exception_order.cpp`
printed outcome, exception type, parameter name, exact message, and a
state-and-version-unchanged check for the whole matrix, before the edit
(`probe18_prefix.log`) and after it (`probe18_postfix.log`). **Exactly 7 of 32
outcome rows changed**, all doubly-invalid nested calls; every success, every
widening-only failure, every inverted-only failure, and every top-level call is
byte-identical. Widening both ends *while* inverted is arithmetically
unreachable — a view's bounds satisfy `!cmp(*upper_, *lower_)`, so widening both
gives `lower < *lower_ <= *upper_ < upper` — and that is proved by an exhaustive
grid rather than asserted in prose.

**Compatibility, verified.** No public signature, return type, `const`
qualification, `[[nodiscard]]`, mangled symbol, vtable (the type has no virtual
members), `sizeof`, `alignof`, or member offset changed. Ownership, live
write-through, bounds inclusivity, nested flattening, the Count cache and its
release/acquire publication protocol, the mutation counter, iterator
invalidation, the thread-safety contract, and the O(1)-in-element-copies
allocation behaviour are all untouched; a rejected call still allocates nothing
and bumps no version. Every in-repository `GetViewBetween` caller was reviewed —
six test files under `modules/collections/tests/System/Collections/Generic/` and
both `test/consumer/collections_sorted_set_view*.cpp` fixtures, with no
production `src/` caller anywhere — and **none** asserted a doubly-invalid
nested call, so none relied on the old precedence.

Closure evidence: `SortedSetNestedViewOrderTests.cpp` with **23** permanent
cases, including an exhaustive `(lower, upper)` grid over `[-2, 12]²` compared
against .NET's decision procedure transcribed independently as an oracle, exact
message and HResult pins for all three exception shapes, a descending custom
comparer, an `operator<`-only element type, `std::string`, nesting to depth
three, and the no-op guarantees after 1,500 consecutive failed constructions;
`SharpRuntimeTests_Collections_Core` **2,252/2,252** (was 2,229);
`scripts/local_ci_check.sh build` at **13,538 tests across 37 executables** (was
13,515), zero warnings and zero errors; ASan+UBSan+LSan over four SortedSet
suites at **128 tests, 0 diagnostics, 0 leaks**, with LeakSanitizer proved
active by a deliberate-leak self-test reporting 4,112 leaked bytes; TSan
deliberately **not** run, because this ticket adds no shared mutable state, no
`const` write, and no new field, leaving #1784's TSan campaign as the governing
evidence; the SortedSet consumer fixture extended with a nested-precedence case
and compiling under `-Wall -Wextra -Wpedantic -Werror`, exiting 0; 41 modules /
90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 **unchanged at 1,939**/1,942; all ten
selective components plus `Collections.Core collections_sorted_set_view.cpp` in
isolation. The disposable `build-abi-1793` tree was removed after confirming it
is repository-local, gitignored, holds only build output, and that its results
are already durably recorded — **1.46 GiB reclaimed**, with both of its evidence
logs kept.

Tickets #1788, #1789, #1791, and #1794 remain `blocked` and untouched; #1773
remains `blocked` and untouched. CNA and mobile-eggbert were not inspected,
searched, configured, built, or modified. No compilation used more than four
parallel jobs, and no push, merge, rebase, tag, or publication occurred.

## Post-audit design batch — ticket #1795, `IDictionaryEnumerator` key/value safety (2026-07-28)

Design ticket **#1795** (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`, P3,
size M, category `design`) is **done**, on local branch
`feature/remediation-coll-idictenumerator-keyvalue-design`, with **no production
or test-source change**. Durable record:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`.

**No new `SR-AUD-*` identifier.** The audit numbering stays frozen at 364;
**SR-AUD-356 stays `remediated`** and **CCF-018 is not reopened**. This was found
during remediation, not during the audit, and the design closes nothing yet.

**Ticket #1794 was deliberately not reused.** Its database row is an
*implementation* row — it migrates the accessors and is blocked on approval to
*perform* that migration, not on a decision about what the migration is.
Recording it as a completed design ticket to reuse the number would have logged
implementation work as done when none was performed. #1794 stays `blocked`;
#1795 was opened as the next available number and completed as the design.

Selected: **Entry-canonical owning accessors with a mandatory `MoveNext`-time
snapshot.** `getEntryProperty()` stays `DictionaryEntry` by value and becomes the
canonical representation; `getKeyProperty()` and `getValueProperty()` return an
owning `std::any` **by value**, equal by construction to that entry's members;
`getCurrentProperty()` keeps #1793's signature and boxes the `DictionaryEntry` on
**both** implementations, matching .NET's `public object Current => Entry;`; and
every implementation must be able to answer every accessor from state the
enumerator itself owns.

**The return-type change alone is not the fix**, which is the design's principal
correction to the shape #1794 assumed. Neither accessor performs a fail-fast
version check, so both dereference a container iterator that a mutation may have
invalidated. On `ListDictionaryInternal` — which caches nothing — that makes even
`getEntryProperty()` and the already-migrated `getCurrentProperty()`
AddressSanitizer `heap-use-after-free` after `Clear()` or destruction. .NET's own
`HashtableEnumerator` snapshots `_currentKey`/`_currentValue` at `MoveNext` and
never reads `_buckets` from an accessor; the design adopts exactly that, and
`Hashtable::Enumerator` already half-implements it.

**Three premises written into ticket #1794's own description are contradicted by
measurement** and are corrected in the design record rather than worked around:

1. *"They are ALREADY const-correct … there is no write path through them."*
   **False for `Hashtable`.** `getValueProperty()` returns a pointer to the live
   `std::unordered_map`'s `mapped_type`, a **non-`const` `std::any`**;
   `const_cast` + assignment through it is well-formed, fully defined C++ that
   rewrites live dictionary storage, leaves the mutation counter unmoved, and is
   invisible to a second enumerator. `getKeyProperty()` reaches the `const
   std::string` key, where the write is undefined behaviour and, at 64 entries,
   produces an entry that `Count` still reports but that **no lookup can return
   by either its old or its new key**. Both classes A and B therefore apply to
   `Hashtable`; both genuinely do *not* apply to `ListDictionaryInternal`, whose
   accessors hand back the caller's own pointers and whose keys are compared by
   address.
2. *"The obvious selected shape is the same one #1793 landed."* Half right —
   `std::any` by value is selected, but it is necessary and not sufficient.
3. *"`getEntryProperty()` already returns `DictionaryEntry` BY VALUE, so the
   by-value answer is already the convention on this very interface."* True of
   the signature and misleading about safety: on `ListDictionaryInternal` that
   by-value entry is built from a dangling `std::list` iterator.

**Two previously unrecorded `ListDictionaryInternal` parity defects** were found
while measuring and are decided in the design, because the ticket must settle the
`Entry`/`Current` relationship: its `getCurrentProperty()` boxes the **key**
where .NET is `Current => Entry`, and it disagrees with itself about `const` on a
value (`DictionaryEntry::Value` is `void*`, the value view's `Current` is
`const void*`, `MemberCollection::copyToCore` writes `void*`). Both are named
explicitly in the approval, and item 2 of the approval is separately declinable.

Measured rather than estimated:

- **Inventory:** exactly **two** production implementations, both private nested
  classes; two adapter enumerators; and — unlike #1792's finding for
  `IEnumerator` — **zero** hand-written test-local implementers anywhere in this
  repository.
- **Call sites:** a 628-translation-unit `[[deprecated]]`-tagged sweep at four
  parallel jobs with 0 compile failures — **10 unique sites** (3 `Entry`,
  5 `Key`, 3 `Value`), 3 library-internal, 7 in tests, 0 outside
  `Collections.Core`.
- **Source break:** a fully migrated three-header shim, compiling standalone
  `-Werror` clean, breaks **1 of 628** translation units at **1 line**.
- **Runtime break:** all 66 `SharpRuntimeTests_Collections_Core` objects
  recompiled against that shim and relinked — **2,250 of 2,252 pass**, the two
  failures being exactly the two parity defects above.
- **Pre-fix evidence:** 20 defects across six modes; **8 AddressSanitizer
  `heap-use-after-free` reports**; **0** UBSan diagnostics on the corruption
  itself; three fatal scenarios that complete *silently* under UBSan alone and
  print a plausible wrong answer; a stack-buffer-overflow from one function
  called through the interface against the *other* implementation; 0
  LeakSanitizer leaks with detection proved active by a 284-byte deliberate-leak
  self-test.
- **Post-fix evidence:** 42 assertions, 0 failures, 0 ASan/UBSan/LSan
  diagnostics.
- **ABI:** the mangled name is **byte-identical** for `const void*`, `std::any`
  and `const DictionaryEntry&`; the vtable slot is unchanged at `0x30`; `this`
  moves `%rdi` → `%rsi` with a hidden `sret` in `%rdi`. A stale caller and a new
  implementation **link with zero diagnostics**, then SEGV — UBSan reporting an
  invalid vptr and a bogus `System::InvalidOperationException` raised out of
  garbage memory.
- **A second, independent stale-object vector** that #1793 did not have:
  `ListDictionaryInternal::NodeEnumerator` grows **40 → 72** bytes while
  `GetEnumerator()` stays `inline` in the public header, so a stale consumer's
  own object file allocates the old size for the new object. Reproduced: links
  clean, then ASan `heap-use-after-free`. `NodeEnumerator` is a **private nested
  class**, so this is *not* a public object-layout change in the sense of
  #1788's, #1789's, or #1791 Phase 2's approvals.
- **Allocation:** `Hashtable` key 0 → **1** (SSO) / **2** (heap key), because
  libstdc++'s `std::any` small-buffer optimisation admits only types that fit in
  a `void*`; `Hashtable` `int` value 0 → **0**; **`ListDictionaryInternal` key
  and value 0 → 0**; `Entry` and `Current` unchanged.
- **Alternatives:** seven evaluated with a compatibility matrix. Alternative F
  (enumerator-owned copies behind an unchanged `const void*`) was **measured**,
  not dismissed — **0 of 628** translation units break and there is no
  calling-convention change — and is rejected as the selected design because it
  leaves type safety and implementation divergence entirely open and reintroduces
  the enumerator/collection desynchronisation #1793 removed. It is retained as
  the documented fallback if the approval is declined, and must never be
  described as a remediation.

Two **pre-existing** `Hashtable` write escapes outside this interface are
recorded in the design's risk register rather than absorbed: the non-`const`
`operator[](const std::string&)` and `getItem()`'s
`const_cast<std::any*>(&it->second)` return both bypass the mutation counter, and
both are already documented in `Hashtable.hpp` as narrow gaps.

Validation, all unchanged as expected for a design-only ticket: 41 modules /
90 edges; validator tests 7/7; catalogue current; database consistent;
`git diff --check` clean; Doxygen 1.9.8 at **1,939**/1,942;
`scripts/local_ci_check.sh build` **13,538 tests across 37 executables** with
zero warnings and zero errors, `SharpRuntimeTests_Collections_Core` at 2,252.
`scripts/check_selective_components.sh` was not run because no public header or
component metadata changed; it is required when #1794 Phase 2 lands. Every probe
is retained under the gitignored `build-probe-idictenum/`.

Tickets #1788, #1789, #1791, and #1794 remain `blocked` and untouched; #1773
remains `blocked` and untouched. CNA and mobile-eggbert were not inspected,
searched, configured, built, or modified. No compilation used more than four
parallel jobs, and no push, merge, rebase, tag, or publication occurred.


---

## Completed IDictionaryEnumerator key/value safety implementation: ticket #1794

Implementation ticket **#1794** (`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, P3,
size M, category `defect`) is **done**, on local branch
`feature/remediation-coll-idictenumerator-keyvalue-safety`, landing the
architecture #1795 selected. Durable record:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37.

**No new `SR-AUD-*` identifier.** The audit numbering stays frozen at 364.
**SR-AUD-356 and CCF-018 are recorded as remediated by this ticket** — this is
the closure of a post-remediation follow-up on an interface CCF-018 did not
originally cover, not a reopening; #1767's lifecycle contract is unchanged and
every one of its regressions still passes.

The user granted the design's §33 approval **in full**: the public source break,
both `ListDictionaryInternal` parity corrections, and the silent ABI break.
`getKeyProperty()` and `getValueProperty()` now return an **owning `std::any` by
value**, and — the half that actually closes the lifetime class — **both
implementations snapshot the entry into enumerator-owned storage during a
successful `MoveNext()`, so no accessor on either implementation dereferences a
container iterator.** The snapshot rule is written into the header as an
invariant of the *interface*.

**Four corrections and extensions to #1795's own record**, recorded in design
§37.1 rather than absorbed:

1. §8.2's prose said "eight" ASan `heap-use-after-free` reports where **its own
   table listed nine**; re-measurement before any source change confirmed
   **nine** of sixteen scenarios. Nine is the figure now used everywhere.
2. §24 never measured `MoveNext()`. The mandatory snapshot costs
   `ListDictionaryInternal::MoveNext()` **2.8 → 23.9 ns per position** (~8.5×),
   because it now builds a `DictionaryEntry` where it previously only advanced an
   iterator; `Hashtable::MoveNext()` is unchanged, having always snapshotted. A
   walk that never reads an accessor now pays for a snapshot it does not use, and
   that is not optimisable away without reintroducing the accessor-time container
   read the nine reports came from.
3. §12.3 predicted 2,250/2,252 against a shim; with both predicted assertions
   updated the real figure is **2,252/2,252**, and **2,316/2,316** with the new
   suite.
4. §22's ABI numbers were measured on a synthetic stand-in and were
   **re-measured on the real interface**, where every prediction held and
   `getValueProperty()`'s vtable slot `0x38` was confirmed alongside
   `getKeyProperty()`'s `0x30`.

Pre-fix evidence was reconfirmed **before** any production change and retained
under the gitignored `build-probe-1794/`: `defects=20` identical to §8.1,
including the `Hashtable` value write being well-formed defined C++ that rewrote
live storage with the counter unmoved, and the 64-entry key corruption leaving an
entry `Count` still reported and no lookup could return by either name; nine ASan
`heap-use-after-free`; silent wrong reads with `diagnostic-from-any-tool=0`; and
the cross-implementation stack-buffer-overflow.

Post-fix: 42 assertions on the real headers with **0 failures and 0
ASan/UBSan/LSan diagnostics and 0 leaks**; the new suite under ASan+UBSan+LSan at
78 tests clean, leak detection proved active by the 284-byte self-test; UBSan
alone at 0 runtime errors; the pre-fix `const void*` caller source **no longer
compiles**, and `const_cast` cannot turn a `std::any` into a pointer, so no
compatibility path is even expressible. **TSan was not run and the precondition
was verified rather than assumed**: no `mutable` member exists in either
enumerator, every accessor is `const`, and every write to `current_` is inside
the non-`const` `MoveNext()`/`Reset()`.

Both silent-ABI mechanisms were reproduced end to end on the real headers: an
old caller linked against a new implementation **links with zero diagnostics**
then SEGVs, with UBSan reporting an invalid vptr and a bogus
`System::InvalidOperationException` raised out of garbage; and
`NodeEnumerator` 40 → 72 behind an `inline` `GetEnumerator()` links clean then
ASan `heap-use-after-free`. `NodeEnumerator` is a **private nested** class, so
this is not a public layout change; exactly one line of §23's table moved.

Validation: **+64 permanent tests**; the three pinned assertions **updated, not
deleted**; positive consumer fixture clean under `-Wall -Wextra -Wpedantic
-Werror` and passing; negative fixture rejected at **every** marked site;
boundaries 41 modules / 90 edges; validator tests 7/7; catalogue current;
database consistent; `git diff --check` clean;
`scripts/check_selective_components.sh` run with a repository-local `TMPDIR`
because a public header changed; Doxygen 1.9.8 at **1,940**/1,942, the single new
warning identified as the unresolvable `\ref` for the new `README.md` link into
`docs/`. **The full gate ran from a dedicated clean `build-abi-1794` tree at
13,602 tests across 37 executables**, zero warnings/errors, with
`SharpRuntimeTests_Collections_Core` at **2,316**.

Left open and explicitly not claimed: `MoveNext()`/`Reset()` after the collection
is destroyed remain undefined; the two **pre-existing** `Hashtable` write escapes
outside this interface now have their own inactive ticket **#1796**
(`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, P3, `blocked`) instead of only a risk
note; and `ListDictionaryInternal`'s key view still boxes `const void*` while its
`copyToCore` normalises the key to `void*`, an asymmetry predating #1794 and
outside the approval.

Tickets #1788, #1789, #1791, and #1796 remain `blocked` and untouched; #1773
remains `blocked` and untouched. #1793 was not reopened. CNA and mobile-eggbert
were not inspected, searched, configured, built, or modified. No compilation used
more than four parallel jobs, and no push, merge, rebase, tag, or publication
occurred.


## Post-audit design batch — ticket #1797, `Hashtable` value-access safety (2026-07-28)

Design ticket **#1797** (`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, P3, size M,
`design`) is **done**. **No `SR-AUD-*` identifier**: the numbering is frozen at
364 and every defect below was found during remediation. No production or test
source changed. Durable record:
`docs/HashtableValueAccessSafetyDesign.md`.

**Ticket #1796 was deliberately not reused.** Its row is an *implementation*
row — it closes the escapes and is blocked on approval to perform that change,
not on a decision about what it is — so recording it as a completed design would
log implementation work as done when none was performed. #1796 **stays
`blocked`**, now depending on #1797, with its acceptance criteria and an exact
four-item approval rewritten from the design. Same #1795 → #1794 handling one
ticket earlier.

**Four premises written into ticket #1796's own description are corrected by
measurement**, each against the record's own convenience:

1. **There are four mutable/aliasing escape routes on `Hashtable`, not two.**
   #1796 names `operator[](const std::string&)` and `getItem()`. It misses
   `at()`, which returns a `const std::any&` **into live map storage** — a
   `const_cast` through it is well-formed, fully defined C++ that rewrote the
   stored value with the counter unmoved, and the reference is one of the nine
   ASan reports. It also misses `setItem`/`Add`'s non-`const` `void*` value
   parameter, an input-side type hole.
2. **Rehash does *not* dangle a retained alias.** `std::unordered_map` is
   node-based; the address of a stored value was **unchanged across 8,000
   insertions**. The hazard is `Remove`, `Clear`, copy assignment, move
   assignment and destruction: **nine AddressSanitizer `heap-use-after-free`
   reports across fourteen scenarios**, with **0** LSan leaks and detection
   proved active by a 317-byte deliberate-leak self-test. Claiming rehash dangles
   would have been wrong.
3. **The most severe defect is one #1796 never mentions.** `operator[]` on an
   *absent* key performs a **structural insert** — `std::unordered_map`'s own
   rule — without touching the counter, so a bare *read* changes `Count`.
   Measured at 4,008 entries, an outstanding enumerator then visited **2,045
   distinct keys**, reached only **6 of its 8** pre-mutation seed keys, threw
   nothing, and produced **no report from ASan, UBSan or LSan**. All sixteen
   reproduced defects are silent under UBSan alone (`0` runtime errors).
4. **The sibling implementation of the same interface has its own, previously
   unrecorded defects.** `ListDictionaryInternal::setItem`'s *replace* branch
   returns before `++version_` — .NET's does `version++` first, unconditionally —
   and both its `getItem` and `setItem` accept a **null key**, `setItem` storing
   it, where .NET throws and where this port's `Hashtable` has thrown since
   #1775. The two `IDictionary` implementations therefore disagree on null keys,
   which is an interface defect rather than a type-local omission — the same
   shape SR-AUD-363 had. Filed as **new inactive ticket #1798**
   (`REMED-COLL-LISTDICTINTERNAL-PARITY`, P3, `blocked`), not absorbed.

**Selected architecture: owning reads, tracked writes, and no public alias into
storage** — the shape #1793 and #1794 landed on this component's enumerator
accessors, completed across the value-access surface. `getItem` returns
`std::any` by value; `operator[]` returns a non-copyable
`Hashtable::ValueReference` proxy whose read conversion returns `std::any` **by
value** and whose assignment advances the counter on insert, replace **and equal
replace** (matching `Hashtable.Insert`, which calls `UpdateVersion()` on both
branches and never compares the old value); a new `const` `operator[]` returns by
value; `at()` returns by value and throws `KeyNotFoundException` instead of
`std::out_of_range`, which a `catch (System::Exception&)` could not see.

**Two proxy properties are load-bearing rather than stylistic, and both were
found by measurement, one during validation of the design itself:** `std::any`'s
template converting constructor `any(T&&)` is constrained only on
`is_copy_constructible_v`, so with a **copyable** proxy `std::any b = h[k];`
prefers that constructor over the proxy's own conversion operator, silently
boxing the *proxy* and throwing `std::bad_any_cast` **at run time with nothing
wrong at compile time**; and a conversion returning `const std::any&` makes
`const std::any& r = h[k];` trip GCC 14's `-Wdangling-reference`, a false
positive that is nevertheless a hard error because every module here compiles
with `-Werror`.

**Measured:** 629 translation units swept; **12 call sites**, every one in the
test suite, with `operator[]` at **0** sites and no library source calling any of
them; **3** further sites in `test/consumer/`, counted by hand because those
fixtures are outside `compile_commands.json`; Phase 2 breaks **3 units / 5
sites**, all in the test suite — **fewer** than migrating `getItem` alone (6
units), because the sibling implementer is migrated in the same change and the
`conflicting return type` errors never occur; `at()` → by value breaks **0**; the mangled name is
**byte-identical** and the vtable slot unchanged at **`0x38`** while `this` moves
`%rdi → %rsi` behind a hidden `sret`, reproduced end to end as a stale caller
that **links with zero diagnostics and then SEGVs** with 14 UBSan
misaligned-address diagnostics naming the caller's *key* pointer used as `this`;
`sizeof(Hashtable)` **unchanged at 72** and `sizeof(ListDictionaryInternal)` at
40, so this is **not** an object-layout break in #1788/#1789/#1791's sense;
reads cost **0 allocations for an `int` payload**, 1 for an SSO `std::string`, 2
for a large one, at 1.2 → 5.4/15.7/27.7 ns.

**The obvious tidy-up is rejected on evidence.** Migrating `setItem`/`Add`'s
raw-key value parameter to `const std::any&` makes `Add("literal", v)` store the
entry under the **stringified address of the literal** — the standard
`const char*` → `const void*` conversion beats the user-defined
`const char*` → `std::string` one — and it compiles clean under
`-Wall -Wextra -Wpedantic -Werror`. Recorded with its measurement so the
implementation ticket does not repeat it.

**Alternative A′ (`getItem` → `const std::any*`) is the documented fallback if
the approval is declined**, measured as **byte-identical machine code** to
today's `void*`: same symbol, same slot, `this` still in `%rdi`. It leaves the
alias-lifetime class **entirely open** — three of the nine ASan reports stay
reachable — and **must never be recorded as a remediation**. A shared generic
proxy with #1791 is **explicitly rejected** on four measured incompatibilities
(locator, copyability, read conversion, element type); recommended order is
**#1796 before #1791**, and the migrations must not be merged.

**The defect is NOT marked remediated.** Validation, all unchanged as expected
for a design-only ticket: 41 modules / 90 edges; validator tests 7/7; catalogue
current; database consistent; `git diff --check` clean; Doxygen 1.9.8 at
**1,940** of the 1,942 ceiling; `scripts/local_ci_check.sh build` at **13,602
tests across 37 executables**. `check_selective_components.sh` not run — no
public header or component metadata changed; required when #1796 Phase 2 lands.
Build directories: `build/` (reused, `--parallel 3`) and the **shared**
`build-probe/` (one compiler process per probe; `MAX_JOBS = 3` in the two Python
sweeps). **No compilation exceeded three jobs** — the ceiling was lowered from
four to three during this ticket at the user's instruction, and
`scripts/local_ci_check.sh` and `scripts/check_selective_components.sh` were
corrected from their hard-coded `--parallel 4` in the same change. The
per-ticket build-directory habit ended here too: `CLAUDE.md` rule 10 is now a
closed table of directory names, and nineteen stale one-shot directories
(421 MB) were deleted with the user's approval.

Tickets #1773, #1788, #1789, #1791 and #1796 remain `blocked` and untouched;
#1798 is newly opened `blocked` and deliberately not begun; #1790, #1792, #1793,
#1794 and #1795 remain `done`, and neither #1793 nor #1794 was reopened. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified. No
push, merge, rebase, tag, or publication occurred.

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

---

## Post-audit design batch — ticket #1799, `ListDictionaryInternal` setter and null-key semantics (2026-07-29)

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

## Post-audit remediation batch — ticket #1798, `ListDictionaryInternal` setter, null-key and key-representation defects closed (2026-07-29)

Implementation ticket #1798 (`REMED-COLL-LISTDICTINTERNAL-PARITY`, P3, size M,
`defect`) is **done**, landing design ticket #1799's record
(`docs/ListDictionaryInternalSetterDesign.md`, whose §37 is the implementation
record) under the **three** explicit per-action approvals §36 required plus the
§36.4 acknowledgement. **No new `SR-AUD-*` identifier**: the audit numbering is
frozen at 364 and all six defects were found during remediation. #1799 remains
`done` and was not reopened.

**Six defects on the second production implementer of `IDictionary`, each
reproduced again against the committed headers before a line was edited.**

`setItem`'s replace branch returned before `++version_` (version `3 → 3` while
the stored value changed), so **four** outstanding enumerator kinds walked to the
end after a replacement with **no diagnostic from the type system, from
AddressSanitizer, or from UndefinedBehaviorSanitizer** — the dictionary
enumerator, the key view, the value view, and the same reached through an
`IDictionary&` — and the **value view enumerated the post-mutation value**. An
equal-value replacement did not bump either. All five raw-key entry points
accepted `nullptr`, and `setItem` **stored** it: a null key could be found,
enumerated, copied out and removed like any other, and — unlike SR-AUD-363's
`Hashtable` case — it **aliased nothing**, because keys here are compared by raw
address and no valid object has the null address. The defect was therefore an
*interface* one: the two implementations of one interface disagreed on every
null-key row, so no polymorphic consumer could rely on either answer. And
`MemberCollection::copyToCore` boxed `const_cast<void*>(n.key)` where all four
other key surfaces box `const void*`: one view, two incompatible element types,
with `std::any_cast<const void*>` on a `CopyTo` slot throwing
`std::bad_any_cast` and a write through the writable pointer **the library, not
the caller, manufactured** for an object the caller had declared `const`
reproduced as an **AddressSanitizer SEGV on read-only storage**.

**The fix is structural, not conventional.** A private `ValidatedKey` throws
`ArgumentNullException("key")` on `nullptr`, and the single `findNode()` locator
accepts nothing else, so no method can reach `list_` without validating — the
whole reason the design rejected a `Hashtable`-style `toKey()` helper, whose
weakness was that a future entry point could simply forget to call it. A new
negative fixture **compiles that claim**: 6 of 6 sites rejected. `setItem` is one
upsert with the bump **after** the mutation, giving a **strong** exception
guarantee .NET's bump-first shape cannot offer, and the key view's `const_cast`
is deleted.

**Two deviations from .NET are deliberate and are now asserted as contract**: a
throwing duplicate `Add` and a `Remove` of an absent key do **not** bump. .NET
`ListDictionaryInternal` bumps first and unconditionally on both; copying that
would have manufactured two new false-positive `InvalidOperationException`s out
of calls that changed nothing and would have contradicted a currently *passing*
assertion. **.NET's own `Hashtable` does neither**, so "match .NET" was never a
specification here — .NET's two implementations disagree on three of ten version
rows. The rule taken is **advance on effective mutation**, which
`MutationCounter.hpp` already documented.

**Four of the design's own figures are corrected by measurement** (§37.1), each
against this record's convenience: §11's "0 existing assertions change" for the
null-key row was **one** — and #1796 had planted it deliberately as "a test to
flip", so it was **flipped, not deleted**; §22 **understated** the stale-object
hazard, which re-measurement shows is link-order dependent at **both** `-O0` and
`-O2`, with a stale object first on the link line making a correctly *rebuilt*
translation unit revert; §21.1's symbol count was 10 lines rather than 7, none of
them a `ListDictionaryInternal` symbol; and §24's "+0.2 ns" is **not resolvable
above noise**, while its load-bearing "0 allocations added" holds exactly.

**A fifth deviation, from §28, is stated so it is not mistaken for scope creep.**
§28 proposed **no** negative fixture and was right *about the representation
change*, which fails at run time and is pinned in the permanent suite and the
positive fixture. It did not consider the design's **other** compile-time claim —
unskippability — which is what a negative fixture can actually prove. **CI
coverage of that fixture, exactly:** its per-site checker lives under the
**gitignored** `build-probe/`, so the committed file is compiled by **no tracked
CI job**. That is pre-existing inactive ticket **#1801**, applies equally to the
three earlier negative fixtures, and is **neither widened nor closed** here. Both
*positive* fixtures are compiled `-Werror` **and run** by
`check_selective_components.sh Collections.Core`.

**Interaction with inactive ticket #1800, recorded and not fixed:** the new suite
adds a **third** `CollectionVersionAccess` specialisation, spelled
**token-for-token** as the two existing `SR1794_SEAM_BODY` ones. Identical
specialisations across translation units are well-formed; the IFNDR is the
**divergence** with `CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY`,
which is pre-existing and is **not introduced, not widened and not fixed** here.
**#1798 does not claim to close #1800.**

**No signature, return type, parameter type, vtable slot, calling convention or
object size changed** — re-measured on the real headers, not the shim: 53 of 53
mangled names byte-identical, the 19-entry vtable identical, `this` still in
`%rdi` with no `sret`, `sizeof` unchanged at 40 / 72 / 24 / 24, and
`ValidatedKey`/`findNode` emitting no symbol at `-O2`. **That is exactly what
makes the rebuild hazard dangerous**: every affected body is `inline` in a
header, so a stale object links with **zero diagnostics** and then **silently
keeps the defect** rather than crashing as #1794's and #1796's breaks did.
`-flto -Wodr` diagnoses nothing. `README.md` carries the entry.

**Validation:** a fresh `cmake --fresh` configuration and clean-first rebuild
(631 objects, **0** predating the configure, all 36 test executables relinked,
0 warnings, 0 errors), then **13,723 tests across 37 executables** from that
rebuilt tree — the floor rises from 13,657 — with `Collections_Core` at **2,437**
(was 2,371, **+66**). ASan + UBSan + LSan clean across the whole
`Collections.Core` suite and both consumer fixtures, LeakSanitizer **proved
active** by a 350-byte self-test, and the §8.3 SEGV now **unreachable**. The
design's own 33-assertion contract probe, re-pointed from the shim to the
production header: **33/33**. 41 modules / 90 edges, validator 7/7, catalogue
current, database consistent, `git diff --check` clean, Doxygen **1,940** of the
1,942 ceiling, full selective-component matrix passing. Every compilation used
at most **three** jobs.

**Still not claimed closed:** address-based key comparison; `MoveNext`/`Reset`
after the collection is destroyed; a view or enumerator outliving its dictionary;
the silent stale-object hazard; `ValidatedKey` being unskippable within the class
but not across the codebase; the cosmetic duplicate-`Add` message divergence; and
the blast radius in CNA and mobile-eggbert, **unmeasured by instruction**
(#1773 stays `blocked`; neither repository was inspected). `Hashtable` was **not**
modified — its absent-key `Remove` over-bump stays inactive ticket **#1802**, and
closing it is what would make the two implementations agree on all ten version
rows.

- `modules/collections/include/System/Collections/ListDictionaryInternal.hpp.audit.md`.

## Post-audit remediation batch — ticket #1802, `Hashtable::Remove`'s absent-key over-bump closed (2026-07-29)

Implementation ticket #1802 (`REMED-COLL-HASHTABLE-REMOVE-VERSION`, P3, size S,
`defect`) is **done**, under the explicit per-action user approval its row
required and which no earlier approval carried. Durable record:
`docs/HashtableValueAccessSafetyDesign.md` §35, a clearly separated follow-up
section; #1796 and #1799 remain `done` and neither is reopened.

**No new `SR-AUD-*` identifier**: the audit numbering is frozen at 364 and the
defect was found during remediation, by design ticket #1799's probe rather than
by the audit. **SR-AUD-363, CCF-018 and SR-AUD-356 are not reopened** and no
finding's status changed.

**The defect.** All three `System::Collections::Hashtable::Remove` overloads —
`const void*`, `const std::string&` and `const char*` — were
`_map.erase(key); ++version_;`, so the fail-fast mutation counter advanced
**whether or not the key was present**. Reproduced again against the committed
headers before a line was edited (`build-probe/1802_probe1_remove.cpp`, log
`build-probe/1802_prefix.log`): **24 defects over 43 checks**, and **0 over the
same 43** after the repair.

- Removing an absent key moved the counter `3 → 4`, so **four** outstanding
  enumerator kinds threw `InvalidOperationException` after an operation that
  changed nothing: the `IDictionaryEnumerator`, the key view, the value view, and
  the same reached through an `IDictionary&`.
- A full walk after one absent `Remove` yielded **0 of 3** entries; `Reset()`
  threw too; the same absent key removed five times moved the counter by **five**;
  at 20,000 entries the counter moved and the enumerator died for a `Remove` that
  removed nothing.
- `Count` and contents were correct on **every** measured row. This is a **false
  positive**, memory-safe and wrong — and it is the *opposite* direction of error
  from #1798's on the sibling implementation, which missed a mutation that really
  happened.

**.NET comparison, read from the current source.** `Hashtable.Remove` calls
`UpdateVersion()` at `Hashtable.cs:999`, **inside** the branch that matched a
bucket; the absent case falls out of the collision walk at `:1004` having touched
neither `_count` nor `_version`. .NET `ListDictionaryInternal.Remove` does
`version++` first and unconditionally (`:181`). .NET's own two `IDictionary`
implementations therefore disagree, which is why the rule had to be *chosen*
rather than copied; the rule taken is .NET `Hashtable`'s, **advance on effective
mutation**, which `detail/MutationCounter.hpp` already documented and
`docs/ListDictionaryInternalSetterDesign.md` §9.3 selected for the interface. It
was **not** derived from `ListDictionaryInternal`, whose bump-first shape #1799
deliberately rejected. **With #1798 and #1802 both closed, the port's two
`IDictionary` implementations agree on all ten version rows of that design's
§6.1.**

**Repair.** One new private `removeKey(const std::string&)` helper —
`if (_map.erase(key) != 0) ++version_;` — that all three overloads route through,
the same "decide once, structurally unskippable" shape `lookupCopy()` gives the
reads and `toKey()` gives the raw-key conversion.
`std::unordered_map::erase(const key_type&)` **already returns the number of
elements removed**, so the effective/no-op distinction costs **no second lookup,
no `Contains` pre-check, no second key conversion, no allocation and no lock** —
the deciding value was already being computed and discarded. The bump follows the
erase, so a throwing key conversion leaves contents and counter untouched: a
**strong** exception guarantee. `toKey()` is unchanged and remains the single
validating conversion site; the null-key contract from #1775, re-asserted by
#1796, is untouched and re-pinned including its message text.

**`Clear()` is a decided deviation and was deliberately not changed.** It still
bumps unconditionally, including on an already-empty table, where .NET
`Hashtable.Clear` early-returns at `:426`. .NET's guard is
`_count == 0 && _occupancy == 0`, and `_occupancy` — buckets whose collision bit
was ever set — has **no `std::unordered_map` analogue**, so `if (_map.empty())
return;` would skip the bump where .NET still bumps and trade one divergence for
a subtler one. It also errs in the memory-safe direction, and matches .NET
`ListDictionaryInternal.Clear` and the port's own sibling. Now an assertion on
both implementations, not a comment.

**Not an ABI break — measured, not asserted.** `sizeof(Hashtable)` **unchanged at
72**, `sizeof(Hashtable::ValueReference)` unchanged at 40, the **19-entry vtable
byte-identical** with `Remove` still at slot `0x70` and `Clear`/`Add`/`setItem`/
`getItem` at `0x68`/`0x60`/`0x40`/`0x38`, `this` still in `%rdi` with **no
`sret`**, the **undefined-symbol list identical**, and
`callClear`/`callAdd`/`callSetItem` byte-identical machine code. One file-local
libstdc++ optimiser clone disappears; one weak COMDAT `removeKey` appears;
nothing a stale caller could need is removed. **A full consumer rebuild is
nevertheless mandatory and silent if skipped** — every affected body is `inline`
in a header, so a stale object links with **zero diagnostics** and keeps the old
false positive, link-order dependently at `-O0` and per-translation-unit at
`-O2`, with `-flto -Wodr` diagnosing nothing. Unlike #1794's and #1796's breaks
the failure mode is silence, never a crash.

**Permanent coverage:** `HashtableRemoveVersioningTests.cpp`, **+67 tests**,
whose enumerator matrix is parameterised over four enumerator families
(`IDictionaryEnumerator`, the key-view and value-view `MemberEnumerator`
adaptations, and the dictionary enumerator through an `IDictionary&`) and whose
interface cases are parameterised over **both** non-generic implementations.
`CollectionVersionCounterTests.cpp`'s `HashtableAdapter` gains
`kHasNoOpMutation = true` with an absent-key `Remove` as its no-op mutation,
matching `ListDictionaryAdapter`. New `-Werror` `Collections.Core` consumer
fixture `test/consumer/collections_hashtable_remove.cpp`, compiled **and run**.

**Interaction with #1800 and #1801, recorded and not fixed.** The new suite adds
a **fourth** `CollectionVersionAccess` specialisation, spelled **token-for-token**
as the three existing `SR1794_SEAM_BODY` ones; identical specialisations across
translation units are well-formed, and the IFNDR is the **divergence** with
`CollectionVersionCounterTests.cpp`'s `SR1787_SEAM_BODY`. The count of *distinct*
bodies is still **two**. #1800 is not introduced, widened or fixed here. **No
negative fixture was added** — nothing in this ticket changes at compile time, so
one would have nothing to fail on — so **#1801 is untouched**, and the new
*positive* fixture, while `-Werror`-compiled and run, is invoked by
`scripts/check_selective_components.sh Collections.Core
collections_hashtable_remove.cpp`, which is **not** in that script's default
matrix and is **not** run by any tracked CI job.

**Validation:** a fresh `cmake --fresh` configuration and clean-first rebuild
(**632 objects, 0 predating the configure**, all 36 test executables relinked, 0
warnings, 0 errors), then **13,790 tests across 37 executables** from that
rebuilt tree — the floor rises from 13,723 — with `Collections_Core` at **2,504**
(was 2,437, **+67**). A later comment-only edit (two doc-comments changed from `§9.3` to `section 9.3` so the two headers stay pure ASCII) triggered one incremental build that recompiled **11 translation units and relinked 1 executable** — the **complete** dependent set, since exactly eleven `.d` files in the tree name `Hashtable.hpp` or `IDictionary.hpp` — leaving **0** objects predating the fresh configuration; the gate below ran from that tree. ASan + UBSan + LSan clean across the whole
`Collections.Core` suite, a focused scenario probe, and **both** consumer
fixtures, with LeakSanitizer **proved active** by a 350-byte self-test reported
as 383 bytes in 2 allocations. TSan not applicable: no atomic, no hidden `const`
write, no cache and no new concurrency claim; `Hashtable` remains not
thread-safe. 41 modules / 90 edges, validator 7/7, catalogue current, database
consistent, `git diff --check` clean, Doxygen **1,940** of the 1,942 ceiling,
full selective-component matrix passing. Allocation counts identical on every
`Remove` path; the single measured slowdown is on the throwing null-key path and
is shown by instruction-level disassembly and by a bare-throw control to be
code-layout, not added work. Every compilation used at most **three** jobs.

**Still not claimed closed:** the raw-key `void*` *value* parameter on
`setItem`/`Add`; accessor or enumerator use after the collection is destroyed; a
`ValueReference` outliving its table; `const std::any& r = table[key];` compiling
as a snapshot; #1800's seam divergence; #1801's untracked negative fixtures; the
deliberate `Clear()`-on-empty deviation, decided rather than closed; and the
blast radius in CNA and mobile-eggbert, **unmeasured by instruction** (#1773
stays `blocked`; neither repository was inspected).
`ListDictionaryInternal` was **not** modified.

- `modules/collections/include/System/Collections/Hashtable.hpp.audit.md`.

## Post-audit remediation batch — ticket #1800, the test-seam ODR violation closed (2026-07-29)

Ticket #1800 (`REMED-COLL-VERSION-SEAM-ODR`, P3, size S, `defect`) is **done**.
Durable record: `docs/CollectionVersionTestSeamDesign.md`. This closes the
finding #1796 **observed and recorded rather than fixed**, and that #1798 and
#1802 each carried forward without widening. #1787, #1794, #1796, #1798 and #1802
remain `done` and none is reopened; the disclosures in their closure sections
above are the historical record of a known defect carried honestly and are **not**
rewritten.

**No production source, signature, symbol, vtable or object layout changed.** Not
one file under any `modules/*/include` or `modules/*/src` was touched. No
consumer rebuild is required or implied.

**The defect.** `SharpRuntime::Testing::CollectionVersionAccess<TOwner>` is
declared by `System/Collections/detail/MutationCounter.hpp`, befriended by
fifteen collections and by `detail::BasicMutationCounter`, and never defined in
production — which is what makes
`test/consumer/collections_mutation_version_negative.cpp` fail to compile, as it
must. **Five** translation units of the single `SharpRuntimeTests_Collections_Core`
program each supplied their own definition, in two families: `SR1787_SEAM_BODY`
(`version` + `positionVersion`) and `SR1794_SEAM_BODY` (`version` alone). Two
definitions of one class with different member sets violate [basic.def.odr]/12
and are ill-formed with **no diagnostic required**.

**Three divergences, not the two the row named.** The inventory was taken by
preprocessing each unit with the build's own flags — so every macro is expanded
before anything is compared — and hashing token sequences
(`build-probe/1800_inventory.py`). Besides `<Hashtable>` and
`<ListDictionaryInternal>`, the **partial** specialisation
`<detail::BasicMutationCounter<V>>` diverged as well (`read` + `write` against
`read` alone), and it is the one both collection-level bodies delegate to. It had
been recorded nowhere.

**"Benign in practice" held only for the configuration in use, and that is
measured.** With the two real bodies and one marked one-token edit
(`build-probe/1800_odr_{a,b,main}.cpp`, real headers, real
`libsharp_runtime_core.a`):

| Optimisation | Link order | TU A reads | TU B reads | Linker said |
|---|---|---:|---:|---|
| `-O0` (what this repository builds) | A then B | **7** | 7 | nothing, exit 0 |
| `-O0` | B then A | **1007** | 1007 | nothing, exit 0 |
| `-O1`, `-O2` | either | 7 | 1007 | nothing, exit 0 |

Swapping two object files on the link line changed the answer received by a unit
that had itself spelled the correct body. At `-O1` and above each unit inlines
its own body and the two disagree **inside one process**, so a future Release
configuration would have turned a link-order coin flip into a wrong answer with
no source change. `ld`, `-flto -Wodr -Wlto-type-mismatch`, ASan with
`ASAN_OPTIONS=detect_odr_violation=2`, and UBSan each reported **nothing**:
GCC's `-Wodr` compares layout and data members, and neither body has a data
member. **Neither `-Wodr` nor a sanitizer is an ODR check**, and that is now a
measurement in this repository rather than a claim.

**The repair.** One authoritative header,
`modules/collections/tests/support/CollectionVersionSeam.hpp`, defines the
counter-level seam and all fifteen collections through one
`SHARP_RUNTIME_COLLECTION_VERSION_SEAM` macro that is `#undef`ed at the end so it
cannot leak. The five suites lost their local blocks and gained one relative
`#include`. A relative include was chosen over a CMake include directory
deliberately: the preprocessor resolves it against the including file, so the
header reaches the full build, the isolated `Collections.Core` selective build,
the sanitizer tree and any future fixture with one spelling and no target
property that could be forgotten in one of them. The **richer** body became
canonical, so no test capability was traded away — the three surviving token
hashes are the `SR1787` hashes unchanged.

**Two lines of defence, both proved.** A suite that includes the header and then
writes its own body is now a hard `error: redefinition`
(`build-probe/1800_redefinition_probe.cpp`). For the one case a compiler cannot
see — a suite that writes a body and does not include the header —
`scripts/check_version_seam_odr.py` fails the gate. It **discovers** seams rather
than hard-coding them (any class template declared and not defined inside
`namespace SharpRuntime::Testing` in a production header; two are found, this one
and #1786's `SortedSetVersionAccess`, which was never broken and is now pinned)
and enforces four rules: never defined in a production tree; exactly one defining
file per specialisation; token-identical definitions; one seam macro per file.
Rule 2 is **deliberately stricter than ISO C++** — two identical definitions in
two executables are legal, but the `CONFIGURE_DEPENDS` glob decides executable
membership by directory, so "two files, currently identical" is one file move
from the defect. Run against the committed **pre-fix** sources the checker
reports all six problems and exits 1; against an injected hypothetical future
suite it exits 1; against the repository it exits 0.
`test/check_version_seam_odr_test.py` carries **12** fixtures for the checker
itself, and `scripts/local_ci_check.sh` — the repository gate and the `full`
GitHub Actions job — runs both before configuring anything.

**Validation**, from `cmake --fresh` and a clean-first rebuild at three jobs
(336 s): **632 objects rebuilt, 0 predating the configure, all 37 test
executables relinked**, 0 warnings, 0 errors; **13,790 tests across 37
executables**, `Collections_Core` **2,504** — both unchanged, because this ticket
moved test code without adding or removing a case. Every seam COMDAT is
byte-identical across the five objects and each symbol has one address in the
executable. The post-fix link-order probe agrees at `-O0`, `-O1` and `-O2` in
both orders. ASan + UBSan + LSan across the whole `Collections.Core` suite:
2,504 passed, **zero diagnostics**. TSan **not applicable and not run**: the
change relocates test-only class definitions and introduces no thread, no shared
mutable state and no atomic; the only atomics in reach are `SortedSet`'s Count
cache, already covered by #1784 and #1786. Full ten-component selective matrix
passing, plus an explicit isolated `Collections.Core` selective build
(`scripts/check_selective_components.sh Collections.Core
collections_mutation_version.cpp`, 2,504 passed) which is **not** in the default
matrix and **not** run by any tracked CI job. 41 modules / 90 edges, validator
7/7, seam checker 12/12, catalogue current, database consistent,
`git diff --check` clean, Doxygen 1.9.8 at **1,940** of the 1,942 ceiling. Build
directories: `build/`, `build-asan/`, the shared `build-probe/` under a `1800_`
file prefix, and `build-tmp/` as `TMPDIR`. **No new build directory was created
and every compilation used at most three jobs.**

**The one cost, reported rather than smoothed over:** the authoritative header
includes all fifteen collection headers, because a full specialisation needs a
complete type. Four suites that previously included two now include fifteen —
**+0.38 to +0.42 s each, about +31 %** of their front-end time, **+1.6 s** total
against a 336 s clean-first rebuild. Splitting the header along the
generic/non-generic line would recover most of it and the checker would still
pass. It was not done: two headers means a maintainer decides which one to
include and which one a new collection belongs in, and that decision is exactly
what produced two bodies in the first place.

**Still not claimed closed:** **#1801 remains `blocked` and is explicitly not
closed** — it asks for a tracked, CI-run *per-site* checker for the six negative
consumer fixtures in `test/consumer/`, generalising
`build-probe/1796_check_negative.py`, which is still untracked; #1800's checker
compiles nothing, knows nothing about `// must fail:` markers, and shares none of
that infrastructure, so `test/consumer/*_negative.cpp` are still proved only by
"the fixture did not compile". No broad repository-wide ODR remediation was
performed and none is claimed: this is one seam family. No new `SR-AUD-*`
identifier — the numbering stays frozen at 364 and the defect was found during
remediation, by #1796.

Tickets #1773, #1788, #1789 and #1791 remain `blocked` and untouched; #1790,
#1792–#1799 and #1802 remain `done` and none was reopened. CNA and mobile-eggbert
were not inspected, searched, configured, built, or modified. No push, merge,
rebase, tag, or publication occurred.

- `scripts/local_ci_check.sh.audit.md`.

## Post-audit remediation batch — ticket #1801, negative consumer fixtures validated per site (2026-07-29)

Ticket #1801 (`REMED-TOOLING-NEGATIVE-FIXTURE-CI`, P3, size S, `tooling`, area
*Developer experience*) is **done**. Durable record:
`docs/NegativeConsumerFixtureValidation.md`. This closes the gap #1796 reported,
#1799 confirmed had no ticket of its own, and #1798, #1802 and #1800 each carried
forward without widening; the "still not claimed closed" disclosures in their
closure sections above are the historical record of a known gap carried honestly
and are **not** retro-edited. #1796, #1797, #1798, #1799, #1800 and #1802 remain
`done` and none is reopened.

This is **infrastructure only**. No production source, signature, symbol, object
layout, vtable, exception contract or collection semantic changed; nothing under
any `modules/*/include` or `modules/*/src` was touched, and
`test/consumer/CMakeLists.txt`, `test/consumer/InjectFixture.cmake` and every
module's CMake metadata are unchanged.

**What was wrong.** Seven committed `test/consumer/*_negative.cpp` files existed
to prove that spellings earlier tickets outlawed are rejected by the compiler,
and **no tracked job compiled any of them**. The per-site checking logic existed
for two of the seven (`build-probe/1796_check_negative.py`,
`build-probe/1798_check_negative.py`), under the gitignored `build-probe/`. The
only tracked mention of any of the seven anywhere in the build or CI surface —
`scripts/local_ci_check.sh`, `scripts/run_component_tests.sh`,
`scripts/check_selective_components.sh`, every `CMakeLists.txt`,
`.github/workflows/components.yml` — was a **docstring**.

**Four inventory corrections, all against the ticket's own row.**

1. **Seven fixtures, not six.** The row predates
   `test/consumer/collections_dictionary_setter_negative.cpp`, added by #1798.
2. **Three of the seven had no per-site checker at all**, not merely an untracked
   one: `collections_mutation_version_negative.cpp` (#1787),
   `collections_object_model_readonlydictionary_negative.cpp` (#1780) and
   `collections_sorted_set_view_negative.cpp` (#1783).
3. **`scripts/check_readonlydict_empty_negative.sh` has never existed in any
   commit.** The #1780 fixture named it as the thing that compiled it. Corrected
   in the fixture.
4. **36 marked sites, now 37.** `collections_mutation_version_negative.cpp`'s one
   marked statement pair was two independent claims sharing a local variable;
   splitting them made both independently compilable.

**The false pass was reproduced, not assumed** (§3 of the record). A copy of the
Hashtable fixture with its first marked site turned into the one spelling the
fixture's own header says still compiles still failed at **nine** other lines, so
a whole-file check reported PASS while one of eleven claims had become false. The
retained gitignored per-site checker caught it at 10 of 11, exit 1. All mutation
work happened in `build-probe/1801_gap/`; no tracked file was edited to produce
it.

**What was built.**

- `scripts/check_negative_consumer_fixtures.py` — the tracked checker. It
  compiles 44 translation units per run (7 all-sites-off baselines + 37 sites)
  with `-std=c++23 -Wall -Wextra -Wpedantic -Werror -fsyntax-only`, include
  directories **derived** from the repository's own CMake component metadata
  rather than duplicated, `LC_ALL=C` and `-fdiagnostics-color=never` for
  deterministic diagnostic wording, and a hard `MAXIMUM_JOBS = 3` that **refuses**
  a higher request instead of clamping it. `-Werror` is not weakened anywhere.
  No CPU-count detection appears in it.
- `test/check_negative_consumer_fixtures_test.py` — 37 cases in 2.1 s, each
  building a miniature repository on disk and compiling it for real, including the
  permanent regression proof against a real tracked fixture.
- The seven fixtures, migrated from `// must fail:` comments to numbered
  `#if SHARP_RUNTIME_NEGATIVE_SITE == N` guards with inline
  `// NEGATIVE(<id>): <fragment>` markers and `//     | <alternative>`
  continuations. Five conventions were compared in §4 of the record; the
  guard-plus-inline-marker form won because the tracked file is compiled **as-is**
  with a `-D`, so nothing is generated, nothing needs cleaning up, and every
  diagnostic names a real tracked line.

**The load-bearing invariant is the clean baseline.** With no site selected a
fixture must compile with **zero** diagnostics. Enabling a guard can only *add*
uses of the surrounding scaffolding, never remove one, so any diagnostic in a
single-site variant is caused by that site; and the checker additionally requires
every diagnostic located in the fixture — `error:` or `required from here` — to
fall inside the enabled guard, so a site that fails because unrelated source broke
is reported as a failure rather than counted as a pass. The same property makes
each fixture honest C++ that an IDE does not present as broken, which is why **no
CMake change was needed at all**, and it lets each guard's `#else` branch document
the migration in compilable form.

**Integration point:** `scripts/local_ci_check.sh`, immediately after #1800's
seam block and **before** `cmake -S . -B build`, which is also the `full` job of
`.github/workflows/components.yml`. Adding it to
`scripts/check_selective_components.sh` or a dedicated CTest target was considered
and rejected with reasons in §9 of the record. The three `forbidden_*.cpp`
component-isolation fixtures stay in the selective script, where the selective
configure step their claim depends on exists; they were already executed by
canonical validation and were never part of this gap.

**Temporary mutation campaign: 7/7.** For each fixture,
`build-probe/1801_mutation_campaign.py` makes one marked site legal against a
mirror root whose `cmake/`, `modules/` and `vendor/` are symlinks to the real tree,
and requires the checker to fail naming exactly that site. Every round reported
**exactly one** problem, which is what proves the 36 still-invalid siblings neither
mask the legal site nor report falsely. No tracked file was mutated and none was
committed.

**Validation**, from a fresh configure (`cmake --fresh`) and a `--clean-first`
rebuild at three jobs: **7 fixtures / 37 sites / 37 rejected**, 44 invocations,
peak 3 jobs, 12.5 s; checker fixtures **37/37**; **0 warnings, 0 errors** in 346 s
with **632** objects and none predating the configure marker; **13,790 tests
across 37 executables**; `Collections.Core` **2,504**; the full ten-component
selective matrix passed with its three forbidden fixtures still rejected; **41
modules / 90 edges**; boundary-validator fixtures 7/7; seam ODR OK with **12/12**
fixtures; component catalogue current; database consistent; Doxygen 1.9.8 at
**1,940** of the 1,942 ceiling; `git diff --check` clean.

**Sanitizers are not applicable and none was built.** The deliverable is a Python
checker plus compile-only fixture validation: no new runtime code, no new thread,
no new allocation, and ASan/UBSan/LSan/TSan cannot observe a compile-rejection
contract. Module CMake metadata is unchanged, so normal test compilation is
unaffected and the existing `Collections.Core` sanitizer coverage from #1796,
#1798 and #1800 stands without being re-measured.

**Build-resource accounting.** `build/` for the fresh configure, clean-first
rebuild and full gate; `build-probe/` for the gap reproduction, the mutation
campaign and the superseded-checker note, all under a `1801_` file prefix;
`build-consumer/` read only, for the retained #1796/#1798 logs; `build-tmp/` as
the repository-local `TMPDIR` and as the self-test's deterministic temporary root.
**No new build directory was created and no compilation exceeded three jobs**,
including inside the new checker, which refuses a higher request.
`scripts/check_selective_components.sh` still needs the repository-local `TMPDIR`
because it calls `mktemp -d`.

**Still not claimed closed:** every fragment was measured against
`g++ (Debian 14.2.0-19) 14.2.0` only — the Clang fallback alternatives and
`-ferror-limit=0` are reasoned, not measured, consistent with the repository's
Linux/GCC verified baseline. `-fsyntax-only` does not link, so a claim that can
only fail at link time cannot be expressed in this framework; none of the seven
makes such a claim. `SortedSetVersionAccess` has no consumer-side fixture proving
it is unreachable — a coverage asymmetry with nothing known to be wrong, recorded
as new inactive ticket **#1803**
(`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`, `blocked`, not begun) rather
than absorbed here. No repository-wide compiler-diagnostic normalisation was
performed and none is claimed. No new `SR-AUD-*` identifier — the numbering stays
frozen at 364 and the gap was found during remediation, by #1796.

Tickets #1773, #1788, #1789 and #1803 remain `blocked` and untouched; #1790 and
#1792–#1802 remain `done` and none was reopened. **Ticket #1791 is now `done`**
(2026-07-29) — see the section appended below. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified. No push, merge, rebase, tag,
or publication occurred.

- `scripts/local_ci_check.sh.audit.md`.

---

## Remediation batch: ticket #1791 — tracked `List<T>` indexer mutation

Ticket **#1791** (`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT`, P2, size L,
`defect`, area `Collections`) implemented the architecture design ticket #1790
selected, under the **exact four-part approval written verbatim in
`docs/ListIndexerVersioningDesign.md` §28**, granted by the user and scoped to
#1791 only. The implementation record is §§29-39 of that document.

**No new `SR-AUD-*` identifier was issued and no finding was reopened.** The
audit numbering stays frozen at **364**, and `AUDIT_FINDINGS_INDEX.md` is
unchanged. This defect was found during remediation by #1790, not by the audit,
and is recorded under #1787's Category D classification
(`docs/CollectionVersionCounterSweep.md` §17).

The non-const indexer of `IList<T>`, `List<T>`, `ObjectModel::Collection<T>` and
`ObjectModel::ReadOnlyCollection<T>` now returns
`System::Collections::detail::ElementReference<T>`, a 16-byte prvalue proxy that
reads as `const T&` and routes every write through the mutation counter;
`getItem`/`setItem` were added as pure virtuals on `IList<T>` and implemented by
all four implementers, including the hand-written one in the test suite. The
mutable `List<T>::ToVector()` was removed with no public replacement.

**Transparency items, none of which is closed by this ticket.**
`begin()`/`end()` still yield an untracked mutable `T&`, so the ticket claims the
last *ordinary* untracked write path is closed rather than the last one; a
*retained* proxy still aliases a slot across reallocation; and a **stale object
file links with no diagnostic, does not crash, reads correct values, and silently
loses mutation tracking**, measured at `-O0` and `-O2` in both link orders,
because `operator[]` keeps its mangled name while its return convention changes.
`Collection<T>::operator[]` still does not run the virtual `SetItem` hook, which
#1791 narrowed rather than closed.

Measured layout and ABI: `sizeof(List<T>)` **40 → 40**, `sizeof(Collection<T>)`
**32 → 40**, `sizeof(ReadOnlyCollection<T>)` **24 → 24**, `IList<T>` vtable
**14 → 16** slots, 4 symbols removed and 18 added. Source break: **1 site in 1 of
631 translation units**.

Validation from a fresh configure plus a clean-first rebuild at **three jobs**
(633 objects, 0 predating the marker, 37 of 38 executables relinked — the
exception being the `EXCLUDE_FROM_ALL` stale historical `build/SharpRuntimeTests`,
which is outside the gate and was deliberately not deleted — 0 warnings, 0
errors): `Collections.Core` **2,554** (was 2,504); full repository **13,840
across 37 executables** (was 13,790); negative consumer fixtures **8 / 51**, every
site rejected (was 7 / 37), plus 37/37 self-test; version-seam ODR **2 seams / 18
specialisations** (was 17) plus 12/12 self-test; module graph **41 / 90**
unchanged; Doxygen **1,940** of the 1,942 ceiling; the full ten-component
selective matrix and a new `Collections.Core` positive consumer fixture passed;
ASan/UBSan/LSan `Collections.Core` **2,554 with zero reports**, LSan proved active
by a bounded self-test; `git diff --check` clean; local CI gate passed. **TSan was
not run**, for the reason design §19 gave.

Tickets #1773, #1788, #1789 and #1803 remain `blocked` and untouched; #1790 and
#1792–#1802 remain `done` and none was reopened. **No shared List/Hashtable proxy
abstraction was introduced**, so #1797 §24's four measured incompatibilities
stand. CNA and mobile-eggbert were not inspected, searched, configured, built, or
modified, so every source-break figure here is *this repository only*. No push,
merge, rebase, tag, or publication occurred.

### Post-remediation follow-up: ticket #1788 — LinkedList mutation-counter widening

Ticket **#1788** (`REMED-COLL-LINKEDLIST-VERSION-WIDEN`, P3, size S, `defect`,
area `Collections`) closed the first of the two approval-blocked residuals ticket
#1787 recorded. **No new `SR-AUD-*` identifier** — the numbering stays frozen at
364, and **no audit finding is reopened**: SR-AUD-356 and SR-AUD-357 stay
`remediated` and every #1767 and #1769 regression passes unmodified.

`LinkedList<T>`'s mutation counter and its `Enumerator`'s snapshot both moved
from 32 to 64 bits, under the explicit user approval that
`sizeof(LinkedList<T>)` may grow **40 → 48** on LP64 with a consequent full
consumer rebuild. Both together, deliberately: widening the container alone would
have made the guard's comparison a silent truncation. The 2^32 revalidation was
reproduced against the committed pre-fix header first —
`build-probe/1788_prefix_defects.log`, `guard-fired=0` for `LinkedList<int>`,
`LinkedList<std::string>` and `Reset()`, `defects-observed=3` — and the identical
source post-fix reads `defects-observed=0`. It mattered because `Enumerator`
holds a raw `data_t*` into `shared_ptr`-owned node storage, making a false
positive a potential use-after-free; at ~10^8 mutations/second the horizon was
about 43 seconds.

Measured: `sizeof(LinkedList<T>)` 40 → **48**, `alignof` unchanged,
`head_`/`tail_`/`count_` offsets unchanged, `sizeof(Enumerator)` **unchanged at
40** (the wider snapshot landed in existing padding), node handle and both STL
iterators unchanged at 16, and **0 `LinkedList` symbols added, removed or
renamed**. No public signature changed; every in-repository caller compiles
unmodified. The break is therefore **binary-only and silent**: a stale object file
links with **no diagnostic in either link order** and then either takes an ASan
heap-buffer-overflow and a SEGV, silently corrupts the member following an
embedded `LinkedList<T>` with **no sanitizer report at all**, or silently loses
mutation invalidation — while the other link order appears healthy.

Mutation-testing the required adapter flip also found, and corrected, a genuine
weakness in #1787's own pin: its narrow-branch assertion positioned the counter
at `snapshot` rather than `snapshot + 2^32`, which is true for a counter of any
width and therefore pinned nothing about the residual its comment described.

Validation from a fresh configure plus a clean-first rebuild at **three jobs**
(634 objects, 0 predating the marker, 37 of 38 executables relinked, 0 warnings,
0 errors): `Collections.Core` **2,594** (was 2,554); full repository **13,880
across 37 executables** (was 13,840); negative fixtures **8 / 51** all rejected
plus 37/37 self-test, none added; version-seam ODR **2 seams / 18
specialisations** plus 12/12 self-test, none added; module graph **41 / 90**
unchanged; Doxygen **1,941** of the 1,942 ceiling, the one new warning attributed
to the single new `README.md` link into `docs/`; the full ten-component selective
matrix and a new tracked `Collections.Core` consumer fixture passed;
ASan/UBSan/LSan `Collections.Core` **2,594 with zero reports**, LSan proved active
by a bounded self-test, and a 200,000-node boundary-positioned teardown clean;
`git diff --check` clean; local CI gate passed. **TSan was not run**: no atomic,
no `mutable` cache, no hidden `const` write, and no thread-safety claim is made
for `LinkedList<T>` before or after.

Tickets #1773, #1789 and #1803 remain `blocked` and untouched — **`BitArray`
keeps its 2^32 residual by design**, since closing it grows the *public*
`BitArray::Enumerator` and that is #1789's separate approval. #1790, #1791 and
#1792–#1802 remain `done` and none was reopened. CNA and mobile-eggbert were not
inspected, searched, configured, built, or modified, and no claim is made about
whether they use `LinkedList<T>`. No push, merge, rebase, tag, or publication
occurred. The full record is `docs/CollectionVersionCounterSweep.md` §19.

### Post-remediation follow-up: ticket #1789 — BitArray mutation-counter widening

Ticket **#1789** (`REMED-COLL-BITARRAY-VERSION-WIDEN`, P3, size XS, `defect`,
area `Collections`) closed the **second and last** of the two approval-blocked
residuals ticket #1787 recorded. **No new `SR-AUD-*` identifier** — the numbering
stays frozen at 364, and **no audit finding is reopened**: SR-AUD-364 stays
`remediated`, ticket #1767's enumerator lifecycle guard and ticket #1793's owning
`Current` are untouched, and every `BitArrayTests.cpp` case passes unmodified.

`BitArray`'s mutation counter and its **public** nested `Enumerator`'s snapshot
both moved from 32 to 64 bits, under the explicit user approval that
`sizeof(BitArray::Enumerator)` may grow **32 → 40** on LP64 with a consequent full
consumer rebuild. Both together, deliberately: widening the container alone would
have made the guard's comparison a silent truncation and left the 2^32 alias in
place while the code claimed otherwise. The 2^32 revalidation was reproduced
against the committed pre-fix header first —
`build-probe/1789_prefix_defects.log`, `guard-fired=0` for `MoveNext`, for
`Reset()`, and at seven laps of 2^32, `defects-observed=3` — and the identical
source post-fix reads `defects-observed=0`. Unlike `LinkedList<T>`'s, the
consequence is a **wrong answer rather than a use-after-free**:
`BitArray::Enumerator` holds an index bounds-checked against the current length on
every step. At ~10^8 mutations/second the horizon was about 43 seconds.

`BitArray` never had the signed-overflow undefined behaviour: its counter was
already `std::uint32_t` before #1787, diverging from .NET's signed `int`
(`BitArray.cs:44`). UBSan reports **0** runtime errors on both sides of this
change, confirming that this ticket closed only the remaining *logical* ABA
horizon.

Measured: `sizeof(BitArray::Enumerator)` 32 → **40** (`arr_` keeps offset 8, the
snapshot at 16 widens, and `index_`/`current_`/`state_` each move by 8 — nine
bytes were needed after an eight-byte snapshot where eight were available, in any
member order); `sizeof(BitArray)` **unchanged at 48**, because the wider counter
landed in the four bytes of tail padding the container already had; `alignof` 8
on both; and **0 `BitArray` symbols added, removed or renamed** (64 on each side,
byte-identical name lists). No public signature changed; every in-repository
caller compiles unmodified. The break is therefore **binary-only and silent**: a
stale object file links with **no diagnostic in any of eight tested
configurations**, and then either corrupts the member following an embedded
`Enumerator` with **no AddressSanitizer report at all** (the bytes are inside the
same allocation), or — at `-O2` — silently reports **zero elements for a
non-empty array**. Under ASan the same case surfaces as a
`new-delete-type-mismatch` ("allocated 32 bytes, deallocated 40"). The fail-fast
guard keeps firing in every configuration, so a consumer cannot use it as
evidence that it rebuilt.

The required adapter flip was **mutation-checked**: putting
`BitArrayAdapter::kNarrowCounter` back to `true` fails **two** tests, including
the residual assertion that #1788 §19.11 strengthened precisely so this flip
would be load-bearing rather than cosmetic.

One measurement is disclosed rather than filed under noise: the `RightShift(1)`
benchmark row moved +88 ns/op (~8%) and reproduced across fourteen paired runs.
It is **not** the counter — `BitArray::RightShift`'s generated code is
instruction-for-instruction identical on both sides, and recompiling both sides
with `-falign-loops=32 -falign-functions=64` inverts the sign, making the post
side 197 ns *faster*. It is `-O2` code alignment.

Validation from a fresh configure plus a clean-first rebuild at **three jobs**
(635 objects, 0 predating the marker, 37 of 38 executables relinked, 0 warnings,
0 errors): `Collections.Core` **2,637** (was 2,594); full repository **13,923
across 37 executables** (was 13,880); negative fixtures **8 / 51** all rejected
plus 37/37 self-test, none added and none needed since no public signature
changed; version-seam ODR **2 seams / 18 specialisations** plus 12/12 self-test,
none added; module graph **41 / 90** unchanged; Doxygen **1,941** of the 1,942
ceiling, **unchanged**; the full ten-component selective matrix and a new tracked
`Collections.Core` consumer fixture passed; ASan/UBSan/LSan `Collections.Core`
**2,637 with zero reports**, LSan proved active by a bounded self-test, and a
200,000-bit boundary-positioned walk clean; `git diff --check` clean; local CI
gate passed. **TSan was not run**: no atomic, no `mutable` cache, no hidden
`const` write, and no thread-safety claim is made for `BitArray` before or after
— `getIsSynchronizedProperty()` still returns `false`.

Tickets #1773 and #1803 remain `blocked` and untouched. #1788 stays `done`;
#1790, #1791 and #1792–#1802 remain `done` and none was reopened. **With #1789,
no collection in this repository retains a 2^32 enumerator-snapshot ABA horizon**
— every one is 2^64, and `detail::NarrowMutationCounter` has no user left. CNA and
mobile-eggbert were not inspected, searched, configured, built, or modified, and
no claim is made about whether they use `BitArray`. No push, merge, rebase, tag,
or publication occurred. The full record is
`docs/CollectionVersionCounterSweep.md` §20.

## Post-audit remediation batch — ticket #1803, the SortedSet version seam's consumer-side guard (2026-07-29)

Ticket #1803 (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`, P3, size XS,
`tooling`, area *Developer experience*) is **done**. Durable record:
`docs/NegativeConsumerFixtureValidation.md`
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
