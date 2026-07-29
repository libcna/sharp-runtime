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
  rather than raising `System::FormatException`.
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
- **SR-AUD-027 (high):** Convert's direct floating-to-integer paths allow NaN
  to bypass comparisons and return spurious platform values rather than throw.
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
  and short-vector overflow reads through `ToInt32`.
- **SR-AUD-042 (medium):** `TotalOrderIeee754Comparer<float>`, `<double>`,
  and `<Half>` implement only ordering and cannot bind to the local
  `IEqualityComparer<T>` interface, omitting .NET's total-order equality and
  hash-comparer contract.
- **SR-AUD-043 (high):** `HashCode::AddBytes(ReadOnlySpan<byte>)` casts a
  negative public span length to an unsigned size and reads past its buffer;
  ASan confirms the overflow.  Span/ReadOnlySpan construction is the confirmed
  upstream cause.
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
