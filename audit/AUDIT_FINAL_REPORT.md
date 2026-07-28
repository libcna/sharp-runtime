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
