# Audit: `modules/io/src/System/IO/StreamWriter.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-337 — medium — StreamReader/StreamWriter Close with leaveOpen keeps the wrapper usable

See the paired StreamReader report for the shared direct probe.  `Close()` conditionally closes only the base stream and never marks this writer disposed, so `leaveOpen=true` leaves subsequent writes accepted rather than rejecting use of the closed wrapper.

## SR-AUD-338 — high — StreamWriter accepts a null base stream and dereferences it during Write

The constructor stores `nullptr` without validation.  The ASan/UBSan probe crashes in `StreamWriter::WriteRaw` after `StreamWriter(nullptr, true).Write("x")`; this is an externally reachable null dereference rather than a managed-style construction error.

## Missing assertions and diagnostics

- No post-Close writer test distinguishes wrapper disposal from keeping a base stream open.
- Null StreamWriter construction/write has no regression or diagnostic assertion.
- Flush, destructor, and text encoding failure paths are not checked with a throwing or buffered underlying stream.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

## Post-audit remediation for SR-AUD-338 (ticket #1806, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged, and **SR-AUD-337 is untouched and
stays `confirmed`** — this ticket repaired the null base stream only, not the
`leaveOpen` disposal contract that shares these two files.

Ticket #1806 (`REMED-IO-TEXT-WRAPPER-NULL-STREAM`, P1, size S) rejects a null
`Stream*` in `StreamReader(Stream*, bool)` and `StreamWriter(Stream*, bool)` with
`ArgumentNullException("stream")`, matching .NET, whose every `Stream`-taking
constructor of both types opens with `ArgumentNullException.ThrowIfNull(stream)`,
and matching the sibling `BinaryReader`/`BinaryWriter` in this same module, whose
already-correct behaviour the audit called out as making the divergence especially
hazardous.

**The finding named one dereference. There were five.** Measured before any
production change, one process per case so a crash could not hide another
(`build-probe/1806_prefix_defects.cpp`, log `build-probe/1806_prefix_defects.log`):

| # | Call on a null base stream | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `StreamReader::Read()` | `-1` | `ArgumentNullException` at construction |
| 2 | `StreamReader::Peek()` | `-1` | same |
| 3 | `StreamReader::ReadLine()` | `""` | same |
| 4 | `StreamReader::ReadToEnd()` | `""` | same |
| 5 | `~StreamReader()`, `leaveOpen=false` | survived (guarded) | same |
| 6 | `StreamWriter::Write(std::string)` | UBSan *member access within null pointer of type `struct Stream`* at `StreamWriter.cpp:23`, ASan **SEGV on 0x0**, exit 1 | same |
| 7 | `StreamWriter::Write(const char*)` | same, `StreamWriter.cpp:23` | same |
| 8 | `StreamWriter::Flush()` | same, `StreamWriter.cpp:30` | same |
| 9 | `StreamWriter::Close()`, `leaveOpen=true` | survived (guarded by `leaveOpen`) | same |
| 10 | `StreamWriter::Close()`, `leaveOpen=false` | same crash, `StreamWriter.cpp:37` | same |
| 11 | `~StreamWriter()`, `leaveOpen=false` | same crash, `StreamWriter.cpp:18` | same |
| 12 | `BinaryReader(nullptr)` | `ArgumentNullException` already | unchanged |
| 13 | `BinaryWriter(nullptr)` | `ArgumentNullException` already | unchanged |

Case 11 is the sharpest and was not part of the finding text: with the **default**
`leaveOpen = false`, merely constructing a `StreamWriter` over a null stream and
letting it leave scope was fatal, because the destructor closed a stream it did
not have. No call on the object was required.

The reader's half is not a crash but is arguably worse in one specific way, and
that is why its guards were **removed** rather than kept. `Read()`/`Peek()`
returning `-1` and `ReadLine()`/`ReadToEnd()` returning `""` are exactly what an
empty document returns, so a programming error was silently laundered into
ordinary, plausible data. With the constructor validating, `stream_` is non-null
for the lifetime of every `StreamReader` — the only other constructor assigns a
freshly allocated `FileStream`, and nothing else writes the member — so leaving
the `stream_ == nullptr` tests in place would have left unreachable code implying
a state that can no longer exist. Two permanent tests pin that `-1` and `""` keep
their one remaining legitimate meaning.

Closure evidence: 11 new permanent regressions in `IOStreamTests.cpp` — reader
null, reader null with `leaveOpen=true`, the reader's parameter name, the same
three for the writer, a cross-type assertion that all four `Stream*`-wrapping
types in this module now answer the identical input identically, the reader's
ordinary read paths after the guards were removed, the empty-stream end-of-stream
meanings, the writer's ordinary write path, and a check that a rejected
construction leaves a neighbouring live stream untouched (throwing from the
constructor body means `~StreamWriter()` never runs, so the failure cannot close
or delete anything). `SharpRuntimeTests_IO` **552/552** (was 541), and the same
552 under AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer with zero
reports (`build-asan/1806_io_asan.log`). Repository gate: 0 warnings, 0 errors,
**13,948 tests across 37 executables** (was 13,937). Doxygen 1,941 of the 1,942
ceiling, unchanged.

