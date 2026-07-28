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
