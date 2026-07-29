# Cross-cutting findings

This file records patterns demonstrated by two or more per-file reports.  It
starts empty deliberately: a pattern is not inferred from a broad grep alone.
Each future entry must link the independent reports and separate confirmed
shared causes from superficially similar symptoms.

## CCF-001 — selective-isolation evidence diverges between local and tracked CI

The local `scripts/check_selective_components.sh` matrix contains ten direct
consumer checks, including `Collections.Blocking`. The GitHub Actions matrix
has only nine and omits that check, while `docs/CMakeComponents.md` and other
handoff documents describe ten tracked selective configurations. This is not
just a stale sentence: it leaves the architecture's most recently split
component without a direct CI closure test. See SR-AUD-001 and these reports:

- `.github/workflows/components.yml.audit.md`;
- `scripts/check_selective_components.sh.audit.md`;
- `docs/CMakeComponents.md.audit.md`;
- `modules/collections-blocking/README.md.audit.md`.

## CCF-002 — date/time input validation is weakened across the DateTime family

`DateTime` fails to validate public constructor time components and accepts
malformed parse input.  `DateTimeOffset` delegates component construction and
date-time parsing to it, then adds a permissive offset parser that normalizes
impossible offset minutes. `TimeOnly` independently accepts malformed suffixes
outside its stated fixed grammar.  These defects weaken date/time input
validation across three public types, while their focused suites lack the
relevant negative assertions.  See SR-AUD-006, SR-AUD-007, SR-AUD-009, and:

- `modules/core/src/System/DateTime.cpp.audit.md`;
- `modules/core/tests/System/DateTimeTests.cpp.audit.md`;
- `modules/core/src/System/DateTimeOffset.cpp.audit.md`;
- `modules/core/tests/System/DateTimeOffsetTests.cpp.audit.md`.
- `modules/core/src/System/TimeOnly.cpp.audit.md`;
- `modules/core/tests/System/TimeOnlyTests.cpp.audit.md`.

## CCF-003 — numeric wrappers diverge from safe boundary and formatting behavior used by nearby numeric code

The 128-bit wrappers correctly document their GCC/Clang dependency, but their
boundary handling is uneven. `Int128` uses unsigned arithmetic for most
overflow-prone operators yet reintroduces signed negation UB at decimal
`MinValue`; `UInt128` forwards arbitrary user shift counts to native operations
instead of applying the sibling's modulo-128 mask. Audited 8/16/32/64/128-bit
formatters all silently accept unknown formats; SByte, Int16, UInt16, UInt32,
UInt64, and UInt128 also miss `B` formatting. Byte, SByte, Int16, UInt16,
Int32, UInt32, Int64, UInt64, and UInt128 fail to reject inverted Clamp bounds,
with the 8/16/32/64-bit callers passing invalid intervals to `std::clamp`.
Decimal and MathF independently have the same missing inverted-bound
validation and select a bound. SByte and Int16 additionally define `IsPositive` inconsistently with the .NET
generic-math zero rule. See SR-AUD-019 through SR-AUD-024, and:

- `modules/core/include/System/Int128.hpp.audit.md`;
- `modules/core/include/System/UInt128.hpp.audit.md`;
- `modules/core/tests/System/IntegerTypesTests.cpp.audit.md`;
- `modules/core/tests/System/UInt128Tests.cpp.audit.md`.
- `modules/core/include/System/Int64.hpp.audit.md`;
- `modules/core/include/System/UInt64.hpp.audit.md`;
- `modules/core/tests/System/Int64NewTests.cpp.audit.md`;
- `modules/core/tests/System/UInt64Tests.cpp.audit.md`.
- `modules/core/include/System/Int32.hpp.audit.md`;
- `modules/core/include/System/UInt32.hpp.audit.md`;
- `modules/core/tests/System/PrimitiveTypeTests.cpp.audit.md`;
- `modules/core/tests/System/UInt32NewTests.cpp.audit.md`;
- `modules/core/tests/System/NumberStylesExtendedTests.cpp.audit.md`.
- `modules/core/include/System/Byte.hpp.audit.md`;
- `modules/core/include/System/SByte.hpp.audit.md`;
- `modules/core/include/System/Int16.hpp.audit.md`;
- `modules/core/include/System/UInt16.hpp.audit.md`;
- `modules/core/tests/System/ByteTests.cpp.audit.md`;
- `modules/core/tests/System/SByteTests.cpp.audit.md`;
- `modules/core/tests/System/Int16NewTests.cpp.audit.md`;
- `modules/core/tests/System/UInt16Tests.cpp.audit.md`.
- `modules/core/include/System/Decimal.hpp.audit.md`.
- `modules/core/tests/System/DecimalNewTests.cpp.audit.md`.
- `modules/core/include/System/MathF.hpp.audit.md`.
- `modules/core/tests/System/MathFTests.cpp.audit.md`.

## CCF-004 — native-width and fixed-width boundaries must not rely on signed C++ overflow

`Int128` decimal parsing/formatting, `TimeSpan::Subtract`, `IntPtr::Add`/`Subtract`,
`ReadOnlyMemory::Slice(start)`, `Index::GetOffset` / Range resolution,
`DateOnly` day/month/year arithmetic, `Tuple` hash combination, and `Utf8Parser`
Int64 minimum conversion each expose
a public .NET-shaped operation that needs well-defined two's-complement or
checked arithmetic, but performs a signed C++ operation first. The first two
were independently confirmed earlier; the IntPtr boundary probe adds a
native-width instance with both directions of overflow, the memory probe adds
unchecked public-subtraction overflow, the Index/Range probe adds intentionally
unchecked .NET offset arithmetic, the DateOnly probe adds range-required
calendar arithmetic, and the Tuple probe adds public hash arithmetic. See
SR-AUD-008, SR-AUD-019, SR-AUD-025, SR-AUD-049, SR-AUD-057, SR-AUD-060,
SR-AUD-062, SR-AUD-084, and:

