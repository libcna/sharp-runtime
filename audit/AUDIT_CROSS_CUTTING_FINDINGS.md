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

**PARTIALLY REMEDIATED (tickets #1876/#1877/#1878, 2026-07-30). Not closed.**
The family plan is `docs/DateTimeValidationBoundaryPlan.md` (19 sections, ticket
#1876), built on 84 measured probe cases plus five one-shape-per-process UBSan
runs. **SR-AUD-006 is `remediated`; SR-AUD-007 is split — `007a remediated`
(offset minutes), `007b open` (grammar) — and SR-AUD-009 stays `confirmed`.**
The plan draws its line exactly where #1857 -> #1858 and #1864 -> #1865 drew
theirs: a change to the accepted **range of component values** is compatible; a
change to the accepted **textual grammar** is approval-gated.

**Membership is larger than the record above states, and one member is outside
it.** The constructor half is **five** public constructors (three `DateTime`,
two `DateTimeOffset`) reached through **six** wrappers that add no validation of
their own, including `Globalization::Calendar::ToDateTime` — which .NET also
leaves unvalidated, so it inherits the repair with no `modules/globalization`
edit. The parser half is **eight** entries across four types, because
**SR-AUD-061** (`DateOnly::TryParse` accepts trailing text) is the same
`std::sscanf`-prefix omission in the same module and is planned with
SR-AUD-007/009 even though this record does not list it.

**The record understates the family's severity: it contains reachable undefined
behaviour.** `hour * TicksPerHour` in `DateTime::dateToTicks` is
`(long long)hour * 36000000000`, computed before any check of `hour`, and
overflows `int64` for `|hour| > 256204778`. UBSan confirmed it from a plain
constructor call — which then **returned** a `DateTime` with a negative tick
count — and, more seriously, from a public string parse:
`DateTime::TryParse("2024-06-15 2000000000:00:00", out)` returned **`true`** with
`ticks = -1148436230838206464`. `TryParse`'s `catch (...)` cannot help, because
undefined behaviour is not an exception. `hour` is the only operand that can
overflow. The same constructors also published objects outside `DateTime`'s own
documented `[0, MaxTicks]` invariant: `DateTime(9999,12,31,24,0,0)` stored
`MaxTicks + 1` and `DateTime(1,1,1,-1,0,0)` stored a negative tick count, because
the component constructors initialise `ticks_` directly and never reach
`DateTime(longcs)`'s range check.

**A second, independent cause the record does not name: validation order.**
.NET evaluates `ValidateOffset(offset)` **before** constructing the clock
`DateTime` in every `DateTimeOffset` component and tick constructor. This port
validated it last — not by choice, but because the clock value was produced in a
mem-initialiser, which cannot sequence a free-standing check before a member's
construction. That had to be corrected in the *same* change as the component
validation: with the hour check added and the order left alone,
`DateTimeOffset(2024,1,1,24,0,0,+15h)` would have started reporting the hour
instead of the offset. Reordering keeps it, and the whole-minutes case, exactly
as they were.

**The repair is one helper, not a sweep.** Every SR-AUD-006 symptom — silent
normalisation, the invariant breach, the UB, the `DateTimeOffset` inheritance and
the parser's acceptance of out-of-range component values — is a single omission
in `DateTime::dateToTicks`, observed from a different entry point.
`TimeOnly::validateHms`, 45 lines away, already carried the missing rule with
.NET's verbatim message and `paramName`; the repair copies it. `TimeOnly` is a
**counter-example inside the family**, not a member of its constructor half: its
constructors and its parser's component checks were already correct, and are now
pinned by tests.

**Premises corrected (measured 2026-07-30, historical text preserved).**

- **The "Required post-audit verification" paragraphs of all four reports are
  wrong about .NET in one respect.** Each asks for an assertion that a failed
  `TryParse` does **not** overwrite its output argument. .NET does the opposite:
  `DateTimeParse.cs:2470` assigns `DateTime.MinValue` before returning `false`.
  The port's preservation is a divergence, and that assertion would have pinned it
  as the contract. Recorded, pinned as *current* behaviour only, and carried to
  **inactive ticket #1880** with no `SR-AUD-*` identifier issued.
- **The port's year/month/day exception identity already deviates from .NET and
  is not part of SR-AUD-006.** .NET throws `paramName = null` with
  `Year, Month, and Day parameters describe an un-representable DateTime.`; the
  port throws `paramName = "year"`/`"day"` with its own messages. Pre-existing,
  observable, deliberately excluded.
- **SR-AUD-007's `+02:75` reproduction understates its class.** `+02:60`,
  `+02:99`, `-02:75` and `+02:-30` all reproduce; the last silently *subtracts*
  thirty minutes from the hour field, yielding `+01:30`.
- **SR-AUD-009's fractional scan is not generally wrong.** `"10:20:30.1"`
  correctly yields `.100`. Only the residual-character and bare-dot halves are
  defects, and both are grammar.
- **`ISOWeek`, `TimeSpan` and `Calendar`'s own guards are not defective.**
  Compared line-by-line against `ISOWeek.cs` (including its deliberate
  `dayOfWeek == 7` acceptance) and `TimeSpan.cs`, which accepts `TimeSpan(25,0,0)`
  by design.

**Deliberately not broadened.** The `TimeZoneInfo` family
(SR-AUD-224…SR-AUD-230), the abstract-`Calendar` shape finding (SR-AUD-281),
`XmlConvert`'s duration grammar (SR-AUD-354) and the `Stopwatch`/`TimeProvider`
elapsed-time arithmetic (SR-AUD-131) are all date/time code and **none** shares
this cause. No `SR-AUD-*` identifier was issued for anything; the numbering stays
frozen at 364.

**What remains before CCF-002 can be closed:** ticket **#1879** (`needs_user`) —
the accepted-grammar change covering SR-AUD-007b, SR-AUD-009 and SR-AUD-061,
recorded with its exact before/after string table — and inactive ticket **#1880**
(`TryParse` failure-output normalisation). Until both are answered this family is
`PARTIALLY REMEDIATED`, and "three of five classes done" must not be read as
closure.

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

**REMEDIATED — tickets #1843/#1844/#1845/#1846/#1847 (2026-07-29/30). CCF-003 is
CLOSED.** All six members are `remediated`: SR-AUD-019 (`Int128` `MinValue`
negation UB), SR-AUD-020 (`UInt128` shift counts masked with `& 127`, matching
`UInt128.operator <<`'s `shiftAmount &= 0x7F`), SR-AUD-021 (unknown format
specifier now raises `FormatException` in all ten integer wrappers, and see
CCF-006 below for the `Single`/`Double` slice), SR-AUD-022 (inverted `Clamp`
bounds rejected with `ArgumentException` across eleven overloads, replacing a
`[alg.clamp]` precondition violation), SR-AUD-023 (`B`/`b` binary formatting
added — for **seven** types, not the six the record names, because `Int128` was
missing it too) and SR-AUD-024 (`SByte`/`Int16` `IsPositive(0)`). The family
plan is `docs/NumericWrapperBoundaryPlan.md`; per-finding evidence is in
`audit/AUDIT_FINDINGS_INDEX.md`. **This closure note was added by ticket #1911
on 2026-07-31**, which found that this section had carried no closure paragraph
since the last member landed — a record defect, not a code defect, and one that
would have made a future session re-select an already-finished family. No
finding status changed and no `SR-AUD-*` identifier was issued.

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
`LinkedList<T>` and `BitArray` kept a 32-bit counter and a documented 2^32
residual, tracked by blocked tickets #1788 and #1789. **This cause's membership
list is still unchanged.**

**Update, 2026-07-29 (ticket #1788, no `SR-AUD-*` identifier).** `LinkedList<T>`'s
half of that residual is **closed**. The user granted the explicit object-size
approval, and both its counter and its `Enumerator`'s snapshot are now 64-bit, so
`sizeof(LinkedList<T>)` grew 40 → 48 on LP64 while `sizeof(Enumerator)` stayed 40
and **no mangled name changed**. The pre-fix 2^32 revalidation was reproduced
first (`guard-fired=0` three times, `defects-observed=3`) and reads
`defects-observed=0` after. **`BitArray` alone still carries this residual**,
deliberately: closing it grows the *public* `BitArray::Enumerator` from 32 to 40
bytes, which is ticket #1789's separate approval and remains `blocked`. The
membership list of this cause is still unchanged; the record is
`docs/CollectionVersionCounterSweep.md` §19.

**Update, 2026-07-29 (ticket #1789, no `SR-AUD-*` identifier).** The residual is
now **fully closed**. The user granted the second, separate object-size approval,
and `BitArray`'s counter and its **public** `Enumerator`'s snapshot are now both
64-bit: `sizeof(BitArray::Enumerator)` grew 32 → 40 on LP64 while
`sizeof(BitArray)` stayed 48 (the counter landed in tail padding it already had)
and **no mangled name changed**. The pre-fix 2^32 revalidation was reproduced
first (`guard-fired=0` for `MoveNext`, for `Reset`, and at seven laps,
`defects-observed=3`) and reads `defects-observed=0` after. **No collection in
this repository retains a 2^32 enumerator-snapshot horizon; every one is 2^64**,
and `detail::NarrowMutationCounter` has no user left. The membership list of this
cause is still unchanged; the record is
`docs/CollectionVersionCounterSweep.md` §20.

**Update, 2026-07-29 (ticket #1829, no `SR-AUD-*` identifier).** This cause now has
a family plan, [`docs/DefinedArithmeticBoundaryPlan.md`](../docs/DefinedArithmeticBoundaryPlan.md),
written before any member was implemented, and all eight members were
**re-reproduced under UBSan on that date** rather than taken from this document's
wording (`build-probe/1829_ccf004_survey.cpp`, one process per case,
`-fno-sanitize-recover` so a UB case aborts). Every one still reproduces. The
membership list is unchanged. Three things the survey established are recorded
here because they change what a future implementer must do:

1. **The members are not interchangeable, and split three ways.** Most are
   *defined-wrap* cases whose current result is already the intended value and
   whose repair changes no observable behaviour at all; one
   (`ReadOnlyMemory::Slice(start)`) needs only its existing check moved ahead of
   the arithmetic; and two — `TimeSpan::TryParse` and the `DateOnly` arithmetic —
   currently produce a **wrong answer** and must start failing. Only those two need
   a compatibility argument. Treating the cause as one repair, which its
   single-sentence framing above invites, would be wrong.
2. **SR-AUD-060 is larger than this document and its own report record.** Four
   `DateOnly.cpp` sites were named (65, 76, 81, 92); the overflow **cascades** into
   `jdnToDate`, which overflows three more times per call at `DateOnly.cpp:35`,
   `:37` and `:39`. Cases 8 and 9 each report **four** UB operations. The real site
   count is **seven**, and guarding only the four named entry points while
   `jdnToDate` stays reachable with a wrapped argument is not remediation. See the
   plan's §2.1.
3. **These findings cannot be reproduced by the obvious probe, which is a trap
   worth naming.** A probe built with `-fsanitize=undefined` but linked against
   `build/` instruments only the header-only members and is blind to everything in
   a `.cpp`, because `build/` is not a sanitizer tree; and at `-O1` GCC
   constant-folds an inlined header overflow and emits no check at all. The first
   run of this survey therefore reported SR-AUD-049, SR-AUD-060 and SR-AUD-008 as
   already fixed, which is false. Link against `build-asan/` and compile at `-O0`.
   The plan's §3 carries the exact recipe. **Absence of a UBSan report is not
   evidence of absence of UB for any member of this cause.**

Nothing above changes any finding's identifier or status: all eight remain
`confirmed`, and the audit numbering stays frozen at 364. The implementation split
is tickets #1830–#1837.

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

**REMEDIATED — tickets #1847 (integer slice) and #1849 (float slice), 2026-07-29/30.
CCF-006 is CLOSED.** Its single member, SR-AUD-021, is `remediated` on both
slices. The ten integral wrappers now raise `System::FormatException` for an
unrecognised format specifier instead of silently treating it as a
general/decimal request, and `Single::ToString`/`Double::ToString` no longer let
`std::invalid_argument`/`std::out_of_range` escape from the precision
`std::stoi` — they raise `FormatException("Format specifier was invalid.")` and
reject an unrecognised specifier loudly, so the two wrapper families now share
one validation policy, which is exactly what this cause asked for. `F`, `E`,
`G`, `R` and `N` remain valid. The residual `N` group-separator and `E`
exponent-width **fidelity** gaps are CCF-007's subject, not this cause's, and
stay open there. **This closure note was added by ticket #1911 on 2026-07-31**,
on the same terms as CCF-003's above: a record defect only, no status change and
no `SR-AUD-*` identifier.

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

**Remediated — ticket #1855 (2026-07-30), CCF-005 Decimal slice.** CCF-008 is
**closed**. SR-AUD-036 was its only member (Decimal + Math + MathF, no fourth
type). All three `Round(…, MidpointRounding)` funnels now reject an out-of-range
enum value with `System::ArgumentException(paramName "mode")`, message
`The value '{n}' is not valid for this usage of the type MidpointRounding.`
(mirroring .NET's `SR.Argument_InvalidEnumValue`). Decimal validates the mode
unconditionally before its scale-vs-decimals early-out (matching
`Decimal.Round(ref, int, MidpointRounding)`); Math/MathF replace the `switch`
default's silent ties-to-even with the throw (matching
`ThrowHelper.ThrowArgumentException_InvalidEnumValue`). Every named mode 0-4
still returns its exact value; +9 add-only tests. Per .NET parity, the
`Round(value, digits, mode)` overloads validate the mode *only through the
funnel*, so a magnitude at/above the round limit (Math ≥ 1e16, MathF ≥ 1e8)
returns unchanged without validating the mode — a faithful match to .NET's own
Math.cs/MathF.cs, not a gap.

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

**REMEDIATED AND COMPLETE — tickets #1901, #1902, #1903, 2026-07-31.** Both
members now have a real ownership boundary and SR-AUD-010 is `remediated`,
taking the post-audit tally to **58 remediated / 305 confirmed / 364**;
numbering stays frozen at **364**, no new `SR-AUD-*` was issued and no `CCF-*`
cause was added. Planned in `docs/SharedPrngConcurrencyPlan.md`, whose §11–§16
hold the measured evidence.

Each site kept its public API exactly, as this cause requires. `Guid::NewGuid`'s
engine and distribution became per-thread (#1901) — nothing is handed out there,
so per-thread state is exactly equivalent. `Random::getSharedProperty()` (#1902)
kept **one object with one stable address on every thread** and moved only the
mutable state per-thread, routed through `internalSample()`, the sole writer and
the funnel every entry point on the class passes through. That asymmetry is the
substantive finding of the family: the two singletons look alike but only one
hands a reference to the caller, and .NET rejects the per-thread-instance
shortcut explicitly at `Random.cs:755–759` for exactly that reason. Both repairs
are confined to `.cpp` bodies — no header touched, `sizeof`/`alignof` unchanged
and asserted, external symbols identical before and after.

The rule this section states — that repairing one singleton leaves the other
independently unsafe — was honoured procedurally: #1901 and #1902 each carried
their own TSan gate and neither was allowed to close the finding, which ticket
#1903 did only after both landed. The 8-thread probe reported **13** races at
`Guid.cpp:344` and **6** inside `Random::internalSample()` before, and is clean
at both sites after. +14 tests; ASan/UBSan/LSan clean; a seeded `Random(seed)`
stream byte-identical across 4,928 dumped values; 100,000 concurrent `NewGuid()`
with zero duplicates.

**What TSan could not have caught, and why the mutation gate matters here.** Two
plausible wrong repairs are perfectly race-free: a `thread_local` shared
*instance*, and per-thread engines that every thread seeds identically. The first
silently hands a cross-thread caller a different generator; the second replaces a
data race with duplicate GUIDs. Both are pinned by tests that fail under
mutation while every sanitizer stays green (§15). A future contributor reaching
for either will be stopped by the suite, not by TSan.

The `SortedSet<T>` exclusion recorded below is unaffected: #1784 was per-object,
caller-owned state and remains **not** a member of this cause.

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

**REMEDIATED AND COMPLETE — tickets #1904 through #1910, 2026-07-31. CCF-010 is
CLOSED.** Its single member, **SR-AUD-046, is `remediated`**, taking the
post-audit tally to **59 remediated / 304 confirmed / 364**; numbering stays
frozen at **364**, no new `SR-AUD-*` was issued and no `CCF-*` cause was added.
The original evidence above and in the six per-file reports is retained
unchanged. Planned in `docs/ComparisonContractPlan.md` (19 sections plus §0's
candidate-family review and §18a's newly discovered population), written before
any production file was changed.

**This cause named the right repair and this batch did exactly it.** The
demand — "centralize or consistently reuse the local comparison policy;
changing only one surface leaves the others divergent" — is met by one new
private header, `modules/core/include/System/detail/ComparisonPolicy.hpp`,
stating the port's counterpart of `Comparer<T>.Default` and
`EqualityComparer<T>.Default` once: `compareValues`, `equalValues`, `hashValue`,
`DefaultLess`/`DefaultGreater`, `moveNaNsToFront` and `defaultSort`. All **66**
comparison sites across the six headers route through it, and each entry point
is `if constexpr`-gated so every non-floating instantiation generates exactly
the code that was there before.

**The record understates its own severity, and that is the substantive finding
of the family.** This section says the operators "make NaN unequal to itself and
unordered against every finite number", i.e. a wrong *answer*. Measured against
the shipped headers before anything changed
(`build-probe/1904_ccf010_probe.cpp`, 36 cases, one forked process each, every
operand produced at run time through `volatile`), `Array::Sort` and
`MemoryExtensions::Sort` also hand `std::sort` a predicate that is **not a
strict weak ordering** — NaN is "equivalent" to every finite value while finite
values order among themselves, so equivalence is not transitive — and
`[alg.sort]` makes that a precondition violation rather than a wrong answer. The
consequence is that the **non-NaN** elements come out **unsorted**: **64 of 196**
size/density/placement shapes corrupted, worst case **3,874 inversions** in
65,536 elements. A single NaN in 4,096 elements does *not* corrupt, which is
exactly why the audit's three-element `{3,NaN,1}` example could not find it.
**No new `SR-AUD-*` identifier was issued** — this was found during remediation
of an existing finding, in the files that finding owns.

**No sanitizer sees any of it.** Across all 36 cases, AddressSanitizer,
UndefinedBehaviorSanitizer and `-D_GLIBCXX_ASSERTIONS -D_GLIBCXX_DEBUG` each
reported **zero** diagnostics, before and after. libstdc++'s debug mode is blind
for a checkable reason: `__glibcxx_requires_irreflexive` verifies `!comp(x, x)`,
and `NaN < NaN` is `false`, so irreflexivity holds; nothing checks transitivity
of equivalence. The permanent suite is therefore the **only** gate, which is why
this family carries **ten** mutations rather than the usual two or three — and
one of them is a deliberate **negative** control (`Sort` via a total-order
comparator instead of the pre-pass) that must and does still pass, so the suite
is shown to pin the contract rather than the implementation.

**Both `Sort` surfaces use .NET's own repair**, not a hand-written comparator:
`SortUtils.MoveNansToFront` followed by an ordinary sort of the remainder
(`ArraySortHelper.cs:285-305`, whose comment gives the same reason). That makes
the precondition violation structurally unreachable. The 196-shape sweep reads
`corrupted=0` afterwards.

**Five further premises were corrected by measurement** and are recorded in
`docs/ComparisonContractPlan.md` §9 rather than by editing the text above:

- **The `MemoryExtensions` half of this section is stale.** That file already
  held a correct `float.Equals` rule in `Detail::MemoryExtensionsElementEquals`,
  used by `Contains`, `IndexOf`, `LastIndexOf`, `Count`, `Replace`,
  `SequenceEqual`, `StartsWith` and `EndsWith`. The real defect was that the
  rule was stated in one file and shared with none; it now forwards to
  `System::detail::equalValues`.
- **`Linq::Min` and `Linq::Max` fail in opposite directions**, where the report
  reads as one symmetric problem: `Min({1,2,NaN})` *lost* the NaN and returned
  1, while `Max({NaN,1,2})` *kept* it and returned NaN. The reference is
  deliberately asymmetric (`Min.cs:130-139` explains why), and a repair that
  made them symmetric would have broken one. A mutation pins this.
- **`Nullable<T>::operator==` was already correct and must not be "fixed".**
  C#'s *lifted* `==` on `T?` is the underlying type's `operator ==`, so
  `(double?)NaN == (double?)NaN` is `false` in .NET too. The `Nullable.hpp`
  report's framing invited changing it alongside `Equals`; that would have been
  a divergence. A permanent test now pins the asymmetry — `a == b` false and
  `a.Equals(b)` true on the same pair.
- **Hashing is part of the repair, not an extra.** Once equality is
  NaN-reflexive, a hash that depends on NaN's payload bits breaks the
  equal-objects-equal-hashes invariant that did not exist before. .NET folds NaN
  and zero together in `Single.GetHashCode`; `hashValue` reproduces that, and
  `Nullable`, `Tuple` and `ValueTuple` route their `GetHashCode` through it.
- **Two rows were already right by accident and are now pinned**:
  `SequenceCompareTo` of two NaNs returns 0, and `Array::BinarySearch` for a
  finite value in a NaN-led array finds it.

Source, ABI, layout, vtable and `noexcept` consequences: **none**. No signature,
`noexcept` specification, virtual function, vtable slot, calling convention,
data member or object layout changed anywhere in the family;
`nm --extern-only` on `libsharp_runtime_core.a` is **identical** before and
after (6,168 symbols) and `sizeof`/`alignof` are identical for all eleven
measured instantiations. Every changed body is `inline` or a template, so a
**full consumer rebuild is mandatory and silent if skipped** — the standing
hazard recorded for #1791, #1802, #1867 and CCF-012. Measured cost on 1,000,000
elements: 1.011× for `float` without NaN, 0.994× with 1% NaN, 1.009× for `int`.

Evidence: **70 permanent add-only regressions** in
`modules/core/tests/System/ComparisonContractTests.cpp`; whole repository
**14,815 tests across 37 executables**, from 14,745; build clean with zero
errors and zero warnings; module graph unchanged at **41/91**; Doxygen 1,941
(ceiling 1,942); `scripts/check_selective_components.sh` passed, run because
the family adds a public header. TSan is recorded **not applicable** rather than
skipped: nothing in the family has shared mutable state, an atomic, a lock, a
cache or a singleton, and every function is pure over its arguments.

**A larger population with the identical cause exists in the `Collections`
module and was deliberately not absorbed.** Measured while planning: **five**
default-ordering sites (`List<T>::Sort`, `List<T>::BinarySearch`,
`ImmutableList<T>::Sort` ×2, `ImmutableArray<T>::Sort`), of which the four
`std::sort` calls carry this cause's precondition violation exactly; **56**
default-equality sites across 20 headers; and a **third shape with no CCF-010
counterpart** — `SortedSet<T>` (`std::set<T>`), `SortedDictionary<K,V>` and
`SortedList<K,V>` (`std::map`) use `std::less` on a possibly-NaN element or key,
violating `[associative.reqmts]`'s ordering requirement for the container's
whole lifetime rather than for one algorithm call. This is a **newly discovered
population found during remediation**, not a CCF-010 finding: **no `SR-AUD-*`
identifier was issued**, and it is ticket **#1912** with the full inventory in
`docs/ComparisonContractPlan.md` §18a. Nothing there is claimed to be wrong
until #1912 reproduces each site itself.

Deliberately **not** members and not closed by this work: the caller-supplied
comparer overloads of `Array::Sort`, `Array::BinarySearch` and
`MemoryExtensions::Sort`, which pass the predicate straight through exactly as
.NET's `IntrospectiveSort` does with a non-default `IComparer<T>`;
`SequenceCompareTo`'s length-tail magnitude; `Linq::Distinct`'s O(n²) loop and
`Linq::OrderBy`'s per-comparison key selection; SR-AUD-044, SR-AUD-051,
SR-AUD-053, SR-AUD-063 and SR-AUD-135 in the same reports; and **CCF-015**
(SR-AUD-048), which shares `MemoryExtensions.hpp` but has an entirely different
cause.

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

**Remediated — tickets #1866/#1867/#1868/#1869/#1870 (2026-07-30). CCF-011 is
CLOSED.** All six members are `remediated`: SR-AUD-052 (`Array`), SR-AUD-058
(`Progress<T>`), SR-AUD-065 (`Lazy<T>`), SR-AUD-099
(`AggregateException::Handle`), SR-AUD-121 (`EventHandler<TEventArgs>`),
SR-AUD-134 (`Linq`). The original evidence above and in the per-file reports is
retained unchanged.

Design-only ticket #1866 recorded the family plan in
`docs/EmptyCallableBoundaryPlan.md` and established the single policy this cause
asked for, which the prose above had already identified in outline: decide
emptiness **at the public boundary, before any element of the input is examined**,
and choose by API shape rather than per type. A delegate *argument* to an ordinary
method is rejected with `System::ArgumentNullException` carrying .NET's own
parameter name (`match`, `comparison`, `converter`, `action`, `predicate`,
`selector`, `keySelector`, `valueFactory`); an *event subscription* is a no-op,
because C# `event += null` is `Delegate.Combine(d, null) == d`; an event *raise*
skips untruthy handlers. Thirty-six public entries across six types now implement
it, and where .NET validates other arguments first the port reproduces that order
— including `Array.FindIndex`'s range-before-callable and
`Array.FindLastIndex`'s callable-before-range asymmetry, which is real reference
behaviour rather than an oversight.

The prose above is confirmed on its central claim — the failures were "different
.NET-compatible outcomes" needing one policy, not one blanket rule — and is
corrected on the *extent* of the silent region in three places, each measured
rather than inferred (`build-probe/1866_prefix.log`, 60 cases):

- The silence was **size**-dependent, not merely emptiness-dependent.
  `Array::Sort`, `Array::BinarySearch`, `Linq::OrderBy` and
  `Linq::OrderByDescending` were also silent for a **one-element** input, because
  `std::sort`/`std::stable_sort` never invoke the comparator below two elements.
- `Linq::First(empty, {})` did not return a normal result: it threw
  `InvalidOperationException`, so the *sequence* error masked the *argument*
  error. The repair is a validation-order change there, not only an added check.
- `AggregateException::Handle` was silent for an aggregate with **no** inner
  exception, and `EventHandler::Add` failed **inside `Add` itself** — not at
  `Raise` — whenever a replay hook was set, which is what makes the empty check's
  position before the hook binding.

Two observable consequences are recorded rather than assumed away: a call that
previously returned an ordinary result for an empty or one-element input now
throws (the call was already wrong, and .NET throws for the identical call), and
`EventHandler::Size()` no longer counts a subscription that could never have been
invoked.

Source, ABI, layout and `noexcept` consequences: **none**. Not one of the
thirty-six entries was `noexcept`; every `Array` and `Linq` entry is a function
template with no mangled symbol to change; no data member, virtual function or
vtable slot was added, removed, reordered or retyped in any of the six types. The
twelve `Array` `Predicate<T>` parameters were renamed `predicate` → `match` so
the signature, the doc-comment and the observable message agree — in C++ a
parameter name is not part of the interface.

Evidence: 48 permanent add-only regressions (Array 13, Linq 8, Lazy 9,
EventHandler 8, Progress 5, AggregateException 5);
`SharpRuntimeTests_Core_Base` 5,301/5,301, from 5,253 before the batch;
whole-repository build clean with zero errors and zero warnings. The direct probe
`build-probe/1866_empty_callable_probe.cpp` was compiled **with**
`-fsanitize=address,undefined` at each step — every one of these headers is
header-only or inline, so instrumenting the probe recompiles the changed code and
there is no stale-archive risk — and exits 0 with zero AddressSanitizer,
UndefinedBehaviorSanitizer and LeakSanitizer reports, including a 2,000-iteration
leak-stress loop over heap-owning callables and handlers that add, remove and
`Clear()` their own list from inside `Raise`/`Report`. Across all 60 cases **no**
`std::bad_function_call` remains; the only eight `no-throw` outcomes left are the
deliberate event-subscription no-ops.

Deliberately **not** members, and not closed by this work: `EventHandler::SetReplayHook`'s
empty value, which is the documented "clear the hook" spelling;
`Progress<T>`'s constructor handler, which already threw correctly;
`Lazy<T>`'s four non-factory constructors, which synthesise a factory that is
never empty; and CCF-010's separate question of *what* a comparison callable
computes (SR-AUD-046), which this work never touches.

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

**PARTIALLY REMEDIATED — tickets #1881/#1882/#1883 (2026-07-30). CCF-012 is NOT
closed.** Its single member, SR-AUD-015, stays `confirmed`. The original
evidence above and in the per-file reports is retained unchanged.

This cause named the right repair — "a shared parsed-token model" — and warned
that "altering just one API preserves divergent brace rules". Both halves were
therefore done in one batch, to one grammar. Design-only ticket #1881 recorded
`docs/CompositeFormatBoundaryPlan.md` (20 sections) and, in its section 0, the
measured comparison against CCF-017 that selected this family.

**The cause was understated, not overstated.** Reproduced against the shipped
library before anything was changed
(`build-probe/1881_family_compare_probe.cpp`, `1881_extra_probe.cpp`; logs
`1881_prefix.log`, `1881_ubsan_prefix.log`, `1881_extra_prefix.log`), the same
root cause — producing output by repeatedly mutating a buffer that already held
substituted argument text — also produced four classes the record does not
mention, every one reachable from an ordinary public call:

- **`String::Format` did not terminate.** `replaceArg` reset its scan cursor to
  0 after every substitution, so it re-read the text it had just inserted.
  `Format("{0}", "{0}")` and `Format("[{0}]", "x{0}y")` never returned. This
  required no malformed format string and no attacker-supplied format:
  `Format("{0}", userText)` hung whenever `userText` contained `{0}` — a
  public-input denial of service in the library's most-called formatting entry.
- **A second, independent non-termination.** `fmtInt`'s padding loop prepended
  one character at a time, copying the whole string each iteration, so
  `Format("{0:D999999999}", 7)` was ~10^18 byte copies. Neither the parse model
  nor `std::stoi` — a third root cause in the same call chain.
- **UBSan-confirmed undefined behaviour**: `String.cpp:149:41: runtime error:
  negation of -2147483648 cannot be represented in type 'int'`, from
  `Format("{0:D-2147483648}", 42)`.
- **Two `std::` exception types escaping a `System`-shaped public API**
  (`std::out_of_range`, `std::invalid_argument`), where the reference raises
  `FormatException`; plus a second silent-corruption shape the audit never
  named, `extractSpec` giving every occurrence of an index the *first*
  occurrence's specifier, so `Format("{0:X}/{0:D3}", 255)` returned `"FF/FF"`.

The severity as filed (`medium`) understates a reachable hang. **No new
`SR-AUD-*` identifier was issued** — all four were found during remediation of
an existing finding, in the files that finding owns — and numbering stays frozen
at 364.

**What #1882 and #1883 shipped.** `String::Format`'s `extractSpec`/`replaceArg`/
`FinalizeFormat` are replaced by one `formatCore` that walks the format string
once and appends to a separate output; all 22 overloads route through it, and so
do the 11 `StringBuilder::AppendFormat` and 11 `Console::Write`/`WriteLine`
wrappers the audit never mentioned. `FormattableString::ToString`'s per-index
find/replace sweep is replaced by the same single pass. `fmtInt`/`fmtDouble`
gained a bounded, non-throwing specifier parse that keeps `std::stoi`'s prefix
semantics — so every specifier that formatted successfully still produces
identical text — while adopting the reference's own digit bound from
`Number.Formatting.Common.cs:93-105`, and an allocation failure inside a
specifier now raises `System::OutOfMemoryException` rather than letting
`std::bad_alloc` escape.

**Why it is not closed.** This cause's own headline claims are the
approval-gated remainder. `{{`/`}}` escaping, rejecting a malformed closing
brace, alignment padding, and `FormattableString`'s missing-index
`FormatException` all change what **currently-succeeding** calls return or
whether they throw at all, across 46 public entries. They are one coherent
decision — adopt .NET's composite-format grammar — split into ticket **#1884**
(`needs_user`) with fourteen exact before/after rows and the precise approval
wording in the plan's section 20, following the #1857/#1858, #1864/#1865 and
#1878/#1879 precedent. Four of the six rows in the plan's engine-divergence
table (§5.3) are therefore still open, and the plan states in advance
(§18) that CCF-012 closes only when #1884 lands.

Compatibility of what did land: **none**. No signature, `noexcept`
specification, virtual function, vtable slot, calling convention, data member or
layout changed in either file; `String` has no data members and
`FormattableString` keeps `format_`/`args_` in order. `FormattableString.hpp`'s
body is inline, so a consumer must recompile — the ordinary consequence of an
inline change, as with #1867/#1868/#1870.

Evidence: **54 permanent add-only regressions** (30 `StringFormatBoundaryTests`,
20 `FormattableStringBoundaryTests`, 4 `StringBuilderTests` pinning the wrapper);
every one of the 38 pre-existing `String::Format` tests, 11
`FormattableStringTests2` cases and 13 integration cases passes **unmodified**.
Whole repository **14,568 tests across 37 executables**, from 14,514; build clean
with zero errors and zero warnings. **Mutation-checked four ways**, each rebuilt
and re-executed: re-parsing rendered argument text dumps core on the termination
test; accepting an oversized specifier fails 5 tests; one bounded round of the
replaced engine's cross-argument reinterpretation fails 5 tests; restoring
`FormattableString`'s per-index sweep fails 5 tests. UBSan is silent at
`String.cpp:149` afterwards, with the probe compiled **together with
`String.cpp`** so the changed code is instrumented directly. ASan + UBSan +
LeakSanitizer over 3,675 `String::Format` cases and the full `FormattableString`
matrix — every format string and operand built at **run time** so constant
folding cannot suppress a diagnostic, including 100 000-character formats with an
item at the last byte — report zero diagnostics, zero leaks and **0 escaped**
non-`System` exceptions. TSan recorded **not applicable**: neither engine has
shared mutable state, an atomic, a lock or a cache.

Deliberately **not** members and not closed by this work: SR-AUD-016
(`String::LastIndexOf` range spill, same file, unrelated cause); the
UTF-8/culture classification bullets in both per-file reports, which are
CCF-015's subject (SR-AUD-048); `Concat`/`Join` size-overflow diagnostics;
`fmtInt`'s `unsigned int` hex truncation for a 64-bit argument; and
`System.Text.CompositeFormat`, which is not ported.

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

**REMEDIATED (ticket #1816, 2026-07-29).** This cause is closed. Its single
member finding, SR-AUD-078, is `remediated`, and the repair covered **both**
headers as this cause required, in one ticket, with tests on both. The trailing
one/two-byte pack is now encoded **before** the backwards loop over the full
3-byte packs — .NET's own order in the `Base64Helper/Base64EncoderHelper.cs`
helper that its `Base64` and `Base64Url` in-place encoders share.

The scope was larger than "4/5-byte source lengths". A 0..24 length sweep for
both types, comparing each in-place result against the same type's own
out-of-place encoder, was wrong in **28 of 50 cases** before the fix and **0 of
50** after: every length with both a full pack and a remainder (4, 5, 7, 8, 10,
11, 13, 14, 16, 17, 19, 20, 22, 23), all of them returning success. A sentinel
byte immediately past the encoded output was never touched in either direction,
so this was silent corruption *inside* the declared output rather than an
overrun — which is why no sanitizer had ever flagged it.

The cause's demand to "test every full-group-plus-remainder boundary" is met by
8 permanent regressions, four per header, including that sweep. The pre-existing
tests covered `dataLength` 2 and 3 only — precisely the two shapes that cannot
exhibit the defect.

The four adjacent findings in the same two headers — SR-AUD-079, SR-AUD-080,
SR-AUD-081, SR-AUD-082 — are **not** members of this cause, were **not** closed
by #1816, and stay `confirmed`. They are scoped, ordered and split into tickets
#1817 through #1820 by `docs/Base64FamilyPlan.md` (ticket #1815, design-only).

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

**Remediated — tickets #1871/#1872 (2026-07-30). CCF-014 is CLOSED.** Both
members are `remediated`: SR-AUD-075 (`SequenceReader<T>`) and SR-AUD-085
(`Utf8Parser`). The original evidence above and in the per-file reports is
retained unchanged.

Design-only ticket #1871 recorded the family plan in
`docs/TryOutputFailureContractPlan.md` and named the cause this record left
implicit: a C# `out T` parameter is **definitely assigned on every returning
path** by the compiler, so the reference's `value = default` lines are mandatory
rather than defensive. Porting `out T` to a C++ `T&` dropped the guarantee and,
with it, the assignment — while the `bytesConsumed = 0` half was kept, which is
why a *checked* failure still looked like a stale success. Ticket #1872 landed
the repair across **11 public entries** — the two `SequenceReader` members and
all nine `Utf8Parser` overloads, not the two examples named above — with
`Utf8Parser`'s ten failure exits routed through one shared private
`fail(value, bytesConsumed)` so the two halves can never again be normalised
independently.

The record's requirement that "a repair needs a consistent default-output
boundary across false returns, plus tests that prepopulate the output" is met
exactly: every new test prepopulates each output with a sentinel no correct
result can produce (value 42 / 99 / `true` / `"stale"`, counter 7).

Four things are established beyond the original evidence, all measured
(`build-probe/1871_prefix.log` before, `build-probe/1872_postfix_asan.log`
after):

- **The defect is grammar-independent.** The `X` and `N` grammars leave the same
  stale value as the default decimal path, and so does a failure *after* a
  successful core parse — a width-range rejection such as `"256"` into `uint8_t`.
  A repair that reset the output only when the core parse failed would have
  missed every one of those.
- **`bytesConsumed` was never wrong, and no partial value was ever published.**
  Every non-throwing failure already set the cursor to 0, and the parser core
  accumulates into locals, copying to the output only after the range check. Of
  the recurring Try-output failure classes, only "output left unchanged where the
  reference writes a default" applies here.
- **The `FormatException` path is parity, not a defect.** Both outputs are left
  unwritten — and .NET does the same deliberately, calling `Unsafe.SkipInit` on
  both before throwing (`ParserHelpers.cs:59-70`). Normalising them would be a
  divergence; a permanent test pins that a caller sentinel survives the throw.
- **The wrapper already compensated for the core.**
  `SequenceReaderExtensions::TryReadLittleEndian` assigns `value = 0` on false
  and reaches `SequenceReader::TryRead` through a helper that *also* assigns
  `T{}`, so two layers implemented the contract the layer between them did not.
  With `BinaryPrimitives::TryRead*`, `Utf8Formatter::TryFormat`,
  `StandardFormat::TryParse`, `ReadOnlySequence::TryGet` and
  `SequenceReader::TryReadTo`, **six** same-module surfaces already had it. This
  was a localised omission, not a module-wide design choice, and all six are now
  pinned by tests so they cannot silently regress.

**Deliberately not broadened.** The repository has roughly 240 other `Try*`
methods. The audit produced no evidence that any of them shares this defect, and
a measured spot-check of the five closest structural siblings found all five
already correct. No `SR-AUD-*` identifier was issued for anything outside the two
findings, and the numbering stays frozen at 364.

Source, ABI, layout and `noexcept` consequences: **none**, with one requirement
recorded rather than assumed away — `value = T{}` needs `T` to be
value-initialisable, which `SequenceReader<T>::TryRead`/`TryPeek` did not
previously require. That is strictly *weaker* than the reference's own
`where T : unmanaged` constraint, and a permanent test instantiates
`SequenceReader<std::string>` to show it. Otherwise no signature, `noexcept`
specification, virtual function, vtable slot or data member changed; `Utf8Parser`
has no data members at all.

Evidence: 14 permanent add-only regressions (`SequenceReader` 7, `Utf8Parser` 7);
`SharpRuntimeTests_Buffers` 536/536; whole-repository build clean with zero
errors and zero warnings. **Mutation-checked**, which is what the record's
"checking only false and zero consumed is insufficient" warning demands: reverting
`Utf8Parser::fail`'s value write fails five permanent tests and reverting
`SequenceReader::TryRead`'s fails three. The direct probe
`build-probe/1871_try_output_probe.cpp` was compiled **with**
`-fsanitize=address,undefined` — both files are header-only, so instrumenting the
probe recompiles the changed code and no stale archive is involved — and exits 0
with zero AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer reports,
including the 20- and 23-digit overflow inputs that drive the accumulator and the
raw `const uint8_t*` walk to their limits. TSan is recorded as **not applicable**:
no shared mutable state, atomic, lock or cache is introduced or touched.

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

**Remediated — tickets #1873/#1874 (2026-07-30). CCF-016 is CLOSED.** All five
members are `remediated`: SR-AUD-093, SR-AUD-094, SR-AUD-095, SR-AUD-096 and
SR-AUD-100. The original evidence above and in the per-file reports is retained
unchanged.

Design-only ticket #1873 recorded the family plan in
`docs/DerivedExceptionHResultPlan.md`; ticket #1874 landed the repair. This cause
asked for "a repeatable constructor-audit and assertion gap" to be closed by
checking "every .NET-shaped exception against its own HResult on every public
overload", and that is exactly the shape of the work: **11 types, 40 public
constructors, 38 wrong results** before, `wrong=0` after
(`build-probe/1873_prefix.log` → `build-probe/1874_postfix.log`).

The root cause is structural rather than eleven independent oversights.
`Exception` initialises `hResult_` to `COR_E_EXCEPTION` and `SystemException`'s
constructor body overwrites it with `COR_E_SYSTEM`; because a C++ base
constructor runs *before* the derived body, a type written as a pure forwarding
constructor — `: SystemException(message) {}` — silently inherits whatever its
nearest base last wrote. The inherited value was never a decision; it was the
absence of one. The same property is what makes the repair safe: a statement in
the derived body always runs last, so no base can overwrite it.

Three things are established beyond the original evidence, all measured:

- **Five findings cover eleven types, not five.** SR-AUD-094 alone spans five,
  and the port has more constructors than the reference in two places
  (`ArrayTypeMismatchException` adds a `const char*` overload,
  `BadImageFormatException` adds `(message, fileName, inner)`). All of them are
  covered.
- **SR-AUD-100's message claim is a false positive.** The port's default text
  `Duplicate objects in argument.` is **byte-identical** to
  `SR.Arg_DuplicateWaitObjectException`
  (`System.Private.CoreLib/src/Resources/Strings.resx:319-321`). Only the HResult
  half of that finding was real; no message change was made, and a permanent test
  now pins the text verbatim so a future reader acting on the finding's wording
  cannot introduce a divergence. The finding is marked `remediated` on its real
  half with the Correction appended, following the SR-AUD-081 / SR-AUD-362
  convention rather than editing the historical text.
- **`AggregateException` is not a twelfth member.** A sweep of all 50
  `modules/core/include/System/*Exception.hpp` headers found exactly twelve with
  no `setHResultProperty` anywhere — the eleven plus `AggregateException`. But
  .NET's `AggregateException.cs` assigns **no** HResult either, so inheriting
  `COR_E_EXCEPTION` is correct parity. It is pinned as a control in both the probe
  and the permanent suite, and deliberately excluded.

**A larger population exists and was deliberately not absorbed.** The same sweep
found **45 of 59** exception types *outside* `modules/core/include/System/`
(`Threading`, `Net`, `IO`, `Text.Json`, `Xml`, `Security.Cryptography`, …) with no
explicit HResult in either header or source. `AggregateException` proves that
inheriting can be correct, so none of the 45 is a defect until it has been checked
against its own reference source. That is a **newly discovered population found
during remediation**, not a CCF-016 finding: **no `SR-AUD-*` identifier was
issued** (numbering stays frozen at 364), and it is recorded with its full
measured type list as inactive ticket **#1875**.

Compatibility: **observable value change only**. No signature, `noexcept`
specification, virtual function, vtable slot, calling convention or data member
changed; every affected type remains an empty derived class and the assignment is
a constructor body statement. Correcting a wrong constant to its documented
reference value is required by this project's own porting checklist (CLAUDE.md
item 5), so it is remediation under a standing rule rather than a discretionary
behaviour change — and the identical shape was already shipped here by
`InvalidCastException` and `InsufficientExecutionStackException`.

Evidence: 18 permanent add-only regressions in `ExceptionRemainingTests.cpp`
(`DerivedExceptionHResultTests`) — one exact-hexadecimal assertion per public
constructor of all eleven types, plus custom-message and parameter-name
preservation, catch-through-base-reference, copy and copy-assign preservation, the
unchanged base codes and the `AggregateException` control.
`SharpRuntimeTests_Core_Base` 5,319/5,319; whole-repository build clean with zero
errors and zero warnings. **Mutation-checked:** deleting one of
`DllNotFoundException`'s three assignments fails two permanent tests. Sanitizers
are recorded as **not applicable** — one integer store per constructor body, no
allocation, ownership transfer, pointer arithmetic, lifetime change, shared state
or new member — rather than skipped silently.

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

**Follow-up (ticket #1802, 2026-07-29): the last divergent row is closed, on the
`Hashtable` side this time.** All three `Hashtable::Remove` overloads were
`_map.erase(key); ++version_;`, so the mutation counter advanced whether or not
the key was present — reproduced at **24 defects over 43 checks** against the
committed headers (`build-probe/1802_prefix.log`). Removing an absent key moved
the counter and then threw `InvalidOperationException` out of **every**
outstanding enumerator kind, after an operation that changed nothing; a full walk
after one absent `Remove` yielded **0 of 3** entries. That is a **false
positive** — `Count` and contents were correct on every row — and it is the
*opposite* direction of error from #1798's, which missed a real mutation. .NET
`Hashtable.Remove` calls `UpdateVersion()` only inside the branch that found and
cleared a bucket (`Hashtable.cs:999`). All three overloads now route through one
private `removeKey()` helper that bumps only when
`std::unordered_map::erase` reports a removal — the value the erase call already
computed and previously discarded, so **no second lookup, no `Contains`
pre-check, no second key conversion, no allocation and no lock** were added. With
#1798 and #1802 both closed, the port's two `IDictionary` implementations agree on
**all ten** version rows of `docs/ListDictionaryInternalSetterDesign.md` §6.1.
`Clear()` keeps its unconditional bump on both, as a decided deviation from .NET
`Hashtable`'s `_occupancy`-guarded early return — `_occupancy` has no
`std::unordered_map` analogue, so the obvious `if (empty) return;` would not
reproduce .NET's rule, and the unconditional bump errs in the memory-safe
direction. No signature, vtable slot, calling convention or object size changed
(`sizeof(Hashtable)` unchanged at 72, 19-entry vtable byte-identical), but every
affected body is `inline` in a header, so a **full consumer rebuild is mandatory
and silent if skipped**. CCF-018, SR-AUD-356 and SR-AUD-363 are **not reopened**;
no new `SR-AUD-*` identifier was created, the numbering staying frozen at 364. New
permanent suite: `HashtableRemoveVersioningTests.cpp` (+67 tests). See
`docs/HashtableValueAccessSafetyDesign.md` §35.

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

**Design status of the two remaining members (ticket #1885, 2026-07-30):
DESIGN-COMPLETE — NEITHER IS REMEDIATED.** SR-AUD-327 and SR-AUD-333 both remain
`confirmed`; the original evidence above and in the per-file reports is retained
unchanged, and **no new `SR-AUD-*` identifier was issued** (numbering stays frozen
at 364). Design-only ticket #1885 reproduced both against the shipped bodies,
one process per case under a watchdog, in three builds from one source
(`-fsanitize=address,undefined`, `-fsanitize-recover=address`, and no sanitizer),
with every production translation unit compiled **from source into the probe** so
no archive could be stale. The selected contract is recorded in
`docs/OwnedTreeLifetimeContractPlan.md`.

It established six facts beyond the original evidence.

1. **The surface is 76 public entries across 27 headers and 13 bodies**, not the
   nine files and two accessors this section names — 41 in `Text.Json`'s node
   family and 35 in `Xml.Linq`.
2. **SR-AUD-333's severity is understated.** `XObject::getParentProperty`
   dispatches a **virtual call** through the freed parent, so **eight** public
   entry points abort the process (`pure virtual method called`, SIGABRT) with no
   sanitizer present at all. Measured totals: 29 ASan `heap-use-after-free`
   accesses (8 JsonNode, 21 XObject) and 3 ASan `stack-overflow`s.
3. **All 57 faulting accesses are READS.** Under `-fsanitize-recover=address`
   with `halt_on_error=0` the whole matrix produces 57 reads-after-free and
   **zero** writes: the mutating paths (`XNode::Remove`, `ReplaceWith`,
   `XContainer::InsertNodeAt`, `XElement::RemoveAttribute`) enter a non-`const`
   member on freed storage but their searches do not match, so they return before
   their `erase`/`insert`. This record therefore claims a read-after-free on a
   mutating path, not heap corruption.
4. **Twelve cases give a wrong answer with no diagnostic in any build**, including
   two allocator-reuse shapes a sanitizer's quarantine hides: a freed `JsonArray`
   slot refilled by a `JsonObject` at the identical address, after which the
   retained child reports the wrong `GetValueKind`; and a freed `XElement` slot
   refilled by another element, after which the retained text node reports the
   squatter's name.
5. **The two members are structurally asymmetric in a way this section's "the
   implementations differ" understates.** `XObject` deletes copy, move and both
   assignments hierarchy-wide; `JsonNode` deletes none of them, so
   `JsonArray copy = *orig;` compiles, shares the children, and leaves them
   reporting the **original** as their parent — and a slicing `operator=` rewrites
   `parent_` on a node still stored in a container. Insertion also differs
   deliberately and in opposite directions: JsonNode **rejects** an
   already-parented node (matching .NET), Xml.Linq **moves** it (an authorised
   documented deviation from .NET's clone).
6. **Three stack-overflow shapes exist that no CCF-019 text mentions**, all
   reachable from public API and one from **untrusted input**: a 20,000-deep
   `JsonArray` nest and a 20,000-deep `XElement` nest each crash on *release*
   (recursive `shared_ptr` teardown), and `JsonNode::Parse` of 20,000 nested
   arrays crashes during parsing. `XDocument::Parse` is the counter-example
   inside the family — tinyxml2 already rejects deep nesting. The
   ancestor/cycle guards are additionally quadratic in depth.

**Selected repair, both members: the owner detaches what it owns, in its own
destructor** — the same contract #1769 shipped for `LinkedListNode<T>` under this
cause. It closes 27 of the 29 use-after-free accesses at **zero bytes of object
layout, zero vtable slots, zero allocations and zero per-access cost**. Every
representation-changing candidate was rejected with measured evidence, including
the only one that reproduces .NET's contract exactly — a strong parent link —
which leaks by construction (2 constructed, **0 destroyed**, 272 bytes in 3
allocations, LeakSanitizer-confirmed), and the `weak_ptr` candidate, which would
make children of the repository's own 77 automatic-storage containers silently
report **no parent**. Implementation is proposed as #1886–#1894, every one
`needs_user` or `blocked` pending the six explicit approvals in
`docs/OwnedTreeLifetimeContractPlan.md` §31. **CNA and mobile-eggbert were not
inspected**, and #1773 stays `blocked`.

**Remediation status of the two remaining members (tickets #1886 and #1890,
2026-07-31): PARTIAL — the core repair landed, NEITHER FINDING IS REMEDIATED.**
The user approved `docs/OwnedTreeLifetimeContractPlan.md` §31 **item 1 and only
item 1**, which covers both core repairs with one approval. #1886 gives
`JsonArray` and `JsonObject` a destructor that clears the parent link of every
child still owned by that container; #1890 gives `XContainer` one for child nodes
and `XElement` one that additionally clears each owned attribute's parent link
**and** its intrusive `next_` sibling link. Everything above is retained
unchanged; **no new `SR-AUD-*` identifier was issued** and numbering stays frozen
at **364**.

Measured by re-running the #1885 probe **unmodified**, through the same build
script, in the same three builds from one source:

| | #1885 baseline | after #1886 | after #1890 |
|---|---|---|---|
| ASan `heap-use-after-free` **cases** | 29 | 22 | **3** |
| Faulting **accesses**, recoverable ASan | 57 | 49 | **5** |
| `pure virtual method called` process aborts | **8** | 8 | **0** |

**26 of the 29 use-after-free cases are closed**, not the 27 the paragraph above
estimated; the difference is recorded rather than rounded, because §13.5 and
§14.3 of the design record always listed three cases as outside the core repair.
The three that remain are **J11** (stale `JsonArray` iterator → #1889), **X15**
(`Extensions::Ancestors`' `std::vector<XElement*>` → #1892) and **X17**
(`getAttributesProperty()`'s reference → #1892); none reaches the defect through
`parent_`, so no parent-link repair could have closed them. **SR-AUD-327 and
SR-AUD-333 therefore both keep the `confirmed (design-complete)` qualifier**, and
the post-audit tally is unchanged at 57 remediated / 306 confirmed / 364.

Cost, measured on both sides: `sizeof` unchanged for all eleven public types,
GCC's own `-fdump-lang-class` class and vtable dumps identical pre- and post-fix,
zero allocations added to construction, access or destruction, LeakSanitizer
clean, module graph still 41/91. The only ABI movement is three **weak COMDAT**
symbols (`XContainerD0/D1/D2Ev`) that GCC had previously inlined away entirely;
no symbol name was removed. +67 permanent tests (14,568 → 14,635), all 53
existing `JsonNodeTests` and 92 existing Xml.Linq cases unmodified,
mutation-checked five ways. **§31 items 2–6 remain unapproved and unstarted**, so
#1887, #1888, #1889, #1891, #1892 stay `needs_user`, #1893 stays `needs_user` and
#1894 stays `blocked`. **CNA and mobile-eggbert were again not inspected**, and
#1773 stays `blocked`.

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


**Remediation status after the exception-path batch (tickets #1887 and #1891,
2026-07-31): still PARTIAL — the two data-loss paths are closed, NEITHER FINDING
IS REMEDIATED.** Everything above is retained unchanged; **no new `SR-AUD-*`
identifier was issued** and numbering stays frozen at **364**.

The user's batch instruction of 2026-07-31 directed `docs/OwnedTreeLifetimeContractPlan.md`
§31 **item 2** to be started, and granted no approval for a source break, a
vtable change, an object- or iterator-layout change, a return calling-convention
change or downstream migration. **#1887** reorders `JsonObject::SetItem` to adopt
the incoming value before detaching the value it replaces (probe case J10);
**#1891** makes `XNode::ReplaceWith` put the replaced node back when a
replacement is refused, and `XContainer::InsertNodeAt` adopt after it inserts
(probe case X20). Items 3, 4, 5 and 6 remain unanswered.

| | after #1886/#1890 | after #1887/#1891 |
|---|---|---|
| ASan `heap-use-after-free` cases | 3 | **3** (J11, X15, X17) |
| Faulting accesses, recoverable ASan | 5 | **5** |
| `pure virtual method called` aborts | 0 | **0** |
| ASan `stack-overflow`s | 3 | **3** (J19c, X27c, X28c) |
| Timeouts | 2 | **2** (J19d, X27d) |
| **Silent data-loss paths** | **2** (J10, X20) | **0** |

Diffing every answer line of the no-sanitizer probe build across all 58 cases
yields exactly **two** semantic differences in the whole matrix — J10 and X20 —
so nothing else in either family moved. +48 permanent tests
(`JsonNodeMutationConsistencyTests` 22, `XLinqMutationConsistencyTests` 26);
repository gate **14,635 → 14,683 tests across 37 executables**.

Both findings keep the `confirmed (design-complete)` qualifier: **SR-AUD-327**
still has J11 and **SR-AUD-333** still has X15/X17, each an ASan-confirmed
use-after-free inside its own files. The post-audit tally is **unchanged at 57
remediated / 306 confirmed / 364**.

Three premises were corrected by measurement and appended to the design record
rather than rewritten into it: §13.5's claim that assign-before-detach *"matches
.NET"* (it does not — .NET's `JsonObject.SetItem` detaches and commits the write
before `AssignParent` runs, §35.4); §25/§14.3's *"validate every replacement
before removing `this`"* (not implementable — it regresses
`doc->getRootProperty()->ReplaceWith(newRoot)`, §36.4); and §31 item 4's
description of #1889 as *only* a layout change (`begin()`/`end()` return the raw
libstdc++ iterator, so it is also a public source break and a **silent** ABI
break, §39.1). Two further items were found unimplementable as worded — §31 item
5's *"return owning handles"* (§38.1) and item 6's no-layout-change claim for
J19d/X27d (§40.3). **CNA and mobile-eggbert were not inspected**, and #1773 stays
`blocked`.


**Remediation status after the compatible-closure batch (tickets #1895 and
#1898, 2026-07-31): COMPATIBLE REMEDIATION COMPLETE, NOT IMPLEMENTED, NEITHER
FINDING REMEDIATED.** Everything above is retained unchanged; **no new
`SR-AUD-*` identifier was issued** and numbering stays frozen at **364**.

The user decided §31 items 3-6. Items 3 (#1888) and 4 (#1889) are **declined**
and moved `needs_user -> blocked`, designs preserved for a coordinated
ABI-breaking release. Item 5's wording is **rejected as non-implementable** and
replaced by §42: #1898 (done) states and pins the borrowed-view contract, #1899
(blocked) holds the one open question. Item 6 is **split**: #1895 (done, the
approved iterative teardown), #1896 (declined, quadratic guards) and #1897 (one
open question, recursive parse). #1892 and #1893 are retired as superseded.

| | after items 1+2 | after the teardown half |
|---|---|---|
| ASan `heap-use-after-free` cases | 3 | **3** - J11, X15, X17 |
| ASan `stack-overflow`s | 3 | **1** - X28c only |
| Timeouts | 2 | **2** - J19d, X27d |
| Silent data-loss paths | 0 | **0** |

**J11, X15 and X17 are now measured, not argued, not to belong to any compatible
repair**: the teardown changed neither their classification nor their answer
lines, and none reaches its defect through a destruction path. §43 reconciles
every §31 item against ticket, status, probe case, approval and commit.

Five premises were corrected by measurement and appended rather than rewritten:
item 5's "return owning handles" is impossible at any layout cost (§42.2); item 5
conflated the ordinary C++ reference-lifetime contract with a genuine design
defect; item 6's five cases are three defects (§40.1); item 6's depth bound
already exists as `JsonDocumentOptions::DefaultMaxDepth = 64` (§40.2); and item
6's "no layout change" claim is false for its quadratic half (§40.3).

**SR-AUD-327 and SR-AUD-333 both stay `confirmed (design-complete)`** and the
post-audit tally is **unchanged at 57 remediated / 306 confirmed / 364**. **CNA
and mobile-eggbert were not inspected**, and #1773 stays `blocked`.

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

## Post-audit remediation note — ticket #1800, test-only access seams (2026-07-29)

Not a new cross-cutting finding and **no new `SR-AUD-*` identifier**: the
numbering stays frozen at 364 and this was found during remediation, by #1796.
Recorded here because the rule it establishes applies to every module, not only
`Collections`.

A **test-only access seam** — a class template that a production header declares
inside `namespace SharpRuntime::Testing` and never defines, so a consumer cannot
name a complete type — must be **defined in exactly one file**, and every suite
that needs it must include that file. Five translation units of one program had
been defining `SharpRuntime::Testing::CollectionVersionAccess` themselves in two
divergent families, giving three specialisations two token-different definitions
in one program: a one-definition-rule violation, ill-formed with **no diagnostic
required**. Measured consequence: at `-O0` the link order decided which body the
whole program executed (7 against 1007, from a unit that had spelled the correct
body itself); at `-O1` and above the two units disagreed inside one process. `ld`,
`-flto -Wodr`, ASan with `detect_odr_violation=2`, and UBSan all reported
nothing, so **neither `-Wodr` nor a sanitizer may be treated as an ODR check** in
this repository.

`scripts/check_version_seam_odr.py` now enforces the rule in
`scripts/local_ci_check.sh`. It discovers seams rather than hard-coding them, so
a seam added by a future ticket in any module is covered without editing it; both
existing seams — `CollectionVersionAccess` and #1786's `SortedSetVersionAccess` —
are single-sited and pinned. The full analysis, alternatives and evidence are in
`docs/CollectionVersionTestSeamDesign.md`, and the rule is stated in `CLAUDE.md`'s
architecture invariants. **This is one seam family; no broad repository-wide ODR
sweep was performed and none is claimed.**

- `scripts/local_ci_check.sh.audit.md`.

## Post-audit remediation note — ticket #1801, negative consumer fixtures (2026-07-29)

A **negative consumer fixture** — a `test/consumer/*_negative.cpp` whose purpose
is to prove that a spelling a remediation ticket outlawed is *rejected by the
compiler* — must be validated **per marked site**, never by observing that the
whole file failed to compile. Seven such fixtures existed, asserting 36 marked
claims between them, and **no tracked job compiled any of them**: the per-site
logic existed for two of the seven, under the gitignored `build-probe/`, and the
only tracked mention of any fixture anywhere in the build or CI surface was a
docstring.

Measured consequence, reproduced before anything was built: a copy of
`collections_hashtable_value_access_negative.cpp` with **one** of its eleven
marked sites made legal still failed at nine other lines, so a whole-file "the
compiler returned non-zero" check reported **PASS** while one of the eleven claims
had silently become false. The retained gitignored per-site checker caught it
(10 of 11, exit 1). Nothing tracked did. **A non-zero compiler exit status may
therefore not be treated as evidence that a compile-rejection contract holds** in
this repository — one broken line hides every other line.

`scripts/check_negative_consumer_fixtures.py` now enforces the rule in
`scripts/local_ci_check.sh`. Each fixture declares its component and wraps each
negative site in a numbered `#if SHARP_RUNTIME_NEGATIVE_SITE == N` guard carrying
its own expected-diagnostic fragments; the checker compiles the all-sites-off
baseline — which must be **diagnostic-free** — plus each site separately, and
requires every diagnostic located in the fixture to fall inside the enabled guard.
It discovers fixtures rather than hard-coding them and derives include directories
from the repository's own CMake component metadata, so a fixture added by a future
ticket in any component is covered without editing it. Seven fixtures, **37**
sites, all rejected; a 7/7 temporary mutation campaign proves each site is named
when it stops failing. The full analysis, the five compared marker conventions and
the evidence are in `docs/NegativeConsumerFixtureValidation.md`, and the rule is
stated in `CLAUDE.md`'s architecture invariants. **One coverage asymmetry is
recorded rather than closed:** `SortedSetVersionAccess` has no consumer-side
fixture, which is inactive ticket #1803; nothing is known to be wrong with it.

- `scripts/local_ci_check.sh.audit.md`.

---

## Cross-cutting: untracked mutable aliases into collection storage — ticket #1791

This is the same cross-cutting shape as the enumerator `Current` work (#1793,
#1794), the `Hashtable` value-access work (#1796) and the `LinkedListNode` and
`ReadOnlyDictionary::Empty` repairs (#1769, #1780): **a public accessor handed
out a raw reference or pointer into live storage, so a caller could mutate behind
the collection's back and could retain the alias past the mutation that freed
it.**

Ticket #1791 closed the `List<T>` / `IList<T>` instance of it. The non-const
indexer returned a plain `T&`, and no C++ mechanism can notify a container of a
write through a reference it already handed out — so an indexed write was
invisible to the fail-fast guard (where .NET's `List.cs:161-162` advances
`_version` unconditionally), and a retained reference reproduced as
heap-use-after-free in eight distinct shapes. It now returns a tracked
`detail::ElementReference<T>` proxy that reads as `const T&`, publishes no `T*` or
`T&`, and advances the counter on every write. The mutable `ToVector()`, which
handed out the whole backing container and so permitted *structural* mutation the
guard never saw, was removed outright.

**Two limits of that closure are recorded rather than closed**, and they are the
cross-cutting lesson worth carrying forward:

1. **A proxy closes the ordinary surface, not every surface.** `begin()`/`end()`
   still yield a mutable `T&` for STL interop, deliberately, mirroring .NET's own
   `CollectionsMarshal.AsSpan` hatch. Any future claim that a collection has "no
   untracked write path" should be read as "no *ordinary* untracked write path"
   unless the STL-interop surface was also constrained.
2. **A proxy is still an alias if you keep it.** `auto r = list[0];` retained
   across a reallocation is a use-after-free exactly as the old `T&` was. The
   proxy removes the *ordinary* way to retain one; it does not make retention
   safe.

**A third item is cross-cutting and applies to every one of these tickets.**
Changing what an accessor *returns* does not change its mangled name, because a
return type is not part of a C++ mangled name. #1791 measured the consequence:
a stale object file linked against a rebuilt program **with no diagnostic of any
kind**, at `-O0` and `-O2`, in both link orders; it did not crash; it read
correct values; and it *silently reverted to the untracked behaviour*. Every
ticket in this family therefore carries a mandatory-full-rebuild note, and none
of them may rely on the linker to enforce it.

`ObjectModel::Collection<T>` gained a mutation counter (`sizeof` 32 → 40, approved
in advance) and, with it, a fail-fast enumerator — it previously version-checked
nothing at all, not even `Add()`. Its plain indexer still does not run the virtual
`SetItem` hook, because the proxy holds a slot and a counter rather than a
collection and so cannot make a virtual call; `setItem` is the hook-running path.
That gap was **narrowed, not closed**, and is documented at the declaration.

---

## Post-audit remediation note — ticket #1803, a test-only seam needs both halves of its proof (2026-07-29)

Ticket #1801's note above records one coverage asymmetry as open: "`SortedSetVersionAccess`
has no consumer-side fixture, which is inactive ticket #1803". It has one now —
`test/consumer/collections_sorted_set_version_negative.cpp`, 15 sites, every site
rejected — and closing it produced a cross-cutting rule that applies to **every**
test-only access seam this repository will ever add, not only to the two it has.

**A seam's guarantee has two independent halves, and one checker cannot see both.**
Ticket #1800 checks *definition ownership* by reading the repository's own source
text: exactly one file may define a given `(seam, template-argument list)`, and no
`modules/*/include` or `modules/*/src` file may define one at all. A negative
consumer fixture checks *consumer reachability* by compiling: an ordinary consumer,
given only the component's declared public include surface, must be unable to name
a complete seam type or reach the state it exists to reach.

Both checkers were run against **identical** mirror repositories, so the division is
a measurement and not an argument (`build-probe/1803_gap_probe.py`):

| Mutation applied to a copy of `SortedSet.hpp` | `check_version_seam_odr.py` | the consumer fixture |
|---|---|---|
| none | OK, 2 seams, 18 definitions | OK, 15/15 rejected |
| the seam's **primary template** given a body | **OK, exit 0** — silently 1 seam | **FAIL**, 5 sites named |
| an explicit **specialisation** defined | FAIL, rule 1 | FAIL, 5 sites named |
| `SortedSet<T>::state_` made **public** | **OK, exit 0** | **FAIL**, 1 site named |

Row two is the sharp one: #1800's checker *discovers* a seam as a class template
declared and **not defined** in a production header, so giving the primary template
a body makes the seam stop being a seam, rule 1 never fires, and the run exits 0
with its seam count quietly dropping from 2 to 1. Its vacuity guard fires only at
**zero** seams. Row four is the other half — private state becoming public is
entirely outside that checker's remit. **Neither result is a defect in the
repository today**, and #1800 was not reopened; the pair of checks is complete
where each alone is not. Strengthening the vacuity guard so that a seam *leaving*
discovery is reported is inactive ticket **#1804**.

**A second, permanent limitation is recorded rather than closed.** A consumer that
reopens `namespace SharpRuntime::Testing` and writes its own explicit specialisation
of either seam *does* obtain the access the friend declaration grants; it compiles
clean under `-Wall -Wextra -Wpedantic -Werror` against the public headers alone, for
`SortedSetVersionAccess<int>` and for `CollectionVersionAccess<List<int>>` alike.
That is well-formed ISO C++ — a `friend class X;` is open to whoever writes `X` —
so it cannot be expressed as a compile-rejection site, and no seam design in C++
avoids it. It is unsupported, and any future claim that a seam is "unreachable"
must be read as *"no ordinary consumer expression reaches it"*, never as
*"no consumer can construct access to it"*.

The rule for a future ticket adding a seam: give it **one definition file and one
`test/consumer/*_negative.cpp` site set**. `CLAUDE.md`'s architecture invariant now
says so; the evidence is `docs/NegativeConsumerFixtureValidation.md` §18 and
`docs/CollectionVersionTestSeamDesign.md` §14.

- `scripts/check_version_seam_odr.py.audit.md`, `scripts/local_ci_check.sh.audit.md`.
