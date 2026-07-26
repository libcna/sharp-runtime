# Audit progress

## Current state

- Audit started: 2026-07-25.
- Scope frozen from a clean `feature/work` source checkout; audit artifacts
  are the only expected working-tree changes.
- Eligible files: 1,748.  Excluded tracked files: 33.
- Completed per-file reports: 294.
- Confirmed findings: 92 (twenty-seven high, fifty-nine medium, six low).  Open risks: 2 documented-adaptation questions.  Blocked reviews: 1 environment-limited validation run.

## Initial validation evidence

`git diff --check` passed. `scripts/local_ci_check.sh build` reached the test
phase after a clean boundary validation, catalogue check, configure, and
warning-free build. It could not complete in this sandbox: all six local-server
`SharpRuntimeTests_Net_Http` cases failed at construction with
`Socket::Socket: socket() failed`. A focused rerun of the same six tests
reproduced the identical zero-millisecond failure. `NEXT.md` already documents
that HTTP, socket, and ping tests need local-network permission, so this is
recorded as an environment-limited validation result, not a source regression
or a reason to weaken/skip the tests. A network-permitted environment must run
the full gate during final audit reconciliation.

## Immediate sequence

1. Continue the foundation-module audit beyond the reviewed DateTime,
   DateTimeOffset, TimeSpan, TimeOnly, String, Char, Object, Type, numeric,
   span/view, array, and interface-adapter surfaces, prioritising remaining
   Core.Base primitive/parser sources and complete test-file inventories.
2. Continue component by component with their tests and .NET source where
   applicable.
3. Reconcile every mirrored report, findings index, and project handoff.

## Assumptions and decisions

- The user explicitly selected an audit-only phase.  Even when a missing
  assertion, diagnostic, or defect is clear, this phase reports it with
  evidence and a proposed verification; it does not repair the source.
- `vendor/` and legal/VCS placeholder files are outside the authored-runtime
  audit; all other tracked first-party text-like files are in scope.
- Documentation claims are reviewed for consistency with local source and
  executable validation, not merely spelling or style.

## Resume point

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
