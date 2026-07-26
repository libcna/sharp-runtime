# Audit progress

## Current state

- Audit started: 2026-07-25.
- Scope frozen from a clean `feature/work` source checkout; audit artifacts
  are the only expected working-tree changes.
- Eligible files: 1,748.  Excluded tracked files: 33.
- Completed per-file reports: 667.
- Confirmed findings: 222 (fifty high, one hundred sixty-one medium, eleven low).  Open risks: 2 documented-adaptation questions.  Blocked reviews: 1 environment-limited validation run.

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
