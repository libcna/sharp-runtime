# Audit progress

## Current state

- Audit started: 2026-07-25.
- Scope frozen from a clean `feature/work` source checkout; audit artifacts
  are the only expected working-tree changes.
- Eligible files: 1,748.  Excluded tracked files: 33.
- Completed per-file reports: 1,748 (100.0% of eligible scope; 1,699/1,699 runtime-module files).
- Findings confirmed at audit closure: 364 (ninety-one high, two hundred
  sixty-two medium, eleven low). The post-audit index now has 356 open
  `confirmed` findings (eighty-seven high, two hundred fifty-nine medium, ten
  low) and 8 `remediated` findings (SR-AUD-089, SR-AUD-090, SR-AUD-356,
  SR-AUD-357, SR-AUD-358, SR-AUD-360, SR-AUD-363, SR-AUD-364). Open risks: 2
  documented-adaptation questions.

## Post-audit remediation checkpoint

Ticket #1767 remediated SR-AUD-356 and SR-AUD-364 / CCF-018 on 2026-07-27.
Permanent lifecycle/mutation regressions pass 13/13, Collections.Core passes
1,435/1,435, and the direct ASan/UBSan replacement probe reports zero
failures. The required network-permitted `scripts/local_ci_check.sh build`
closure gate now passes 12,694/12,694 tests across 37 executables with zero
warnings/errors, including all formerly sandbox-blocked HTTP/socket cases.
Module boundaries remain 41/90; catalogue, database, and diff checks pass.
Doxygen remains below its ceiling at 1,941/1,942. LeakSanitizer initialization
remains environment-limited by the sandbox's `ptrace` policy, while the same
probe is clean with ASan and UBSan active.

Design ticket #1768 and implementation ticket #1769 then remediated SR-AUD-357
(CCF-019, LinkedListNode lifetime) on 2026-07-27. The selected contract is
recorded in `docs/LinkedListNodeLifetime.md`: independently allocated,
reference-counted nodes with an explicit null/detached/attached state, so
`Remove`, `Clear`, and destruction of the owning `LinkedList<T>` detach the node
and retain its value instead of leaving a dangling `std::list` iterator.
Permanent regressions pass 49/49, Collections.Core passes 1,484/1,484, the
standalone `Collections.Core` public-header consumer fixture compiles with
`-Werror`, and the direct ASan/UBSan probe that produced the original
heap-use-after-free reports `failures=0` — with LeakSanitizer enabled and
clean in this run. The network-permitted `scripts/local_ci_check.sh build`
gate passes 12,743/12,743 tests across 37 executables with zero
warnings/errors. Module boundaries remain 41/90; validator-test, catalogue,
database, selective-component, and diff checks pass, and Doxygen stays at
1,941/1,942 with no new warning from the touched headers. CCF-019's JsonNode
(SR-AUD-327) and XML LINQ (SR-AUD-333) members remain open by design.

Design ticket #1770 and implementation ticket #1771 then remediated SR-AUD-358
(CCF-020, raw `ICollection::CopyTo`) on 2026-07-27. #1770 changed no source; it
recorded the contract in `docs/ICollectionCopyToDesign.md` and established by
direct probe that the six implementations disagreed on the destination element
type (`std::any*`/`void**`/`DictionaryEntry*`, sizes 16/8/32) and that an
element-type mismatch through the interface produced no crash at all, only a
LeakSanitizer-confirmed 32-byte leak. #1771 landed the approved public source-
and ABI-breaking change: `virtual void CopyTo(void*, intcs) = 0` is removed from
`ICollection` in favour of non-virtual, validating `CopyTo(ObjectSpan, intcs)` /
`CopyTo(std::vector<std::any>&, intcs)` over one protected pure virtual
`copyToCore(ObjectSpan, intcs)` hook, plus checked typed
`std::vector<void*>` / `std::vector<DictionaryEntry>` overloads on the concrete
collections. No deprecated shim was retained, so a stale call site is a compile
error naming the replacement rather than a run-time throw; every consumer must
rebuild against the changed vtable, per `docs/Migration-ICollectionCopyTo.md`.
Permanent regressions pass 128/128 across every `ICollection` implementation
(and 128/128 again under ASan + UBSan + LeakSanitizer), Collections.Core passes
1,612/1,612, the standalone `Collections.Core` public-header consumer fixture
compiles with `-Werror` and runs successfully, the replacement
ASan/UBSan/LeakSanitizer probe reports `failures=0` with no diagnostic and no
leak on the four previously unsafe scenarios, and the old raw calls now yield
four captured `no matching function` compile errors. The full repository gate
passes 12,871/12,871 tests across 37 executables with zero warnings/errors.
Module boundaries remain 41/90; validator-test, catalogue, database,
selective-component, and diff checks pass, and Doxygen is at 1,942/1,942 --
unchanged from before the ticket, because the README link to the new migration
document adds one instance of the pre-existing unresolved-markdown-link warning
shared by every README documentation link, offsetting one removed from
`ICollection.hpp`.

Ticket #1775 then remediated SR-AUD-363 (Hashtable `IDictionary` key and view
contracts) on 2026-07-27. It belongs to no `CCF-*` group. Two facts beyond the
original evidence were established by direct probe first: a consumer that
follows the `IDictionary` documentation and uses the promised view without a
null check is an ASan-confirmed SEGV plus a UBSan `member access within null
pointer of type 'struct ICollection'`, while the sibling
`ListDictionaryInternal` answers the identical caller code correctly — making
this an interface defect with divergent implementations rather than a
Hashtable-local omission; and the stringified null key `"0"` aliases the
ordinary string key `"0"`, while a third entry point, `Remove(const char*)`,
terminated on `std::string`'s null construction with a `std::logic_error`
invisible to code catching `System::Exception&`. Both view properties now
return a live, caller-owned `MemberCollection` delegating `Count`, `SyncRoot`,
`IsSynchronized`, `GetEnumerator`, and `copyToCore` to the owning table and
reusing the #1771/#1774 copy boundary unchanged; `toKey()` became the single
validating conversion site every raw-key path passes through. No public
signature changed and no virtual member was added or removed, so this is
neither a source nor an ABI break. Permanent regressions pass 70/70 —
parameterised over both non-generic `IDictionary` implementations — and 70/70
again under ASan + UBSan + LeakSanitizer with no diagnostic and no leak; the
33-assertion replacement probe reports `failures=0`; Collections.Core passes
1,732/1,732; the standalone `Collections.Core` public-header consumer fixture
compiles with `-Werror` and runs successfully; and the network-permitted
`scripts/local_ci_check.sh build` gate passes 12,991/12,991 tests across 37
executables with zero warnings/errors. Module boundaries remain 41/90;
validator-test, catalogue, database, selective-component, and diff checks
pass, and Doxygen stays at exactly 1,942/1,942.

Two separate pre-existing defects surfaced while implementing #1775 were
recorded as inactive tickets #1776 and #1777. #1777 is a genuine post-audit
documentation-only discovery with no covering `SR-AUD-*` identifier. #1776 was
described the same way at the time, but that was inaccurate: SR-AUD-089 and
SR-AUD-090, already `confirmed` within the frozen SR-AUD-001..364 numbering,
covered this exact defect pair. Ticket #1776 (`REMED-CORE-ARGNULL-MESSAGE`,
P2, size XS) remediated both on 2026-07-27 on local branch
`feature/remediation-argument-null-message`: `ArgumentNullException(paramName)`
now passes its raw default message straight to the
`ArgumentException(message, paramName)` base constructor instead of
precomposing the suffix itself, so the `(Parameter 'x')` marker is appended
exactly once (SR-AUD-090) and, as a side effect of removing the unsafe local
concatenation, a null `paramName` no longer null-dereferences (SR-AUD-089).
Permanent regressions pass 26/26 new cases across `ArgumentNullExceptionTests.cpp`,
`ArgumentExceptionTests.cpp`, and `ArgumentOutOfRangeExceptionTests.cpp`;
`SharpRuntimeTests_Core_Base` 4,972/4,972 and
`SharpRuntimeTests_Collections_Core` 1,732/1,732; the `Core.Base` standalone
public-header consumer fixture extended to cover throw/catch through
`System::Exception` compiles and runs under `-Werror`. No public signature,
virtual member, or inheritance changed, so this is neither a source nor an ABI
break. Findings index: 357 open `confirmed`, 7 `remediated`.

Ticket #1778 (`REMED-COLL-CONCURRENTDICT-ADDORUPDATE`, P2, size S) remediated
SR-AUD-360 on 2026-07-27 on local branch
`feature/remediation-coll-concurrentdict-addorupdate`; the findings index now
records **356 open findings and eight `remediated`**. `ConcurrentDictionary`'s
two `AddOrUpdate` overloads unconditionally overwrote the entry with the
update factory's result even when another thread had mutated or removed it
between the observed read and the write-back, silently discarding the
intervening write. Real .NET's `TryUpdateInternal` instead gates the commit
on `EqualityComparer<TValue>.Default.Equals` against the previously observed
value and retries (re-observes, re-invokes the factory) on a mismatch. Both
overloads now loop the same way, still without holding the internal mutex
across either factory call, preserving the existing reentrancy/deadlock-
avoidance guarantee. This requires `TValue::operator==`, the same requirement
`TryUpdate` on this class already carries; no public signature changed and no
virtual member was added or removed, so this is neither a source nor an ABI
break. Pre-fix reproduction (gitignored
`build-probe-concurrentdict/probe1_lost_update.cpp`) deterministically
reproduced the finding's own `add-or-update-result=1 final=1` symptom (5/5
runs); post-fix, the same probe and a 16-thread/32,000-operation stress probe
both pass cleanly under ASan+UBSan+ThreadSanitizer. Closure evidence: 4 new
permanent regressions in `ConcurrentDictionaryTests.cpp`;
`SharpRuntimeTests_Collections_Core` 1,736/1,736 (was 1,732); network-permitted
`scripts/local_ci_check.sh build` 13,021/13,021 across 37 executables, zero
warnings/errors (was 13,017); boundaries unchanged at 41 modules/90 edges;
validator tests 7/7; catalogue current; database consistent; `git diff --check`
clean. Doxygen was independently found already at 1,944 warnings before this
ticket's changes (not the 1,942 figure recorded elsewhere in this document);
this ticket adds zero new warnings, so it did not introduce that drift --
see the ticket's planning notes for the measurement.

