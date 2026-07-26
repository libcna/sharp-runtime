<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# NEXT.md

*Last verified: 2026-07-26. Branch: `feature/audit`. The P0 component-boundary
repair, three P1 parity repairs, P1 portability revalidation, and twenty-two bounded
P2 API slices are complete: 41 physical modules, 90 production dependency
edges, and 12,681 tests across 37 executables. A repository-wide, evidence-only
audit is now in progress under `audit/` (local ticket #1766).*

This is the cold-start handoff for the next working session. Keep it focused
on verified facts, remaining bounded work, and commands needed to resume.
Historical session detail belongs in git history and `plan.sqlite3`.

## Current state

- A CNA-style deep audit is active. `audit/AUDIT_SCOPE.md` fixes a 1,748-file
  first-party scope and the mirrored `audit/<source-path>.audit.md` convention;
  `audit/AUDIT_MANIFEST.md` and `AUDIT_PROGRESS.md` record coverage. This
  phase must not repair production code or tests: it gathers reproducible
  evidence, missing assertions/diagnostics, and an ordered remediation backlog.
- Initial audit validation passed boundary validation, catalogue freshness, and
  a zero-warning native build. It could not complete the full suite in this
  sandbox because the six local-server `Net.Http` cases fail immediately with
  `Socket::Socket: socket() failed`; this matches the documented requirement
  for local-network permission. The tests remain enabled and need a
  network-permitted final-gate rerun.
- The first 491 audit reports confirm one hundred thirty-seven findings: tracked CI omits the
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
  serializes undefined fields as `-1` in `ToString(fieldCount)`; and the
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
- The full native baseline is a warning-free build with 12,681 passing tests
  across 36 component executables and one integration executable.
- Doxygen 1.9.8 emits 1,942 warnings with the tracked configuration. Run
  `scripts/check_doxygen_warnings.sh` to prevent increases; lower totals are
  accepted, and a dedicated Ubuntu 24.04 CI job enforces the same limit. A
  Doxygen-version change requires a deliberate re-baseline.
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
1,765 completed tickets, plus active audit ticket #1766. Ticket #1737 records the completed P0 split, tickets
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

## Recommended next bounded tasks

Do not start an unrelated consumer-driven P2 slice while the repository-wide
audit is active. Complete ticket #1766 first, reconcile the findings index,
then turn evidence-backed findings into isolated repair tickets.  The former
candidates remain valid only after that work:

1. **Other documented partial surfaces.** Examples include wider
   debugger/process/XML surfaces.
2. **Advanced `ImmutableList<T>::Builder` operations by consumer need.**
   Query, sorting, and copy overloads remain explicitly deferred; retain the
   vector-backed snapshot semantics if a focused consumer requires one.
3. **Reduce the Doxygen backlog incrementally.** The reproducible Doxygen 1.9.8
   baseline is 1,942 warnings. Keep touched public APIs from increasing it and
   avoid a mass comment-only rewrite.

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

1. Read `CLAUDE.md`, this file, `plan.md`, and `audit/AUDIT_SCOPE.md`.
2. Inspect `git status --short --branch`, ticket #1766, and
   `audit/AUDIT_PROGRESS.md`.
3. Resume the first pending audit shard; write evidence-backed mirrored reports
   and update the manifest/progress/index as each coherent shard completes.
4. Do not repair production code during this phase. Run the project gates at
   audit milestones and record their exact results before final reconciliation.