Source and ABI consequences: none. No public signature, object layout, vtable or
exported symbol changed. One behavioural note for consumers: code that constructed
either wrapper over a null stream and relied on the reader's silent end-of-stream
now receives `ArgumentNullException` at construction. No in-repository caller did
so, and no test asserted the old behaviour.

Two separable defects were found in these files while doing this work and are
recorded as inactive tickets rather than folded in:

- **#1808** — neither text wrapper validates `CanRead`/`CanWrite`. .NET's
  `StreamReader.cs:147` and `StreamWriter.cs:103` throw
  `ArgumentException(SR.Argument_StreamNotReadable / _StreamNotWritable)`; this
  port accepts a read-only stream into a `StreamWriter` and fails later, if at
  all. Kept separate because rejecting a stream that is merely unsuitable is a
  different contract from rejecting one that does not exist, and it can break
  existing callers.
- **#1809** — `TextWriter::Write(const char*)` forms `std::string(value)` and
  `StreamWriter::Write(const char*)` calls `std::strlen(value)`, both undefined
  for a null pointer, across the whole `TextWriter` family rather than these two
  files. `.NET`'s `TextWriter.Write(string?)` treats null as a no-op, so the
  correct answer here is a contract decision, not a mechanical guard.

Neither carries a new `SR-AUD-*` identifier; the audit numbering stays frozen
at 364.

## Follow-up #1809 closed (2026-07-29): the null `const char*` contract

The two separable defects recorded above under #1806 are no longer both open.
**#1809 is complete.** `SR-AUD-337` and `SR-AUD-338` are unaffected: 337 stays
`confirmed`, 338 stays `remediated`, and no new `SR-AUD-*` identifier was issued —
the numbering stays frozen at 364.

Design ticket **#1823** decided the contract for the whole family first
(`docs/TextWrapperInputContractPlan.md`), because #1809 was a contract decision
rather than a guard. The measurement it did found **three** structurally different
current failures where the ticket named two, and the unnamed one is the worst
(`build-probe/1823_prefix_defects.log`, cases 20–27):

| Surface | Before | After |
|---|---|---|
| `TextWriter::Write/WriteLine(const char*)` on `StringWriter` | libstdc++ `std::logic_error` — a `std::` exception, not a `System::` one, so `catch (const System::Exception&)` missed it | no-op / line terminator only |
| `StreamWriter::Write/WriteLine(const char*)` | AddressSanitizer **SEGV on `0x0`** inside `strlen` | no-op / line terminator only |
| `Console::Write/WriteLine(const char*)` | `badbit` set on `std::cout` **permanently**, silently disabling every later console write in the process | no-op / line terminator only, `std::cout` still `good()` |

The contract is .NET's own rule for a null **string**, since these `const char*`
overloads have no .NET counterpart and exist only so a string literal binds to the
string overload instead of `Write(bool)`: `TextWriter.cs:277-283` makes
`Write(string?)` a no-op, `TextWriter.cs:502-509` makes `WriteLine(string?)` write
only the line terminator, and neither ever throws. `StreamWriter` needed its own
test because it is the one override of the base overload — the base guard alone is
bypassed by virtual dispatch, including through `TextWriter::WriteLine`.

Closure evidence: 14 permanent regressions (10 in `IOStreamTests.cpp`, 4 in
`Batch10ConsoleTests.cpp`) covering null, empty and ordinary input on each of the
five surfaces, the terminator-only `WriteLine` rule, a cross-type assertion that
`StringWriter` and `StreamWriter` answer null identically, an inertness check that a
null write between two ordinary writes leaves them contiguous, and three `Console`
cases asserting `std::cout.good()` afterwards. `SharpRuntimeTests_IO` **562/562**
(was 552) and `SharpRuntimeTests_Console` **127/127** (was 123), both clean under
AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer
(`build-asan/1809_io_asan.log`, `build-asan/1809_console_asan.log`; activation
proven in `build-asan/1809_asan_activation.log` by the runtime's own
`ASAN_OPTIONS=help=1` banner and 53 `__asan`/`__ubsan` symbols). Repository gate:
0 warnings, 0 errors, **14,060 tests across 37 executables** (was 14,046).

Source and ABI consequences: none. No public signature, virtual, vtable, object
layout or exported symbol changed. `TextWriter`'s two overloads are `inline` in a
public header, so a consumer that does not rebuild keeps the old inlined body —
a stale-inline consideration, not an ABI break, and benign in this direction
because the old body only ever failed on the input the new one handles.

The other follow-up, **#1808**, is still open and was rescoped by #1823 to the
`StreamReader` half; its `StreamWriter` half is blocked ticket **#1824**.