**Correction (ticket #1781, 2026-07-27):** the paragraph above states this
ticket's Doxygen figure was "independently found already at 1,944 warnings."
Ticket #1781 re-ran the repository's canonical
`scripts/check_doxygen_warnings.sh` (Doxygen 1.9.8, `doxygen Doxyfile`,
`grep -c ': warning:'`) on the current tree three times, including once from a
fully clean `docs/generated/`, and got exactly **1,942** warnings every time --
the documented ceiling, not 1,944. This confirms ticket #1779's own
re-measurement (see `NEXT.md` and `plan.md`): the 1,944 figure came from a
looser, non-canonical `grep -c 'warning:'` pattern (no leading colon) that
additionally matches two bare `Inheritance graph ... not generated` advisory
lines, not from an actual warning-count regression this ticket introduced.
This correction is recorded here rather than silently rewriting the paragraph
above, per this repository's practice of preserving historical narrative.

Design ticket #1782 (`REMED-COLL-SORTEDSET-VIEW-DESIGN`, P2, size M,
design-only) then answered SR-AUD-361 on 2026-07-27 on local branch
`feature/remediation-coll-sortedset-view-design`, changing no production or test
source. `SortedSet<T>::GetViewBetween` returns an independent snapshot instead
of .NET's live, range-enforced, write-through `TreeSubSet`. The selected
architecture -- `SortedSet<T>` holding `std::shared_ptr<State>` plus optional
bounds, so one public type is either an owning set or a bounded live view, with
`GetViewBetween`'s return type unchanged -- is recorded in
`docs/SortedSetLiveViewDesign.md` alongside four rejected alternatives, a
fourteen-row compatibility matrix, all thirty-five required design decisions,
and a six-probe evidence index. **SR-AUD-361 stays `confirmed`**, qualified
`confirmed (design-complete)`, so the open/remediated counts are unchanged at
355/9. Implementation is ticket **#1783**
(`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L), created **`blocked`** pending
explicit approval of the `const` removal on `GetViewBetween`, the
snapshot-to-live-view semantic change, and the measured object-layout change
(`sizeof(SortedSet<int>)` 56 → 40, `sizeof(SortedSet<std::string>)` 56 → 104).

Evidence: the gitignored `build-probe-sortedset/` tree independently reproduced
the finding's own `source-add-visible-in-view=0` /
`view-add-visible-in-source=0` symptom and the full pre-fix contract under
ASan+UBSan+LeakSanitizer with no diagnostic and no leak; a prototype of the
selected architecture passes the identical matrix with `failures=0`, including a
100,000-element scale case; the three existing `GetViewBetween` tests and the 41
mutable-`SortedSet` tests rerun unchanged and passing; boundaries unchanged at
41 modules/90 edges, validator tests 7/7, catalogue current, database
consistent, `git diff --check` clean; and Doxygen 1.9.8 at exactly 1,942/1,942 --
unchanged, since this ticket added only `docs/*.md` and `audit/*.md`, which
Doxygen does not scan. The full `scripts/local_ci_check.sh build` gate was run
rather than omitted and passed 13,022/13,022 tests across 37 executables with
zero build warnings/errors — unchanged from ticket #1780.
`scripts/check_selective_components.sh` was not run: no public header and no
component metadata changed.

**Planning-accuracy correction (ticket #1782, 2026-07-27):** `NEXT.md`,
`plan.md`, and `SortedSet.hpp`'s own `@warning` block each state that a live
view is not achievable on `std::set` without a hand-rolled tree matching .NET's
own, and ticket #1779 used that premise to defer SR-AUD-361. The premise does
not hold -- `std::set` already provides `lower_bound`, `upper_bound`, and stable
iterators, and the prototype demonstrates a working bounded view. The real cost
is the ownership model, the copy/move semantics, and the `const` removal.
Recorded here rather than rewritten in place, per the same preserved-narrative
practice.

## Initial validation evidence

`git diff --check` passed. `scripts/local_ci_check.sh build` reached the test
phase after a clean boundary validation, catalogue check, configure, and
warning-free build. It could not complete in this sandbox: all six local-server
`SharpRuntimeTests_Net_Http` cases failed at construction with
`Socket::Socket: socket() failed`. A focused rerun of the same six tests
reproduced the identical zero-millisecond failure. `NEXT.md` already documents
that HTTP, socket, and ping tests need local-network permission, so this is
recorded as an environment-limited validation result, not a source regression
or a reason to weaken/skip the tests. A network-permitted environment was
required before a remediation batch could close and ticket #1767 subsequently
passed that gate, as recorded above.

## Immediate sequence

1. Do not begin remediation in this audit-only phase.  Review the completed
   final report and findings index with the user, then create bounded repair
   tickets in severity/dependency order.
2. Retain the documented network-permitted full-gate run as a validation
   prerequisite for remediation closure.

## Assumptions and decisions

- The user explicitly selected an audit-only phase.  Even when a missing
  assertion, diagnostic, or defect is clear, this phase reports it with
  evidence and a proposed verification; it does not repair the source.
- `vendor/` and legal/VCS placeholder files are outside the authored-runtime
  audit; all other tracked first-party text-like files are in scope.
- Documentation claims are reviewed for consistency with local source and
  executable validation, not merely spelling or style.

## Latest checkpoint

The complete 142-file `Collections` shard is mirrored; together with
`modules/README.md`, it closes the deterministic 1,748-file scope.  Its focused
target passed 1,422/1,422.  Direct ASan/UBSan probes confirm SR-AUD-356
(invalid enumerator Current), SR-AUD-357 (retained LinkedListNode use-after-free),
and SR-AUD-358 (raw ICollection CopyTo null write). Direct functional probes
confirm SR-AUD-359 (mutable ReadOnlyDictionary.Empty), SR-AUD-360
(ConcurrentDictionary lost update), SR-AUD-361 (snapshot SortedSet view),
SR-AUD-362 (duplicate FrozenDictionary key overwrite), and SR-AUD-363
(Hashtable null-key/null-view contract); source inspection confirms SR-AUD-364
(BitArray enumerator state/version gap). The preceding XML checkpoint passed
377/377 and IO passed 527/527. Audit-only: no production or test source changed.

## Audit closure

The reconciled scope has exactly 1,748 eligible source reports and no missing
mirror.  The final evidence, validation limits, severity roll-up, and repair
handoff are in `AUDIT_FINAL_REPORT.md`; remediation requires separate user
direction and bounded follow-up tickets.

Final reconciliation passed `gmake -C build -j4`, the plan database
consistency checker, module-boundary validation (41 physical modules, 90
production edges), and `git diff --check`.

## Resume point

The audit is complete. Do not resume source review under ticket #1766; begin
only separately authorized remediation planning from `AUDIT_FINAL_REPORT.md`
and `AUDIT_FINDINGS_INDEX.md`. The remaining historical detail below records
how earlier evidence was obtained.

The integration shard is complete.  `System::String`, `System::Char`,
`System::Object`, `System::Type`, `System::Int128`, `System::UInt128`,
`System::Int64`, `System::UInt64`, `System::Int32`, `System::UInt32`,
`System::Byte`, `System::SByte`, `System::Int16`, `System::UInt16`, and
`System::Boolean`, `System::IntPtr`, `System::UIntPtr`, `System::Enum`,
`System::Single`, `System::Double`, `System::Decimal`, `System::Math`, and
`System::MathF`, `System::BitConverter`, and
`System::Numerics::BitOperations`, `System::Numerics::DivisionRounding`, and
`System::Numerics::TotalOrderIeee754Comparer`, `System::HashCode`, and
`System::Span`/`System::ReadOnlySpan`, `System::SpanSplitEnumerator`,
`System::Memory`/`System::ReadOnlyMemory`, `System::MemoryExtensions`, and
`System::Guid`, `System::Array`, and `System::ArraySegment`
source reviews are complete, as are `System::Convert` declaration,
implementation, and focused tests; `System::Half` and its focused tests are
also complete. The
128-bit focused validation passed 59/59 Core.Base and 75/75 integration tests;
the 64-bit focused validation passed 85/85 Core.Base and 36/36 integration
tests; the 32-bit numeric/style filter passed 167/167; and the focused
8/16-bit numeric filter passed 312/312. Boolean's focused filter passed 37/37
with no new finding; IntPtr/UIntPtr focused validation passed 20/20 but UBSan
confirmed IntPtr Add/Subtract boundary UB; Enum validation passed 19/19 with
no new finding; Convert validation passed 204/204 but direct probes confirmed
SR-AUD-026 through SR-AUD-028; Half validation passed 87/87 and the exhaustive
65,536-pattern finite round trip found no mismatch. The final numeric filter
demonstrated that
SR-AUD-021 through SR-AUD-023 extend across the reviewed small wrappers and
confirmed SR-AUD-024: SByte/Int16 `IsPositive(0)` returns false despite the
.NET generic-math `>= 0` contract, while their tests lock in the wrong result.
UBSan confirmed `Int128` minimum decimal-boundary UB and `UInt128`
out-of-range shift UB. Single's focused suite passed 102/102 and Double's
focused suite passed 164/164. Their direct probes independently confirmed
SR-AUD-029 through SR-AUD-033; Single also has SR-AUD-034, which excludes a
positive-sign NaN from `IsPositive`. Both floating wrappers extend SR-AUD-021
format diagnostics. Decimal's focused suite passed 143/143; its direct probe
confirmed SR-AUD-035 through SR-AUD-038 and extended SR-AUD-022 to Decimal.
The Math/MathF focused filter passed 174/174; its direct probe extended
SR-AUD-022, SR-AUD-031, and SR-AUD-036, and confirmed SR-AUD-039/040. Resume
the next unreviewed `Core.Base` primitive-adjacent or parser surface after its
source/test inventory is read. BitConverter's focused suite passed 67/67, but
ASan confirmed SR-AUD-041: typed vector decoders read before/after short or
negative-index input rather than validating it. BitOperations' focused suite
passed 13/13; a UBSan/ASan probe found no behavioral mismatch in its implemented
32/64-bit surface, but current .NET `Crc32C` and exact `TrailingZeroCount(long)`
support are absent and need an API-baseline decision before classification.
DivisionRounding has the correct five declaration values and explicitly lacks
consumers. TotalOrderIeee754Comparer's focused filter passed 6/6 and its probe
found correct ordering over raw half/single/double values, but SR-AUD-042
confirms its omission of the local and .NET equality-comparer contract.
HashCode's focused filter passed 25/25, but SR-AUD-043 is ASan-confirmed:
`AddBytes` turns a public negative ReadOnlySpan length into a huge raw read.
The completed Span audit confirms the shared root: both public constructors
accept negative lengths.  It also confirms SR-AUD-044: all Span/ReadOnlySpan
copy paths corrupt overlapping `std::string` ranges through forward `std::copy`.
SpanSplitEnumerator's focused filter passed 11/11, but SR-AUD-045 confirms
that an empty exact sequence never advances and produces an infinite stream of
empty spans rather than the source once. MemoryExtensions' focused filter
passed 92/92, but the ASan/UBSan probe confirms SR-AUD-047: static `CopyTo`
writes past a short destination. Its nontrivial overlap path extends SR-AUD-044;
SR-AUD-046 records float NaN ordering/search/sequence-comparison divergence,
and SR-AUD-048 records Unicode-whitespace trim divergence. Memory and
ReadOnlyMemory validation passed 49/49 and 23/23 in Buffers plus 7/7 duplicate
Core.Base ReadOnlyMemory smoke tests; their sanitizer probe extends SR-AUD-043
and SR-AUD-044 and confirms new high SR-AUD-049: `ReadOnlyMemory::Slice(INT_MIN)`
overflows signed arithmetic before throwing. Guid validation passed 80/80, but
an independent eight-thread TSan probe confirms that `Guid::NewGuid` races on
its static Mersenne Twister (extending SR-AUD-010); Guid's char/UTF-8
span-parsing overloads also extend SR-AUD-043 through raw signed-to-`size_t`
conversion. New high SR-AUD-050 records that both `NewGuid` and
`CreateVersion7` use a seeded standard PRNG instead of current .NET's OS CSPRNG
strong-entropy source. Resume the next unreviewed direct Span consumer or
remaining Core.Base primitive/parser after full source and test inventory.
The alias-only `Action`/Buffers `SpanAction` and `ReadOnlySpanAction` headers
plus their six direct Buffer tests are audited with no new finding. Their
11-test integration alias filter and six-test Buffer filter passed; a
warning-free standalone compile confirmed that the Core and Buffers duplicate
public alias declarations compose in one translation unit. Resume the next
direct Span consumer or remaining Core.Base primitive/parser after full source
and test inventory.
ArraySegment validation passed 45/45. Its sanitizer probe extends SR-AUD-044
for both overlapping CopyTo paths and confirms new high SR-AUD-054: default
segment `Slice(0)` binds a null vector reference and reaches ASan/UBSan instead
of .NET's `InvalidOperationException`; `ToArray` can silently return empty.
New SR-AUD-055 records that vector CopyTo resizes an undersized destination;
SR-AUD-043 and SR-AUD-018 extend to its vector-length narrowing and invalid
hash assertions. Resume the next direct Span consumer or remaining Core.Base
primitive/parser after full source and test inventory.
Array validation passed 80/80, but its independent sanitizer probe extends
SR-AUD-044 (`std::string` overlap `abcd` becomes `aaaa`) and SR-AUD-046 (NaN
sort/search divergence). New high SR-AUD-051: raw-pointer `Array::Copy` passes
negative signed metadata and arbitrary nontrivial objects to `memcpy`; ASan
reports a negative-size parameter and invalid string destruction. New
SR-AUD-052 records empty callable diagnostics, and low SR-AUD-053 records the
exact `Array.MaxLength` mismatch. Resume the next direct Span consumer or
remaining Core.Base primitive/parser after full source and test inventory.
The `ISpanFormattable`, `ISpanParsable`, `IUtf8SpanFormattable`, and
`IUtf8SpanParsable` declarations plus their three focused direct test files
are now audited. Their combined focused Core.Base filter passed 33/33 with no
new implementation finding. The reports record missing failure-output,
non-null-provider, exception-taxonomy, malformed-UTF-8, and short-buffer
assertions. The supporting `ISpanFormattable` section of
`SystemTypesRemainingTests.cpp` was used as validation evidence but that larger
test source is not marked complete until its full file-wide audit. Resume a
complete remaining Core.Base test/source inventory or the next direct Span
consumer.
The core `IFormatProvider`, `IFormattable`, `IObservable`, `IObserver`,
`IParsable`, `IProgress`, and `IServiceProvider` declarations plus
`InterfaceTests2.cpp` are audited. Their focused filter passed 11/11. New
medium SR-AUD-056 records that the sole direct observable fixture returns no
unsubscription handle and permits `OnNext` after `OnCompleted`, contrary to
the local contracts; no first-party production observable implementation
exists. Resume the next complete Core.Base interface or primitive/test
inventory.
`WeakReference`/`WeakReferenceT` and their dedicated test source are audited.
Their combined focused filter passed 23/23 with no new evidence-backed finding.
The shared-pointer adaptation explicitly stores but cannot implement
TrackResurrection; the reports retain missing stale-output, aliasing, cycle,
and concurrency lifetime assertions. Resume the next complete Core.Base
value-type or primitive/test inventory.
`FormattableString`, its factory, and the focused direct test source are
audited. Core.Base and integration filters passed 11/11 and 13/13, but the
probe extends SR-AUD-015: sequential replacement reinterprets inserted brace
text, breaks escaped braces, and leaves missing indices literal. New low
SR-AUD-059 records the factory documentation's false empty-format exception
claim; implementation and current .NET both permit empty format text. Resume
the next complete Core.Base value-type or primitive/test inventory.
`CharEnumerator` and its dedicated state-machine tests are audited. The
focused Core.Base filter passed 11/11 with no new evidence-backed finding. The
reports retain Current-after-dispose/reset, large-string narrowing,
embedded-NUL/non-ASCII, clone-at-end, and repeated-dispose assertion gaps.
Resume the next complete Core.Base value-type or primitive/test inventory.
`MDArray` rank constants and their dedicated two-test source are audited; the
focused filter passed 2/2 with no new finding. No multidimensional allocation
or indexing implementation currently consumes the constants. Resume the next
complete Core.Base value-type or primitive/test inventory.
`ValueTuple` and its dedicated direct tests are audited. The combined direct
and aggregate Core.Base filter passed 53/53, while a standalone float-NaN probe
extends SR-AUD-046: raw `<` makes NaN compare equal to finite tuple items and
raw `==` makes a tuple containing NaN unequal to itself, unlike the local .NET
default comparer/equality-comparer implementation. No production or test
source changed. Resume the next complete Core.Base value-type or primitive/test
inventory.
`DateOnly` header, implementation, and complete DateOnly/TimeOnly test source
are audited. Their focused filter passed 119/119, but UBSan confirms new high
SR-AUD-060: `FromDayNumber`, `AddDays`, `AddMonths`, and `AddYears` perform
signed overflow before range handling for reachable extreme public arguments.
New medium SR-AUD-061 records that the ISO parser accepts arbitrary trailing
text. No production or test source changed. Resume the next complete Core.Base
value-type or primitive/test inventory.
`StringComparer` and its direct tests are audited. Their 42-test focused filter
passed with no new implementation finding. The reports retain the documented
UTF-8-byte/culture fallback and extend SR-AUD-018: one test incorrectly
requires two unequal case-sensitive strings to have different hashes. No
production or test source changed. Resume the next complete Core.Base
value-type or primitive/test inventory.
`Tuple`, TupleExtensions, and both direct Tuple test sources are audited. Their
combined filter passed 94/94, but the NaN probe extends SR-AUD-046. New high
SR-AUD-062: `tupleHashCombine` reaches UBSan-confirmed signed-addition overflow
for a public Tuple2 hash input. New medium SR-AUD-063: every Tuple component is
publicly mutable although local .NET Tuple uses readonly backing fields. The
newer direct tuple tests also extend SR-AUD-018 by forbidding valid hash
collisions. No production or test source changed. Resume the next complete
Core.Base value-type or primitive/test inventory.
`Lazy<T>`, LazyThreadSafetyMode, and direct tests are audited. The combined
Lazy filter passed 38/38, but a standalone probe confirms three new medium
findings: SR-AUD-064 invalid modes are silently accepted and dispatched as
PublicationOnly; SR-AUD-065 empty `std::function` factories defer to native
`bad_function_call`; and SR-AUD-066 PublicationOnly recursion wrongly throws
InvalidOperationException. CCF-011 extends to the empty factory boundary. No
production or test source changed. Resume the next complete Core.Base
value-type or primitive/test inventory.
`Buffer` and its direct test source are audited. The focused filter passed
38/38, but ASan confirms new high SR-AUD-067: raw BlockCopy converts negative
count metadata to unbounded memmove. The generic typed-vector raw-byte path
also extends SR-AUD-051; a vector<string> probe reaches double-free because no
trivially-copyable constraint exists. No production or test source changed.
Resume the next complete Core.Base value-type or primitive/test inventory.
`ValueType` and its direct test source are audited. The combined direct and
aggregate Core.Base filter passed 10/10. New medium SR-AUD-068 records that
the public, constructible C++ base defaults to identity/address semantics,
where current .NET has an abstract base with fieldwise default equality and
value hashing; the direct fixture locks in the incompatible identity fallback.
No production or test source changed. Resume the next complete Core.Base
value-type or primitive/test inventory.
`SequencePosition` and the complete mixed buffer batch test source are audited.
Its six focused SequencePosition tests and all six batch suites passed 63/63.
New medium SR-AUD-069 records that public mutable `void*`/integer components
break .NET's opaque readonly position contract; a standalone compiler probe
rewrites both values after construction. No production or test source changed.
Resume the next complete Core.Base or Buffers source/test inventory.
`ArrayBufferWriter<T>` is audited using the complete Batch6 test evidence.
Its focused ten-test subset passed within the 63/63 batch filter. New medium
SR-AUD-070 records that vector resize and `Clear` silently require a
default-constructible element type; a standalone C++20 compile probe for a
valid non-default-constructible type fails in `std::vector::resize`. No
production or test source changed. Resume the next complete Buffers
implementation or source/test inventory.
`MemoryPool<T>` is audited using the complete Batch6 test evidence. Its
focused 11-test subset passed within the 63/63 batch filter, but ASan confirms
new high SR-AUD-071: owner `Dispose` returns empty Memory instead of throwing
and turns a retained Memory view into a null dereference. SR-AUD-070 extends
to its hidden default-constructor requirement. No production or test source
changed. Resume the next complete Buffers implementation or source/test
inventory.
The `IBufferWriter<T>` and `IMemoryOwner<T>` abstract headers are audited with
no new standalone implementation finding. Their reports retain missing public
nonempty-buffer, post-Advance invalidation, post-dispose, and polymorphic
conformance assertions; SR-AUD-071 remains owned by MemoryPoolHeapOwner.
No production or test source changed. Resume the next complete Buffers
implementation or source/test inventory.
`ReadOnlySequence<T>` is audited using the complete Batch6 evidence. Its
focused nine-test subset passed within the 63/63 batch filter, but an
ASan/UBSan probe confirms two new high findings: SR-AUD-072 raw null pointer
construction dereferences null; SR-AUD-073 `TryGet` accepts before-start and
negative forged positions, exposing pre-slice data or out-of-bounds memory.
New medium SR-AUD-074 records default-sequence enumeration of one empty
segment rather than none. No production or test source changed. Resume the
next complete Buffers implementation or source/test inventory.
`SequenceReader<T>` is audited using the complete Batch6 evidence. Its focused
13-test subset passed within the 63/63 batch filter, but a standalone probe
confirms new medium SR-AUD-075: false TryRead/TryPeek calls retain stale output
references rather than assigning default as current .NET does. No production
or test source changed. Resume the next complete Buffers implementation or
source/test inventory.
`BinaryPrimitives` is audited using the complete Batch6 evidence. Its focused
14-test subset passed within the 63/63 batch filter; full source review found
no new evidence-backed implementation defect. Reports retain missing Try*,
floating payload, 128-bit, big-endian CI, and MSVC 128-bit API-baseline
evidence. No production or test source changed. Resume the next complete
Buffers implementation or source/test inventory.
`ArrayPool<T>` and its dedicated direct tests are audited. Its focused filter
passed 7/7, but a standalone probe confirms new medium SR-AUD-076: both zero
configuration inputs are accepted and both public Create limits are discarded,
where current .NET requires positive values and creates configured buckets.
SR-AUD-070 extends to vector/default-construction in Rent and clear. No
production or test source changed. Resume the next complete Buffers
implementation or source/test inventory.
`MemoryManager<T>`, `IPinnable`, and the complete Batch16 buffer test source
are audited. Its nine-suite focused filter passed 37/37. No new standalone
implementation defect is classified: manager-backed Memory is an explicit,
documented unsupported C++ storage adaptation that the test intentionally
expects as `NotSupportedException`. Reports retain the missing lifecycle, pin,
configured-pool, intern-identity, currency-rounding, SearchValues, and reader
extension assertions. No production or test source changed. Resume the next
complete Buffers implementation or source/test inventory.
`SearchValues<T>` is audited using the complete Batch16 evidence. Its focused
eight-test subset passed within the 37/37 filter, but a standalone C++20
compile probe confirms new medium SR-AUD-077: the public equality-comparable
template promise silently requires `std::hash<T>` through unordered_set. No
production or test source changed. Resume the next complete Buffers
implementation or source/test inventory.
`SequenceReaderExtensions` is audited using the complete Batch16 evidence. Its
focused six-test subset passed within the 37/37 filter; full source review
found no new evidence-backed implementation defect in signed contiguous
16/32/64-bit paths. Reports retain unsigned, false-output/state, multi-segment,
big-endian, strict union-punning, and include-hygiene evidence gaps. No
production or test source changed. Resume the next complete Buffers
implementation or source/test inventory.
`Base64` and its dedicated direct suite are audited. `Base64Test.*` passed
40/40, but a standalone probe confirms high SR-AUD-078: in-place encoding a
full triple followed by a remainder overwrites the unread remainder and corrupts
the Base64 output. It also confirms SR-AUD-079 through SR-AUD-081: decoder and
validator accept noncanonical unused padding bits, the streaming overload
accepts padding when `isFinalBlock` is false, and padded decode consumes trailing
whitespace that current .NET deliberately leaves unconsumed. No production or
test source changed. Resume the next complete Buffers text implementation or
source/test inventory.
`Base64Url` and its dedicated direct suite are audited. `Base64UrlTest.*`
passed 31/31, but its separately reproduced in-place path extends SR-AUD-078
and its final-sextet decoder/validator extends SR-AUD-079. New medium
SR-AUD-082 records that it rejects current .NET's optional final `=` and `%`
padding forms despite documenting itself as the counterpart. No production or
test source changed. Resume the next complete Buffers text implementation or
source/test inventory.
`StandardFormat`, `OperationStatus`, and their complete mixed fixture are
audited. The three-suite filter passed 38/38; no enum defect is classified, but
the StandardFormat probe confirms new medium SR-AUD-083: `ToString` renders a
default/zero-symbol format as embedded NUL text instead of .NET's empty string.
No production or test source changed. Resume the next complete Buffers text
implementation or source/test inventory.
`Utf8Formatter` and its dedicated 25-test suite are audited. Its direct filter
passed 25/25 and the earlier precision-99 staging-buffer repair remains covered
for D/signed-D/X/N. No new evidence-backed implementation defect is classified
in the documented bool/integer subset; reports retain missing overload,
signed-minimum, exact-short-buffer, format-alias, and generated-vector
assertions. No production or test source changed. Resume the next complete
Buffers text implementation or source/test inventory.
`Utf8Parser` and its dedicated 25-test suite are audited. Its direct filter
passed 25/25, but UBSan confirms new high SR-AUD-084: default and `N` Int64
minimum parsing negate `INT64_MIN`. New medium SR-AUD-085 retains stale false
outputs, and SR-AUD-086 rejects valid leading plus signs for default/D integer
parsing. No production or test source changed. Resume the next complete Buffers
text implementation or source/test inventory.
`ReadOnlySequenceSegment` and `BuffersExtensions` are audited with their
focused 11/11 Batch17 subset. New medium SR-AUD-087 records that a segment
chain cannot construct any C++ ReadOnlySequence despite the header's claim;
the underlying sequence has only vector/raw-pointer construction. No production
or test source changed. Resume the Batch17 source or next complete Buffers
implementation inventory.
The complete mixed `Batch17BuffersTests.cpp` source is audited. Its seven-suite
filter passed 67/67. The report maps its normal-path sequence/Base64 coverage to
SR-AUD-075, SR-AUD-078 through SR-AUD-082, and SR-AUD-087; no new standalone
test-contract defect is classified. No production or test source changed.
Resume the next complete Buffers implementation or source/test inventory.
`MemoryHandle` is audited using the complete Batch16 evidence. New medium
SR-AUD-088 records that its documentation promises RAII/destructor cleanup,
but the type has no destructor calling `Dispose` and scope exit never unpins.
No production or test source changed. Resume the next complete Buffers/core
boundary implementation or source/test inventory.
The Buffers module is now complete at 40/40 mirrored reports. Its remaining
module metadata and three direct test files passed a combined 54/54 filter. No
new standalone implementation defect was classified: the ArrayBufferWriter
tests extend SR-AUD-070's generic-type coverage gap; BinaryPrimitives tests add
broad direct evidence without replacing byte-exact/cross-platform checks; and
`EnumeratorTests.cpp` visibly lacks the assertion needed to expose SR-AUD-074.
Resume the next complete Core.Base source/test inventory.
`ArgumentException`, `ArgumentNullException`, and
`ArgumentOutOfRangeException` declarations, implementations, and direct tests
are audited. Their combined filter passed 64/64, but ASan/UBSan confirms new
high SR-AUD-089: `ArgumentNullException(const char*)` dereferences a null
parameter name during message construction. New low SR-AUD-090 records its
duplicated parameter suffix, and new medium SR-AUD-091 records the hidden
`std::to_string` requirement in generic comparison/equality guards. The
Unicode whitespace path extends SR-AUD-048 and creates CCF-015. No production
or test source changed. Resume the next complete Core.Base exception/source
inventory.
The base `Exception` and `SystemException` declarations and implementations
are audited using the selected 62/62 direct test filter as evidence; the two
large shared test sources remain pending full file-wide review. New medium
SR-AUD-092 records that default C++ `Exception` stores an empty message where
current .NET returns its nonempty fallback diagnostic, and two tests lock that
behavior in. SystemException has no new standalone defect. No production or
test source changed. Resume the next complete Core.Base exception/source
inventory.
The full `ExceptionTests.cpp` and `ExceptionNewTests.cpp` test sources are now
audited. Their twelve-suite filter passed 124/124; reports preserve the
default-message assertion that locks SR-AUD-092, the missing null C-string and
exact-suffix checks for SR-AUD-089/090, Unicode whitespace coverage for
SR-AUD-048, and generic-template coverage for SR-AUD-091. No production or
test source changed. Resume the next complete Core.Base exception/source
inventory.
`ArithmeticException`, `DivideByZeroException`, and `OverflowException`
headers/implementations are audited. Arithmetic/DivideByZero focused filters
passed 7/7 and overflow remains covered by the 124/124 shared fixture. No new
standalone defect was classified; reports retain specific HResult,
inner-exception, null-C-string, and checked-arithmetic integration assertions.
No production or test source changed. Resume the next complete Core.Base
exception/source inventory.
`InvalidOperationException`, `NotImplementedException`,
`NotSupportedException`, `NullReferenceException`, and
`ObjectDisposedException` headers/implementations are audited against the
complete 124/124 shared exception evidence. No new standalone implementation
defect was classified; reports retain exact message/HResult, null-C-string,
inner-exception, and real state-transition diagnostic gaps. No production or
test source changed. Resume the next complete Core.Base exception/source
inventory.
`ArrayTypeMismatchException`, `FieldAccessException`,
`IndexOutOfRangeException`, `OutOfMemoryException`, and
`InsufficientMemoryException` are audited against the shared 124/124 evidence.
New medium SR-AUD-093: ArrayTypeMismatch inline constructors retain the base
SystemException HResult (`0x80131501`) instead of .NET's
`COR_E_ARRAYTYPEMISMATCH` (`0x80131503`). No production or test source
changed. Resume the next complete Core.Base exception/source inventory.
`MemberAccessException`, `MethodAccessException`, `MissingMemberException`,
`MissingFieldException`, and `MissingMethodException` are audited against a
complete plural/singular 61/61 filter. Their inline constructor chains assign and override the
documented HResults correctly, and ordinary class/member diagnostic formats
pass exact assertions. No standalone defect was confirmed; reports preserve
missing empty/UTF-8 name, stored-inner identity, and native-reflection-boundary
coverage. No production or test source changed. Resume the next complete
Core.Base exception/source inventory.
`ApplicationException`, `AppDomainUnloadedException`,
`BadImageFormatException`, `CannotUnloadAppDomainException`, and
`DataMisalignedException` are audited against a complete 43/43 family filter.
New medium SR-AUD-094: none assigns its derived HResult, leaving one at
`COR_E_EXCEPTION` and four at `COR_E_SYSTEM` instead of their five documented
codes. The local .NET source plus `/tmp/sharp-runtimervc-exception-hresult-audit-probe`
reproduce every mismatch; CCF-016 links it to SR-AUD-093. No production or test
source changed. Resume the next complete Core.Base exception/source inventory.
`TypeLoadException`, `TypeAccessException`, `TypeUnloadedException`,
`DllNotFoundException`, and `EntryPointNotFoundException` are audited against
a complete 48/48 family filter. New medium SR-AUD-095: the Dll and entry-point
derivatives retain `COR_E_TYPELOAD` (`0x80131522`) rather than their distinct
derived codes; local .NET source and the shared HResult probe reproduce both.
The three base/sibling types correctly set their codes. CCF-016 now covers the
repeated constructor audit gap. No production or test source changed. Resume
the next complete Core.Base exception/source inventory.
`AccessViolationException`, `ContextMarshalException`,
`InsufficientExecutionStackException`, `InvalidCastException` (declaration and
implementation), and `InvalidProgramException` are audited against a focused
32/32 filter. New medium SR-AUD-096: AccessViolation and ContextMarshal leave
the base `COR_E_SYSTEM` HResult rather than .NET's `E_POINTER` and
`COR_E_CONTEXTMARSHAL`; local source and the shared probe reproduce both.
The other three types correctly set their codes. CCF-016 now covers the
additional pair. No production or test source changed. Resume the next complete
Core.Base exception/source inventory.
`MulticastNotSupportedException`, `NotFiniteNumberException`,
`PlatformNotSupportedException`, `RankException`, and `StackOverflowException`
(declaration and implementation) are audited against a focused 29/29 filter.
All reviewed constructors set their documented HResults; no standalone defect
was confirmed. The reports preserve missing all-overload HResult, special
floating-value, stored-inner, native delegate/rank/platform, and actual
stack-overflow integration diagnostics. No production or test source changed.
Resume the next complete Core.Base exception/source inventory.
`AggregateException` and its 13-test direct fixture are audited. The filter
passes 13/13, but isolated probes confirm high SR-AUD-097: a null public inner
`exception_ptr` enters `std::rethrow_exception` and segfaults. Medium
SR-AUD-098 records loss of first-inner state, custom diagnostic text, and .NET
Flatten leaf order; medium SR-AUD-099 records empty Handle predicates deferred
to `std::bad_function_call`. CCF-011 now includes that empty-callable path. No
production or test source changed. Resume the next complete Core.Base
exception/source inventory.
`DuplicateWaitObjectException`, `ExecutionEngineException`, `FormatException`
(declaration and implementation), `TimeoutException` (declaration and
implementation), `UnauthorizedAccessException` (declaration and
implementation), and `TypeInitializationException` are audited against a
38/38 filter. New medium SR-AUD-100: DuplicateWaitObject retains generic
`COR_E_ARGUMENT` rather than `COR_E_DUPLICATEWAITOBJECT` and has a divergent
default wait-array diagnostic; the shared probe and local .NET source reproduce
it. CCF-016 extends accordingly. No production or test source changed. Resume
the next complete Core.Base exception/source inventory.
`System::IO::IOException` (declaration and implementation),
`DirectoryNotFoundException` (declaration and implementation), and
`Security::Cryptography::CryptographicException` are audited. Their focused
filter selects 0 tests, while the shared probe verifies existing default
HResults. New medium SR-AUD-101 records absent public IOException custom-HResult,
DirectoryNotFound path-plus-inner, and CryptographicException composite-format
overloads. No production or test source changed. Resume the next complete
Core.Base exception/source inventory.
`Progress<T>` and its dedicated tests are audited. Its focused filter passed
9/9, but a standalone probe confirms new medium SR-AUD-058: an empty added
event-style handler is stored and later throws `std::bad_function_call`, unlike
.NET's nullable event-delegate no-op. Resume the next complete Core.Base
value-type or primitive/test inventory.
The adjacent `IAsyncDisposable`, `IAsyncResult`, `ICloneable`, `IComparable`,
and `ICustomFormatter` declarations plus their shared 12-test interface fixture
are audited. The focused Core.Base filter passed 12/12 with no new
evidence-backed finding. Reports preserve the missing completed-task,
asynchronous-result state transition, comparison-extrema, clone-depth, and
typed-custom-formatting assertions. Resume the next complete Core.Base
interface or primitive/test inventory.
`Index`, `Range`, and their two dedicated test sources are audited. Their
combined Core.Base filter passed 40/40, but an independent UBSan probe confirms
new high SR-AUD-057: an end-based maximal Index plus `INT_MIN` length causes
both `Index::GetOffset` and Range's resolved-length subtraction to execute
signed C++ overflow instead of .NET's defined unchecked arithmetic. Resume the
next complete Core.Base value-type or primitive/test inventory.
`Nullable<T>` and its dedicated test source are audited. Their direct focused
filter passed 24/24 (47/47 including the duplicate smoke cases in the pending
large test file), but a standalone NaN probe extends SR-AUD-046: raw `<` makes
nullable NaN compare equal to a finite value, and raw optional equality makes
NaN unequal to itself instead of using .NET's default comparer and equality
comparer. Resume the next complete Core.Base value-type or primitive/test
inventory.
`IConvertible`, `DBNull`, and the focused `DBNullTests.cpp` source are
audited. Core.Base and integration DBNull filters passed 9/9 and 11/11,
respectively, with no new evidence-backed finding. The documented
culture-invariant IConvertible adaptation and DBNull singleton/ref-return
boundary remain explicit review notes. Resume the next complete Core.Base
interface or primitive/test inventory.
`IEquatable`, `IDisposable`, and their two direct test sources are now
audited; their focused Core.Base filter passed 22/22 with no new
evidence-backed finding. The reports note that the nominal shared-pointer reset
test explicitly disposes before destruction and that its repeated-dispose
counter does not test resource idempotence. Resume the next complete Core.Base
interface or primitive/test inventory.
`AppContext`, `AppDomain` (declaration and implementation), `AppDomainSetup`,
and the dedicated setup fixture are now audited. The combined adjacent filter
passes 11/11, but the direct probe proves that AppContext named data cannot
configure the base directory or a compatibility switch (medium SR-AUD-102);
AppDomain then discards data/switch state instead of forwarding to AppContext
(medium SR-AUD-103). `ApplyPolicy` also accepts empty and NUL-containing names
that current .NET rejects (medium SR-AUD-104). The reports retain all silent
event-stub and platform-path fallback assertion gaps. No production or test
source changed. Resume the next coherent Core.Base runtime/configuration
source inventory.
The existing `Environment` declaration, implementation, and complete 99-test
fixture reports are now strengthened with direct .NET comparison. The direct
filter is green, but the isolated probe confirms four medium defects: Unix
special folders ignore XDG/option/error behavior
(SR-AUD-105); empty environment values delete their key (SR-AUD-106); valid
4,866-byte current directories become empty strings (SR-AUD-107); and raw
command-line concatenation loses argument quoting (SR-AUD-108). No production
or test source changed. Resume the next coherent Core.Base runtime source
inventory.
`GC`, `GCCollectionMode`, `GCGenerationInfo`, `GCNotificationStatus`, the two
historical forwarding headers, and the complete 61/61 direct fixture are now
audited. The RAII/no-tracing-GC boundary is explicit: Collect, metrics,
pressure, finalizer, and no-GC-region APIs are consistent no-op/zero adapters,
and notification waits correctly return `NotApplicable`. No new classified
defect or source/test change resulted. Resume the next coherent Core.Base
runtime source inventory.
`Activator`, `RuntimeTypeHandle`, `RuntimeType`, and their two dedicated
runtime-type fixtures are now audited. The combined direct filter passes
16/16, but a direct construction probe confirms medium SR-AUD-109: value
Activator uses braced initialization and changes initializer-list-capable
constructor arguments. SR-AUD-110 records that the public RuntimeType enum
occupies the name of an unrelated internal .NET reflection class. No production
or test source changed. Resume the next coherent Core.Base runtime source
inventory.
`ModuleHandle`, `RuntimeArgumentHandle`, `RuntimeFieldHandle`, and
`RuntimeMethodHandle` are now audited. Their combined existing filter passes
19/19, but it masks a direct-header compilation failure: ModuleHandle defines
`ResolveTypeHandle` before its RuntimeTypeHandle return type is complete
(medium SR-AUD-111). The remaining no-metadata/no-varargs adapters are
explicitly documented. No production or test source changed. Resume the next
coherent Core.Base runtime source inventory.
`ArgIterator`, `TypedReference`, and the complete Batch12 arg-handle fixture
are now audited. The direct filter passes 11/11, but medium SR-AUD-112 records
that five ArgIterator tests call non-static methods through reinterpreted
character storage whose object lifetime never began. TypedReference's
intrinsic/reflection omission remains explicit. No production or test source
changed. Resume the next coherent Core.Base runtime source inventory.
`AssemblyLoadEventArgs`, ThreadStatic/STA/MTA marker attributes, and their
three dedicated fixtures are now audited. The marker filter passes 18/18, but
medium SR-AUD-113 confirms that ThreadStaticAttribute has no C++ attachment or
`thread_local` storage mechanism despite claiming per-thread field values.
Assembly-load string payloads and the STA/MTA no-effect boundary are explicit.

Audit checkpoint 2026-07-26 20:10: 405/1748 mirrored reports. Attribute
base/targets/usage declaration and source, ten related attribute headers, and
eight complete direct fixtures audited; the focused Core.Base attribute filter
passed 77/77. New SR-AUD-114: Attribute is constructible and gives all
unoverridden derived attributes identity/address equality rather than .NET's
abstract fieldwise contract. New SR-AUD-115/116: ObsoleteAttribute cannot
affect a declaration/compiler diagnostic and erases nullable string state. New
low SR-AUD-117: deprecated LoaderOptimization values have only Doxygen, not
C++ compiler, deprecation. Explicit context/serialization/params/reflection
marker limitations remain documented adaptations. No source/test changes.
Resume a complete remaining Core.Base header/source/test group after refreshing
its inventory.

Audit checkpoint 2026-07-26 20:30: 413/1748 mirrored reports. Delegate,
MulticastDelegate, MulticastAction, Delegate implementation, and four complete
direct/mixed fixtures audited. Delegate filter passed 70/70; the adjacent
Batch14 filter passed 25/25. The compiled probe confirms SR-AUD-118 through
SR-AUD-120: Combine/Remove lose concrete type and accept a mismatched subtype;
separately allocated equal multicast entries compare unequal; and Remove cannot
remove a multi-entry invocation-list subsequence. MulticastAction's tokenized
event-field adaptation has no standalone confirmed defect. No source/test
changes. Resume a complete remaining Core.Base header/source/test group after
refreshing its inventory.

Audit checkpoint 2026-07-26 20:50: 419/1748 mirrored reports. EventArgs and
EventHandler declaration/source plus their full dedicated fixtures audited;
the focused filter passed 32/32. A runtime probe confirms that EventHandler
stores an empty callable and Raise later throws std::bad_function_call
(SR-AUD-121, extending CCF-011). A negative compile probe confirms its const
event-argument signature rejects a mutable event-data handler (SR-AUD-122).
No source/test changes. Resume Resolve/Unhandled event arguments and aliases,
or another complete Core.Base header/source/test group after inventory review.

Audit checkpoint 2026-07-26 21:10: 426/1748 mirrored reports. Resolve and
UnhandledException event arguments/handlers plus three full direct/mixed
fixtures audited; selected event filter passed 33/33. New SR-AUD-123:
ResolveEventHandler requires a string result and lacks a distinct nullable
“not resolved” state even under the string/reflection adaptation. Unhandled
exception payload and sender adaptations are explicit; AppDomain dispatch
remains covered by SR-AUD-103. No source/test changes. Resume another complete
Core.Base header/source/test group after refreshing its inventory.

Audit checkpoint 2026-07-26 21:30: 430/1748 mirrored reports. ApplicationId
and ApplicationIdentity plus their dedicated fixtures audited; focused filter
passed 22/22. New SR-AUD-124/125: ApplicationId loses byte/null-aware identity
modeling and required name validation, while ToString omits its token and uses
a different grammar. ApplicationIdentity remains a documented legacy/reflection
adaptation with no separate finding. No source/test changes. Resume another
complete Core.Base group after inventory review.

Audit checkpoint 2026-07-26 21:50: 444/1748 mirrored reports. Seven enum
headers and seven direct fixtures audited; combined filter passed 42/42. Values
and flag operators match local .NET; SR-AUD-036 remains a consumer validation
issue for MidpointRounding and no new finding was classified. No source/test
changes. Resume another complete Core.Base group after inventory review.

Audit checkpoint 2026-07-26 22:10: 450/1748 mirrored reports. Converter,
Predicate, and Func declarations plus their three full direct fixtures audited;
the combined filter passed 17/17. C++ probe output confirms that `Func<void>`
and `Converter<int, void>` compile and are exact Action aliases, while the
counterpart C# probe fails with CS1547. New medium SR-AUD-126 records that
unconstrained C++ result types collapse .NET's separate Action category. No
source/test changes. Resume another complete Core.Base header/source/test
group after inventory review.

Audit checkpoint 2026-07-26 22:30: 454/1748 mirrored reports. DateTimeKind,
DayOfWeek, and CrashReason headers plus the complete CrashReason fixture
audited; their combined filter passed 17/17. The two Date/Day focused sections
remain in the not-yet-complete `SystemTypesRemainingTests.cpp` and therefore
do not mark that source audited. Values match .NET, but new medium SR-AUD-127
records that a public top-level CrashReason copies an internal nested NativeAOT
enum despite having no first-party production consumer. No source/test changes.
Resume another complete Core.Base header/source/test group after inventory
review.

Audit checkpoint 2026-07-26 22:50: 459/1748 mirrored reports.
ContextBoundObject, MarshalByRefObject, and LocalDataStoreSlot plus two
complete direct fixtures audited; the selected filter passed 14/14. A C++
probe confirms direct MarshalByRefObject construction and a child write
overwriting the parent LocalDataStoreSlot value; the C# counterpart rejects
base construction as abstract (CS0144). New medium SR-AUD-128/129 cover the
base's missing abstract/obsolete-public shape and the non-thread-local slot
with no Thread API. The existing mixed Batch3 report now records the prior
test that locks base construction. No source/test changes. Resume another
complete Core.Base header/source/test group after inventory review.

Audit checkpoint 2026-07-26 23:10: 461/1748 mirrored reports. Inline
Diagnostics::Stopwatch and its complete fixture audited; focused filter passed
20/20. The UBSan probe prints the published 10 MHz frequency then confirms
signed overflow for GetElapsedTime(INT64_MIN, INT64_MAX). New medium
SR-AUD-130 records its fabricated 100-ns timer unit versus .NET Unix's native
1 GHz timestamp frequency; new high SR-AUD-131 records the reachable signed
overflow. No source/test changes. Resume another complete Core.Base
header/source/test group after inventory review.

Audit checkpoint 2026-07-26 23:30: 463/1748 mirrored reports.
TryWriteInterpolatedStringHandler and its full direct fixture audited; focused
filter passed 13/13. Format probe prints bool=1, hex=255, and double=3.140000;
ASan confirms a positive-length null destination writes through null and exits
134. New high SR-AUD-132 covers the raw-pointer crash; new medium SR-AUD-133
covers ignored formats and hardcoded non-.NET value text. No source/test
changes. Resume another complete Core.Base header/source/test group after
inventory review.

Audit checkpoint 2026-07-26 23:50: 465/1748 mirrored reports. Header-only
Linq and its full direct fixture audited; focused filter passed 45/45. Probe
confirms empty callbacks return normal results for empty vectors but later
throw std::bad_function_call, raw float comparison rejects/duplicates NaN and
misses a late NaN minimum, and UBSan confirms Sum(INT_MAX,1) signed overflow.
New medium SR-AUD-134 covers callback validation; new high SR-AUD-135 covers
Sum overflow; SR-AUD-046 and CCF-010/011 now include LINQ. No source/test
changes. Resume another complete Core.Base header/source/test group after
inventory review.

Audit checkpoint 2026-07-26 23:59: 469/1748 mirrored reports. Void and
UnitySerializationHolder plus both complete direct fixtures audited; combined
singular-suite filter passed 12/12. Local C# rejects ordinary Void construction,
ToString, and generic use with CS0673, while C++ tests lock in all three. New
medium SR-AUD-136 records that false generic/value contract; new medium
SR-AUD-137 records UnitySerializationHolder's replacement of internal
serialization-only data/signatures with a public raw-code object. No source/
test changes. Resume another complete Core.Base header/source/test group after
inventory review.

Audit checkpoint 2026-07-26 23:59:30: 475/1748 mirrored reports. Six complete
direct exception fixtures (Arithmetic, Overflow, Format, NotImplemented,
NotSupported, and PlatformNotSupported) audited; their exact suite filter
passed 36/36. No new production defect is classified: reports record concrete
HResult, inner-exception, null/UTF-8, and real consumer-route assertion gaps.
PlatformNotSupported's existing header report was corrected to reflect its
three-constructor HResult test. No source/test changes. Resume another complete
Core.Base header/source/test group after inventory review.

Audit checkpoint 2026-07-26 23:59:45: 481/1748 mirrored reports. Six complete
exception fixtures (CannotUnloadAppDomain, DataMisaligned, ContextMarshal,
ExecutionEngine, MemberAccess, and MulticastNotSupported) audited; selected
filter passed 31/31, including three duplicate CannotUnload suite cases from
another source. No new finding: direct CannotUnload/DataMisaligned tests now
document SR-AUD-094's missing HResults, ContextMarshal documents SR-AUD-096,
and the other three fixtures correctly assert their HResults. No source/test
changes. Resume another complete Core.Base header/source/test group after
inventory review.

Audit checkpoint 2026-07-26 23:59:55: 487/1748 mirrored reports. Six complete
runtime exception fixtures (ArrayTypeMismatch, Rank, OutOfMemory,
NullReference, SystemException, and TypeUnloaded) audited; selected filter
passed 40/40. No new finding: ArrayTypeMismatch direct text tests omit the
existing SR-AUD-093 HResult, while the other five verify their important
HResult paths. No source/test changes. Resume another complete Core.Base
header/source/test group after inventory review.

Audit checkpoint 2026-07-27 00:10:00: 491/1748 mirrored reports. The complete
BadImageFormat fixture and three shared exception fixture sources were audited;
the selected AppDomainUnloaded/BadImageFormat/DllNotFound/
DuplicateWaitObject/EntryPointNotFound filter passed 33/33. No new finding:
those tests leave confirmed SR-AUD-094, SR-AUD-095, and SR-AUD-100 HResult
diagnostics unguarded. No source/test changes. Resume another coherent
Core.Base header/source/test group after inventory review.

Audit checkpoint 2026-07-27 00:20:00: 497/1748 mirrored reports. Six complete
member/type-access exception fixture sources were audited; the selected filter
passed 58/58. No new finding: MissingField/MissingMember/MissingMethod,
MethodAccess, and InvalidOperation assert their derived HResults; TypeAccess
asserts its default HResult while header review covers the remaining overloads.
No source/test changes. Resume another coherent Core.Base header/source/test
group after inventory review.

Audit checkpoint 2026-07-27 00:30:00: 499/1748 mirrored reports. Two complete
TypeLoad/TypeInitialization fixture sources were audited; selected filter
passed 25/25. No new finding: TypeLoad checks default HResult while header
review covers its remaining overloads, and TypeInitialization checks its sole
public constructor's HResult and retrievable inner cause. No source/test
changes. Resume another coherent Core.Base header/source/test group after
inventory review.

Audit checkpoint 2026-07-27 00:40:00: 501/1748 mirrored reports. UriBuilder
and its complete direct fixture were audited; all 27 direct tests pass. A
C++/current-.NET probe confirms four new medium findings: copied credentials
fuse UserName/Password, relative input serializes as invalid `:///`, raw
equality/hash disagree with Uri identity, and Scheme/IPv6 Host setters skip
required normalization/validation (SR-AUD-138 through SR-AUD-141). No
source/test changes. Resume the next coherent Uri header/source/test group.

Audit checkpoint 2026-07-27 00:50:00: 504/1748 mirrored reports. Uri public
header, implementation, and full direct fixture were audited; all 57 tests
pass. C++/current-.NET probes confirm four new medium findings: raw
case/default-port identity, opaque mailto port loss, incorrect query/fragment/
network-path base resolution, and malformed IPv6/invalid UriKind acceptance
(SR-AUD-142 through SR-AUD-145). No source/test changes. Resume the next
coherent Uri header/source/test group.

Audit checkpoint 2026-07-27 01:00:00: 506/1748 mirrored reports. UriParser and
its complete direct fixture were audited; all 14 tests pass. Current-.NET and
C++ probes confirm two new medium findings: the public custom-parser
registration/participation contract is absent and protected hooks are public
stubs, while IsKnownScheme accepts invalid input rather than throwing
(SR-AUD-146 and SR-AUD-147). No source/test changes. Resume the next coherent
Uri header/source/test group.

Audit checkpoint 2026-07-27 01:10:00: 510/1748 mirrored reports. UriTypeConverter,
UriFormatException, and both complete direct fixtures were audited; selected
filter passed 13/13. One new medium finding: the converter throws for empty
text where current .NET returns null, and its non-nullable C++ return plus test
lock that behavior (SR-AUD-148). No source/test changes. Resume the next
coherent Uri header/source/test group.

Audit checkpoint 2026-07-27 01:20:00: 524/1748 mirrored reports. Seven Uri
value-type headers and all seven direct fixtures were audited; selected filter
passed 38/38. Three new medium API findings: UriCreationOptions has no Uri
consumer, UriPartial has no GetLeftPart, and UriHostNameType has no
CheckHostName classifier (SR-AUD-149 through SR-AUD-151). Other audited enum
values match .NET. No source/test changes. Resume the next coherent Uri
header/source/test group.

Audit checkpoint 2026-07-27 01:30:00: 525/1748 mirrored reports. The concise
Uri module README is audited and its Core.Base dependency claim is accurate.
All 27 eligible Uri component files now have mirrored evidence reports. No new
finding or source/test change. Resume the next unreviewed runtime module.

Audit checkpoint 2026-07-27 01:40:00: 530/1748 mirrored reports. Architecture,
OSPlatform, RuntimeInformation declaration/implementation, and their shared
fixture were audited; selected filter passed 11/11. Three new medium findings:
OSPlatform cannot represent default, RuntimeInformation omits two public
identity properties, and Windows OSArchitecture aliases process architecture
(SR-AUD-152 through SR-AUD-154). No source/test changes. Resume the next
coherent Runtime header/source/test group.

Audit checkpoint 2026-07-27 01:50:00: 532/1748 mirrored reports. NativeMemory
and its complete direct fixture were audited; all 20 tests pass. The POSIX
AlignedRealloc old-size bounded-copy repair is confirmed in source and no new
defect is classified; reports record AllocZeroed overflow, reallocation, and
near-limit alignment assertion gaps. No source/test changes. Resume another
coherent Runtime header/source/test group.

Audit checkpoint 2026-07-27 02:00:00: 534/1748 mirrored reports.
ExceptionDispatchInfo and its complete direct fixture were audited; all 4 tests
pass. Current-.NET/C++ probes confirm new medium SR-AUD-155: null exception_ptr
is accepted and deferred to an undefined rethrow path instead of immediate
ArgumentNullException. No source/test changes. Resume another coherent Runtime
header/source/test group.

Audit checkpoint 2026-07-27 02:10:00: 536/1748 mirrored reports. GCSettings
and the complete aggregate Runtime fixture were audited; the direct GC filter
passed 9/9 and the full shared filter passed 82/82. Current-.NET source/C++
probe confirms new medium SR-AUD-156: both setters persist invalid enum casts
instead of rejecting them, and callers can set NoGCRegion despite its
runtime-owned current-.NET state. No source/test changes. Resume another
coherent Runtime header/source/test group.

Audit checkpoint 2026-07-27 02:20:00: 538/1748 mirrored reports.
AmbiguousImplementationException and ExternalException were audited; their
shared 7-test filter passes. Current-.NET/C++ probes confirm three new medium
findings: both retain COR_E_SYSTEM instead of their derived HResults,
Ambiguous has the wrong catch hierarchy and no inner-cause constructor, and
External omits error-code construction/ErrorCode diagnostics (SR-AUD-157
through SR-AUD-159). No source/test changes. Resume another coherent Runtime
header/source/test group.

Audit checkpoint 2026-07-27 02:30:00: 542/1748 mirrored reports. Caller,
CompilerGenerated, StateMachine, and CompilerFeatureRequired compiler-services
metadata headers were audited; their shared filter passed 9/9. Three headers
make their compiler-unconsumed native adaptation explicit. One low finding:
the direct CompilerFeatureRequired fixture locks a freely mutable C++
IsOptional setter where current .NET permits the property only during
initialization (SR-AUD-160). No source/test changes. Resume another coherent
Runtime header/source/test group.

Audit checkpoint 2026-07-27 02:40:00: 551/1748 mirrored reports. The remaining
nine small compiler-marker/derived-state-machine headers were audited against
current .NET source. Their shared fixture was already green (StateMachine 2/2,
metadata markers 1/1); all declare their compiler-unconsumed C++ adaptation
explicitly and have no production consumer. No new finding or source/test
change. Resume another coherent Runtime header/source/test group.

Audit checkpoint 2026-07-27 02:50:00: 554/1748 mirrored reports.
MethodImplOptions, MethodCodeType, and MethodImplAttribute were audited; the
targeted group passed 10/10. Enum values, raw-short conversion, and mutable
MethodCodeType match current .NET's representable metadata shape; no new
finding. Reports record untested flags/unknown values and the explicit absence
of native code-generation consumption. No source/test changes. Resume another
coherent Runtime header/source/test group.

Audit checkpoint 2026-07-27 03:00:00: 555/1748 mirrored reports.
ConditionalWeakTable was audited; direct tests passed 7/7. C++/managed probes
confirm two new medium findings: snapshot Reset rewinds and retains
non-current values although current .NET Reset is no-op and retains only
Current, and unconstrained C++ templates accept working scalar tables where
.NET requires reference types (SR-AUD-161 and SR-AUD-162). No source/test
changes. Resume another coherent Runtime header/source/test group.

Audit checkpoint 2026-07-27 03:10:00: 556/1748 mirrored reports.
RuntimeHelpers was audited; its direct filter passed 5/5. Native identity,
subarray, cleanup, and conservative-reference operations are coherent; CLR
metadata and stack/CER routes explicitly throw or document their native no-op
adaptation. No new finding or source/test change. Resume another coherent
Runtime header/source/test group.

Audit checkpoint 2026-07-27 03:20:00: 557/1748 mirrored reports.
VersioningAttributes was audited; all seven fixture suites passed 11/11.
Current-.NET/C++ probes confirm two medium findings: the public
OSPlatformAttribute hierarchy and common native consumer are absent, while
nullable/mutable metadata is collapsed into immutable constructor strings
(SR-AUD-163 and SR-AUD-164). No source/test changes. Resume another coherent
Runtime header/source/test group.

Audit checkpoint 2026-07-27 03:30:00: 558/1748 mirrored reports.
InteropAttributes was audited; all eleven fixture suites passed 23/23.
C++/managed probes and source comparison confirm four medium findings:
UnmanagedType has a wrong/missing value surface; StructLayout/DllImport defaults
diverge; MarshalAs/COM metadata is truncated and retyped; and all attributes
are detached from native declaration/marshalling/PInvoke behavior (SR-AUD-165
through SR-AUD-168). No source/test changes. Resume another coherent Runtime
header/source/test group.

Audit checkpoint 2026-07-27 03:40:00: 563/1748 mirrored reports.
PosixSignal, PosixSignalContext, PosixSignalRegistration, its Unix source, and
the direct fixture were audited; the focused filter passed 9/9. Safe helper
processes confirm four findings: registration drops a prior signal action and
never restores it, rejects current-.NET-supported positive raw Unix signals,
stops a child for non-cancelled SIGTSTP where current .NET does not, and can
block its raw handler on a full default-blocking pipe (SR-AUD-169 through
SR-AUD-172). No source/test changes. Resume another coherent Runtime
header/source/test group.

Audit checkpoint 2026-07-27 03:50:00: 565/1748 mirrored reports.
SerializationInfo and StreamingContext were audited; their direct shared
filter passed 3/3 and a warnings-as-errors standalone compile passed. Their
near-empty C++ surfaces are explicitly documented permanent legacy-serialization
stubs with no production consumer, so no new finding was classified. No
source/test changes. Resume the next coherent module source/test group.

Audit checkpoint 2026-07-27 04:00:00: 567/1748 mirrored reports.
Runtime's CMake registration and README were audited; boundary validation
passed with 41 modules/90 edges and the generated catalogue is current. Their
dependency and component claims match. All 518 eligible Runtime files now
have mirrored reports; no new finding or source/test change. Resume the next
coherent module source/test group.

Audit checkpoint 2026-07-27 04:10:00: 568/1748 mirrored reports.
Uri's final CMake registration was audited; boundary validation passed with 41
modules/90 edges and the generated catalogue is current. The static target and
sole Core.Base public dependency match the catalogue. All 28 eligible Uri
files now have mirrored reports; no new finding or source/test change. Resume
the next coherent module source/test group.

Audit checkpoint 2026-07-27 04:20:00: 571/1748 mirrored reports.
CharUnicodeInfo, UnicodeCategory, and CharTests2 were audited; the direct
filter passed 63/63. C++/managed probes confirm two medium findings: Unicode
numeric APIs recognize only ASCII/a few Latin-1 values, and the C-locale
classifier maps ordinary assigned BMP characters to OtherNotAssigned
(SR-AUD-173 and SR-AUD-174). The documented non-BMP mapping limitation remains
an adaptation, not a duplicate finding. No source/test changes. Resume the
next coherent Core.Base source/test group.

Audit checkpoint 2026-07-27 04:30:00: 572/1748 mirrored reports.
BFloat16 was audited; existing BitConverterTests passed 67/67 and a C++20
bit-level probe passed. Two medium findings: float construction truncates
instead of current .NET round-to-nearest-even, and the public type omits most
of the managed numeric/parse/format/conversion surface (SR-AUD-175 and
SR-AUD-176). No source/test changes. Resume the next coherent Core.Base
source/test group.

Audit checkpoint 2026-07-27 04:40:00: 574/1748 mirrored reports.
NumberStyles and the shared integer parser were audited; NumberStylesExtended
passed 43/43. C++/managed probes confirm two medium cross-wrapper findings:
AllowExponent is ignored though current .NET integer parsing accepts it, and
unknown/incompatible style masks are accepted rather than rejected with
ArgumentException (SR-AUD-177 and SR-AUD-178). No source/test changes. Resume
the next coherent Core.Base source/test group.

Audit checkpoint 2026-07-27 04:50:00: 579/1748 mirrored reports.
Experimental Property/ReadonlyProperty, portable floating from_chars, Prop,
and SharpRuntimeHelper were audited; their focused integration filter passed
8/8. Probes confirm high SR-AUD-180 (the old-Apple fallback scans beyond its
from_chars range), medium SR-AUD-179 (Property assignment returns stale cache),
and low SR-AUD-181 (advertised experimental auto/custom macros cannot compile).
No source/test changes. Resume the next coherent Core.Base source/test group.

Audit checkpoint 2026-07-27 05:00:00: 581/1748 mirrored reports.
EnvironmentVariableTarget and generic IEqualityComparer were audited;
EnvironmentTests passed 99/99 and the focused comparer filter passed 4/4.
Their public ordinal/interface contracts and direct consumers are coherent. No
new finding or source/test change. Resume the next coherent Core.Base
source/test group.

Audit checkpoint 2026-07-27 05:10:00: 583/1748 mirrored reports, 182
confirmed findings. StringNormalizationExtensions and NormalizationForm were
audited; the focused fixture passed 5/5 but covers only ASCII. C++/managed
FormC probes confirm SR-AUD-182: the C++ API reports decomposed `e` + U+0301
normalized and returns `65CC81`, while .NET reports false and returns `C3A9`.
The source documents its Unicode-table stub but does not mark the public API
unsupported. Audit-only; no source/test changes. Resume the next coherent
Core.Base source/test group.

Audit checkpoint 2026-07-27 05:20:00: 587/1748 mirrored reports, 182
confirmed findings. UnauthorizedAccessException, ObjectDisposedException,
TimeoutException, and StackOverflowException direct fixture sources were
audited; the focused filter passed 41/41 (including four previously audited
companion StackOverflow cases). Normal construction/HResult coverage is
coherent. Reports record missing inner-identity, null/UTF-8, non-default
HResult, producer-integration, and real-overflow assertions. No new finding
or source/test change. Resume the next coherent Core.Base source/test group.

Audit checkpoint 2026-07-27 05:30:00: 588/1748 mirrored reports, 182
confirmed findings. Int32NewTests was audited; its focused filter passed 9/9.
The MinValue magnitude checks and normal comparison/hash paths agree with
current .NET. The report records tautological hash, boundary, tie, parsing,
formatting, and cross-cutting SR-AUD-021/022 assertion gaps. No new finding or
source/test change. Resume the next coherent Core.Base source/test group.

Audit checkpoint 2026-07-27 05:40:00: 589/1748 mirrored reports, 182
confirmed findings. NotFiniteNumberExceptionTests was audited; its direct
filter passed 9/9. C++/managed probes confirm `80131528` for all six public
constructors, matching the fixture's expanded coverage. Reports retain NaN
payload/sign, inner identity, null/UTF-8, and real-producer assertion gaps. No
new finding or source/test change. Resume the next coherent Core.Base
source/test group.

Audit checkpoint 2026-07-27 05:50:00: 590/1748 mirrored reports, 182
confirmed findings. CultureInvariantFormattingTests was audited and ran 1/1
under an installed `en_US.utf8` locale. The reviewed stream paths explicitly
imbue `std::locale::classic()` and retain invariant output. The report records
the host-locale skip path and missing custom-facet/concurrency coverage. No
new finding or source/test change. Resume the next coherent Core.Base
source/test group.

Audit checkpoint 2026-07-27 06:00:00: 591/1748 mirrored reports, 182
confirmed findings. Batch13BufferTests was audited; its generic BlockCopy and
unsigned MemoryCopy filter passed 10/10. It covers checked primitive-vector
and overlap/capacity paths, but has no assertion for high SR-AUD-067 raw
negative metadata or SR-AUD-051 nontrivial generic-vector copying. No new
finding or source/test change. Resume the next coherent Core.Base source/test
group.

Audit checkpoint 2026-07-27 06:10:00: 595/1748 mirrored reports, 184
confirmed findings. Threading AsyncCallback, EventResetMode, WaitHandle, and
EventWaitHandle headers were audited; focused valid-mode/multi-wait tests
passed 9/9. C++/managed probes add SR-AUD-183 (empty/null collections and
invalid timeouts silently succeed/timeout/loop) and SR-AUD-184 (invalid event
mode accepted). No source/test changes. Resume Threading direct fixtures or
another coherent source/test group.

Audit checkpoint 2026-07-27 06:20:00: 597/1748 mirrored reports, 184
confirmed findings. Dedicated Threading AsyncCallbackTests and Core's mixed
MiscNewTests were audited; their focused filters passed 4/4 and 6/6. They
cover normal marker/callback construction but not real APM state transitions,
callback failures, or asynchronous wait/lifetime behavior. No new finding or
source/test change. Resume another coherent Threading/Core source/test group.

Audit checkpoint 2026-07-27 06:30:00: 598/1748 mirrored reports, 184
confirmed findings. Core Batch4Tests was audited; its six-suite focused filter
passed 21/21. The batch covers normal Resolve/APM/exception/GC stub behavior,
but omits SR-AUD-123 nullable resolution plus SR-AUD-094/096 HResult paths,
and real APM/GC telemetry integration. No new finding or source/test change.
Resume another coherent Core or Threading source/test group.

Audit checkpoint 2026-07-27 06:40:00: 599/1748 mirrored reports, 184
confirmed findings. Core Batch11ArrayTests was audited; its fifteen-suite
range/copy/search/read-only filter passed 38/38. It supplies broad normal int
vector coverage but omits all high raw/overlap/nontrivial Array paths and
medium float/callable diagnostics already recorded as SR-AUD-044/046/051/052.
No new finding or source/test change. Resume another coherent Core source/test
group.

Audit checkpoint 2026-07-27 06:50:00: 600/1748 mirrored reports, 184
confirmed findings. Core Batch15TypesTests was audited; its seven-suite filter
passed 59/59. It supplies normal Math/exception/type-handle coverage but locks
in SR-AUD-068's constructible identity ValueType and masks SR-AUD-111 through
include order. No new finding or source/test change. Resume another coherent
Core source/test group.

Audit checkpoint 2026-07-27 07:00:00: 603/1748 mirrored reports, 186
confirmed findings. IO BinaryData plus its Core and IO direct fixtures were
audited; filters passed 33/33 and 15/15. C++/managed evidence adds SR-AUD-185
(raw malformed UTF-8 text instead of replacement decoding) and SR-AUD-186
(ReadOnlyMemory snapshot instead of wrapper semantics). BinaryData's
nonnegative-hash test extends SR-AUD-018. No source/test changes. Resume IO
direct fixtures or another coherent source/test group.

Audit checkpoint 2026-07-27 07:10:00: 606/1748 mirrored reports, 186
confirmed findings. Threading WaitCallback, WaitOrTimerCallback, and
LockRecursionPolicy headers were audited; related registered-wait/ordinal
filter passed 4/4. Callback aliases are coherent native pointer/function
adaptations; reports record the absent direct state/timeout/error/lifetime
coverage. No new finding or source/test change. Resume Threading consumers or
another coherent source/test group.

Audit checkpoint 2026-07-27 07:20:00: 609/1748 mirrored reports, 189
confirmed findings. ThreadPool, RegisteredWaitHandle, and IThreadPoolWorkItem
were audited; focused Threading validation passed 10/10. ASan confirms
SR-AUD-187's raw-pointer work-item use-after-free, an isolated null
registration process core-dumps for SR-AUD-188, and C++/managed configuration
probes confirm SR-AUD-189's no-op success and invalid-input acceptance. No
source/test change. Resume Threading fixture sources or another coherent
source/test group.

Audit checkpoint 2026-07-27 07:30:00: 611/1748 mirrored reports, 189
confirmed findings. Batch9ThreadingTests and ThreadingRemainingTests were
fully audited; the complete Threading executable passed 359/359. The fixtures
cover several synchronized regression paths but leave SR-AUD-183/184 and
SR-AUD-187 through SR-AUD-189 invalid/lifetime/state boundaries unasserted.
No source/test change. Resume the remaining Threading source inventory.

Audit checkpoint 2026-07-27 07:40:00: 615/1748 mirrored reports, 189
confirmed findings. ApartmentState, ThreadPriority, ThreadState, and Timeout
were audited; the complete Threading executable remains green at 359/359.
Public values and both timeout constants match current .NET. Reports record
invalid-cast, consumer-state, and InfiniteTimeSpan assertion gaps. No
source/test change. Resume the remaining Threading source inventory.

Audit checkpoint 2026-07-27 07:50:00: 620/1748 mirrored reports, 191
confirmed findings. Timer, ITimer, TimeProvider, its source, and direct tests
were audited; complete Threading validation passed 359/359. UBSan extends
SR-AUD-131 to TimeProvider timestamp subtraction; C++/managed probes add
SR-AUD-190 empty Timer callback acceptance and SR-AUD-191 successful Change
after disposal. The fixture's raw-this Timer comment is obsolete; Timer holds
shared State. No source/test change. Resume the remaining Threading inventory.

Audit checkpoint 2026-07-27 08:00:00: 622/1748 mirrored reports, 194
confirmed findings. Thread header/source audited; complete Threading validation
remains 359/359. Native/managed probes add SR-AUD-192 empty-start termination,
SR-AUD-193 external CurrentThread ID collision, and SR-AUD-194 discarded
Start(void*) parameter. Shared RunState avoids the prior raw-owner UAF. No
source/test change. Resume Threading test fixtures or remaining sources.

Audit checkpoint 2026-07-27 08:10:00: 623/1748 mirrored reports, 195
confirmed findings. Batch8ThreadingTests was fully audited; complete Threading
validation remains 359/359. Low SR-AUD-195 records that its running-state
fallback masks the zero-valued Running flag and therefore accepts every state.
No source/test change. Resume ThreadingTests or remaining source inventory.

Audit checkpoint 2026-07-27 08:30:00: 630/1748 mirrored reports, 197
confirmed findings. ThreadingTests was fully audited; complete Threading
validation remains 359/359. Low SR-AUD-197 records that its CurrentThread ID
test discards the observed value, so it cannot protect SR-AUD-193. No
source/test change. Resume remaining Threading source inventory.

Audit checkpoint 2026-07-27 09:00:00: 636/1748 mirrored reports, 202
confirmed findings. Monitor audited; Threading remains 359/359. High
SR-AUD-202: recursive Wait retains one mutex level and a signaller deadlocks,
as confirmed by a two-second bounded child timeout. No source/test change.

Audit checkpoint 2026-07-27 09:10:00: 638/1748 mirrored reports, 202
confirmed findings. ReaderWriterLock and LockCookie audited; complete Threading
validation remains 359/359. Batch9 covers normal recursion, upgrade/downgrade,
release/restore, and timeout paths; reports retain contention, TimeSpan, forged
cookie, and address-reuse gaps. No source/test change. Resume remaining
Threading source inventory.

Audit checkpoint 2026-07-27 09:20:00: 639/1748 mirrored reports, 205
confirmed findings. ReaderWriterLockSlim direct filter passed 27/27, but TSan
confirms unsynchronized Dispose/entry (SR-AUD-203); direct .NET comparisons
also confirm disposal while held, queued-writer reader admission/starvation
(SR-AUD-204), and invalid recursion-policy reflection (SR-AUD-205). No
source/test change. Resume the remaining Threading source inventory.

Audit checkpoint 2026-07-27 09:30:00: 642/1748 mirrored reports, 208
confirmed findings. Mutex/Semaphore/SemaphoreSlim focused filter passed 22/22.
UBSan confirms shared Release signed overflow (SR-AUD-206); TSan confirms two
SemaphoreSlim public-operation races (SR-AUD-207); C++/managed comparison
confirms a no-op Mutex Close (SR-AUD-208). No source/test change. Resume the
remaining Threading source inventory.

Audit checkpoint 2026-07-27 09:40:00: 645/1748 mirrored reports, 209
confirmed findings. Reset-event focus passed 21/21. C++/.NET probes extend
no-op Close to Auto/ManualResetEvent (SR-AUD-208); a paired compile probe
confirms neither event is a local WaitHandle (SR-AUD-209); TSan extends
disposal-race SR-AUD-207 to ManualResetEventSlim. No source/test change.
Resume the remaining Threading source inventory.

Audit checkpoint 2026-07-27 09:50:00: 648/1748 mirrored reports, 212
confirmed findings. Barrier/Countdown focus passed 33/33. Bounded C++/.NET
probes confirm Barrier post-phase property deadlock (SR-AUD-210) and
CountdownEvent Reset(0) failing to wake a waiter (SR-AUD-211); TSan confirms
Barrier ParticipantCount and Countdown disposal races (SR-AUD-212 and
extension of SR-AUD-207). No source/test change. Resume remaining Threading
source inventory.

Audit checkpoint 2026-07-27 10:00:00: 653/1748 mirrored reports, 213
confirmed findings. Lock/SpinLock/SpinWait focus passed 32/32; default
SpinLock owner tracking agrees with .NET. C++/.NET probes add SR-AUD-213:
SpinWait accepts invalid -2 timeout and defers an empty callback to native
bad_function_call. No source/test change. Resume remaining Threading source
inventory.

Audit checkpoint 2026-07-27 10:10:00: 655/1748 mirrored reports, 213
confirmed findings. Interlocked/Volatile filter passed 12/12, and a TSan
four-thread integer plus release-publication probe completed with no
diagnostic. No new finding or source/test change. Resume remaining Threading
source inventory.

Audit checkpoint 2026-07-27 10:20:00: 661/1748 mirrored reports, 213
confirmed findings. OperationCanceledException filter passed 15/15 and the
related three exception filters passed 9/9. Construction/hierarchy paths are
coherent; reports retain null, inner-identity, HResult, producer, and named-
handle capability gaps. No new finding or source/test change. Resume remaining
Threading source inventory.

Audit checkpoint 2026-07-27 10:30:00: 664/1748 mirrored reports, 215
confirmed findings. AsyncLocal/ExecutionContext focused filter passed 10/10.
Direct C++/current-.NET probes add SR-AUD-214 (callback observes old local
value before commit) and SR-AUD-215 (null ExecutionContext::Run succeeds
instead of InvalidOperationException); AsyncLocalValueChangedArgs has no new
finding. Audit-only; no source/test changes. Resume remaining Threading source
inventory.

Audit checkpoint 2026-07-27 10:40:00: 667/1748 mirrored reports, 222
confirmed findings. LazyInitializer/ThreadLocal focus passed 16/16 and
SynchronizationContext focus passed 6/6. TSan confirms SR-AUD-216 (mixed
non-atomic/atomic Lazy target access) and SR-AUD-218 (ThreadLocal Dispose
race); ASan confirms SR-AUD-221 (dangling Current context). Direct C++/.NET
probes add SR-AUD-217, SR-AUD-219/220, and SR-AUD-222 for factory/property,
tracking, and Send callback divergences. Audit-only; no source/test changes.
Resume Threading CMake/README completion.

Audit checkpoint 2026-07-27 10:50:00: 669/1748 mirrored reports, 222
confirmed findings. Threading CMake/README audited; module-boundary validator
reported 41 modules/90 edges, the generated catalogue is current, and the
Threading target built successfully. No new finding or source/test change. All
72 eligible Threading module files now have mirrored reports; resume another
coherent module shard.

Audit checkpoint 2026-07-27 11:00:00: 671/1748 mirrored reports, 222
confirmed findings. Security.Cryptography.Random CMake/README audited;
module-boundary validator reports 41 modules/90 edges, generated catalogue is
current, and its static target builds. All five eligible module files now have
mirrored reports. No new finding or source/test change; resume another
coherent module shard.

Audit checkpoint 2026-07-27 11:10:00: 677/1748 mirrored reports, 223
confirmed findings. Legacy TimeZone header/source, its two exception headers,
and related fixtures audited; focused filters passed 15/15. New York C++/.NET
comparison adds SR-AUD-223: CurrentTimeZone freezes one current offset and
always reports no DST. Exception default HResults agree. Audit-only; no
source/test changes. Resume TimeZoneInfo core surface.

Audit checkpoint 2026-07-27 11:20:00: 681/1748 mirrored reports, 229
confirmed findings. TimeZoneInfo header/source and both fixtures audited;
focused filters passed 99/99. Direct C++/.NET probes add SR-AUD-224..229 for
stale TryFind output, factory/rule validation, case equality, rule identity,
and New York BaseUtcOffset. Explicit no-DST/serialization subset remains
documented adaptation. Audit-only; no source/test changes. Resume TimeZone
CMake/README completion.

Audit checkpoint 2026-07-27 11:30:00: 683/1748 mirrored reports, 229
confirmed findings. TimeZone CMake/README audited; module boundary validator
reports 41 modules/90 edges, generated catalogue is current, static target
built, and full fixture passed 114/114. All 12 eligible TimeZone files are now
mirrored. No new finding or source/test change; resume another coherent shard.

Audit checkpoint 2026-07-27 11:40:00: 703/1748 mirrored reports, 232
confirmed findings. All 21 eligible Threading.Tasks files are now mirrored;
the complete fixture passed 171/171. ASan confirms SR-AUD-230: a
TaskCanceledException retains a dangling Task pointer after a local task dies.
Direct current-.NET comparisons add SR-AUD-231 (empty delegates are deferred
as bad_function_call) and SR-AUD-232 (zero/less-than--1 parallel degrees run
instead of throwing). Audit-only; no source/test changes. Resume another
coherent module shard.

Audit checkpoint 2026-07-27 11:50:00: 710/1748 mirrored reports, 235
confirmed findings. All seven Threading.Channels files are now mirrored and
the full fixture passed 39/39. Direct C++/.NET comparisons add SR-AUD-233:
capacity zero behaves as a one-element buffer; SR-AUD-234: ReadAsync leaks a
completion error instead of ChannelClosedException with it as cause; and
SR-AUD-235: an invalid FullMode bypasses bounded capacity. Audit-only; no
source/test changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 12:00:00: 715/1748 mirrored reports, 235
confirmed findings. All five Collections.Async files are now mirrored and its
complete fixture passed 6/6. Synchronous shared_ptr enumerator and disposal
semantics are explicit native adaptations; no additional evidence-backed
finding or source/test change. Resume another coherent module shard.

Audit checkpoint 2026-07-27 12:10:00: 719/1748 mirrored reports, 235
confirmed findings. All four Storage files are now mirrored; focused
StoragePaths integration tests passed 2/2, component boundaries report 41/90,
and the static target builds. The platform-specific root-selection policy is
project-specific, with no new evidence-backed finding. Audit-only; no
source/test changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 12:20:00: 726/1748 mirrored reports, 235
confirmed findings. All seven Net.Mime files are now mirrored and its complete
fixture passed 26/26. Direct parser/setter probes agree with current .NET for
trailing separators and empty Boundary; documented MIME grammar/encoded-word
limits remain explicit adaptations. Audit-only; no source/test changes. Resume
another coherent module shard.

Audit checkpoint 2026-07-27 12:30:00: 734/1748 mirrored reports, 236
confirmed findings. All eight Net.Http.Json files are mirrored. Content-only
tests passed 6/6; two loopback HttpClient tests remain environment-blocked at
TcpListener socket creation. ASan/current-.NET comparison adds SR-AUD-236:
null HttpContent segfaults rather than yielding ArgumentNullException. No
source/test changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 12:40:00: 745/1748 mirrored reports, 237
confirmed findings. All eleven Collections.ObjectModel files are mirrored and
its complete fixture passed 124/124. ASan confirms SR-AUD-237: destroying a
ReadOnlyObservableCollection while its shared source survives leaves the
source with a callback that dereferences the dead wrapper on its next mutation.
No source/test changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 12:50:00: 753/1748 mirrored reports, 239
confirmed findings. All eight Timers files are mirrored and its complete
fixture passed 9/9. Native/current-.NET probes add SR-AUD-238 (a throwing
Elapsed handler aborts the C++ process instead of being contained) and
SR-AUD-239 (the event receives null instead of the Timer as sender). No
source/test changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 13:00:00: 761/1748 mirrored reports, 240
confirmed findings. All eight Net.Security files are mirrored and its complete
fixture passed 13/13. Generated TLS cipher-suite parity is 310/310; UBSan
adds SR-AUD-240 because hashing a valid 255-byte ALPN identifier uses signed
overflow undefined behavior. No source/test changes. Resume another coherent
module shard.

Audit checkpoint 2026-07-27 13:10:00: 771/1748 mirrored reports, 241
confirmed findings. All ten IO.IsolatedStorage files are mirrored; its static
library and the dependent IO fixture passed 527/527. A safe temporary-root
probe adds SR-AUD-241: an absolute POSIX path discards the C++ store root,
unlike current .NET's leading-separator normalization. No source/test changes.
Resume another coherent module shard.

Audit checkpoint 2026-07-27 13:20:00: 781/1748 mirrored reports, 242
confirmed findings. All ten IO.Compression.Zip files are mirrored; its library
built and focused integration ZIP fixture passed 38/38. Native/current-.NET
comparison adds SR-AUD-242: null Stream causes a native Read-mode SIGSEGV and
is silently retained in Create instead of producing ArgumentNullException. No
source/test changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 13:30:00: 794/1748 mirrored reports, 244
confirmed findings. All thirteen Console files are mirrored and its complete
fixture passed 123/123. Native/current-.NET comparison adds SR-AUD-243 (invalid
ConsoleColor is accepted) and SR-AUD-244 (negative cursor positions are stored
and emitted instead of rejected). No source/test changes. Resume another
coherent module shard.

Audit checkpoint 2026-07-27 13:40:00: 808/1748 mirrored reports, 245
confirmed findings. All fourteen Text.RegularExpressions headers/docs are
mirrored. Direct ASan/current-.NET comparison adds SR-AUD-245: a Match's
NextMatch continuation captures raw Regex this and dereferences it after the
Regex leaves scope. No source/test changes. Resume another coherent module
shard.

Audit checkpoint 2026-07-27 13:50:00: 824/1748 mirrored reports, 246
confirmed findings. All sixteen Security files are mirrored; the complete
fixture passed 38/38 and direct C++/current-.NET probes reviewed exception and
principal behavior. SR-AUD-246 confirms that explicit Unicode role membership
is reduced to bytewise ASCII lowercasing (`ÄDMIN`/`ädmin` false versus managed
true). VerificationException and CAS/transparency attributes remain recorded
ignored-surface placeholders. No source/test changes. Resume another coherent
module shard.

Audit checkpoint 2026-07-27 14:00:00: 831/1748 mirrored reports, 246
confirmed findings. The remaining seven Core.Base files are mirrored, closing
all 426 eligible Core.Base reports; complete fixture passed 4,946/4,946.
String declaration/tests retain SR-AUD-015/016 coverage gaps, DateTime
properties omit SR-AUD-006/007 boundaries, and the remaining mixed fixtures
are documented without a new independent defect. No source/test changes.
Resume another coherent module shard.

Audit checkpoint 2026-07-27 14:10:00: 848/1748 mirrored reports, 252
confirmed findings. All seventeen Net.WebSockets files are mirrored. Its target
built; 22/24 tests passed while two loopback cases are environment-blocked at
socket creation. ASan confirms SR-AUD-247 raw ClientWebSocket lifetime UAF;
C++/managed probes add SR-AUD-248 through SR-AUD-251 (header injection,
subprotocol grammar, lost inner exception, ignored cancellation), and source
reachability confirms SR-AUD-252 inert keep-alive options. No source/test
changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 14:20:00: 872/1748 mirrored reports, 252
confirmed findings. All twenty-four ComponentModel files are mirrored; the
dedicated fixture passed 98/98. Attributes, notification/event adapters,
change/init interfaces, and AsyncCompletedEventArgs are coherent in the
supported C++ subset. DataAnnotations and PropertyDescriptorCollection remain
explicit ignored metadata/stub surfaces. No source/test changes. Resume another
coherent module shard.

Audit checkpoint 2026-07-27 14:30:00: 895/1748 mirrored reports, 255
confirmed findings. All twenty-three Net.NetworkInformation files are mirrored.
Its target built and 28/39 tests passed; eleven live interface/ICMP cases are
blocked by sandbox permissions. C++/managed probes confirm delayed async
argument validation, sliced Ping inner exceptions, and fabricated default
reply options (SR-AUD-253 through SR-AUD-255). No source/test changes. Resume
another coherent module shard.

Audit checkpoint 2026-07-27 14:40:00: 920/1748 mirrored reports, 259
confirmed findings. All twenty-five IO.Compression files are mirrored; its
existing target passed 22/22. ASan/UBSan confirms unbounded raw negative-length
zlib input and null-stream dereference; native/current-.NET probes add invalid
mode/post-close behavior and inert compression strategy/options constructors
(SR-AUD-256 through SR-AUD-259). No source/test changes. Resume another
coherent module shard.

Audit checkpoint 2026-07-27 14:50:00: 945/1748 mirrored reports, 262
confirmed findings. All twenty-five IO.Hashing files are mirrored; its target
passed 96/96. ASan/UBSan confirms raw positive-null buffer dereferences and a
native probe confirms Adler/CRC negative lengths silently succeed; source
comparison confirms the XXH `LE` helpers are host-endian copies (SR-AUD-260
through SR-AUD-262). No source/test changes. Resume another coherent module
shard.

Audit checkpoint 2026-07-27 15:10:00: 974/1748 mirrored reports, 267
confirmed findings. All twenty-nine Net.Sockets files are mirrored. Its target
built; 54/88 tests passed and 34 socket/send-dependent cases are blocked by
sandbox permission. Native probes confirm negative SendPacketsElement count
coercion and silent invalid-NetworkStream descriptor I/O; source/current-.NET
comparison confirms raw async Socket lifetime and Tcp/Udp IPv4/argument gaps
(SR-AUD-263 through SR-AUD-267). No source/test changes. Resume another
coherent module shard.

Audit checkpoint 2026-07-27 15:30:00: 1008/1748 mirrored reports, 275
confirmed findings. All thirty-four Diagnostics files are mirrored; its target
passed 159/159. Direct native probes confirm Process negative-timeout,
destruction/zombie, redirected-restart, EINTR, and detached-process-group
defects (SR-AUD-268 through SR-AUD-273); source review identifies unsafe
multithreaded-fork child calls (SR-AUD-274); and TSan confirms the global
Debug provider replacement/write race (SR-AUD-275). Audit-only; no source/test
changes. Resume another coherent module shard.

Audit checkpoint 2026-07-27 15:50:00: 1031/1748 mirrored reports, 278
confirmed findings. All twenty-eight Numerics files are mirrored; its target
passed 299/299. Direct probes establish that zero Vector2/3/4 and Plane
normalization returns finite zero rather than .NET NaN propagation
(SR-AUD-276), `Complex::Abs` has the wrong public type and default formatting
is incompatible (SR-AUD-277), and published generic-math static interface
members compile but fail linkage (SR-AUD-278). Current .NET source verifies
that zero projection dimensions/aspect are not validation defects. Audit-only;
no source/test changes. Resume Globalization.

Audit checkpoint 2026-07-27 16:20:00: 1084/1748 mirrored reports, 285
confirmed findings. All fifty-three Globalization files are mirrored; its
target passed 676/676. Direct probes confirm UTF-8 byte/text-element confusion
and invalid-byte substring output (SR-AUD-279), an abstract-base Calendar
constructible fallback (SR-AUD-281), inert `IdnMapping::AllowUnassigned`
(SR-AUD-282), ignored Unicode comparison/casing semantics (SR-AUD-283/284),
and fabricated unknown culture/region data (SR-AUD-285). TSan confirms the
process-global CurrentCulture/CurrentUICulture race and cross-thread leakage
(SR-AUD-280). Audit-only; no source/test changes. Resume the next coherent
module shard.

Audit checkpoint 2026-07-27 08:40:00: 634/1748 mirrored reports, 199
confirmed findings. CancellationToken, CancellationTokenRegistration,
CancellationTokenSource, and source were audited; Threading remains 359/359.
Native/managed probes add SR-AUD-198 empty callback acceptance and ASan/UBSan
SR-AUD-199 public null-state token crash. No source/test change. Resume
remaining Threading source inventory.

Audit checkpoint 2026-07-27 08:50:00: 635/1748 mirrored reports, 201
confirmed findings. PeriodicTimer audited with no local fixture. Native probes
add SR-AUD-200 fractional period truncation and SR-AUD-201 double consumption
by concurrent waiters; current .NET documentation specifies one consumer. No
source/test change. Resume remaining Threading source inventory.

Audit checkpoint 2026-07-27 08:20:00: 629/1748 mirrored reports, 196
confirmed findings. ThreadStart and five Thread exception/event headers were
audited; complete Threading validation remains 359/359. Medium SR-AUD-196
records public ThreadStartException constructors that managed compilation
rejects as internal. ThreadStart extends SR-AUD-194; no event producer exists.
No source/test change. Resume ThreadingTests or remaining source inventory.
No production or test source changed. Resume the next coherent Core.Base
metadata/attribute source inventory.

## Findings recorded in this pass

- **SR-AUD-001 (medium):** the local selective script has ten fixtures,
  including `Collections.Blocking`, but the GitHub Actions matrix has only
  nine and omits that direct isolation check.
- **SR-AUD-002 (medium):** the architecture validator enforces substantially
  more invariants than its seven isolated negative fixtures cover; test-only
  dependency and allow-list branches lack direct regression tests.
- **SR-AUD-003 (low):** `BlockingCollection<T>` rejects negative fractional
  `TimeSpan` values that .NET truncates to the valid infinite-timeout value.
  Its polling-based removal of .NET's `WaitAny` collection-count ceiling is
  recorded as a documentation/parity decision rather than a confirmed bug.
- **SR-AUD-004 (low):** `source_header_inventory.py` reports only the number
  of `ported` tasks despite claiming a source-to-plan cross-reference.
- **SR-AUD-005 (medium):** `index_dotnet_types.py` destructively rebuilds a
  hardcoded database under a different `sharp-runtime` checkout.
- **SR-AUD-006 (high):** `DateTime` component constructors do not validate
  hour/minute/second/millisecond, so invalid input can normalize or bypass the
  documented tick range; DateTimeOffset inherits this path.
- **SR-AUD-007 (medium):** DateTime and DateTimeOffset parsing accepts
  malformed date/time or impossible offset minutes instead of returning false
  or throwing.
- **SR-AUD-008 (high):** `TimeSpan::TryParse` accepts a day count beyond the
  representable range and returns a wrapped negative duration; subtraction
  checks signed overflow only after evaluating the overflowing expression.
- **SR-AUD-009 (medium):** `TimeOnly::TryParse` accepts malformed fixed-format
  input such as a nonnumeric fractional suffix as a successful time.
- **SR-AUD-010 (high):** `Random::Shared` and `Guid::NewGuid` each return or
  use unsynchronised mutable static PRNG state; independent TSan probes confirm
  a C++ data race under normal concurrent use.
- **SR-AUD-011 (medium):** `Version::ToString(fieldCount)` emits the internal
  `-1` sentinel when Build or Revision is unspecified instead of throwing.
- **SR-AUD-012 (medium):** the valid full signed domain of
  `RandomNumberGenerator::GetInt32` reaches implementation-defined conversion
  and signed-overflow-prone arithmetic rather than a defined unsigned offset.
- **SR-AUD-013 (medium):** the nominal synchronous
  `SynchronizationContext::Send` test has no observable postcondition, so it
  passes even if the callback is never invoked.
- **SR-AUD-014 (medium):** compression integration tests recursively remove or
  overwrite fixed `/tmp` paths, making them non-isolated and unsafe to rerun in
  the presence of unverified existing artifacts.
- **SR-AUD-015 (medium):** the bespoke `String::Format` parser rejects valid
  escaped braces and accepts a stray closing brace as literal output.
- **SR-AUD-016 (medium):** four-argument substring `LastIndexOf` can return a
  match that extends past the requested `startIndex`/`count` range.
- **SR-AUD-017 (medium):** `Char::Parse` accepts malformed overlong UTF-8 as a
  valid BMP character despite documenting a `FormatException` for invalid UTF-8.
- **SR-AUD-018 (low):** Object and HashCode tests require distinct values to
  yield distinct/nonzero hash codes, even though the hash contract permits
  collisions and does not reserve zero.
- **SR-AUD-019 (high):** `Int128::TryParse` and decimal `ToString` negate
  `MinValue`, reaching UBSan-confirmed signed-overflow undefined behavior.
- **SR-AUD-020 (high):** `UInt128` forwards shift counts of 128+ to native
  shifts without .NET's modulo-128 mask, reaching sanitizer-confirmed UB.
- **SR-AUD-021 (medium):** audited 8/16/32/64/128-bit formatters silently
  accept unknown formats; 128-bit variants additionally leak `std::stoi`
  rather than raising `System::FormatException`. **Integer slice remediated
  #1847; float slice remediated #1849 (2026-07-30):** `Single`/`Double`
  `ToString(value, format)` now wrap the precision `std::stoi` in try/catch →
  `FormatException("Format specifier was invalid.")` and reject an unrecognised
  specifier loudly (was a silent round-trip). Both slices closed; CCF-006 closes
  with #1849. `N`/`E`/`G` value-fidelity gaps deferred to CCF-007.
- **SR-AUD-022 (medium):** Byte, SByte, Int16, UInt16, Int32, UInt32, Int64,
  UInt64, UInt128, and Decimal do not reject inverted Clamp bounds; 8/16/32/64-bit
  paths reach invalid `std::clamp` use while UInt128/Decimal select a bound.
- **SR-AUD-023 (medium):** SByte, Int16, UInt16, UInt32, UInt64, and UInt128
  silently return decimal for the .NET integral binary `B`/`b` format.
- **SR-AUD-024 (medium):** SByte and Int16 return false from `IsPositive(0)`
  even though .NET generic math defines the predicate as `value >= 0`; their
  suites assert the wrong value.
- **SR-AUD-025 (high):** IntPtr Add/Subtract perform signed pointer-width
  arithmetic before conversion, reaching UBSan-confirmed overflow for
  `MaxValue + 1` and `MinValue - 1` instead of defined unchecked wrap.
- **SR-AUD-026 (high):** several direct Convert integral overloads silently
  wrap negative/out-of-range input instead of throwing `OverflowException`.
  **Remediated — ticket #1853 (2026-07-30):** `ToChar(int)`, `ToByte(long)`,
  `ToUInt32(int)`, `ToUInt32(long)`, `ToUInt64(int)`, `ToUInt64(long)` now
  range-check and throw `OverflowException` (`ToChar` uses `[0,255]` for this
  1-byte-char port). Value-only, no UB; +26 tests.
- **SR-AUD-027 (high):** Convert's direct floating-to-integer paths allow NaN
  to bypass comparisons and return spurious platform values rather than throw.
  **Remediated — ticket #1853 (2026-07-30):** `ToInt32/ToInt64(double)`
  (`Convert.cpp`) and `ToUInt32/ToUInt64(double)` (inline `Convert.hpp`) now
  reject NaN and ±Inf with `OverflowException` via a `!std::isfinite` guard
  before the cast. **Premise corrected:** the NaN→int `static_cast` is genuine
  UB per `[conv.fpint]`, not merely implementation-defined — UBSan
  `float-cast-overflow` reproduced `nan is outside the range of representable
  values of type 'unsigned int'` at `Convert.hpp:390` pre-fix, clean post-fix.
  +15 tests.
- **SR-AUD-028 (medium):** Convert Base64 decoding accepts malformed padding
  and rejects whitespace that the .NET contract permits.
- **SR-AUD-029 (medium):** Single and Double `Round(value, digits)` accept
  precision outside their 0–6/0–15 ranges, producing a value or NaN instead of
  the required `ArgumentOutOfRangeException`.
- **SR-AUD-030 (medium):** Single and Double `IsPow2` reject valid subnormal
  powers of two, including `Epsilon`.
- **SR-AUD-031 (medium):** Single, Double, and Math `ILogB(NaN)` return the C
  library's `Int32.MinValue` sentinel instead of .NET's `Int32.MaxValue`
  non-finite result.
- **SR-AUD-032 (medium):** Single and Double Pi-scaled trigonometric methods
  use naive multiplication and lose exact zero/sign results at integer and
  half turns.
- **SR-AUD-033 (medium):** Single and Double parse/format delegate to a C++
  subset, rejecting valid default .NET input and producing incompatible `N`/`E`
  text; their invalid-format diagnostics also extend SR-AUD-021.
- **SR-AUD-034 (medium):** Single `IsPositive` rejects positive-sign NaN even
  though the .NET generic-math predicate uses only the sign bit.
- **SR-AUD-035 (medium):** Decimal's custom parser rejects default valid
  whitespace/grouping, turns range overflow into `FormatException`, and drops
  excess fractional precision instead of rounding to the nearest Decimal.
- **SR-AUD-036 (medium):** Decimal, Math, and MathF `Round` map an invalid
  public `MidpointRounding` value to an ordinary rounding result instead of
  throwing `ArgumentException`.
- **SR-AUD-037 (medium):** Decimal `ToOACurrency` truncates rather than rounds
  to the documented nearest four-decimal OLE Automation currency unit.
- **SR-AUD-038 (medium):** Decimal raw construction, parsing, and `CopySign`
  erase signed zero although `GetBits` makes that representation observable.
- **SR-AUD-039 (medium):** Math's double base-log path misses the base-one,
  zero, and positive-infinity special cases, returning infinity or signed zero
  instead of .NET's NaN.
- **SR-AUD-040 (medium):** MathF ties-to-even Round observes C++'s mutable
  floating-point mode; `FE_UPWARD` makes `Round(2.5f)` return `3` instead of
  `2`, unlike the sibling Math guard.
- **SR-AUD-041 (high):** BitConverter typed vector `To*` decoders have no
  index/remaining-width checks; ASan confirms both negative-index underflow
  and short-vector overflow reads through `ToInt32`. **Remediated — ticket
  #1851 (2026-07-30):** all 14 decoders now validate via a shared
  `validateDecodeRange` (`ArgumentOutOfRangeException("startIndex")` on
  negative/over-large index, `ArgumentException(value)` on insufficient width,
  before any read; `ToBoolean` throws only `ArgumentOutOfRangeException`). ASan
  reproduced the pre-fix `heap-buffer-overflow READ` at `BitConverter.hpp:126`
  and confirmed a clean throw post-fix; +46 tests.
- **SR-AUD-042 (medium):** `TotalOrderIeee754Comparer<float>`, `<double>`,
  and `<Half>` implement only ordering and cannot bind to the local
  `IEqualityComparer<T>` interface, omitting .NET's total-order equality and
  hash-comparer contract.
- **SR-AUD-043 (high):** `HashCode::AddBytes(ReadOnlySpan<byte>)` casts a
  negative public span length to an unsigned size and reads past its buffer;
  ASan confirms the overflow.  Span/ReadOnlySpan construction is the confirmed
  upstream cause. **043a partially remediated — ticket #1852 (2026-07-30):**
  the `Span`/`ReadOnlySpan` pointer ctors now reject a negative length and the
  `Span`/`ReadOnlySpan`/`Memory`/`ArraySegment` vector ctors reject a
  `size()>INT32_MAX` source (shared `System::detail::checkedSpanLength`), keeping
  the signed `intcs` field (no layout change). ASan reproduced the pre-fix
  `heap-buffer-overflow READ` at `HashCode.hpp:76` and confirmed a clean
  construction-time throw post-fix, closing the reachable exploit. **043b stays
  open (ticket #1854, `needs_user`):** the `ReadOnlyMemory` `noexcept`/`constexpr`
  ctors and `HashCode::AddBytes noexcept` need an exception-spec change to throw,
  which requires user approval; they are defense-in-depth now that 043a closes
  the reachable path. Finding stays **confirmed** until 043b lands.
- **SR-AUD-044 (high):** Span and ReadOnlySpan CopyTo/TryCopyTo use forward
  `std::copy`, corrupting overlapping nontrivial source ranges instead of
  preserving .NET's overlap-safe copy semantics.
- **SR-AUD-045 (high):** `SpanSplitEnumerator` treats an empty exact sequence
  as a zero-length repeating match, so `MoveNext` never completes and a
  range-for loop becomes infinite.
- **SR-AUD-046 (medium):** default `MemoryExtensions` sort, binary search,
  and sequence comparison use C++ operators rather than the .NET comparison
  contract, mishandling float NaN and invalidating the `std::sort` comparator.
- **SR-AUD-047 (high):** static `MemoryExtensions::CopyTo` omits destination
  capacity validation; an ASan probe copying two `int`s into one element reports
  a heap-buffer-overflow. Its forward-copy overlap path also extends SR-AUD-044.
- **SR-AUD-048 (medium):** `MemoryExtensions` whitespace trim uses
  locale-dependent byte `std::isspace`; it retains UTF-8 U+00A0 even though
  .NET treats it as whitespace.
- **SR-AUD-049 (high):** `ReadOnlyMemory::Slice(start)` subtracts an unchecked
  start from its signed length before validation; UBSan confirms `Slice(INT_MIN)`
  signed-overflow UB instead of a direct `ArgumentOutOfRangeException`.
- **SR-AUD-050 (high):** `Guid::NewGuid` and `CreateVersion7` derive their
  supposedly strong random fields from one seeded Mersenne Twister rather than
  the OS CSPRNG used by current .NET, making the output predictable after
  recovery of the standard PRNG state.
- **SR-AUD-051 (high):** raw-pointer `Array::Copy` forms unchecked pointer
  offsets and calls `memcpy` for arbitrary `T`; ASan confirms a negative-size
  operation and a nontrivial `std::string` copy later corrupts destruction.
- **SR-AUD-052 (medium):** every Array overload accepting a `std::function`
  skips boundary validation; empty functions either silently succeed on empty
  arrays or throw `std::bad_function_call` rather than an argument error.
- **SR-AUD-053 (low):** `Array::MaxLengthProperty()` reports `INT32_MAX`, not
  current .NET's `0x7FFFFFC7` maximum, with no documented vector adaptation.
- **SR-AUD-054 (high):** default `ArraySegment` operations omit the required
  invalid-underlying-array guard; `Slice(0)` dereferences null under ASan/UBSan
  while `ToArray`/copy paths can silently return a normal empty result.
- **SR-AUD-055 (medium):** `ArraySegment::CopyTo(std::vector<T>&)` resizes an
  undersized target rather than preserving the fixed destination and capacity
  exception semantics of the .NET counterpart.
- **SR-AUD-056 (medium):** the direct `IObservable<T>` test fixture returns a
  null subscription and permits post-completion notification; its tests omit
  unsubscription and terminal-state assertions, so it is not a valid provider
  behavior oracle.
- **SR-AUD-057 (high):** `Index::GetOffset` and
  `Range::GetOffsetAndLength` use signed C++ arithmetic for the intentionally
  unvalidated .NET offset path; a maximal from-end Index with `INT_MIN` length
  reaches UBSan-confirmed overflow rather than defined unchecked behavior.
- **SR-AUD-058 (medium):** `Progress<T>` stores an empty added callback and
  later throws native `std::bad_function_call` on `Report`, while the .NET
  nullable event subscription is a no-op rather than a delayed failure.
- **SR-AUD-059 (low):** `FormattableStringFactory::Create` documentation says
  empty format throws, but implementation and current .NET permit a valid
  empty format; the public exception claim is false.
- **SR-AUD-060 (high):** `DateOnly::FromDayNumber`, `AddDays`, `AddMonths`,
  and `AddYears` perform signed C++ arithmetic before range handling; UBSan
  confirms overflow on four reachable extreme public-input paths.
- **SR-AUD-061 (medium):** `DateOnly::TryParse` accepts arbitrary trailing
  text after a valid ISO date prefix through unchecked `std::sscanf` prefix
  conversion; `Parse` inherits the false success.
- **SR-AUD-062 (high):** `Tuple` hash combining adds signed `intcs` values
  after a bit shift; a reachable Tuple2 hash input causes UBSan-confirmed
  signed overflow instead of .NET's defined unchecked hash arithmetic.
- **SR-AUD-063 (medium):** every C++ `TupleN` exposes mutable public component
  fields although .NET Tuple components are immutable readonly properties;
  users can modify a created tuple in place without a documented adaptation.
- **SR-AUD-064 (medium):** Lazy constructors store invalid
  `LazyThreadSafetyMode` values and access silently dispatches them as
  PublicationOnly instead of throwing an argument-range error.
- **SR-AUD-065 (medium):** Lazy accepts empty `std::function` factories and
  fails only on the first `Value` access with native `std::bad_function_call`,
  rather than rejecting invalid factory input at construction.
- **SR-AUD-066 (medium):** Lazy's unconditional reentrancy guard throws for
  PublicationOnly even though that .NET mode must not throw the
  None/ExecutionAndPublication recursive-Value exception.
- **SR-AUD-067 (high):** raw-pointer `Buffer::BlockCopy` does not reject
  negative offset/count metadata; negative count casts to `size_t` and reaches
  ASan-confirmed unbounded `memmove` rather than an argument exception.

Implementation ticket #1783 (`REMED-COLL-SORTEDSET-LIVE-VIEW`, P2, size L) then
closed SR-AUD-361 on 2026-07-28 on local branch
`feature/remediation-coll-sortedset-live-view`, landing exactly the architecture
#1782 selected after the user granted the approval design section 28 required.
**SR-AUD-361 moves to `remediated`**, so the counts become **354 open and ten
`remediated`**. `SortedSet<T>` now holds `std::shared_ptr<State>` plus optional
bounds, and `GetViewBetween` -- still returning `SortedSet<T>` by value, now
without its `const` qualifier -- returns a live, inclusive-bounds-enforcing,
bidirectionally write-through handle onto the same tree. The finding's own
symptom is inverted where it was measured: `source-add-visible-in-view=1` and
`view-add-visible-in-source=1`, against the original evidence's 0 in both
directions. The four adjacent defects #1782 measured are closed with it, with no
new `SR-AUD-*` identifiers: probe 3's `operator<`-only element type now compiles
`-Werror` and runs; bounds are enforced after construction; nested views may only
narrow; and probe 2's `move-assign` **ASan `heap-use-after-free`** and `outlive`
**ASan `stack-use-after-scope`** are both gone, with `copy-assign` now yielding
the correct pre-assignment element instead of a silently wrong one.

Evidence: 47 new permanent regressions plus two standalone consumer fixtures
(positive `-Werror` against `Collections.Core` only, exits 0; negative `const`
caller correctly rejected); `SharpRuntimeTests_Collections_Core` 1,783/1,783 with
all 41 pre-existing SortedSet cases passing and no assertion edited;
`scripts/local_ci_check.sh build` at **13,069 tests across 37 executables**, up
from 13,022, with zero build warnings and zero errors; boundaries unchanged at 41
modules/90 edges with no new dependency edge; validator tests 7/7; catalogue
current; database consistent; `git diff --check` clean; Doxygen 1.9.8 at
**1,937** warnings against the 1,942 ceiling; all ten selective components
passing with a repository-local `TMPDIR` (run this time, because a public header
did change); the whole new suite clean under ASan+UBSan+LeakSanitizer with LSan
verified active by a deliberate-leak self-test; and a post-fix behavior probe
with 82 assertions and `failures=0`. Two limitations are recorded in design
section 30 rather than hidden: a bounded exception-ordering divergence from .NET
for a nested call that is simultaneously inverted and widening, and a
ThreadSanitizer-measured data race when concurrent threads call
`getCountProperty()` on *one* view object -- documented, not synchronized, since
the type claims no thread safety. Ticket #1773 remains `blocked`; CNA and
mobile-eggbert were not inspected or modified.

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

Ticket #1818 (`REMED-BUFFERS-BASE64-NONFINAL-PADDING`, P2, size S, area Buffers)
is the **third** ticket of the Base64 family plan
([`docs/Base64FamilyPlan.md`](../docs/Base64FamilyPlan.md), ticket #1815) and the
second of its three sequenced `decodeCore` tickets. It remediates **SR-AUD-080**,
which is now `remediated` in `audit/AUDIT_FINDINGS_INDEX.md` and in the owning
report `audit/modules/buffers/include/System/Buffers/Text/Base64.hpp.audit.md`.

**The defect.** `Base64::decodeCore` consulted `isFinalBlock` only *after* an
incomplete unpadded group. A **complete padded** group was decoded and returned
`Done` regardless of the flag, so a chunked caller was told a terminal quantum was
ordinary intermediate data.

**Why .NET disagrees, from two independent paths.**
`Base64Helper/Base64DecoderHelper.cs`'s `DecodeFrom` sets
`int skipLastChunk = isFinalBlock ? 4 : 0`; with the flag clear, `maxSrcLength`
covers the whole source, every character goes through the four-element loop where
`'='` is unmapped (`-1`), and the tail block that understands padding is
unreachable — that file's own comment says *"if isFinalBlock is false, we will
never reach this point"*. The whitespace-tolerant path reaches the same verdict:
`DecodeWithWhiteSpaceBlockwise` computes a per-block `localIsFinalBlock` and then
forces it back to false whenever the caller's `isFinalBlock` is false.

**The fix** is therefore one rule, not a new state machine: with `isFinalBlock`
false, `'='` is `InvalidData`. It fires at the **first** padding character, before
the quantum completes and before any destination arithmetic — which is what keeps
`bytesConsumed`/`bytesWritten` on the last completed quantum boundary, and what
stops a too-small destination from masking the rejection.

**The finding understated the surface.** It named
`DecodeFromUtf8("QQ==", …, false)`. Measured over seven non-final shapes on both
overloads (`build-probe/1818_defects.cpp`, logs `1818_prefix_defects.log` and
`1818_postfix_defects.log`), **six** were wrong: the bare padded quantum, a padded
quantum after a complete one, the single-`=` spelling, padding in a non-terminal
position, and padding split by whitespace. Only the unpadded control was already
right.

**Two residual divergences are recorded, not fixed.** Both are in the cursor
reported *alongside* `InvalidData`: `QUJD QQ==` differs by the one whitespace byte
.NET's `InvalidDataFallback` skips before re-entering the decoder, and the
pre-existing `QQ==QUJD` with `isFinalBlock == true` differs because
`DecodeWithWhiteSpaceBlockwise` reverts its block counters to `0,0` when
non-whitespace follows the padding. Neither changes a status or a decoded byte.
They are inactive ticket **#1822**, with no `SR-AUD-*` identifier — the numbering
stays frozen at 364.

**This narrows the accepted input set**, like #1817 and in the same direction.
Every `isFinalBlock == true` outcome is byte-for-byte unchanged, and so is
`IsValid`: it has no `isFinalBlock` parameter and *is* the final-block decoder's
validator, so decoder/validator agreement is preserved without touching
`validateCore`. Unpadded incomplete quanta keep `NeedMoreData`. All 104
pre-existing `Base64*` tests pass unmodified.

**Tests: +7 permanent regressions** — padded rejection with the exact cursor on
five shapes, the `char` overload inheriting it through the shared core, every
`isFinalBlock == true` outcome pinned unchanged, unpadded quanta still
`NeedMoreData`, the rejection beating a zero-length destination, `IsValid`
unaffected, and a two-chunk stream proving the flag still does its job.

**Validation.** `SharpRuntimeTests_Buffers` **492/492** (was 485), and the same 492
under **ASan + UBSan + LSan with zero reports**
(`build-asan/1818_buffers_asan.log`); the probe is clean under the same three
(`build-probe/1818_asan.log`, sanitizer activation proven by the runtime's own
`ASAN_OPTIONS=help=1` dump). Repository gate: **0 warnings, 0 errors**, **14,021
tests across 37 executables** (was 14,014).

**Source and ABI consequences: none.** No signature, virtual, return convention,
object layout or exported symbol changed; only the accepted input set did.

**Still open in this family**, in the plan's order: **#1819** (SR-AUD-081, trailing
whitespace wrongly consumed), **#1820** (SR-AUD-082, Base64Url rejects optional
final padding), and **#1821** (the empty-buffer status divergence, no `SR-AUD-*`).

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

---

Ticket **#1830** (`REMED-CORE-INDEX-DEFINED-OFFSET`, P1, size XS, category
`remediation`, area *Core*) is **done** and **SR-AUD-057 is now `remediated`** — the
first CCF-004 member repaired, under the family plan
`docs/DefinedArithmeticBoundaryPlan.md` written by design ticket #1829. The index counts
move to **22 remediated** and **342 confirmed** of 364.

`Index::GetOffset` evaluated `length - value_` in signed `intcs`, so
`Index::FromEnd(INTCS_MAX).GetOffset(INTCS_MIN)` was UBSan-confirmed undefined
behaviour. It is CCF-004 **class A** — .NET's `Index.cs` deliberately skips validation
for performance, and C# gives that decision meaning because its default integer
arithmetic has *defined* two's-complement wrap, which a C++ port cannot inherit by
executing signed overflow. The subtraction now happens in `SharpRuntime::uintcs`, and
`GetOffset` remains `noexcept` (pinned by a `static_assert` inside a test).

**The finding's site count was wrong, and is corrected here rather than backfilled.**
SR-AUD-057's own text and the family plan's §2 both described
`Range::GetOffsetAndLength` as merely *consuming* `Index`'s operation. It has a
**second, independent** overflow at `Range.hpp:99`: for a maximal from-end range over an
`INTCS_MIN` length the unsigned bounds checks **pass**, and the following `end - start`
is undefined behaviour (`signed integer overflow: -2147483648 - 1`). SR-AUD-057 has
**two** sites, and both are fixed — the finding is `remediated` on that basis, not on the
one site the audit named.

**Why the survey missed it is itself worth recording**, because it applies to every
remaining CCF-004 ticket: the family plan's own §3 reproduction recipe uses
`-fno-sanitize-recover`, which **aborts at the first diagnostic**, so `Index.hpp:61`'s
report hid `Range.hpp:99`'s in the same process. The plan now carries a §12 amendment —
enumerate a finding's sites with the *recovering* build and collect every diagnostic; use
the aborting build only to prove a site is gone. SR-AUD-060's seven `DateOnly` sites
(ticket #1837) are where this matters most.

**No observable change**, proven rather than asserted: the pinned values (`offset == 1`,
`length == 2147483647`, `FromEnd(MAX).GetOffset(0) == -2147483647`) are the ones measured
*before* the repair, which is what makes the class A claim verifiable.

**Tests: +10 permanent regressions.** `SharpRuntimeTests_Core_Base` **5002/5002**, clean
under **ASan + UBSan + LSan with zero reports** (`build-asan/1830_core_asan.log`); the
probe shows the two diagnostics present before (`build-probe/1829_ccf004_survey.log`) and
absent after (`build-probe/1830_postfix.log`). Repository gate: **0 warnings, 0 errors,
14,106 tests across 37 executables** (was 14,098). Module graph **41 / 91**.

**Source and ABI consequences: none.** No signature, virtual, vtable, object layout or
mangled symbol changed; `GetOffset` remains `noexcept`.

---

Ticket **#1832** (`REMED-CORE-INTPTR-DEFINED-WRAP`, P1, size XS, category `remediation`,
area *Core*) is **done** and **SR-AUD-025 is now `remediated`** — the second CCF-004
member. The index counts move to **23 remediated** and **341 confirmed** of 364.

`IntPtr::Add` evaluated `pointer.value + offset` in `intptr_t` and `Subtract` the mirror,
so both native boundary cases were signed-overflow UB. .NET exposes these as
`pointer + offset` / `pointer - offset` for `nint` under ordinary **unchecked** C#
arithmetic, whose modulo-native-width wrap is *defined*. Both now compute in `uintptr_t`.
CCF-004 **class A**.

Site enumeration used the **recovering** build first, per the amendment ticket #1830 added
as `docs/DefinedArithmeticBoundaryPlan.md` §12, and confirmed exactly **two** sites
(`IntPtr.hpp:105` and `:114`); the `friend operator+` / `operator-` forms forward to the
named methods rather than duplicating the arithmetic, and are pinned independently so a
refactor that stops forwarding cannot silently reintroduce the overflow.

**One implementation subtlety is recorded because getting it wrong would have been a
regression, not a neutral difference.** The offset needs a **two-step** cast,
`intcs → intptr_t → uintptr_t`. `intcs` is 32-bit and `uintptr_t` is 64-bit on LP64, so a
**negative** offset must be sign-extended *before* widening; casting straight to
`uintptr_t` zero-extends it and turns `Add(p, -1)` into an addition of 4294967295. That
would be a wrong answer, not merely a differently-arrived-at one, and it is pinned by
dedicated negative-offset tests in both methods and both operator forms.

**No observable change**, proven: `MaxValue + 1 == MinValue` and `MinValue - 1 == MaxValue`
are the values measured *before* the repair (`build-probe/1829_ccf004_survey.log` cases 3
and 4, UB present, vs `build-probe/1832_postfix.log`, UB absent and values byte-identical).

**Tests: +9 permanent regressions** — both boundaries, negative offsets in both methods,
ordinary offsets, the operator forms independently, and the extreme `intcs` offsets in
both directions. `SharpRuntimeTests_Core_Base` **5009/5009**, clean under **ASan + UBSan +
LSan with zero reports** (`build-asan/1832_core_asan.log`). Repository gate: **0 warnings,
0 errors, 14,113 tests across 37 executables** (was 14,106). Module graph **41 / 91**.

**Source and ABI consequences: none.**

---

Ticket **#1831** (`REMED-CORE-TUPLE-DEFINED-HASH`, P1, size XS, category `remediation`,
area *Core*) is **done** and **SR-AUD-062 is now `remediated`** — the third CCF-004 member
repaired under `docs/DefinedArithmeticBoundaryPlan.md`. The index counts move to
**24 remediated** and **340 confirmed** of 364.

`System::detail::tupleHashCombine` evaluated `((h1 << 5) + h1) ^ h2` in signed `intcs`.
The C++20 shift is defined; the addition is undefined behaviour when it is not
representable, and it is **reachable through ordinary public API** — `detail::tupleHash`
masks element hashes to the low 31 bits, so any element hash of 2^26 or more triggers it.
The audited input `Tuple2<intcs,intcs>(0x03ffffff, 0)` is what an `intcs` element of that
value produces directly. The whole expression is now evaluated in `SharpRuntime::uintcs`
with one conversion back, and the site carries the plan's §4 comment convention naming the
class and citing .NET's *unchecked* `Tuple.CombineHashCodes`.

This is CCF-004 **class A**, so the repair had to change **no value at all**, and that was
verified by measurement rather than argued: `build-probe/1831_prefix_values.log` and
`build-probe/1831_postfix.log` are byte-identical for sixteen probes covering Tuple1
through Tuple8, the audited overflowing input, both signed extremes and a `std::string`
element. Every literal in the new tests is a **pre-fix** measured value.

**Five reachable shapes were enumerated, and only three of them overflow.** With the
recovering build and **one process per shape** — the §12 amendment, which matters here
because all five shapes overflow the *same line* and UBSan deduplicates by source
location, so a single process reports the first and stays silent for the rest — cases 1
(`Tuple2(0x03ffffff,0)`), 3 (`Tuple3`, where the *outer* combine overflows on the inner
result even though items 2 and 3 are zero) and 4 (`Tuple8`, whose last operand is the
**unmasked** `Rest.GetHashCode()`) each report `Tuple.hpp:23:27: runtime error: signed
integer overflow: 67108863 + 2147483616`. Cases 2 (`combine(INTCS_MAX, 0)`) and 5
(`combine(-2000000000, 0)`) do **not** overflow, so the finding's reachable surface is
narrower than "any large operand" and wider than the one input the audit named. All five
are silent after the fix under `-fno-sanitize-recover=undefined`, which makes a surviving
site an abort rather than a line in a log.

**A second, structurally identical site was found outside the finding and was NOT folded
in.** `System::Net::Security::SslApplicationProtocol::GetHashCode()`
(`SslApplicationProtocol.hpp:72`) runs the same signed djb2 step over the ALPN protocol-id
bytes. `SslApplicationProtocol("spdy/3.1")` — a real registered protocol id — reports
`signed integer overflow: 729647660 + 1873888640` (`build-probe/1831_ssl_alpn_hash.log`).
It is a different file in a different module and was never named by the audit, so it is
separable: it became **inactive ticket #1838**, **no new `SR-AUD-*` identifier was issued**
and audit numbering stays frozen at **364**. Whether a given name overflows is
input-specific rather than a function of length — `"http/1.1"` is the same length, returns
a negative hash and does *not* report — so #1838's repair must be unconditional. The same
inventory **cleared** `System/ValueTuple.hpp`: its `detail::vtHashCombine` already
accumulates in `size_t` and is unaffected.

**Tests: +6 permanent regressions.** `SharpRuntimeTests_Core_Base` **5015/5015**, clean
under **ASan + UBSan + LSan with zero reports** (`build-asan/1831_core_asan.log`); the
ASan `TupleTests.cpp.o` was **proven recompiled** against the new header before the run,
because the previous handoff recorded the tree's core objects as pre-#1830. Repository
gate: **0 warnings, 0 errors, 14,119 tests across 37 executables** (was 14,113). Module
graph **41 / 91**.

**Source and ABI consequences: none.** No signature, virtual, vtable, object layout or
mangled symbol changed. `tupleHashCombine` remains `noexcept`, pinned by a `static_assert`
inside a test, as plan §8 requires.

---

Ticket **#1833** (`REMED-CORE-READONLYMEMORY-SLICE-ORDER`, P2, size S, category
`remediation`, area *Core*) is **done** and **SR-AUD-049 is now `remediated`** — the fourth
CCF-004 member repaired under `docs/DefinedArithmeticBoundaryPlan.md`. The index counts
move to **25 remediated** and **339 confirmed** of 364.

`ReadOnlyMemory<T>::Slice(intcs start)` evaluated `length_ - start` as the second **call
argument**, so the subtraction ran before the two-argument overload's already-correct
unsigned check could reject `start`. `Slice(INTCS_MIN)` on a three-element memory reported
`ReadOnlyMemory.hpp:140:25: runtime error: signed integer overflow: 3 - -2147483648`. It
then happened to throw the correct exception, which is exactly why this is CCF-004
**class B**: the repair moves the check earlier and nothing else. It is now the single
unsigned compare real .NET uses at `ReadOnlyMemory.cs:154-163`, so a negative `start`
compares as huge and one comparison covers both ends; once `0 <= start <= length_` is
known, `length_ - start` provably cannot overflow.

**Byte-identical exception, proven not assumed.** The probe prints the exception type,
`paramName` and full `Message` for every boundary of every one-argument `Slice`;
`build-probe/1833_prefix_values.log` and the values section of
`build-probe/1833_postfix.log` are **identical** with the pre-fix UBSan line removed. The
rejected inputs already threw `ArgumentOutOfRangeException("start")` with
`"Specified argument was out of the range of valid values. (Parameter 'start')"`, and still
do.

**The whole one-argument-`Slice` family was inventoried, as the ticket required, and the
audit's site count is confirmed correct at one.** `Memory<T>::Slice(intcs)`,
`Span<T>::Slice(intcs)`, `ReadOnlySpan<T>::Slice(intcs)` and
`ArraySegment<T>::Slice(intcs)` all validate with a signed `start < 0 || start > length_`
pair-compare **before** subtracting, so their `length_ - start` is unreachable with an
out-of-range operand; each was run with `INTCS_MIN` in its own process at `-O0` and
measured **clean** (`build-probe/1833_prefix_ub.log`, cases 2–5). They are pinned by tests
anyway, so a later "simplification" into the old forward-then-check shape is caught.
`ReadOnlySequence<T>`'s five one-argument `Slice` forms were inventoried too and are clear:
each resolves through `GetPosition`, which validates, and its `Slice(longcs, longcs)`
carries a comment recording the same class of fix from an earlier ticket.

`ArraySegment<T>::Slice`'s `paramName` is `"index"`, not `"start"`. That is **correct and
deliberate** — .NET's `ArraySegment<T>.Slice`'s parameter is named `index` — and is now
pinned as such so the divergence is not "harmonised" away.

**This finding is why plan §3 cause 2 exists**, and it held: at `-O1` GCC folds the wholly
compile-time `3 - INT_MIN` and emits **no UBSan check at all**, so the audited case passes
silently. The probe uses `-O0` and additionally holds its operands in `volatile`, so it
does not depend on the optimisation level.

**Tests: +7 permanent regressions.** `SharpRuntimeTests_Core_Base` **5022/5022**, clean
under **ASan + UBSan + LSan with zero reports** (`build-asan/1833_core_asan.log`), with the
ASan `Batch3TypeTests.cpp.o` proven recompiled first; ASan matters here because plan §7
names it for the one member whose fix touches a pointer and a length. Post-fix probe run
under `-fno-sanitize-recover=undefined`, so a surviving site would abort rather than print.
Repository gate: **0 warnings, 0 errors, 14,126 tests across 37 executables** (was 14,119).
Module graph **41 / 91**.

**Source and ABI consequences: none.** No signature, virtual, vtable, object layout or
mangled symbol changed. The function was not `noexcept` before and is not now; it already
threw for every input the new guard rejects.

---

Ticket **#1834** (`REMED-CORE-INT128-MINVALUE-DEFINED`, P1, size S, category `remediation`,
area *Core*) is **done** and **SR-AUD-019 is now `remediated`** — the fifth CCF-004 member
repaired under `docs/DefinedArithmeticBoundaryPlan.md`. The index counts move to
**26 remediated** and **338 confirmed** of 364.

Two public paths negated a **signed** `__int128` whose magnitude 2^127 is not
representable. `Int128::TryParse` (`Int128.hpp:234`) converted the already-correct unsigned
magnitude to signed and *then* negated it; `Int128::ToString` (`Int128.hpp:143`) negated
`value_` to build its decimal magnitude. Both reported
`negation of 0x80000000000000000000000000000000 cannot be represented in type '__int128'`.
Both now stay in `unsigned __int128` and convert once at the end — the idiom
`Int128::operator-()` in the same header already used, with a comment at each site saying so
in case a later reader mistakes the cast for redundancy.

**CCF-004 class A, verified by measurement.** `build-probe/1834_prefix_values.log` and the
values section of `build-probe/1834_postfix.log` are **identical** across thirty-one probes:
both endpoints, both endpoints' neighbours in both directions, the malformed inputs, the
zero-magnitude negative `"-0"`, every format path, and the operator vectors. Both
diagnostics present before, zero present after under `-fno-sanitize-recover=undefined`.

**The finding's reach is wider than the two entry points the audit named, and the extra
reach needed no extra fix.** `ToString(format)` is a *third* public door onto
`Int128.hpp:143`: the `D`, `d` and `G` paths, and the width-padded `D40`/`D45` forms, all
delegate to `ToString()` and so all reported the same diagnostic (probe cases 4 and 5).
`Parse` is a fourth, onto `:234` (case 3). All four are now silent, and all four are pinned
by tests — because a fix justified only by `TryParse` and `ToString()` would leave a reader
unable to tell whether the format overloads had been considered. The hexadecimal `X`/`x`
path never negated and is pinned unchanged.

**Nothing else in the file has this shape**, verified rather than assumed: the only other
candidate negations are `Abs`, which explicitly throws `OverflowException` for `MinValue`
before negating, and `operator/` / `operator%`, which explicitly throw for `MinValue / -1`.
`operator+`, `operator-`, `operator*` and unary `operator-` already compute in
`unsigned __int128`. `UInt128` cannot have the defect. Plan §10.5's exclusion of the
operator arithmetic therefore holds, and the existing operator vectors are asserted here so
this ticket cannot disturb them.

**No new compiler-extension dependency**: `unsigned __int128` was already required by this
header, so the MSVC limitation recorded in `CLAUDE.md` is unchanged.

**Tests: +8 permanent regressions.** `SharpRuntimeTests_Core_Base` **5030/5030**, clean
under **ASan + UBSan + LSan with zero reports** (`build-asan/1834_core_asan.log`), with the
ASan `IntegerTypesTests.cpp.o` and `libsharp_runtime_core.a` proven rebuilt first.
Repository gate: **0 warnings, 0 errors, 14,134 tests across 37 executables** (was 14,126).
Module graph **41 / 91**.

**Source and ABI consequences: none.** No signature, virtual, vtable, object layout or
mangled symbol changed.

---

Ticket **#1835** (`REMED-BUFFERS-UTF8PARSER-INT64-MIN`, P1, size S, category `remediation`,
area *Buffers*) is **done** and **SR-AUD-084 is now `remediated`** — the sixth CCF-004
member repaired under `docs/DefinedArithmeticBoundaryPlan.md`. The index counts move to
**27 remediated** and **337 confirmed** of 364.

`tryParseInt` (`Utf8Parser.hpp:224`) and the grouped `N` branch of `tryParseIntegerCore`
(`:357`) both deliberately admit a negative magnitude of `INT64_MAX + 1` — exactly
`INT64_MIN`'s magnitude — and then converted it to a signed `int64_t` and negated it. Both
reported `negation of -9223372036854775808 cannot be represented in type 'long int'`. Both
now negate in the unsigned domain and convert once.

**CCF-004 class A, verified by measurement.** `build-probe/1835_prefix_values.log` and the
values section of `build-probe/1835_postfix.log` are **identical** across twenty-nine
probes, covering both audited inputs, every signed width at its own minimum, one past each
end in both directions, `"-0"`, ordinary values, both formats, and the untouched unsigned
and hex paths. `parsed`, `value` **and** `bytesConsumed` are asserted, not only the value.
Both diagnostics present before, zero after under `-fno-sanitize-recover=undefined`.

**The ticket's question about Int32/Int16/SByte is answered, and the answer is neither
"same treatment" nor "excluded".** Those widths have **no negation of their own** — there
are exactly two sites — but they are **not** unaffected, because
`tryParseIntegerCore`'s magnitude limit is `INT64_MAX + 1` regardless of `byteWidth` and the
type-width check is applied by the **caller afterwards**. So
`TryParse("-9223372036854775808", int32_t&, …)` executed the undefined negation and only
*then* returned false, and the same holds for `int16_t`, `int8_t` and the grouped format
(`build-probe/1835_prefix_ub.log`, cases 3–6). The audit's Int64-only framing understates
the reachability: **four more public overloads reached the same UB**, all fixed by the same
two edits, and all now pinned by tests. This is recorded as an addition; the finding keeps
its identifier and its site count of two.

The `X`/`x` hexadecimal path never negated — it sign-extends a bit pattern — and the
unsigned overloads have no negative branch at all. Both are asserted unchanged so the edit
cannot have leaked across the branch.

**The audit's note that "no direct test covers either minimum input" was accurate**; the
five new suites are the first direct coverage of both.

**Tests: +5 permanent regressions.** `SharpRuntimeTests_Buffers` **522/522**, clean under
**ASan + UBSan + LSan with zero reports** (`build-asan/1835_buffers_asan.log`), with the
ASan `Utf8ParserTests.cpp.o` proven recompiled first. Repository gate: **0 warnings, 0
errors, 14,139 tests across 37 executables** (was 14,134). Module graph **41 / 91**.

**Source and ABI consequences: none.** No signature, virtual, vtable, object layout or
mangled symbol changed; both changed functions are private static helpers.

---

Ticket **#1841** (`REMED-IO-COMPRESSION-CLOSED-CAPABILITIES`, P3, size S, category
`remediation`, area *IO.Compression*) is **done**. It carries **no `SR-AUD-*` identifier** and
**changes no finding's status** — audit numbering stays frozen at **364**, and the index still
reads **27 remediated / 337 confirmed**. It is Layer 1(b) of
`docs/StreamCapabilityContractDesign.md`, split out of blocked ticket **#1828**.

`DeflateStream`, `GZipStream` and `ZLibStream` returned their **mode alone** from
`getCanReadProperty()` and `getCanWriteProperty()`, so a **closed** wrapper still claimed the
capability. Real .NET's `DeflateStream.cs:171-195` returns `false` whenever
`_stream == null`, *before* consulting the mode; `GZipStream`/`ZLibStream` delegate to a
`DeflateStream` and inherit that, whereas this port gives each of the three its **own copy** of
the bodies — which is why the fix is three edits and why the tests cover all three rather than
one.

**Measured on the full twelve-combination matrix before and after** — three wrappers × two
modes × before/after `Close()` — for **both** `leaveOpen` values
(`build-probe/1841_prefix.log`, `build-probe/1841_postfix.log`). Before: every wrapper kept its
capability across `Close()` in all twelve. After: every one reports `false`, and the open-state
answers and the valid compress-and-close cycle are byte-identical.

**No new member and no object-layout change, which is why this half needed no approval.**
`state_->initialized` already exists, is set at the end of the constructor, and is cleared in
`Close()` **unconditionally and before** the flush loop that can throw
(`DeflateStream.cpp:200`). `inner_` is the wrong signal: `Close()` nulls it only when
`leaveOpen` is `false`, so a `leaveOpen == true` wrapper keeps a live `inner_` after `Close()`
and would still look open. Both `leaveOpen` values are asserted for exactly that reason. The
assumption that this half needed a disposed **flag** — and therefore a layout approval, like
`SR-AUD-337` — was wrong, and #1839 measured it wrong before this ticket started.

**The delegation half is deliberately still absent and is now pinned as such.** .NET also
conjoins the inner stream's matching capability. That consults another object's
possibly-defaulted declaration, so it is covered by the single approval in
`StreamCapabilityContractDesign.md` §6.2 and remains blocked **#1828**. A test asserts today's
`CanRead == true` for a wrapper over a closed inner stream, with a message telling a future
reader to invert it when #1828 lands — so the split is visible in the test suite, not only in a
document.

**Tests: +6 permanent regressions.** `SharpRuntimeTests_IO_Compression` **37/37**, clean under
**ASan + UBSan + LSan with zero reports** (`build-asan/1841_io_compression_asan.log`), with the
ASan `CompressionTests.cpp.o`, `libsharp_runtime_io.a` and `libsharp_runtime_io_compression.a`
all proven rebuilt first. Repository gate: **0 warnings, 0 errors, 14,145 tests across 37
executables** (was 14,139). Module graph **41 / 91**.

**Source and ABI consequences: none.** No signature, virtual, vtable, object layout or mangled
symbol changed. The observable change is confined to what a **closed** wrapper reports, which
is the wrong answer being corrected.

---

Ticket **#1840** (`DOC-IO-STREAM-CAPABILITY-DEFAULTS`, P3, size XS, category `remediation`,
area *IO*) is **done**. **Documentation only** — no behaviour, signature or default changed.
It carries **no `SR-AUD-*` identifier** and changes no finding's status; audit numbering stays
frozen at **364** and the index still reads **27 remediated / 337 confirmed**. Layer 1(a) of
`docs/StreamCapabilityContractDesign.md`.

`System::IO::Stream`'s class doc-comment listed which members a subclass must implement and
said **nothing** about the three capability properties, whose defaults are
`getCanWriteProperty() == false`, `getCanReadProperty() == true` and
`getCanSeekProperty() == false` — all three **abstract** in .NET (`Stream.cs:29-31`). A stream
author therefore had no way to learn that **omitting an override is itself a declaration**, and
that the declaration made by silence differs per property.

That gap is the root of the whole #1824/#1827/#1828 family, and it is not hypothetical:
`tests/integration/…/CompressionTests.cpp`'s `ThrowingWriteStream` overrides `Write()` and not
the property, which is why it is the only thing in the repository that #1839's measured
experiment broke.

The class doc-comment now carries the three-row table, states that the two mistakes the defaults
invite point in **opposite** directions (an unwritten `CanWrite` makes a guard **over**-reject a
working stream; an unwritten `CanRead` makes a guard **under**-reject an unreadable one), notes
that `getCanSeekProperty()` does **not** gate `Seek()`/`Position` — so leaving it out does not
stop seeking, it only makes capability-checking callers refuse — and states that a stream whose
capability depends on its lifetime must fold that in, citing `MemoryStream` (#1826) and the zlib
wrappers (#1841). Each of the three properties also gained its own doc-comment saying what its
default is, that it is a deviation, and what omitting it declares.

**No test-count change** — a doc-only ticket adds no regressions and the repository gate holds at
**14,145 tests across 37 executables**, 0 warnings, 0 errors. Canonical Doxygen: **1,941**
warnings against the 1,942 ceiling, unchanged. Module graph **41 / 91**.

**Source and ABI consequences: none whatsoever.**

Ticket **#1836** (`REMED-CORE-TIMESPAN-DEFINED-TICKS`, P1, size M, category `remediation`,
area *Core*) is **done** and **SR-AUD-008 is now `remediated`** — the seventh CCF-004 member
repaired and the family's only member spanning **two** classes. The index counts move to
**28 remediated** and **336 confirmed** of 364.

The finding has two halves, in different classes, and each was fixed as its own kind of
change. **Class A** was `TimeSpan::Subtract`'s single expression `ticks_internal -
ts.ticks_internal`, undefined at `TimeSpan.cpp:263` for `MinValue.Subtract(1)`,
`MaxValue.Subtract(-1)` and `MinValue - 1` (three public doors, not one). Real .NET computes
`operator -(TimeSpan, TimeSpan)` unchecked (`TimeSpan.cs:877-879`) and *then* tests the sign
bits — the wrap is intended and the sign-bit test is the real guard — so the subtraction now
runs in `ulongcs` and converts back, and every returned value, exception type and message is
byte-identical. **Class C** was the shared parse core: `TryParse` overflowed the tick
accumulation at **four** distinct columns of the same statement (`:454:53` day product,
`:454:16` and `:457:22` two accumulations, `:459:29` the int64-minimum negation) reached by
different inputs, plus a **fifth** undefined operation from the C library — `std::sscanf`'s
`%d` conversion of a value the target cannot represent (C17 7.21.6.2p10), measured wrapping
`"2147483648.00:00:00"` to `-2147483648` and twenty digits to `-1`. Components are now read as
`long long` behind an eighteen-digit run limit and the magnitude is accumulated in `ulongcs`
with one range check per sign; the negative direction admits exactly one more magnitude (2^63,
`MinValue`) than the positive, which is .NET's deliberate asymmetry
(`TimeSpanParse.cs:612-618`, `:816-822`).

**A method correction that generalises (recorded as `DefinedArithmeticBoundaryPlan.md`
§17.4).** A UBSan sweep enumerates *undefined operations*, not *wrong answers*. Two of this
member's four wrong answers are invisible to UBSan: `"--5.00:00:00"` returned the **positive**
five-day duration (two sign inversions cancelling, nothing overflowing) and
`"-10675199.02:48:05.4775809"` returned the **positive** `MaxValue`. Both are now rejected —
the doubly-signed string as a `FormatException` (a second sign character is malformed, not
overflow, in .NET's tokenizer), the out-of-range one as `OverflowException`.

**Parse is a behaviour change on the throwing entry point too**, as §16.1 warned: it used to
*return* the wrapped value, and now raises `OverflowException` with .NET's
`SR.Overflow_TimeSpanElementTooLarge` text for an out-of-range component while keeping
`FormatException` for a malformed string. The class C compatibility argument (§17.6) is the
one the batch accepted for #1817/#1818/#1825: the four inputs that change from success to
failure never produced a correct value, three by undefined behaviour, so no new approval is
required (plan §9). SR-AUD-008's site count is corrected to **six** and its public-door count
to **five** — `Subtract`, `operator-`, `TryParse`, `Parse`, and `System::Xml::XmlConvert::
ToTimeSpan` in another module, all pinned by tests.

**CCF-004 class A verified by measurement.** `build-probe/1836_prefix.log` and
`build-probe/1836_postfix.log`, nineteen cases, **one process per case**, against a
`build-asan` tree whose `TimeSpan.cpp.o` and `libsharp_runtime_core.a` were proven newer than
the source both before and after the edit; the recovering build enumerated the sites and the
`-fno-sanitize-recover=undefined` build then returned exit 0 for every case, proving each
gone. ASan + UBSan + LSan clean (`build-probe/1836_asan_ubsan_lsan.log`).

**Tests: +17 permanent regressions** (15 in `TimeSpanTests.cpp`, 2 in the XML module's
`XmlSupportTests2.cpp` for the cross-module door). Repository gate: **0 warnings, 0 errors,
14,162 tests across 37 executables** (was 14,145). Module graph **41 / 91**. Canonical
Doxygen unchanged.

**Source and ABI consequences: none.** `TimeSpan.hpp` is unchanged; the shared parse core is
a `static` file-local function with internal linkage, so no declaration, member, signature,
symbol or object layout changed.

Ticket **#1837** (`REMED-CORE-DATEONLY-DEFINED-ARITH`, P1, size M, category `remediation`,
area *Core*) is **done** and **SR-AUD-060 is now `remediated`** — the eighth and final CCF-004
member, which **closes the family**. The index counts move to **29 remediated** and **335
confirmed** of 364.

The finding's seven signed-overflow sites are all removed: `:65` (`FromDayNumber`) and `:76`
(`AddDays`) by adding in the **unsigned** day-number domain and rejecting with one unsigned
compare *before* `jdnToDate` is reached, which makes the `:35`/`:37`/`:39` multiplication cascade
inside `jdnToDate` **unreachable with an out-of-range argument** rather than needing three more
guards; `:81` (`AddMonths`) and `:92` (`AddYears`) by bounding the delta (+/-120000 and +/-10000)
before any arithmetic. The port stores `year_`/`month_`/`day_`, not .NET's `uint _dayNumber`, so
.NET's idiom was translated rather than copied: `FromDayNumber`/`AddDays` take the unsigned-compare
idiom (`DateOnly.cs:73-81`, `:121-132`), `AddMonths`/`AddYears` take `DateTime.AddMonths`/`AddYears`'s
bounds-then-divide idiom (`DateTime.cs:960-977`, `:1020-1032`) which this repository's own
`DateTime::AddMonths` already implements. `kMaxDayNumber` was **measured** at 3652058.

**Two silent wrong answers, not just UB (16.4).** `DateOnly(1,1,1).AddYears(INTCS_MIN)` computed
`INTCS_MIN*12 == -6*2^32`, which wrapped to **zero** and returned `0001-01-01` unchanged — a date
2.1 billion years earlier asked for and the same date returned with no error. It now throws.
**A ~179-million-iteration loop (16.5)** in `AddMonths(INTCS_MIN)` (`month_ + n` did not overflow,
`1 + INT_MIN` fits, but the `while (m < 1)` normalisation ran to completion the slow way) is
removed: the two `while` loops are replaced by .NET's single division
`q = (m > 0) ? (m-1)/12 : m/12-1`, so no path iterates unboundedly.

**The paramName decision (design §18.5).** Every current rejection named `year` with
`"DateTime: date component out of range (Parameter 'year')"`, leaked from the `DateTime`
constructor. This ticket adopts **.NET's per-method paramNames** — `dayNumber`, `value`, `months`,
`value` — because parity is the mission, the inherited `year` was not a deliberate `DateOnly`
contract, and the exception **type** is unchanged. One documented consequence: a moderately
out-of-range input whose arithmetic never overflowed (e.g. `AddMonths(200000)`) used to throw
`year` and now throws `months`/`value` — a paramName change on a non-UB rejection path, authorised
by the acceptance criterion's "stated, justified paramName choice" and §4.3, changing no input
from success to failure and no exception type. No in-repository test pinned the old string.

**Verified by measurement.** `build-probe/1837_prefix.log` vs `build-probe/1837_postfix.log`,
one process per case, `build-asan` proven newer than source before and after; the recovering build
enumerated all seven sites and the `-fno-sanitize-recover=undefined` build returned exit 0 for
every case afterwards. Every valid date, day number and Add* result is byte-identical (case 9),
including both domain endpoints and the widest legal day spans.

**Tests: +8 permanent regressions** in `DateOnlyTimeOnlyTests.cpp`. Repository gate: **0 warnings,
0 errors, 14,170 tests across 37 executables** (was 14,162). Module graph **41 / 91**. Canonical
Doxygen unchanged.

**Source and ABI consequences: none.** `DateOnly.hpp` is unchanged; `kMaxDayNumber` is a
file-local `static constexpr` and every change is inside a function body.

Ticket **#1842** (`REMED-IO-FILESTREAM-CLOSED-CAPABILITIES`, P3, size XS, category
`remediation`, area *IO*) is **done**. **No SR-AUD-\* identifier and no audit status change**:
numbering stays frozen at 364, counts stay **29 remediated / 335 confirmed**. It is the
prerequisite for the Stream capability wrapper guards (#1824/#1827/#1828) -- a closed FileStream
must not pass a reader/writer wrapper constructor guard.

`FileStream::getCanReadProperty()` and `getCanWriteProperty()` returned the bare `canRead_` /
`canWrite_` flags, which reflect the access mode requested at construction and are never reset by
`Close()`, so a closed FileStream still claimed the capability -- the identical stale-capability
shape #1826 fixed in MemoryStream, while `getCanSeekProperty()` here already folded in
`file_.is_open()`. Both now return `canX_ && file_.is_open()`, matching real .NET's
`OSFileStreamStrategy.CanRead => !_fileHandle.IsClosed && (_access & FileAccess.Read) != 0` and the
symmetric `CanWrite` (`Strategies/OSFileStreamStrategy.cs:73-75`, reached through
`FileStream.cs:386-389`). Unlike MemoryStream -- whose .NET `CanWrite => _writable` deliberately
stays true after close (#1826) -- .NET FileStream folds the closed state into **both** directions,
so both do here.

**+5 permanent regressions** in `IOStreamTests.cpp` (read-only, write-only and read-write opens
before and after `Close()`; the property/#1825-operation agreement after close; the open-file
no-change case). `SharpRuntimeTests_IO` clean under **ASan + UBSan + LSan, 0 reports** on the
disposal path. Repository gate: **0 warnings, 0 errors, 14,175 tests across 37 executables**.
Module graph **41 / 91**. **Source and ABI consequences: none** -- two inline header bodies, no
signature, member, vtable, layout or symbol change.

Ticket **#1838** (`REMED-NET-SECURITY-ALPN-DEFINED-HASH`, P2, size XS, category `remediation`,
area *Net.Security*) is **done**. **No SR-AUD-\* identifier and no audit status change**: it is a
side-finding discovered while inventorying SR-AUD-062's structurally equivalent sites under #1831
(different module, different file, never named by the audit), so numbering stays frozen at 364 and
counts stay **29 remediated / 335 confirmed**.

`SslApplicationProtocol::GetHashCode()` (`SslApplicationProtocol.hpp:72`) ran the same signed djb2
step `hash = ((hash << 5) + hash) ^ byte` that SR-AUD-062's `tupleHashCombine` did, in signed
`intcs`, and overflowed for reachable ALPN ids -- `"spdy/3.1"` reported `signed integer overflow:
729647660 + 1873888640`. Fixed exactly as #1831: accumulate the shift, addition and xor in
`uintcs`, one conversion to `intcs` at the end. **CCF-004 class A, verified by measurement.** The
hash of every input is byte-identical -- `"h2"` 3418, `"http/1.1"` -869919367, `"h2c-15"`
-238047472, `"spdy/3.1"` -1691431011, empty 0, a 65-byte id 609988858, a high-bit byte vector
1237484447 -- measured before (retained `build-probe/1831_ssl_alpn_hash.log`) and after
(`build-probe/1838_prefix_postfix.log`). UBSan at `-O0` linked against `build-asan` showed the
`:72` diagnostic for `"spdy/3.1"` present before and, under `-fno-sanitize-recover=undefined`,
exit 0 after. `System/ValueTuple.hpp`'s `vtHashCombine` was re-confirmed clear (accumulates in
`size_t`).

**+2 permanent regressions** in `SecuritySupportTests.cpp`. Repository gate: **0 warnings,
0 errors, 14,177 tests across 37 executables**. Module graph **41 / 91**. **Source and ABI
consequences: none** -- one inline header body, header-only, no member or signature change.

Ticket **#1824** (`REMED-IO-STREAMWRITER-DIRECTION`, P2, size S, category `remediation`, area
*IO*) is **done**. **No SR-AUD-\* identifier**; numbering stays frozen at 364, counts stay
**29 remediated / 335 confirmed**. It was **blocked** pending the shared Stream-capability
approval; the user granted the decision in `docs/StreamCapabilityContractDesign.md` §6.2 and it
is now implemented.

`StreamWriter(Stream*)` validated only the null stream, while the sibling `BinaryWriter` already
rejected an unwritable stream -- the repository was inconsistent between siblings (§3.1). The
constructor now throws `ArgumentException("Stream was not writable.")` (no `(Parameter …)` suffix,
matching .NET's message-only `Argument_StreamNotWritable`) after the null check and before use,
per §6.2's verbatim declaration and `StreamWriter.cs:135-146`. The null check runs first, so a
null stream still reports `ArgumentNullException`. Because `getCanWriteProperty()` defaults to
**false** (where `getCanReadProperty()`'s is true), this rejects an *undeclared-writable* custom
stream -- the write-direction twin of #1808's read guard -- which is exactly the semantic the §6.2
approval covers; the one-line fix for such a stream is to override the property.

Measured compatible under #1839: every in-repository `StreamWriter(Stream*)` site wraps a
`MemoryStream` or `FileStream`, both of which override the property, so **nothing in the
repository is rejected**. With #1842, a closed `FileStream` now also reports `CanWrite == false`
and is rejected at construction rather than passing the guard and failing at first write.

**+8 permanent regressions** in `IOStreamTests.cpp` (undeclared-writable rejected; the exact
message; no `(Parameter …)` suffix; null-before-unwritable order; the one-line-fixed stream
accepted in both `leaveOpen` modes; production writable streams still construct; the closed
FileStream rejected; the `StreamWriter`/`BinaryWriter` cross-type identity). Two new test doubles,
`UndeclaredWritableTestStream` and `DeclaredWritableTestStream`. `SharpRuntimeTests_IO` **599/599**.
Repository gate: **0 warnings, 0 errors, 14,185 tests across 37 executables**. Module graph
**41 / 91**. **Source and ABI consequences: none** -- one added guard in a `.cpp` body, no
signature, member, vtable, layout or symbol change.

Ticket **#1828** (`REMED-IO-COMPRESSION-STALE-CAPABILITIES`, delegation half, P3, size S,
category `remediation`, area *IO*) is **done**. **No SR-AUD-\* identifier**; numbering stays
frozen at 364, counts stay **29 remediated / 335 confirmed**. The closed-state half shipped as
#1841; this is the inner-stream **delegation** half, covered by the §6.2 approval.

All three zlib wrappers (`DeflateStream`, `GZipStream`, `ZLibStream` -- separate bodies in this
port, not the single delegation .NET uses) now conjoin the inner stream's matching capability:
`getCanReadProperty()` is `mode_ == Decompress && inner_->getCanReadProperty()` and
`getCanWriteProperty()` is `mode_ == Compress && inner_->getCanWriteProperty()`, matching
`DeflateStream.cs:171-195`. `inner_ == nullptr` is added to the closed guard so the delegation
cannot dereference the inner stream a non-`leaveOpen` `Close()` nulled. Before this a wrapper over
an incapable inner stream still claimed the mode's capability (`build-probe/1841_prefix.log`,
last line); now a Decompress wrapper over a non-readable inner reports `CanRead == false` and a
Compress wrapper over a non-writable inner reports `CanWrite == false`. Measured compatible under
#1839 (the whole-gate experiment showed the zlib delegation breaks nothing).

The `ZlibClosedCapabilityTests.InnerStreamDelegationIsStillAbsent` test that #1841 wrote to pin
the *absent* delegation was **inverted** as its own comment instructed ("when #1828 lands this
assertion inverts"), and replaced by four `ZlibInnerDelegationTests`: Decompress over an
unreadable inner and Compress over a non-writable inner both report false across all three
wrappers; a capable `MemoryStream` inner still reports the mode capability; and a closed wrapper
still reports false over a live capable inner (the #1841 guard wins). **Net +3 regressions**
(four added, one inverted-and-renamed). `SharpRuntimeTests_IO_Compression` **40/40**. Repository
gate: **0 warnings, 0 errors, 14,188 tests across 37 executables**. Module graph **41 / 91**.
**Source and ABI consequences: none** -- three `.cpp` body edits (one conjunct and one null
guard each), no signature, member, vtable, layout or symbol change.

Ticket **#1827** (`REMED-IO-ZIP-MODE-CAPABILITIES`, P2, size S, category `remediation`, area
*IO*) is **done**, which **completes the Stream-capability family (#1824, #1827, #1828) the §6.2
approval unblocked.** **No SR-AUD-\* identifier**; numbering stays frozen at 364, counts stay
**29 remediated / 335 confirmed**.

`ZipArchive(Stream*)` validated the null stream and (since #1813) the mode range, but not the
stream's capabilities against the mode. `validateZipArchiveCapabilities()` now runs after both,
matching .NET's ValidateModeCapabilities (`ZipArchive.cs:962-975`) with messages verbatim from
`System.IO.Compression`'s `Strings.resx`: `Create` needs `CanWrite` ("Cannot use create mode on a
non-writable stream."), `Read` needs `CanRead` ("Cannot use read mode on a non-readable stream."),
`Update` needs all three ("Update mode requires a stream with read, write, and seek
capabilities."). The `CanWrite` direction is covered by the §6.2 approval; the two decisions the
design left to this ticket were taken as:

- **`Update` requires `CanSeek`** (the design's "harshest clause", since `CanSeek` defaults
  false). Adopted, matching .NET: `Update` reads the central directory and rewrites it in place,
  and this port's prior best-effort *append*-to-a-non-seekable-Update-stream path (`Dispose()`
  line ~520) would have corrupted the archive. Measured compatible under #1839 -- no in-repository
  `Update` caller wraps a stream lacking a capability.
- **A `Read`-mode UNSEEKABLE stream is buffered, not rejected.** Only `CanRead` is required for
  `Read`. This port already reads the whole input into `memBuf` at construction and never seeks
  the caller's stream while reading, so an unseekable readable stream is genuinely supported --
  matching .NET's `isReadModeAndUnseekable` buffering -- not merely un-rejected. Rejecting it
  "is not an option" (design §6.2), and it isn't.

The one in-repository migration the #1839 whole-gate experiment surfaced was applied:
`ThrowingWriteStream` (`tests/integration/…/CompressionTests.cpp`) gained
`getCanWriteProperty() -> true`, declaring truthfully that it *does* implement `Write()` (it just
throws to simulate an I/O failure) rather than to bypass the guard; its two original
Dispose/Destructor assertions still hold.

**+8 permanent regressions** in the integration `CompressionTests.cpp` (Create/Read/Update
rejections with exact messages; the Read-mode unseekable tolerance proved by a real round trip;
null-before-mode-before-capability order; the fully-capable valid path across all three modes;
capability-rejection runs no destructor). `SharpRuntimeIntegrationTests` **864/864**, and the
#1827 subset is clean under **ASan + UBSan + LSan, 0 reports** on the rejection/disposal path.
Repository gate: **0 warnings, 0 errors, 14,196 tests across 37 executables** (verified by a full
`scripts/run_component_tests.sh build` run). Module graph **41 / 91**. **Source and ABI
consequences: none** -- one added file-local validator plus one call in a `.cpp` body, no
signature, member, vtable, layout or symbol change.

## Post-audit remediation checkpoint — namespace review + SR-AUD-020 (2026-07-30)

Batch `feature/remediation-batch-1804-namespace-review`. Two units.

**Tooling ticket #1804** (`REMED-TOOLING-SEAM-DISCOVERY-VACUITY`, P3, `done`) closed the
defence-in-depth gap #1803 measured: `scripts/check_version_seam_odr.py` discovered a seam as a
class template *declared and not defined* in `namespace SharpRuntime::Testing`, so giving a
seam's **primary template** a body made it leave discovery, the run exited 0, and the seam count
fell from 2 to 1 silently. Discovery now also surfaces a **defined** primary class template there
(a `template<…>` head), so the seam re-enters the inventory and rule 1 rejects it. Non-hard-coded
and template-only, so a legitimate non-template `SharpRuntime::Testing` helper is not rejected —
the wontfix trigger did not apply. Self-tests 12→15; reproduction `build-probe/1804_gap_probe.py`;
mutation campaign on the real `MutationCounter.hpp` confirmed reject-then-restore. Durable record
`docs/CollectionVersionTestSeamDesign.md` §15; `CLAUDE.md` seam invariant updated. **No SR-AUD-\*
identifier**; no production code touched.

**The namespace review** selected the **numeric primitive-wrapper boundary family (CCF-003)** —
the handoff's recommended next family — and produced `docs/NumericWrapperBoundaryPlan.md`: file
and public-surface inventory, the five findings re-verified against current source and .NET,
cross-cutting overlap with CCF-005/006/007/008, the shared root causes, the class-A/B/C
classification, an approval matrix (nothing crosses the boundary), test/sanitizer matrices, and a
recommended ticket order. Tickets **#1844 (SR-AUD-024)**, **#1845 (SR-AUD-023)**, **#1846
(SR-AUD-022, P1 — live `std::clamp` UB)**, **#1847 (SR-AUD-021)** opened `todo` and ready; the
discovered-defect **#1848** (test exec bit) opened `todo`.

**Implementation ticket #1843** (`REMED-CORE-UINT128-SHIFT-MASK`, P1, `done`) remediated
**SR-AUD-020**, a **class-A** fix. `UInt128::operator<<`/`operator>>` forwarded an out-of-range
count to the native `unsigned __int128` shift — UB for a count of 128+ or a negative count,
reproduced under UBSan `-fno-sanitize-recover` (`UInt128.hpp:95:73: shift exponent 128 is too
large`). They now mask the count with `& 127`, matching .NET (`UInt128.cs:2051/2087`,
`shiftAmount &= 0x7F`) and the sibling `Int128`. Post-fix UBSan probe clean, out-of-range results
equal .NET's `& 0x7F`, in-range results byte-identical (`build-probe/1843_uint128_shift_*.log`).
**+3 permanent regressions** in `UInt128Tests.cpp`; `SharpRuntimeTests_Core_Base` **5056/5056**.
**Source and ABI consequences: none** — two inline operator bodies, no signature, member, vtable,
layout or symbol change.

**Audit tally: 30 remediated / 334 confirmed / 364 total** (SR-AUD-020 moved `confirmed →
remediated`; `AUDIT_FINDINGS_INDEX.md` updated). Numbering stays frozen at 364; **no new SR-AUD-\*
identifier** was issued by this batch.

## Post-audit remediation checkpoint — CCF-007 Pi-trig + parse whitespace (2026-07-30)

Batch `feature/remediation-batch-floating-fidelity`. Two units.

**Implementation #1861 (`REMED-CORE-PITRIG-FIDELITY`, P2, `done`)** remediated
**SR-AUD-032** (CCF-007). `Single`/`Double` `SinPi`/`CosPi`/`TanPi`/`SinCosPi`
were rewritten from the naive `std::sin(x*Pi)` forms to the .NET
integral/fractional-turn reduction, ported verbatim from `sinpi(f)`/`cospi(f)`/
`tanpi(f)` (amd/aocl-libm-ose, BSD 3-Clause) including the interval kernels
`SinForIntervalPiBy4`/`CosForIntervalPiBy4`/`TanForIntervalPiBy4` (the Double
kernels keep the reference `xTail` parameter, always `0.0` at the Pi-scaled call
sites). Integer turns now return a sign-carried zero, Sin half-turns `±1`, Cos
half-turns `0`, Tan half-turns `±Infinity`, non-finite inputs `NaN`, and ordinary
fractional values stay within libm ULPs. `noexcept`/`[[nodiscard]]`/signatures/
object layout unchanged (both header-only). **+20 permanent tests** (10 `SingleTest`
+ 10 `DoubleTests2`). UBSan + ASan + `float-cast-overflow` clean on the header-only
inline probe `build-probe/1861_pitrig_probe.cpp`. **Premise corrected:** the
CCF-007 plan §12 / ticket said `TanPi(1)==+0`; the reference returns `-0` for odd
positive integers (`Single.cs:2125`, `Double.cs:2209`), so `TanPi(+1)==-0` and
`TanPi(-1)==+0` — the tests assert the measured values. **Source and ABI
consequences: none** beyond the enlarged inline bodies. `SR-AUD-032 → remediated`.

**Implementation #1864 (parse whitespace slice, `done`)** landed the compatible
portion of **SR-AUD-033** parse: `Single`/`Double` `tryParseCore` trims
leading/trailing ASCII whitespace via a non-allocating `std::string_view` before
the NaN/Infinity token checks and `FromCharsFloat`; interior whitespace and an
empty/all-whitespace string still fail. `equalsIgnoreCaseAscii` widened to
`std::string_view`. **+8 permanent tests** (4 `SingleTest` + 4 `DoubleTests`).
The approval-gated tail (accept `,` thousands + return `±Infinity` on magnitude
overflow) was split to **needs_user #1865** (no new SR-AUD identifier — part of
SR-AUD-033), mirroring #1857→#1858. `SR-AUD-033` stays `confirmed` (partial) until
its format slice (#1863) and parse tail (#1865) both land.

**Design refinement** added `docs/FloatingValueFidelityPlan.md` §19 (reference-exact
decision records for #1862/#1863 and the #1854↔#1862 reconciliation and #1858↔#1865
comma decision), with reciprocal cross-refs in `ConversionBoundaryFamilyPlan.md`
§19.5 and `DecimalBoundaryFamilyPlan.md` §12. #1858's design was already precise
and is left blocked unchanged. No approval-gated implementation was performed.

**Audit tally: 43 remediated / 321 confirmed / 364 total** (SR-AUD-032 moved
`confirmed → remediated`; `AUDIT_FINDINGS_INDEX.md` updated). Numbering stays
frozen at 364; **no new SR-AUD-\* identifier** was issued. (The intermediate
30→42 progression from the CCF-004/CCF-005/CCF-006 and earlier CCF-007 batches was
tracked in `NEXT.md`, `plan.md`, and `AUDIT_FINDINGS_INDEX.md`; this file's
per-batch log resumes here.) Full component gate **14,396/14,396** across 37
executables; Doxygen 1,941/1,942; module graph 41/91; seams 2/18; negative
fixtures 9/66 — all green.


**CCF-019 design batch (#1885, 2026-07-30) — design-only, nothing remediated.**
SR-AUD-327 (`JsonNode`) and SR-AUD-333 (`XObject`) were reproduced against the
shipped bodies and planned; **no production file changed**. 47 cases, one forked
process each under a watchdog, three builds from one source (ASan+UBSan,
recoverable ASan, no sanitizer), every production translation unit compiled from
source into the probe so no archive could be stale: **29 ASan
`heap-use-after-free` accesses**, **3 `stack-overflow`s**, **57 reads / 0 writes**
under recoverable ASan, and **12** further cases wrong with no diagnostic at all.
Six premises corrected by measurement (surface is 76 public entries across 27
headers and 13 bodies, not nine files and two accessors; SR-AUD-333 aborts the
process through a **virtual** call on freed storage at eight entry points with no
sanitizer). Selected contract: **the owner detaches what it owns in its own
destructor** — `docs/OwnedTreeLifetimeContractPlan.md` — at zero layout, zero
vtable, zero allocation and zero per-access cost; the strong-parent-link
alternative that matches .NET exactly was rejected on a LeakSanitizer-confirmed
leak (2 constructed, 0 destroyed), and the `weak_ptr` alternative on the
repository's own 77 automatic-storage containers. **Audit tally unchanged: 57
remediated / 306 confirmed / 364 total**, two of the 306 now carrying the
`confirmed (design-complete)` qualifier; numbering stays frozen at 364 and **no
new SR-AUD-\* identifier** was issued. Implementation is #1886–#1894, all
`needs_user` or `blocked` pending six explicit approvals. Baselines carried
forward unchanged (nothing was rebuilt): 14,568 tests / 37 executables, module
graph 41/91, Doxygen 1,941/1,942, seams 2/18, negative fixtures 9/66. CNA and
mobile-eggbert were not inspected; #1773 stays `blocked`.


**CCF-019 core-repair batch (#1886 + #1890, 2026-07-31) — PARTIAL; neither
finding remediated.** The user approved
`docs/OwnedTreeLifetimeContractPlan.md` §31 **item 1 and only item 1**, which
covers both core repairs. **#1886**: `JsonArray` and `JsonObject` each declare a
destructor that clears the parent link of every child whose link still names that
container (the `== this` guard is load-bearing here — J08's aliasing copy and
J13's public `DetachParent()` both leave a container holding a child owned by
someone else). **#1890**: `XContainer` does the same for child nodes and
`XElement` additionally clears every owned attribute's parent link **and** its
intrusive `next_` sibling link, a second borrowed link that dangles
independently. Re-running the #1885 probe **unmodified**, same build script,
same three builds from one source: ASan `heap-use-after-free` **cases 29 → 3**,
faulting accesses **57 → 5**, and `pure virtual method called` process aborts
**8 → 0**. **26 of 29 closed** — one fewer than §1's estimate of 27, recorded in
§34.4 rather than rounded. The three that remain are **J11** (stale `JsonArray`
iterator → #1889) and **X15**/**X17** (`Extensions::Ancestors`' raw `XElement*`
and `getAttributesProperty()`'s reference → #1892); none reaches its defect
through `parent_`. **Audit tally unchanged: 57 remediated / 306 confirmed / 364
total**, and SR-AUD-327 and SR-AUD-333 both keep the
`confirmed (design-complete)` qualifier; numbering stays frozen at 364 and **no
new SR-AUD-\* identifier** was issued. Cost, measured: `sizeof` unchanged for all
eleven public types, GCC `-fdump-lang-class` class/vtable dumps identical
pre/post, zero allocations added to construction, access or destruction, LSan
clean; the only ABI movement is three weak COMDAT `XContainerD0/D1/D2Ev` symbols
GCC had previously inlined away, with no name removed. Baselines: **14,635 tests
/ 37 executables** (was 14,568; +32 `JsonNodeLifetimeTests`, +35
`XLinqLifetimeTests`, all 53 existing `JsonNodeTests` and 92 existing Xml.Linq
cases unmodified), module graph 41/91, Doxygen 1,941/1,942, seams 2/18, negative
fixtures 9/66. Mutation-checked five ways; ASan+UBSan+LSan clean over 179/179
Text.Json and 127/127 Xml.Linq with sanitizer activation proved by a controlled
self-test. §31 items 2–6 remain unapproved and unstarted (#1887, #1888, #1889,
#1891, #1892, #1893 `needs_user`; #1894 `blocked`). CNA and mobile-eggbert were
not inspected; #1773 stays `blocked`.

**CCF-019 exception-path batch (#1887 + #1891, 2026-07-31) — PARTIAL; neither
finding remediated.** Started under the user's batch instruction directing
`docs/OwnedTreeLifetimeContractPlan.md` §31 **item 2**, which grants no approval
for a source break, a vtable change, an object- or iterator-layout change, a
return calling-convention change or downstream migration — and needs none.
**#1887** reorders `JsonObject::SetItem` to adopt the incoming value before
detaching the value it replaces (probe case J10: the object no longer holds a
value reporting no parent, and a second container now rejects it). **#1891**
makes `XNode::ReplaceWith` put the replaced node back when a replacement is
refused, and `XContainer::InsertNodeAt` adopt after it inserts (probe case X20:
`<a>victim</a>` stays `<a>victim</a>` instead of becoming `<a/>`). **Both
CCF-019 data-loss paths are now closed; the ASan residue is unchanged at 3
use-after-free cases** (J11 → #1889, X15/X17 → #1892), 3 stack-overflows and 2
timeouts (→ #1893).

Evidence: the #1885 probe re-run unmodified, whose pre-change replay reproduced
the previous batch's recorded end state exactly (0 of 58 changed) before either
edit; diffing every answer line of the no-sanitizer build across all 58 cases
after both edits yields exactly **two** semantic differences in the whole matrix,
J10 and X20. **+48 permanent tests** (`JsonNodeMutationConsistencyTests` 22,
`XLinqMutationConsistencyTests` 26); floor **14,635 → 14,683 tests / 37
executables**; all 179 pre-existing Text.Json and 127 pre-existing Xml.Linq cases
pass unmodified. Six mutations across the two tickets (5, 12, 12, 3, and one
recorded honestly at **0** because it is only observable on `std::bad_alloc`).
ASan+UBSan+LSan clean over 201/201 Text.Json and 153/153 Xml.Linq. Module graph
41/91, Doxygen 1,941/1,942, seams 2/18, negative fixtures 9/66 — all unchanged.

**SR-AUD-327 and SR-AUD-333 both stay `confirmed (design-complete)`**; the
post-audit tally is **unchanged at 57 remediated / 306 confirmed / 364** and
**numbering stays frozen at 364**. The same batch completed design-only
compatibility reviews for #1888 (§37), #1892 (§38), #1889 (§39, the full
sixteen-item package) and #1893 (§40, root-cause classification); all four stay
`needs_user` and #1894 stays `blocked`. Five plan premises were corrected by
measurement and appended rather than rewritten (§35.4, §36.4, §38.1, §39.1,
§40.2/§40.3). CNA and mobile-eggbert were not inspected; #1773 stays `blocked`.

**CCF-019 compatible closure (#1895 + #1898, 2026-07-31) — neither finding
remediated.** The user decided §31 items 3–6: item 3 (#1888) and item 4 (#1889)
**declined** and moved `needs_user → blocked` with their designs preserved; item
5's wording **rejected as non-implementable** and replaced (§42) by #1898
(**done**) and #1899 (**blocked**, one question); item 6 **split**, its
compatible iterative-teardown half **approved** and delivered as #1895, its
quadratic half declined as #1896 and its parse half left as #1897.

**CCF-019 parse half (#1897, 2026-07-31) — the last stack overflow closed;
neither finding remediated.** The user approved **option B only**:
`JsonNode::Parse` now builds its tree with an explicit heap worklist instead of
recursing. Probe **X28c** goes from ASan `stack-overflow` to `clean`, so
**CCF-019 has no stack-overflow case left** and its only case reachable from
**untrusted input** is closed. Measured: SIGSEGV between 16,000 and 18,000
nested levels before, correct construction to 200,000 after; the round-trip,
null-semantics, malformed-input and sibling-ordering control output is
**byte-for-byte identical** before and after; 13 strong symbols before and 13
after, identical, with an identical undefined set; layout, vtable and exception
specification untouched; throughput neutral. Option **A** (apply the existing
`JsonDocumentOptions::DefaultMaxDepth = 64`, as .NET and this module's own
`JsonDocument::Parse` do) was **not approved and is not implemented**, so
`JsonNode::Parse` still accepts text .NET rejects — documented in its
doc-comment and pinned by two tests. **SR-AUD-327 and SR-AUD-333 both stay
`confirmed (design-complete)`** because J11, X15/X17 and J19d/X27d remain; the
post-audit tally is unchanged and numbering stays frozen at **364**. Evidence:
`docs/OwnedTreeLifetimeContractPlan.md` §44. CNA and mobile-eggbert were not
inspected; #1773 stays `blocked`.

**#1895** makes container teardown iterative: probe **J19c** and **X27c** go from
ASan `stack-overflow` to `clean`, and exactly 2 of 58 probe cases changed. All
six stated approval conditions are met and measured — no signature, vtable,
object-layout or iterator-layout change, no consumer migration, and no touch to
`DefaultMaxDepth`. **#1898** states and pins the Xml.Linq borrowed-view contract
without any source, ABI, layout or semantic change.

+48 permanent tests; gate **14,683 → 14,731 across 37 executables**. Module graph
41/91, Doxygen 1,941/1,942, seams 2/18, fixtures 9/66 — unchanged.
ASan+UBSan+LSan clean over 218/218 and 184/184.

**SR-AUD-327 and SR-AUD-333 both stay `confirmed (design-complete)`**; the tally
is **unchanged at 57 remediated / 306 confirmed / 364**; numbering frozen at
**364**; **no new ticket carries an `SR-AUD-*` identifier**. Five plan premises
were corrected by measurement (§40.2/§40.3, §42.2, plus §39.1 and §38.1 retained
from the previous batch). Next family selected and planned: **CCF-009**
(`docs/SharedPrngConcurrencyPlan.md`, #1900–#1903). CNA and mobile-eggbert were
not inspected; #1773 stays `blocked`.

## Post-audit remediation checkpoint — CCF-009 shared PRNG concurrency (2026-07-31)

**Tickets #1901, #1902, #1903 — all `done`. CCF-009 is COMPLETE and SR-AUD-010 is
`remediated`.** This is the first post-audit family to be finished outright
rather than left design-complete or approval-blocked; it was chosen for exactly
that reason (`docs/SharedPrngConcurrencyPlan.md` §1).

**#1901** gave `Guid::NewGuid` a per-thread engine and distribution;
`CreateVersion7` inherited it. **#1902** gave `Random::getSharedProperty()` an
ownership boundary **without** making the shared object per-thread: it keeps one
stable address on every thread, and `internalSample()` — the sole writer of the
generator state and the funnel all entry points pass through — routes the draw to
the calling thread's generator. The per-thread-instance shortcut is the one .NET
rejects at `Random.cs:755–759`. **#1903** closed the finding only after both
landed.

TSan: **13** races at `Guid.cpp:344` and **6** in `Random::internalSample()`
before, **0** at both sites after. Seeded `Random(seed)` byte-identical across
4,928 dumped values. 100,000 concurrent `NewGuid()`, zero duplicates.
+14 permanent tests; gate **14,731 → 14,745 across 37 executables**. Module graph
41/91, Doxygen 1,941/1,942, seams 2/18, fixtures 9/66 — unchanged.
ASan+UBSan+LSan clean over 168/168. No header touched; `sizeof`/`alignof` of
`Random` and `Guid` unchanged and asserted; external symbols identical before and
after on both objects.

Four mutations run; the two that matter (`thread_local` shared instance;
identically-seeded per-thread engines) are **race-free and invisible to every
sanitizer**, and are caught by the new tests alone.

**Tally moves for the first time in several batches: 57 → 58 remediated, 306 →
305 confirmed, 364 total.** Numbering frozen at **364**; no new `SR-AUD-*`
identifier; no `CCF-*` cause added. SR-AUD-050 (predictable PRNG at the same
lines) stays `confirmed` — unrelated to ownership. CNA and mobile-eggbert were
not inspected; #1773 stays `blocked`.

## Post-audit remediation checkpoint — Group A of the approved A–D packet (2026-07-31)

**Tickets #1854 and #1862 — both `done`. SR-AUD-043 and SR-AUD-029 are
`remediated`.** Approved by the batch instruction in the exact words of
`docs/RemainingApprovalDecisions.md` §A.10 — option **A(i)**, drop `noexcept`
(and the one `constexpr`), throw. Both tickets were decided the same way in the
same batch, which is what §A.6 asked for: the project now has **one** convention
for "an argument check is blocked by an exception specification", not two.

Five exception specifications and one `constexpr` were removed.
`Single::Round(float,intcs)` and `Double::Round(double,intcs)` reject `digits`
outside `[0,6]` / `[0,15]` before the `std::pow`, with .NET's own resource
strings verbatim; `ReadOnlyMemory<T>`'s three constructors and
`HashCode::AddBytes(ReadOnlySpan<uint8_t>)` reject a negative length, an
oversized `std::vector` and a negative-offset/count `ArraySegment`.

**Measured, 35 probe cases in one process each**
(`build-probe/1854_{prefix,postfix}_plain.log`): eleven previously-silent wrong
answers became throws — `Round(1.2345f,99)` was **NaN**, `Round(1.2345f,-1)` was
**0**, `Round(1.2345,16)` silently ignored the request, and
`ReadOnlyMemory<uint8_t>(data,-1)` constructed with `length == -1`. Every valid
input is **byte-identical** before and after.

**Two premises corrected, both preserved additively.** (1) §A.5 called #1854
"pure defence in depth"; measured, only two of its three halves are unreachable —
`ReadOnlyMemory(const T*, intcs)` was directly reachable. The tests say so
site-by-site rather than averaging: the reachable site gets behavioural
assertions, the two unreachable ones get `static_assert`s on the exception
specification plus unchanged-valid-behaviour tests, and comments stating the
throw cannot be triggered publicly. (2) §A.8's `constexpr` concern is
unobservable — no accessor on `ReadOnlyMemory<T>` is `constexpr`, so a
`constexpr`-constructed view could never be queried in a constant expression.

**Two newly discovered defects filed as inactive tickets, not absorbed.**
**#1927** — `Single::Round`/`Double::Round` re-implement the rounding inline
instead of delegating to `MathF::Round`/`Math::Round` as .NET does, so
`Single::Round(3.0e38f,6)` and `Double::Round(1e300,15)` return **`inf`** where
the port's own `MathF`/`Math` return the value unchanged. **#1928** —
`Math::Round`'s message is missing .NET's leading `"Rounding "`. Both are value
or message changes on currently-valid input, outside this approval. **No new
`SR-AUD-*` identifier; numbering frozen at 364.**

+24 permanent tests; gate **14,920 → 14,941 across 37 executables** (the count is
+21, not +24: three of the additions replaced or extended existing cases in
place). ASan and UBSan over all 35 probe cases: **zero diagnostics, answers
identical to the plain build** — which restates rather than claims coverage,
since neither sanitizer can see a missing argument check. No parameter list,
return type, object layout, vtable or mangled name changed; an Itanium mangled
name does not encode `noexcept`, so there is **no exported-symbol break**.

**Tally: 59 → 61 remediated, 305 → 303 confirmed, 364 total** (SR-AUD-029 and
SR-AUD-043 both `confirmed → remediated`; SR-AUD-043's status string was
`confirmed (043a remediated; 043b open)` and is now plain `remediated`).
**CCF-005 is complete.** CCF-007 keeps SR-AUD-033's format (#1863) and parse
(#1865) slices. CNA and mobile-eggbert were not inspected; #1773 stays `blocked`;
#1888/#1889/#1896 stay declined.

## Post-audit remediation checkpoint — Group B of the approved A–D packet (2026-07-31)

**Tickets #1865 and #1858 — both `done`. SR-AUD-035 is `remediated`; SR-AUD-033's
parse half is closed and its format half stays open as #1863.** Approved by the
batch instruction in the exact words of `docs/RemainingApprovalDecisions.md`
§B.8 — option **B(i)**, staged in the three commits §B.5 required so that the one
dangerous row can be reverted alone.

**Commit 1 (#1865, rows B-3 and B-4).** `Single`/`Double` `tryParseCore` now
implements the two parts of `NumberStyles.Float | AllowThousands` that
`std::from_chars` cannot express, through one shared private header
`System/detail/FloatParseGrammar.hpp` so the two types cannot drift: `,` group
separators by .NET's exact scanner rule (accepted only after a digit and before
the decimal separator; group **sizes** deliberately unvalidated, because they are
a formatting concept), and an out-of-range magnitude saturating instead of
throwing.

**Commit 2 (#1858, row B-2).** The `Decimal` scanner became a private static
returning `ParseStatus { Ok, Malformed, Overflow }`, so `Parse` throws .NET's
`OverflowException` for a well-formed oversized magnitude while malformed text
stays a `FormatException`. `TryParse` keeps its `bool` and its no-partial-write
guarantee.

**Commit 3 (#1858, row B-1) — the one dangerous change, isolated on purpose.**
`,` is now the invariant-culture group separator in `Decimal` too. Migration
note: `docs/Migration-DecimalCommaGroupSeparator.md`.

**Three premises corrected, all preserved additively.**

1. **The overflow repair is wider than §B.3's single row, and legitimately so.**
   Measured (`build-probe/1865_prefix_plain.log` cases B01–B08), `std::from_chars`
   returns **one** `errc::result_out_of_range` for overflow *and* underflow and
   leaves the output **unwritten** in both. §B.3 lists only `"1e999"`, but §B.6
   describes the implementation as "a `result_out_of_range`-with-all-chars-consumed
   branch" — one branch, both directions — and .NET decides both in one function
   (`Number.NumberToFloat`: `PositiveInfinity` above `MaxDecimalExponent`, `Zero`
   below `MinDecimalExponent`, sign applied afterwards). Underflow is therefore
   structurally equivalent under exactly the same recorded contract, and is
   repaired here: `"1e-999"` → `+0`, `"-1e-999"` → `−0`.
2. **B-1 changes two values, not one.** §B.3 names `Parse("1,5")` (`1.5m` →
   `15m`). Measured, `Parse(",5")` also changes — `0.5m` → `FormatException` —
   because a group separator requires a preceding digit, which is what .NET does.
   Tabulated in the migration note rather than left to be discovered.
3. **The packet disagrees with itself about B-1, and that is recorded rather than
   resolved silently.** §0's summary row recommends "Approve B, split — take
   #1865 whole, take only #1858's overflow half", i.e. **not** B-1; §B.5
   recommends "B(i), staged" and §B.8's approval wording approves B-1 explicitly,
   as its own commit, with a migration note. The packet's preamble designates
   §"Approval wording" as the operative sentence, so B-1 was implemented — and
   the isolation the approval itself demanded is exactly what makes reverting it
   alone a one-command operation if §0 was the intent.

`TryParse`'s `noexcept` is unchanged on all three types — the one allocation the
separator path can make is guarded — so no exception specification moved. That
would have been a Group A-shaped change, and Group B does not approve one.
`Half` inherits the widened float grammar by delegation with no edit, pinned by a
test.

+38 permanent tests (two of them replacing the two `*_PendingApproval` tests,
which §B.7 required to be inverted); gate **14,941 → 14,964 across 37
executables**. ASan and UBSan over 68 probe cases with `Decimal.cpp` compiled
**into** the probe so the `.cpp` half is instrumented too: **zero diagnostics,
answers identical to the plain build** — restating rather than claiming coverage,
since no sanitizer can see a grammar or exception-taxonomy defect. No public
signature, exception specification, object layout or ABI change.

**Tally: 61 → 62 remediated, 303 → 302 confirmed, 364 total** (SR-AUD-035
`confirmed → remediated`; SR-AUD-033 stays `confirmed` with its parse half
closed, because #1863 still owns the format half). **The CCF-005 Decimal slice is
complete.** CNA and mobile-eggbert were not inspected; #1773 stays `blocked`;
#1888/#1889/#1896 stay declined.

## Post-audit remediation checkpoint — Group C of the approved A–D packet (2026-07-31)

**Tickets #1879 and #1884 — both `done`. SR-AUD-007 (with 007a), SR-AUD-009,
SR-AUD-061 and SR-AUD-015 are all `remediated`; CCF-012 is complete.** Approved by
the batch instruction in the exact words of
`docs/RemainingApprovalDecisions.md` §C.8. Group C is the batch's **behaviour-
incompatible by design** group: text the library used to accept is now rejected —
and unlike group B's comma, **the caller finds out**, through `false` or a
`FormatException`.

**#1879** replaced `std::sscanf` in all four date/time parsers with
`System::detail::DateTimeTextScanner` and a whole-string consumption rule.
`"2024-06-15junk"` was a valid date; `"2024-06-15 10:xx:00"` and
`"2024-06-15 trailing"` both parsed as **midnight**; `DateOnly::TryParse` read a
full timestamp and silently kept only its date.

**#1884** gave `String::Format` and `FormattableString::ToString` one shared
scanner transcribed from `ValueStringBuilder.AppendFormat.cs`. Beyond the fourteen
approved rows it closes a divergence neither finding named: **the two engines
disagreed with each other**, not only with .NET — `"{{0}}"` threw in one and
produced `"{v}"` in the other; a missing index threw in one and stayed literal in
the other.

**Five premises corrected, all preserved additively.**

1. **`docs/DateTimeValidationBoundaryPlan.md` §20.1 is wrong about .NET for two of
   its fifteen rows.** `ParseFraction` (`Globalization/DateTimeParse.cs:479-492`)
   accepts `".1234567"`; `ParseTimeZone` (`:530-548`) accepts `"+2:5"` and reads
   it as 2h05m — **125 minutes, exactly what the port already produced**, so
   §C.4's "wrong answer that survives round-tripping" was not wrong at all. Both
   rejections are deliberate **narrowings** of this port's fixed-width,
   millisecond-resolution subset — the same subset that has always rejected
   `"2024-6-15"` — implemented as approved and recorded, with the widening
   question filed as inactive **#1929**.
2. **Replacing `sscanf` removes `%d`'s own leniencies too**, which §20.1 does not
   list: `" 024-06-15"` parsed as year 24, and `"+10:20:30"`,
   `"2024-06-15  1:20:30"` and `"+ 2:00"` all parsed. Structurally the same
   defect, repaired under the same approval.
3. **§20.1's test matrix names "the four `Ccf002_*GrammarIsPinnedUnchanged`
   tests"; there are two**, plus one unanticipated `DateTimeTests` fraction test
   that also had to be inverted.
4. **`docs/CompositeFormatBoundaryPlan.md` §20.1 row 5 names the wrong reason.** A
   **trailing** `}` runs .NET's `MoveNext` off the end first, so the reference
   reports `Format_UnclosedFormatItem`, not `Format_UnexpectedClosingBrace`. The
   port matches .NET and pins both spellings.
5. **`TimeOnly`'s unpadded `"1:2:3"` was deliberately kept**, because .NET accepts
   it; padding it would have been a fresh divergence. The narrowing is applied
   exactly where the packet asked and nowhere else.

**Deliberately not adopted:** whitespace inside a format item (`"{0 }"`,
`"{0, 6}"`), which .NET accepts. It is a *widening*, no approved row asks for it,
and §20.7 authorises only the §20.1 rows — recorded in the plan §21.4.

+51 permanent tests; gate **14,964 → 14,987 across 37 executables**. ASan and
UBSan over 82 + 36 probe cases with the five affected `.cpp` files compiled
**into** the probes: **zero diagnostics before and after**, answers identical to
the plain builds — restating rather than claiming coverage, since no sanitizer can
see an over-permissive grammar. **No public signature, `noexcept` specification,
virtual function, vtable slot, data member, `sizeof`, `alignof` or member offset
changed** in either ticket.

**Tally: 62 → 66 remediated, 302 → 298 confirmed, 364 total.** **CCF-012 is
complete** (#1881, #1882, #1883, #1884). CCF-002's remaining member is #1880
(CCF2-E, `TryParse` failure output), inactive. **No new `SR-AUD-*` identifier;
numbering frozen at 364.** CNA and mobile-eggbert were not inspected; #1773 stays
`blocked`; #1888/#1889/#1896 stay declined.

---

## Post-audit CCF-002 closure — ticket #1880 (2026-08-01)

All four measured date/time TryParse doors now publish MinValue on false,
matching current .NET and the already-established CCF-014 repository
convention. The output-preservation wording in their original audit records is
retained but explicitly corrected. +4 permanent matrices pass (5,585
Core.Base); affected ASan+UBSan objects were proven current and focused tests
are clean. No public declaration, `noexcept`, `constexpr`, layout, vtable,
symbol or module-edge change; no audit identifier. Totals remain **67 / 297 /
364** and CCF-002 is closed. Evidence: date-time plan §23.

---

## Post-audit HResult population — ticket #1875 (2026-08-01)

The deliberately inactive 45-type population from CCF-016 was independently
confirmed still wanted and compared against official current-.NET source at
`dotnet/runtime` commit `0eb5481340ea675857c7a7abf18f68a60b52a686`. The
historical two-category premise was corrected: 12 types assign a dedicated
constant, 30 purely inherit, and 3 conditionally propagate. The port was exact
for 27 pure controls. All 36 represented constructors of the 12 dedicated rows
now assign the exact code; the reduced Win32 root assigns inherited `E_FAIL`,
which also corrects NetworkInformation, Socket and the represented WebSocket
surface. The two separable inner-HResult gaps are retained as inactive #1932.

Prefix 13/15 failing → postfix 15/15 passing, with 70 exact permanent
assertions; the full integration executable passes 880/880. Combined
ASan+UBSan focused tests pass 15/15 against an instrumented object newer than
all changed headers. Leak discovery hit the already-known ptrace limitation,
so the successful semantic run disabled leak detection honestly; no ownership,
allocation or shared-state mechanism changed. SR-AUD-157 moves to
`remediated`; SR-AUD-158, SR-AUD-159, SR-AUD-196, SR-AUD-230 and SR-AUD-250
retain their prior states. Audit totals are **68 remediated / 296 open / 364
total**, numbering frozen and no new identifier issued.

---

## Conditional HResult follow-through — ticket #1932 (2026-08-01)

Exact Option 2R is now implemented for HttpRequestException H3/H4/H5 and
WebException W3/W5 only. Exact System inner HResults—including zero—propagate;
null/non-System pointers retain the family base; error/status metadata does not
win. The new 13-test permanent matrix and sync/async forwarding controls pass.
The change has no declaration, ABI, layout, vtable, symbol, `noexcept`, or
`constexpr` consequence. Audit numbering and totals remain **68 / 296 / 364**;
no new finding was created.

---

## ImmutableSortedSet comparator-equivalence closure — ticket #1936 (2026-08-01)

The user approved exact Option 1 for the post-audit generic defect discovered
during #1925. `ImmutableSortedSet<T>::SetEquals` now preserves this-set
comparer precedence, rebuilding, and the post-collapse count check, then uses
a two-direction comparator-equivalence range scan for every `T`, with a true
shared-backing fast path. Direct float/double/long-double NaN sets and generic
custom-comparer sets are reflexive; their proper-subset/proper-superset
contradiction is closed. Nullable-floating policy and every other collection
remain unchanged.

Eleven permanent tests raise Collections.Core to 2,763 and the socket-enabled
repository gate to 15,092 tests across 37 executables. Eight mutations have no
unexpected survivor; six are killed and the removed count check and fast path
are correctly classified behaviorally equivalent. ASan+UBSan focused tests
and a 2,000-iteration lifetime/stateful-comparer probe are clean; LSan cannot
claim discovery because its post-test ptrace step is denied. Public
declarations, aliases, iterator types, layout, vtables, `noexcept`, and
`constexpr` are unchanged; approved inline-template helper/body symbols move.
Audit numbering stays frozen at 364 and totals remain **68 remediated / 296
open / 364 total** because #1936 has no SR-AUD identifier.

## Post-audit remediation checkpoint — the `System::Threading` namespace review (2026-08-03)

Ticket **#1950** performed the namespace review that the drained remediation queue
called for: `plan.sqlite3` held zero `todo` tickets, and the `task` porting queue is
exhausted, so the next unit of work had to be selected from the audit inventory.
`modules/threading/` was chosen over the larger `modules/core/` (72 open) because its
38 open findings — **14 high** — are the largest untouched severity concentration
that no existing `docs/*Plan.md` already covers, and because every proposed repair is
internal to a component whose only public dependencies are `Core.Base` and `TimeZone`.
The durable record is `docs/ThreadingNamespaceReviewPlan.md`, which carries the file
and public-surface inventory, all 38 findings mapped to nine root causes, the
dependency order, the source/ABI/layout approval matrix, the test and sanitizer
matrices, twelve bounded tickets (#1947–#1949, #1951–#1959), explicit exclusions and
namespace completion criteria. **No new `SR-AUD-*` identifier was issued.**

Four findings were remediated in the same batch, all with reproduction before repair:

- **SR-AUD-206** (high, #1947) — `Semaphore::Release` and `SemaphoreSlim::Release`
  computed `count_ + releaseCount > maxCount_` in signed `intcs`. Both guards now use
  .NET's own non-overflowing `maxCount_ - count_ < releaseCount`. Two corrections to
  the finding's recorded extent are documented in the owning per-file report: the
  pre-fix UBSan probe reports **four** sites, not two (the increment overflows
  independently of the guard), and the surviving state was
  `CurrentCount == -2147483648`, which left the instance permanently unusable rather
  than merely under-diagnosed.
- **SR-AUD-211** (high, #1948) — `CountdownEvent::Reset` reached the signalled state
  without notifying its condition variable. Reproduced exactly as recorded
  (`reset0=TIMEOUT`, exit 1) and closed with an unconditional `notify_all()`, with a
  control test proving a reset to a non-zero count still leaves a waiter blocked.
- **SR-AUD-195** and **SR-AUD-197** (low, #1949) — two test assertions that could not
  fail (`state & 0 == 0`; a `(void)id` cast). Both replaced with assertions that can.
  **SR-AUD-193 was deliberately not asserted either way** and remains `confirmed`.

Three further defects were found while establishing the repository baseline and are
recorded as their own tickets rather than absorbed silently. **#1946** and **#1960**:
the repository **did not compile at all** on GCC 13 / libstdc++ 13 — `MathF::Round`
used `std::floorf`/`std::ceilf`, which libstdc++ does not declare, and a deliberate
`const T&` binding in `ListIndexerProxyTests.cpp` tripped GCC 13's new
`-Wdangling-reference` under `-Werror`. **#1961** (P0): `Dns::GetHostEntry(IPAddress)`
recursed without bound and killed the process with SIGSEGV whenever the queried
address had no reverse mapping, because `getnameinfo` succeeds and returns the address
in numeric form, which the string overload re-parsed as a literal and fed straight
back. All three are fixed. **#1962** is opened `blocked` and unstarted: `Ping` only
ever opens an unprivileged `SOCK_DGRAM` ICMP socket, so it fails wherever
`net.ipv4.ping_group_range` is closed even when the process holds `CAP_NET_RAW`.

The index now reads **72 remediated / 292 open / 364 total**.

**Gate result, reported exactly as measured.** 37 executables, **15,105 tests**,
15,098 passing, 1 skipped (a locale-dependent invariant-formatting case), **6 failing
for two environment reasons that are not caused by this batch and were not hidden**:
five `PingTests` cannot open an ICMP socket because this container's
`net.ipv4.ping_group_range` is `1  0` (ticket #1962 — a real gap in `Ping`, left
open), and one `SocketTests` case cannot construct an `AF_INET6` socket because IPv6
is absent from the container entirely (`/proc/net/if_inet6` does not exist). **No test
was disabled, skipped or made conditional to obtain a green run.**


---

## Post-audit remediation batch, 2026-08-03 — `Threading.Tasks` + `Threading.Channels` (#1965–#1968)

Branch `feature/remediation-batch-tasks-channels-1965-1968`. The queue is the one
`docs/ThreadingTasksChannelsReviewPlan.md` §11 opened; **no new `SR-AUD-*` identifier is
issued and numbering stays frozen at 364**. Note that the preceding
`System::Threading` batch (#1947–#1949, #1951–#1955) did not append here, so the
last tally recorded above (72 remediated) is stale; the authoritative count is the
one derived from `AUDIT_FINDINGS_INDEX.md`, which read **87 remediated / 277
confirmed / 364 total** when this batch began.

### #1965 — SR-AUD-231 (cause TC-A, CCF-011 in a third module)

`confirmed → remediated`. Every public `Threading.Tasks` entry that stores a callable
now decides emptiness at the boundary, before any worker starts or continuation is
registered, with .NET's own exception type and parameter name.

Five premises corrected by measurement
(`build-probe/1965_probe1_tasks_empty_callables.cpp`, 37 cases; before/after/ASan logs
retained), all recorded in the owning per-file report:

1. **22 public entries, not the two named** — eleven distinct bodies edited, the rest
   inheriting the check by forwarding.
2. **`Parallel`'s failure was already catchable.** It arrived as
   `System::AggregateException`, so CCF-011's "outside the `System::Exception`
   hierarchy" consequence applies only to the sixteen `Task`/`TaskT`/`TaskFactory`
   entries, where a bare `std::bad_function_call` escaped.
3. **`Parallel::Invoke` is not an `ArgumentNullException` site.** .NET reports a null
   *element* of the actions array with a plain `ArgumentException` carrying no
   parameter name. This is the same trap `docs/ThreadingNamespaceReviewPlan.md` §17.1
   recorded for `LazyInitializer` and `SynchronizationContext::Send`: the family's
   usual spelling would have swapped one non-matching result for another.
4. **The failure was data-dependent in two unnamed shapes** — a zero-length range or
   empty source ran no iteration, and an already-cancelled token short-circuited
   before the action, so both returned normally with an empty callable.
5. **`ContinueWith` did register** the empty continuation and returned a *faulted*
   task rather than reporting an argument error.

**Reference-evidence limitation.** `/rv/tmp/runtime/src/libraries/` is not present in
this environment, so .NET parameter names could not be re-read from local source; they
come from the API contract for the exact overloads, and where the answer was uncertain
the conservative base type was chosen, following #1954's precedent for SR-AUD-184.

Post-fix probe: **zero** `bad_function_call` and **zero** deferred-aggregate outcomes
across all 22 entries; every valid-callable case byte-identical. ASan + UBSan + LSan
**0 reports before, 0 after** (34 sanitizer symbols present in the instrumented binary,
0 in the plain one). `SharpRuntimeTests_Threading_Tasks` **171 → 208**. No public
signature, object layout, vtable, `noexcept` specification or component edge changed.

**Index after #1965: 88 remediated / 276 confirmed / 364 total.**

### #1966 -- SR-AUD-232 (cause TC-B/1)

`confirmed -> remediated`. `ParallelOptions::MaxDegreeOfParallelism` is validated at the
entry of the one method that reads it: -1 keeps its "unlimited" meaning, every value >= 1
is honoured exactly, and 0 together with every value <= -2 throws
`ArgumentOutOfRangeException("MaxDegreeOfParallelism")` before any iteration is dispatched.
Measured before the repair (`hardware_concurrency = 4`): degrees -3, -2 and 0 each ran the
whole loop at peak concurrency 4, confirming the report's claim that an invalid cap
silently became a core-count cap.

Three points recorded rather than assumed:

1. **The repair cannot be placed where .NET places it.** .NET validates in the
   `ParallelOptions` setter; this port's field is a public mutable data member with nowhere
   to put a check. Only the point of detection moves -- the exception type and parameter
   name are .NET's. This is the same shape that gates SR-AUD-235 as #1969, and the reason
   this one is nonetheless compatible is that `Parallel::For` *reads* the option, so there
   is a public entry at which to reject before any work runs.
2. **The degree check must precede #1965's `body` check**, and on the intermediate tree it
   did not: the port reported the body error where .NET reports the degree error.
3. **One site.** This port has no `ForEach` options overload; the other four loop overloads
   are untouched, which a regression asserts.

ASan + UBSan + LSan 0 reports. +10 tests (208 -> 218). No signature, layout, vtable or edge
change.

**Index after #1966: 89 remediated / 275 confirmed / 364 total.**

### #1967 -- SR-AUD-234 (cause TC-C)

`confirmed -> remediated`. `ChannelReader<T>::ReadAsync` on an error-completed channel throws
`ChannelClosedException` with the writer's exception retained as its inner exception, for both
the FIFO and the prioritized shapes. `WaitToReadAsync`, `WaitToWriteAsync` and
`getCompletionProperty()` keep exposing the cause unwrapped, which is .NET's contract for them
and what makes the wrapper meaningful; buffered items are still handed over before the closure
is reported.

**One premise corrected, and it changes the finding's extent.** The report states that *"only
ReadAsync loses its API-specific closed channel boundary"*. Measured, `ChannelWriter<T>::WriteAsync`
has the identical defect from the identical code shape: a cleanly completed channel gave it a
`ChannelClosedException`, an error-completed one let the raw producer error escape. .NET routes
both through `ChannelUtilities.GetInvalidCompletionValueTask`. Repaired together as the same
defect at a second site -- the same treatment SR-AUD-008 and SR-AUD-183 received -- with **no new
`SR-AUD-*` identifier**; numbering stays frozen at 364.

A `ChannelClosedException` raised by a consumer subclass's own `WaitToReadAsync`/`WaitToWriteAsync`
is passed through rather than nested inside a second one.

**Concurrency evidence, with the probe's capability proved first.** Four scenarios x 400 rounds
on a fresh channel per round, the completing thread released by a shared latch: one blocked
reader, four blocked readers, three writers blocked on a full bounded channel, and a mixed
`ReadAsync` + `WaitToReadAsync` pair on a prioritized channel. Against the pre-fix header the
probe reported **3,600 wrong outcomes out of 3,600**; against the repaired header, **0**.
ThreadSanitizer reported **0 data races in both runs** -- the correct result for an
exception-contract defect -- and the pre-fix run is what makes that zero evidence about the code
rather than about the probe, per `docs/ThreadingNamespaceReviewPlan.md` §19.4. Every translation
unit on the racing path was compiled from source with `-fsanitize=thread`; no archive was linked
in. ASan + UBSan + LSan: 0 reports.

+13 tests (39 -> 52). No signature, layout, vtable or edge change; `ChannelReader`'s
`enable_shared_from_this` lifetime design is untouched.

**Index after #1967: 90 remediated / 274 confirmed / 364 total.**

### #1968 -- SR-AUD-233 (cause TC-B/2)

`confirmed -> remediated`. A zero-capacity bounded channel is a rendezvous channel:
`TryWrite`/`TryRead` return false with no waiting peer, a parked reader is handed a writer's
item directly, `WriteAsync` blocks until a reader arrives, and `Count` stays 0. Every non-zero
capacity, the unbounded and prioritized channels, and the drop modes at capacity >= 1 are
byte-identical before and after.

**The load-bearing part of this ticket is an evidence conflict inside the repository, resolved
in the open.** `Channel.hpp`'s `effectiveCapacity()` carried a comment asserting it had been
*"verified against BoundedChannel.cs"* that a capacity-0 channel *"still buffers one item"* and
is *"observably equivalent to a capacity-1 channel for every publicly-visible outcome"* -- the
exact opposite of SR-AUD-233. The finding was preferred because its evidence is a **behavioural
managed probe** while the comment's is a **reading of source**, because it is the later record
and was re-checked by #1964's review, and because the comment is self-undermining (it concedes
.NET has a direct-hand-off path, then asserts that path changes no return value). The
contradicting comment was **replaced, not silently dropped**.
`/rv/tmp/runtime/src/libraries/` is absent from this environment, so the reading could not be
adjudicated directly; the repair is confined to the capacity predicates and three methods and is
revertible without touching any signature.

The drop modes at capacity 0 had no evidence either way -- the audit's probe covers only `Wait`
mode -- and now discard the item and keep `Count == 0`, which is the only reading consistent
with a channel that has no room: the incoming item is simultaneously the newest and the oldest.
Recorded as reasoned, not measured.

**Layout gate.** Every public type is unchanged and now pinned by `static_assert`:
`Channel<int>` 32, `ChannelReader<int>`/`ChannelWriter<int>` 24, `ChannelOptions` 16,
`BoundedChannelOptions` 24, both `detail` impls 40, all with alignment 8. The single growth is
`detail::ChannelState<int>` (240 -> 248) for the waiting-peer counter -- a `detail` type that
appears in no public signature, in an INTERFACE target where nothing can be linked across
versions.

**Concurrency evidence.** Four scenarios x 300 rounds on a fresh channel: one reader/one
producer, four readers/four producers, a completion racing a parked reader, and three writers
blocked in `WaitToWriteAsync` racing an arriving reader. Capability proved first: **600 wrong
outcomes against the pre-fix header, 0 after**. The close-while-parked and WaitToWriteAsync-race
scenarios reported 0 in both runs, recorded honestly -- a capacity-1 buffer also delivers every
item, so only the "nothing may be buffered" assertions discriminate. TSan 0 data races in both
runs, fully instrumented from source. ASan + UBSan + LSan 0 reports.

**Test rewrite, identified as such.** The two zero-capacity tests that pinned the incorrect
capacity-one behaviour were **replaced, not deleted**; each old concern still has a test
asserting the corrected contract. +12 tests (52 -> 64).

**Index after #1968: 91 remediated / 273 confirmed / 364 total.**

### #1971 -- SR-AUD-214 and SR-AUD-189 (cause T-H, the verified-compatible half of #1958 Group A)

Both `confirmed -> remediated`. **#1958 remains blocked** and keeps its other six members.

`docs/ThreadingNamespaceReviewPlan.md` §20.3 listed **three** members as compatible and needing
no approval. That claim was verified independently by measurement before anything was split, and
**it holds for two of the three**.

**SR-AUD-215 was excluded and stays with #1958.** §20.3 attached its own caveat -- that
`Capture()` always returns `nullptr` -- and treated it as a test problem. Measured, it is a
reachability problem: `Capture()` returns null unconditionally, the default constructor is
private (proved by a compile probe), and `CreateCopy()` is a non-static member needing an
instance. There is no reachable way to obtain a non-null `ExecutionContext*`, so rejecting null
would make `Run` throw for **every** call a consumer can write, including
`Run(Capture(), callback, state)`, which works today. That is mandatory downstream migration with
no working alternative, not a validation-only change; landing it needs a `Capture()` that returns
a real context, which is an ownership and lifetime design. #1958's approval request (A) is
re-worded accordingly, and two regressions pin the current contract so the exclusion is testable.

**SR-AUD-214** reproduced exactly and is confirmed on the half it states only in passing: a
reentrant write from inside the value-changed handler was overwritten by the delayed outer
assignment. `setValueProperty` now commits before notifying; the equal-value no-op is untouched
and separately pinned.

**SR-AUD-189** is wider than the probe the report quotes -- `SetMinThreads(0,0)`,
`SetMaxThreads(0,0)` and a maximum below the current minimum also all returned true. Every rule
of the repair carries its provenance, since `/rv/tmp/runtime/src/libraries/` is absent here:
measured (negative rejected; a valid pair stored and observable), .NET-documented (the min/max
consistency rule; a maximum of at least 1), or reasoned (zero accepted as a minimum). .NET's
further rule refusing a maximum below the processor count is deliberately **not** adopted --
unverifiable here and machine-dependent, the same reasoning as #1952's `MaxWaitHandles`. Measured
consequence recorded: on this four-core container `SetMinThreads(7,9)` now returns **false**
because the default maximum is 8, where the audit's 16-core managed probe got `True`.

`ThreadPool` is static-only, so the stored configuration lives in function-local statics behind
one mutex and **no object layout exists to change**; §3.1 item 4's opportunistic `int -> intcs`
correction is folded in and shown mangling-identical by `nm`.

Concurrency, capability proved first: the AsyncLocal ordering scenario reported **8,000
violations before, 0 after**; the ThreadPool scenario reported 0 in both runs, recorded honestly
because setters that store nothing cannot break an invariant. TSan 0 data races in both runs;
ASan + UBSan + LSan 0 reports. +16 tests (429 -> 445).

**Index after #1971: 93 remediated / 271 confirmed / 364 total.**

## The `System::Runtime` namespace review and its compatible half — #1972–#1978, #1982 (2026-08-03)

`docs/SystemRuntimeNamespaceReviewPlan.md` (ticket **#1972**) converts the 21 open findings
in `modules/runtime/` into **twelve** root causes and a bounded queue (#1973–#1986). It
issues **no new `SR-AUD-*` identifier**; numbering stays frozen at **364**. Selected over
`System::Uri` on **severity** — three high-severity findings against `uri`'s zero, all three
in one `.cpp` body — not on count.

**Seven findings remediated by the compatible half**, one cause each:

| Ticket | Cause | Finding | What changed |
|---|---|---|---|
| #1973 | R-E | SR-AUD-155 | `Capture`/static `Throw` reject null; the two unnamed moved-from routes get their own guard |
| #1974 | R-B | SR-AUD-172 | the self-pipe **write** end becomes `O_NONBLOCK`; the read end stays blocking by design |
| #1975 | R-A | SR-AUD-169 | the complete prior `struct sigaction` is saved and restored, not replaced by `SIG_DFL` |
| #1976 | R-F | SR-AUD-156 | both `GCSettings` enum domains are validated at the boundary |
| #1977 | R-D | SR-AUD-170 | positive raw signal numbers are accepted and both spellings of a signal agree |
| #1978 | R-J | SR-AUD-059 + SR-AUD-168 disclosure | two headers' documented contracts are made true; zero runtime change |
| #1982 | R-I | SR-AUD-162 | the wider generic domain is classified as a deliberate, documented native adaptation |

**Nine corrections to the audit record, every one measured**, with the historical text
preserved and the correction appended to the owning per-file report. The four that changed
what was built:

- **SR-AUD-155 has four undefined-behaviour routes, not the two it names.** The implicitly
  declared move constructor and move assignment leave a moved-from `ExceptionDispatchInfo`
  null, so `Throw()` faults through ordinary well-formed C++ that passes no null anywhere.
  A check placed only where the finding asks leaves both open. Third occurrence of this
  shape after SR-AUD-199.
- **SR-AUD-169's consequence is sharper than recorded.** `SIG_IGN` on `SIGHUP` became
  `SIG_DFL`, and SIGHUP's default **terminates** — so a process that deliberately ignored it
  is killed by the next one. The audit's SIGWINCH probe cannot show this because SIGWINCH's
  default *is* ignore.
- **SR-AUD-170 also rejects the positive spelling of *supported* signals**, so the enum was
  accepted in exactly one of its two valid spellings. The repair had to make them agree, not
  merely add a path.
- **SR-AUD-162's premise does not survive translation.** `std::weak_ptr<int>` is well
  defined; the managed `where T : class` exists because the **CLR** cannot weakly handle a
  value type, which has no C++ counterpart. Narrowing would delete working functionality.

A fifth correction is methodological and caught a **false negative before it became a
conclusion**: the first version of this review's own probe forked *after* the parent had
registered, so the children inherited `watcherRunning_ == true` with no watcher thread, and
both SR-AUD-171 and SR-AUD-172 reported "did not reproduce". Reordered, both reproduce.
Every forked case now prints a liveness marker.

**Two post-audit defects recorded rather than absorbed, neither issuing an identifier:**
**#1985** (the self-pipe descriptors survive `exec()`) and **#1986** (`dispatchSignal`
invokes a copied handler after `Dispose()` has returned, so a handler capturing caller stack
state touches a dead frame — the per-file report hints at this without issuing a finding).

**Still open in this namespace:** ten public-shape divergences (#1980, approval-gated, split
into five groups with G-1 recommended as a purely additive minimum), the job-control /
chaining question (#1979, approval-gated — the port's behaviour is reproduced here but
.NET's is not, which puts it on #1963's side of the managed-probe line rather than #1968's),
the weak-table enumerator (#1981, approval-gated, object-layout change), and Windows OS
architecture (#1983, deferred: no toolchain, no host, no reference tree).

**Index after #1982: 100 remediated / 264 confirmed / 364 total** — of which **14** carry
the `confirmed (design-complete)` qualifier, twelve of them added by this review's §10
designs.

---

## Post-audit remediation checkpoint — the `System::Uri` namespace review (2026-08-03)

Review ticket **#1987**; durable record `docs/SystemUriNamespaceReviewPlan.md`. The
namespace's **fourteen** open findings (SR-AUD-138 … SR-AUD-151, all medium) map to
**eleven root causes** U-A … U-K and to twelve tickets, with **nothing dropped**: seven
compatible implementations (#1988–#1994) and five approval-gated designs (#1995–#1999).

**Closed by this batch:** SR-AUD-138 (#1993), SR-AUD-143 (#1989), SR-AUD-144 (#1990) and
SR-AUD-145 — whose two unrelated halves needed two tickets, #1991 and #1992.

**Six corrections to the audit record, every one measured** by
`build-probe/1987_probe1_uri_boundaries.cpp` (before/after logs retained):

- **SR-AUD-145 fabricates a *port*, not only a host.** `http://[::1/path` produced
  `host="[:"` **and `port=1`**, because the authority is split on `rfind(':')` with the
  brackets unchecked. Two further bracketed shapes the finding never names —
  `[::1]junk` and the empty `[]` — were accepted the same way.
- **SR-AUD-143 is not `mailto`-specific**: the opaque branch hard-coded `-1`, so `telnet:`
  lost 23 too.
- **A second, unnamed site lost the default port**: `http://example.com:/` reported `-1`
  while `http://example.com/` reported `80`.
- **The largest defect in this namespace had no finding at all.** `Uri::parse` located the
  scheme with `find("://")` — a search for `"://"` *anywhere* — and consulted the
  grammar-correct `findSchemeColon` only when that failed, although that function's own
  doc-comment states the RFC 3986 rule. Consequence: `/path?redirect=http://evil.com` and
  three more ordinary shapes all **threw**. Repaired by #1988 as a **proved** strict
  widening.
- **SR-AUD-140 has an availability half**: a `UriBuilder` with an unparseable field compares
  equal to itself but has no obtainable hash, because `GetHashCode` parses `ToString()`.
- **`Uri.hpp` promised a lower-case scheme the parser never produced** — a documentation
  defect, repaired without behaviour change by #1994.

**Five post-audit defects recorded as inactive tickets, none issuing an identifier:**
**#2000** (empty authority accepted), **#2001** (opaque base fabricates an authority),
**#2002** (a relative `Uri` never splits its query or fragment), **#2003** (embedded NUL
crosses the parser), **#2004** (the `Equals`/`GetHashCode` asymmetry) and **#2005**
(whitespace, deferred for want of reference evidence). Numbering stays frozen at **364**.

**Evidence discipline.** `/rv/tmp/runtime/src/libraries/` and the audit's own
`/tmp/sharp-runtimervc-uri-*` probe directories are **absent from this environment**. The
plan's §7 therefore states, per repair, what evidence survives. The two narrowings that
landed rest on evidence **inside this repository** — `HttpClient::parseUrl`'s identical
rejection of an unterminated IPv6 literal, and the already-approved enum-domain policy of
#1976/#1954. SR-AUD-147's narrowing has **no** surviving evidence and is blocked as #1998
accordingly, on the same line as #1963.

**Still open in this namespace:** URI identity and `UriBuilder` identity (#1995,
approval-gated — equality semantics), the `UriBuilder` setters and relative promotion
(#1996, four groups, G-1+G-2 recommended), the four absent public shapes (#1997, four
groups, A-1+A-2 recommended), `IsKnownScheme` argument validation (#1998), and
`UriTypeConverter`'s unrepresentable null (#1999, a public virtual signature change).

**Index after #1994: 104 remediated / 260 confirmed / 364 total** — of which **24** carry
the `confirmed (design-complete)` qualifier, ten of them added by this review's §14 designs.