- `modules/core/src/System/TimeSpan.cpp.audit.md`;
- `modules/core/include/System/Int128.hpp.audit.md`;
- `modules/core/include/System/IntPtr.hpp.audit.md`.
- `modules/core/include/System/ReadOnlyMemory.hpp.audit.md`.
- `modules/core/include/System/Index.hpp.audit.md`.
- `modules/core/include/System/Range.hpp.audit.md`.
- `modules/core/src/System/DateOnly.cpp.audit.md`.
- `modules/core/tests/System/DateOnlyTimeOnlyTests.cpp.audit.md`.
- `modules/core/include/System/Tuple.hpp.audit.md`.
- `modules/core/tests/System/TupleNewTests.cpp.audit.md`.
- `modules/buffers/include/System/Buffers/Text/Utf8Parser.hpp.audit.md`.
- `modules/buffers/tests/System/Buffers/Utf8ParserTests.cpp.audit.md`.

**Related, but deliberately not a member (ticket #1786, 2026-07-28):** the
collection mutation counters ticket 1713 introduced share this cause's shape —
an unbounded `++` on a signed `intcs` that is UB once it reaches `INTCS_MAX`,
which ticket #1786 reproduced under UBSan for `SortedSet<T>` — but they are
**not** counted as CCF-004 instances and no `SR-AUD-*` identifier is issued for
them. Two reasons. The arithmetic here is not at a *public boundary*: the
counter is a private implementation detail with no .NET-shaped operation
exposing it, so nothing in CCF-004's "public .NET-shaped operation that needs
well-defined or checked arithmetic" framing applies. And the repair is not
CCF-004's repair: those instances need checked or explicitly wrapping arithmetic
at a boundary a caller can observe, whereas the counter needed a **wider** type,
because its real defect is snapshot reuse (ABA) rather than a wrong result at
the boundary — widening fixes both, checking would fix only one. This cause's
membership list is unchanged. #1786's analysis is in
`docs/SortedSetVersioningDesign.md`, and the fourteen collections still carrying
the pattern are inactive ticket #1787.

**Follow-up (ticket #1787, 2026-07-28):** the sweep is now **done** and the
non-membership decision above is confirmed rather than revisited, but two facts
recorded here need correcting. There were **sixteen** counter-carrying types, not
fifteen — `BitArray` was missed by #1786's inventory, and it is also the one
whose counter was already `std::uint32_t`, so it never had the signed-overflow
UB at all. And the sweep found a **third** defect that CCF-004's framing does not
reach either, for a different reason than the counter's width does: the
implicitly declared copy/move assignment operator transplanted the *source's*
counter into the destination, leaving an enumerator apparently valid over storage
the assignment had already destroyed — six AddressSanitizer
`heap-use-after-free`/`heap-buffer-overflow` reproductions, needing **no
arithmetic overflow at all**. That is a special-member-function defect, not a
defined-arithmetic one, which is a further independent reason these instances are
not CCF-004 members: no amount of checked arithmetic at a boundary would have
found or fixed it. The repair is
`System::Collections::detail::BasicMutationCounter`, whose assignment advances
the destination rather than taking the source's value; the full record is
`docs/CollectionVersionCounterSweep.md`. Thirteen types were fully repaired;
`LinkedList<T>` and `BitArray` keep a 32-bit counter and a documented 2^32
residual, tracked by blocked tickets #1788 and #1789. **This cause's membership
list is still unchanged.**

## CCF-005 — high-value conversion APIs need explicit boundary and special-value validation

The audited primitive wrappers, Decimal, and `Convert` share a recurring testing shape:
common valid values and ordinary finite overflow are covered, while an API's
semantically distinct invalid domain is omitted. `Convert` exposes unchecked
integer narrowing/sign changes and NaN casts; small integer wrappers expose
inverted intervals and wrong zero predicates; Decimal omits its inverted Clamp
range, parser boundary, invalid rounding enum, and raw-sign vectors;
BitConverter's typed vector decoders omit all index/remaining-width validation
and reach ASan-confirmed out-of-bounds reads; Span permits a negative public
length and HashCode converts it to a huge unsigned raw read; static
MemoryExtensions CopyTo skips its destination-length check and writes beyond a
short span; IntPtr exposes native extrema. These are independent implementations
but the same missing assertion strategy allows green normal-path suites to
conceal public contract defects. See
SR-AUD-021 through SR-AUD-027, SR-AUD-035, SR-AUD-036, SR-AUD-038, SR-AUD-041,
SR-AUD-043, SR-AUD-047, and the
owning reports listed above.

## CCF-006 — numeric format validation is not normalized at the public API boundary

The audited integral wrappers consistently accept an unknown format such as
`"Q"` as a general/decimal request. The newly audited `Single` and `Double`
wrappers do the same and additionally let malformed precision such as `"Fz"`
escape as `std::stoi`. These are independently implemented wrappers, but they
expose the same C++ fallback or exception instead of the documented
`System::FormatException`; the public boundary needs one explicit validation
policy rather than type-specific behavior.  See SR-AUD-021 and:

- `modules/core/include/System/Byte.hpp.audit.md`;
- `modules/core/include/System/SByte.hpp.audit.md`;
- `modules/core/include/System/Int16.hpp.audit.md`;
- `modules/core/include/System/UInt16.hpp.audit.md`;
- `modules/core/include/System/Int32.hpp.audit.md`;
- `modules/core/include/System/UInt32.hpp.audit.md`;
- `modules/core/include/System/Int64.hpp.audit.md`;
- `modules/core/include/System/UInt64.hpp.audit.md`;
- `modules/core/include/System/Int128.hpp.audit.md`;
- `modules/core/include/System/UInt128.hpp.audit.md`;
- `modules/core/include/System/Single.hpp.audit.md`.
- `modules/core/include/System/Double.hpp.audit.md`.

## CCF-007 — the binary float wrappers delegate public edge semantics to unsuitable native primitives

`Single` and `Double` independently use the same direct native recipes for
decimal rounding, `IsPow2`, `ilogb`, Pi-scaled trigonometry, and ordinary text
conversion. These recipes work for normal finite values but omit .NET's
type-specific precision validation, subnormal classification, special-value
mapping, exact turn reduction, and default number grammar. The two direct
probes show the same family of failures at their respective precisions; repair
must be coordinated but retain the distinct 0–6 and 0–15 rounding limits. See
SR-AUD-029 through SR-AUD-033 and:

- `modules/core/include/System/Single.hpp.audit.md`;
- `modules/core/tests/System/SingleTests.cpp.audit.md`;
- `modules/core/include/System/Double.hpp.audit.md`;
- `modules/core/tests/System/DoubleTests.cpp.audit.md`;
- `modules/core/tests/System/DoubleTests2.cpp.audit.md`.

## CCF-008 — public numeric Round overloads do not uniformly validate `MidpointRounding`

Decimal, Math, and MathF each expose a public `MidpointRounding` overload and
all use a switch default as an ordinary rounding operation: Decimal truncates,
while Math and MathF use ties-to-even.  The .NET APIs reject values outside the
five named enum members with `ArgumentException`.  Their focused suites cover
named modes and ordinary values but none casts an invalid value.  This is a
separate boundary-validation pattern from digit-range checks.  See SR-AUD-036
and:

- `modules/core/src/System/Decimal.cpp.audit.md`;
- `modules/core/tests/System/DecimalTests2.cpp.audit.md`;
- `modules/core/include/System/Math.hpp.audit.md`;
- `modules/core/tests/System/MathTests.cpp.audit.md`;
- `modules/core/include/System/MathF.hpp.audit.md`;
- `modules/core/tests/System/MathFTests.cpp.audit.md`.

## CCF-009 — process-wide mutable PRNG state has no concurrency boundary

`Random::Shared` returns one mutable subtractive generator, while
`Guid::NewGuid` hides one mutable Mersenne Twister behind a static factory.
Both implementations rely on thread-safe static initialization but then mutate
the generated state without a mutex, atomics, or thread-local ownership.
Independent TSan probes confirm each race.  `Guid::CreateVersion7` reaches the
same engine through `NewGuid`.  A repair must preserve each public API while
introducing a real ownership/synchronization boundary; merely changing one
singleton leaves the other independently unsafe.  See SR-AUD-010 and:

- `modules/core/src/System/Random.cpp.audit.md`;
- `modules/core/tests/System/RandomTests.cpp.audit.md`;
- `modules/core/src/System/Guid.cpp.audit.md`;
- `modules/core/tests/System/GuidTests.cpp.audit.md`.

**Related, but deliberately not a member (ticket #1784, 2026-07-28):** the
`SortedSet<T>` live-view Count-cache race that ticket #1783 introduced and
ticket #1784 removed is **not** a CCF-009 instance and must not be counted as
one. It shares this cause's *symptom* — a TSan-confirmed unsynchronized write
reached through an API that does not look like a mutation — but not its cause.
CCF-009 is about **process-wide singleton** state that every caller shares
whether they know it or not, so no caller can opt out and a repair must
introduce a real ownership boundary. #1784's defect was **per-object** state on
a caller-owned instance, so the ordinary "do not share an object across threads
without synchronizing" rule already covered mutation; what was wrong was that a
`const`, observationally read-only member wrote at all. It carries no
`SR-AUD-*` identifier (the numbering is frozen at 364), was fixed by making the
two cache fields atomic with a release/acquire publication protocol rather than
by adding a synchronization boundary, and adds **no** thread-safety guarantee.
This cause's membership list is unchanged. See
`docs/SortedSetLiveViewDesign.md` §31 and
`audit/modules/collections/include/System/Collections/Generic/SortedSet.hpp.audit.md`.

## CCF-010 — raw C++ ordering is not the .NET comparison contract for floating values

`MemoryExtensions` and `Array` both choose raw `<` and `==` for their default
sort/search paths, while `NullableHelper` and every generic `ValueTuple` /
`Tuple` `CompareTo` use raw `<`; `Nullable`, `ValueTuple`, and `Tuple` equality
use raw `==`. LINQ repeats that pattern in Contains, Distinct, Min/Max, and
OrderBy. Those operators make NaN unequal to itself and unordered against every
finite number, while .NET's default floating comparer gives NaN a stable place
before finite values and its equality comparer treats NaN as equal to itself.
Independent probes show Array/MemoryExtensions sort `{3,NaN,1}` as `1,3,NaN`
and fail to find NaN, Nullable compares NaN with a finite value as equal,
ValueTuple/Tuple do both at their component boundary, and LINQ fails Contains,
retains duplicate NaN, and returns a finite Min for a later NaN. The repair
must centralize or consistently reuse the local comparison policy; changing
only one surface leaves the others divergent. See SR-AUD-046 and:

- `modules/core/include/System/MemoryExtensions.hpp.audit.md`;
- `modules/core/tests/System/MemoryExtensionsTests.cpp.audit.md`;
- `modules/core/include/System/Array.hpp.audit.md`;
- `modules/core/tests/System/ArrayTests.cpp.audit.md`.
- `modules/core/include/System/Nullable.hpp.audit.md`.
- `modules/core/tests/System/NullableTests.cpp.audit.md`.
- `modules/core/include/System/ValueTuple.hpp.audit.md`.
- `modules/core/tests/System/ValueTupleTests.cpp.audit.md`.
- `modules/core/include/System/Tuple.hpp.audit.md`.
- `modules/core/tests/System/TupleTests.cpp.audit.md`.
- `modules/core/tests/System/TupleNewTests.cpp.audit.md`.
- `modules/core/include/System/Linq.hpp.audit.md`.
- `modules/core/tests/System/LinqTests.cpp.audit.md`.

## CCF-011 — empty `std::function` values cross public boundaries without an explicit policy

`Array` accepts empty callable arguments, then either silently returns on an
empty collection or fails only after reaching `std::function::operator()`.
`Progress<T>` correctly rejects an empty constructor handler, but its separate
event-like registration method stores an empty handler and raises
`std::bad_function_call` only on a future report. `Lazy<T>` similarly accepts
an empty factory and fails only at first value access.
`AggregateException::Handle` likewise accepts an empty predicate and reaches
`std::bad_function_call` at first inner exception. The APIs have different
.NET-compatible outcomes (Array arguments need an argument error; adding a
null event delegate is a no-op; Lazy needs a constructor argument error), but
none may defer the policy to a native exception or an empty-input accident.
`EventHandler<TEventArgs>` repeats the event-specific route: it stores an empty
subscriber and Raise reaches `std::bad_function_call`. LINQ callback overloads
also accept empty `std::function` values: empty vectors silently return a
normal result while nonempty traversal eventually throws the native exception.
Tests cover only normal callables. See SR-AUD-052, SR-AUD-058, SR-AUD-065,
SR-AUD-099, SR-AUD-121, SR-AUD-134, and:

- `modules/core/include/System/Array.hpp.audit.md`;
- `modules/core/tests/System/ArrayTests.cpp.audit.md`;
- `modules/core/include/System/Progress.hpp.audit.md`;
- `modules/core/tests/System/ProgressTests.cpp.audit.md`.
- `modules/core/include/System/Lazy.hpp.audit.md`.
- `modules/core/tests/System/LazyTests.cpp.audit.md`.
- `modules/core/include/System/AggregateException.hpp.audit.md`.
- `modules/core/include/System/EventHandler.hpp.audit.md`;
- `modules/core/tests/System/EventHandlerTests.cpp.audit.md`.
- `modules/core/include/System/Linq.hpp.audit.md`.
- `modules/core/tests/System/LinqTests.cpp.audit.md`.

## CCF-012 — hand-written composite-format replacement is not a format parser

`String::Format` and `FormattableString::ToString` independently reconstruct a
subset of composite-format behavior with sequential text replacement. The
String path rejects valid escaped braces and accepts malformed closing braces.
The FormattableString path additionally expands brace text inserted as an
argument, expands within escaped braces, and leaves a missing index literal.
Normal one/two-placeholder tests pass for both paths, but grammar/error cases
are absent. A repair needs a shared parsed-token model or a deliberately narrow
documented formatter; altering just one API preserves divergent brace rules.
See SR-AUD-015 and:

- `modules/core/src/System/String.cpp.audit.md`;
- `modules/core/include/System/FormattableString.hpp.audit.md`;
- `modules/core/tests/System/FormattableStringTests.cpp.audit.md`.

## CCF-013 — sibling Base64 encoders duplicate an unsafe in-place write order

`Base64::EncodeToUtf8InPlace` and `Base64Url::TryEncodeToUtf8InPlace` both
encode full three-byte groups backwards, then read the trailing one/two input
bytes.  The first full output group writes offset three and overwrites that
unread remainder for 4/5-byte source lengths.  Independent probes turn
`ABC\\0` into `QUJDRA==` and `QUJDRA` instead of the correct `QUJDAA==` and
`QUJDAA`; both APIs report success.  The shared algorithmic shape and test
gap mean a repair must cover both headers and test every full-group-plus-
remainder boundary rather than correcting just the padded variant. See
SR-AUD-078 and:

- `modules/buffers/include/System/Buffers/Text/Base64.hpp.audit.md`;
- `modules/buffers/tests/System/Buffers/Base64Tests.cpp.audit.md`;
- `modules/buffers/include/System/Buffers/Text/Base64Url.hpp.audit.md`;
- `modules/buffers/tests/System/Buffers/Base64UrlTests.cpp.audit.md`.

## CCF-014 — false Try-style calls must not leak stale output into the next control path

`SequenceReader::TryRead` / `TryPeek` and every implemented
`Utf8Parser::TryParse` overload reset their Boolean/status indication and
cursor on false but leave the caller's value untouched.  Current .NET assigns
the `out` parameter's default in both API families.  Independent probes show a
failed reader retains 42/99 and a failed parser retains an `int` 42 or `bool`
true.  A repair needs a consistent default-output boundary across false returns,
plus tests that prepopulate the output; checking only false and zero consumed
is insufficient. See SR-AUD-075, SR-AUD-085, and:

- `modules/buffers/include/System/Buffers/SequenceReader.hpp.audit.md`;
- `modules/buffers/tests/System/Buffers/Batch6BuffersTests.cpp.audit.md`;
- `modules/buffers/include/System/Buffers/Text/Utf8Parser.hpp.audit.md`;
- `modules/buffers/tests/System/Buffers/Utf8ParserTests.cpp.audit.md`.

## CCF-015 — UTF-8 public text cannot use C-locale byte whitespace classification

`MemoryExtensions` trim and `ArgumentException::ThrowIfNullOrWhiteSpace`
independently pass UTF-8 byte sequences through `std::isspace`.  Both therefore
retain U+00A0 even though their .NET-shaped character contracts treat it as
whitespace: the first returns it after Trim and the second accepts it instead
of throwing.  Casting to `unsigned char` avoids negative-byte undefined
behavior, but does not turn a byte locale predicate into Unicode character
classification.  A repair needs one declared UTF-8 decode/Unicode whitespace
policy and malformed-input diagnostics rather than more isolated byte checks.
See SR-AUD-048 and:

- `modules/core/include/System/MemoryExtensions.hpp.audit.md`;
- `modules/core/tests/System/MemoryExtensionsTests.cpp.audit.md`;
- `modules/core/include/System/ArgumentException.hpp.audit.md`;
- `modules/core/src/System/ArgumentException.cpp.audit.md`;
- `modules/core/tests/System/ArgumentExceptionTests.cpp.audit.md`.

## CCF-016 — inline exception constructors need a complete derived-HResult audit

`ArrayTypeMismatchException`, five newly reviewed sibling types, two
`TypeLoadException` derivatives, and two additional `SystemException`
derivatives rely on the code set by `SystemException`, `Exception`, or
`TypeLoadException` rather than explicitly assigning the documented error code
for the derived exception. Their independently green message/inheritance suites
omitted HResult assertions, so one class retained `0x80131501` instead of
`0x80131503`, a five-class probe found `ApplicationException=0x80131500` and
four `SystemException` derivatives at `0x80131501`, the TypeLoad pair both
reported `0x80131522` instead of distinct codes, the latest pair remained at
`0x80131501` instead of `E_POINTER` / `0x80131504`, and
`DuplicateWaitObjectException` retains `0x80070057` rather than its own
`0x80131529`. This is a repeatable
constructor-audit and assertion gap: every .NET-shaped exception should be
checked against its own HResult on every public overload rather than inheriting
the immediate base value by accident. See SR-AUD-093 through SR-AUD-096 and
SR-AUD-100, and:

- `modules/core/include/System/ArrayTypeMismatchException.hpp.audit.md`;
- `modules/core/include/System/ApplicationException.hpp.audit.md`;
- `modules/core/include/System/AppDomainUnloadedException.hpp.audit.md`;
- `modules/core/include/System/BadImageFormatException.hpp.audit.md`;
- `modules/core/include/System/CannotUnloadAppDomainException.hpp.audit.md`;
- `modules/core/include/System/DataMisalignedException.hpp.audit.md`;
- `modules/core/include/System/DllNotFoundException.hpp.audit.md`;
- `modules/core/include/System/EntryPointNotFoundException.hpp.audit.md`;
- `modules/core/include/System/AccessViolationException.hpp.audit.md`;
- `modules/core/include/System/ContextMarshalException.hpp.audit.md`;
- `modules/core/include/System/DuplicateWaitObjectException.hpp.audit.md`.

## CCF-017 — the Attribute base's identity fallback changes every unoverridden attribute's value semantics

Current .NET makes `Attribute` abstract and supplies same-type fieldwise
equality/hash semantics.  The C++ base is constructible and compares/hashes
addresses, so independently constructed equal values of every attribute that
does not override both methods compare unequal.  The direct probe demonstrates
this for `CLSCompliantAttribute(true)` and two empty `FlagsAttribute` objects;
the ContextStatic, ParamArray, Serializable, and Obsolete fixtures likewise
exercise identity rather than a .NET value contract.  A repair needs one base
policy plus vectors for payload, empty-marker, type-mismatch, and hash/equality
behavior rather than per-marker overrides.  See SR-AUD-114 and:

- `modules/core/include/System/Attribute.hpp.audit.md`;
- `modules/core/include/System/CLSCompliantAttribute.hpp.audit.md`;
- `modules/core/include/System/ContextStaticAttribute.hpp.audit.md`;
- `modules/core/include/System/FlagsAttribute.hpp.audit.md`;
- `modules/core/include/System/ObsoleteAttribute.hpp.audit.md`;
- `modules/core/tests/System/SystemAttributeTests.cpp.audit.md`.

## CCF-018 — enumerator lifecycle checks are not consistently enforced before native storage access

The `IEnumerator<T>` abstraction must reject `Current` before a successful
first `MoveNext` and after enumeration ends. Generic List, Queue, Stack,
SortedList, LinkedList, ObjectModel Collection and ReadOnlyCollection, and the
ConcurrentBag/Queue/Stack snapshot enumerators all directly index native
storage instead. The List probe is ASan-confirmed as a heap-buffer-overflow;
the sibling implementations have the same unguarded cursor shape. BitArray
independently returns a stale cache and lacks mutation/version detection. A
repair needs one lifecycle policy and tests across each storage category, not
only a List bounds check. See SR-AUD-356, SR-AUD-364, and:

**Remediation status (ticket #1767, 2026-07-27): REMEDIATED.** A shared
`EnumeratorState` now rejects before-start and after-end `Current` access
before native storage is touched across all ten implementations. `BitArray`
also records a version and rejects `MoveNext`/`Reset` after each mutating API.
The permanent 13-test regression suite, full 1,435-test Collections.Core
target, direct ASan/UBSan probe, and network-permitted 12,694-test repository
gate all pass. The original evidence remains above and in the per-file reports.

**Post-remediation follow-up (tickets #1792 and #1793, 2026-07-28), no new
`SR-AUD-*` identifier.** Ticket #1790's mutable-access inventory found a second,
distinct defect on the same interface: `Current` was guarded against invalid
*states* by #1767, but what a *valid* `Current` handed back through the
non-generic accessor was a mutable `void*` aliasing live collection storage,
filled by `const_cast<T*>(&Current())`. Design ticket #1792 measured it as six
distinct defect classes reaching thirteen generic and eight non-generic
implementations plus two hand-written test-local ones, with four further
`const_cast`s outside the bridge, four ASan `heap-use-after-free` reports, and a
`std::unordered_map` key rewritten in place. Implementation ticket #1793 then
changed the accessor to return an owning `std::any` by value, the counterpart of
.NET's `object IEnumerator.Current`, under an explicit three-part user approval
covering a public source break on both interfaces and a **silent ABI break**
requiring a full consumer rebuild. `Generic::IEnumerator<T>::Current()` is
unchanged at `const T&`, and this finding's own lifecycle contract is unchanged
— every #1767 regression still passes unmodified. **CCF-018 and SR-AUD-356 stay
`remediated`.** See `docs/IEnumeratorCurrentSafetyDesign.md`. Two hazards on the
same interface family remain deliberately open and are recorded there: the typed
`Current()` reference window, and `IDictionaryEnumerator`'s `const void*`
key/value accessors, the latter opened as ticket #1794
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY`, `blocked`, not begun).

**Second post-remediation follow-up (design ticket #1795, 2026-07-28), no new
`SR-AUD-*` identifier.** #1794 is an *implementation* row and was deliberately
**not** reused as a design ticket; design ticket **#1795**
(`REMED-COLL-IDICTENUM-KEYVALUE-SAFETY-DESIGN`) answers it instead, with no
production or test-source change, and #1794 stays `blocked`. Two facts from that
design belong to *this* cross-cutting finding, because they are about a
lifecycle check being absent before native storage access — exactly CCF-018's
subject — on an interface CCF-018 did not cover:

- **Neither `IDictionaryEnumerator` accessor performs a fail-fast version
  check**, so both dereference a container iterator that a mutation may have
  invalidated. On `ListDictionaryInternal`, which caches nothing, that reaches
  `getEntryProperty()` and even the already-migrated, `std::any`-returning
  `getCurrentProperty()`: eight AddressSanitizer `heap-use-after-free` reports
  were reproduced, three of them on accessors whose return type is *already* an
  owning value. A return-type change alone therefore does not close the lifetime
  class; the design requires a `MoveNext`-time snapshot into enumerator-owned
  storage, which is what .NET's `HashtableEnumerator` does.
- **`Hashtable`'s `getValueProperty()` does have a write path**, contradicting
  ticket #1794's own premise: it returns a pointer to the live map's non-`const`
  `std::any`, so `const_cast` + assignment is well-formed, defined C++ that
  rewrites dictionary storage with the mutation counter unmoved.

**CCF-018 and SR-AUD-356 stay `remediated` and are not reopened**: #1767's
lifecycle contract is unchanged and every one of its regressions still passes.
See `docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`.

**Closed by implementation ticket #1794 on 2026-07-28**, under an explicit user
approval covering the public source break, the two `ListDictionaryInternal`
parity corrections, and a silent ABI break through two independent mechanisms.
Both accessors now return an owning `std::any` by value **and** — the half that
actually closes the lifecycle class this finding is about — **both
implementations snapshot the entry into enumerator-owned storage during a
successful `MoveNext()`, so no accessor on either implementation dereferences a
container iterator.** The snapshot rule is written into
`IDictionaryEnumerator.hpp` as an invariant of the *interface*, not as an
implementation detail: an implementation that reads its container inside an
accessor is wrong even when its signatures are right.

One correction to the paragraph above, made by re-measurement before any source
changed and recorded rather than silently adopted: **the figure is nine
AddressSanitizer `heap-use-after-free` reports, not eight.** The design record's
§8.2 table listed nine and its prose sentence said eight; nine of sixteen
scenarios reproduced, and nine is the figure now used by the header, the
permanent suite, and `README.md`.

**CCF-018 and SR-AUD-356 remain `remediated`** — this is the closure of a
post-remediation follow-up on an interface CCF-018 did not originally cover, not
a reopening. Still open and explicitly not claimed: `MoveNext()`/`Reset()` after
the collection itself is destroyed remain undefined, which is the port-wide
borrowing convention rather than an enumerator-lifecycle gap. New permanent
suite: `DictionaryEnumeratorKeyValueSafetyTests.cpp` (+64 tests, parameterised
over both implementations). See `docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`
§37.

**Follow-on closure by ticket #1796 on 2026-07-28 — the same class, on the
*value-access* surface #1794 deliberately left open.** #1794's own record named
two remaining `Hashtable` escapes at
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §30 risk 6 and §37.6 so they
would not be mistaken for closed; design ticket #1797 then found that there were
**four**, not two, and implementation ticket #1796 closed all four under an
explicit four-item user approval. `IDictionary::getItem` returns an owning
`std::any` by value on both implementations; `Hashtable::operator[]` returns a
**non-copyable `ValueReference` proxy** so `table[key] = value` is a tracked
insert-or-replace and a bare read of an absent key no longer *structurally
inserts*; a new `const` `operator[]` and a by-value `at()` (throwing
`KeyNotFoundException` rather than the `catch (const System::Exception&)`-invisible
`std::out_of_range`) complete the surface. **The nine `heap-use-after-free`
reports on this surface are now zero**, and the worst case — which produced no
sanitizer report at all — is closed by measurement: in #1797's exact experiment,
`Count` goes **8 → 8** where it went **8 → 4,008**, and an outstanding enumerator
walks **8 of 8** distinct keys where it walked **2,045** of 4,008 and reached
only 6 of its 8 seeds. This is a **silent ABI break** — byte-identical mangled
name, vtable slot unchanged at `0x38`, `this` moving `%rdi → %rsi` behind a
hidden `sret`, with a stale caller linking at `exit=0` and then segfaulting at
`exit=139` — so **every consumer must be fully rebuilt**. `sizeof` is unchanged
(72 / 40), so it is not a layout break. New permanent suite:
`HashtableValueAccessSafetyTests.cpp` (+55 tests). Still not claimed closed:
`setItem`/`Add`'s raw-key `void*` *value* parameter, and accessor use after the
collection itself is destroyed. `ListDictionaryInternal`'s own two defects are
untouched and remain ticket **#1798**. See
`docs/HashtableValueAccessSafetyDesign.md` §34.

**Follow-up (ticket #1798, 2026-07-29): the `ListDictionaryInternal` half is now
closed too, and there were SIX defects rather than the two named above.** Design
ticket #1799 measured, beyond the `setItem` replace-version bypass and the
accepted null key: the key view's `CopyTo` laundering away the caller's `const`
(**an AddressSanitizer SEGV on a write to read-only storage**, through a
writable pointer the library manufactured with `const_cast`); a throwing
duplicate `Add` and a `Remove` of an absent key diverging from .NET
`ListDictionaryInternal` in the *opposite* direction from the setter; and a
previously unrecorded over-bump on the *sibling* implementation,
`Hashtable::Remove` of an absent key, now inactive ticket **#1802** and
**deliberately not begun** — `Hashtable` was not modified by #1798. Validation is
now **structurally unskippable** (a private `ValidatedKey` that the single
locator is the only consumer of, rather than a `toKey()`-style convention), the
counter advances on **effective mutation only**, and every key surface boxes
`const void*`. Two deviations from .NET are deliberate and documented: a throwing
`Add` and an absent `Remove` do **not** invalidate enumerators, matching .NET
`Hashtable` rather than .NET `ListDictionaryInternal`, whose bump-first shape
would have manufactured two new false-positive `InvalidOperationException`s.
CCF-018 and the findings above are **not reopened**. See
`docs/ListDictionaryInternalSetterDesign.md` §37.

- `modules/collections/include/System/Collections/Generic/IEnumerator.hpp.audit.md`;
- `modules/collections/include/System/Collections/Generic/List.hpp.audit.md`;
- `modules/collections/include/System/Collections/Generic/Queue.hpp.audit.md`;
- `modules/collections/include/System/Collections/Generic/Stack.hpp.audit.md`;
- `modules/collections/include/System/Collections/Generic/SortedList.hpp.audit.md`;
- `modules/collections/include/System/Collections/Generic/LinkedList.hpp.audit.md`;
- `modules/collections/include/System/Collections/ObjectModel/Collection.hpp.audit.md`;
- `modules/collections/include/System/Collections/ObjectModel/ReadOnlyCollection.hpp.audit.md`;
- `modules/collections/include/System/Collections/Concurrent/ConcurrentBag.hpp.audit.md`;
- `modules/collections/include/System/Collections/Concurrent/ConcurrentQueue.hpp.audit.md`;
- `modules/collections/include/System/Collections/Concurrent/ConcurrentStack.hpp.audit.md`;
- `modules/collections/include/System/Collections/BitArray.hpp.audit.md`.

## CCF-019 — borrowed native handles outlive the owner without a liveness boundary

JsonNode children, XML LINQ children, and now LinkedListNode each expose a
copyable public handle while retaining a raw parent/container pointer or native
iterator. Retaining the child/node after owner destruction reaches
ASan-confirmed use-after-free in all three representative surfaces. The
implementations differ, but ownership cannot remain an undocumented raw pointer
when the handle is publicly storable. Repair requires an explicit lifetime or
detachment policy and tests that retain handles across owner destruction and
structural removal. See SR-AUD-327, SR-AUD-333, SR-AUD-357, and:

**Remediation status (tickets #1768/#1769, 2026-07-27): PARTIAL —
LinkedListNode only.** SR-AUD-357 is remediated. `LinkedListNode<T>` now refers
to an independently allocated, reference-counted node with an explicit
null/detached/attached state; removal, `Clear`, and owner destruction detach the
node and retain its value instead of leaving a dangling iterator, and the
contract is recorded in `docs/LinkedListNodeLifetime.md`. Evidence: 49 permanent
regressions, a clean direct ASan/UBSan probe, 1,484/1,484 Collections.Core, and
the network-permitted 12,743-test repository gate. This was deliberately **not**
generalised into a shared lifetime abstraction: SR-AUD-327 (JsonNode) and
SR-AUD-333 (XML LINQ `XObject`) have different public surfaces and remain
`confirmed`, each needing its own compatibility review before repair. The
original evidence above and in the per-file reports is retained.

- `modules/text-json/include/System/Text/Json/Nodes/JsonNode.hpp.audit.md`;
- `modules/xml-linq/include/System/Xml/Linq/XObject.hpp.audit.md`;
- `modules/collections/include/System/Collections/Generic/LinkedList.hpp.audit.md`.

**Related, but deliberately not a member (ticket #1782, 2026-07-27):**
SR-AUD-361 (`SortedSet<T>::GetViewBetween`) is **not** a CCF-019 instance and
must not be counted as one. Its returned object is a fully detached snapshot
that retains no pointer or iterator into the source, so it exhibits the opposite
of this shared cause: probe evidence confirms it survives owner destruction with
no sanitizer diagnostic. The cross-reference is recorded only because ticket
#1782's design independently selected the *same ownership idiom* CCF-019's
LinkedListNode repair used -- independently allocated, reference-counted state
shared by every handle -- for a different reason: to give the live bounded view
required by SR-AUD-361 a lifetime rule equivalent to .NET's GC-rooted
`TreeSubSet._underlying`. See `docs/SortedSetLiveViewDesign.md` §12. Ticket
#1782 additionally measured, inside that same class, one genuine instance of
this cause's failure mode that is **not** SR-AUD-361 and receives no new
identifier (the numbering is frozen at 364): `SortedSet<T>::Iterator` stores a
raw `const SortedSet*` owner, and whole-object assignment overwrites the
`version_` guard instead of bumping it, so copy-assignment yields a silently
wrong dereference and move-assignment is an ASan-confirmed
`heap-use-after-free`. It is folded into ticket #1783's scope, which replaces
that raw owner pointer with a `shared_ptr<const State>`.

**Closed (ticket #1783, 2026-07-28).** That instance is now repaired.
`SortedSet<T>::Iterator` holds `std::shared_ptr<const State>` instead of a raw
`const SortedSet*`, and copy assignment rebinds the handle instead of
overwriting the version counter, so the state an iterator enumerates cannot be
freed or silently swapped underneath it. Re-running the same probe against the
shipped header: `copy-assign` now yields the correct pre-assignment element
where it previously produced a silently wrong one with no diagnostic at all,
`move-assign` exits 0 with no report where it was an ASan
`heap-use-after-free`, and `outlive` exits 0 where it was an ASan
`stack-use-after-scope` inside `checkVersion()` itself. SR-AUD-361 is
`remediated`; it was never a CCF-019 member and still is not counted as one, so
this cause's membership list is unchanged.

**Unaffected by ticket #1785 (2026-07-28).** The nested-view exception-ordering
parity correction changes only which exception a `GetViewBetween` call that is
simultaneously widening and inverted selects. It moves one `if` inside one
inline body and touches no owner pointer, no `shared_ptr`, no lifetime rule, and
no member of `SortedSet<T>` or `Iterator`. This cause's membership list, the
closure above, and SR-AUD-361's `remediated` status are all unchanged by it.

## CCF-020 — raw polymorphic output parameters erase the validation information public contracts require

The legacy non-generic ICollection interface accepts `void*` plus a starting
index but has no element type, nullability, rank, or capacity representation.
ArrayList, Queue, Stack, Hashtable, and ListDictionaryInternal therefore write
through unvalidated caller storage; ArrayList's null destination is
ASan-confirmed. This is a shared interface-design fault rather than five
independent bounds omissions. A safe repair needs a typed/length-aware adapter
or a deliberately constrained API migration. See SR-AUD-358 and:

**Design status (ticket #1770, 2026-07-27): DESIGN-COMPLETE — NOT REMEDIATED.**
SR-AUD-358 remains `confirmed`; the original evidence above and in the per-file
reports is retained unchanged. Design-only ticket #1770 recorded the selected
contract in `docs/ICollectionCopyToDesign.md` and established two facts beyond
the original evidence, by direct probe against the current headers: the six
implementations disagree on the destination element type (`std::any*`, `void**`,
`DictionaryEntry*` — sizes 16/8/32), so no `ICollection*` caller can allocate a
correct destination; and an element-type mismatch through the interface produces
no crash at all, only a LeakSanitizer-confirmed 32-byte leak from
`Hashtable::CopyTo`. Both confirm this is one interface-design fault, not five
bounds omissions. Selected: a length-aware, statically typed `Span<std::any>`
destination behind a non-virtual interface, so validation runs exactly once in
`ICollection` before any implementation writes; `CopyTo(void*, intcs)` leaves
the virtual interface and is retained briefly as a deprecated, never-writing
shim. .NET's rank, non-zero-lower-bound, and element-type-mismatch diagnostics
are intentionally unsupported because they require a runtime `Array` object and
a working `System::Type`, both permanently out of scope. Implementation is
proposed as inactive ticket #1771 and is gated on explicit user approval of the
narrow public-API break.

**Remediation status (ticket #1771, 2026-07-27): REMEDIATED.** SR-AUD-358 is
`remediated`; the original evidence above and in the per-file reports is retained
unchanged. The user approved the public source- and ABI-breaking change, and
ticket #1771 landed it: `virtual void CopyTo(void*, intcs) = 0` is **removed**
from `ICollection`, replaced by non-virtual, validating
`CopyTo(ObjectSpan, intcs)` / `CopyTo(std::vector<std::any>&, intcs)` plus one
protected pure virtual `copyToCore(ObjectSpan, intcs)` per implementation.
`detail::requireValidCopyDestination` is now the single validation site shared by
all six implementations and by the typed `std::vector<void*>` /
`std::vector<DictionaryEntry>` concrete overloads, so the "five independent
bounds omissions" shape this finding warned about is structurally unreachable: an
implementation cannot be entered without it. The approved decision departs from
the design record in one respect (section 21 of `docs/ICollectionCopyToDesign.md`):
the deprecated, never-writing shim was **not** retained, because a shim would let
a stale call site compile and fail at run time while removal makes it a compile
error naming the replacement. Evidence: the original probe's four scenarios no
longer compile (four `no matching function` diagnostics, each listing the
surviving overloads); the replacement probe runs the same scenarios plus
non-trivial-value, heterogeneous, and 100,000-element cases under ASan + UBSan +
LeakSanitizer with zero diagnostics and zero leaks; 128 permanent regressions
across every implementation (also clean under sanitizers); 1,612/1,612
Collections.Core; 12,871 tests across 37 executables. Consumer guidance is in
`docs/Migration-ICollectionCopyTo.md`. CCF-019's JsonNode and XML LINQ members are
unaffected and remain `confirmed`.

**Follow-up correction (ticket #1774, 2026-07-27): still REMEDIATED.** SR-AUD-358
and CCF-020 are not reopened; the paragraph above is left as the historical
record of what #1771 shipped. #1771's `detail::requireValidCopyDestination`
rejected every null-pointer destination outright, including a valid empty
`ObjectSpan{nullptr, 0}` or a default-constructed empty `std::vector<std::any>`
copied from an empty collection — stricter than intended, since `ObjectSpan` has
no distinct managed-null-array state and a null-and-zero-length destination is
simply "no storage, no elements", matching .NET's `new object[0]`. Ticket #1774
corrected the rule so a null pointer is rejected only when paired with a
*positive* length; a non-empty collection copied into a zero-length destination
still fails, but on capacity, not nullness. Evidence: `CopyToBoundaryTests.cpp`
gained parameterised empty-to-empty, malformed-null-with-length, and
`copyToCore`-not-reached cases (1,662/1,662 after the addition); the standalone
probe `build-probe-copyto/probe10_empty_span_correction.cpp` passes 10/10
assertions under ASan + UBSan + LeakSanitizer with zero diagnostics and zero
leaks. Recorded in section 22 of `docs/ICollectionCopyToDesign.md`.

- `modules/collections/include/System/Collections/ICollection.hpp.audit.md`;
- `modules/collections/include/System/Collections/ArrayList.hpp.audit.md`;
- `modules/collections/include/System/Collections/Queue.hpp.audit.md`;
- `modules/collections/include/System/Collections/Stack.hpp.audit.md`;
- `modules/collections/include/System/Collections/Hashtable.hpp.audit.md`;
- `modules/collections/include/System/Collections/ListDictionaryInternal.hpp.audit.md`.
