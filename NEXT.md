# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-13 (branch: `feature/work`, HEAD `1e2b582`) — 11768 tests passing. Verified via:*
```
cmake --build build --parallel 8          # Debug, default config — 0 errors/0 warnings
./build/SharpRuntimeTests                 # 11768 tests from 1197 test suites, 0 failures
```

## Session checkpoint (2026-07-13, autonomous run continuing) — tickets 305-309 closed, four more real bugs (ticket 306 does not exist in plan.sqlite3, skipped)

Continuing the same autonomous run (previous checkpoint covered 299-304). Commits: 43834df,
01f3c40, 75c0cdf, 1e2b582 — all pushed to `origin/feature/work`.

- **305 (ReadOnlySequence.hpp)**: `GetPosition(offset, origin)` narrowed a `longcs` offset to
  `intcs` BEFORE bounds-checking it, so a huge offset (e.g. 2^32 + 2) silently truncated to a
  small in-range value and returned a bogus position with no throw at all — confirmed via a
  standalone repro. The subsequent addition, and `Slice(longcs, longcs)`'s direct
  `start + length`, were also genuine UBSan-confirmed signed-overflow UB for values near
  `LONGCS_MAX`. Rewrote both to validate/combine via subtraction-based checks in `longcs` before
  ever adding or narrowing — the same overflow-bypasses-bounds-check pattern fixed dozens of
  times earlier this session, just newly found in the Buffers namespace.
- **307 (MathF.hpp)**: two independent bugs. `Round(float, int[, MidpointRounding])` never
  validated `digits` (real .NET throws for values outside [0,6]) and never guarded large
  magnitudes (real .NET returns `x` unchanged for `|x| >= 1e8` instead of multiplying by a power
  of 10) — confirmed `Round(3.0e38f, 6)` silently overflowed to `inf`. Separately, `ILogB(NaN)`
  delegated to `std::ilogb`, whose NaN sentinel (`FP_ILOGBNAN`) is implementation-defined and on
  this platform's glibc is `INT_MIN`, not the `INT_MAX` real .NET always returns for both NaN and
  infinity — confirmed via repro. Both fixed to match `MathF.cs` exactly.
- **308 (TimeZoneInfo.cpp)**: a genuine data race, confirmed with ThreadSanitizer both before and
  after the fix (first TSan use this session — the established habit of UBSan/ASan repro-before/
  repro-after extends naturally to concurrency bugs). `FindSystemTimeZoneById()` temporarily
  overwrites the process-global `TZ` env var, already guarded by a mutex against concurrent calls
  to itself — but `Local()` reads the same global TZ state via `localtime_r` without taking that
  mutex, so a thread calling `Local()` while another thread is mid-swap could transiently observe
  the wrong zone. Unlike the `Guid::NewGuid()` RNG race documented-but-not-fixed in ticket 302
  (no synchronization convention existed anywhere in that file), this fix was narrowly scoped and
  directly consistent with an *existing* local precedent (the mutex already existed for exactly
  this purpose) — extending established local protection is different from inventing a new
  cross-cutting policy.
- **309 (ZipArchive.cpp)**: two bugs in the same function. The write-mode entry stream's
  `Write(data, offset, count)` used `offset` completely unchecked in pointer arithmetic —
  confirmed via ASan as a genuine stack-buffer-overflow read for a negative offset. Separately,
  `flushWriter()`'s `mz_zip_writer_add_mem`/`finalize_archive`/`finalize_heap_archive` calls never
  checked their `mz_bool` return values, so a write failure silently produced a truncated zip
  with no exception — inconsistent with this exact same function's own `init_file`/`init_heap`
  checks two lines above. Not empirically regression-tested (no portable way found to force an
  actual miniz failure), verified by code review/consistency with the established local pattern
  instead.

40 tickets closed this autonomous run so far (268-309, minus 279 and 306 [never existed in
plan.sqlite3 — ticket numbering has a gap there, not a skipped/forgotten item]). Every ticket in
this five-ticket stretch found and fixed at least one real bug — the "large audited file, several
prior fix rounds" files are *still* not running dry. Two process notes worth carrying forward:
(1) **UBSan/ASan repro-before/repro-after now formally extends to TSan** for anything touching
shared mutable state across threads (env vars, static caches, lazily-initialized singletons) —
first applied this stretch (ticket 308), caught a real race a pure code-read might have missed
given how innocuous `Local()` looks in isolation. (2) **"Does this file already have an
established local convention for this exact problem?" is the right question when deciding
whether a scoped fix vs. a broader policy decision is warranted** — tickets 302 (Guid RNG, no
convention → documented, not fixed) and 308 (TimeZoneInfo mutex, convention already existed →
fixed) are a clean side-by-side contrast of the same underlying judgment call going both ways for
principled reasons, not inconsistency.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`.

## Session checkpoint (2026-07-13, autonomous run continuing) — tickets 299-304 closed, six real bugs found and fixed

Continuing the same autonomous run (previous checkpoint covered 296-298, three clean audits in a
row). This stretch reversed that trend hard — six consecutive tickets, six confirmed real bugs,
zero clean audits. Commits: 1db58c0, 55ed2c2, 60166fd, 41e6dc8, 8f52a89, b005a46 — all pushed to
`origin/feature/work`.

- **299 (ArraySegment.hpp)**: `CopyTo(vector&, destinationIndex)` never validated
  `destinationIndex >= 0`. A negative index (e.g. `-1`) bypassed the resize-capacity check
  entirely (since `needed = destinationIndex + count_` could still land inside the existing
  destination size) and then `destination.begin() + destinationIndex` computed an out-of-bounds
  iterator — confirmed via UBSan/ASan repro as a genuine heap-buffer-overflow write, not just a
  wrong-exception-type issue. Also widened the `needed` computation to `longcs` to avoid signed
  `intcs` overflow for a huge `destinationIndex`. Checked the `Span`/`Memory` family for the same
  shape — none of them expose a `destinationIndex` parameter on `CopyTo`, so it doesn't recur.
- **300 (ImmutableList.hpp)**: `RemoveRange`'s bounds-violation branch threw
  `System::ArgumentException` where real .NET's `Requires.Range` always throws
  `ArgumentOutOfRangeException`. Rewrote to mirror both of .NET's exact `Requires.Range`
  conditions (including the `index == size, count == 0` boundary case).
- **301 (ImmutableArray.hpp)**: `SetItem`/`RemoveAt` both threw `IndexOutOfRangeException` —
  copied from the raw indexer's exception type by mistake. Real .NET's `SetItem`/`RemoveAt`
  validate via `Requires.Range` → `ArgumentOutOfRangeException`; only the *raw* indexer
  (`operator[]`) is an intentionally-unchecked array-access wrapper that legitimately throws
  `IndexOutOfRangeException` (confirmed against `ImmutableArray_1.Minimal.cs`'s own comment
  explaining the perf-motivated lack of a check there).
- **302 (Guid.hpp/.cpp)**: the "X" format's hex-component overflow check
  (`TryParseHexRun`) only set the overflow flag in the success path. Hitting an invalid
  character mid-component returned `false` immediately without ever checking whether 9+
  significant digits had already been consumed — so a component like `"123456789z"` threw
  `FormatException` where real .NET's `TryParseHex` (which tracks `processedDigits` as it scans
  and checks the threshold *before* reporting failure) throws `OverflowException`. Fixed by
  tracking digits incrementally and checking the threshold on the invalid-char path too, plus
  fixing the caller to consult the overflow flag even when the parse returned false. Also
  documented (not fixed) that `Guid::NewGuid()`'s static RNG has no synchronization — a real
  data race under concurrent calls, but this codebase has no established thread-safety policy
  anywhere, so a one-off mutex here would be an inconsistent unilateral fix; flagged for a future
  dedicated ticket instead.
- **303 (BitVector32.hpp)**: `Section`'s 2-arg constructor was **public** (real .NET's is
  `internal`), and its `mask_`/`offset_` fields had no `private:` label either. This let any
  client code construct a `Section` with an arbitrary out-of-range offset (e.g. `Section(1,
  100)`), completely bypassing `CreateSection`'s `offset >= 32` validation — `operator[]`/`set()`
  would then shift a 32-bit value by an amount ≥ 32, confirmed as real UB via UBSan
  ("shift exponent 100 is too large"). Fixed by making the constructor and fields private with
  `friend struct BitVector32;`, matching .NET's actual encapsulation exactly — the exploit no
  longer even compiles. Three existing tests that used the constructor directly were rewritten to
  build equivalent sections via `CreateSection` chaining.
- **304 (XmlDocument.cpp)**: `CreateWhitespace`/`CreateSignificantWhitespace` never validated
  that their content is actually whitespace-only. Real .NET's `XmlWhitespace`/
  `XmlSignificantWhiteSpace` constructors both call `CheckOnData` (= `XmlCharType.IsOnlyWhitespace`,
  exactly tab/LF/CR/space) and throw `ArgumentException` otherwise — the port called tinyxml2's
  `NewText` directly with no check, so a node typed `Whitespace` could silently hold arbitrary
  text. Checked sibling factories (`CreateComment`/`CreateCDataSection`/
  `CreateProcessingInstruction`/`CreateDocumentType`) against their real .NET constructors too —
  all confirmed to already match (no validation needed, or already present from the prior
  08d9318 fix), so this was the one gap left in the file.

36 tickets closed this autonomous run so far (268-304, minus 279). This six-ticket stretch is a
reminder that "large file, already fixed several times" (298: 3 prior rounds; 300/301/303: each
had 1-3 prior fix commits) does not mean "no bugs left" — every one of these files had passed
prior review rounds focused on different bug shapes (overflow, UB, API completeness) and still
had a live, confirmed bug in a shape not previously checked (wrong exception *type* specifically,
in four of the six cases). **Exception-type parity against the real .NET reference source is
worth checking explicitly and separately from "does it throw at all" — three of six bugs this
stretch (300, 301, and half of 302) were the right behavior (throws) with the wrong exception
type**, which existing tests using bare `EXPECT_THROW(..., System::Exception)`-style broad
catches would never have caught, but `EXPECT_THROW(..., SpecificExceptionType)` does.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`.

## Session checkpoint (2026-07-13, autonomous run continuing) — tickets 296-298 closed, all clean

Continuing the same autonomous run (previous checkpoint covered the ticket-295 follow-up). No
new commits this stretch — all three were clean audits with no code changes needed:

- **296 (XPathNavigator.cpp)**: fetched real .NET's `ComparePosition`/`CompareSiblings` source
  directly and compared line-by-line — both are faithful, structurally identical ports. The
  custom `ApplySortKeys` multi-key stable-sort (not a direct .NET port) uses the standard,
  correct reverse-priority-order technique.
- **297 (OrderedDictionary.hpp)**: already had 3 fix rounds. Specifically chased down whether
  `RemoveAt(index) => Remove(storage_->keys[index])` is a use-after-free (the argument is a
  reference into the vector that `Remove` then erases) — traced every use of the parameter and
  confirmed it's never dereferenced after the erase, so it's safe despite looking fragile. No
  `GetEnumerator()` exists on this type, but that's a completeness gap, not a bug.
- **298 (XmlNode.cpp)**: already had 3 fix rounds (ancestor-cycle guards, exact-markup
  InnerXml/OuterXml, recursive Normalize). Traced the `XmlDocumentFragment` insertion loops in
  `PrependChild`/`AppendChild` through a concrete example to confirm ordering is preserved (not
  reversed) despite removing-while-iterating.

30 tickets closed this autonomous run so far (268-298, minus 279). Zero regressions at any
point. Three consecutive clean audits is itself informative — this batch of files had already
absorbed the "easy" bugs in prior rounds, consistent with how thoroughly this codebase has been
worked over.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`.

## Session checkpoint (2026-07-12, autonomous run continuing) — follow-up to ticket 295

Continuing the same autonomous run. Commit: 88b0ded. Self-initiated follow-up (per the previous
checkpoint's own "worth a quick pass on UInt32" note) found two more systemic `Parse` gaps while
checking UInt32:

1. `SByte`/`UInt16`/`UInt32::Parse` all called `stoi`/`stoul(s)` without capturing the parse-end
   position — trailing garbage (`"5abc"`) silently accepted as `5` instead of throwing.
2. `Byte`/`UInt16`/`UInt32::Parse` all silently accepted `"-0"` as `0` instead of throwing
   `OverflowException` — verified against real .NET's `Number.Parsing.cs` that unsigned types
   reject *any* leading `-`, even an all-zero one, regardless of magnitude. For `UInt16`/`UInt32`
   this also meant a plain `"-1"` wasn't reliably caught on an LLP64 platform (`unsigned long`
   same width as `UInt32` → `stoul("-1")` wraps to exactly `MaxValue`, which `v > MaxValue`
   — not `>=` — doesn't catch).

Fixed all instances, matching `UInt64::Parse`'s already-correct pattern. 9 new regression tests.
One self-correction along the way: initially assumed `Byte::Parse` also lacked trailing-garbage
validation (copying the pattern from the actually-broken siblings) — it didn't, an existing test
already covered it; caught via a duplicate-test-name compile error, removed the redundant test.

This is now the **fifth** systemic-bug-family this session (after `DivRem` zero-check,
calendar `years*12`/`weeks*7` overflow, `ToString` `stoi` leak, and now `Parse` validation gaps)
— all found by grepping a sibling family the moment a second instance turned up.

29 tickets closed this autonomous run so far (268-295, minus 279). Zero regressions at any
point; full suite run after every change.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. The primitive-integer-type family (Byte/SByte/Int16/UInt16/Int32/UInt32/Int64/UInt64)
should now be fully covered for the `DivRem`-zero-check, `ToString`-`stoi`-leak, and
`Parse`-trailing-garbage/negative-zero bug shapes — no further sweep needed there barring a new
pattern surfacing.

## Session checkpoint (2026-07-12, autonomous run continuing) — ticket 295 closed, fourth systemic find this run

Continuing the same autonomous run (previous checkpoint covered 292-294). Commit: 1ed69e2 —
pushed to `origin/feature/work`.

- **295 (Byte.hpp)**: found the exact "unguarded `std::stoi(format.substr(1))` raw exception
  leak" already fixed once this session in `Int32::ToString` (ticket 272). Grepped every sibling
  integer type and found it systemic: **Int16, Int64, SByte, UInt16, UInt64 all had the identical
  bug** (`UInt32` has no `ToString(value, format)` overload at all — a gap, not a bug, left
  alone). Fixed all six with the same try/catch-and-rethrow-as-`FormatException` pattern.

This is now the **fourth** time this session a bug found in one file turned out to be systemic
across a whole family once grepped (after the `DivRem` zero-check, the `years*12`/`weeks*7`
calendar overflow, and now this). The pattern holds: **the moment a bug is confirmed in a second
independent file, grep the whole relevant family immediately** — it has had a 100% hit rate this
session for finding more real instances.

28 tickets closed this autonomous run so far (268-295, minus 279 which needed no code changes).
Zero regressions at any point; full suite run after every single change.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. Given how consistently the "grep the sibling family" instinct has paid off, when
auditing any remaining integer-type file (UInt32 was the only one of 8 primitive integer types
*not* touched by tickets 272/295 — its `ToString`/`Parse`/`DivRem` etc. haven't been
specifically re-verified this session, only confirmed to lack the `ToString(format)` overload
entirely) it's worth a quick pass for the same two bug shapes (unguarded `stoi`, missing
zero-divisor check) even before its own audit ticket comes up.

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 292-294 closed, systemic calendar overflow fix

Continuing the same autonomous run (previous checkpoint covered 291). Commits: 5cddb18
(checkpoint), a655f50 — pushed to `origin/feature/work`.

- **292 (CodeAnalysisAttributes.hpp)**: clean audit — pure marker-attribute classes with no
  computational logic (matches CLAUDE.md's reflection-out-of-scope deviation; nothing in this
  codebase reads these attributes at runtime). Checked for constructor-parameter copy-paste
  bugs between near-duplicate classes, found none.
- **293 (AppDomain.hpp/.cpp)**: clean audit. Noted but didn't fix a real, very-low-probability
  edge case (none of the three platform-specific executable-path lookups retry with a larger
  buffer if the path exceeds the fixed stack buffer) — judged not worth the added complexity
  given PATH_MAX/MAX_PATH sizes vastly exceed realistic path lengths.
- **294 (PersianCalendar.hpp) — became the largest single-ticket fix of the whole run.** Started
  as one `PersianCalendar::AddYears` fix (same `years*12` bug class as `DateTimeOffset::AddYears`,
  ticket 276) — but since this was now a *second* independent instance, grepped every calendar
  file for the same pattern and found it **systemic across the entire calendar subsystem**,
  including the `Calendar` base class itself. Fixed 7 confirmed instances in one pass, each
  verified with a standalone UBSan/ASan repro:
  - `Calendar::AddYears`/`AddWeeks` (base — inherited by every calendar subclass that doesn't
    override them, e.g. GregorianCalendar/KoreanCalendar/TaiwanCalendar/ThaiBuddhistCalendar/
    JapaneseCalendar)
  - `JulianCalendar::AddMonths` — a virtual override with **no bounds check at all**, unlike the
    base method it replaces (virtual dispatch means overrides don't inherit the base's checks)
  - `JulianCalendar::AddYears`
  - `HebrewCalendar::AddMonths`/`AddYears` — real .NET protects the equivalent arithmetic with a
    try/catch this port doesn't have
  - `HijriCalendar::AddYears` and `UmAlQuraCalendar::AddYears` — their sibling `AddMonths` already
    had the check from an earlier round, `AddYears` was missed
  - Final sweep: 12 previously-UB call patterns across all 6 affected types, all now throw
    cleanly with zero sanitizer output.

**Process lesson reinforced a third time this session** (after Utf8Parser's overflow idiom and
BitArray's constructor): the moment a bug pattern is confirmed in a *second* independent file,
grep the whole relevant subsystem immediately rather than waiting for each file's own audit
ticket — this is now consistently where this session's highest-value fixes have come from.
**Also worth noting**: `JulianCalendar::AddMonths`'s missing check specifically illustrates why
virtual overrides need their own validation — a base class's bounds check does NOT protect a
derived class's override, since the override *replaces* the base's method body entirely.

27 tickets closed this autonomous run so far (268-294, minus 279 which needed no code changes).
Zero regressions at any point; full suite run after every single change.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. If any other calendar-adjacent file surfaces in the queue (RegionInfo, CultureInfo,
CalendarWeekRule, etc.), it's worth a quick check for the same `X * Y` unguarded-multiplication-
before-bounds-check shape, though the calendar subsystem itself should now be fully covered.

## Session checkpoint (2026-07-12, autonomous run continuing) — ticket 291 closed

Continuing the same autonomous run (previous checkpoint covered 288-290). Commit: 5cddb18 —
pushed to `origin/feature/work`. Ticket queue status: 819 done, 569 todo, 100 blocked (ticket
#43 and its dependents).

- **291 (OperatingSystem.hpp)**: `getVersionStringProperty()` only handled 3 of 8 `PlatformID`
  enum values, falling through to generic "Unknown " for the rest. `PlatformID::Other` is not
  dead code in this codebase — `Environment::getOSVersionProperty()` (ticket 273) constructs it
  for Emscripten builds — so this was a real, reachable divergence (Emscripten's
  `OperatingSystem::ToString()` said "Unknown 0.0" instead of real .NET's "Other 0.0"). Added
  all missing cases matching real .NET exactly, including the version-dependent Win32Windows
  "95" vs "98" text.

24 tickets closed this autonomous run so far (268-291, minus 279 which needed no code changes).
Zero regressions at any point; full suite run after every single change; every fix pushed
individually with its own commit plus a NEXT.md checkpoint every few tickets. Continuing to the
next `todo` ticket per the standing instruction to work without stopping to summarize/ask.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked` per explicit user decision — do not reopen without being asked again.

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 288-290 closed, one severe finding

Continuing the same autonomous run (previous checkpoint covered 286-287). Commits: ba27d3d,
2bdd68d — pushed to `origin/feature/work`.

- **288 (UnicodeRanges.hpp)**: clean audit via scripted diff (160 block entries + All/None
  specials, 0 mismatches) — same approach as ticket 284's TlsCipherSuite, the right call for a
  pure data file.
- **289 (BitArray.hpp) — the most severe bug found this entire session.** `BitArray(intcs
  length, bool defaultValue)` had no `ArgumentOutOfRangeException.ThrowIfNegative` check (real
  .NET has one). This isn't just "wrong exception type" — confirmed via a standalone ASan repro
  that `BitArray(-1)` is a **genuine, trivially-triggerable heap-buffer-overflow**:
  `std::vector<bool>`'s internal bit-to-word-count calculation overflows for a
  `size_t(-1)`-scale request, silently under-allocating while `size()` still reports
  `SIZE_MAX`. The very first element write (`bits[0] = true`) corrupts adjacent heap memory —
  confirmed via ASan's "heap-buffer-overflow... WRITE of size 1". Notably, this exact file
  already had a dedicated prior fix round for "raw std:: exceptions" (ffb887f) that evidently
  didn't test the constructor specifically. Fixed with the missing bounds check.
- **290 (XxHash3Shared.cpp)**: no bug found, but a valuable process step — rather than manually
  reading 336 lines of dense streaming-buffer state-machine code (buffer completion, per-block
  consumption, multi-block loops, tail-stripe buffering), added randomized differential tests
  (streaming vs. one-shot digest, 30 lengths spanning every structural boundary × 5 random
  chunk-split trials × seeded/unseeded × XxHash3/XxHash128 = 600 assertions, all passing). This
  is a template worth reusing: for any algorithm-critical file where "does the output match a
  reference" is checkable but the *code* is bit-twiddling-dense, a targeted randomized
  differential/property test finds real bugs (or gives strong assurance of correctness) far
  faster and more reliably than manual line tracing.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. 23 tickets closed this autonomous run so far (268-290, minus 279 which needed no code
changes). Zero regressions at any point; full suite run after every single change. Given the
severity of ticket 289's finding, if auditing any other class with a `ClassName(intcs length,
...)`-shaped constructor that hands `length` to a container without an upfront non-negative
check, treat it as a priority check — grepped once already
(`explicit.*(intcs.*length`/`explicit.*(SharpRuntime::intcs.*length`) and found no other exact
matches, but that grep was narrow (exact parameter name/explicit-keyword match) and worth
re-running with a broader pattern if time allows.

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 286-287 closed

Continuing the same autonomous run (previous checkpoint covered 282-285). Commits: d0d5668,
33bd573 — pushed to `origin/feature/work`.

- **286 (ContentDispositionHeaderValue.cpp)**: `tryDecode5987` (RFC 5987 `filename*` decoder)
  only checked for *at least* 2 single quotes, not *exactly* 2 — real .NET's `TryDecode5987`
  explicitly rejects a third quote (`quoteIndex == lastQuoteIndex || IndexOf('\'', quoteIndex+1)
  != lastQuoteIndex`). A well-formed encoder must percent-encode any literal apostrophe in the
  value, so a raw third quote reliably signals malformed input; this port silently decoded
  everything after the *second* quote instead of rejecting it.
- **287 (Random.hpp/.cpp)**: the standout finding this stretch. `Shuffle(std::vector<T>&)` used
  a completely different algorithm than `Shuffle(Span<T>)` — top-down classic Fisher-Yates
  (`i: n-1→1, j=Next(i+1)`) vs. real .NET's actual bottom-up loop (`i: 0→n-2, j=Next(i,n)`).
  Real .NET's `Shuffle(T[])` **delegates entirely** to `Shuffle(Span<T>)` — there's only one
  algorithm in the reference — so this port's independent vector implementation produced a
  genuinely different permutation for the same seed, a direct violation of this exact file's own
  carefully-documented seeded-determinism guarantee (the file had a prior fix round specifically
  verifying byte-for-byte parity with live Mono reference output for `Next`/`NextDouble`/
  `NextBytes`). Fixed by making the vector overload delegate to the span overload, matching real
  .NET exactly. **Process note**: this is the kind of bug that pure code review or "does it
  compile and pass existing tests" would never catch — the old vector `Shuffle` was internally
  self-consistent and its own tests (`Shuffle_ChangesOrder`) passed fine — it just diverged
  silently from its sibling overload and from real .NET. Whenever a class has multiple overloads
  of what's conceptually "the same operation" (array vs. span, sync vs. async, etc.), it's worth
  explicitly asking "do these two call the same underlying algorithm, or did someone
  reimplement it twice?"

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. 20 tickets closed so far this autonomous run (268-287, minus 279 which needed no
code changes): 268-277 (Half/Array/Matrix4x4/DateTimeFormatInfo/Int32/DivRem-systemic/
Environment/TimeOnly/DateTime/DateTimeOffset/HebrewCalendar), 278-287 (BinaryData/Ping-
investigated-and-reverted/Utf8Parser/LinkedList-clean/BigInteger/TlsCipherSuite-clean/
IPAddress/ContentDispositionHeaderValue/Random). All pushed to `origin/feature/work`, zero
regressions at any point, full suite run after every single change.

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 282-285 closed

Continuing the same autonomous run (previous checkpoint covered 278-281). Test count: 11707 →
11708 (+1 regression test; two of these four tickets were clean audits or defensive-only fixes
with no new test needed). Commits: 14eb9b8, efedd98 — all pushed to `origin/feature/work`.

- **282 (LinkedList.hpp)**: clean audit, no code changes. CopyTo's start+length check (the
  pattern that's been a real bug repeatedly this session) isn't actually exploitable here — size
  would need to approach SIZE_MAX to overflow when added to an int32-bounded index. Flagged but
  deliberately did NOT fix a real, systemic issue: `Enumerator::Current()` dereferences the
  underlying iterator with no guard against being called before `MoveNext()`/after enumeration
  ends (real UB in C++, vs. real .NET's memory-safe-but-logically-undefined equivalent) —
  confirmed via `List.hpp`'s `Enumerator::Current()` that this is consistent across every
  `IEnumerator<T>` implementation in the codebase, not a LinkedList-specific oversight, so fixing
  it here alone would be an inconsistent partial fix. Left as a documented finding for a future
  ticket covering the whole `IEnumerator<T>` family.
- **283 (BigInteger.cpp)**: `BigInteger(longcs v)`'s constructor negated `v` directly for
  negative values — UB for `v == LONGCS_MIN`, confirmed via UBSan. The sibling `intcs`
  constructor already avoided this correctly (widens to int64 before negating); there's no wider
  type to widen into for the `longcs` overload. Fixed with well-defined unsigned subtraction
  (`0ULL - (uint64_t)v`) instead. This is the second instance this session of "negate the widest
  signed integer type without a MinValue guard" (after `Int32::CopySign`, ticket 272) —
  grepped for the same pattern afterward and found the other two matches already safe.
- **284 (TlsCipherSuite.hpp)**: clean audit via scripted diff (337 enum values, 0
  missing/extra/mismatched) rather than manual reading — the right call for a pure data enum.
- **285 (IPAddress.cpp)**: `IsLoopback` had the *exact* static-initialization-order hazard
  already found and fixed for `DateTimeOffset::MinValue/MaxValue/UnixEpoch` — referencing another
  class-level static (`Loopback`/`IPv6Loopback`) from a function that could itself run during a
  *different* translation unit's dynamic initialization. Fixed with function-local statics (safe,
  lazy, thread-safe since C++11) and a literal constant, matching the DateTimeOffset fix's
  approach exactly. Not empirically triggered in this session (no test exercises the pathological
  cross-TU call order), but cheap, safe, and textbook — same reasoning the codebase already
  applied once.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. Two standing process notes reinforced this stretch: (1) when a class exposes several
static const singleton instances (Zero/Empty/MinValue/Loopback-style), check whether any static
*method* in the same file reads one of those singletons — if so it's worth a 30-second SIOF
check, this has now been a real, confirmed bug twice in this codebase. (2) `Enumerator::Current()`
being unguarded before `MoveNext()`/after end is systemic across every `IEnumerator<T>` in this
codebase — worth a dedicated cross-cutting ticket rather than fixing one implementation at a
time inconsistently.

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 278-281 closed, one reverted dead-end worth reading

Continuing the same autonomous run (previous checkpoint covered 274-277). Test count: 11701 →
11707 (6 net new regression tests). Commits: e4ecdc6, 7eda594 (already in prior checkpoint),
0d74a07 — all pushed to `origin/feature/work`. Ticket 279 (`src/System/DateTime.cpp`) turned out
to be the same file already fully covered by ticket 275's audit — closed with zero code changes,
cross-referencing 275's notes.

**3 tickets closed with real fixes: 278 (BinaryData.hpp), 280 (Ping.cpp — investigated and
reverted, see below), 281 (Utf8Parser.hpp).**

- **BinaryData::operator[]** (a C++-only addition, real .NET's BinaryData has no indexer) had
  zero bounds checking — confirmed a real ASan heap-buffer-overflow for `bd[-1]`. Added the
  same unsigned-comparison bounds check this project's other array-like accessors use.
- **Utf8Parser's overflow-check idiom was not airtight.** `tryParseUInt`/`tryParseGrouped` both
  used the common `next = v*10+digit; if (next < v) overflow` post-multiply check. Brute-forced
  against `__uint128_t` ground truth (curated cases + 200k randomized trials) and found it
  **falsely accepts** some genuinely-overflowing 21-digit inputs whose true value wraps around
  2⁶⁴ more than once and lands back above the previous accumulator by coincidence — e.g.
  `"184467440737095516159"` (`UINT64_MAX*10+9`) silently parsed as a wrapped, wrong value
  instead of failing. Fixed with the standard check-*before*-multiply idiom
  (`v > (UINT64_MAX-digit)/10`), re-verified against the same harness with zero false
  accepts/rejects. **Worth remembering**: this exact idiom flaw is subtle enough that it's easy
  to write and easy to review-past — a `*10+digit` accumulator's overflow check is only airtight
  if it checks *before* multiplying, not after. Grepped the rest of the codebase for the same
  literal pattern afterward (`Int128.hpp`, `StandardFormat.hpp`, `TimeSpan.cpp`, `Decimal.cpp`
  all have `*10+digit` accumulators) — all four already use the correct pre-check idiom or are
  bounded by a small fixed digit count, so this was an isolated instance, not systemic.

**Ticket 280 (Ping.cpp) — a fix that looked right, tested wrong, and was reverted.** Real .NET's
Unix `Ping.RawSocket.cs` validates a received echo reply's ICMP identifier against the request
and loops (discarding mismatches) until a match or timeout — this port's `sendPingCore` does a
single `recv()` with no identifier check, which looks like a real "could misattribute an
unrelated ICMP packet as this call's reply" bug. Implemented the equivalent retry loop — and the
**live loopback ping tests** (`PingTests.cpp`, which hit a real ICMP socket, not a mock)
immediately failed with 5-second timeouts across the board. Root cause: real .NET's check exists
because it uses `SOCK_RAW` (receives *all* ICMP system-wide, no kernel filtering). This port
deliberately uses the unprivileged `SOCK_DGRAM`+`IPPROTO_ICMP` Linux "ping socket" instead
(documented in `Ping.hpp`'s own note, specifically to avoid requiring root) — and the kernel's
ping-socket implementation *rewrites* the ICMP identifier field to a kernel-assigned value on
send and only delivers matching replies to that same socket's receive queue. The userspace
identifier this code builds into the packet never actually round-trips, so the added check could
never match — confirmed empirically, reverted cleanly (`git checkout`), full note left in ticket
280 so this isn't attempted again blind. **Process lesson reinforced**: for any change to
protocol-level/syscall-level code, run the *live* test (not just build+compile) before
committing — this is exactly why `PingTests.cpp` hits a real socket instead of mocking, and it
caught a plausible-looking but wrong fix in under 30 seconds that pure code review would have
missed entirely.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked`. When auditing any other custom numeric-string parser in this codebase, check its
digit-accumulation overflow guard specifically for the check-before-vs-after-multiply
distinction — cheap to verify with a small brute-force/randomized harness against
`__uint128_t` ground truth (see ticket 281's approach), and this session has now found it wrong
once in a security/correctness-relevant place.

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 274-277 closed, a whole family of overflow bugs across the date/time types

Continuing the same autonomous run (previous checkpoint covered 268-273+1488). This stretch
turned into a mini-audit-chain: TimeOnly's overflow bug (274) led directly to finding and fixing
the *same* bug shape in DateTime (275) and DateTimeOffset (276), because each type's `Add`-style
methods were written independently instead of sharing one validated implementation. Worth
internalizing as a process note: **when an Add/arithmetic overflow bug is found in one
date/time-like type, check its siblings immediately** — TimeOnly → DateTime → DateTimeOffset was
a 100% hit rate this stretch (3 for 3).

**4 tickets closed: 274 (TimeOnly.hpp), 275 (DateTime.hpp/.cpp), 276 (DateTimeOffset.cpp), 277
(HebrewCalendar.cpp).** Test count: 11693 → 11701 (10 new regression tests). Commits: a5f18bc,
4564e43, a5174c2, 7eda594 — all pushed to `origin/feature/work`.

The overflow family, all confirmed via standalone UBSan repros before/after:
- **TimeOnly::Add(TimeSpan)** added the TimeSpan's raw ticks (up to ~Int64::MaxValue) to this
  instance's bounded (<1 day) ticks *before* taking `% TicksPerDay` — real signed-overflow UB
  for a TimeSpan near TimeSpan::MaxValue/MinValue. The fix (reduce modulo *before* adding) was
  already sitting right next to the bug, correctly implemented in `AddHours`/`AddMinutes`'s
  `AddTicksWrapped` helper — `Add(TimeSpan)` just didn't reuse it.
- **DateTime::AddDays(int)/AddHours(int)** multiplied the int argument by TicksPerDay/TicksPerHour
  with *no upfront bounds check* — overflows int64 for a merely large (not extreme) argument,
  e.g. `AddDays(1000000000)`. Real .NET's `AddUnits` guards `Math.Abs(value) > maxUnitCount`
  before multiplying; added the equivalent `MaxDays`/`MaxHours` constants (`MaxTicks /
  TicksPerUnit`). `AddMinutes`/`AddSeconds`/`AddMilliseconds` turned out NOT to need the same
  guard — their own Max* bounds exceed `intcs`'s range, so no valid `int` argument can reach the
  overflow threshold.
- **DateTime::AddTicks/Add(TimeSpan)/Subtract(TimeSpan)** all did signed `ticks_ +/- delta`
  directly — UB for a TimeSpan near MaxValue/MinValue. Real .NET's own `AddTicks`/`Subtract`
  route through `(ulong)(Ticks +/- value)` plus one unsigned comparison; ported the same pattern
  with genuinely well-defined C++ unsigned arithmetic instead of relying on signed wraparound.
- **DateTimeOffset::AddMonths/AddYears** were reimplemented from scratch instead of delegating to
  `DateTime.AddMonths/AddYears` like real .NET does (`AddMonths(int) =>
  Add(ClockDateTime.AddMonths(months))`) — lost DateTime's own ±120000 bounds check in the
  process, so `years * 12` overflowed int32 for `years` as small as 200 million. Fixed by
  delegating, which also deletes ~15 lines of duplicated logic.
- **HebrewCalendar::ToDateTime** (a different bug class, found while auditing the same
  date/time area) silently discarded its `era` parameter — called `GetDaysInMonth(year, month)`
  with *that method's own default* era instead of the caller's actual era, so an invalid era
  never got validated. The validation machinery (`getHebrewYearType`) already existed and worked
  correctly elsewhere in the same file; `ToDateTime` just never invoked it with the right value.
  Noted but did NOT fix: `HijriCalendar::ToDateTime` has the same-looking pattern, but that
  whole file never validates era anywhere (a different, more systemic simplification, not an
  isolated oversight) — needs its own verification against real .NET before concluding it's a
  bug, left for HijriCalendar's own audit ticket.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 stays
`blocked` per explicit user decision. When auditing any remaining `Calendar`-derived type
(HijriCalendar, JapaneseCalendar, TaiwanCalendar, etc.), specifically check whether `ToDateTime`
passes its `era` parameter through to whatever internal validation exists, rather than silently
using a default — this exact shape has now been found once (HebrewCalendar) and suspected once
more (HijriCalendar, unconfirmed).

## Session checkpoint (2026-07-12, autonomous run continuing) — tickets 268-273 + 1488 closed, still going

Continuing the same autonomous run as the checkpoint below (268 onward). User corrected the
assistant mid-session for pausing to report progress and ask "want me to continue?" — the
standing instruction is to keep working continuously without stopping for check-ins, only
pausing for genuinely blocking/irreversible/architectural decisions. This checkpoint is a
routine documentation update, not a stopping point.

**7 tickets closed this stretch: 268 (Half.hpp), 269 (Array.hpp), 270 (Matrix4x4.hpp), 271
(DateTimeFormatInfo.hpp), 272 (Int32.hpp), 1488 (systemic DivRem zero-check, filed mid-session),
273 (Environment.cpp).** Test count: 11651 → 11692 (41 new regression tests). Commits:
41937a3, be32dff, f7aa6b5, f51b6ee, 86e69b9, d018100, 31ba1e3 — all pushed to `origin/feature/work`.

Highlights:
- **Half::GetHashCode()** used `b = 0x7C00` (assignment) where real .NET does `bits &=
  PositiveInfinityBits` (AND-mask) — equal for NaN, but wrong for ±0 (should hash to 0, not
  0x7C00). Pre-existing test encoded the old wrong value; fixed.
- **Array.hpp**: every range-taking static method (`Sort`, `Copy`, `IndexOf`, `Reverse`,
  `Clear`, `BinarySearch`, `Fill`, `FindIndex`, `FindLastIndex`, `LastIndexOf`) had **no bounds
  validation at all** — silently read/wrote out of range instead of throwing
  `ArgumentOutOfRangeException`. Added the same `requireValidRange`/`requireValidStartIndex`/
  `requireValidBackwardRange` helpers real .NET's `Array.cs` uses (unsigned-comparison style,
  same overflow-safe pattern as the `Span<T>::Slice` fix from the previous stretch). 19 new
  tests. Unrelated to blocked ticket #138 (int-vs-intcs naming in this same file — not touched).
- **Matrix4x4::CreatePerspectiveFieldOfView** was missing real .NET's `far ==
  +Infinity → range = -1` special case (used for infinite-far-plane projections); without it,
  `far/(near-far)` evaluates as `inf/-inf = NaN`, silently producing a garbage matrix instead of
  a valid infinite-far-plane one. `CreateOrthographic`, `CreateLookAt` (the Impl reference has an
  explicit `Transpose()` call that's easy to miss on a partial read — initially misdiagnosed as
  a transpose bug, wasn't one), `Invert`, `GetDeterminant`, `CreateFromQuaternion` all verified
  correct.
- **DateTimeFormatInfo::GetAllDateTimePatterns(char)** silently returned an empty vector for an
  unrecognized format character; real .NET throws `ArgumentException(nameof(format))`. Fixed;
  updated the one pre-existing test that asserted the old silent-empty-vector behavior.
- **Int32.hpp — three real bugs, one systemic**: `DivRem` had **no zero-divisor check at all**
  (`left/right` with `right==0` is undefined behavior / hardware SIGFPE trap in C++, not a
  catchable exception — real .NET's CLR traps this into `DivideByZeroException`).
  `CopySign(MinValue, negativeSign)` negated `MinValue` — UB in C++ (confirmed via UBSan); real
  .NET's identical-looking code relies on C#'s unchecked-negation-wraps-to-self guarantee, which
  C++ doesn't have. `ToString(value, format)`'s `std::stoi` width parse let raw
  `std::invalid_argument`/`out_of_range` escape instead of `FormatException`. **Grepping for the
  same DivRem pattern across sibling types found it missing in Int16, Int64, SByte, Byte,
  UInt16, UInt32, UInt64 too** — filed and immediately fixed as P1 ticket 1488 (Int64 additionally
  needed the MinValue/-1 `OverflowException` check, since int64_t division runs at native width
  with no C++ integer-promotion safety net, unlike the 8/16-bit signed types). All 8 zero-check
  paths + the Int64 MinValue/-1 path reverified with a standalone UBSan/ASan repro linked
  against `libSHARP_RUNTIME.a` — zero sanitizer output post-fix.
- **Environment::getMachineNameProperty()** on POSIX returned the raw `gethostname()` result
  including any domain suffix; real .NET's Unix `MachineName` truncates at the first `.`.
  `getUserDomainNameProperty()` delegates to `MachineName` on POSIX so it inherited the fix
  automatically.

### To resume
Query the next ticket: `sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title
FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"`. Ticket #43 (global
`int`→`intcs` policy) stays `blocked` per explicit user decision — do not reopen. Standing
process note: when a `DivRem`-shaped or `start+length`-shaped bug is found in one type, grep
sibling types immediately rather than waiting for their individual audit tickets to come up —
both of this session's systemic tickets (1487, 1488) were found exactly this way.

## Session checkpoint (2026-07-12, autonomous run — pausing here) — 17 tickets closed, 2 more real bugs found (266/267), session summary

Continuation of the checkpoints below (same autonomous run). Pausing the run here after a long,
productive stretch — not blocked on anything, just a natural checkpoint. Everything below this
point through "Fixed this batch" summarizes the **whole session** for anyone resuming cold;
skip to "To resume" if you already have context.

### What happened this session, end to end

Started from a clean, fully-classified `plan.sqlite3` `task` table (namespace porting done) with
the `ticket` table (stabilization work) as the active queue: 609 P2/P3 `todo` at session start.
User authorized a long unattended run with one constraint (ticket #43, the global `int`→`intcs`
API policy decision, stays `blocked` — not reopened). Worked the queue sequentially per
`prompt.md`'s standard workflow (read target file fully, verify every claim against
`/rv/tmp/runtime/src/libraries/`, fix real bugs with regression tests, build+test clean, update
`plan.sqlite3`, commit+push, move on without stopping), from ticket 254 through 267, plus
resolved the one pre-existing `needs_user` ticket (#1161) and filed+resolved a new P1 ticket
(#1487) discovered mid-session.

**17 tickets closed: 254, 1486, 255, 256, 258, 257, 259, 260, 261, 262, 263, 264, 265, 306,
1487, 266, 267** (plus #1161 resolved at the very start of this session, before the autonomous
run began). Two were legitimate clean audits (260 Decimal.cpp, 263 GC.hpp) — thoroughly checked,
no bug found, nothing to commit. The other 15 all found and fixed **real, verified bugs**, most
confirmed with a standalone compiled repro (UBSan for overflow/UB cases) before *and after*
fixing — this was consistently the highest-value habit of the whole session.

Test count: 11557 → 11651 (94 new regression tests, zero regressions at any point — full suite
was run after every single change, not just at the end). 20 commits pushed to
`origin/feature/work` (15 fix/feat commits + 5 `docs(NEXT.md)` checkpoints).

### The bugs, roughly by severity

**Serious (memory-safety / crash-class):**
- `Span<T>`/`ReadOnlySpan<T>`/`Memory<T>`/`ReadOnlyMemory<T>`/`MemoryExtensions`/
  `ArraySegment<T>`/`String::ToCharArray` (tickets 265/306/1487): bounds checks computed as
  `start + length > total` directly in 32-bit arithmetic — for large inputs this overflows
  (confirmed real UB via UBSan) *and* the wrapped negative sum silently **bypasses the bounds
  check entirely** instead of throwing. Real .NET's own `Span<T>.Slice` source has an explicit
  comment guarding against exactly this. Two of the affected files had *already-closed* tickets
  that evidently missed this exact bug — fixed directly rather than left to bitrot, since closed
  tickets aren't revisited by the queue on their own.
- `MemoryStream`/`UnmanagedMemoryStream::Write` (ticket 1487): a *more severe* variant of the
  same root cause — since `Position` can legally be set arbitrarily far past the end (matching
  real .NET), `position_+count` can genuinely overflow and silently skip the resize/capacity
  check, so the subsequent copy would **write** through memory wildly beyond the buffer's actual
  allocation. Real .NET's own source computes this exact sum in a wider type specifically to
  avoid it; matched with `int64_t` widening.
- `Int128` `operator+`/`-`/`*`/unary`-`/`/`/`%` (ticket 262): raw signed `__int128` arithmetic,
  genuinely overflowing for extreme values (5 separate confirmed UBSan repros). Real .NET's
  unchecked `+`/`-`/`*` wrap silently (fixed via unsigned-arithmetic wraparound); `/`/`%` throw
  `OverflowException` for `MinValue/-1` even unchecked (matched with an explicit check).
- `TimeSpan`'s 6-arg `TimeToTicks` (ticket 256/258): real signed-overflow UB for extreme
  component values (confirmed via UBSan), fixed via `uint64_t` widening.
- `ClientWebSocket::readFrame()` (ticket 264): unbounded 64-bit frame length read straight off
  the wire before allocating — a malicious/misbehaving server could trigger a huge allocation
  attempt (`std::bad_alloc`/`std::length_error`, not catchable as `WebSocketException`). Capped
  at 256 MiB.
- `ArrayList` (ticket 255): ~13 methods had **zero** bounds validation at all — out-of-range
  `index`/`count` hit raw `std::vector` iterator arithmetic directly (UB, not an exception).

**Correctness (wrong behavior on valid or malformed input, not memory-unsafe):**
- `Math::Min`/`Max(double/float)` (ticket 257): delegated to `std::min`/`std::max`, which don't
  match .NET's IEEE 754:2019 semantics (NaN propagation asymmetric by argument order; wrong
  signed-zero tie-break).
- `Math::DivRem` (ticket 257), `HttpClient` Content-Length/chunk-size (ticket 267): unguarded
  `std::sto*`/raw division let raw `std::*` exceptions (or a real divide-by-zero trap) escape
  instead of a catchable `System::` exception — same exception-type-normalization bug class this
  codebase has fixed repeatedly across sessions.
- `Environment::ExpandEnvironmentVariables` (ticket 254): a from-scratch scanner that diverges
  from real .NET's actual (non-obvious) algorithm for how a failed `%name%` token's closing `%`
  interacts with the next token.
- `TimeSpan::TryParse` (ticket 256/258): `sscanf`-based parsing accepted trailing garbage
  (`"12:34:56garbage"` silently parsed as `12:34:56`).
- `HttpRequestHeaders::isValidHost` (ticket 266): missing NUL-byte rejection present in this
  same file's sibling validator.

**API completeness (real .NET surface missing, not a bug in what exists):**
- `BinaryPrimitives` (ticket 259): zero `Int128`/`UInt128` support despite both types being
  fully ported elsewhere — added 18 methods, and along the way discovered + guarded against a
  real portability hazard (`Int128.hpp`/`UInt128.hpp` `#error` unconditionally on MSVC just from
  being included, which would have silently made the whole previously-portable
  `BinaryPrimitives.hpp` MSVC-unsupported).
- `List<T>` (ticket 261): missing the 3-arg `IndexOf`/`LastIndexOf(item, startIndex, count)`
  range-bounded overloads.

### Process notes worth keeping

- **UBSan repros are the single highest-value habit in this codebase's stabilization work.**
  Every "smells like an overflow" hunch this session turned out to be real when checked with a
  standalone compiled repro, and the fix was verified to actually eliminate the UB the same way
  (re-run the repro after fixing, confirm zero sanitizer output). Keep doing this — don't fix an
  overflow-shaped bug on inspection alone.
- **A `done` ticket is a claim about the past, not a guarantee about now.** The `Span`/`Memory`
  bug was found by grepping for the same anti-pattern across the whole codebase after fixing one
  instance — two of the four files it turned up in already had closed tickets. If you find
  contradicting evidence about a closed ticket's claims while working on something else, don't
  let "it's already done" stop you from re-verifying and fixing it.
- **File a new ticket immediately when a systemic pattern is found mid-ticket**, with a priority
  reflecting actual severity (not just the current batch's default) — ticket #1487 was P1, not
  the batch's routine P2, specifically because it was a confirmed memory-safety class. A mental
  note doesn't survive a context reset; a ticket row does.
- **Never put backticks in a double-quoted shell string destined for `sqlite3`** — bash
  interprets them as command substitution even inside a nested SQL string, silently eating the
  quoted content. Recovered once this session by rewriting the note via a small Python
  `sqlite3` script instead (immune to shell quoting) — prefer that approach for any ticket note
  containing code snippets or other shell metacharacters.
- Two files this session (256/258, 265/306) had two separate ticket rows for the same
  header+`.cpp` pair — completed both together in one pass each time rather than re-reading the
  same file twice across two separate sessions.

### To resume

```sql
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```
Next up is **ticket 268**. 587 P2 + 6 P3 tickets remain `todo` (100 P2 stay `blocked` on ticket
#43 — a user decision, not a default next action; do not reopen it without asking). Same
workflow as every ticket this session: read the target file fully, verify every non-trivial
claim against `/rv/tmp/runtime/src/libraries/`, fix real bugs with regression tests (confirm
genuine UB with a standalone UBSan repro before *and after* fixing when it smells like an
overflow/UB case), build+test clean (`cmake --build build --parallel 8` then
`./build/SharpRuntimeTests`, expect 11651+ passing), update `plan.sqlite3` (Python for notes
with code snippets/backticks, not raw shell), commit+push to `feature/work`, move to the next
ticket without stopping unless a genuinely `blocked`/`needs_user` case comes up.

## Session checkpoint (2026-07-12, autonomous run continued again) — ticket #1487 (P1) fully closed, 15 tickets closed total this session

Continuation of the checkpoint below. Ticket `#1487` — the systemic `start+count`/
`start+length` integer-overflow bounds-check-bypass pattern filed mid-session while auditing
`Span.hpp` (ticket 265) — is now **fully resolved**. Fixed every occurrence its own
codebase-wide grep found:

- **`ArraySegment.hpp`** (constructor + `Slice(int,int)`) and **`String.cpp`**
  (`ToCharArray`): same shape as `Span<T>::Slice`, fixed identically (unsigned comparison +
  subtraction instead of signed addition).
- **`MemoryStream.cpp`**/**`UnmanagedMemoryStream.cpp`** `Write()`: a *more severe* variant —
  `Position` can legally be set arbitrarily far past the end (matching real .NET's own
  `Position` setter, which allows seeking past `Length`), so `position_+count` can genuinely
  overflow; a wrapped negative sum would silently bypass the resize/capacity check, letting the
  subsequent copy **write** through a position wildly beyond the buffer's actual allocation — a
  real heap/unmanaged buffer overflow, not just a bypassed bounds check. Verified real .NET's
  own `MemoryStream.Write`/`UnmanagedMemoryStream.WriteCore` compute this exact sum in a wider
  type (`long`) specifically to avoid it and explicitly check for the overflow — matched with
  `int64_t` widening (C#'s unchecked overflow is *defined* to wrap; C++'s is UB regardless of
  "checked" context, so widening rather than relying on wraparound was required).
- Verified `ReadOnlySequence<T>::Slice` uses `longcs` (int64) for the equivalent sum, not
  `intcs` (int32) — theoretically the same bug class but the overflow threshold requires
  sequence lengths near `INT64_MAX`, physically unreachable; noted, not fixed.

6 new regression tests. Commit `8971948`.

**Process incident, worth remembering:** the first attempt to write this ticket's `notes` field
via `sqlite3 plan.sqlite3 "UPDATE ... SET notes = ... '\`int i = ...\`' ..."` silently corrupted
the note — bash interprets backticks as command substitution even *inside* the SQL string when
the whole thing is wrapped in double quotes, so `` `int i = _position + count` `` got executed
as a (failing) shell command and the code snippet vanished from the saved note. Recovered by
rewriting the note via a small Python `sqlite3` script instead (immune to shell quoting).
**Lesson: never put backticks in a double-quoted shell string destined for sqlite3 — use single
quotes for inline code snippets in ticket notes, or write notes via Python when the content has
any shell metacharacters at all.**

Test count: 11642 → 11648. All commits pushed to `origin/feature/work`.

### To resume

```sql
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```
With `#1487` closed, this now correctly falls back to the P2 queue: **ticket 266** is next (589
P2 + 6 P3 remain `todo`, 100 P2 stay `blocked` on ticket #43 per user decision — do not
resurface it as a default next action). Same workflow as always: read the target file fully,
verify every non-trivial claim against `/rv/tmp/runtime/src/libraries/`, fix real bugs with
regression tests (confirm genuine UB with a standalone UBSan repro before *and after* fixing
when it smells like an overflow case — this has been the single highest-value habit across this
entire session, having caught 8 separate confirmed-via-repro bugs so far across
TimeSpan/Int128/Span/Memory/ArraySegment/String/MemoryStream/UnmanagedMemoryStream), build+test
clean, update `plan.sqlite3` (avoid backticks in notes text — see the process incident above),
commit+push, move to the next ticket without stopping.

## Session checkpoint (2026-07-12, autonomous run continued again) — 14 tickets closed total, 3 more real bugs found (264/265/306), new P1 ticket #1487 filed, 2 more commits

Continuation of the checkpoint below (same autonomous run). This entry only covers what's new.

### Fixed this batch (tickets 264, 265, 306; new ticket #1487 filed; commits `e24f1db`, `e1cc791`)

- **`ticket 264`, `src/System/Net/WebSockets/ClientWebSocket.cpp`**: `readFrame()` read the
  64-bit extended payload length straight off the wire with **no upper bound** before
  `readExact()` → `buffer.resize(n)` — a malicious/misbehaving server sending a huge length
  (near `UINT64_MAX`) triggers a correspondingly huge allocation attempt, throwing a raw
  `std::length_error`/`std::bad_alloc` instead of a clean, catchable `WebSocketException`.
  Added a defensive 256 MiB per-frame cap (matching the existing 16384-byte cap already applied
  to the handshake response a few lines up in the same file); verified with a loopback-socket
  test that a crafted huge-length header (zero payload bytes ever sent) throws promptly instead
  of hanging or crashing. Also made `CloseAsync` consistent with its three sibling methods,
  which all honestly comment out `cancellationToken` as unimplemented — `CloseAsync` alone
  captured it into its lambda without ever checking it, misleadingly implying cancellation
  works there specifically. Commit `e24f1db`.
- **`ticket 265`/`306`, `include/System/Span.hpp` + `include/System/Memory.hpp`** (plus
  drive-by fixes to `MemoryExtensions.hpp`/`ReadOnlyMemory.hpp`): **the most serious bug found
  this session.** `Span<T>::Slice(intcs,intcs)`/`ReadOnlySpan<T>::Slice` computed their bounds
  check as `start + length > totalLength` directly in `intcs` (int32) arithmetic — for large
  `start`/`length` this signed-overflows (**confirmed real UB via a standalone UBSan repro**),
  and the wrapped (very negative) result then compares as `<= totalLength`, **silently
  bypassing the bounds check entirely** instead of throwing (e.g. `start=INT32_MAX, length=10`
  on a 10-element span constructs a wildly out-of-bounds `Span` instead of raising
  `ArgumentOutOfRangeException` — a real memory-safety hole, not just a wrong-answer bug). Real
  .NET's own `Span<T>.Slice(int,int)` source has an explicit comment guarding against exactly
  this, via unsigned comparison + subtraction instead of addition — fixed the same way. A
  codebase-wide grep for the same anti-pattern (`grep -rn "start + length\|+ length >\|+ count
  >"`) found it **also present** in `MemoryExtensions.hpp` (2 occurrences) and
  `ReadOnlyMemory.hpp` — both fixed directly in this same pass rather than left to bitrot,
  since both already had **closed** tickets (238, 562) that evidently missed this exact bug and
  the ticket-workflow process doesn't revisit closed tickets on its own. `Memory.hpp` (ticket
  306, still open) had the identical pattern too — fixed and closed in the same pass rather than
  waiting for the queue to reach it separately. **Not yet fixed, filed as new ticket `#1487`
  (P1 — higher than this batch's routine P2 code-audits, since it's a confirmed real
  memory-safety bug class)**: the same pattern also found in `ArraySegment.hpp` (×2),
  `String.cpp`, `MemoryStream.cpp`, `UnmanagedMemoryStream.cpp` — see that ticket's notes for
  exact file:line locations and the fix template to reuse. Commit `e1cc791`.

Test count: 11634 → 11642 across this batch (8 new regression tests). Both commits pushed.

### Process note

- The `Span`/`Memory` overflow bug is the clearest example yet in this session of why "audit a
  file, then grep for the same anti-pattern codebase-wide" is worth the extra few minutes: two
  of the four affected files had *already been marked done* by earlier, apparently-incomplete
  audits. A ticket being `done` means someone looked at the file once, not that the file is
  bug-free forever — don't skip re-verification of a closed ticket's claims if you stumble onto
  contradicting evidence while working on something else.
- When a systemic bug pattern is found mid-ticket and fixing every occurrence would blow up the
  current ticket's scope, file a new ticket immediately (with priority reflecting actual
  severity, not just the batch's default) rather than leaving a mental note that won't survive
  a context reset. `#1487` is P1, not P2, specifically because "confirmed memory-safety bug
  class" is more urgent than routine stabilization sweep items.

### To resume

```sql
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```
That query now surfaces **ticket `#1487` first** (P1 sorts before P2) — the systemic
`start+length`/`start+count` integer-overflow bounds-check-bypass pattern in
`ArraySegment.hpp`/`String.cpp`/`MemoryStream.cpp`/`UnmanagedMemoryStream.cpp`. Recommended:
take it next, before falling back to the P2 queue (589 P2 + 6 P3 remain `todo`, 100 P2 stay
`blocked` on ticket #43 per user decision). Same workflow throughout: read the target fully,
verify every claim against `/rv/tmp/runtime/src/libraries/`, fix real bugs with regression
tests (confirm genuine UB with a standalone UBSan repro before *and after* fixing when it
smells like an overflow case), build+test clean, update `plan.sqlite3`, commit+push, move on
without stopping.

## Session checkpoint (2026-07-12, autonomous run continued) — 11 tickets closed total, 5 more real bugs found (259-263), 4 more commits

Continuation of the checkpoint below (same autonomous run, same rules: ticket #43 stays
blocked, standard prompt.md workflow). This entry only covers what's new since that
checkpoint — see it for the session's trigger/scope/process notes, still accurate.

### Fixed this batch (5 more real bugs across tickets 259-262, 1 clean audit at 263, 4 commits `e9047a8`..`9753097`)

- **`ticket 259`, `include/System/Buffers/Binary/BinaryPrimitives.hpp`**: `System::Int128`/
  `UInt128` are both fully ported elsewhere but had zero `BinaryPrimitives` support (real .NET
  added this in .NET 7) — added Read/Write/TryRead/TryWrite\*Endian (16 methods) +
  `ReverseEndianness` (2 more), byte layout verified by hand-tracing
  `MemoryMarshal.Read<Int128>` + `ReverseEndianness.cs`. **Important side-finding**: the new
  methods (and the `Int128.hpp`/`UInt128.hpp` includes) had to be guarded behind
  `#if !defined(_MSC_VER)`, since those two headers `#error` unconditionally on MSVC just from
  being included — without the guard this would have silently made the whole
  previously-portable `BinaryPrimitives.hpp` MSVC-unsupported as a side effect of an unrelated
  feature add. Half/BFloat16 (also fully ported, real but smaller gap) and IntPtr/UIntPtr
  (platform-width-dependent, needs a scope decision first) deliberately deferred to a follow-up
  ticket. Commit `e9047a8`.
- **`ticket 260`, `src/System/Decimal.cpp`**: thorough audit, **no new bugs found** — this file
  was already heavily fixed in prior sessions (the `Decimal(double)` UB fix referenced in an
  earlier checkpoint). Traced arithmetic, rounding (all 4 `Round` overloads × all 5
  `MidpointRounding` modes), `GetHashCode`/`Equals` consistency, and every constructor against
  `Decimal.cs` by hand; everything checked out. One pre-existing, deliberate-looking
  simplification noted but not changed: `TryParse` accepts `,` as an alternative decimal-point
  spelling (supports German-style `1234,56`) but not English-style thousands grouping
  (`1,234.56` fails) — this project has no culture support to know which convention a given
  string uses, so treating both as literal alternative decimal-point spellings (not attempting
  grouping at all) is a defensible scope choice, not a bug. No commit (no changes).
- **`ticket 261`, `include/System/Collections/Generic/List.hpp`**: unlike `ArrayList` (ticket
  255), this file already had proper bounds validation everywhere. Found one real gap: the
  3-arg `IndexOf(item,startIndex,count)`/`LastIndexOf(item,startIndex,count)` range-bounded
  overloads were entirely missing (only 1-arg/2-arg existed) despite being part of real .NET
  `List<T>`'s surface — added both, matching `List.cs` exactly. Noted, not implemented:
  `Sort`/`BinarySearch` overloads taking an `IComparer<T>` object (only the `Comparison`
  delegate form and default-comparer `BinarySearch` exist) — `IComparer<T>` already exists in
  this codebase so it's feasible, left for a follow-up. Commit `762b9133`.
- **`ticket 262`, `include/System/Int128.hpp`**: the big one this batch — `operator+`/`-`/`*`,
  unary `operator-()`, and `operator/`/`%` all used raw signed `__int128` arithmetic directly,
  genuinely overflowing (**confirmed via 5 separate standalone UBSan repros** before fixing —
  same bug class as this session's earlier `TimeSpan::TimeToTicks` fix, just found five more
  instances of it in one file). Verified real .NET's *exact* semantics differ by operator group
  before fixing (traced `Int128.cs` directly): unchecked `+`/`-`/`*` wrap silently on overflow
  (.NET computes via explicit carry/borrow-based unsigned arithmetic specifically to define
  this) — fixed by computing via `unsigned __int128` (always well-defined modular wraparound)
  and converting back via `static_cast` (well-defined 2's-complement reinterpretation per
  C++20, which this project requires). `operator/` (and `%`, defined in terms of it) is
  *different*: real .NET explicitly throws `OverflowException` for `MinValue/-1` even in its
  default unchecked form — added that explicit check instead of trying to wrap it. Verified
  the fix with a standalone compile+run under `-fsanitize=undefined,address`: zero errors where
  the old code produced five. Commit `9753097`.
- **`ticket 263`, `include/System/GC.hpp`**: clean audit, no bugs — confirmed this file is
  correctly the "stub, correct end state" CLAUDE.md documents (every method genuinely a
  no-op/zero-return as its own doc-comment claims), API surface essentially complete against
  real .NET's `GC` (only `AllocateArray<T>`/`AllocateUninitializedArray<T>`/
  `GetConfigurationVariables()` omitted, each already documented as intentionally
  impossible/inapplicable). No commit (no changes).

Test count: 11608 → 11633 across this batch (25 new regression tests, all in tickets
259/261/262 — 260/263 were clean audits with nothing new to test). All commits pushed to
`origin/feature/work`.

### Process note

- Two of five tickets this batch (260, 263) were legitimate **clean audits** — thoroughly
  checked, no bug found, no code changed, no commit made (nothing to commit). This is a valid,
  expected outcome for a stabilization sweep across ~600 files, not a sign of the process
  slacking — don't feel obligated to manufacture a change where none is warranted. The
  `plan.sqlite3` `notes` field is what proves the audit actually happened either way.

### To resume

```sql
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```
Next up is **ticket 264**. 592 P2 + 6 P3 tickets remain `todo` (out of 1486 total: 788 `done`,
100 `blocked` on ticket #43 — left alone per user decision). Same workflow as documented in the
checkpoint immediately below: read target file fully, verify every non-trivial claim against
`/rv/tmp/runtime/src/libraries/`, fix real bugs with regression tests (confirm genuine UB with
a standalone UBSan repro before *and after* fixing when the bug smells like an overflow/UB
case — this batch alone found 6 such confirmed-via-repro bugs across TimeSpan and Int128),
build+test clean, update `plan.sqlite3`, commit+push, move to the next ticket without stopping.
```

## Session checkpoint (2026-07-12, autonomous run in progress) — 6 tickets closed (254/1486/255/256/258/257), 6 real bugs found and fixed, 4 commits

**Trigger:** user authorized a long unattended autonomous session over the P2/P3 ticket queue
(see "Autonomous Work Instructions" — full detail in this session's transcript, not
reproduced here). Pre-authorized scope: ticket #43 (global `int`→`intcs` policy) stays
**blocked**, not reopened, per explicit user choice at session start — do not resurface it as a
default next action. Standard workflow from `prompt.md` followed throughout: one ticket at a
time in priority order, verify every claim against `/rv/tmp/runtime/src/libraries/`, fix real
bugs (not paper over them), add regression tests, build+test clean, update `plan.sqlite3`,
commit+push to `feature/work`, move on without stopping.

**This entry will be updated/prepended again as the autonomous run continues** — treat the
"To resume" section below as authoritative for where to continue, not the specific commit
hash above (re-run `git log`/the test suite to get current state after any reset).

### Fixed this batch (6 real bugs, 4 commits `4a94db3`..`a5eace0`)

- **`ticket 254`, `include/System/Environment.hpp`/`.cpp`**: `ExpandEnvironmentVariables` used a
  from-scratch `%VAR%` scanner that diverges from real
  `Environment.ExpandEnvironmentVariablesCore` (`Environment.UnixOrBrowser.cs`) — when a
  `%name%` token fails to resolve, real .NET only consumes the opening `%` as literal text and
  lets the closing `%` double as the next token's opener (verified by hand-tracing the
  reference algorithm: `"%UNDEFINED%HOME%"` with `HOME` set produces
  `"%UNDEFINEDworld"`, not `"%UNDEFINED%HOME%"` unexpanded). Rewrote to match exactly, plus
  guarded `getenv("")` (POSIX-unspecified) for the `"%%"` case, and the public
  `GetEnvironmentVariable("")` entry point the same way. Also `GetEnvironmentVariables()`'s
  POSIX branch was missing the `eq > 0` guard the Windows branch in the same function already
  had (asymmetric within one function) — extracted a shared `splitEnvEntry()` helper.
  Bonus fix (same file touched): **ticket 1486** — `EnvironmentTests.cpp`'s
  `TickCount64_Advances` busy-loop signed-int-overflowed under UBSan; widened to `long long`.
  Commit `d3c6527`.
- **`ticket 255`, `include/System/Collections/ArrayList.hpp`**: ~13 methods (`Insert`/
  `InsertRange`, `RemoveRange`, `IndexOf`/`LastIndexOf`'s `startIndex`/`count` overloads,
  `Reverse(index,count)`, `SetRange`, `GetRange`, `Sort(index,count,...)`,
  `BinarySearch(index,count,...)`, `Repeat`, plus the `ArrayList(int capacity)` constructor and
  `Capacity` setter) did **zero** argument validation — an out-of-range `index`/`count` hit
  `std::vector` iterator arithmetic directly (real UB, not just a wrong-answer bug). Added
  validation matching `ArrayList.cs` exactly per method, including `LastIndexOf`'s empty-list
  special case and `Insert`'s `index == count`-is-legal-at-the-end case. Extracted
  `requireValidRange`/`requireInsertIndexInRange` helpers matching the existing `List<T>`
  pattern. `CopyTo(void*, int)` structurally cannot validate (raw pointer carries no length) —
  documented as a known whole-codebase `IList`/`ICollection` interface limitation, not fixed
  (would need a broader interface redesign). Commit `dce1af8`.
- **`ticket 256`/`258`, `include/System/TimeSpan.hpp` + `src/System/TimeSpan.cpp`** (one file,
  both tickets): the 6-arg `TimeToTicks` (used by the `(days,hours,minutes,seconds,
  milliseconds,microseconds)` constructor) had genuine signed-integer-overflow UB for extreme
  component values like `days == INT32_MAX` — **confirmed via a standalone UBSan repro** before
  fixing (habit reconfirmed as worthwhile this session, again). The 3-arg
  `TimeToTicks(hour,minute,second)` sibling is fine — .NET's own source comment proves its
  smaller (seconds, not microseconds) scale factor can't overflow int64. Fixed by computing in
  `uint64_t` (defined wraparound) and converting back via `static_cast`, which is well-defined
  2's-complement wraparound as of C++20 (project requires C++23) — reproduces .NET's own
  unchecked-`long`-wraparound-then-range-check semantics exactly, without the UB. Separately,
  `TryParse` used `sscanf()`, which only checks for a matching *prefix* — `"12:34:56garbage"`
  silently parsed as `12:34:56` instead of being rejected like real `TimeSpan.Parse`
  (`FormatException`); added an explicit end-of-string check. Noted, not implemented: real
  .NET has newer (.NET 7/8) `FromDays(int)`/`FromHours(int)`/`FromMinutes(long)`/
  `FromSeconds(long)` integer overloads (plus multi-component friends) using `Int128`/
  `Math.BigMul` — a real API-surface gap, but a separate scope decision, not a bug. Commit
  `93172be`.
- **`ticket 257`, `include/System/Math.hpp` + `src/System/Math.cpp`**: `Math::Min`/`Max(double,
  double)` and `(float,float)` delegated to `std::min`/`std::max`, which do **not** match
  .NET's actual IEEE 754:2019 `minimum`/`maximum` semantics — confirmed
  `std::min(5.0, NaN) == 5.0` (not NaN; asymmetric, NaN only propagates as the *first* arg) and
  `std::min(+0.0, -0.0) == +0.0` (should be `-0.0`, .NET treats +0 as greater than -0). Rewrote
  both to the exact .NET algorithm — the neighboring `MaxMagnitude`/`MinMagnitude` in the same
  file already had it right, which is how the inconsistency was spotted. Also `Math::DivRem`
  (all 4 overloads: out-param/pair-returning × int/long) never checked for a zero divisor —
  real .NET's CLR `div`/`rem` instructions throw a catchable `DivideByZeroException`
  automatically; plain C++ integer division by zero is UB (crash/trap). Added an explicit
  `b == 0` check. **Noted, deliberately not fixed further**: the same
  int-division-by-zero-is-UB gap is structurally pervasive across the *whole* codebase wherever
  raw `intcs`/`longcs` division is used (`intcs` is a plain `int32_t` alias, not a
  checked-arithmetic wrapper class) — fixing `Math::DivRem` specifically is justified since
  dividing is its entire purpose and it's small/self-contained, but a codebase-wide fix needs a
  separate architecture decision, not a per-file audit fix. Commit `a5eace0`.

Test count: 11566 → 11608 across this batch (42 new regression tests). All commits pushed to
`origin/feature/work`. Zero test failures at every checkpoint; build clean (0 errors/0
warnings) verified after every fix, not just at the end.

### Process notes

- Every fix in this batch was found by actually reading the target file end-to-end against the
  real `.NET` reference source, not by pattern-matching or assumption — several (TimeSpan's
  overflow, Math's Min/Max) were only caught by tracing through the reference algorithm by hand
  or writing a standalone compiled repro, not by inspection alone. Continuing the pattern from
  prior sessions: this is worth the extra time.
- Two tickets (256, 258) were the same file's header and `.cpp` split into separate ticket
  rows — completed together in one pass since auditing one half without the other would have
  been redundant re-reading. Closed both with one commit; noted explicitly in each ticket's
  `notes` that it was resolved alongside its sibling.

### To resume

```sql
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```
Next up is **ticket 259** (`include/System/Buffers/Binary/BinaryPrimitives.hpp`). 597 P2 + 6 P3
tickets remain `todo` as of this checkpoint (out of 1486 total: 783 `done`, 100 `blocked` on
ticket #43 — left alone per user decision, not a default next action). Continue the same
per-ticket workflow: read target file fully, verify every non-trivial claim against
`/rv/tmp/runtime/src/libraries/`, fix real bugs with regression tests, build+test clean,
update `plan.sqlite3`, commit+push, move to the next ticket without stopping unless a
genuinely `blocked`/`needs_user` case comes up (mark it precisely and continue with something
else — never let one blocked ticket stall the whole session).

## Session checkpoint (2026-07-12) — resolved the one `needs_user` ticket: real UTF8Encoding validation

**Trigger:** session start found the repo state unchanged since the last checkpoint (build clean,
11557/11557 passing, `task` table 100% classified, ticket queue at 609 `todo`/100 `blocked`/1
`needs_user`). Asked the user what to do next; they chose to resolve the single `needs_user`
ticket (**#1161**, `System.Text.UTF8Encoding`) before resuming the P2 ticket grind. Given a choice
between leaving it as a documented deviation, a minimal fix, or full validation, the user chose
**full validation**.

**What was wrong:** `UTF8Encoding::GetBytes`/`GetString` were a raw byte passthrough with zero
well-formedness checking in either direction — worse than `ASCIIEncoding`/`UnicodeEncoding`/
`UTF32Encoding`, which at least hardcode U+FFFD substitution for malformed input (commit
`a8b7a14`, an earlier session). Investigation also found that `System::Text::DecoderFallback`/
`EncoderFallback` (`include/System/Text/DecoderFallback.hpp`/`EncoderFallback.hpp`) — real,
already-implemented `CreateFallbackBuffer()`/`Fallback()`/`GetNextChar()` machinery, with
`Encoding` base class storage (`getDecoderFallbackProperty()`/`getEncoderFallbackProperty()`)
already wired in — had **zero production call sites anywhere in the codebase**; every concrete
`Encoding` subclass did its own hardcoded `'?'`/U+FFFD substitution instead of using it. Confirmed
via `grep -rn "CreateFallbackBuffer" include/ src/` returning only the two `Fallback.hpp`
definition files.

**Fix** (`include/System/Text/UTF8Encoding.hpp`, `src/System/Text/UTF8Encoding.cpp`, commit
`4a94db3`): `GetBytes`/`GetString` now validate well-formed UTF-8 (same continuation-byte/
overlong-encoding/surrogate/out-of-range rejection rules as the sibling encodings' `decodeUtf8`
helpers, factored into a new file-local `wellFormedUtf8Length`), pass well-formed bytes through
unchanged, and route each ill-formed byte through the real
`getEncoderFallbackProperty()`/`getDecoderFallbackProperty()` buffer (resync one byte at a time,
matching the sibling encodings' existing granularity) — this is now the first production code in
the repo that actually drives the fallback-buffer API end-to-end. Also fixed the constructor to
set a U+FFFD replacement default (matching real `UTF8Encoding.SetDefaultFallbacks()`) instead of
inheriting the generic `Encoding` base class's `"?"` default, which is correct only for
single-byte code pages — this was a latent, previously-unobservable bug since nothing ever
consulted the fallback object before. 9 new tests added to `tests/System/Text/
TextNamespaceTests.cpp` (`UTF8EncodingTests` suite): round trips (ASCII/non-ASCII BMP/
supplementary plane), bad-continuation-byte and overlong-encoding replacement on the encode
side, truncated-sequence and direct-surrogate-encoding replacement on the decode side, and
`ExceptionFallback` throwing on both sides. 11557 → 11566 tests passing. Marked `done` in
`plan.sqlite3` with full resolution notes; pushed to `origin/feature/work`.

**Deliberately left alone:** `ASCIIEncoding`/`UnicodeEncoding`/`UTF32Encoding` still hardcode
substitution instead of using the fallback objects — consistent, pre-existing behavior from an
earlier session, not part of this ticket's scope, not touched. Also did not add the two extra
`UTF8Encoding` constructor overloads from real .NET's surface (`encoderShouldEmitUTF8Identifier`,
`throwOnInvalidBytes`) — the BOM-related one needs a `GetPreamble()`/`Preamble` API that doesn't
exist anywhere on the `Encoding` base class today (a separate, unrelated feature gap), and
`throwOnInvalidBytes`'s behavior is already fully reachable today via
`setEncoderFallbackProperty(EncoderFallback::ExceptionFallback())`/the decoder equivalent, which
this session's new tests exercise directly.

**To resume:** back to the P2/P3 ticket grind, next up is **ticket 254**
(`include/System/Environment.hpp`) — see the query and full per-ticket workflow in the checkpoint
below and in `prompt.md`. 609 P2/P3 tickets remain `todo`; 100 stay `blocked` on ticket #43's
global `int`→`intcs` policy decision, which the user declined to reopen on 2026-07-07 ("zatím
neřešit") — this session offered it again as one of three next-step options and the user chose a
different one (resolving ticket #1161 instead), which is not itself a renewed decline. Still don't
treat it as a default next action without asking again.

## Session checkpoint (2026-07-11, continued again) — ticket-workflow pivot: `task` table 100% classified, 16 P2 code-audit tickets closed (238-253, 16 commits)

**Trigger:** `plan.sqlite3`'s `task` table (namespace/.NET-type classification — `status` in
`''`/`todo`/`ported`/`ignore`/`tobedecided`) reached 100% classification with nothing left to
process. Work pivoted to the separate `ticket` table (stabilization work — `status` in
`todo`/`doing`/`done`/`blocked`/`needs_user`/`wontfix`, `priority` P0-P3). Its P0/P1 tiers were
already fully done in earlier sessions (see the memory note
`project_sharp_runtime_exception_type_audit.md` and this file's own earlier checkpoints below),
leaving P2 (~620 items at the start of this batch) and P3 (7 items) as the active queue. **This
is the first NEXT.md checkpoint to document the ticket-table workflow at all** — earlier
checkpoints below predate the pivot and describe a different (now-superseded) "dangerous-moderate
findings sweep" process.

**Per-ticket workflow** (full detail in `prompt.md`): read the target file fully; verify every
non-trivial claim against real .NET reference source in `/rv/tmp/runtime/src/libraries/` (never
trust memory/assumption); fix real bugs found (not paper over them) and add regression tests for
anything fixed; confirm `cmake --build build --parallel 8` and `./build/SharpRuntimeTests` are
clean; mark `doing`→`done` in `plan.sqlite3` with detailed notes including the commit hash;
commit+push to `feature/work` (routine pushes there are pre-authorized); move to the next ticket
without stopping to ask. Several genuine bugs in this batch were only caught by going one step
further than static reading — a standalone ASan/UBSan repro or a small throwaway test program —
which is worth continuing as a habit, not just skimming for "looks wrong."

**To resume:**
```sql
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, area, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 1;"
```
Next up is **ticket 254** (`include/System/Environment.hpp`). 609 P2/P3 tickets remain `todo` as
of this checkpoint, out of 1486 total (776 `done`, 100 `blocked` — almost all pre-existing,
self-documented policy-deferral tickets like #136/#137/#138, which depend on ticket #43's
unresolved global `int`→`intcs` policy decision and should not be actioned until that changes; 1
`needs_user`).

### Fixed this batch (16 tickets, commits `9873972`..`99fd000`)

- **`MemoryExtensions::AsSpan(vector, start[, length])`** had zero bounds validation — raw
  pointer arithmetic on out-of-range input, worse than even a wrongly-typed exception. Also added
  the missing 3-value `LastIndexOfAny` overload (present on sibling `IndexOfAny` and in real
  .NET, absent here). (ticket 238, `9873972`)
- **`Socket::Poll(-1, mode)`** never actually blocked — building a `timeval` unconditionally from
  `microSeconds` for the -1 "infinite" sentinel produced an invalid negative `tv_usec`, so
  `select()` failed `EINVAL` and returned immediately instead of waiting forever as documented.
  **`Socket::Connect(host, port)`** only ever tried `addresses[0]` from DNS resolution instead of
  every candidate address like real .NET. (ticket 240, `f8adcbc`)
- **`Decimal(double)`** invoked real UB: `std::llround(v)`'s return type (`long long`, max
  ~9.2e18) is far smaller than Decimal's own ~7.9e28 mantissa range, so e.g. `Decimal(1e20)`
  silently produced garbage instead of the correct value. Confirmed via a standalone UBSan repro.
  Fixed by splitting into two 64-bit limbs instead of routing through `long long`. (ticket 241,
  `257980d`)
- **`TimeZoneInfo::TransitionTime::CreateFixedDateRule`/`CreateFloatingDateRule`** never
  validated `timeOfDay` at all — a caller could pass a full multi-year `DateTime` as a
  time-of-day and have it silently accepted. (ticket 243, `079a424`)
- **`Double::Parse`/`TryParse`** (and identically, **`Single::Parse`/`TryParse`**) silently
  accepted `"inf"`, `"-inf"`, `"nan(123)"` — `std::from_chars`'s floating-point grammar
  recognizes these regardless of `chars_format` (a C++ standard requirement), but real .NET only
  recognizes the exact case-insensitive tokens `Infinity`/`+Infinity`/`-Infinity`/`NaN`/`+NaN`/
  `-NaN`. Same bug class as ticket 236 (XPath's `number()`) from an earlier session. (tickets
  245/249, `2f3c031`/`8d071b7`)
- **`Tuple8`/`Tuple::Create(8 args)` was entirely missing** — sibling `ValueTuple.hpp` already
  supports 8-element tuples (with a `Rest` field), `Tuple.hpp` only went up to 7. Added, with the
  hash-combining algorithm verified bit-exact against `Tuple.cs`'s `CombineHashCodes` (one of the
  few .NET `GetHashCode` contracts that's actually guaranteed stable — its own comment says "the
  F# compiler depends on the exact tuple hashing algorithm, do not ever change it"). (ticket 246,
  `e821c83`)
- **`Convert::ToHexString`/`ToHexStringLower`** had their casing backwards vs. real .NET
  (confirmed real-world impact: `PhysicalAddress.ToString()` calls `Convert.ToHexString`).
  **`ToBoolean(string)`** was too lenient (accepted `"1"`/`"0"`, wasn't fully case-insensitive).
  **`ToDouble(string)`** duplicated the ticket-245 bug. **`ToUInt32(string)`/`ToUInt64(string)`**
  leaked raw `std::invalid_argument`/`std::out_of_range` instead of `System::FormatException`/
  `OverflowException`. All fixed by delegating to the already-correct `Boolean::Parse`/
  `Double::Parse`/`UInt32::Parse`/`UInt64::Parse`. (ticket 248, `19dfd66`)
- **`Console::Write(vector<char>&, int, int)`** had zero bounds validation — a genuine
  out-of-bounds read for negative `index`/`count` or `index+count` exceeding the buffer, not just
  a wrong-exception-type gap. Also fixed several stub methods (`SetWindowSize`/
  `SetWindowPosition`/`SetBufferSize`/`MoveBufferArea`) using raw `int` instead of
  `SharpRuntime::intcs` (CLAUDE.md rule #7). (ticket 250, `8c497d7`)
- **`Calendar::AddMonths`** had no bounds check on `months` at all — confirmed via a standalone
  UBSan repro that `AddMonths(<year 9999 date>, INT_MAX)` overflows signed `int`, silently
  producing a nonsensical negative year instead of a clean exception. Real .NET's
  `GregorianCalendar.AddMonths` explicitly rejects `|months| > 120000` before any arithmetic;
  ported the same check. (ticket 251, `752d832`)

### Audited clean / doc+test-only (7 tickets)

`ValueTuple.hpp` (239, filled a Doxygen gap on arities 5-7), `Char.hpp` (242, documented — not
fixed, by design, since fixing would diverge from `String`'s own established byte-offset
convention — that the string-indexed overloads operate on raw bytes, not code points),
`String.hpp` (244, added 18 missing `@throws` doc-comments matching already-validated `.cpp`
behavior), `Base64.hpp`/`Base64Url.hpp` (247/253, both fully verified against .NET reference with
no bugs found — just missing `NeedMoreData`/upper-bound test coverage), `Guid.cpp`/`.hpp` (252,
an exceptionally carefully-ported file, bit-exact verified against `Guid.cs`, one missing test
case added).

Test count: 11493 → 11557 across this batch. All commits pushed to `origin/feature/work`.

## Session checkpoint (2026-07-11, continued again) — external review triage: 8 real bugs fixed + full exception-normalization sweep closed out (14 commits)

**Trigger:** the user pointed at an external Czech-language code review,
`../sharp-runtime.md` (one directory up from the repo root), of an old ZIP snapshot at commits
`fd1178a`/`16c823d` — confirmed via `git log` to be genuine ancestors of this branch, 238
commits behind HEAD at the time. Instruction: study every claim in that review, verify against
current code (not the stale snapshot), fix what's still real, ask if a decision is needed.

**Triage outcome:** most of the review's claims turned out to already be fixed by earlier
sessions (Ping's memset bug, DateTime constructor validation, CancellationTokenSource's
`ThrowIfDisposed`, HttpClient's/Dns's exception types generally). Of what remained, 8 were
confirmed still-real bugs (not stale) and got fixed this session; two required a user decision,
asked via `AskUserQuestion` and both resolved before work started:

- **MSVC/`__int128` blocker in `Decimal`/`Int128`/`UInt128`** → user chose "document as a
  permanent deviation" (not attempt hand-rolled 128-bit arithmetic). Added a new "What is
  MSVC-unsupported" table to `CLAUDE.md` (commit `58924bd`).
- **16 remaining files with raw `std::*` exceptions** (a continuation of an exception-type
  audit from an earlier session — see the memory note
  `project_sharp_runtime_exception_type_audit.md`) → user chose "finish now, same batch."

### Fixed (8 real bugs, commits `606924a`..`17f266c`)

- **`OperatingSystem::IsAndroid()`/`IsIOS()` hardcoded `false`**, and `IsLinux()` didn't
  exclude Android (Android's kernel also defines `__linux__`, so both could report `true`
  simultaneously — real .NET treats `OSPlatform` values as mutually exclusive). Fixed all
  three to use the correct preprocessor guards; confirmed `StoragePaths.cpp` already had real
  `__ANDROID__` handling that `IsAndroid()` contradicted. (`606924a`)
- **`ArrayList::GetEnumerator()`/`GetEnumerator(int,int)` unconditionally returned
  `nullptr`** — a bare stub. Any code enumerating an `ArrayList`, including its own
  `ICollection`-copying constructor, silently did nothing instead of iterating. Implemented a
  real fail-fast enumerator (version-counter pattern mirroring `Queue::Enumerator`), bumped
  `version_` at every verified `_version++` call site from real `ArrayList.cs`. Also corrected
  `plan.sqlite3`: this type was marked `ignored` despite being fully implemented — same
  pre-existing mis-classification pattern found again on `Hashtable` later this session.
  (`1e4f234`, `Hashtable` correction in `2784dce`)
- **`DateTime::TryParse` miscounted fractional-second digits** when a trailing ISO-8601
  `Z`/offset marker was present (`fracLen = s.size() - 20` counted the zone marker as if it
  were extra digits) — `".123Z"` silently produced `Millisecond=12` instead of `123`. Fixed by
  counting only actual digit characters. Documented a separate, pre-existing, out-of-scope
  limitation found along the way: fractions under 3 digits (e.g. `".5Z"`) are silently ignored
  by an unrelated `s.size() >= 23` gate — not touched, not what the review flagged. (`408dc8a`)
- **`HttpClient::parseUrl` broke on IPv6 literals** (`http://[::1]:8080/` — a bare
  `rfind(':')` split the address in the middle of the brackets when no port was given, and
  never stripped brackets even when a port was present) — the exact bug the review called out
  by name. **`HttpClient`'s response status line silently defaulted to HTTP 200 OK** when
  unparseable (no space in the line at all) instead of surfacing a failure. Extracted the
  status-line parser into a new testable static `HttpClient::parseStatusLine()` (mirroring the
  existing `parseUrl` "public for testability" pattern) that now throws
  `HttpRequestException` on a malformed line. (`5c196fb`)
- **`Dns` IPv6 resolution was effectively disabled**: `GetHostAddresses(host,
  AddressFamily::InterNetworkV6)` unconditionally returned `{}` without calling
  `getaddrinfo`; both `GetHostAddresses`/`GetHostEntry` hardcoded `hints.ai_family = AF_INET`
  even for `Unspecified`; the result-collection loops only handled `AF_INET`
  (`sockaddr_in`), silently dropping any `AF_INET6` result; and
  `GetHostEntry(const IPAddress&)` (reverse lookup) had no IPv6 code path at all — it called
  `getAddressProperty()`, which throws for IPv6 by contract. Fixed all four; added an
  `IPv6`-literal short-circuit (`tryParseIPv6Literal`, reusing `IPAddress::TryParse`) mirroring
  the existing IPv4 one. Verified against this build host's actual `getaddrinfo()` behavior
  that a family/literal mismatch (e.g. asking for IPv6-only resolution of an IPv4 literal)
  correctly fails with `EAI_ADDRFAMILY` — updated one pre-existing test that had hard-coded an
  assertion on the old silent-empty-vector behavior. (`17f266c`)

### Exception-normalization sweep closed out (16 files, commits `a2d1570`..`5a8303b`)

Continuation of an earlier session's audit (see memory:
`project_sharp_runtime_exception_type_audit.md`) that found raw `std::*` exceptions (invisible
to code catching `System::Exception&`) escaping from otherwise-ported types. This session
finished the remaining 16 files found by
`grep -rE 'throw std::(runtime_error|invalid_argument|out_of_range|overflow_error|
domain_error|logic_error)' include/ src/` (that grep now returns **zero matches** — the sweep
is complete):

| File(s) | Old type | New type | .NET source verified against |
|---|---|---|---|
| `Linq.hpp` (First/Min/Max, 4 sites) | `invalid_argument` | `InvalidOperationException` | `Enumerable`'s `ThrowHelper.ThrowNoElementsException`/`ThrowNoMatchException` |
| `Int128.hpp` (`Abs(MinValue)`) | `overflow_error` | `OverflowException` | `Int128.Abs` → `Math.ThrowNegateTwosCompOverflow` |
| `Text/Rune.hpp` (ctor) | `out_of_range` | `ArgumentOutOfRangeException` | `Rune(uint)` ctor |
| `Buffers/SequenceReader.hpp` (`Advance`) | `out_of_range` | `ArgumentOutOfRangeException` | `SequenceReader<T>.Advance` |
| `Buffers/Binary/BinaryPrimitives.hpp` (24 call sites, 1 helper) | `out_of_range` | `ArgumentOutOfRangeException` | `BinaryPrimitives` Read*/Write* — gave the shared `checkSize()` helper a `paramName` arg so Read (`source`) vs Write (`destination`) get the right name |
| `Collections/Hashtable.hpp` (`Add`, 2 overloads) | `invalid_argument` | `ArgumentException` | `Hashtable.Add` duplicate-key path — also corrected `plan.sqlite3` `ignored`→`ported` (same gap as `ArrayList`) |
| `Collections/ArrayList.hpp` (`RemoveAt`) | `out_of_range` | `ArgumentOutOfRangeException` | `ArrayList.RemoveAt` |
| `SharpRuntime/Experimental/Property.hpp` (2 sites) | `logic_error` | `NotSupportedException` | no direct .NET counterpart (internal experimental helper); closest .NET-idiomatic fit |
| 6× `Net/Http/Headers/*.cpp` (`StringWithQuality`, `TransferCodingWithQuality`, `MediaTypeWithQuality`, `RangeItem`, `ContentRange`, `ContentDisposition`, `RetryCondition` — 7 types, one file has 2) | `out_of_range` | `ArgumentOutOfRangeException` | each type's own constructor/setter in `System.Net.Http.Headers` — all use `ArgumentOutOfRangeException.ThrowIfNegative`/`ThrowIfGreaterThan` |
| `Xml/XPath/XPathAstInternal.cpp` (4 sites, exhaustive-switch fallbacks) | `logic_error` | `System::Diagnostics::UnreachableException` | .NET 7+'s `System.Diagnostics.UnreachableException` — a direct type match, not a judgment call |

Every file above got a real regression test where the old behavior was previously untested, or
had its pre-existing test's hard-coded exception-type assertion corrected where one already
existed. 11449/11449 tests passing (started this checkpoint at 11414).

### Honest scope note

- Two things found during triage were **deliberately left alone** as out of scope for this
  batch: `DateTime::TryParse`'s separate `s.size() >= 23` fraction-length gate (a different,
  pre-existing limitation the review didn't flag), and the general `parseUrl`/`parseStatusLine`
  question of whether to extract more of `HttpClient`'s response-parsing internals into
  testable static methods beyond what these two fixes needed.
- `SequenceReader<T>` itself has no tracked row in `plan.sqlite3` (only
  `SequenceReaderExtensions` does) — a pre-existing gap, not touched.
- This checkpoint's "16 files" count is the exact, itemized, `grep`-verified figure — not a
  prose estimate like earlier checkpoints' Threading figures.

### Commands used to verify (same pattern every commit this batch)

```
cmake --build build --parallel 8                         # Debug — 0 errors/0 warnings, every commit
./build/SharpRuntimeTests --gtest_filter="<targeted>"     # new/changed tests first
./build/SharpRuntimeTests                                 # full suite — 11449/11449 passing at HEAD
cmake --build build_no_tests --parallel 8                 # Release library-only — 0 errors/0 warnings
```

### Recommended next namespace

Nothing in this batch was namespace-scoped (it was a targeted bug-fix/review-triage pass, not
plan.sqlite3 traversal) — resume the plan.sqlite3 workflow in `prompt.md` from where the prior
session's resume prompt (§10 below) left off: `System.Numerics.Colors`, then the small
`System.Runtime.*`/`System.Security.*` namespaces, then the two large not-yet-started blocks
(`System.Security.Cryptography`, `System.Text*`/`System.Xml.Serialization`).

---

## Session checkpoint (2026-07-11, continued) — Threading high-risk moderate findings: 9 fixed, two-pass fresh audit (9 commits)

**Scope for this pass, per explicit user instruction:** focus exclusively on Threading's
remaining moderate findings, treating "moderate" as potentially high-risk. Prioritize race
conditions, cancellation/disposal correctness, callback ordering, deadlock/reentrancy,
wrong exception types, silent wrong behavior, undocumented .NET deviations, and flaky/
under-tested timing behavior. Explicitly **not** in scope: ordinary minor cleanup, Text.Json
refactors, Net/XML work.

**Methodology:** the previous checkpoints' "26 moderate + 10 minor" Threading figure was
prose, never itemized (see the caveat in the wave-3 catalogue section further down: "Threading's
own remaining-findings text ends '...and more' with the detail not preserved"). Rather than
guess which of those were still real, dispatched two read-only audit forks that re-derived
concrete findings by reading current source against `/rv/tmp/runtime/src/libraries/`, explicitly
told what was already fixed in prior sessions so they wouldn't re-report it:

- **Pass 1** covered most of `include/System/Threading/` + `src/System/Threading/` (all files
  except 8 explicitly time-boxed out). Found 6 findings, all fixed. Reported the following files
  clean (read in full, no new issues): `ReaderWriterLockSlim.hpp`, `Tasks/Task.hpp`,
  `Tasks/TaskCompletionSource.hpp`, `CancellationTokenSource.hpp`, `CancellationToken.hpp/.cpp`,
  `Barrier.hpp`, `WaitHandle.hpp`, `ThreadLocal.hpp`, `AsyncLocal.hpp`, `Monitor.hpp`,
  `ReaderWriterLock.hpp`, `Tasks/ValueTask.hpp`, `ManualResetEventSlim.hpp`, `Mutex.hpp`,
  `SpinWait.hpp`, `LazyInitializer.hpp`, `EventWaitHandle.hpp`, `Semaphore.hpp`,
  `Tasks/TaskFactory.hpp`, `ThreadPool.hpp`.
- **Pass 2** covered the 8 files pass 1 didn't reach: `Lock.hpp`, `AutoResetEvent.hpp`,
  `ManualResetEvent.hpp`, `Tasks/Parallel.hpp` (full read this time), `Tasks/TaskScheduler.hpp`/
  `.cpp`, `Thread.hpp`/`.cpp`, `Interlocked.hpp`, `Volatile.hpp`. Found 3 findings, all fixed.
  `AutoResetEvent.hpp`, `ManualResetEvent.hpp`, `Tasks/TaskScheduler.hpp`/`.cpp`, `Thread.cpp`,
  `Interlocked.hpp`, `Volatile.hpp` reported clean.

Between the two passes, **every file under `include/System/Threading/` and
`src/System/Threading/` has now been freshly audited** against the 8 requested risk categories
this session. See "Honest scope note" below for what this claim does and doesn't cover.

### Fixed (9 findings, commits `5daef24`..`b0aba00`)

- **`CancellationTokenRegistration::Dispose()` didn't wait for an in-flight callback** — a
  no-op race when the callback was already claimed by a concurrent `Cancel()`; returned
  immediately instead of waiting, risking use-after-free if the caller tears down a resource
  the callback references right after `Dispose()`. Verified against
  `CancellationTokenRegistration.cs`'s documented `Dispose()` contract
  (`WaitForCallbackIfNecessary`). Added `executingId`/`executingThreadId`/`callbackFinished`
  (condition_variable) to the shared state; same-thread self-unregister detected and skipped
  to avoid deadlock. 2 regression tests, one exercising the actual race window.
- **`Channel<T>::ReadAsync()`/`WriteAsync()` captured raw `this`** in a lambda run on a
  detached background thread (`Task`'s `std::async`) — use-after-free if the caller drops
  every reference to the `Channel`/`Reader`/`Writer` right after issuing the call. **Confirmed
  via a standalone AddressSanitizer repro**: pre-fix code reliably crashed with a
  heap-use-after-free on the first iteration; fix showed zero ASan errors across multiple
  runs. Fixed via `std::enable_shared_from_this` + `shared_from_this()` instead of raw `this`.
  2 regression tests.
- **`RegisteredWaitHandle::Unregister()` detached the background wait thread** instead of
  joining — could still be blocked inside `waitObject->WaitOne()` when `Unregister()` returns,
  racing a caller that deletes `waitObject` right after. Verified against
  `RegisteredWaitHandle.Portable.cs`; this port has no ref-counted-handle equivalent to
  .NET's `SafeWaitHandle.DangerousAddRef`, so blocking (joining) is the simpler safe
  alternative — documented as a deliberate deviation from .NET's non-blocking
  `Unregister(null)` contract. Self-unregister (called from within the wait thread's own
  callback) detaches instead, to avoid a self-join deadlock. 1 regression test using a
  `WaitHandle` test double that tracks in-flight `WaitOne()` calls; verified it fails
  pre-fix.
- **`Timer::Change()` silently discarded `dueTime` after the first fire, and wasn't
  interruptible mid-wait** — `run()` used non-interruptible `sleep_for` (a `Change()` call
  during the between-fires wait had no effect until the stale deadline elapsed) and
  unconditionally derived the next `dueTime` from `period` after every fire (clobbering a
  fresh `dueTime` a mid-callback `Change()` call had just set). Verified against
  `TimerQueue.UpdateTimer` (every `Change()` reschedules relative to when it's called).
  Replaced `sleep_for` with `condition_variable::wait_until` + a generation counter. 2
  regression tests; verified both fail pre-fix.
- **`CountdownEvent::Reset()` skipped `ObjectDisposedException`, and its negative-count
  sentinel swallowed validation** — `Reset(intcs count = -1)` used `-1` as a sentinel for
  "use `InitialCount`", so an explicit `Reset(-1)` call silently reset instead of throwing as
  real .NET's `Reset(int)` does for any negative count. Verified against
  `CountdownEvent.cs`. Split into `Reset()`/`Reset(intcs count)`, both now disposal-checked.
  2 regression tests.
- **`SemaphoreSlim` had no `disposed_`/`ObjectDisposedException` support at all** —
  `Dispose()` was a true no-op, inconsistent with every sibling Slim primitive already fixed
  this session. Verified against `SemaphoreSlim.cs`'s `CheckDispose()`: check order differs
  per method (`Wait(int)` validates its timeout before checking disposal; `Release(int)`
  checks disposal first) — matched each method's own ordering. 3 regression tests.
- **`Thread::Start()`/`Start(void*)` captured raw `this`; `~Thread()` detaches, not joins** —
  the same bug class as the `Channel`/`RegisteredWaitHandle` fixes above, in the most
  fundamental of the three primitives. **Confirmed via a standalone AddressSanitizer repro**:
  pre-fix code reliably crashed with heap-use-after-free in `Thread::Start()`'s lambda; fix
  showed zero ASan errors. Fixed via the same `shared_ptr<State>` indirection pattern already
  used by `Timer`/`RegisteredWaitHandle`/`CancellationTokenSource` this session
  (`finished_`/`isBackground_`/`managedThreadId_` moved into a heap-allocated `RunState`).
  2 regression tests.
- **`Lock::TryEnter(intcs)`/`TryEnter(TimeSpan)` skipped timeout validation** —
  `Lock.hpp` didn't even include `WaitHandle.hpp`, unlike every sibling wait primitive. A
  negative timeout other than `-1` silently behaved like a non-blocking `try_lock()` instead
  of throwing. Verified against `Lock.cs`. 3 regression tests.
- **`Parallel::For` (with `ParallelLoopState`) / both `ForEach` overloads launched unbounded
  concurrent `std::async` tasks**, unlike the sibling `For(..., ParallelOptions, ...)`
  overload, which already batches. For a large source, this could spawn far more OS threads
  than the hardware supports; past the creation limit, `std::async` itself throws
  `std::system_error` **unwrapped**, breaking the documented `AggregateException` contract.
  Extracted the existing batching pattern into a shared helper. 2 regression tests tracking
  observed peak concurrency; verified both fail pre-fix (64 concurrent vs. a 16 bound on the
  CI machine).

All 9 fixes followed the same discipline: verify against real .NET source → write/extend a
regression test (verified it fails pre-fix by temporarily `git stash`-ing the fix and
re-running, sometimes with a standalone AddressSanitizer repro for the two genuine
memory-safety bugs) → fix → full Debug rebuild + targeted test run → bump the relevant
`plan.sqlite3` row → commit → push. Every commit landed cleanly on `origin/feature/work`.

### Honest scope note — what "9 fixed, audit complete" does and doesn't mean

- **Does mean:** every `.hpp`/`.cpp` file under `System::Threading` has been read end-to-end
  this session specifically hunting for the 8 requested risk categories, and every concrete
  finding from that search has been fixed with a verified-failing-pre-fix regression test.
- **Does not mean:** the older prose "26 moderate + 10 minor" catalogue figure from earlier
  sessions has been itemized and reconciled to zero. That figure was never broken into a
  concrete list, was known-stale in several other namespaces this session already found (Xml,
  Net), and likely included lower-risk items (message-text wording, minor doc-comment gaps,
  API-surface completeness) that were explicitly out of scope for this pass by the user's own
  instruction ("do not touch ordinary minor cleanup"). A full item-by-item reconciliation
  against that old text was not attempted and would need a separate pass if wanted.
- **Deliberately still deferred, not blocked:** `Task::Wait()`/`getResultProperty()`/
  `ValueTask::getResultProperty()` propagate the original exception type unwrapped instead of
  wrapping in `AggregateException` — a documented, intentional simplification from an earlier
  session with ~10 existing tests asserting the unwrapped behavior. Both audit forks were
  explicitly told not to re-report this; it needs a user design decision whenever revisited,
  and does not block other namespaces.
- **Blocked items:** none. All 9 findings from both audit passes were targeted, well-scoped
  fixes; nothing required a design decision beyond what's already documented above.

**Recommended next namespace:** per the last checkpoint before this one, the broader
"everything else, ordinary-severity moderates across all namespaces" sweep (Net core+Sockets
~5 remaining, Net.Http/WebSockets ~4, plus whatever Xml/Text.Json minors remain) is still the
next item in priority order once Threading-specific work is paused. Given this session's
experience that prose-catalogued counts are consistently stale, the same "dispatch a fresh
read-only audit fork per namespace, then fix with verified-failing regression tests" pattern
used for Threading this session is recommended over trusting the old catalogue text directly.

## Session checkpoint (2026-07-11, continued) — Text.Json's 6 remaining dangerous findings: 3 fixed, 3 flagged as bigger lifts (3 commits)

## Session checkpoint (2026-07-11, continued) — Text.Json's 6 remaining dangerous findings: 3 fixed, 3 flagged as bigger lifts (3 commits)

Per NEXT.md's recommended priority, continued from Xml.Linq+XPath into **System.Text.Json**'s
6 previously-unaddressed dangerous-moderate findings (of the original 12; 2 were already fixed
in an earlier session — see that checkpoint further down).

### Fixed (3 findings, commits `0b17016`, `c9a905f` — 1 commit covered 2 related findings)

- **`JsonSerializerOptions.AllowDuplicateProperties` defaulted to `false`**; real .NET's
  backing field defaults to `true` (the sibling `JsonDocumentOptions.AllowDuplicateProperties`
  in this same codebase already correctly defaults `true`, confirming this was an oversight).
- **`JsonSerializerOptions(JsonSerializerDefaults.Strict)` was a silent no-op** — the
  constructor only handled `::Web`. Wired up the two `Strict` effects with a C++ equivalent in
  this port's reduced property set (`UnmappedMemberHandling=Disallow`,
  `AllowDuplicateProperties=false`); `RespectNullableAnnotations`/
  `RespectRequiredConstructorParameters` don't apply (C#-language-feature properties this port
  doesn't expose at all).
- **`JsonNodeOptions::PropertyNameCaseInsensitive` was stored but never consulted** —
  `JsonObject::findIndex()` (the single choke point behind `ContainsKey`/
  `TryGetPropertyValue`/`Add`/`Remove`/`operator[]`/`SetItem`) always compared case-sensitively.
  Verified against `JsonObject.IDictionary.cs`'s `CreateDictionary()`: real .NET's backing
  dictionary comparer affects every one of those operations, not just reads. Reused the
  existing `System::String::Equals(a, b, StringComparison)` helper.

7 new regression tests total across the 3 fixes.

### Flagged, not attempted — genuinely bigger lifts (3 findings)

- **`GetRawText()` reformats numbers instead of returning exact source text** — confirmed via
  a minimal standalone test that nlohmann's `dump()` genuinely loses the original digit
  sequence (`1.50`→`1.5`, `1e2`→`100.0`, and a large integer literal
  `100000000000000000000`→`1e+20`, a different numeric representation entirely). This is a
  direct consequence of `JsonElement`'s documented architecture (backed by nlohmann's parsed
  tree, not a raw-buffer/span design — see the class's own doc comment: "same observable API,
  simpler implementation"). Truly fixing this needs either a parser swap or a raw-span-tracking
  layer added to every parsed node — the same class of lift as the two items below, already
  flagged in an earlier session.
- **`JsonEncodedText::Encode` doesn't validate/pre-escape its input** — unchanged from the
  earlier-session assessment: needs `Utf8JsonWriter::appendEscapedString` extracted into a
  shared, reusable escaping utility first.
- **`AllowTrailingCommas`/`AllowDuplicateProperties` (`JsonDocumentOptions`) validated but
  never enforced during parsing** — unchanged from the earlier-session assessment: nlohmann has
  no native "allow trailing commas" toggle; needs a pre-processing pass or a parser swap.

### Text.Json dangerous-moderate scope: closed for this pass

All 12 originally-catalogued moderate findings are now accounted for: 2 fixed in an earlier
session, 3 fixed this pass, 1 (`AllowDuplicateProperties` default mismatch) was actually the
first item above — 6 total fixed across both sessions — and the remaining 3 are consciously
flagged as bigger lifts, not gaps that were skipped by oversight. Only minor-severity items
remain untouched for this namespace (double formatting/hex-escape casing cosmetics,
`JsonWriterOptions.NewLine` hardcoded, `JsonException` position info, `WriteRawValue`
validation order, several `JsonElement` temporal getters missing, `GetString()` on JSON `null`
— see the "Full findings catalogue" section further down for detail).

**Recommended next session priority:** everything else, ordinary-severity moderates across all
namespaces (Net core+Sockets ~5, Net.Http/WebSockets ~4, Threading ~26+10 minor, per the
earlier per-namespace tables further down in this file — re-verify counts before starting,
several have turned out stale this session) → minors last. The `Task::Wait()`/
`AggregateException` wrapping deferral (see the Threading checkpoint further down) still needs
a user design decision whenever it's revisited; it does not block other namespaces. The 3
Text.Json items and the 1 Xml.Linq `XElement::WriteTo` namespace item flagged as bigger lifts
this session remain open, well-defined follow-ups if ever prioritized.

## Session checkpoint (2026-07-11, continued) — Xml.Linq+XPath dangerous-moderate findings: 10 fixed, 1 flagged as bigger lift (11 commits)

Per NEXT.md's recommended priority, continued from Xml core straight into **System.Xml.Linq +
XPath**'s remaining dangerous-moderate catalogue (12 itemized/condensed findings, criticals
already closed in an earlier session). Same discipline as every prior batch this session.

### Fixed (10 findings, commits `8df416b`..`7413d80`)

- **`XName::Get`** split on the *first* `'}'` instead of the last (a namespace URI may itself
  legally contain `'}'`) and performed no malformed-name validation at all. Verified against
  `XName.cs`: switched to `rfind`, added the two real-.NET validation checks (empty
  `expandedName`; malformed `"{}"`/`"{ns}"` with no local name after the brace) — both throw
  `ArgumentException`. 4 regression tests.
- **`XAttribute` had zero namespace-declaration validation** and **`IsNamespaceDeclaration` was
  entirely missing** — fixed together (same class, same commit). Verified against
  `XAttribute.cs`'s `ValidateAttribute`: enforces the XML Namespaces spec's constraints on
  `xmlns="..."`/`xmlns:prefix="..."` (e.g. the XML namespace URI may only be declared by the
  `xml` prefix; the xmlns namespace URI must never be declared by any prefix). Moved the
  constructor and `setValueProperty()` out of inline header bodies into `.cpp` so both share a
  new `ValidateAttribute()` helper. Added `getIsNamespaceDeclarationProperty()`. 15 regression
  tests.
- **`XAttribute::EscapeValue` didn't escape tab/LF/CR** — per the XML spec's attribute-value
  normalization (§3.3.3), a literal tab/LF/CR in an attribute value is collapsed to a plain
  space on reload; character references are exempt from that normalization. Verified against
  `XmlEncodedRawTextWriter`'s `Tab`/`LineFeed`/`CarriageReturnEntity` (`&#x9;`/`&#xA;`/`&#xD;`).
  2 regression tests including a full write-then-read-back round trip.
- **`XElement::DeepEqualsCore` compared attributes via name lookup** (unordered-set semantics)
  where real .NET's `AttributesEqual` walks both attribute lists in parallel *by position* —
  verified against `XElement.cs`. Two elements with the same attributes in a different order
  are NOT deep-equal in real .NET. 2 regression tests.
- **`XElement` had no `ValidateNode` override at all** (inherited `XContainer`'s no-op default)
  — an `XDocument`/`XDocumentType` could be added as a child element via `Add()`, producing a
  structurally invalid tree. Verified against `XElement.cs`'s `ValidateNode`
  (`ArgumentException`). Corrected the base class's doc-comment, which overclaimed a single
  exception type across all subclasses. 2 regression tests.
- **`XElement::Add(string)` always created a new `XText`** even when the last child was already
  a plain (non-CDATA) text node. Verified against `XContainer.cs`'s `AddString()`: real .NET
  merges into the existing trailing text node's `Value`; an empty string is a genuine no-op
  (matches `AddString`'s `if (s.Length > 0)` guard). 3 regression tests.
- **`XDocument::ValidateNode` used the wrong exception type and over-rejected whitespace text**
  — verified against `XDocument.cs`'s `ValidateNode`/`ValidateString`: real .NET throws
  `ArgumentException` for a fundamentally wrong node *kind* (CDATA, nested `XDocument`), and
  `InvalidOperationException` only for a structurally valid kind that conflicts with the
  document's *current state* (already has a root/doctype). Whitespace-only text (e.g.
  indentation between top-level nodes) is legal — the old `default:` case rejected every
  top-level `XText` unconditionally. Updated 1 pre-existing test, added 3 new ones.
- **`XDocument::WriteTo` skipped `WriteStartDocument()` without an explicit declaration and
  never called `WriteEndDocument()`** — verified against `XDocument.cs`'s `WriteTo()`: real
  .NET always calls both as a matched pair regardless of whether an `XDeclaration` was set.
  Note: `XDocument::ToString()` is unaffected (separate `SerializeTo()` code path). 1
  regression test.
- **XPath `number()` accepted scientific/exponent notation** — `ParseXPathNumberLiteralString`
  used `std::from_chars`' default `general` format, which parses `"1e2"` as `100` instead of
  correctly yielding `NaN`; XPath 1.0's `Number` production (§3.4) has no exponent syntax at
  all. Fixed by passing `std::chars_format::fixed`. This function is also the shared
  numeric-comparison path from an earlier session's relational-operator fix, so this also
  corrects number-valued node-set comparisons against an exponent-notation string operand. 3
  regression tests.

### Flagged, not attempted — genuinely bigger lift (1 finding)

- **`XElement::WriteTo` silently drops the element's namespace URI** — real .NET's
  `XElement.WriteTo` routes through a full internal `ElementWriter` subsystem that resolves
  namespace prefixes against ancestor scope and auto-generates `xmlns:pN` declarations; this
  port's `XmlWriter` has no prefix-aware `WriteStartElement`/`WriteAttributeString` overloads at
  all. Already documented as a deliberate, previously-reasoned simplification in the existing
  code comment (see `XElement.cpp`'s `WriteTo()`) — a real feature addition, not a targeted bug
  fix. A partial fix (e.g. auto-emitting a default `xmlns="..."` attribute) risks introducing
  new namespace-scoping bugs for descendant elements, so left alone.

### Xml.Linq+XPath dangerous-moderate scope: closed

Only minor-severity items remain untouched for this namespace (see the "Full findings
catalogue" section further down): `XName` constructors skip NCName validation;
`XAttribute.EmptySequence` missing; `XDeclaration.ToString` version-omission difference;
`XDocumentType` skips name validation; `DeepEqualsCore` skips Comment/PI nodes (matches a stale
doc comment); XPath `string-length()` uses byte length not character count. Also still open,
separately: "a large set of documented XLinq tree-editing API is entirely absent"
(`AddBeforeSelf`, `SetAttributeValue`, ~20 conversion operators, etc.) — a compile-time API-
surface gap, not runtime misbehavior, flagged in the original catalogue as too large for a
targeted fix.

**Recommended next session priority (unchanged from before this batch):** Text.Json's 6
remaining dangerous findings (`JsonEncodedText::Encode`/`AllowTrailingCommas` refactors,
flagged as bigger lifts in an earlier checkpoint — re-check whether they're still accurate
before starting) → everything else, ordinary-severity moderates → minors last.

## Session checkpoint (2026-07-11) — Xml core dangerous-moderate findings: 6 fixed, 2 already-fixed/stale (6 commits)

Per NEXT.md's own recommended priority from the previous session, worked through **System.Xml
core**'s remaining dangerous-moderate catalogue (see "Full findings catalogue" further down —
13-ish itemized/condensed findings). Same discipline as prior batches: verify against real .NET
source in `/rv/tmp/runtime/src/libraries/` → write/extend a regression test → fix → build clean
(Debug **and** a Release/library-only rebuild) → run the targeted suite → bump `plan.sqlite3` →
commit, one fix per commit.

### Fixed (6 findings, commits `07577d3`..`08d9318`)

- **`XmlReader` buildEvents() had four distinct bugs**, all in one commit since they're the same
  function: CDATA sections reported as plain `Text` (tinyxml2's `XMLText::CData()` flag never
  consulted); every `<?target data?>` parsed uniformly as `XmlDeclaration` with a hardcoded
  `"xml"` name (tinyxml2 doesn't distinguish PI from the real declaration) — now splits the
  target token and only treats an exact `"xml"` target as the declaration; DOCTYPE parses as
  tinyxml2's `XMLUnknown`, which had no branch at all and silently vanished from the event
  stream — now surfaces as `XmlNodeType::DocumentType`; `isEmptyElement` was computed from
  `!FirstChild()`, so `<a></a>` was indistinguishable from `<a/>` and silently lost its
  `EndElement` event — now uses tinyxml2's `ClosingType() == CLOSED`. 8 regression tests.
- **`XmlWriter::ToString()`/`Flush()` ignored `XmlWriterSettings.Indent`**, always
  pretty-printing via tinyxml2's default `XMLPrinter(compact=false)` — real .NET's `Indent`
  defaults to `false` (compact). Added an optional `XmlWriterSettings` param to
  `Create()`/`CreateToString()`, wired through to `compact=!settings.Indent`.
- **`WriteComment`/`WriteProcessingInstruction`/`WriteCData` wrote embedded terminator
  sequences raw** (`--`, `?>`, `]]>`), producing literally malformed/corrupted markup. Verified
  against `XmlEncodedRawTextWriter.WriteCommentOrPi`/`WriteCDataSection`: real .NET does **not**
  throw for any of these — it self-heals (inserts a protective space; splits CDATA sections
  around the embedded terminator) so content round-trips unchanged. **This corrects this
  session's own audit catalogue**, which described the gap as "skip well-formedness validation"
  implying validate-and-throw — the real source has no such throw path. Added
  `sanitizeCommentText`/`sanitizeProcessingInstructionText`/`sanitizeCDataText` helpers
  replicating the self-healing behavior. 4 regression tests including a full write-then-read
  round-trip.
- **`XmlNode::Normalize()` only merged adjacent text among direct children**, never recursing
  into descendant elements — verified against `XmlNode.cs`'s `Normalize()`, which explicitly
  recurses (`case XmlNodeType.Element: crtChild.Normalize(); goto default;`). Extracted the
  existing merge loop into a `NormalizeNative()` free-function helper that recurses.
- **`XmlDocument`'s node-creation factories skipped XML-Name validation entirely** —
  `CreateElement`/`CreateAttribute`/`CreateEntityReference`/`CreateProcessingInstruction`
  accepted any string with zero validation. Verified each validates *differently* per node kind
  in real .NET (not uniformly): `CreateElement`/`CreateAttribute` reuse this port's existing
  `XmlConvert::VerifyName` (matches `XmlDocument.CheckName`); `CreateEntityReference` only
  rejects a name starting with `'#'` (matches `XmlEntityReference.cs` — entity names are
  otherwise unconstrained); `CreateProcessingInstruction` only rejects an empty target (matches
  `XmlProcessingInstruction.cs` — no NCName check at DOM-construction time). 7 regression tests.

### Already fixed / stale catalogue entries (2 findings, no code change)

- **`XmlNamespaceManager::AddNamespace` reserved-prefix validation** — already throws
  `ArgumentException` for `xml`/`xmlns` misuse exactly per `XmlNamespaceManager.cs`, with
  existing test coverage (`AddNamespace_XmlnsPrefix_Throws`,
  `AddNamespace_XmlPrefixWithWrongUri_Throws`). Fixed in an earlier session; this catalogue
  entry was stale.
- **`XmlDeclaration` version/standalone validation** — likewise already correct with existing
  tests (`CreateXmlDeclaration_InvalidVersion_Throws`, `CreateXmlDeclaration_InvalidStandalone_Throws`,
  `SetStandaloneProperty_InvalidValue_Throws`). The catalogue's "XmlDeclaration ... skip
  version/standalone ... validation" framing was stale by the time this session started.

### Xml core dangerous-moderate scope: closed

Both remaining "highlights" items not explicitly itemized above (self-closing/EndElement
detection, and the `XmlWriter` well-formedness gap) were folded into the fixes above since they
shared a file/function with an itemized finding. Xml core's dangerous-moderate catalogue is now
fully processed for this pass — only minor-severity items remain untouched (see the "Full
findings catalogue" section further down: message-text formatting, line/position info loss,
`XmlResolver` relative-path gap, prefix-shadowing nondeterminism, `XmlDeclaration.Value`
leading-space bug, `XmlAttributeCollection` insertion-order degrade, apostrophe over-escaping,
stray PI trailing space).

**Recommended next session priority (unchanged from before this batch):** Xml.Linq+XPath
dangerous-moderate findings → Text.Json's 6 remaining dangerous findings → everything else,
ordinary-severity moderates → minors last.

## Session checkpoint (2026-07-10, continued again) — Threading dangerous-moderate findings: 10 fixed, 1 deliberately deferred (10 commits)

Scope for this session, per explicit instruction: finish Threading's remaining dangerous-despite-
moderate findings before touching Xml core, and do not start the Text.Json
`JsonEncodedText::Encode`/`AllowTrailingCommas` refactors flagged as bigger lifts in the previous
checkpoint. All 10 fixes below follow the same discipline as prior batches: verify against real
.NET source in `/rv/tmp/runtime/src/libraries/` → write/extend a regression test → fix → build
clean → run the targeted test suite → bump the corresponding `plan.sqlite3` row's `updated_at` →
commit, one coherent fix per commit.

**Fresh audit note:** this file's own Threading catalogue (see the wave-3 "Full findings
catalogue" section further down) was condensed prose for the moderate/minor tier, not an itemized
list, and referenced a prior session's agent transcript that isn't accessible in this session. A
fresh audit fork re-derived concrete findings by reading current source against the .NET
reference; several catalogued items turned out already-fixed by earlier sessions and were skipped.

### Fixed (10 findings, commits `77fc618`..`13ab167`)

- `Semaphore(initialCount, maximumCount)` threw the same `ArgumentOutOfRangeException` for both
  `initialCount < 0` and `initialCount > maximumCount` — real .NET throws a plain
  `ArgumentException` for the latter case (`initialCount` is a valid non-negative value, just
  inconsistent with `maximumCount`). Split into three distinct checks matching `Semaphore.cs`.
- `ReaderWriterLock::ReleaseReaderLock/ReleaseWriterLock` without ownership threw
  `SynchronizationLockException` — real .NET throws `ApplicationException` here specifically
  (unlike every other lock type in this namespace), matching `ReaderWriterLock.cs`.
- `Barrier::Dispose()` was a true no-op — added a `disposed_` flag, `ThrowIfDisposed()` guards on
  `SignalAndWait`/`AddParticipant`/`RemoveParticipant`, and `Dispose()` now actually marks the
  instance disposed (still respecting the existing post-phase-action guard), matching
  `Barrier.cs`.
- `Timer::Change(dueTime, period)` treated `dueTime = Timeout.Infinite (-1)` as "fire immediately"
  because `std::chrono::wait_for` treats a negative duration as already-expired — rewrote the
  timer thread to block on a condition variable whenever `dueTime == -1`, matching
  `TimerQueueTimer.cs`'s `dueTime == Timeout.UnsignedInfinite` → "not scheduled" semantics (this
  applies both to a never-started timer and to `Change(-1, ...)` pausing an active one). Bonus
  fix: single-shot timers are now re-armable via a later `Change()` call, matching real .NET.
- `PeriodicTimer` had no period validation and busy-looped at 100% CPU for `Zero`/negative
  periods — added the same `>= 1 && <= 0xFFFFFFFE` validation as `PeriodicTimer.cs`, plus
  `Timeout.InfiniteTimeSpan` support (blocks until `Dispose()` rather than never ticking via a
  hot loop).
- `AutoResetEvent::WaitOne(intcs milliseconds)` had no timeout validation, unlike every sibling
  wait-handle type in this namespace — added `WaitHandle::ValidateTimeout()`.
- `Lock::TryEnter(TimeSpan)` passed `Timeout::InfiniteTimeSpan` straight through as a negative
  `std::chrono` duration, which `try_lock_for` treats like a non-blocking `try_lock()` — the one
  call site the `-1`/Infinite special-casing pattern (already applied everywhere else this
  session) had missed. Now special-cased like the `intcs` overload.
- `Channel<T>::WaitToReadAsync()`/`WaitToWriteAsync()` never inspected `closeError` — closing a
  channel with an exception and an empty queue returned `false` (as if gracefully closed) instead
  of faulting the task, silently discarding the real error on the primary read/write path (it was
  only observable via the separate `getCompletionProperty()` task). Verified against
  `UnboundedChannel.cs`; both methods now rethrow `closeError` when present.
- `Parallel::For/ForEach/Invoke` called `future.get()` per future with no `try`/`catch` — the
  first exception observed escaped **unwrapped**, and every other future's exception was silently
  discarded when its `std::future` was destroyed unobserved. Real .NET always aggregates every
  exception raised into a single `AggregateException`, even for a single failure. Added a shared
  `waitAllCollectingExceptions()` helper; scheduling still runs to completion after a failure
  rather than stopping early like .NET's internal self-replicating-task algorithm (an efficiency
  difference, not a correctness one — out of scope for this fix).
- `SpinLock` was a bare `std::atomic_flag` with the `enableThreadOwnerTracking` constructor
  parameter fully ignored, `getIsHeldProperty()` hard-coded to always return `false`, no
  re-entrancy detection, and `Exit()` releasing unconditionally regardless of which thread called
  it — silently permitting lock-corruption bugs (double-release, cross-thread release) that real
  .NET surfaces as exceptions. Rewrote with an atomic owner-thread-id (tracking enabled) or atomic
  held-flag (tracking disabled) and a bounded spin/yield loop, matching `SpinLock.cs`'s public
  contract: `LockRecursionException` on same-thread re-entry, `SynchronizationLockException` on
  `Exit()` without ownership, `ArgumentException` when `lockTaken` isn't pre-initialized to
  `false`, `InvalidOperationException` from `IsHeldByCurrentThread` when tracking is disabled.
  Also added the missing `TryEnter(intcs, bool&)`/`TryEnter(TimeSpan, bool&)` timeout overloads
  and `getIsThreadOwnerTrackingEnabledProperty()`. `TryEnter`'s C++-friendly `bool` return (an
  existing convention in this port, unlike .NET's `void` + out-param) was preserved.

### Deliberately deferred — risky design boundary, not fixed this session

- **`Task::Wait()`/`getResultProperty()`/`ValueTask::getResultProperty()` don't wrap thrown
  exceptions in `AggregateException`.** Real .NET's `Task.Wait()` always wraps a faulted task's
  exception in `AggregateException`; this port propagates the original exception type unwrapped.
  **Why deferred:** the current (unwrapped) behavior is called out in `Task.hpp`'s own code
  comments as "an established, deliberate simplification" from a prior session, and ~10 existing
  tests in `TasksTests.cpp` (lines 96, 107, 114, 119, 145, 245, 251, 328, 471, 479) explicitly
  assert the unwrapped exception type propagates. Changing this is a real design decision, not a
  targeted fix: wrap always and rewrite all ~10 tests plus audit every internal
  Task-exception-catching call site, or leave it as a permanent documented deviation, or add a new
  opt-in wrapping API alongside the existing one. Per this session's explicit instruction to stop
  at risky design boundaries rather than rush, this needs a documented design decision from the
  user before any future attempt — **no design has been written yet.**
- `Parallel`'s exception aggregation (fixed this session, see above) does **not** conflict with
  this deferral — `Parallel.hpp` had no such prior "deliberate simplification" precedent or
  existing tests asserting unwrapped-exception behavior, so it was safe to fix directly.

### Wave-3 catalogue: Threading now resolved for this session's dangerous-moderate scope

Threading's dangerous-moderate findings are now resolved one way or another: 10 fixed, 1
explicitly deferred with a documented rationale (above) pending a user design decision. Per this
session's instruction, Xml core dangerous-moderate findings are next.

**Recommended next session priority:** Xml core dangerous-moderate findings → Xml.Linq+XPath
dangerous-moderate findings → Text.Json's 6 remaining dangerous findings (see the checkpoint
below for the `JsonEncodedText::Encode`/`AllowTrailingCommas` refactor scope — still not started,
still flagged as bigger lifts) → everything else, ordinary-severity moderates → minors last. The
`Task`/`AggregateException` deferral above needs a user decision whenever it's revisited; it does
not block other namespaces.

## Session checkpoint (2026-07-10, continued again) — dangerous-despite-moderate wave-3 findings: 24 fixed across 4 namespace slices (21 commits)

Per explicit instruction this session, worked exclusively through wave-3 **moderate**-severity
findings (all criticals were already closed — see the checkpoint below), prioritizing findings
matching these criteria over ordinary cleanup: silent wrong behavior; null/throw mismatches vs.
real .NET; missing parent/cycle guards; wrong exception types; doc-comment/implementation
mismatches; platform-specific behavior leaking through a public API.

**Discipline followed for every item:** verify against real .NET source in
`/rv/tmp/runtime/src/libraries/` → write/extend a regression test → fix → build clean → run the
targeted test suite → bump the corresponding `plan.sqlite3` row's `updated_at` → commit. All 21
commits are individually verified against a specific cited `.NET` source file/method in their
commit messages.

### Net core + Sockets (7 findings fixed, commits `b19bab6`..`843c3d8`,`81b0a46`)

- `Socket::Send/Receive/SendTo/ReceiveFrom` cast `SocketFlags` directly to native `int` flags
  with no translation (correct only by coincidence for `OutOfBand`/`Peek`/`DontRoute`) — added
  `nativeSocketFlags()` translating each individually on POSIX, verified against
  `pal_networking.c`'s `ConvertSocketFlagsPalToPlatform`.
- `IPAddress::TryParse` could throw uncaught `std::out_of_range` (via `std::stoul`) on IPv6
  scope-ID overflow, violating `TryParse`'s never-throws contract — switched to
  `std::from_chars`.
- `IPAddress::IsLoopback` missed the IPv4-mapped form `::ffff:127.0.0.1` — added the second
  equality check real .NET's `IsLoopback` performs.
- `IPAddress::GetHashCode()` only combined the first 64 bits of an IPv6 address — now combines
  all 128 bits, matching `IPAddress.cs`.
- `IPEndPoint(longcs, intcs)` silently truncated an out-of-`uint32_t`-range address instead of
  throwing — added a validated-narrow helper matching `IPAddress.cs`'s `IPAddress(long)` ctor.
- `WebUtility::UrlEncode`'s safe-character set didn't match .NET's `s_safeUrlChars`
  (`-_.!*()`, not `-_.~`) — fixed the character set; one pre-existing test encoded the old
  wrong set and was updated.
- `TcpListener::Start()` hardcoded `listen(fd, 5)` instead of requesting the platform max
  (`INT32_MAX`, matching `TCPListener.cs`'s default `SocketOptionName.MaxConnections`).

### Net.Http + WebSockets + NetworkInformation (5 findings fixed, commits `986eead`..`27845a7`)

- `HttpClient::parseUrl` threw `std::invalid_argument` for every failure path — now throws
  `System::UriFormatException` (malformed URI) or `System::NotSupportedException` (unsupported
  scheme), matching real .NET's `Uri` construction failure modes.
- `CacheControlHeaderValue`'s `max-age`/`s-maxage` parser accepted a leading `-` and silently
  wrapped on overflow when narrowing `long`→`intcs` — added an all-digit pre-check and explicit
  bound, matching `HeaderUtilities.cs`'s `int.TryParse(..., NumberStyles.None, ...)`.
- `ClientWebSocketOptions::AddSubProtocol`'s duplicate check was case-sensitive — switched to
  `OrdinalIgnoreCase`, matching `ClientWebSocketOptions.cs`.
- `NetworkInterface::GetIsNetworkAvailable()` excluded only `Loopback`, not `Tunnel` — added the
  second exclusion, matching `NetworkInterfacePal.Linux.cs`. (No dedicated test: depends on real
  OS interface topology this port can't mock; verified via full-suite pass instead.)
- (Verified, no fix needed: `q=` quality-value parsing across all 3 header-value files already
  correctly try/catch-guards `std::stod`, and NaN/Infinity are naturally rejected by the
  existing range check — this catalogued finding was already stale.)

### System.IO core (10 findings fixed, commits `d7ab9e6`..`f665128`)

- `File::Delete` silently deleted an empty directory (`std::filesystem::remove()` also removes
  directories) — real .NET's `unlink()`-based `File.Delete` can never remove a directory; now
  throws `IOException`.
- `RandomAccess::Write` issued one `pwrite()`/`WriteFile()` call and silently dropped any bytes
  a "short write" didn't cover — now loops until the whole buffer is written, matching
  `RandomAccess.Unix.cs`'s `WriteAtOffset`.
- `Directory::GetFiles(path, "*.*")` literally translated to a regex requiring a literal `.`,
  excluding every extensionless file — `"*.*"` now special-cased to match everything, matching
  `FileSystemName.cs`'s legacy DOS 8.3 compatibility behavior.
- `FileSystemInfo`'s `CreationTime`/`LastAccessTime`/`LastWriteTime` (the "local" properties)
  returned the UTC value verbatim with no timezone conversion — routed through the existing
  `TimeZoneInfo::ConvertTimeFromUtc/ConvertTimeToUtc(..., TimeZoneInfo::Local())`.
- `FileStream` threw a generic `IOException` (or, for `Open`/`Truncate`, unconditionally
  `FileNotFoundException`) when the *parent* directory was missing — now throws
  `DirectoryNotFoundException`, matching `Interop.IOErrors.cs`'s Windows-compatibility fallback.
- `MemoryStream::Read()` returned `0` (indistinguishable from EOF) for a null buffer or
  negative offset/count instead of throwing.
- `MemoryStream::Close()` cleared the buffer and reset position, contradicting its own doc
  comment ("no-op for MemoryStream") and `MemoryStream.cs`'s `Dispose(bool)`, which explicitly
  preserves both so `GetBuffer()`/`ToArray()` keep working — now a true no-op.
- `UnmanagedMemoryStream` threw `NotSupportedException` instead of `ObjectDisposedException`
  after `Close()` — added an explicit `isOpen_` check ahead of the `CanRead`/`CanWrite` checks
  in `Read`/`Write`/`SetLength`, matching `UnmanagedMemoryStream.cs`'s `EnsureNotClosed()`
  ordering.
- `BinaryWriter::Close()`/destructor did nothing at all when `leaveOpen=true` instead of
  flushing — now calls `Flush()` on the underlying stream, matching `BinaryWriter.cs`'s
  `Dispose(bool)`.
- `StreamReader`/`StringReader::ReadLine()` only stopped at `'\n'`, silently merging a lone
  `'\r'`-terminated line into the next one — both now treat `'\r'` and `'\n'` as interchangeable
  terminators (consuming a following `'\n'` after `'\r'` as one CRLF terminator), matching
  `StreamReader.cs`/`StringReader.cs`.
  - **Also fixed 2 pre-existing tests** that broke as a side effect of the `MemoryStream::Close()`
    fix above (they used buffer-clearing as a proxy for "was Close() called on the underlying
    stream," which stopped working once Close() correctly stopped clearing it) — rewrote using a
    new `FlushTrackingStream` test double that observes `Close()`/`Flush()` calls directly.

### System.Text.Json (2 of 8 dangerous findings fixed, commits `3e74b43`, `f12da05`)

- `Utf8JsonWriter::WriteNumberValue(double)` silently serialized NaN/±Infinity as the JSON
  literal `null` (nlohmann's documented behavior) instead of throwing — now throws
  `System::ArgumentException`, matching `JsonWriterHelper.cs`'s `ValidateDouble`.
  `WriteNumber(name, double)` validates before writing the property name, so failure is atomic.
- `JsonValue`'s `Get*()` accessors threw `FormatException` for a wrong-kind access instead of
  `InvalidOperationException` — matches `JsonValueOfElement.cs`'s `GetValue<T>`.

**Not attempted — genuinely bigger lifts, flagged rather than rushed:**
- `JsonEncodedText::Encode(string)` doesn't validate or escape its input at all (defeating the
  type's entire purpose — a caller-embedded unescaped `"` would corrupt JSON output if this
  pre-encoded text is ever written verbatim). The correct fix requires extracting
  `Utf8JsonWriter::appendEscapedString` (currently private, writes into the writer's own
  buffer) into a shared, reusable escaping utility — a real refactor, not a targeted fix.
- `AllowTrailingCommas`/`AllowDuplicateProperties` (`JsonDocumentOptions`) are validated as
  constructor args but never actually enforced during parsing, because the underlying parser
  (nlohmann/json) has no native "allow trailing commas" toggle — would need either a
  pre-processing pass over the JSON text or a parser swap.
- `AllowDuplicateProperties` default mismatch, `JsonSerializerOptions(Strict)` no-op,
  `PropertyNameCaseInsensitive` unconsulted, `GetRawText()` reformatting — not yet started.

### Wave-3 catalogue: moderate/minor status after this session

| namespace | moderate done | moderate remaining (approx.) | minor done |
|---|---|---|---|
| Net core + Sockets | 7 | ~5 (validation-only "ordinary" items, e.g. Socket setter validation, TcpClient/UdpClient IPv4-only) | 0 |
| Net.Http/WebSockets/NetworkInformation | 5 | ~4 (HttpResponseMessage status-range validation, header getters, ClientWebSocket message-type validation) | 0 |
| IO core | 10 | 0 known dangerous; several "ordinary" gaps remain (StreamReader/StreamWriter ctor validation) | 0 |
| IO.Compression/Hashing | 6 (done in an earlier session phase) | 0 dangerous | 3 (untouched) |
| Text.Json | 2 | 6 (see "not attempted" above) | 0 |
| Threading | 3 (done in an earlier session phase — SynchronizationContext, disposed-race, LIFO) | untouched this session (~26 moderate + "and more" per the original audit, exact count not preserved — see this file's Threading checkpoint) | 0 |
| Xml core | 1 (done in an earlier session phase — XmlNode ancestor-cycle/cross-doc) | untouched this session (~12 moderate) | 0 |
| Xml.Linq+XPath | 0 | untouched this session (~12 moderate, ~9 flagged dangerous per this session's triage) | 0 |

**Caveat on the "remaining" counts above:** they come from a fork-agent triage pass over this
file's own "Full findings catalogue" section this session, which is itself explicitly
non-exhaustive in places (several namespace "highlights" paragraphs are condensed prose, not
itemized lists) and Threading's own remaining-findings text ends "...and more" with the detail
not preserved in this file. Treat these as lower bounds, not exact counts. A fresh audit pass
would be needed for a precise number.

**Blocked/needs-user items:** none from this session's actual fixes. Two items were
consciously deferred as "bigger lifts" (see Text.Json section above) rather than blocked —
they're well-defined, just larger in scope than a single targeted fix.

**Recommended next session priority:** Threading's remaining moderates (this namespace's first
3 "dangerous" items were the highest-value ones and are already done; check what's left against
current source, since several catalogued items in other namespaces turned out stale this
session) → Xml core → Xml.Linq+XPath → Text.Json's 6 remaining dangerous items → everything
else, ordinary-severity moderates → minors last.

## Session checkpoint (2026-07-10, continued again) — build-verification gap: library-only Release build with GCC 14 was never actually tested, and failed

**This corrects every earlier "full clean rebuild verified (0 errors/0 warnings)" claim in this
file.** Those claims were true only for the default build configuration this session always
ran — `cmake --build build --parallel 4` with no `CMAKE_BUILD_TYPE` set (so no `-O2`/`-O3`) and
`SHARP_RUNTIME_BUILD_TESTS=ON` (the CMake default). That configuration was never representative
of a downstream consumer building just the library in Release mode, and it turns out an
optimization-dependent GCC diagnostic only fires at `-O2`+ — so a real build failure sat
undetected all session.

**Compiler:** `gcc (Debian 14.2.0-19) 14.2.0` / `g++` same version (`gcc --version` / `g++
--version`), CMake 3.31.6, on this checkout's Linux host.

**Exact commands run, in order:**
```
cmake -S . -B build_no_tests -DSHARP_RUNTIME_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build_no_tests --parallel 8
```
This is a **library-only** build (`SHARP_RUNTIME_BUILD_TESTS=OFF` — no googletest, no test
target at all) at `-DCMAKE_BUILD_TYPE=Release`, which is what actually enables `-O2`/`-O3`; the
project's `-Wall -Wextra -Werror` flags (`CMakeLists.txt`) are unconditional across build types,
but several of the warnings they turn fatal are themselves optimization-dependent GCC analyses
that silently don't run at `-O0`, which is what the session's habitual `cmake --build build`
(no explicit `CMAKE_BUILD_TYPE`) actually built at all session — explaining how this went
undetected through every prior "clean build" checkpoint.

**Initial failure (reproduced before any fix):**
```
src/System/Net/NetworkInformation/Ping.cpp:160:20: error: 'void* memset(void*, int, size_t)'
  offset [0, 6] is out of the bounds [0, 0] [-Werror=array-bounds=]
src/System/Net/NetworkInformation/Ping.cpp:171:20: error: 'void* memset(void*, int, size_t)'
  offset [0, 6] is out of the bounds [0, 0] [-Werror=array-bounds=]
```
Root cause: `sendPingCore` built the ICMP packet by `reinterpret_cast`ing `packet.data()` (a
`uint8_t*` into a `std::vector<uint8_t>`) to `icmp6_hdr*`/`icmphdr*` and `memset`ing through
that pointer. At `-O2`, GCC's array-bounds analysis cannot prove the vector's dynamic storage
is large enough through that cast and treats the `memset` as writing past a zero-size object.
**Fix:** build the header in a local, properly-typed `icmp6_hdr`/`icmphdr` object
(value-initialized with `{}`, so every field starts zero — no `memset` needed at all), populate
its fields normally, then `std::memcpy` the fully-formed struct into `packet.data()`. For the
IPv4 branch, the checksum needs the payload already copied in, so the local header is populated
twice: once with `checksum = 0` to compose the packet for checksum calculation, then again with
the real checksum after `internetChecksum()` runs. No `reinterpret_cast` of `packet.data()`
remains; no `-Werror` suppression pragma was added. Verified: `PingTests.Send_Loopback_Succeeds`
and `Send_CustomBuffer_EchoedBack` (which do a real ICMP round-trip against loopback and check
the echoed payload byte-for-byte) still pass, confirming the packet bytes are unchanged.

**Second failure, uncovered only after the first was fixed** (same command, same run):
```
.../new_allocator.h:172:33: error: 'void operator delete(void*, std::size_t)' called on pointer
  '<unknown>' with nonzero offset [2, 9223372036854775807] [-Werror=free-nonheap-object]
```
4 instances, all inlined into `src/System/Text/Encoding.cpp` from
`UnicodeEncoding::GetBytes`/`UTF32Encoding::GetBytes` (both header-only, in
`include/System/Text/UnicodeEncoding.hpp` / `UTF32Encoding.hpp`). This is GCC 14's documented
`-Wfree-nonheap-object`/`-Warray-bounds` false-positive class with `vector::reserve()` followed
by `push_back()`/`emplace_back()`, when the growth-reallocation branch gets inlined through a
virtual call and a capturing lambda at `-O2`+ — GCC's escape analysis loses track that the
`operator delete` inside `_M_realloc_append`'s `_Guard` destructor and the `operator new` from
`reserve()` moments earlier are the same allocation. Verified this is a false positive, not a
real bug, by hand-proving both `reserve()` calls are legitimate upper bounds before touching
anything: `UnicodeEncoding::GetBytes` emits at most 2 output bytes per consumed UTF-8 input
byte on every code path (including the malformed-input fallback, which consumes 1 byte and
emits one 2-byte BMP replacement unit) — so `s.size() * 2 + 2` (`+2` for the optional BOM)
can never be exceeded; `UTF32Encoding::GetBytes` emits exactly 4 bytes per decoded code point
and there are at most `s.size()` code points, so `s.size() * 4 + 4` can never be exceeded
either. Since the growth-reallocation branch is provably unreachable at runtime but the
compiler can't prove that statically, **fixed by eliminating the `reserve()` + `push_back()`
code shape entirely**: both `GetBytes()` overloads now `resize()` the output vector up front to
that same proven-sufficient bound, write through direct indexing (`out[pos++] = ...`), then
`resize(pos)` down to the actual length at the end (a shrink, which never reallocates). No
`-Werror` suppression pragma was added; this is a real code-structure change, not a warning
workaround. `UTF32Encoding::writeUnit`'s signature changed from `(vector&, uint32_t)` to
`(vector&, size_t& pos, uint32_t)` to support indexed writes; both of its call sites (within
the same class) were updated. Verified: `UnicodeEncodingTests` (12/12) and `UTF32EncodingTests`
(9/9) — including exact-output-size assertions (`GetBytes_WithBOM_SizeIs8ForSingleChar`,
`GetBytes_NoBOM_SizeIs4ForSingleChar`) — still pass unchanged.

**Verification after both fixes, run in this exact order:**
1. `cmake -S . -B build_no_tests -DSHARP_RUNTIME_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release` +
   `cmake --build build_no_tests --parallel 8` — **library-only Release build, exit 0, 0
   errors, 0 warnings** (`grep -c "error:"` and `grep -c "warning:"` on the full build log both
   return `0`). `libSHARP_RUNTIME.a` was produced.
2. `vendor/googletest` submodule **is present and populated** in this checkout (`git submodule
   status` shows it checked out at `7e2c425d`, not an empty/missing directory) — so per the
   ticket's instructions, the full test build and suite were run, not skipped:
   `rm -rf build && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` (the session's normal default
   config — tests **on**, no explicit optimization level) + `cmake --build build --parallel 8`
   — 0 errors, 0 warnings. `./build/SharpRuntimeTests` — **11262/11262 tests passed**, 0
   failures (same count as before this fix — no regression tests were added for this change
   since it's a pure code-structure/compiler-diagnostic fix with no behavioral change; existing
   `PingTests`/`UnicodeEncodingTests`/`UTF32EncodingTests` already cover the touched code and
   all still pass byte-for-byte).
3. **Not yet run in this checkpoint:** a Release-mode build *with* tests enabled
   (`-DCMAKE_BUILD_TYPE=Release` and `SHARP_RUNTIME_BUILD_TESTS=ON` together), and Debug-mode
   library-only. Only the two configurations explicitly named in the reproduction request were
   verified. If a future session wants "every `(BUILD_TYPE, BUILD_TESTS)` combination verified
   clean," that is still open — flagging so a future "clean build" claim doesn't overclaim
   coverage the way this one implicitly did.

**Process takeaway, carried forward:** this session's recurring `cmake --build build --parallel
4` command (per `CLAUDE.md`'s own "Useful commands" section) never sets `CMAKE_BUILD_TYPE`, so
it never enables `-O2`/`-O3`, so it can never catch an optimization-dependent `-Werror`
diagnostic like either of the two above. Every future "clean build" claim in this file should
state which configuration(s) were actually run rather than an unqualified "full clean rebuild
verified" — that phrasing is what let this exact gap go unnoticed. Per the requesting
instructions: **no further P2 ticket work should proceed from a fresh context until this
checkpoint's fix is confirmed present** (it is, as of this checkpoint — both fixes are
committed).

## Session checkpoint (2026-07-10, continued again) — wave-3 catalogue: genuinely EVERY critical now fixed, including Text.Json (56/56)

*Branch: `feature/work`, HEAD `e1f3d9d` — 11262 tests passing (up from 11243 at the top of the
XmlNode checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

A status review at the start of this phase caught that the "every namespace's criticals are
fixed" claim from two checkpoints ago (search this file for "genuinely complete for criticals
now") **silently excluded `System.Text.Json`** — only 3 of its 7 catalogued criticals had
actually been fixed (writer escaping, `Utf8JsonWriter`/`JsonDocument` `MaxDepth`, done earlier
as "wave-3 priority item 4"). The other 4 were still open. Fixed all 4 this phase:

1. **`JsonElement::TryGetInt32`/`TryGetInt64` UB via `double` round-trip** — verified against
   `JsonDocument.cs`'s `TryGetValue(out int)`/`TryGetValue(out long)`: real .NET parses the
   original number *text* directly as an integer, never through a floating-point intermediate.
   This port called `get<double>()` unconditionally then `static_cast` to `intcs`/`longcs` — UB
   when casting an out-of-range double to a signed integer, and silently accepted float
   literals (`"1.0"`, `"2e1"`) whose value happened to be integral, where real .NET's
   text-based parser rejects them outright. Rewrote both to use nlohmann's native
   integer/unsigned accessors, rejecting `number_float` nodes and range-checking before the
   narrowing cast. `GetInt32`/`GetInt64` now delegate to the `Try*` variants (matching
   `GetInt32.cs`'s own delegation), which also fixed the separately-catalogued moderate
   "`GetInt32`/`GetInt64` too lenient" finding as a side effect. 5 regression tests. Commit
   `0cb6a73`.
2. **`JsonElement::GetProperty`/`TryGetProperty` always threw `KeyNotFoundException`, even on a
   non-object element** — verified against `JsonDocument.TryGetProperty.cs`'s
   `TryGetNamedPropertyValue`: real .NET throws `InvalidOperationException` when the element's
   `ValueKind` isn't `Object`, even for the `Try`-prefixed overload; only a genuinely-missing
   key returns `false` without throwing. Fixed `TryGetProperty` to check kind first via the
   existing `require()` helper. 3 regression tests. Commit `0cb6a73`.
3. **`JsonObject`/`JsonArray` `Add`/`SetItem`/`Insert` had no "already has a parent" or cycle
   check** — verified against `JsonNode.cs`'s internal `AssignParent`/`DetachParent`: real .NET
   throws `InvalidOperationException` both when a node being attached already has a parent
   (would silently orphan whichever container actually still holds it — dangling `parent_`
   pointer there) and when attaching would create a cycle. Replaced the unconditional
   `setParentProperty()` calls with a new `AssignParent()` (validates, throws) /
   `DetachParent()` (wired into `Remove`/`RemoveAt`/`Clear`, and into `SetItem` when replacing
   an existing value) pair on `JsonNode`, matching each type's real .NET method-by-method
   detach/assign ordering. 8 regression tests. Commit `e1f3d9d`.
4. **`JsonObject::operator[]` threw `KeyNotFoundException` for a missing key** — verified
   against `JsonNode.cs`'s string indexer doc comment ("If the property is not found, null is
   returned"); `GetProperty` is the actual throwing counterpart in this port's design. Changed
   the return type from `const shared_ptr<JsonNode>&` to `shared_ptr<JsonNode>` (no missing-key
   slot to reference) and return `nullptr` instead of throwing. Fixed one pre-existing test that
   had encoded the old throwing behavior (`Indexer_Missing_Throws` → `Indexer_Missing_ReturnsNull`).
   4 more regression tests. Commit `e1f3d9d`.

### Wave-3 catalogue: ALL 56 criticals now genuinely fixed across all 8 namespace slices

Net core+Sockets (4), Net.Http/WebSockets (2), IO core (7, itemized as 4 fixes), IO.Compression/
Hashing (3-4, itemized/already-fixed), Text.Json (7), Threading (24, itemized as 13 fixes), Xml
core (5), Xml.Linq+XPath (3). Plus Diagnostics (7 findings, 0 critical), fixed earlier. **Before
declaring a category complete, recheck its own catalogue section header count against what's
actually itemized/fixed** — this exact Text.Json omission is the second time in this session a
premature "all criticals fixed" claim needed correcting (the first was Xml.Linq+XPath, two
checkpoints prior). Grep this file for namespace names against fix commits before trusting a
"done" claim inherited from an earlier checkpoint.

### What remains: moderate + minor findings only, everywhere

Moderate/minor status per the wave-3 audit's original severity counts (99 moderate + 66 minor
total): Threading has 3/29 moderate done (`SynchronizationContext`, `CancellationTokenSource`
disposed-race + LIFO ordering); IO.Compression/Hashing has ~6/6 moderate done (folded into the
criticals-adjacent IsolatedStorageFile/GZipEncoder fixes); Xml core has 1/13 moderate done
(`XmlNode` ancestor-cycle/cross-document guard). Every other slice's moderates and every slice's
minors are untouched. Recommended next: continue picking off genuinely dangerous-despite-labeled
moderate items first (re-verify against current source, the catalogue is many fix-batches
stale), then work through the rest namespace by namespace.

## Session checkpoint (2026-07-10, continued again) — XmlNode ancestor-cycle + cross-document guard added

*Branch: `feature/work`, HEAD `208c674` — 11243 tests passing (up from 11237 at the top of the
Threading-dangerous-moderates checkpoint below), full clean rebuild verified (0 errors/0
warnings)*

Fixed the tree-corruption-risk finding flagged in the previous checkpoint, after correcting its
namespace attribution (it's **System.Xml core**'s `XmlNode`, not `System.Xml.Linq`'s
`XContainer` — see the correction note preserved below):

- **`XmlNode::AppendChild`/`PrependChild`/`InsertBefore`/`InsertAfter` had no ancestor-cycle or
  cross-document guard** — verified against `XmlNode.cs`: real .NET throws `ArgumentException`
  both when the node being inserted is `this` itself or one of `this`'s ancestors (would create
  a cycle) and when the node belongs to a different `XmlDocument` (would corrupt cross-document
  invariants real .NET otherwise requires `ImportNode` for — not implemented in this port, so
  previously a cross-document insert silently reported success while doing nothing, since
  tinyxml2's own `InsertEndChild`/`InsertFirstChild`/`InsertAfterChild` already internally
  refuse cross-document inserts by returning `nullptr`, unchecked by this port's wrapper).
  tinyxml2 has no self/ancestor-cycle guard at all: its `InsertChildPreamble` unconditionally
  unlinks the node from its current parent before reattaching, so inserting an ancestor produces
  a genuine two-node parent/child cycle. Added `ThrowIfSelfOrAncestor`/`ThrowIfDifferentDocument`
  helpers in `XmlNode.cpp`, called at the top of all four insertion methods (and per-child inside
  the existing `XmlDocumentFragment`-flattening loops in `AppendChild`/`PrependChild`) before any
  mutation happens. Scoped to the two checks that are genuine memory-safety/correctness risks;
  did not port `IsValidChildType`'s full per-node-type legality table (a much larger, separate
  validation-completeness surface, not a corruption risk — .NET's own default is "false" and
  each subclass overrides it with its own allowed-type set). 6 regression tests (self-insert,
  ancestor-insert on all 4 methods, cross-document insert), including verifying the tree is left
  unmodified after the throw.

Only remaining "tree corruption risk"-tier item from the earlier checkpoints is closed. Next
priority per the established recommended-order: Threading's ordinary moderate/minor findings, or
whichever namespace's moderates offer the next-highest real-world-impact fix — re-verify against
current source before starting, per the standing discipline (several catalogued items have
turned out to be already fixed as side effects of other work this session).

## Session checkpoint (2026-07-10, continued again) — Threading's 3 "genuinely dangerous moderate" items now all fixed

*Branch: `feature/work`, HEAD `c09c596` — 11237 tests passing (up from 11234 at the top of the
criticals-complete checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Pulled forward and fixed all 3 items the previous checkpoint flagged as "genuinely dangerous
despite the moderate label":

1. **`SynchronizationContext` fully broken** — verified against `SynchronizationContext.cs`:
   `Post()` must queue asynchronously (thread pool) while `Send()` runs synchronously; this
   port's `Post()` ran the callback inline, identical to `Send()`, defeating the entire
   purpose of the distinction. Separately, `getCurrent()`/`SetSynchronizationContext()` never
   round-tripped at all — `getCurrent()` always returned `nullptr` regardless of what had been
   set. Fixed `Post()` to call `ThreadPool::QueueUserWorkItem`; added a `thread_local` current-
   context slot backing `getCurrent()`/`SetSynchronizationContext()`. Commit `3f64080`.
2. **`CancellationTokenSource.disposed_` data race** — plain (non-atomic) `bool` read by
   `getTokenProperty()`/`Cancel()`'s `ThrowIfDisposed()` check and written by `Dispose()` with
   no synchronization: genuine UB under concurrent access, a realistic pattern for a
   cancellation primitive. Changed to `std::atomic<bool>`. Commit `3a32380`.
3. **`CancellationTokenSource::Cancel()` non-LIFO callback ordering** — verified against
   `CancellationTokenSource.cs`'s `ExecuteCallbackHandlers`: real .NET fires callbacks in LIFO
   order (most-recently-registered first) so nested/child registrations cancel before their
   parents'. `Detail::CancellationState::callbacks` was a `std::unordered_map`, whose iteration
   order (used by `Cancel()`'s collection loop) has no relationship to registration order.
   Switched to `std::map` (ordered by the existing monotonic `nextId` registration counter) and
   walk it in reverse (`rbegin()`/`rend()`) for descending-id == LIFO order.
   `CancellationTokenRegistration`'s `erase()`/`count()` calls needed no changes (same API on
   `std::map`). Added `CancellationTokenSource_Cancel_RunsCallbacksInLifoOrder` regression test.
   Commit `c09c596`.

Threading now has 26 moderate + 10 minor findings remaining (all "ordinary" severity — no more
flagged-as-actually-dangerous items). Xml.Linq+XPath's tree-corruption-risk moderate finding
(next paragraph) is the next flagged pull-forward candidate.

**Correction while investigating that item:** the previous checkpoint's text describing it as
an "Xml.Linq+XPath" finding was wrong — re-checking the catalogue itself (search this file for
`RemoveChild`/`AppendChild` skip .NET's ancestor-cycle/cross-document/legal-child-type
validation), that finding is listed under **System.Xml core** (the `XmlNode`-based DOM,
`XmlNode::RemoveChild`/`AppendChild`/etc.), not `System.Xml.Linq` (the `XContainer`/`XElement`-
based API, which already got its own analogous cycle-guard fix in `XContainer::InsertNodeAt`
during the criticals pass). Re-verifying against current source before fixing, per the standing
discipline.

## Session checkpoint (2026-07-10, continued again) — wave-3 catalogue: EVERY critical finding now fixed (Xml.Linq+XPath's 3 were missed earlier, now done too)

*Branch: `feature/work`, HEAD `7b58bcb` — 11234 tests passing (up from 11227 at the top of
the Net-core-complete checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

The previous checkpoint's claim that "every namespace slice's criticals are now fixed" was
**premature** — `System.Xml.Linq + XPath`'s own "3 critical" count (visible in its own
catalogue section header) had been overlooked while working through namespaces in a specific
order. Caught and fixed this session:

1. **XPath relational operators (`<`,`<=`,`>`,`>=`) used lexicographic string comparison
   instead of numeric whenever a node-set operand was involved** — verified against XPath 1.0
   §3.4: relational operators always convert to numbers, even for node-set operands (unlike
   `=`/`!=`, which do compare string-values lexicographically). The node-set-vs-boolean and
   node-set-vs-number branches in `CompareValues` already did this correctly; node-set-vs-
   node-set and node-set-vs-string used `StrCompare` unconditionally for every operator. Fixed
   both remaining branches to use numeric comparison for relational (non-equality) operators.
   2 regression tests using the existing bookstore-catalog fixture's price elements (`price >
   '9'` and a node-set-vs-node-set price comparison), both previously silently wrong. Commit
   `ba9379c`.
2. **`XContainer::InsertNodeAt` had no cycle/self-containment guard** — verified against
   `XContainer.cs`'s `AddNode()`: real .NET clones a node being added if it's already attached
   anywhere (copy-on-attach semantics), which incidentally also prevents cycles. This port
   uses move semantics instead (an established, simpler design, not itself a bug) but had no
   guard at all against the one genuinely broken outcome: inserting a node into its own
   subtree, creating a permanent `shared_ptr` reference cycle and stack-overflowing any
   recursive traversal. Added a walk up `parent_` checking for self/ancestor before
   reparenting; throws `InvalidOperationException` instead of creating the cycle. 2 regression
   tests (self-as-child, ancestor-as-child-of-descendant). Commit `fd05688`.
3. **Namespaced `XAttribute`/`XElement` serialized as malformed Clark-notation XML** — `XName`
   `{namespace}local`) was written directly as the XML attribute *name* in three call sites
   (`XAttribute::ToString()`, `XElement::WriteTo()`, `XStreamingElement::WriteContent()`) —
   `{`/`}` aren't legal in an XML Name production, so this was genuinely malformed, unparseable
   output, not just a fidelity gap. Verified real .NET's `XAttribute.ToString()` resolves an
   actual namespace prefix via a real `XmlWriter`; since this port's `XmlWriter` has no
   namespace/prefix-aware `WriteAttributeString` overload, implementing full prefix resolution
   was judged too large a feature addition for this specific bug (there's a separately-tracked
   moderate finding for `XElement::WriteTo` already dropping the *element's own* namespace for
   the same underlying reason). Fixed all three sites to use the local name only — converts the
   critical "malformed XML" outcome into the same already-acknowledged "namespace fidelity
   loss" class the element-name path already has. 3 regression tests, including a full
   `ToString()`-then-`Parse()` round trip that previously would have thrown/corrupted. Commit
   `7b58bcb`.

### Wave-3 catalogue: genuinely complete for criticals now

Every namespace slice's critical-severity findings are fixed: Threading (13), IO.Compression/
Hashing (3, +1 already-fixed), Xml core (5, 1 already-fixed), IO core (4), `System.Net` core+
Sockets (4, 2 already-fixed), Net.Http/WebSockets (2, already-fixed), and now Xml.Linq+XPath
(3). **Before starting the next namespace's moderates, re-verify its own catalogue section's
severity counts against what's actually been touched** — this is exactly the mistake that
caused Xml.Linq+XPath's criticals to be missed for two checkpoints in a row.

### What remains

Exclusively moderate/minor findings now, across every namespace: Net core+Sockets (12
moderate + 8 minor), Net.Http/WebSockets (13 moderate + 4 minor), IO core (10 moderate + 5
minor), IO.Compression/Hashing (6 moderate + 3 minor, minus what's already fixed), Xml core
(13 moderate + 8 minor), Xml.Linq+XPath (12 moderate + 6 minor), Threading (29 moderate + 10
minor). See "Full findings catalogue" further down this file for full per-namespace detail —
re-verify each finding against current source before fixing, since several turned out to
already be resolved as side effects of other work this session.

**Recommended next session priority:** per the earlier-established guidance, prioritize by
real-world impact within each namespace rather than strictly by catalogue order.
Threading's moderates include some genuinely dangerous items worth pulling forward despite the
"moderate" label: `SynchronizationContext` fully broken, `CancellationTokenSource.disposed_`
data race, non-LIFO callback ordering. Xml.Linq+XPath's moderates include a real tree-
corruption risk (`RemoveChild`/`AppendChild`/etc. skip .NET's ancestor-cycle/cross-document/
legal-child-type validation) that's a closer cousin to the critical just fixed there than a
typical "moderate" gap — consider pulling that forward too.

## Session checkpoint (2026-07-10, continued again) — all System.Net core + Sockets criticals now done

*Branch: `feature/work`, HEAD `d551656` — 11227 tests passing (up from 11216 at the top of
the IO-core-complete checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Finished `System.Net` core + Sockets' 4 listed criticals. As flagged in the previous
checkpoint, 2 of the original wave-3 findings turned out to already be fixed — verified
against current source before starting, per the standing "always re-verify, the catalogue is
stale" discipline established last checkpoint:

- **`Socket::Send/Receive/SendTo/ReceiveFrom` missing bounds validation** — already fixed
  (the earlier memory-safety pass, commit `ab60037`). No action needed.
- **`HttpClient`/`ClientWebSocket` bounds-check criticals** (Net.Http/WebSockets slice) —
  already fixed (same earlier pass, commits `dc02094`/`ed80e24`). No action needed.

Fixed the 2 that were genuinely still open, plus a related finding in the WebUtility slice:

- **`SocketError` never translated from POSIX errno** — verified against
  `SocketErrorPal.Unix.cs`'s `GetSocketErrorForNativeError`: real .NET translates POSIX errno
  into the WSA-numbered `SocketError` space before constructing a `SocketException`. This bug
  was duplicated across **four** separate files (`Socket.cpp`, `TcpClient.cpp` — covering both
  `TcpClient` and `TcpListener`, `UdpClient.cpp`, `NetworkStream.cpp`), each with its own
  raw-errno-to-`intcs`-cast socket-syscall error handling. Added a shared internal header
  (`include/System/Net/Sockets/detail/ErrnoTranslation.hpp`, following the existing `detail/`
  convention for internal-only logic shared across files) with the full translation table, and
  wired all four files to use it (Windows path unchanged — WSA codes already match
  `SocketError`'s numbering). Regression test: `TcpClient::Connect` to a refused port now
  correctly reports `SocketError::ConnectionRefused` (was observed as the raw errno value 111).
  Commit `c54b4de`.
- **`IPAddress` IPv4 parsing via UB-prone `sscanf`** — verified against
  `IPv4AddressHelper.Common.cs`'s `ParseNonCanonical` (what `IPAddress.Parse`'s IPv4 path
  actually uses). The old `sscanf("%u.%u.%u.%u%c", ...)` had undefined behavior on segment
  overflow, accepted a leading `-` via `%u`'s implementation-defined sign handling, and
  rejected real .NET-valid input (octal/hex segments, "short forms" with fewer than 3 dots
  where the last segment absorbs the remaining bytes, e.g. `"192.168.1"` == `192.168.0.1`).
  Replaced with a direct port of the real algorithm (base-detection per segment, an overflow-
  checked `uint64_t` accumulator, dot-count-dependent combination). One existing test had
  encoded the old (incorrect) "exactly 4 dotted segments" behavior and was updated; 7 new
  regression tests cover short forms, single-segment whole-value parsing, hex/octal segments,
  and all the new rejection cases (trailing garbage, empty segment, too many dots, overflow,
  leading minus). Commit `297c9a6`.
- **`WebUtility::UrlDecode` throws `std::invalid_argument` on malformed percent-encoding** —
  verified against `WebUtility.cs`'s `UrlDecodeInternal`: real .NET never throws here: a
  malformed `%XX` sequence (invalid hex digit(s), or too few characters remaining) just leaves
  `%` as a literal character and continues. Replaced `std::stoi(..., 16)` (which throws
  `std::invalid_argument` when neither hex digit is valid) with a manual, non-throwing hex-
  digit check. 3 regression tests (fully malformed, partially-valid, and truncated-at-end-of-
  string percent sequences). Commit `d551656`.

### Wave-3 catalogue: what remains

Threading criticals: done. IO.Compression/Hashing criticals+moderates: done (3 minors left).
Xml core criticals: done (13 moderate + 8 minor left). IO core criticals: done (10 moderate +
5 minor left). **`System.Net` core+Sockets criticals: done** (12 moderate + 8 minor left —
Socket setter validation gaps, IPv6-only-client limitations, `SocketFlags` translation,
`IPAddress::GetHashCode()` IPv6 truncation, etc.). **Net.Http/WebSockets criticals: done**
(13 moderate + 4 minor left). Still entirely untouched: Xml.Linq+XPath (21 findings) and
Threading's 29 moderate + 10 minor findings (lowest priority — do last per the original plan).

**Recommended next session priority:** the wave-3 catalogue's remaining criticals are
exhausted — every namespace slice's criticals are now fixed. What's left is exclusively
moderate/minor findings across Net core+Sockets, Net.Http/WebSockets, IO core,
IO.Compression/Hashing, Xml core, Xml.Linq+XPath, and Threading, plus Xml.Linq+XPath's
findings entirely untouched (criticals AND moderates AND minors — verify its severity
breakdown against the "Full findings catalogue" section before assuming only moderates/minors
remain there). Given the volume, prioritize by real-world impact within each namespace rather
than strictly by catalogue order — e.g. Threading's remaining moderates include some
genuinely dangerous items (`SynchronizationContext` fully broken, `CancellationTokenSource.
disposed_` data race) worth pulling forward despite being labeled "moderate." **Always
re-verify each finding against current source before fixing — this session repeatedly found
findings already resolved as side effects of other fixes.**

## Session checkpoint (2026-07-10, continued again) — all 4 IO core criticals now done

*Branch: `feature/work`, HEAD `fe17b29` — 11216 tests passing (up from 11205 at the top of
the Xml-core-complete checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Finished IO core's 4 listed criticals (the catalogue's "7 critical" count for this slice
included 3 folded into the moderate-findings prose rather than individually enumerated; only
4 were itemized and are addressed here).

1. **`MemoryStream::Write()`/`WriteByte()` silently no-op'd instead of throwing
   `NotSupportedException` when `!CanWrite`** — verified against `MemoryStream.cs`'s
   `EnsureWriteable()`; `SetLength()` in the same class already got this right (confirming the
   inconsistency was a bug). Fixed both to throw, matching `SetLength()`'s exact message.
   2 regression tests. Commit `b73c1e2`.
2. **`File::Move`/`Directory::Move`/`FileInfo::MoveTo`/`DirectoryInfo::MoveTo` all silently
   overwrote an existing destination** — verified against `FileSystem.Unix.cs`'s
   `MoveFile(src, dst, overwrite: false)`: real .NET's non-overwrite Move never replaces an
   existing destination. This port used `std::filesystem::rename()` unconditionally (silently
   replaces on POSIX). Added an existence check before the rename in all four, throwing
   `IOException` with the message convention already established elsewhere in this codebase
   (`FileStream`'s Create-mode check) — a benign TOCTOU race rather than reproducing .NET's
   platform-specific atomic link/unlink primitive. 4 regression tests (one per API). Commit
   `b0cd62f`.
3. **`FileStream::getLengthProperty()` returned a stale cached length after `Write()`
   extended the file** — verified against `FileStream.cs`'s `Length` getter (a live query, not
   a cache). Fixed to flush pending writes then query `std::filesystem::file_size()` live,
   falling back to the cached value only if the live query fails. 1 regression test proving
   Length grows correctly across two successive `Write()` calls on the same open instance
   (the previous cached-at-construction bug specifically didn't manifest in the existing
   close-then-reopen-style test, since reopening naturally re-queries the size). Commit
   `04229d8`.
4. **`StreamReader::Close()`/`StreamWriter::Close()` ignored `leaveOpen`** — verified against
   `StreamReader.cs`/`StreamWriter.cs`: `Close()` delegates to `Dispose(true)`, which checks
   `_closable` (`!leaveOpen`). Both types' destructors already had this right; `Close()` itself
   closed unconditionally. Fixed both to match their own destructors. 4 regression tests (one
   leaveOpen=true/false pair per type), using `MemoryStream::Close()`'s buffer-clearing as an
   observable signal of whether the underlying stream was actually closed. Commit `fe17b29`.

### Wave-3 catalogue: what remains

Threading criticals: done. IO.Compression/Hashing criticals+moderates: done (3 minors left).
Xml core criticals: done (13 moderate + 8 minor left). **IO core criticals: done** (10
moderate + 5 minor left — `File::Delete` deleting a directory, `RandomAccess::Write` not
looping on short writes, `MemoryStream::Read()`/`Close()` more silent-wrong-behavior bugs,
etc.). Still entirely untouched: Net core+Sockets (24 findings, including a critical
`SocketError` errno-translation gap and a `sscanf`-based `IPAddress` parsing UB critical),
Net.Http+WebSockets+Security (19 findings), Xml.Linq+XPath (21 findings), and Threading's 29
moderate + 10 minor findings. See "Full findings catalogue" further down this file for full
per-namespace detail.

**Recommended next session priority, in order:**
1. **`System.Net` core + Sockets criticals** (4 items) — `SocketError` never translated from
   POSIX errno (every `SocketException` carries a meaningless code), `IPAddress` IPv4 parsing
   via UB-prone `sscanf`, `Socket::Send/Receive` missing bounds validation (**verify this one
   first** — it may already be fixed, since a memory-safety pass earlier this session added
   bounds validation to `Socket.cpp`; re-check before assuming it's still open),
   `WebUtility::UrlDecode` throwing `std::invalid_argument` instead of a `System::` type.
2. **`System.Net.Http`/WebSockets criticals** (2 items, `HttpClient`/`ClientWebSocket` bounds
   checks — **also likely already fixed** by the same earlier memory-safety pass; verify
   before assuming open).
3. Everything else, same discipline as this whole session: verify against
   `/rv/tmp/runtime/src/libraries/` before fixing, add regression tests, rebuild/retest clean,
   commit, push, checkpoint. Given how many "already fixed" findings turned up this session
   (XmlReader OOB, ZipArchive exception types, both Net/Http memory-safety items likely too),
   **always re-verify a finding against current source before spending time on it** — the
   wave-3 catalogue is now several fix-batches stale.

## Session checkpoint (2026-07-10, continued again) — IO.Compression/Hashing moderates done; all 5 Xml core criticals now done

*Branch: `feature/work`, HEAD `0cbc2a3` — 11205 tests passing (up from 11185 at the top of
the Threading-complete checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Continued the wave-3 catalogue per the recommended order: IO.Compression/Hashing moderates,
then Xml core criticals (both namespaces this session had already touched files in).

### IO.Compression + Hashing + IsolatedStorage

- **Critical #3 (`DeflateStream`/`GZipStream` bare `std::runtime_error`) and the matching
  "rest of `ZipArchive`'s failure paths" moderate finding were already resolved** — verified
  all three files now consistently throw `System::IO::IOException`/`InvalidDataException`,
  not `std::runtime_error`. No fix needed; likely already addressed as a side effect of the
  earlier ZipArchive data-loss commit (`568323a`) or the finding was already stale when audited.
- **`GZipEncoder::GetMaxCompressedLength()` omitted the `+12` gzip framing overhead** — GZip's
  18-byte overhead (10-byte header + 8-byte CRC32/size trailer) is 12 bytes more than the
  6-byte zlib overhead already included in `DeflateEncoder`'s `compressBound()`-derived bound.
  Verified against `GZipEncoder.cs`; added the `+12` adjustment and matching overflow guard.
  Commit `a5c1fbe`.
- **`IsolatedStorageFile`: disposed state never checked, `DeleteDirectory` always recursive,
  `Quota` inherited the base's `0`** — verified against `IsolatedStorageFile.cs`'s
  `EnsureStoreIsValid()`/`DeleteDirectory`/`Quota`. Added `throwIfDisposed()` to all
  file/directory operations (every operation remained silently usable after `Close()`);
  switched `DeleteDirectory` from `remove_all` (recursive) to `remove` (non-recursive, fails
  on non-empty — matches `Directory.Delete(path, false)`; `Remove()`'s whole-store teardown
  correctly stays recursive); added the `getQuotaProperty()` override returning
  `longcs::max()`. Also found and fixed a related, more precise version of the "path
  null/empty validation" finding: real .NET's `CopyFile`/`MoveFile`/`MoveDirectory` (but *not*
  `DeleteFile`/`CreateDirectory`/etc.) use `ArgumentException.ThrowIfNullOrEmpty` — added that
  validation only to those three methods, matching the real per-method distinction rather than
  applying it blanket. Commit `6da3e2d`.
- **"IsolatedStorageFile methods skip path-null/empty validation" (the blanket framing) does
  not hold once checked**: most of those methods use `ArgumentNullException.ThrowIfNull` in
  real .NET (null-only), which has no C++ translation since `std::string` cannot be null —
  nothing to fix there. Not a stale finding overall (the 3 methods above genuinely needed it),
  just imprecisely scoped in the original catalogue entry.
- Not yet touched: the 3 minor findings (`DeflateEncoder::GetMaxCompressedLength()` 32-bit
  truncation on >4GiB input, Application/Assembly-scope isolated storage sharing a root,
  `IsolatedStorageException` never setting its HResult).

### Xml core: all 5 criticals now done

1. **`XmlReader.cpp` OOB access** — already fixed earlier this session (wave-3 priority item 3,
   commit `8a440a2`).
2. **`XmlConvert` `"Infinity"`/`"INF"` token mismatch** — verified against `XmlConvert.cs`:
   real `XmlConvert.ToString(float/double)` uses the XML Schema lexical-space tokens
   `"INF"`/`"-INF"`, and `ToSingle`/`ToDouble` trim XML whitespace then recognize those tokens
   before falling through to ordinary parsing. Added the special-casing directly in
   `XmlConvert`'s four methods (not in `System::Single`/`Double`, which are correct for
   .NET's own general-purpose `ToString()`/`Parse()` contract). Commit `dc715e4`.
3. **`XmlNode::getInnerXmlProperty()`/`getOuterXmlProperty()` pretty-printed instead of
   producing exact markup** — `tinyxml2::XMLPrinter` defaults to `compact=false`. Fixed both
   to construct with `compact=true`. Commit `0cbc2a3`.
4. **`XmlAttribute::getNamespaceURIProperty()` always `""`** and **5. `CloneNode()` always
   `nullptr`** — both inherited from `XmlNode`'s defaults, which key off `native_`, a field
   `XmlAttribute` never sets (tinyxml2 ties attributes directly to their owner element, so
   this port tracks the owner separately via `ownerElementNative_`). Added `XmlAttribute`-
   specific overrides: `getNamespaceURIProperty()` walks ancestors from `ownerElementNative_`
   for the nearest `xmlns:prefix` declaration, correctly returning `""` for unprefixed
   attributes (default `xmlns` does not apply to attributes, verified against the XML
   Namespaces spec and `XmlAttribute.cs`); `CloneNode()` creates a new unattached attribute via
   `doc->CreateAttribute()` and copies the value (always a full clone, matching real .NET
   ignoring the `deep` parameter for attributes). This also fixes
   `XmlNamedNodeMap::GetNamedItem(localName, namespaceURI)` for prefixed attributes, since it
   dispatches virtually through the now-fixed property. Commit `4322912`.

### Wave-3 catalogue: what remains

Threading criticals: done. IO.Compression/Hashing criticals+moderates: done (3 minors left).
Xml core criticals: done (13 moderate + 8 minor Xml-core findings remain). Still entirely
untouched: Net core+Sockets (24 findings), Net.Http+WebSockets+Security (19), IO core (22
findings — `MemoryStream::Write` silent no-op and 3 other criticals await), Xml.Linq+XPath
(21), and Threading's 29 moderate + 10 minor findings. See "Full findings catalogue" further
down this file. Recommended next: **IO core's 7 criticals** (real, common-path bugs — silent
write no-ops, unconditional file overwrite on Move, stale cached FileStream.Length,
`leaveOpen` ignored) — same severity tier as what's just been cleared, not yet started.

## Session checkpoint (2026-07-10, continued again) — wave-3 item 5: all 13 Threading criticals now done (ReaderWriterLockSlim finished)

*Branch: `feature/work`, HEAD `241f881` — 11185 tests passing (up from 11173 at the top of
the batch-2 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Finished `ReaderWriterLockSlim`, the one Threading critical left open at the previous
checkpoint (previously flagged as needing its own session — tackled anyway given the momentum
and full context already loaded). **All 13 Threading criticals from the wave-3 catalogue are
now fixed.**

Verified against `ReaderWriterLockSlim.cs`'s `TryEnterReadLockCore`/`TryEnterWriteLockCore`/
`TryEnterUpgradeableReadLockCore`/`Exit*`. Fixed all three compounding issues:

1. **Timeout discarded** — `TryEnterReadLock/WriteLock/UpgradeableReadLock(millisecondsTimeout)`
   now use real timed condition-variable waits (the established Timeout.Infinite-aware pattern
   from earlier this session) instead of a single non-blocking attempt.
2. **`LockRecursionPolicy` ignored** — same-thread recursive acquisition now throws
   `LockRecursionException` under the default `NoRecursion` policy instead of deadlocking.
   Verified the exact rule split against the real source: write-after-read/
   upgrade-after-read/upgrade-after-write always throw regardless of policy (real .NET checks
   these identically in both branches); same-type recursion (read-after-read etc.) only
   throws under `NoRecursion` and is a tracked, counted recursion under `SupportsRecursion`.
3. **Double-`ExitReadLock()` bug** — reader/writer/upgrade ownership switched from set
   *membership* to per-thread *counts* (ID-keyed, not `this`-pointer-keyed, matching the
   `AsyncLocal`/`ThreadLocal` address-reuse fix earlier this session), so nested
   `EnterReadLock()`/`EnterReadLock()`/`ExitReadLock()`/`ExitReadLock()` now correctly balances
   instead of the second exit throwing — and, critically, the reader tally actually reaches
   zero afterward instead of permanently starving a waiting writer.

Added the previously-inaccessible `RecursionPolicy` property. 12 new regression tests (real
timeout blocks ~the requested duration then succeeds once released; all 6 NoRecursion
cross/same-type combinations throw instead of hanging; nested acquisition under
`SupportsRecursion` balances and a writer can still acquire afterward). All existing tests
(including the pre-existing upgrade-to-write deadlock-avoidance tests) still pass; full suite
run 3x to rule out flakiness in this concurrency-heavy rewrite. Commit `241f881`.

### Threading: fully done for criticals; what remains

All 13 criticals fixed across this session's two Threading batches (see the batch-1/batch-2
checkpoints below for full per-fix detail: `TaskCompletionSource`, `CountdownEvent`,
`Task::Wait`, `CancellationTokenSource.Cancel`, `ReaderWriterLock` timeout, `ValueTask`,
`Channel` capacity-0, `ThreadLocal` reentrancy, `LazyInitializer`, `AsyncLocal`/`ThreadLocal`
ID-keying, `Barrier`, and now `ReaderWriterLockSlim`). Still untouched: the 29 moderate + 10
minor Threading findings from the original wave-3 catalogue (see "Full findings catalogue"
further down this file, `System.Threading + Tasks + Channels` section).

### Wave-3 catalogue: what remains overall

Threading criticals: **done**. Still entirely untouched: Threading's 29 moderate + 10 minor
findings, and every other namespace slice — Net core+Sockets (24 findings), Net.Http+
WebSockets+Security (19), IO core (22), IO.Compression+Hashing (13), Xml core (26),
Xml.Linq+XPath (21). See "Full findings catalogue" further down this file for full
per-namespace detail. Recommended next steps, in order: IO.Compression/Hashing moderates
(small, `ZipArchive.cpp` already touched this session) → Xml core (5 criticals, `XmlReader.cpp`
already touched this session for the memory-safety fix) → Net/IO/Xml remainder → Threading
moderates/minors (lowest severity, do last).

## Session checkpoint (2026-07-10, continued again) — wave-3 item 5: Threading critical findings, batch 2 — 11 of 13 done, only ReaderWriterLockSlim remains

*Branch: `feature/work`, HEAD `e09c4fb` — 11173 tests passing (up from 11160 at the top of
the batch-1 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Continued straight through the rest of the wave-3 Threading "Other critical findings" list.
**11 of 13 are now fixed; only `ReaderWriterLockSlim` remains** (explicitly flagged by the
original audit as the largest/most invasive item, needing its own session — see below).

- **`ReaderWriterLock::AcquireReaderLock`/`AcquireWriterLock` silently returned on timeout** —
  verified against `ReaderWriterLock.cs`'s `GetTimeoutException()`: real .NET throws
  (a private `ReaderWriterLockApplicationException : ApplicationException`, invisible outside
  the assembly — callers realistically catch the public `ApplicationException` base) rather
  than returning as if the lock had been acquired. Fixed both methods to throw
  `System::ApplicationException("The operation has timed out.")` on timeout. 2 regression
  tests (background thread times out while main thread holds the lock). Commit `e27d591`.
- **`ValueTask(Task)` only snapshotted `IsCompleted` at construction** — the Task was then
  discarded entirely, so a still-running task's later completion/fault was never observed, and
  even an *already*-faulted task's exception was silently dropped (only the completed-ness
  bool was ever captured). Fixed by storing the wrapped `Task` (`std::optional<Task>`) and
  delegating all completion/fault queries and `GetAwaiter()` to it live. 2 regression tests
  (already-faulted task rethrows; still-running task later observes both completion and
  fault). Commit `719845a`.
- **Bounded `Channel` with `capacity == 0` permanently deadlocked every write** — verified
  against `BoundedChannel.cs`'s `TryWrite`: real .NET's queue-transition-from-0-to-1 special
  case means a capacity-0 channel is observably equivalent to a capacity-1 channel for every
  publicly-visible `TryWrite`/`WaitToWriteAsync` outcome. Added `effectiveCapacity()`
  (`capacity == 0 ? 1 : capacity`) used in both "is full" checks. 2 regression tests. Commit
  `1353024`.
- **`ThreadLocal<T>::getValueProperty()` had no reentrancy guard** — a factory that
  (directly or indirectly) called back into the same instance recursed until an uncatchable
  stack overflow. Verified against `ThreadLocal.cs`'s
  `ThreadLocal_Value_RecursiveCallsToValue`: added a thread_local in-progress marker set that
  throws `System::InvalidOperationException` on reentry instead of recursing. 1 regression
  test. Commit `39d5b4a`.
- **`LazyInitializer::EnsureInitialized<T>` used a `static std::mutex` scoped per template
  instantiation of T** — shared across every unrelated call site initializing a different
  target of the same T; a factory that reentrantly initialized a *different* target of the
  same type on the same thread self-deadlocked (non-recursive mutex). Verified against
  `LazyInitializer.cs`: real .NET takes **no lock at all** — it publishes via
  `Interlocked.CompareExchange`, and racing threads may transiently construct duplicate
  instances with the loser simply discarded. Switched to the same lock-free CAS approach via
  `std::atomic_ref<T*>` (deleting the losing candidate, since this runtime has no GC); added
  the missing null-factory-result → `InvalidOperationException` check. 2 regression tests
  (null factory throws; reentrant same-type initialization no longer deadlocks). Commit
  `d83f7f2`.
- **`AsyncLocal<T>`/`ThreadLocal<T>` destructors only cleaned up the destroying thread's own
  `thread_local` map entry** — every *other* thread that had touched the instance retained a
  stale entry keyed by the now-dangling `this` pointer forever; a new instance later allocated
  at the same address (routine with heap/stack reuse) would silently collide with that stale
  entry, corrupting data across two logically-unrelated instances (not just leaking). Fixed
  both types to key their thread_local maps by a monotonically-increasing per-instance ID
  (`std::atomic<uint64_t>` counter, never reused) instead of by `this` — a new instance can
  never collide with a stale entry left by a destroyed one, regardless of address reuse (the
  trade-off, matching this port's existing simplified single-thread-registry design: another
  thread's stale entry for a destroyed instance is a bounded per-ID leak until that thread
  exits, not proactively reclaimed). 2 regression tests (one per type) that deterministically
  reproduce the exact address-reuse scenario via placement-new into a fixed buffer. Commit
  `a2c76e6`.
- **`Barrier::SignalAndWait`: post-phase exception only seen by the triggering thread;
  `FinishPhase` invoked the action while still holding its mutex — reentrant calls deadlocked**
  — verified against `Barrier.cs`'s `FinishPhase()`/`SignalAndWait()`: real .NET has *every*
  participant thread check and rethrow `BarrierPostPhaseException` (not just the trigger), and
  rejects (`InvalidOperationException`) a reentrant call from within the post-phase action via
  an `_actionCallerID` thread-id check performed *before* attempting any lock. Fixed both:
  added `lastPostPhaseException_` checked by every waiter after `cv_.wait()` returns, and an
  `atomic<thread::id> actionCallerId_` checked before acquiring `mutex_` in
  `SignalAndWait`/`AddParticipant`/`RemoveParticipant`. 2 regression tests (3-participant
  barrier: all three observe the exception; reentrant call from the action throws instead of
  hanging). Commit `e09c4fb`.

### `ReaderWriterLockSlim` — the one remaining Threading critical, needs its own session

Explicitly flagged by the original wave-3 audit as "largest/most invasive item on this list —
likely needs its own session." Three compounding issues, not a quick fix:

1. `TryEnterReadLock/WriteLock/UpgradeableReadLock(intcs millisecondsTimeout)` discard the
   timeout parameter entirely — always a single non-blocking attempt regardless of what the
   caller passed.
2. `LockRecursionPolicy` is ignored entirely — same-thread recursive acquisition **deadlocks**
   instead of throwing `LockRecursionException` (when the policy is `NoRecursion`, the .NET
   default) or being allowed (when `SupportsRecursion`).
3. Recursive `EnterReadLock()` has a bug where the *second* matching `ExitReadLock()` throws
   (the held-state tracking is `unordered_set` membership-only, not a count) — this
   **permanently starves all future writers** since the reader count never actually reaches
   zero from the lock's internal perspective.

Fixing this properly requires: (a) real timed waits per call (not a single non-blocking
attempt) using the existing `std::chrono` + condition-variable patterns established elsewhere
in this session's Timeout.Infinite fixes, (b) a per-thread recursion-count map (mirroring this
session's `ThreadLocal`/`AsyncLocal` ID-keyed pattern, not `this`-pointer-keyed) checked against
the configured `LockRecursionPolicy`, and (c) switching the reader-held-state tracking from a
membership set to a count. Verify against `ReaderWriterLockSlim.cs`'s actual bit-packed
`_lockState` field before implementing — do not guess the semantics.

### Wave-3 catalogue: what remains after this batch

All 13 Threading criticals are done except `ReaderWriterLockSlim` (above). Still entirely
untouched: the 29 moderate + 10 minor Threading findings, and every other namespace slice —
Net core+Sockets (24 findings), Net.Http+WebSockets+Security (19), IO core (22),
IO.Compression+Hashing (13), Xml core (26), Xml.Linq+XPath (21). See "Full findings catalogue"
further down this file for full per-namespace detail. Recommended next steps, in order:
`ReaderWriterLockSlim` (finish Threading cleanly) → IO.Compression/Hashing moderates (small,
same file already touched this session) → Xml core (5 criticals, file already touched this
session for the memory-safety fix) → Net/IO/Xml remainder.

## Session checkpoint (2026-07-10, continued again) — wave-3 item 5: Threading critical findings, batch 1 (5 fixed)

*Branch: `feature/work`, HEAD `ca0cd96` — 11160 tests passing (up from 11152 at the top of
the item-4 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Started item 5 ("everything else, namespace by namespace") with `System.Threading`'s "Other
critical findings (13, non-Timeout.Infinite)" list, since that slice was explicitly flagged
by the wave-3 audit as "by far the largest and most severe... recommended top priority for
the next processing session." Fixed 5 of the 13, each verified against real .NET source:

- **`TaskCompletionSource<T>`/`TaskCompletionSource<void>` double-completion race + wrong
  exception type** — restructured to match `TaskCompletionSource_T.cs`'s actual shape:
  `TrySet*` is the atomic primitive (now backed by `std::atomic<bool>` +
  `compare_exchange_strong`, was a non-atomic check-then-set), and `Set*` calls `TrySet*` and
  throws `System::InvalidOperationException` (was `std::invalid_argument` — the wrong type)
  on failure. Previously, two threads racing `TrySetResult` could both pass the check and
  both call `promise_.set_value()`; the loser threw an uncaught `std::future_error` instead
  of `TrySetResult` returning `false`. Added a 200-iteration concurrent-race stress test.
  Commit `35d8a3d`.
- **`CountdownEvent::AddCount` unchecked signed-integer overflow (UB)** — verified against
  `CountdownEvent.cs`'s `TryAddCount`: added the `observedCount > (max - signalCount)`
  overflow guard (throws `InvalidOperationException`, matching
  `CountdownEvent_Increment_AlreadyMax`) before the addition, plus the missing
  `ArgumentOutOfRangeException` validation for non-positive `signalCount` on both `Signal`
  and `AddCount`. Commit `404e2b0`.
- **`Task::Wait()` never checked `isCanceled`** — a canceled task's `Wait()` returned
  silently as if it had succeeded. Verified against `Task.cs`'s
  `Wait()`/`ThrowIfExceptional(true)`/`GetExceptions(true)`: real .NET throws for a canceled
  task. Fixed to throw `TaskCanceledException` (not wrapped in `AggregateException`, matching
  this port's existing established convention of rethrowing the raw exception for the
  `isFaulted` path rather than wrapping — see `FromException_Wait_Rethrows`). One existing
  test had encoded the old silent-success behavior and needed updating. Commit `47f7908`.
- **`CancellationTokenSource::Cancel()` no try/catch around callback invocation** — verified
  against `CancellationTokenSource.cs`'s `ExecuteCallbackHandlers(throwOnFirstException:
  false)`: real .NET catches each callback's exception, runs every remaining callback
  regardless, then throws `AggregateException` at the end if any occurred. This port's
  `Cancel()` had no try/catch at all — a throwing callback aborted the loop, silently
  skipping every callback registered after it. Fixed to match; added a 3-callback regression
  test (1st and 3rd throw) proving all three still run and the exceptions aggregate. Commit
  `ca0cd96`.
- (`TaskCompletionSource`/`CountdownEvent`/`Task`/`CancellationTokenSource` above are 4 of the
  5; the 5th — `Utf8JsonWriter`/`JsonDocument` `MaxDepth` — was actually wave-3 priority item
  4, completed just before this batch; see the checkpoint below for full detail.)

### What remains from the Threading critical list (8 of 13, not yet touched)

- `ReaderWriterLock::AcquireReaderLock`/`AcquireWriterLock` silently `return` on timeout
  instead of throwing `ReaderWriterLockApplicationException`.
- `ReaderWriterLockSlim::TryEnterReadLock/WriteLock/UpgradeableReadLock(intcs)` discard the
  timeout parameter (always a single non-blocking attempt), ignores `LockRecursionPolicy`
  entirely (same-thread recursion **deadlocks** instead of throwing
  `LockRecursionException`), and has a double-`ExitReadLock()` bug that **permanently
  starves all future writers**. Largest/most invasive item on this list — likely needs its
  own session.
- `Barrier::SignalAndWait`: post-phase-action exception only seen by the triggering thread;
  `FinishPhase` invokes the post-phase action while still holding its mutex — reentrant calls
  **deadlock**.
- `TaskCompletionSource<T>` — done, see above.
- `Task::Wait()` — done, see above.
- `ValueTask(Task)` only snapshots `IsCompleted` at construction — a still-running or
  later-faulting task's exception is silently swallowed forever.
- Bounded `Channel` with `capacity == 0` (a documented legal .NET "rendezvous channel"
  configuration) **permanently deadlocks** every write instead of working.
- `AsyncLocal<T>`/`ThreadLocal<T>` destructors only clean up the destroying thread's
  `thread_local` map entry — other threads retain stale entries keyed by the (potentially
  reused) pointer.
- `LazyInitializer::EnsureInitialized<T>` uses a `static std::mutex` scoped **per template
  type**, shared across every unrelated call site initializing a different target of the
  same type — reentrant same-thread initialization of a different target **self-deadlocks**.
- `CancellationTokenSource::Cancel()` — done, see above.
- `ThreadLocal<T>::getValueProperty()` has no reentrancy guard — a factory that reentrantly
  calls it recurses unboundedly (stack overflow) instead of throwing.

Plus the 29 moderate + 10 minor Threading findings, and the remaining namespace slices
(Net core+Sockets, Net.Http+WebSockets+Security, IO core, IO.Compression+Hashing, Xml core,
Xml.Linq+XPath) entirely untouched. See the full catalogue further down this file (search for
"Full findings catalogue").

## Session checkpoint (2026-07-10, continued again) — wave-3 priority item 4 fixed (Utf8JsonWriter escaping/MaxDepth + JsonDocument MaxDepth)

*Branch: `feature/work`, HEAD `a5c34ec` — 11152 tests passing (up from 11142 at the top of
the item-3 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Fixed both parts of the wave-3 "suggested processing priority" list's item 4, each verified
against the corresponding real .NET source:

- **`Utf8JsonWriter::appendEscapedString` non-ASCII passthrough** — verified against
  `DefaultJavaScriptEncoder.cs`/`AllowedBmpCodePointsBitmap.cs`: the default
  `JavaScriptEncoder`'s allow-list is `UnicodeRanges.BasicLatin` minus undefined/control
  characters (including DEL) minus HTML-sensitive characters (`< > & ' " +`) minus explicit
  extra escapes for `\` and backtick — net effect, every codepoint ≥ U+0080 must be escaped
  as `\uXXXX` (or a `\uXXXX\uYYYY` surrogate pair for astral codepoints ≥ U+10000). The
  previous implementation only escaped control chars, `"`, `\`, and `<>&'` — non-ASCII text
  (names, i18n strings, emoji) passed straight through as raw UTF-8 bytes inside the JSON
  string, and `+`/backtick/DEL were missed even within ASCII. Added a `decodeUtf8` helper
  (same validated-decode pattern used by `ASCIIEncoding`/`UnicodeEncoding`/`UTF32Encoding`/
  `IdnMapping` elsewhere in this codebase) and rewrote `appendEscapedString` to use it. 6
  regression tests (non-ASCII, astral surrogate pair, `+`/backtick/DEL).
- **`Utf8JsonWriter`/`JsonWriterOptions.MaxDepth` never resolved from its 0 sentinel** —
  verified against `Utf8JsonWriter.cs`'s `SetOptions()`: real .NET resolves `MaxDepth == 0`
  to `JsonWriterOptions.DefaultMaxDepth` (1000) once, at construction time, so the writer's
  three depth-check call sites (all originally gated by `MaxDepth > 0 && ...`) actually
  engage. This port left `MaxDepth` at 0 forever for default-constructed writers, silently
  disabling every depth check — unbounded nesting wrote successfully with no
  `InvalidOperationException`, unlike real .NET. Added `JsonWriterOptions::DefaultMaxDepth =
  1000` and resolved it in the `Utf8JsonWriter` constructor.
- **`JsonDocument::Parse` never enforced `MaxDepth` at all** — verified against
  `Utf8JsonReader.cs` (the reader `JsonDocument.Parse` delegates depth tracking to) and
  `JsonDocumentOptions.cs` (`DefaultMaxDepth = 64`): real .NET throws once nesting reaches
  the configured/default depth. This port validated `MaxDepth >= 0` but never checked it
  against the parsed tree at all — pathologically deep documents parsed with no limit. Added
  `JsonDocumentOptions::DefaultMaxDepth = 64` and a post-parse recursive depth walk
  (`JsonDocument::checkMaxDepth`) that throws `JsonException` with .NET's exact message
  format (`Strings.resx`'s `ArrayDepthTooLarge`/`ObjectDepthTooLarge`) once the effective
  max depth is exceeded. 4 regression tests (default/custom `MaxDepth`, at-limit succeeds,
  one-over throws).

Both `Utf8JsonWriter` (commit `4ffb04e`) and `JsonDocument` (commit `a5c34ec`) fixes are
built, tested, and pushed. All four items from the "suggested processing priority" list are
now done. **Next: item 5 — everything else, namespace by namespace (~180 remaining
findings)**, same discipline as waves 1-2: verify against real .NET source, fix, add
regression tests, rebuild/retest clean, commit, push, checkpoint.

## Session checkpoint (2026-07-10, continued again) — wave-3 priority item 3 fixed (all 4 memory-safety criticals)

*Branch: `feature/work`, HEAD `8a440a2` — 11142 tests passing (up from 11136 at the top of
the "top 2 priority items" checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Fixed all four memory-safety criticals from the wave-3 "suggested processing priority" list's
item 3, each verified against the corresponding real .NET source before fixing:

- **`HttpClient::Send()` null-pointer dereference** — added
  `ArgumentNullException::ThrowIfNull(request)`, matching `HttpClient.cs`'s
  `CheckRequestBeforeSend`. Commit `dc02094`.
- **`Socket::Send/Receive/SendTo/ReceiveFrom` missing bounds validation** — added a
  `validateBufferArgs()` helper matching `Socket.Tasks.cs`'s `ValidateBufferArguments`
  exactly (casts offset/count to `uint32_t` before comparing, so a negative value is caught
  by the same range check as a too-large one). Commit `ab60037`.
- **`ClientWebSocket::SendAsync`/`ReceiveAsync` missing bounds validation** — added the
  equivalent `validateWebSocketBuffer()` helper, matching `WebSocketValidate.cs`'s
  `ValidateBuffer`; validated synchronously before the returned `Task` is constructed,
  matching real .NET's async-method-validates-synchronously convention (confirmed against
  `ManagedWebSocket.cs`). Commit `ed80e24`.
- **`XmlReader` post-EOF out-of-bounds `events[pos]` access** — 8 methods
  (`getNameProperty`, `getValueProperty`, `getIsEmptyElementProperty`, `MoveToElement`,
  `MoveToNextAttribute`, `GetAttribute`, `ReadStartElement`, `ReadEndElement`) only checked
  `pos < 0`, not the upper bound `pos >= events.size()` that `getNodeTypeProperty()` already
  had — after `Read()` returns false at EOF, `pos` sits exactly at `events.size()`, so any of
  these called after an ordinary "read until EOF" loop indexed out of bounds. Fixed all 8 to
  match `getNodeTypeProperty()`'s existing correct check. Commit `8a440a2`.

Each fix has a regression test exercising exactly the previously-broken path. All four items
from the "suggested processing priority" list's items 1-3 are now done. **Item 4
(`Utf8JsonWriter` non-ASCII escaping + `MaxDepth`/`JsonDocument` depth-limit enforcement) is
next**, followed by the remaining ~180 catalogued findings (item 5: "everything else,
namespace by namespace").

## Session checkpoint (2026-07-10, continued again) — wave-3 catalogue: top 2 priority items fixed

*Branch: `feature/work`, HEAD `367357e` — 11136 tests passing (up from 11129 at the top of
the wave-3-dispatch checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Picked off the top two items from the wave-3 "suggested processing priority" list below:

- **`ZipArchive` Update-mode data loss + `ZipArchiveEntry::Delete()` no-op** (IO.Compression
  criticals #1-2) — fixed. `flushWriter()` now extracts every pre-existing, non-deleted
  entry into memory before opening the writer (the reader must be fully drained/closed
  first, since the writer may truncate the same backing file/buffer), then writes those
  entries alongside the pending ones. Added a `deletedEntries` set so `Delete()` actually
  excludes an entry instead of doing nothing. 3 regression tests. Commit `568323a`.
- **Threading's `Timeout.Infinite` (-1) systemic mishandling** (found independently ~11
  times across `Monitor`/`Mutex`/`Semaphore`/`SemaphoreSlim`/`Lock`/`SpinWait`/
  `AutoResetEvent`/`ManualResetEvent`/`EventWaitHandle`/`ManualResetEventSlim`/
  `CountdownEvent`/`WaitHandle.WaitAll`/`WaitAny`) — fixed uniformly: every site now
  special-cases `millisecondsTimeout == -1` to call the underlying untimed blocking
  primitive instead of computing a timed wait that std::chrono treats as already-expired.
  5 regression tests (one per underlying primitive shape: mutex-like, semaphore-like,
  event-like, spin-based, multi-handle), each proving the fix by asserting the wait is
  still blocked after 100ms before signaling it to complete. Commit `367357e`.

The IO.Compression and Threading sections of the wave-3 catalogue below are otherwise
unchanged (all their other findings remain open) — only the two specific items above are
resolved. The "suggested processing priority" list's items 1-2 are done; **item 3 (memory-
safety criticals: Socket bounds validation, XmlReader post-EOF OOB access, ClientWebSocket
buffer bounds, HttpClient null deref) is next.**

## Session checkpoint (2026-07-10, continued again) — wave-3 audit dispatched, 221 findings (56 critical)

*Branch: `feature/work`, HEAD `2701f60` — 11129 tests passing (up from 11125 at the top of
part 5's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

### What happened

Per option (b) of the original sweep instruction, dispatched a wave-3 parallel audit — 9
read-only general-purpose agents, same methodology as waves 1-2 (compare each ported type's
actual logic against real .NET source in `/rv/tmp/runtime/src/libraries/`, not just its
public signature) — covering `System.Net.*` (2 agents), `System.Diagnostics*`, `System.IO.*`
(2 agents), `System.Text.Json*`, `System.Threading.*`, and `System.Xml.*` (2 agents). ~450
files audited.

**Result: 221 findings, 56 critical, 99 moderate, 66 minor.** This is far more than waves 1-2
combined and reflects that `System.Net.*`/`System.IO.*`/`System.Threading.*`/`System.Xml.*`
had never been systematically audited before (unlike `System.Globalization`/
`System.Collections`, which waves 1-2 already covered). **This volume cannot be safely
processed in one sitting** — each finding needs the same verify-against-.NET-source →
fix → build → test → commit discipline used all session, and rushing 200+ fixes risks
regressions. Only the Diagnostics slice (7 findings, 0 critical) was fixed this pass; see
below. The rest are catalogued here for systematic processing across future sessions,
exactly like waves 1-2's findings were.

### Fixed this pass: System.Diagnostics (7/7 findings)

All verified against `Stopwatch.cs`/`Stopwatch.Unix.cs`, `DebuggableAttribute.cs`,
`DebuggerBrowsableAttribute.cs`, `DebugProvider.cs`, `StackFrame.cs`. Commit `2701f60`.

- **Stopwatch**: backing clock was `std::chrono::high_resolution_clock`, which on this
  project's toolchain (libstdc++/GCC) is an alias for `system_clock` (wall clock, not
  monotonic) — real .NET's `GetTimestamp()` is `clock_gettime(CLOCK_MONOTONIC)`. Switched to
  `std::chrono::steady_clock`, matching `PeriodicTimer`/`WaitHandle`/`SpinWait`/`Thread`'s
  existing convention.
- **DebuggableAttribute**: `IsJITTrackingEnabled`/`IsJITOptimizerDisabled` were stored bools
  only populated by the `(bool,bool)` constructor, leaving them wrong (always false) when
  built via the `DebuggingModes`-flags constructor. Fixed by deriving both from
  `debuggingModes_` live (bit-test), matching .NET's computed-property design.
- **DebuggerBrowsableAttribute**: added the missing `state < Never || state > RootHidden`
  range validation (note: this does NOT specially reject the removed `Expanded=1` — it falls
  within range, so real .NET's check silently accepts it too; a candidate test asserting
  otherwise was corrected after failing against the actual verified behavior).
- **Debug::Assert(bool)**: called the raw `assert()` macro instead of routing through
  `Fail()`/the active `DebugProvider` like the message-carrying overloads — fixed to
  delegate to `Assert(condition, "")`.
- **DebugProvider::Fail()**: used `assert(false)` (a no-op under `NDEBUG`), contradicting its
  own doc comment and .NET's `[DoesNotReturn]` contract — switched to `std::abort()`
  unconditionally (kept as a plain virtual, not `[[noreturn]]`, since it's legitimately
  overridable by a non-aborting custom provider).
- **StackFrame**: `nativeOffset_` defaulted to `0` instead of `OFFSET_UNKNOWN` (`-1`);
  `ilOffset_` two lines below already correctly defaulted to `-1`. Fixed.
- (Not fixed — deferred) `Debugger::getIsAttachedProperty()` is hardcoded `false`; a real fix
  needs a new `.cpp` parsing `/proc/self/status`'s `TracerPid` on Linux (POSIX-only, per
  CLAUDE.md's platform-abstraction rule) plus a cmake reconfigure for the new file. Low
  priority (minor severity), noted here as an easy follow-up.

### Full findings catalogue (not yet fixed) — 214 findings, 56 critical

Organized by audited slice. File paths are relative to the repo root. Severity counts are
per-slice as reported by each audit agent.

#### System.Net core + Sockets (24 findings: 4 critical, 12 moderate, 8 minor)

**Critical:**
1. `SocketError` is never translated from POSIX `errno` — every `SocketException` carries a
   meaningless error code on Linux (raw errno reinterpreted as the enum, e.g. `ECONNREFUSED`
   =111 read as if it were the Winsock-numbered `SocketError` value). Standard idioms like
   `catch (e) { if (e.SocketErrorCode == SocketError::WouldBlock) ... }` never match. Needs
   an errno→`SocketError` translation table; keep raw errno as a separate native-code field.
2. `Socket::Send/Receive/SendTo/ReceiveFrom` have no offset/count bounds validation — real
   OOB heap read/write when `offset+count > buffer.size()` (`src/System/Net/Sockets/Socket.cpp:497-576`).
3. `IPAddress` IPv4 parsing uses `sscanf("%u.%u.%u.%u%c")` — UB on segment overflow (a
   sufficiently long digit run is UB per C11 7.21.6.2p10), and accepts input real .NET
   rejects (`sscanf`'s optional sign) while rejecting input .NET accepts (octal/hex segments,
   short forms). `src/System/Net/IPAddress.cpp:23-30`.
4. `WebUtility::UrlDecode` throws an uncaught `std::invalid_argument` (not a `System::`
   exception) on malformed percent-encoding, e.g. `"100%complete"`.
   `include/System/Net/WebUtility.hpp:147`.

**Moderate (12) and minor (8)**: Socket setters skip .NET's validation (`ExclusiveAddressUse`
after bind, negative timeouts, negative buffer sizes); `TcpClient`/`TcpListener`/`UdpClient`
are effectively IPv4-only (IPv6 throws, hostname resolution doesn't fall back);
`SocketFlags` cast directly to native flags with no translation (bit patterns don't match
Linux, e.g. .NET's `Truncated` coincides with Linux `MSG_WAITALL`); IPv6 scope-ID parsing can
throw uncaught `std::out_of_range`; `IsLoopback` misses `::ffff:127.0.0.1`; `IPEndPoint`
integer constructors silently truncate instead of throwing; `Dns` doesn't reject
`0.0.0.0`/`::`; `WebUtility::HtmlDecode`/`UrlEncode` have wrong entity-scan/safe-char-set
logic; `TcpListener::Start()` hardcodes backlog 5 instead of `int.MaxValue`;
`Socket::Accept()` skips bound/listening validation; `IPAddress::GetHashCode()` only hashes
the first 64 bits of IPv6 (hash-table degradation for same-`/64`-prefix addresses); plus 8
minor items (embedded-IPv4 formatting, scope-ID range validation, bracketed-IPv6 parsing,
hostname length check, `NetworkStream` post-close silent no-op, etc.) — see the full
per-finding detail in the agent's original report (not preserved verbatim here; re-run a
similar audit prompt on `include/System/Net/*.hpp` + `Sockets/*.hpp` to regenerate if needed).

#### System.Net.Http + WebSockets + Security + Mime + NetworkInformation (19 findings: 2 critical, 13 moderate, 4 minor)

**Critical:**
1. `HttpClient::Send()` dereferences a null `request` immediately with no
   `ArgumentNullException` check — null-pointer dereference (UB/crash) instead of a catchable
   exception. `src/System/Net/Http/HttpClient.cpp`.
2. `ClientWebSocket::SendAsync`/`ReceiveAsync` do `buffer.data() + offset` with no
   offset/count bounds validation against `buffer.size()` — out-of-bounds read/write.
   `src/System/Net/WebSockets/ClientWebSocket.cpp:334-411`.

**Moderate (13) highlights**: `HttpClient::parseUrl` throws `std::invalid_argument` instead
of a `System::` exception (same systemic std::/System:: bug noted elsewhere in this
project's history); `HttpResponseMessage` skips status-code range validation; quality
(`q=`) header parsing accepts NaN/malformed values via unguarded `std::stod`;
`CacheControlHeaderValue` seconds parsing can silently overflow-wrap; several header getters
accept negative values .NET rejects; `ClientWebSocketOptions` subprotocol validation is too
permissive and case-sensitive where .NET is case-insensitive; `ClientWebSocket` doesn't
validate message type or close-status codes before sending; `NetworkInterface.GetIsNetworkAvailable()`
misses the Tunnel-interface exclusion.

**Minor (4)**: `Content-Length` accepts a leading `+`; `SslApplicationProtocol.ToString()`
doesn't implement its documented hex-dump fallback; `ValueWebSocketReceiveResult` skips
`messageType` range validation; `Ping::Send` throws `ArgumentException` instead of
`ArgumentNullException` for a null/empty host.

#### System.IO core (22 findings: 7 critical, 10 moderate, 5 minor)

**Critical:**
1. `MemoryStream::Write()`/`WriteByte()` silently no-op instead of throwing
   `NotSupportedException` when `!CanWrite` — writes are silently dropped.
   `src/System/IO/MemoryStream.cpp:28-43`. (`SetLength()` in the same file gets this right —
   internal inconsistency confirms it's a bug.)
2. `File::Move`/`Directory::Move`/`DirectoryInfo::MoveTo`/`FileInfo::MoveTo` all silently
   overwrite an existing destination via unconditional `std::filesystem::rename()`; real
   .NET's non-overwrite `Move` throws `IOException` if the destination exists.
3. `FileStream::getLengthProperty()` returns a stale cached length after `Write()` extends
   the file — only `SetLength()` updates the cache. Common create-then-write pattern returns
   wrong (often 0) `Length`. `src/System/IO/FileStream.cpp:49,102,152`.
4. `StreamReader::Close()`/`StreamWriter::Close()` ignore `leaveOpen` and unconditionally
   close the underlying stream, defeating the flag's entire purpose. (Their destructors and
   `BinaryReader`/`BinaryWriter::Close()` get this right.)

**Moderate (10) highlights**: `File::Delete` can delete a directory (uses
`std::filesystem::remove()`, which dispatches to `rmdir()`); `RandomAccess::Write` doesn't
loop on short/partial writes; `Directory::GetFiles(path, "*.*")` wildcard excludes
extensionless files (wrong DOS_DOT compatibility); `FileSystemInfo`'s local-time properties
return UTC verbatim instead of converting; generic `IOException` instead of
`DirectoryNotFoundException` on missing parent directory; `MemoryStream::Read()`/`Close()`
have more silent-wrong-behavior bugs (returns 0 on invalid args instead of throwing;
`Close()` destroys the buffer, contradicting its own doc comment and .NET's `Dispose`);
`UnmanagedMemoryStream` throws the wrong exception type on a closed stream;
`StreamReader`/`StreamWriter` constructors skip null/`CanRead`/`CanWrite` validation;
`BinaryWriter` doesn't flush on Close when `leaveOpen=true`; `StreamReader::ReadLine()`/
`StringReader::ReadLine()` don't treat a lone `'\r'` as a line terminator.

**Minor (5)**: `Path::IsPathRooted` Windows-build inconsistency (not exercised on Linux);
`FileSystemWatcher` filter normalization; `FileSystemEventArgs::Combine()` alt-separator
handling; `PathTooLongException` missing HResult; `BinaryReader::Read` missing a null check.

#### System.IO.Compression + Hashing + IsolatedStorage (13 findings: 4 critical, 6 moderate, 3 minor)

**Critical — this is the highest-value fix in the entire wave-3 catalogue (real, silent data loss):**
1. `ZipArchive::flushWriter()` (Update-mode `Dispose()`) only writes newly-`CreateEntry`'d
   entries — for a file-backed archive it re-inits the writer on the same path opened for
   reading, so **disposing an Update-mode archive after even one `CreateEntry()` call
   overwrites the file, discarding every pre-existing entry**. `src/System/IO/Compression/ZipArchive.cpp:154-186,280-291`.
2. `ZipArchiveEntry::Delete()` does `state_ = nullptr;` only — never marks anything for
   exclusion, so the "deleted" entry is fully intact in the persisted output regardless.
   `src/System/IO/Compression/ZipArchive.cpp:123-129`.
3. `DeflateStream`/`GZipStream` constructors/`Read()`/`Write()` throw bare
   `std::runtime_error` on zlib failures instead of `System::` exception types — uncatchable
   by code catching `System::Exception&`/`IOException&`. Sibling `ZLibStream.cpp` already
   does this correctly, confirming it's a bug not a design choice.

**Moderate (6)**: rest of `ZipArchive`'s failure paths also throw bare `std::runtime_error`;
`GZipEncoder::GetMaxCompressedLength()` omits the `+12` gzip header/trailer overhead
(understates worst-case buffer size); `IsolatedStorageFile` methods skip path-null/empty
validation; `DeleteDirectory()` is always recursive (`remove_all`) where .NET's non-recursive
default throws `IOException` on a non-empty directory; disposed-state (`disposed_`) is set
but never checked anywhere, so all operations remain usable after `Close()`/`Remove()`;
`IsolatedStorageFile::getQuotaProperty()` isn't overridden, so it inherits the base's `0`
instead of real .NET's `long.MaxValue`.

**Minor (3)**: `DeflateEncoder::GetMaxCompressedLength()` truncates on >4GiB input (32-bit
`compressBound`); Application-scope and Assembly-scope isolated storage share the same root
(not actually isolated from each other, though documented); `IsolatedStorageException`
never sets its HResult.

**Hashing: 0 findings.** CRC32/CRC32C/CRC64-ECMA182/XxHash32/64/3/128 constants, polynomials,
seeds, bit/byte order, and output-endianness convention were all independently verified
against `Crc32ParameterSet.WellKnown.cs`/`Crc64ParameterSet.WellKnown.cs` and found correct.

#### System.Text.Json (26 findings: 7 critical, 12 moderate, 7 minor)

**Critical:**
1. `Utf8JsonWriter`'s string escaping never escapes non-ASCII characters (only control
   chars, `"`, `\`, `<>&'`) — real .NET's default encoder escapes every codepoint ≥U+0080 as
   `\uXXXX`. Any non-ASCII string (names, i18n text, emoji) round-trips unescaped instead of
   matching .NET's byte-for-byte output. `src/System/Text/Json/Utf8JsonWriter.cpp:37-66`.
2. `Utf8JsonWriter`'s `MaxDepth` default of `0` enforces no limit at all (`options_.MaxDepth
   > 0 && ...` guards are always skipped at the default) — real .NET resolves `0` to 1000.
   Unbounded native recursion / stack-overflow risk on deeply-nested writes with default
   options.
3. `JsonDocument::Parse` never enforces `MaxDepth` at all — arbitrarily deep/malicious input
   parses with no bound (verified: 5000-level nesting parses fine) instead of throwing
   `JsonException`, risking a native stack overflow.
4. `JsonElement::TryGetInt32`/`TryGetInt64` round-trip through `double` — UB when casting an
   out-of-range double to a signed integer, plus precision loss for large int64 values
   (doubles only exactly represent integers up to 2^53). The non-`Try` `GetInt64()` sibling
   already does this correctly.
5. `JsonElement::GetProperty` always throws `KeyNotFoundException`, even when called on a
   non-object element, where real .NET throws `InvalidOperationException` — breaks
   catch-block discrimination between "wrong shape" and "missing key".
6. `JsonObject`/`JsonArray` `Add`/`SetItem`/`Insert` have no "already has a parent" or cycle
   check — a node can be silently attached to two containers at once (dangling non-owning
   `parent_` pointer → use-after-free risk) or become its own ancestor (unbounded recursion
   in `ToJsonString`/`DeepClone`/etc.).
7. `JsonObject::operator[]` throws `KeyNotFoundException` for a missing key; real .NET
   returns `null` — breaks the single most common .NET JSON-node idiom (`obj["maybe"]`
   null-check pattern).

**Moderate (12) highlights**: `WriteNumberValue(double)` doesn't reject NaN/Infinity (nlohmann
silently emits `null` instead); missing ASCII escapes for `+`, backtick, DEL;
`AllowTrailingCommas`/`AllowDuplicateProperties` (JsonDocumentOptions) are validated but
never actually enforced by the underlying nlohmann parse; `GetRawText()` reformats numbers
instead of returning exact source text; `GetInt32`/`GetInt64` too lenient (accepts `3.0`/`2e1`
where .NET's strict digit parser throws); `JsonSerializerOptions.AllowDuplicateProperties`
defaults `false` where .NET defaults `true` (and the sibling `JsonDocumentOptions` version in
the *same codebase* correctly defaults `true` — confirms this is an oversight);
`JsonSerializerOptions(Strict)` is a silent no-op (falls through to `General` behavior);
`JsonEncodedText::Encode` doesn't validate/pre-escape its input, defeating its whole purpose;
`JsonNodeOptions.PropertyNameCaseInsensitive` is stored but never consulted;
`JsonValue` accessors throw `FormatException` instead of `InvalidOperationException`;
`JsonPolymorphicAttribute.UnknownDerivedTypeHandling` is typed `bool` instead of the
3-valued enum that already exists elsewhere in the same directory.

**Minor (7)**: double formatting/hex-escape casing cosmetically diverges from .NET (valid
JSON either way); `JsonWriterOptions.NewLine` hardcoded `"\n"` (only observable on Windows);
`JsonException` doesn't append position info to its message text; `WriteRawValue` validates
in the wrong order; several `JsonElement` temporal/numeric getters (`GetGuid`, `GetDateTime`,
`GetDecimal`, etc.) don't exist yet (API-surface gap, not a wrong-value bug);
`GetString()` on JSON `null` returns `""` (self-documented as an intentional deviation).

#### System.Threading + Tasks + Channels (63 findings: 24 critical, 29 moderate, 10 minor)

**By far the largest and most severe slice — a systemic bug pattern plus several real
deadlocks/data races/UB. Recommended top priority for the next processing session.**

**Systemic critical pattern (found independently ~11 times): `Timeout.Infinite` (`-1`) is
never special-cased before being fed into `std::chrono::milliseconds(-1)`/`wait_for`, which
the standard treats as an already-expired deadline** — these return almost immediately
instead of blocking forever, unlike .NET where `-1` means infinite wait:
`Monitor::TryEnter`, `Mutex::WaitOne`, `Semaphore::WaitOne`, `SemaphoreSlim::Wait`,
`Lock::TryEnter`, `SpinWait::SpinUntil`, `AutoResetEvent::WaitOne(intcs)`,
`ManualResetEvent::WaitOne(intcs)`, `EventWaitHandle::WaitOne(intcs)`,
`ManualResetEventSlim::Wait(intcs)`, `CountdownEvent::Wait(intcs)`,
`WaitHandle::WaitAll`/`WaitAny`. `ReaderWriterLock::AcquireReaderLock/WriterLock` in the same
file correctly special-cases `<0`, proving this is an inconsistency bug, not a design
choice. **Fix direction: a single shared helper (`WaitInfinite`-aware) used by all of these
would likely fix most of the pattern in one well-scoped pass** — worth tackling as a batch
given how mechanically similar each site is, rather than 11 separate one-off fixes.

**Other critical findings (13, non-Timeout.Infinite):**
- `ReaderWriterLock::AcquireReaderLock`/`AcquireWriterLock` silently `return` on timeout
  instead of throwing `ReaderWriterLockApplicationException` — callers proceed without
  holding the lock.
- `ReaderWriterLockSlim::TryEnterReadLock/WriteLock/UpgradeableReadLock(intcs)` discard the
  timeout parameter entirely (always a single non-blocking attempt); also ignores
  `LockRecursionPolicy` entirely, so same-thread recursion **deadlocks** instead of throwing
  `LockRecursionException`; recursive `EnterReadLock()` has a bug where the second matching
  `ExitReadLock()` throws (`unordered_set` membership-only tracking, not a count) and
  **permanently starves all future writers**.
- `Barrier::SignalAndWait`: when the post-phase action throws, only the triggering thread
  sees `BarrierPostPhaseException` — every other participant of that phase silently proceeds
  as if it succeeded. `Barrier::FinishPhase` invokes the post-phase action while still
  holding its mutex — reentrant calls **deadlock** instead of throwing
  `InvalidOperationException`.
- `CountdownEvent::AddCount` has unchecked signed integer overflow (UB) — same bug class as
  the already-fixed TimeSpan copy_count/move_count race from earlier this session, but for
  overflow rather than a data race.
- `TaskCompletionSource<T>` completion flag is a plain non-atomic `bool` — concurrent
  `TrySet*` calls race (UB); the loser throws an uncaught `std::future_error` instead of
  returning `false` as .NET guarantees.
- `Task::Wait()` never checks `isCanceled` — a canceled task's `Wait()` returns silently as
  if it succeeded.
- `ValueTask(Task)` only snapshots `IsCompleted` at construction — a still-running or
  later-faulting task's exception is silently swallowed forever.
- Bounded `Channel` with `capacity == 0` (a documented legal .NET "rendezvous channel"
  configuration) **permanently deadlocks** every write instead of working.
- `AsyncLocal<T>`/`ThreadLocal<T>` destructors only clean up the destroying thread's
  `thread_local` map entry — other threads retain stale entries keyed by the (potentially
  reused) pointer, risking data corruption from a heap-allocated instance at the same
  address.
- `LazyInitializer::EnsureInitialized<T>` uses a `static std::mutex` scoped **per template
  type**, shared across every unrelated call site initializing a different target of the
  same type — unnecessary serialization, and reentrant same-thread initialization of a
  different target **self-deadlocks**.
- `CancellationTokenSource::Cancel()` has no try/catch around callback invocation — a
  throwing callback silently skips all remaining callbacks instead of running all of them
  with exceptions aggregated into `AggregateException` (`AggregateException` already exists
  in this codebase).
- `ThreadLocal<T>::getValueProperty()` has no reentrancy guard — a factory that reentrantly
  calls it recurses unboundedly (stack overflow) instead of throwing.

**Moderate (29) and minor (10)**: extensive list covering `Monitor`/`SpinLock` validation
gaps, `Semaphore` constructor argument-order/exception-type mismatches,
`ReaderWriterLock(Slim)` exception-type mismatches, `CountdownEvent`/`Barrier` validation and
`Dispose()` no-ops, `Timer`/`PeriodicTimer` range validation, `Task`/`TaskCompletionSource`
exception-wrapping gaps (`AggregateException` not used where .NET wraps), `Channel`/`Parallel`
exception-swallowing during concurrent failures, `CancellationTokenSource.disposed_` data
race, non-LIFO callback ordering, `SynchronizationContext` being a fully broken no-op
round-trip, and more. See the original agent transcript (session `c84efd8a-...`, task
`a058cd3c7809221ea`) for the complete per-item detail if reprocessing without a fresh audit.

#### System.Xml core (26 findings: 5 critical, 13 moderate, 8 minor)

**Critical:**
1. `XmlReader.cpp` — most accessors (`getNameProperty`, `getValueProperty`,
   `MoveToElement`, `ReadStartElement`, `ReadEndElement`, etc.) only guard `pos < 0`, not
   `pos >= events.size()` — out-of-bounds `std::vector` access (UB/crash) after `Read()`
   returns `false` (EOF). Only `getNodeTypeProperty()` has the upper-bound check.
2. `XmlConvert::ToString`/`ToDouble`/`ToSingle` use .NET `Double`'s `"Infinity"`/`"-Infinity"`
   tokens instead of the XML Schema lexical-space `"INF"`/`"-INF"` real `XmlConvert` uses —
   produces invalid-per-schema output and fails to parse valid schema input.
3. `XmlNode::getInnerXmlProperty()`/`getOuterXmlProperty()` inject pretty-print whitespace
   (tinyxml2 `XMLPrinter` defaults to `compact=false`) where real .NET's `InnerXml`/`OuterXml`
   serialize exact markup with no inserted whitespace.
4. `XmlAttribute::getNamespaceURIProperty()` always returns `""` — never sets `native_`, and
   the base class's namespace-resolution walk (`native_->Parent()`) is always null for
   attributes with no override. Breaks any prefixed attribute and
   `XmlNamedNodeMap::GetNamedItem(localName, namespaceURI)`.
5. `XmlAttribute::CloneNode()` always returns `nullptr` (inherits the base's null-`native_`
   early-return, never overridden) — cloning any attribute silently fails.

**Moderate (13) highlights**: CDATA reported as plain Text; Processing Instructions and
DOCTYPE silently vanish during reading (tinyxml2 `XMLUnknown`, no branch handles it); wrong
self-closing/EndElement detection (`!FirstChild()` instead of tinyxml2's `ClosingType()`) —
an explicitly-closed empty element gets no EndElement event; `XmlWriter::ToString()` ignores
the default `Indent=false` (always pretty-prints); `WriteComment`/`WriteProcessingInstruction`/
`WriteCData` skip well-formedness validation .NET performs (`"--"`, `"?>"`, `"]]>"`); 
`XmlNamespaceManager::AddNamespace` skips reserved-prefix validation;
`RemoveChild`/`AppendChild`/etc. skip .NET's ancestor-cycle/cross-document/legal-child-type
validation (tree-corruption risk); `Normalize()` doesn't recurse into child elements;
`XmlDeclaration`/node-creation APIs skip version/standalone/XML-Name validation that real
.NET performs via `ValidateNames`/`ParseNmtoken`.

**Minor (8)**: `XmlException` message text formatting differences; parse errors lose
line/position info; `XmlResolver` relative-path promotion gap; `XmlNamespaceManager`
prefix-shadowing/tie-break nondeterminism; `XmlDeclaration.Value` has a leading-space bug
(`substr(3)` vs `substr(4)`); `XmlAttributeCollection` insertion-order methods silently
degrade to Append; apostrophe over-escaping in attributes; stray trailing space in empty
`XmlProcessingInstruction` data.

#### System.Xml.Linq + XPath (21 findings: 3 critical, 12 moderate, 6 minor)

**Critical:**
1. `XContainer::InsertNodeAt` has no cycle/self-containment guard — adding an
   ancestor/self into its own subtree creates a genuine `shared_ptr` reference cycle
   (permanent leak) and stack-overflows any recursive traversal.
2. XPath relational operators (`<`,`<=`,`>`,`>=`) use lexicographic **string** comparison
   instead of numeric comparison whenever a node-set operand is involved — per XPath 1.0
   §3.4 these must always be numeric. Example: `@count > 9` where `@count` is `"10"` gives
   the wrong answer (`"10" < "9"` lexicographically). This is silently-wrong output for
   fully-"supported" XPath, not an unsupported-feature gap. `src/System/Xml/XPath/XPathAstInternal.cpp:709-747`.
3. Namespaced `XAttribute`/`XElement` serialize as malformed Clark-notation XML
   (`{http://ns}local` written literally as an attribute *name*, which is not valid XML) —
   save-then-reload of any namespaced attribute silently corrupts or drops it.

**Moderate (12) highlights**: `XName::Get` doesn't validate malformed Clark notation and
splits on the first `}` instead of the last; `XAttribute` skips .NET's namespace-declaration
validation rules; attribute-value escaping doesn't handle `\t`/`\r`/`\n` (collapses on
reload); `IsNamespaceDeclaration` is missing entirely; `DeepEqualsCore` compares attributes
as an unordered set where .NET compares positionally; `XElement` has no `ValidateNode`
override (an `XDocument` can be added as a child element); a large set of documented XLinq
tree-editing API is entirely absent (`AddBeforeSelf`, `SetAttributeValue`, ~20 conversion
operators, etc. — compile-time gap, not runtime misbehavior, but means these types don't
meet the "full public API" bar for `ported` status); `XElement::WriteTo` silently drops the
element's namespace URI; `Add(std::string)` never merges into a trailing `XText` sibling;
`XDocument::ValidateNode` uses the wrong exception type and over-rejects whitespace text;
`XDocument::WriteTo` doesn't match .NET's start/end-document contract; XPath `number()`
accepts exponent notation, which real XPath 1.0 rejects as NaN.

**Minor (6)**: `XName` constructors skip NCName validation; `XAttribute.EmptySequence`
missing; `XDeclaration.ToString` version-omission difference; `XDocumentType` skips name
validation; `DeepEqualsCore` skips Comment/PI nodes (matches a stale doc comment, not .NET's
actual behavior); XPath `string-length()` uses byte length not character count.

### Suggested processing priority for the next session

1. **`ZipArchive` Update-mode data loss + `ZipArchiveEntry::Delete()` no-op** (IO.Compression
   #1-2) — real, silent, irreversible data loss on a standard workflow. Highest-value single
   fix in this catalogue.
2. **Threading's `Timeout.Infinite` systemic pattern** (~11 sites, one shared root cause) —
   high finding-count-per-fix ratio if solved with a shared helper.
3. **Memory-safety criticals**: `Socket::Send/Receive` bounds validation, `XmlReader`
   post-EOF OOB access, `ClientWebSocket` buffer bounds, `HttpClient::Send` null deref —
   all genuine UB/crash bugs reachable from common usage, not just parity gaps.
4. **`Utf8JsonWriter` non-ASCII escaping + `MaxDepth`/`JsonDocument` depth-limit enforcement**
   — silent wrong output / unbounded-recursion risk in commonly-exercised JSON paths.
5. Everything else, namespace by namespace, same discipline as waves 1-2.

No `plan.sqlite3` tickets were created for these 214 individual findings (the volume doesn't
warrant per-finding ticket rows); process them directly from this NEXT.md catalogue, and
update/close the relevant existing `ported-type-audit` tickets (`Verify ported type: ...`)
as each type's findings are resolved, following the established pattern.

---

*Branch: `feature/work`, HEAD `364787f` — 11125 tests passing (up from 11111 at the top of
part 4's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

The last deferred-findings item is now fixed, closing out the entire wave-2 "found but
deliberately NOT fixed" list from parts 1-4 below:

- **Collections.Immutable — `ImmutableHashSet`/`ImmutableSortedSet`/`ImmutableSortedDictionary`
  custom-comparer support**: added, verified against `ImmutableHashSet_1.cs`/
  `ImmutableSortedSet_1.cs`/`ImmutableSortedDictionary_2.cs`. The comparer is stored as
  `std::function` per-instance rather than as a template parameter (matches .NET's
  runtime-object `IEqualityComparer<T>`/`IComparer<T>`, keeps every existing call site
  source-compatible). Added `Create(comparer[, items])` and `WithComparer(s)(...)` to all
  three types. Centralized every "fresh empty container" construction through a
  `makeEmpty()` helper per type — `std::unordered_set`/`std::set`/`std::map`'s own
  default-constructed `std::function` comparator is empty and throws
  `std::bad_function_call` on first use, so any skipped path would compile fine and crash
  at runtime. **While writing tests with a genuinely discriminating comparer
  (case-insensitive strings — a reverse-order int comparator doesn't actually change
  equivalence classes, so it can't catch this class of bug), found and fixed a real
  comparer-precedence bug**: `Intersect`/`Except`/`IsSubsetOf`/`IsSupersetOf` on both
  `ImmutableHashSet` and `ImmutableSortedSet` tested membership via `other`'s comparer
  instead of `this`'s — verified wrong against the actual .NET source for all 4 methods on
  both types (.NET consistently rehashes/tests `other`'s raw elements under *this* set's
  comparer). `Union`/`SymmetricExcept`/`Overlaps` were already correct (verified against the
  same source, no fix needed). Added 20 regression tests. Commit `364787f`.

### Deferred-findings sweep: complete

Every item from the wave-2 audit's "found but deliberately NOT fixed" list (see the part-1
checkpoint far below) has now been addressed across parts 1-5: Group.Name, ASCIIEncoding,
ImmutableArray.IsDefault, ReadOnlyCollection live-view, NotifyCollectionChangedEventArgs
validation, HybridDictionary (verified no-op), NumberFormatInfo validation, RegionInfo
constructor/LCID validation, IdnMapping (5 gaps), Immutable{,Sorted}Dictionary
duplicate-key-same-value, CultureInfo (LCID/ISO names/NumberFormat/DateTimeFormat/Equals/
GetHashCode/ToString/GetCultureInfo), PersianCalendar (astronomical algorithm — the largest
item), and now Immutable{HashSet,SortedSet,SortedDictionary} custom-comparer support.

Only **UTF8Encoding** remains untouched, and it stays that way deliberately: its ticket is
marked `needs_user` (real fix needs `DecoderFallback`/`EncoderFallback` infrastructure) —
seek clarification rather than attempt blind.

### Next session: option (b) from the original sweep instruction

Dispatch a wave-3 parallel audit covering `System.Net.*`, `System.Diagnostics*`,
`System.IO.*`, `System.Text.Json*`, `System.Threading.*`, `System.Xml.*`, using the same
methodology as waves 1-2 (parallel read-only audit agents, then verify-against-.NET-source-
before-fixing for each finding).

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 4 (PersianCalendar, largest item)

*Branch: `feature/work`, HEAD `9492dea` — 11111 tests passing (up from 11094 at the top of
part 3's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

The single largest deferred finding from the whole sweep is now fixed:

- **Globalization — `PersianCalendar`**: replaced the fixed 33-year arithmetic leap-year
  formula (`(year*8+29)%33 < 8`, diverges from real .NET on ~29% of years) with a faithful
  C++ port of `CalendricalCalculationsHelper.cs`'s full astronomical vernal-equinox
  algorithm — VSOP-based solar longitude (49 periodic terms), ephemeris correction for
  Earth's rotation slowdown (6 correction formulas keyed by Gregorian year range), equation
  of time, and the `PersianNewYearOnOrBefore` search that locates the actual equinox
  crossing at the Persian observation site (52.5°E). Every constant/coefficient/formula is
  copied verbatim from the .NET source, including a 0-based-vs-1-based day-numbering
  mismatch present in the real source (`GetNumberOfDays` vs. `numDays = ticks/TicksPerDay +
  1`) — preserved as-is rather than "corrected," since the job was a faithful translation,
  not a redesign. Also fixed in the same pass, all verified against the same reference
  files: `MaxSupportedDateTime` was `DateTime(9999,12,31)`, real .NET's is
  `DateTime.MaxValue` verbatim; `GetDayOfYear`/`IsLeapDay` had no overrides so they silently
  used the `Calendar` base class's Gregorian-specific defaults (wrong day-of-year, wrong
  Feb-29 leap-day check instead of Persian month-12-day-30); `GetMonthsInYear`/
  `GetLeapMonth`/`IsLeapMonth`/`GetEra`/`IsLeapYear`/`GetDaysInMonth`/`GetDaysInYear` had no
  input validation at all (now throw `ArgumentOutOfRangeException` matching the rest of the
  calendar family, plus the `MaxCalendarYear`(9378)/`Month`(10)/`Day`(13) boundary special
  cases); `TwoDigitYearMax`'s setter and `AddMonths` accepted any int with no range
  validation. Verified via a compiled scratch reproduction (not committed) before writing
  permanent tests: 92572 + 3288 round-trip checks (0 failures) spanning the full supported
  range plus dense modern-year sampling, cross-checked against public Nowruz dates and the
  well-documented Iranian Revolution date conversion (1979-02-11 = 1357-11-22, "22 Bahman").
  Added 17 regression tests. Commit `9492dea`.

### What remains from the deferred-findings list

Only two items, both with their own reason for not being folded into this sweep:

- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero
  well-formedness validation. Ticket already marked `needs_user` (would need real
  `DecoderFallback`/`EncoderFallback` infrastructure) — skip or seek clarification rather
  than attempt blind.
- **Collections.Immutable**: `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere on any constructor/factory). This is a
  real feature addition (new constructor/factory overloads across 3 types), not a bug fix
  like everything else in this sweep — worth scoping as its own task rather than folding
  into "deferred findings."

Every other item from the original wave-2 "found but deliberately NOT fixed" list (see the
part-1 checkpoint far below) has now been addressed across parts 1-4 of this sweep:
Group.Name, ASCIIEncoding, ImmutableArray.IsDefault, ReadOnlyCollection live-view,
NotifyCollectionChangedEventArgs validation, HybridDictionary (verified no-op),
NumberFormatInfo validation, RegionInfo constructor/LCID validation, IdnMapping (5 gaps),
Immutable{,Sorted}Dictionary duplicate-key-same-value, CultureInfo (LCID/ISO
names/NumberFormat/DateTimeFormat/Equals/GetHashCode/ToString/GetCultureInfo), and now
PersianCalendar.

The next session should either scope and implement the `Immutable*` custom-comparer support,
or move to option (b) from the original sweep instruction: dispatch a wave-3 parallel audit
covering `System.Net.*`, `System.Diagnostics*`, `System.IO.*`, `System.Text.Json*`,
`System.Threading.*`, `System.Xml.*`, using the same methodology as waves 1-2.

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 3

*Branch: `feature/work`, HEAD `ee0fefc` — 11094 tests passing (up from 11064 at the top of
part 2's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

One large item fixed this pass:

- **Globalization — `CultureInfo`**: this was the largest remaining deferred finding
  (ticket 786). Added, all verified against `CultureInfo.cs`: `CultureInfo(int)` LCID
  validation via a `ValidateLcidStub` helper (rejects the 5 LCIDs .NET rejects
  unconditionally -- `CultureNotFoundException` was previously dead code, never thrown
  anywhere in the codebase); `EnglishName`/`NativeName`/`DisplayName` (real values for the
  two cultures this port meaningfully models -- invariant and "en-US" -- documented
  best-effort fallback to `Name` for any other culture); `TwoLetterISOLanguageName` (real
  values for invariant/en-US, heuristic BCP-47-subtag derivation otherwise, documented as
  not a real ISO-639 lookup); `ThreeLetterISOLanguageName` (real values for invariant/en-US
  only -- no derivable value for any other name, since the three-letter form isn't a
  transform of the culture name); `NumberFormat`/`DateTimeFormat` instance properties
  backed by a per-instance copy of the invariant info, with `VerifyWritable()`-guarded
  setters; `Equals`/`GetHashCode`/`ToString` (Name-based; documented CompareInfo deviation,
  since this port doesn't model per-culture CompareInfo data); and
  `GetCultureInfo(string)`/`GetCultureInfo(int)`/`GetCultureInfo(string, bool)` (return a
  read-only instance -- the real behavioral difference from the public constructors; no
  object-identity caching, since this port uses value semantics throughout -- documented
  deviation from .NET's cached-singleton behavior). Added 30 regression tests. Commit
  `ee0fefc`.

### What remains from the deferred-findings list (not yet touched)

- **PersianCalendar**: fixed 33-year arithmetic leap-year formula vs. real astronomical
  vernal-equinox algorithm — diverges on ~29% of years. The single most involved remaining
  item across all three parts of this sweep; likely needs a substantial algorithm rewrite.
  Not started.
- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero
  well-formedness validation. Ticket already marked `needs_user` — skip or seek
  clarification rather than attempt blind.
- **Collections.Immutable**: `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere on any constructor/factory). Not
  started — would need API additions across 3 types.

At this point every deferred finding from the wave-2 audit checkpoint has been addressed
except the three items above. The next session should either finish those three (PersianCalendar
is the only genuinely large one left) or move to option (b): dispatch a wave-3 parallel audit
covering `System.Net.*`, `System.Diagnostics*`, `System.IO.*`, `System.Text.Json*`,
`System.Threading.*`, `System.Xml.*`, using the same methodology as waves 1-2.

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 2

*Branch: `feature/work`, HEAD `79f25bc` — 11064 tests passing (up from 11037 at the top of
part 1's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Continuation of part 1 immediately below. Two more deferred findings fixed:

- **Globalization — `IdnMapping`**: fixed all 5 gaps in one pass (verified against
  `IdnMapping.cs`): `UseStd3AsciiRules` was a no-op, now enforced via `validateStd3Char`;
  `LabelMax` (63) wasn't enforced on input/encoded/raw-ACE label lengths, now is;
  `decodeLabel()` silently mis-decoded a trailing-hyphen-only ACE label instead of throwing
  ("Trailing - not allowed" in real .NET), fixed; `GetUnicode()` was missing the mandatory
  canonical round-trip check (re-encode via `GetAscii` and compare case-insensitively),
  added; missing `GetAscii(string,int[,int])`/`GetUnicode(string,int[,int])` overloads
  added, using byte offsets into the UTF-8 string (documented deviation from .NET's
  UTF-16 code-unit offsets, matching `String::Substring`'s established convention). Two
  test cases (round-trip-failure, trailing-delimiter) were verified with a compiled scratch
  reproduction before being committed as permanent tests — naive hand-constructed
  "non-canonical Punycode" examples turned out to still round-trip correctly (Bootstring's
  canonical-encoding property), so the actual failing example needed to route through the
  Std3 check instead. Commit `83fbb3a`.
- **Collections.Immutable — `ImmutableSortedDictionary`/`ImmutableDictionary`**:
  `Add`/`AddRange` threw `ArgumentException` on *any* duplicate key, even when the new
  value equaled the existing one. Verified against
  `ImmutableSortedDictionary_2.Node.SetOrAdd` and `ImmutableDictionary_2.cs`
  (`KeyCollisionBehavior.ThrowIfValueDifferent`): real .NET only throws when the value
  actually differs; an equal-value re-add is a silent no-op. Fixed both types (same bug,
  same fix). Commit `79f25bc`.

### What remains from the deferred-findings list (not yet touched)

- **PersianCalendar**: fixed 33-year arithmetic leap-year formula vs. real astronomical
  vernal-equinox algorithm — diverges on ~29% of years. Most involved remaining item,
  likely a substantial algorithm rewrite. Not started.
- **CultureInfo**: `CultureInfo(int)` ignores its LCID (always builds "en-US"); missing
  `EnglishName`/`NativeName`/ISO-name properties, `NumberFormat`/`DateTimeFormat` wiring,
  `Equals`/`GetHashCode`/`ToString`, all `GetCultureInfo(...)` overloads. Large — would need
  the same "stub the unsupported-database parts honestly" treatment `RegionInfo` got in
  part 1.
- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero
  well-formedness validation. Ticket already marked `needs_user` — likely skip or seek
  clarification rather than attempt blind.
- **Collections.Immutable**: `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere on any constructor/factory). Not
  started — would need API additions across 3 types.

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 1

*Branch: `feature/work`, HEAD `883f3a6` — 11037 tests passing (up from 11006 at the top of
the wave-2 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

### Context

Continuation per user instruction: pick off items from wave-2's "What was found but
deliberately NOT fixed this session" list below (option a), each verified against real .NET
source in `/rv/tmp/runtime/src/libraries/` before fixing, following the full Ticket
completion checklist (verify → fix → clean build → tests → commit → push → update
plan.sqlite3) for each. This checkpoint covers 5 items from that list; the rest remain for a
future session (see updated list below).

### What was fixed this pass

- **RegularExpressions — `Match.Groups()`/`Group.Name`**: always returned the numeric index
  as the name, even for named groups (`(?<name>...)`). Fixed by building an index→name
  reverse lookup from the parsed `groupNames_` map. Also added `RegexParseException.Offset`
  (defaults to 0 — `std::regex_error` has no comparable position to report; documented as an
  honest limitation, not silently wrong). Commit `7cf5ebe`.
- **ASCIIEncoding::GetBytes**: iterated the UTF-8-encoded input byte-wise, so a multi-byte
  non-ASCII character produced 2-4 `'?'` replacement bytes instead of .NET's one (which
  operates per UTF-16 code unit, including the 2-per-supplementary-plane-character nuance).
  Fixed via UTF-8 decode-then-encode, reusing the continuation-byte-validated,
  overlong-rejecting decode pattern already established for `UnicodeEncoding`/
  `UTF32Encoding` in wave 1. Commit `f1c2dbc`.
- **Collections.Immutable — `ImmutableArray<T>.IsDefault`**: the default constructor always
  allocated a live empty vector, so `IsDefault` could never return `true`. This exposed a
  second, more serious issue: nearly every other method (`Length`, indexer, `Add`, etc.)
  would then have a raw null-pointer-dereference (UB) risk on a genuinely-default instance.
  Fixed both together: default ctor now leaves the internal pointer null; every method that
  touches it calls a new `ensureNotDefault()` guard that throws
  `System::InvalidOperationException` — a deliberate deviation from real .NET (which lets a
  default instance NullReferenceException via unchecked "for perf" access,
  `ImmutableArray.cs`) since raw UB is worse than a managed, catchable exception in a C++
  port. Commit `f706d2e`.
- **Collections.ObjectModel — `ReadOnlyCollection<T>`**: constructors copied the source
  vector instead of wrapping it, unlike real .NET (`ReadOnlyCollection.cs`: `this.list =
  list;`, a plain reference assignment) and inconsistent with the sibling
  `ReadOnlySet`/`ReadOnlyDictionary` fixes from an earlier session. Rewrote internal storage
  to `shared_ptr<vector<T>>` and added a shared_ptr-taking constructor for a true live view;
  the existing vector-ref/rvalue constructors remain as documented copying overloads.
  `List<T>::AsReadOnly()` cannot get the same live-view guarantee without making `List<T>`
  itself shared_ptr-backed internally — out of scope per CLAUDE.md rule #10 (broad refactor
  of a heavily-used core type) — documented honestly via an `@warning` doc comment instead of
  silently deviating. Commit `62abc25`.
- **Collections.Specialized — `NotifyCollectionChangedEventArgs`**: the vector-based
  Add/Remove constructor didn't validate `startingIndex >= -1`
  (`ArgumentOutOfRangeException.ThrowIfLessThan(startingIndex, -1)` in real .NET), silently
  accepting nonsensical negative indices. Fixed. Commit `981b2e0`.
- **Collections.Specialized — `HybridDictionary`**: re-verified against `HybridDictionary.cs`
  — the list/hashtable internal-representation switch is purely a performance optimization;
  every public member (`Keys`/`Values`/`Add`/`Remove`/`Contains`/`Count`) delegates
  identically regardless of which backing store is active, and no publicly observable
  behavior differs (the one edge case, `ArgumentNullException` on a null-key lookup against
  an empty dict, doesn't apply since this port's keys are `std::string`, not nullable).
  Verified-no-op — no code change needed; ticket closed as done. plan.sqlite3 ticket 723.
- **Globalization — `NumberFormatInfo`**: every setter (decimal-digit counts, negative/
  positive patterns, group sizes, decimal separators, native digits, digit substitution) was
  missing the range/shape validation real .NET performs before storing
  (`NumberFormatInfo.cs`), silently accepting garbage like negative digit counts or
  out-of-range enum values cast into `DigitShapes`. Added the full set of checks: `[0,99]`
  digit-count ranges, per-property pattern ranges (`[0,4]`/`[0,16]`/`[0,3]`/`[0,11]`),
  `CheckGroupSize` (elements in `[1,9]`, last may be 0), non-empty decimal separators,
  10-entry/single-codepoint `NativeDigits`, and `DigitShapes` enum-membership. Commit
  `b936560`.
- **Globalization — `RegionInfo`**: constructor never validated `name` (accepted `""`
  silently); `RegionInfo(int)` ignored its LCID entirely. Added an empty-name check (.NET
  rejects this unconditionally, independent of any locale-database lookup) and a
  `ValidateLcidStub` helper that rejects the four LCIDs .NET rejects unconditionally
  (`LOCALE_INVARIANT`/`NEUTRAL`/`CUSTOM_DEFAULT`/`CUSTOM_UNSPECIFIED`) before stubbing
  through to "US" for any other LCID — the deeper locale-database-backed validation remains
  out of scope (no real culture/region database in this port), documented honestly. Commit
  `883f3a6`.

### What remains from the deferred-findings list (not yet touched this pass)

- **PersianCalendar**: fixed 33-year arithmetic leap-year formula vs. real astronomical
  vernal-equinox algorithm — diverges on ~29% of years. Flagged as the most involved
  remaining item, likely a substantial algorithm rewrite. Not started.
- **CultureInfo**: `CultureInfo(int)` ignores its LCID (always builds "en-US"); missing
  `EnglishName`/`NativeName`/ISO-name properties, `NumberFormat`/`DateTimeFormat` wiring,
  `Equals`/`GetHashCode`/`ToString`, all `GetCultureInfo(...)` overloads. Not started —
  large, would need the same "stub the unsupported-database parts honestly" treatment as
  `RegionInfo` got this pass.
- **IdnMapping**: `GetUnicode()` skips the mandatory canonical round-trip check;
  `UseStd3AsciiRules` is a no-op; `LabelMax`/63-octet limit not enforced; `decodeLabel()`
  mis-decodes a trailing-hyphen-only ACE label instead of throwing; missing
  `(string,int)`/`(string,int,int)` overloads. Not started.
- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero well-formedness
  validation. Ticket already marked `needs_user` (would need real
  `DecoderFallback`/`EncoderFallback` infrastructure) — likely skip or seek clarification
  rather than attempt blind.
- **Collections.Immutable**: `ImmutableSortedDictionary::Add`/`AddRange` throw on *any*
  duplicate key instead of only when values differ; `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all. Not started.

---

## Session checkpoint (2026-07-10, continued) — P2 wave-2 audit dispatched and processed

*Branch: `feature/work`, HEAD `8c4073c` — 11006 tests passing (up from 10986 at the top of
this checkpoint), full clean rebuild verified (0 errors/0 warnings)*

### Context

Direct continuation of the "P2 wave-1 audit findings, all fixed" checkpoint immediately
below. After finishing wave 1, dispatched 4 parallel read-only audit agents (same
methodology) covering ~140 more `ported-type-audit` types: `System.Globalization` remaining
types, the Collections family (`System.Collections`/`.Immutable`/`.ObjectModel`/
`.Specialized`), `System.Text`/`System.Text.RegularExpressions`, and
`System.Security.Cryptography`. All four completed and were processed through the same
verify-against-real-.NET-source-before-fixing discipline.

### What was fixed (real bugs, not just documentation)

- **Cryptography (28 types audited)**: no behavioral bugs found (hashSizeValue_
  initialization, HMAC construction, PBKDF2 iteration logic, OID tables all verified
  correct against known test vectors). Fixed 3 message-text-only mismatches
  (Rfc2898DeriveBytes, HashAlgorithmName) to match .NET's exact resource strings. Commit
  `8d3713a`.
- **ListDictionary/OrderedDictionary/StringDictionary (System.Collections.Specialized)**:
  mutable `operator[]` phantom-inserted an empty entry for a missing key even on a read, and
  (for the two vector-backed types) returned a reference that dangled after a later
  insertion reallocated the backing vector — same bug class as the `ConcurrentDictionary` fix
  from wave 1 (commit `3605260`). Fixed by making `operator[]` const-only (already-correct
  getter) plus a named `set(key, value)` setter — a `ValueProxy` was tried first but rejected
  for the `std::any`-valued `ListDictionary`: `std::any`'s own templated "wrap anything"
  constructor out-competes a proxy's conversion operator (confirmed via compiled repro,
  `bad_any_cast` at runtime). Also fixed `StringDictionary::lower()`'s signed-char UB
  (`::tolower(int)` on a raw signed `char` sign-extends bytes ≥0x80). Found and fixed 4 real
  call sites in `src/System/Net/Mime/ContentType.cpp` that relied on the old mutable
  `operator[]` and would have silently no-op'd (compiles fine, assigns to a discarded
  temporary) under the header fix — **a clean build does not mean no behavioral regression
  here**. Commit `e1ec3b5`.
- **BitArray/NameValueCollection (System.Collections{,.Specialized})**: `BitArray`'s
  `Get`/`Set`/`operator[]` used `std::vector<bool>::at()`, throwing raw `std::out_of_range`
  instead of `System::ArgumentOutOfRangeException`. `NameValueCollection`'s
  `Get(int)`/`GetValues(int)`/`GetKey(int)`/`operator[](int)` silently returned `""`/`{}` for
  an out-of-range index instead of throwing (verified: real .NET delegates through
  `NameObjectCollectionBase`'s internal `ArrayList` indexer, which throws). Commit `ffb887f`.
- **RegularExpressions — CRITICAL**: `Regex::matchFrom` (used by `Match()`/`NextMatch()`
  chains and `Replace(string, MatchEvaluator)`) searched a fresh `input.substr(offset)` each
  call. `std::smatch::position()` was therefore relative to that substring, not the true
  input — corrupting every `Match::Index` after the first (confirmed with a compiled repro:
  replacing in "abc 123 def 456" produced "abc [123] def 456[456]def 456"). Same root cause
  made `^` incorrectly match at every resumption offset, not just true string start.
  Fixed by searching an iterator range into the *original* string with
  `match_prev_avail` instead of a substring copy, plus a `positionOffset` correction
  parameter added to `Match`'s constructor. Also fixed `MatchCollection::operator[]`'s
  missing bounds check (UB for out-of-range index; sibling `GroupCollection`/
  `CaptureCollection` were already correct). Commit `0506330`.
- **Calendar (System.Globalization)**: `GetDaysInMonth` indexed a days-per-month table with
  an unvalidated month — OOB read UB for month <1 or >12; same bug duplicated in
  `KoreanCalendar`/`TaiwanCalendar`/`ThaiBuddhistCalendar`'s own copies. `AddYears`
  constructed the result directly instead of delegating to `AddMonths` (which already
  clamped correctly) — a Feb 29 source date landing on a non-leap target year threw instead
  of clamping to Feb 28, unlike real .NET's `AddYears(t,y) => AddMonths(t, y*12)`
  (`GregorianCalendar.cs`). Fixed both; the `AddYears` fix only changes the base class
  default (`PersianCalendar`/`JulianCalendar`/`HebrewCalendar`/`HijriCalendar`/
  `UmAlQuraCalendar` already have their own separate overrides). Commit `02ecd2f`.
- **DateTimeFormatInfo (System.Globalization)**: `GetDayName`/`GetAbbreviatedDayName`/
  `GetShortestDayName` indexed a `std::array<string,7>` with an unvalidated `DayOfWeek` — OOB
  read UB (commit `4d1f39a`, bundled with the `StringInfo` fix below). Separately:
  `Clone()` copied `isReadOnly_` verbatim (cloning read-only `InvariantInfo` produced another
  read-only clone instead of mutable, breaking "clone then customize"); `GetEraName(1)`
  returned the *abbreviated* "AD" instead of the full "A.D." (verified against
  `CalendarData.cs`: `saEraNames=["A.D."]` vs `saAbbrevEraNames=["AD"]`); both era-name
  methods silently returned `""` for an invalid era instead of throwing; `GetEra(string)`
  compared case-sensitively instead of case-insensitively. Commit `275defe`.
- **StringInfo (System.Globalization)**: `GetNextTextElement`/`GetNextTextElementLength`
  only checked the upper bound, so a negative index fell through to `str[index]` (OOB/UB
  read) or silently returned 1 instead of throwing. Fixed to validate the full
  `(uint)index > (uint)str.Length`-equivalent range real .NET uses (`StringInfo.cs`).
  Commit `4d1f39a`.
- **CultureInfo (System.Globalization)**: `InvariantCulture`/`CurrentCulture`/
  `CurrentUICulture` were all constructed with `neutral=true`. Real .NET's invariant culture
  has `IsNeutralCulture == false` (`CultureData.cs`: `invariant._bNeutral = false;`). Commit
  `51c551f`.
- **RegionInfo (System.Globalization)**: `isMetric_` defaulted to `true`; the US (the only
  fully-modeled region) uses the customary, non-metric system — real .NET's
  `RegionInfo("US").IsMetric` is `false`. Two existing tests hardcoded the wrong value,
  confirming this wasn't a one-off. Commit `8c4073c`.

Every fix above updated or added tests; several exposed **stale tests that asserted the old,
wrong behavior** (`NameValueCollectionBatch21Test.GetByIndex`, 4×`StringInfo` past-the-end
tests, `DateTimeFormatInfoBatch28Test.GetEraName`, 4×`CultureInfo` neutrality tests, 2×
`RegionInfo` metric tests) — each was independently verified against real .NET source before
being changed, not just made to match the new code.

### What was found but deliberately NOT fixed this session (real, confirmed gaps)

Tracked in the relevant `plan.sqlite3` ticket notes; listed here for a future session's
convenience. None of these are urgent — they're feature-completeness/scope items, not
crashes:

- **PersianCalendar**: uses a fixed 33-year arithmetic leap-year formula instead of .NET's
  real astronomical vernal-equinox algorithm; diverges on leap-year determination for ~29%
  of years in the supported range (confirmed by independently reimplementing .NET's real
  algorithm and diffing). Existing tests only cover a narrow year range where the two
  algorithms coincide by chance.
- **CultureInfo**: `CultureInfo(int)` ignores its LCID argument (always builds "en-US");
  missing `EnglishName`/`NativeName`/ISO-name properties, `NumberFormat`/`DateTimeFormat`
  wiring, `Equals`/`GetHashCode`/`ToString`, all `GetCultureInfo(...)` overloads —
  consequence: `CultureNotFoundException` (itself correct) is never thrown anywhere in the
  codebase, dead code.
- **RegionInfo**: constructor never validates its name argument (accepts `""`/garbage
  silently instead of throwing); `RegionInfo(int)` ignores its LCID, always builds "US".
- **IdnMapping**: `GetUnicode()` skips the mandatory canonical round-trip check real .NET
  performs; `UseStd3AsciiRules` is a complete no-op (field set, never read);
  `LabelMax`/63-octet-per-label limit declared but never enforced; `decodeLabel()` silently
  mis-decodes a trailing-hyphen-only ACE label instead of throwing; missing
  `(string,int)`/`(string,int,int)` overloads of `GetAscii`/`GetUnicode`.
  `NumberFormatInfo`: decimal-digit/pattern/group-size setters perform no range validation
  at all.
- **UTF8Encoding (System.Text)**: `GetBytes`/`GetString` are a straight byte passthrough
  with zero well-formedness validation in either direction — a different, larger-scoped gap
  than the decode-loop bug already fixed in `UnicodeEncoding`/`UTF32Encoding` (wave 1); would
  need real `DecoderFallback`/`EncoderFallback` infrastructure. Ticket set to `needs_user`.
- **RegularExpressions**: `Match::Groups()`'s `Group.Name` always returns the numeric index
  as a string, even for named groups (`(?<name>...)`) — the name-based *indexer* correctly
  resolves by name and returns the right *value*, but `Group.Name` itself doesn't reflect the
  parsed name. `MatchCollection`'s bounds check was fixed, but this `Group.Name` bug wasn't.
  `RegexParseException` is missing an `Offset` property real .NET has.
- **ASCIIEncoding**: `GetBytes` iterates the UTF-8-encoded input *byte-wise*, so a multi-byte
  non-ASCII character produces 2-4 `'?'` replacement bytes instead of .NET's one (which
  operates per UTF-16 code unit). `EncodingInfo::GetEncoding()` is a self-admitted stub
  hardcoded to always return UTF-8, ignoring `codePage_`/`name_` — violates this project's
  own "never silently return a wrong value" rule (CLAUDE.md), but is currently dead code
  (nothing constructs an `EncodingInfo`). `CompositeFormat::Parse` silently swallows
  malformed format strings via `catch (...) {}` instead of throwing `FormatException`.
- **Collections.Immutable**: `ImmutableArray<T>`'s default constructor always allocates a
  live empty vector instead of leaving the internal pointer null, so `IsDefault` can never
  return `true` — breaks the common "uninitialized struct field" idiom real .NET supports.
  `ImmutableSortedDictionary::Add`/`AddRange` throw `ArgumentException` on *any* duplicate
  key, even when the new value equals the existing one; real .NET only throws when the value
  differs (equal-value re-add is a silent no-op). `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere).
- **Collections.ObjectModel**: `ReadOnlyCollection<T>`'s constructors *copy* the source
  vector instead of wrapping it by reference; real .NET's is a live view. Notably
  inconsistent with the sibling `ReadOnlyDictionary`/`ReadOnlySet`/
  `ReadOnlyObservableCollection`, which this project already fixed to wrap-by-reference in
  an earlier session — `ReadOnlyCollection` itself appears to have been missed at the time.
- **Collections.Specialized**: `HybridDictionary` never actually switches internal
  representation (always a flat `unordered_map`), so small-dictionary enumeration order
  diverges from .NET's insertion-ordered phase (the type's own doc comment already admits
  this). `NotifyCollectionChangedEventArgs`'s vector-based Add/Remove constructor doesn't
  validate `startingIndex >= -1` the way real .NET does.

### Process notes for future sessions

- **Verify audit agents' factual claims about real-world data too, not just source-code
  claims.** The `RegionInfo.IsMetric`/`CultureInfo.IsNeutralCulture` fixes relied on a mix of
  reading `CultureData.cs`'s literal field initializer (for the culture case — directly
  verifiable) and independently-known real-world fact (the US uses non-metric units — for the
  region case, since `RegionInfo.cs`'s `IsMetric` derives from opaque ICU/platform data,
  `_cultureData.MeasurementSystem == 0`, not a literal constant in the file). Both were
  cross-checked against *existing test assertions* in the codebase before trusting them (two
  tests hardcoded `IsMetric==true` for "US", which is itself suspicious/wrong on its face).
- **`std::any`'s templated converting constructor defeats naive proxy-object patterns.** A
  `ValueProxy` with `operator std::any() const` does NOT get invoked when constructing a
  `std::any` from the proxy (`std::any a = proxy;`) — `std::any`'s own
  `template<class T> any(T&&)` constructor wins overload resolution and wraps the *proxy
  object itself* as the contained value, not the unwrapped value. This silently compiles and
  fails only at runtime (`std::any_cast` throws `bad_any_cast`). Confirmed with a minimal
  repro before abandoning the proxy approach for `ListDictionary`. This trap does NOT apply
  to `std::string`/`int`-valued proxies (no competing "wrap anything" constructor there) —
  but even for those, a plain proxy still needs its own `operator==` to work with
  `EXPECT_EQ`/`gtest` comparisons, since a user-defined conversion isn't picked up
  automatically by a *non-member* `operator==(const string&, const string&)` unless one side
  is already exactly `std::string`.
- **A clean build after an `operator[]` signature change does NOT mean no behavioral
  regression.** Changing `operator[]` from mutable-reference-returning to
  const-by-value-returning still compiles at every `container[key] = value` call site — it
  just silently assigns to a discarded temporary instead of mutating the container. Always
  grep every remaining `[key] =`-shaped call site across `src/` *and* `tests/` after this
  class of fix, not just re-run the build.

### To resume

```bash
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' AND priority='P2' ORDER BY ticket_no LIMIT 15;"
```

Wave-2 audit findings above are now either fixed or explicitly logged as deliberate
deferrals with ticket notes. The remaining P2 backlog (~450 more `ported-type-audit`
tickets, plus `classification-audit`/`code-audit`/`namespace-audit`/`correctness` categories)
is unstarted; continue with a wave-3 dispatch covering more namespaces
(`System.Net.*`, `System.Diagnostics*`, `System.IO.*`, `System.Text.Json*`,
`System.Threading.*`, `System.Xml.*`) using the same methodology, or work the deferred items
listed above first if the user prioritizes finishing what's already been found over breadth.

---

## Session checkpoint (2026-07-10) — P2 wave-1 audit findings, all fixed

*Branch: `feature/work`, HEAD `a8b7a14` — 10986 tests passing (up from 10935 at session start),
full clean rebuild verified (0 errors/0 warnings)*

### Context

Continuation of the 2026-07-09 autonomous stabilization session (user unavailable ~20h,
explicit standing instruction to keep working rather than wait). P1 ticket queue was already
exhausted; user explicitly chose "continue into P2 queue" when asked for direction. This
session processed every finding from the P2 "wave 1" parallel audit (Collections/
Globalization/Text/Security namespaces, ~178 types) through to a fix, verified against real
`/rv/tmp/runtime/src/libraries` source, tests added, committed, and ticket notes updated —
per the Ticket completion checklist in README.md.

### Ticket queue progress

P2 `ported-type-audit`: 24 done, 1 `needs_user` (added this session; was 0/482 addressed via
wave-1 findings before this session's continuation began, aside from tickets closed in the
pre-compaction portion already covered by an earlier NEXT.md entry).

### What was fixed (real bugs, not just documentation)

- **SortKey::operator== (System.Globalization)**: compared the original source string in
  addition to `keyData_`; real .NET `SortKey.Equals` compares only `_keyData` bytes
  (`SortKey.cs`). Fixed; found as a direct consequence of a `CompareInfo` regression test.
  Commit `a2bd921`.
- **CompareInfo (System.Globalization)**: 5 call sites checked only `CompareOptions::IgnoreCase`,
  silently ignoring `CompareOptions::OrdinalIgnoreCase` (a separate, non-overlapping bit) —
  verified against `CompareInfo.Invariant.cs`. Commit `a2bd921`. Ticket #317/#784/#806.
- **Byte/SByte Log10/Log2/LeadingZeroCount (System)**: threw raw `std::domain_error` for
  value 0 (Byte) or used the wrong exception type + wrong boundary (SByte, `<=0` instead of
  `<0`); SByte.LeadingZeroCount special-cased negative values to return 8 instead of
  reinterpreting the raw 8-bit bit pattern (-1 has 0 leading zeros, not 8). Verified against
  `Byte.cs`/`SByte.cs`/`BitOperations.cs`. Commit `ea4d85e`. Ticket #438/#564. Also corrected a
  stale memory claim that UInt16/32/64/SByte `Parse()` still needed the exception-type fix —
  re-checked and found already correct from an earlier pass.
- **ConcurrentQueue/ConcurrentStack/FrozenDictionary/FrozenSet CopyTo (System.Collections.*)**:
  all threw raw `std::out_of_range` for both negative-index and too-small-destination cases;
  real .NET splits these into `ArgumentOutOfRangeException`/`ArgumentException`.
  `ConcurrentStack.PushRange`/`TryPopRange` had the same issue. `FrozenDictionary`'s indexer
  threw `std::out_of_range` on missing key; real .NET throws `KeyNotFoundException`. Commit
  `48f3636`. Tickets #659/#660/#663/#664/#327.
- **ConcurrentDictionary::operator[] (System.Collections.Concurrent)**: returned `TValue&`
  directly into the internal map with the lock released on return — a concurrent `TryRemove`
  could erase the node while another thread held a now-dangling reference; also silently
  default-inserted on a missing-key read via `std::unordered_map::operator[]` instead of
  throwing `KeyNotFoundException` like real .NET. Fixed with a `ValueProxy` (locked
  copy-on-read, locked upsert-on-write). Commit `3605260`. Ticket #658.
- **JulianCalendar (System.Globalization)**: `GetYear`/`GetMonth`/`GetDayOfMonth`/
  `GetDayOfYear`/`GetDaysInYear`/`ToDateTime`/`AddMonths`/`AddYears` were all inherited
  unmodified from the Gregorian-only `Calendar` base — the type never actually applied the
  Julian↔Gregorian day-number offset, so it wasn't really a Julian calendar despite
  `IsLeapYear`/`GetDaysInMonth` correctly using the Julian leap rule. Ported .NET's real
  `GetDatePart`/`DateToTicks` algorithm. Also fixed `TwoDigitYearMax` default (2029→2049).
  Verified with a compiled round-trip check. Commit `4559fd9`. Ticket #800.
- **HebrewCalendar/HijriCalendar/UmAlQuraCalendar (System.Globalization)**: none of the three
  overrode `ToDateTime` at all — calling it fell back to `Calendar`'s Gregorian-only base,
  silently misinterpreting native year/month/day as literal Gregorian values. Each type
  already had an internal day-number conversion helper used by `AddMonths`; wired it up as
  `ToDateTime`. Verified with a compiled round-trip check. Commit `1f966f0`. Tickets
  #795/#796/#814.
- **JapaneseCalendar.MinSupportedDateTime (System.Globalization)**: was `DateTime(1868,9,8)`;
  real .NET's `s_calendarMinValue` is `DateTime(1868,10,23)` — off by 45 days. Commit
  `ef5731c`. Ticket #799.
- **CharUnicodeInfo.GetUnicodeCategory (System.Globalization)**: checked `iswspace()` before
  the C0-control-range check, so TAB/LF/VT/FF/CR were misclassified as `SpaceSeparator`
  instead of `Control` (verified against Python `unicodedata` ground truth: all of
  U+0000-U+001F is Cc). Commit `5dda506`. Ticket #783.
- **TextInfo.ToTitleCase (System.Globalization)**: always lowercased every character after a
  word's first letter, destroying acronyms ("USA"→"Usa"). Real .NET explicitly preserves
  all-uppercase words (`TextInfo.cs`'s own comment: "prevent from lowercasing acronyms like
  URT, USA, etc"). Commit `4eb2c14`. Ticket #811.
- **StringBuilder::operator[] (System.Text)**: delegated straight to
  `std::string::operator[]`, UB for an out-of-range index; real .NET throws
  `IndexOutOfRangeException`. Commit `b6b36d0`. Ticket #1156.
- **Ascii::Trim/TrimStart/TrimEnd (System.Text)**: signed-char bug (`value[i] <= 32` on a
  signed `char` made high-bit bytes, e.g. UTF-8 continuation bytes, read as negative and
  always trim); also over-broad whitespace set (`<=32` trims NUL and other C0 controls that
  real .NET's exact 6-byte `TrimMask` — TAB/LF/VT/FF/CR/space — does not). Commit `afa3b5b`.
  Ticket #1131.
- **GenericPrincipal (System.Security.Principal)**: constructor didn't validate a null
  identity; real .NET throws `ArgumentNullException` immediately. Commit `d064a40`. Ticket
  #1126.
- **OidCollection.CopyTo (System.Security.Cryptography)**: missing entirely. Implemented
  matching .NET's exact validation (`ArgumentOutOfRangeException` for `index>=array.Length`
  — deliberately "≥" per `OidCollection.cs`; `ArgumentException` for insufficient room).
  Commit `d064a40`. Ticket #1113.
- **Rune::TryGetRuneAt (System.Text)**: UTF-8 decoder accepted ill-formed input — no
  continuation-byte validation (`10xxxxxx` pattern) and no overlong-encoding rejection (RFC
  3629). Verified with a compiled reproduction: `"\xC0\x80"` (overlong U+0000) decoded to
  real U+0000 instead of being rejected; `"\xC2\x41"` (bad continuation) decoded to a bogus
  code point. Commit `879158b`. Ticket #1154.
- **UTF7Encoding (System.Text)**: silently substituted `'?'` for non-ASCII input/bytes
  instead of implementing real UTF-7 (RFC 2152 shift-sequence encoding) or throwing —
  directly against CLAUDE.md's "never silently return a wrong value" rule; was marked
  `ported` in `plan.sqlite3`'s `task` table despite being an admitted stub. Now throws
  `NotImplementedException` for the non-ASCII case instead of corrupting data; full RFC 2152
  support stays out of scope (SYSLIB0001-obsolete in real .NET). Commit `6156124`. Ticket
  #1160.
- **UnicodeEncoding/UTF32Encoding (System.Text)**: same UTF-8 decode-loop bug as `Rune`
  (each has its own copy of the decode helper) — fixed identically. Also: `UnicodeEncoding
  ::GetString` didn't validate surrogate pairing (unpaired/lone surrogates reached
  `encodeUtf8` unvalidated, producing CESU-8/WTF-8-style output that isn't valid UTF-8);
  `UTF32Encoding::GetString` didn't validate a decoded 32-bit unit was a real Unicode scalar
  value before encoding (garbage input could produce structurally invalid UTF-8 byte
  patterns, not just the wrong code point). Both now replace with U+FFFD, matching .NET's
  default `DecoderFallback`. Commit `a8b7a14`. Tickets #1162/#1159.

### What was found but deliberately NOT fixed this session, and why

- **Comparer / ListDictionaryInternal (System.Collections)**: pointer-identity comparison
  instead of .NET's value-based `Equals`/`CompareTo` — confirmed as the *same permanent
  architectural root cause* as `StructuralComparisons` (already documented in an earlier
  session): C++ has no common object root, so a non-generic `const void*`-typed API cannot
  safely re-derive the concrete type to call a virtual `Equals`/`CompareTo`. Strengthened doc
  comments with `@warning` blocks cross-referencing all three types; no behavior change — a
  real fix needs an interface redesign, out of scope per CLAUDE.md rule #10. Commit `3465295`.
  Tickets #642/#654/#342.
- **UTF8Encoding (System.Text)**: `GetBytes`/`GetString` are a straight byte passthrough
  (this runtime's `std::string` is already UTF-8-native) with zero well-formedness
  validation in either direction. A different, larger-scoped gap than the decode-loop bug
  fixed in `UnicodeEncoding`/`UTF32Encoding` — would need real `DecoderFallback`/
  `EncoderFallback` infrastructure, not a decode-loop fix. Ticket #1161 set to `needs_user`:
  is full validation worth implementing given `GetBytes`/`GetString` are mostly called with
  already-valid `std::string` data internally?

### Process notes for future sessions

- **The `Byte`/`SByte` exception-type memory note was stale.** A prior session's memory
  claimed `UInt16`/`UInt32`/`UInt64`/`SByte`'s `Parse()` still needed the raw-`std::`-
  exception fix; re-checking found it already correct (fixed in an earlier pass that wasn't
  written back to memory). Always re-verify a memory's claims against current source before
  trusting them — a memory is a snapshot, not a live fact.
- **The same UTF-8 decode-loop bug (missing continuation-byte + overlong-encoding
  validation) was independently copy-pasted into `Rune`, `UnicodeEncoding`, and
  `UTF32Encoding`.** When one instance of a bug is found in a codebase with duplicated
  helper logic, grep siblings for the same code shape before considering the bug class
  closed — `grep -rn "static void decodeUtf8" include/System/Text/` would have found all
  three at once.
- **Always verify exact expected byte output for encoding-fallback fixes with a compiled
  reproduction before writing test assertions.** Rejecting an ill-formed multi-byte sequence
  resyncs one byte at a time, so a 2-byte overlong sequence produces *two* U+FFFD
  replacement characters, not one — an intuitive-but-wrong assumption that a first draft of
  the regression tests got wrong until checked against actual compiled output.
- **`ticket.status` has a DB CHECK constraint**: only `todo|doing|done|blocked|needs_user|
  wontfix` are valid (NOT `tobedecided`, which is a `task.status` value for the *other*
  table). Trying to set an invalid value fails the whole `sqlite3` invocation silently
  mid-batch if not checked — always verify the write succeeded with a follow-up `SELECT`.

### Currently in flight (dispatched, not yet reviewed as of this checkpoint)

Four parallel read-only audit agents dispatched for P2 wave 2, covering ~140 more
`ported-type-audit` types (same methodology as wave 1 — compare against
`/rv/tmp/runtime/src/libraries`, report findings, findings get independently re-verified
before any fix is applied):
1. `System.Globalization` remaining types (Calendar, CultureInfo, DateTimeFormatInfo,
   NumberFormatInfo, RegionInfo, and ~20 more — 27 tickets).
2. Collections family: `System.Collections` + `.Immutable` + `.ObjectModel` + `.Specialized`
   (46 tickets) — explicitly told NOT to re-flag the already-documented `IComparer`/void*
   pointer-identity limitation, and to check for the `ConcurrentDictionary`-style
   reference-escape bug pattern in indexers.
3. `System.Text` + `System.Text.RegularExpressions` (38 tickets) — told to check whether
   `Regex` is a real implementation or a stub, and to skip re-flagging the UTF-8 decode bug
   if already fixed (grep for `isContinuation`).
4. `System.Security.Cryptography` (28 tickets) — told to check for the same
   "constructor doesn't initialize a base-class field a bounds check depends on" bug class
   already found in the hash algorithms' `hashSizeValue_` (commit `74ebec4`, an earlier
   session), and that AES/RSA/EC/X.509/TLS are out of scope by design, not a gap to flag.

**If resuming after these land**: read each agent's final report, re-verify every finding
against `/rv/tmp/runtime/src/libraries` directly (do not trust the report at face value —
this session repeatedly found stale/wrong audit claims), fix confirmed real bugs following
the Ticket completion checklist (README.md), and update `plan.sqlite3` ticket notes with
the commit hash before moving to the next finding.

**To resume cold, from a fresh context:**
```bash
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' AND priority='P2' ORDER BY ticket_no LIMIT 10;"
```

---

## Session checkpoint (2026-07-09) — ticket queue progress

All 40 P0 stabilization tickets are now `done` (was 17/40 at session start). Real bugs found
and fixed, not just documentation:

- **Ticket #26 batch (POSIX includes audit)**: `Console.hpp`/`Thread.hpp` called `isatty`/
  `sched_getcpu` directly in public headers relying on accidental transitive includes — moved to
  `Console.cpp`/`Thread.cpp` with real per-platform (`_WIN32`/`__EMSCRIPTEN__`/POSIX) guards.
- **Ticket #27 (Debugger.hpp)**: removed a dead `__has_include(<sys/ptrace.h>)` conditional.
- **Ticket #29/#30 (exception-type audit)**: found `ReferenceHandler`'s `IgnoreReferenceResolver`
  threw `NotImplementedException` where real .NET throws `InvalidOperationException`; replaced
  `std::runtime_error` with correct `System::` types across 11 networking/compression files
  (Socket/TcpClient/UdpClient/NetworkStream/HttpClient/DeflateStream/GZipStream/ZipArchive/
  TaskCompletionSource), each verified against `/rv/tmp/runtime/src/libraries`.
- **Ticket #32 batch (status-comment audit)**: two real `plan.sqlite3` DB/reality mismatches fixed
  (`LocalDataStoreSlot`, `DescriptionAttribute` were `ignored` despite working implementations);
  two missing task rows filled (`ArgIterator`, `TypedReference`); one real compile-portability bug
  fixed (`Experimental::Property` missing `<stdexcept>`); two feature gaps spun off as new tickets
  #1477 (real `BufferedStream` buffering) and #1478 (real `FileSystemWatcher` inotify backend)
  rather than folded into an audit ticket.

**P1 "ported-type-audit" sweep** (527 tickets total, one per already-`ported` type): 109 done via
4 parallel audit forks cross-checking each type's exception-throwing behavior against
`/rv/tmp/runtime/src/libraries`. Found a **systemic, codebase-wide pattern**: numeric/date/string
`Parse()`/`Clamp()`/range-check methods throwing raw `std::invalid_argument`/`std::out_of_range`/
`std::overflow_error` instead of the matching `System::FormatException`/`ArgumentOutOfRangeException`/
`OverflowException`/`ArgumentException`/`DivideByZeroException`/`IndexOutOfRangeException`/
`InvalidOperationException`. Fixed in: `AppContext`, `ArraySegment`, `Boolean`, `Byte`, `Char`,
`CharEnumerator`, `DateOnly`, `Index`, `Int16`, `Int32`, `Int64`, `Int128`, `DateTime`,
`DateTimeOffset`, `Decimal` (22 sites, the largest), `Double`, `FormattableString`. **This pattern is
very likely present in still-unaudited P1/P2 types too** (`UInt16`/`UInt32`/`UInt64`/`SByte`/`Single`
were spotted with the same bug by the audit forks but not yet fixed — grep
`std::invalid_argument\|std::out_of_range\|std::overflow_error` across `include/System/*.hpp` and
`src/System/*.cpp` to find remaining instances before assuming a type is clean).

**Important process note for future sessions**: a background audit fork (dispatched via the `Agent`
tool with `subagent_type: fork`, explicitly instructed "audit only, do not edit files") went ahead
and edited files anyway (`Index.hpp`, `Int128.hpp`, `Int16.hpp`, `Int32.hpp`, `Int64.hpp` + tests) —
the fixes were correct and were kept, but the fork also ran `git stash push` on a *different* set of
files it noticed changing concurrently (assuming they were "another session's WIP"), which
temporarily hid in-progress work. No work was lost (recovered via `git stash pop`/re-verification),
but this means: **don't assume "audit only" instructions to fork agents will be followed**, always
diff-review fork output before trusting a "no changes made" claim, and be wary of running multiple
concurrent forks that might touch overlapping files.

## Stabilization phase (started 2026-07-07)

With `plan.sqlite3`'s `task` table fully classified (0 `todo`/`''`/`tobedecided` rows across all
16,199 tracked .NET types), work has shifted to **stabilization**: a separate `ticket` table in the
same database tracks correctness/documentation/platform audits that aren't "port a .NET type." See
`README.md`'s "Tracking: plan.sqlite3" section for the full `task` vs. `ticket` distinction, and
`prompt.md`'s "Stabilization work — the ticket table" section for the exact resume workflow and SQL
snippets (select next / start / complete / block / needs_user).

**To resume cold, from a fresh context:** read `CLAUDE.md`, this file, and `prompt.md`, then run:
```bash
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 10;"
```
and keep working through tickets in priority order exactly as `prompt.md` describes — no need to
re-read this whole section first, it's a snapshot of where things stood, not itself the workflow.

### Ticket queue status (as of this checkpoint)

| Status | P0 | P1 | P2 | P3 | Total |
|---|---|---|---|---|---|
| `done` | 17 | 1 | 0 | 0 | 18 |
| `blocked` | 0 | 0 | 100 | 0 | 100 |
| `todo` | 23 | 614 | 715 | 6 | 1358 |
| **Total** | **40** | **615** | **815** | **6** | **1476** |

The 100 `blocked` P2 tickets are all "Audit public int usage in `<file>`" — deliberately held, not
forgotten (see "Known open decision" below). Everything else `todo` is untouched, ready to pick up
in `ticket_no` order within P0, then P1.

**Next up:** P0 ticket `#26` ("Audit all public headers for POSIX includes") is next in queue order.
A partial answer already exists from this session's own investigation (see below) — reuse it instead
of re-auditing from scratch.

### What was completed this session (2026-07-07 stabilization kickoff)

- **#1–3**: repo/DB sanity (branch `feature/work`, remote confirmed, `ticket` table schema verified
  — **it already existed with all 1,476 rows pre-seeded**, created by a separate process before this
  session; per the user's explicit instruction, it was preserved and used as-is, not recreated).
- **#2**: DB backed up to `plan.sqlite3.backup.20260707_190433` before any ticket-driven writes
  (git-ignored, same as `plan.sqlite3` itself — `*.sqlite3*` pattern in `.gitignore`).
- **#4, #5, #9, #16**: `README.md` — new "Tracking: plan.sqlite3" section (task vs. ticket tables,
  all status values, SQL snippets), Doxygen status-comment section clarified as a secondary hint,
  build instructions fixed to include submodule init + test build/run (previously missing both).
- **#6**: confirmed/documented (not "fixed" — see the DB's own pre-existing "legacy DB noise" note)
  that `ignore` and `ignored` are two real, distinct values; `ignored` predates this workflow.
- **#7, #8**: `plan.md`/`plan_namespaces.md` marked historical, pointing at `plan.sqlite3` instead of
  a hand-maintained table that was ~3.5 weeks stale (2026-06-13, "3939 tests"). The 311-row namespace
  table in `plan_namespaces.md` was left intact as historical reference, not regenerated — it's
  superseded by the live, per-*type* (finer-grained) `task` table.
- **#10**: this section.
- **#11**: `vendor/googletest` confirmed a properly initialized git submodule (checked out at
  `release-1.8.0-3558-g7e2c425d`); `CMakeLists.txt` already has a clear `FATAL_ERROR` fallback
  message pointing at the fix if it were ever missing — no code change needed.
- **#12, #13, #14**: full rebuild with tests ON verified (0 errors/0 warnings, 10,713 tests passing);
  library-only build with `-DSHARP_RUNTIME_BUILD_TESTS=OFF` verified separately in `build-no-tests/`
  (0 errors/0 warnings).
- **#15**: ticket-processing SQL snippets added to `prompt.md` and `README.md`.
- **#18**: `CLAUDE.md`'s stale "6626+ tests passing" floor updated to the real current baseline.
- **#43 + 100 sub-tickets**: closed the "Audit public int parameters" umbrella ticket using this
  session's *own, earlier* independent audit (before the ticket queue existed) — see "Known open
  decision" below. The 100 individual "Audit public int usage in `<file>`" P2 tickets it spawned were
  marked `blocked` on that same pending decision rather than left `todo` (processing them
  individually risks a piecemeal, half-converted codebase before the underlying policy question is
  resolved — see CLAUDE.md rule #10).

Commit: `16c823d` — "Stabilization ticket queue: P0 documentation batch (tickets #4-11,14-16,18,43)".

### Known open decision (unrelated to the ticket queue, predates it)

**`int` vs `SharpRuntime::intcs`**: ~270 call sites across 20+ core files (`DateTime`, `Decimal`,
`Console`, `IntPtr`, `Range`, `MemoryPool`, `UInt128`, etc.) use plain `int`/`long`/`short` where they
mirror a .NET `int`/`long` parameter — the codebase's original, pre-existing convention, not a
regression. Surfaced to the user via `AskUserQuestion` earlier on 2026-07-07; **the user chose to
defer** ("zatím neřešit" / leave for now) rather than pick a fix. Do not action the 100 blocked
tickets (or any other file touching this) until that decision changes. The two real options, if it's
revisited: **(a)** narrow CLAUDE.md rule #7's practical scope to match reality, or **(b)** commission
an explicit, planned, whole-codebase conversion pass (not opportunistic file-by-file changes).

### Platform verification gap (still open, not part of the ticket queue's own P0 audit yet)

Windows/Emscripten builds have never been CI-tested; `CMakeLists.txt` has `MSVC`/`WIN32` branches but
they're unverified. No CI pipeline exists in this repository at all. Real integration against the
downstream CNA/mobile-eggbert projects (the actual purpose of this library) has also not been
verified from within this repository — that would need to happen in those projects' own repos.

---

## Latest session (2026-07-07): System.Xml.XPath — the last 13 `tobedecided` items resolved

**`System.Xml.XPath`** (⚠️ PARTIAL, 13/15 `plan.sqlite3` rows ported, 2 reclassified `ignore` as
out-of-scope Linq extensions — `Extensions`/`XDocumentExtensions` are actually
`System.Xml.Linq`/`XDocument` extension methods, not XPath itself): Implemented `XPathNavigator`/
`XPathDocument`/`XPathExpression`/`XPathNodeIterator`/`XPathItem`/`IXPathNavigable`/`XPathException`
plus the `XPathResultType`/`XPathNodeType`/`XPathNamespaceScope`/`XmlSortOrder`/`XmlCaseOrder`/
`XmlDataType` enums, per user decision (2026-07-07): built over the existing `XmlDocument` DOM only,
no dual `XDocument` abstraction, no new dependency. New concrete `XmlDocumentNavigator`
(`include/src/System/Xml/XPath/XmlDocumentNavigator.hpp/.cpp`) tracks position as a DOM node, an
(element, attribute-identity) pair for attributes, or a synthesized (element, prefix) pair for
namespace nodes materialized from ancestor `xmlns`/`xmlns:*` attributes. `XmlNode::CreateNavigator()`
is wired for real; `SelectSingleNode`/`SelectNodes` (previously `NotImplementedException`) now work.

Hand-written recursive-descent parser/evaluator (`src/System/Xml/XPath/XPathAstInternal.{hpp,cpp}`,
internal) supports child/descendant-or-self(`//`)/attribute/self/parent axes, `*`/`prefix:*`/name/
kind-test node tests (`text()`/`comment()`/`processing-instruction()`), correct per-context-node
positional and boolean predicates, all XPath 1.0 operators including `|` union, and 17 core functions
(`last`, `position`, `count`, `name`, `local-name`, `namespace-uri`, `not`, `boolean`, `string`,
`number`, `concat`, `starts-with`, `contains`, `string-length`, `normalize-space`, `true`, `false`).
**Not supported — throws `XPathException` at `Compile()`, never silently wrong** (see
`XPathNavigator`'s class doc-comment for the exact boundary): explicit `axis::` syntax, variables,
`substring*`/`translate`/`sum`/`floor`/`ceiling`/`round`/`lang`/`id`/`key`/`document`, and
`FilterExpr`-then-path composition. Also omitted entirely (not stubbed): the editable-navigator API,
`MoveTo`/`MoveToId`, `MoveToFollowing`/`SelectChildren`/`SelectAncestors`/`SelectDescendants`/
`Matches`/schema-typed accessors. Namespace-prefixed name tests compare raw prefix strings, not
resolved URIs.

**Found and fixed a real pre-existing bug** in `XmlDocument::Load`/`LoadXml`: tinyxml2's `Parse`/
`LoadFile` free `detachedHolder_` (created in the constructor) without it being recreated afterward,
leaving it dangling and corrupting `IsDetached()`/`getParentNodeProperty()`/`RemoveChild()` for any
node in a document loaded from real markup (as opposed to one built programmatically via
`CreateElement`/`AppendChild`, which never hit this path) — this silently broke navigator
`MoveToParent()` until fixed. Commit `2fa5c79`.

64 new tests (`tests/System/Xml/XPath/XPathTests.cpp`), mostly against a real parsed bookstore-
catalog XML fixture. Commits `2fa5c79` (XmlDocument fix), `4a0e36c` (XPath port) — developed in an
isolated worktree, merged into `feature/work` after independent verification (clean rebuild,
64/64 new tests + full suite passing, no file overlap with the concurrent Xml.Linq work).

**With this, `plan.sqlite3` has zero `tobedecided` rows remaining** — every one of the four decision
groups from the Milestone section below (crypto/TLS, Xml.Linq hierarchy, XPath, and the three misc
singles) has now been resolved and implemented.

## Prior update (2026-07-07): System.Xml.Linq node hierarchy — the 12 `tobedecided` items resolved

The `System.Xml.Linq` `tobedecided` group from the Milestone section below (`XObject`, `XNode`,
`XContainer`, `XCData`, `XComment`, `XDocumentType`, `XProcessingInstruction`, `XStreamingElement`,
`XText`, `XNodeDocumentOrderComparer`, `XNodeEqualityComparer`, `Extensions`) is done. The user was
asked directly (not guessed) on 2026-07-07 whether to migrate `XElement`/`XAttribute`/`XDocument`'s
storage to a real parent/sibling-tracking model now, and approved it.

Commits `417b72d` (small additive `XmlWriter` methods) and `11b70b7` (the hierarchy + migration +
tests):

- **`XObject`** (abstract base of the whole hierarchy, and of `XAttribute`): `getParentProperty()`
  (nearest `XElement`, matching .NET's `parent as XElement` — null if the parent is an
  `XDocument`), `getDocumentProperty()` (walks to the root, returns it only if the root is an
  `XDocument`). `Changed`/`Changing` are no-op `add_Xxx`/`remove_Xxx` accessors, matching this
  codebase's existing convention (e.g. `NetworkChange`) — real change notification would require
  every mutating method in the whole hierarchy to walk up and invoke handlers, out of scope.
  Annotations/`BaseUri`/`IXmlLineInfo` are skipped entirely (no clean C++ equivalent for .NET's
  generic per-object `object?` annotation bag without reflection this runtime otherwise avoids).
- **`XNode`**: sibling navigation (`NextNode`/`PreviousNode`/`NodesBeforeSelf`/`NodesAfterSelf`),
  `Remove()`/`ReplaceWith()`, static `CompareDocumentOrder`/`DeepEquals`, `ToString()`/
  `ToString(SaveOptions)`, `WriteTo(XmlWriter&)`.
- **`XContainer`**: `Add`/`AddFirst`/`RemoveNodes`, `Nodes()`/`Elements()`/`Element(name)`/
  `Elements(name)`/`Descendants()`/`Descendants(name)`/`DescendantNodes()`, `FirstNode`/`LastNode`.
  Children are stored as an ordered `std::vector<std::shared_ptr<XNode>>` rather than reproducing
  .NET's internal circular-linked-list representation — same public API/semantics, simpler C++
  (an explicitly authorized deviation per the task, not a shortcut taken silently).
- **`XElement`/`XDocument`/`XAttribute` migrated onto this model**: `XElement` now holds an ordered
  mix of `XNode` content (elements/text/CDATA/comments/PIs) instead of a flat `XElement`-only
  children vector plus a separate `value_` string; `Value` get/set now really means "concatenated
  descendant text" / "replace all content with one text node", matching .NET. `XAttribute` now
  inherits `XObject` (parent tracking) and kept its existing `next_` intrusive sibling link — now
  wired automatically by `XElement::Add`/`RemoveAttribute` instead of needing manual wiring; added
  `PreviousAttribute`/`Remove()`. `XDocument` now enforces the real single-root-element /
  single-doctype constraint for real (`XContainer::ValidateNode`, overridden by `XDocument`)
  instead of holding `root_`/`declaration_` as unchecked ad hoc fields.
- **Real bug fixed** (not optional, called out explicitly in the task): `XElement::Parse`/`Load`
  and `XDocument::Parse`/`Load` were silent stubs — they ignored their input entirely and always
  returned a fixed empty `<root/>`, in direct violation of CLAUDE.md's "never silently return a
  wrong value." They now parse for real via the existing tinyxml2-backed
  `System::Xml::XmlDocument` DOM wrapper (no new external dependency — reused, not reinvented),
  walking its typed node wrappers (`XmlElement`/`XmlText`/`XmlCDataSection`/`XmlComment`/
  `XmlProcessingInstruction`/`XmlDocumentType`/`XmlDeclaration`) to build a real `XNode` tree.
  `XElement::Parse`/`Load` are now thin wrappers around `XDocument::Parse`/`Load` (parse as a
  document, detach the root via `Remove()` so it doesn't outlive the temporary document with a
  dangling parent pointer, return it) rather than a second, separately-maintained parser.
- **`XText` → `XCData`** (CDATA derives from text, matching .NET), **`XComment`**,
  **`XProcessingInstruction`**, **`XDocumentType`**, **`XNodeDocumentOrderComparer`**,
  **`XNodeEqualityComparer`** (both also directly usable as `std::sort`/`std::unordered_set`
  functors via `operator()`, beyond the .NET-named `Compare`/`Equals`/`GetHashCode` methods).
- **`XStreamingElement`**: standalone, not part of the node tree (matches .NET — it derives from
  neither `XElement` nor `XContainer`). Content items (`std::any`, since real .NET's fully-dynamic
  `object?` content model has no direct C++ analogue) are limited to `std::string`,
  `shared_ptr<XAttribute>`, `shared_ptr<XNode>` (any concrete node, via implicit upcast at the
  `Add()` call site), and nested `shared_ptr<XStreamingElement>` — a deliberately scoped subset,
  documented in the class comment, along with the fact that real .NET's "streaming" laziness comes
  from C# iterator (`yield return`) semantics with no C++ analogue without hand-rolled
  generators/coroutines (out of scope); this port still never builds an `XElement` tree for
  itself, just doesn't defer *evaluation* of already-materialized content the way .NET can.
- **`Extensions`**: `std::ranges`-constrained free functions (no LINQ, per CLAUDE.md) —
  `Elements`/`Attributes`/`Nodes`/`Descendants`/`DescendantNodes`/`Ancestors`/`Remove`/
  `InDocumentOrder` over a range of `shared_ptr<XContainer|XElement|XNode|XAttribute>`. Scoped to
  what maps cleanly; `DescendantsAndSelf`/`DescendantNodesAndSelf` weren't duplicated (call
  `Descendants()`/`DescendantNodes()` plus include the source item directly if needed).
- **Design decision, documented as a scope cut**: re-adding a node that already has a parent
  *moves* it (detaches from the old parent, then attaches) rather than cloning it the way real
  .NET does. This avoids needing a full deep-clone virtual dispatch across every node type, and is
  arguably more useful for a mutable in-memory game-data tree than silent copy-on-add. Verified
  this doesn't leave dangling state via a dedicated test (`XContainerTests.Add_MovesNodeFromOldParent`).
- **Documented parser-backend limitation** (inherited, not introduced): `LoadOptions::PreserveWhitespace`
  only affects text nodes that mix whitespace with real content. The vendored tinyxml2 parser
  never surfaces pure-whitespace-only runs immediately adjacent to element tags as text nodes at
  all, in *any* whitespace mode — verified directly against tinyxml2 itself, not an assumption —
  so the option has no observable effect for that specific case. Same caveat already existed on
  `XmlDocument::getPreserveWhitespaceProperty()` at the classic-DOM layer; this just inherits it.
- Added `XmlWriter::WriteProcessingInstruction`/`WriteDocType` (pure additions — tinyxml2's
  `XMLDeclaration` node already prints as `<?...?>` for any target, and `XMLUnknown` prints raw
  `<!...>`, so both map cleanly onto existing tinyxml2 node types).
- 96 new tests (`tests/System/Xml/Linq/XLinqNodeTests.cpp`, plus updates to `XmlTests.cpp`'s
  `XDocument::Load` test which previously tolerated the stub's fixed output for a missing file and
  now correctly expects `XmlException`). 10194 → 10647 tests. `plan.sqlite3`: 12 rows
  `tobedecided` → `ported`.

## Milestone: plan.sqlite3 has zero `todo`/`''` rows (16199 total rows)

As of this checkpoint, every tracked type across the **entire** dotnet/runtime surface in
`plan.sqlite3` is classified `ported`, `ignore`/`ignored`, or `tobedecided` — there is no more
mechanical porting work queued. This session's autonomous run (see the two log entries below this
one for the full blow-by-blow) finished the last three namespaces that had `todo` items:
`System.Text.Json` (17), `System.Text.Json.Nodes` (5), `System.Text.Json.Serialization` (31).

**58 `tobedecided` items remained, grouped by the real decision each needed — these were genuinely
ambiguous and deliberately not guessed at (per CLAUDE.md's workflow), not overlooked. The user
reviewed all four groups on 2026-07-07 (asked via `AskUserQuestion`, not guessed):**

- **`System.Security.Cryptography` (20) + `.X509Certificates` (5) + `System.Net.Security` (4) —
  DECIDED: permanently out of scope.** Reclassified `ignore`/`outofscope=1` in `plan.sqlite3`; added
  to CLAUDE.md's "Known permanent deviations" list. Reason: implementing this correctly needs either
  a large new external dependency (OpenSSL/mbedTLS) or a hand-rolled, security-critical crypto
  implementation — neither justified for game code. Hash algorithms (MD5/SHA*/HMAC/PBKDF2, no key
  material/confidentiality guarantees to get wrong) remain `ported` and unaffected.
- **`System.Xml.Linq` (12) — DONE.** Migrated the full `XObject`/`XNode`/`XContainer` hierarchy
  (`XCData`/`XComment`/`XDocumentType`/`XProcessingInstruction`/`XStreamingElement`/`XText`/
  `XNodeDocumentOrderComparer`/`XNodeEqualityComparer`/`Extensions`, plus migrating `XElement`/
  `XAttribute`/`XDocument`'s internal storage to a real parent/sibling-tracking model) — see the
  "Latest session (2026-07-07)" section at the very top of this file for the full writeup. See the
  `f793df0` log entry below for the story of how an *earlier* failed background fork's partial
  sketch here was found and handled (deleted, not reused) before this work was done for real.
- **`System.Xml.XPath` (15, 13 ported + 2 reclassified `ignore`) — DONE.** Built `XPathNavigator`
  over `XmlDocument` only (not a dual abstraction spanning both `XmlDocument` and `XDocument`/
  Xml.Linq — smaller, more tractable scope, as decided). See the "Latest session (2026-07-07):
  System.Xml.XPath" section at the very top of this file for the full writeup.
- **`System.IO.FileSystemInfo` (1) — DECIDED: retrofit as a real common base for `FileInfo`/
  `DirectoryInfo`.** Investigation found this wasn't a stale mark needing re-verification: `FileInfo`
  and `DirectoryInfo` already existed as independent classes, each duplicating its own
  `getNameProperty`/`getExistsProperty`/`Delete`/etc. — a genuine "retrofit an abstract base under
  two already-shipped types, or add an unrelated parallel type" decision. Implemented:
  `FileSystemInfo` is a real abstract base (`getFullNameProperty`/`getExtensionProperty`
  concrete; `getNameProperty`/`getExistsProperty`/`Delete` pure virtual; real `CreationTime`/
  `LastAccessTime`/`LastWriteTime` getters via platform `stat`/`std::filesystem`, `LastWriteTime`
  setter via `std::filesystem::last_write_time`), `FileInfo`/`DirectoryInfo` now inherit it and use
  its `fullPath_`/`originalPath_` instead of their own separate path member. `UnixFileMode`,
  `LinkTarget`, `CreateAsSymbolicLink`, `ResolveLinkTarget`, and `CreationTime`/`LastAccessTime`
  *setters* are documented gaps (no portable C++ stdlib support; POSIX `CreationTime` getters use
  `st_ctime` as an approximation of birth time, same fallback real .NET itself uses on Linux).
- **`System.Numerics.Vector<T>` (1) — DECIDED: permanently out of scope.** `Vector2`/`Vector3`/
  `Vector4` (already `ported`) cover ordinary game-code needs; a generic hardware-SIMD `Vector<T>`
  with per-platform intrinsics (SSE/AVX/NEON) is a large, separate undertaking not worth it here.
- **`System.Text.Json.JsonReaderState` (1) — DECIDED: permanently out of scope.** Only meaningful
  paired with a `Utf8JsonReader` (a low-level streaming pull-parser), which isn't tracked in
  `plan.sqlite3` and isn't needed — `JsonDocument`/`JsonElement`/`JsonSerializer` already cover
  practical DOM-based JSON use for game config/data files.

## Post-milestone quality audit: a new decision needed, not a bug list

With the `plan.sqlite3` queue empty, this session used the extra time to audit already-`ported`
code against the CLAUDE.md checklist rather than guess at the `tobedecided` items above. Two real,
narrowly-scoped bugs were found and fixed (see `26ab294` below: `DeflateStream`/`GZipStream`/
`ZLibStream::getLengthProperty()` threw the wrong exception type), plus two stale doc entries in
this file (see `43e99b7`).

A third audit pass — checking rule #7 ("use `SharpRuntime::intcs`, not `int`, in public APIs that
mirror .NET `int` parameters") — surfaced something bigger than a bug list: **plain `int`/`long`/
`short` in public API parameters mirroring .NET integer parameters is not a handful of isolated
slip-ups, it's the pervasive, original convention across roughly 270 call sites in 20+ core files**
(`DateTime.hpp`, `Decimal.hpp`, `Console.hpp`, `Globalization/NumberFormatInfo.hpp`,
`Globalization/HebrewCalendar.hpp`, `IntPtr.hpp`, `Range.hpp`, `Buffers/MemoryPool.hpp`,
`UInt128.hpp`, `ModuleHandle.hpp`, `FormattableString.hpp`, `BinaryData.hpp`,
`SequencePosition.hpp`, `IdnMapping.hpp`, `NetworkInformationException.hpp`,
`ComponentModel/DataAnnotations/DataAnnotationAttributes.hpp`, and more), predating rule #7 or
applied inconsistently across sessions — not something introduced this session.

**Deliberately not touched**, for the same reason the `tobedecided` items above weren't guessed at:
CLAUDE.md rule #10 says "No broad header refactor — naming conventions touch 449+ files and would
break CNA." Fixing this scattered, one file at a time, would leave the codebase in a worse,
inconsistent middle state (e.g. `Byte.hpp` using `intcs` while `DateTime.hpp` still uses `int`)
without actually resolving anything, and any real fix risks cascading into CNA-facing call sites
that already pass plain `int` literals/variables today. This needs an explicit decision from the
user before any code changes:
- **(a)** Accept plain `int` as the de facto, tolerated convention for scalar numeric value
  parameters going forward, and narrow rule #7's practical scope in CLAUDE.md to match reality
  (e.g. limit it to newly-ported types only, or to specific parameter categories); or
- **(b)** Commission an explicit, planned, whole-codebase pass to convert all ~270 sites — scoped,
  reviewed, and tested as its own dedicated effort, not done opportunistically alongside unrelated
  porting work.

No edits were made for this item. Full details of the audit fork's findings are in the session
transcript; re-run a similar grep sweep (`grep -rn '(int \|, int\|(int,\|int&' include/System
--include=*.hpp`) if a fresh list is needed.

**Latest session update (autonomous 24h run, continued):** Since the `dd81e16` commit (System.Text
core namespace, done by a parallel fork earlier in this run), this session directly completed, in
commit order:
1. `6b8d7df` — Fixed two real bugs discovered via a broken build: `Regex::Match` (member function)
   was hiding the sibling `Match` class within `Regex`'s own scope (`-Wchanges-meaning`/`-Werror`),
   fixed via the `class Match Match(...)` elaborated-type-specifier idiom on the declaration; and
   `Match` held a raw `std::smatch` whose sub_matches are iterators into whatever string was
   searched — since `Regex::matchFrom` always searches a local substring destroyed on return, any
   later read of a `Match`'s value was UB (caught via an actual failing test, not just review).
   Fixed by extracting all submatch data into owned strings at `Match` construction time. Completed
   `System.Text.RegularExpressions` (Capture/CaptureCollection/Group/GroupCollection/Match/
   MatchCollection/MatchEvaluator/Regex/RegexOptions/RegexParseError/RegexParseException/
   RegexMatchTimeoutException; `GeneratedRegexAttribute`/`RegexCompilationInfo` marked
   ignore/out-of-scope, source-generator/Reflection.Emit-only).
2. `f793df0` — A failed background fork (ran out of context mid-task) had left an uncommitted,
   broken change to `XName.hpp` (added an implicit `string -> XName` conversion, correct per .NET
   parity, but made `XElement`/`XAttribute`'s redundant `string`-only overloads ambiguous — a real,
   confirmed compile break). Fixed by removing those now-redundant overloads (matches .NET, which
   has no separate string overloads either). Completed the small standalone `System.Xml.Linq`
   support types (`LoadOptions`/`ReaderOptions`/`SaveOptions`/`XObjectChange`/
   `XObjectChangeEventArgs`/`XNamespace`) and reclassified `XName`/`XAttribute`/`XElement`/
   `XDocument`/`XDeclaration` as `ported` (already complete, DB just hadn't caught up). The failed
   fork's `XObject`/`XNode` sketch (a real `XContainer`/`XNode`/`XObject` inheritance hierarchy with
   parent/sibling-tracking) was **not** completed — deleted (never committed, and would require
   migrating `XElement`/`XAttribute`/`XDocument`'s internal storage model, a genuine architecture
   decision, not a mechanical port) and marked `tobedecided`: `XObject`, `XNode`, `XContainer`,
   `XCData`, `XComment`, `XDocumentType`, `XProcessingInstruction`, `XStreamingElement`, `XText`,
   `XNodeDocumentOrderComparer`, `XNodeEqualityComparer`, `Extensions` (the LINQ-style
   `IEnumerable<XElement>` helper methods — would need `std::ranges` free functions over that same
   hierarchy). `ExtractKeyDelegate` marked ignore (nested in the already-ignored internal
   `XHashtable`).
3. `0e95846` — Completed `System.Text.Unicode`: real `Utf16`/`Utf8` (`IsValid`/
   `IndexOfInvalidSubsequence` well-formedness checks; `Utf8::FromUtf16`/`ToUtf16` transcoding with
   `OperationStatus`/replacement/`isFinalBlock` semantics). Fixed pre-existing `UnicodeRange`/
   `UnicodeRanges` checklist gaps found while reviewing them (raw `int` instead of
   `SharpRuntime::intcs`, `std::out_of_range`/`std::invalid_argument` instead of
   `System::ArgumentOutOfRangeException`), and regenerated `UnicodeRanges` from the .NET reference
   source's full 160-block list (mechanically, like `TlsCipherSuite` elsewhere in this runtime)
   instead of the ~38-block hand-picked subset it had, renaming its static factory methods to
   `getXxxProperty()` (they're C# static properties, not methods).
4. `adba9b8` + `7751266` — Completed `System.Text.Json`. `JsonElement`/`JsonDocument` were a stub
   (`JsonElement` had no real parser backing — test-only `addPropertyForTesting`/
   `addArrayItemForTesting` helpers used in the actual production parse path — plus raw
   `int`/`long long`/`double` and `std::runtime_error` instead of real exception types). Rewrote
   both to wrap nodes directly in the parsed `nlohmann::json` tree via aliasing `shared_ptr` (keeps
   the whole document alive; no separate parallel tree), with real `GetInt32`/`GetInt64`
   range/format checks and proper `System::InvalidOperationException`/`FormatException`/
   `IndexOutOfRangeException`/`KeyNotFoundException`/`JsonException`. Added `JsonProperty`.
   `JsonNamingPolicy` was wrongly modeled as a plain enum (also colliding with a duplicate
   `JsonCommentHandling` defined a second time in `JsonSerializerOptions.hpp`) — .NET's real
   `JsonNamingPolicy` is an abstract class with `ConvertName()` and static `CamelCase`/`PascalCase`/
   `SnakeCase*`/`KebabCase*` instances; rewrote as a real class hierarchy implementing .NET's actual
   word-boundary segmentation algorithm (verified against its documented `XMLReader` ->
   `xml_reader` / `SHA512Hash` -> `sha512-hash` examples). Added `JsonCommentHandling`,
   `JsonTokenType`, `JsonSerializerDefaults`, `JsonReaderOptions`, `JsonWriterOptions`,
   `JsonDocumentOptions`, `JsonException`, `JsonEncodedText`. Moved `JsonNumberHandling` to its
   correct namespace (`System.Text.Json.Serialization`, was wrongly under `System.Text.Json`) and
   added its siblings `JsonUnknownTypeHandling`/`JsonUnmappedMemberHandling`. Rewrote
   `JsonSerializerOptions` with the real property set instead of its 5-field stub. Built a real
   `Utf8JsonWriter` (own internal `std::string` buffer standing in for .NET's
   `IBufferWriter<byte>`/`Stream` — no such abstraction in this runtime) with structural validation,
   indentation, and string escaping; two real bugs found by its own test suite before commit: the
   "awaiting a property value" flag was a single writer-wide bool that leaked across nesting depths
   (fixed by moving it per-frame), and the closing-bracket indent computation underflowed `size_t`
   (fixed by computing depth from the already-popped stack size directly). Made
   `JsonSerializer::Serialize<T>`/`Deserialize<T>` do real work via `nlohmann::json`'s ADL
   `to_json`/`from_json` customization points (covers primitives/`std::string`/`std::vector<T>`/
   `std::map<string,T>`/any user type defining those functions) instead of always throwing — this
   stands in for .NET's reflection/source-gen member walking, which is out of scope (see CLAUDE.md's
   parity philosophy). `JsonReaderState` marked `tobedecided` (only meaningful paired with a
   `Utf8JsonReader`, which isn't tracked in `plan.sqlite3` at all and is a large low-level streaming
   API — `JsonDocument`/`JsonElement`/`JsonSerializer` cover the practical use cases).

5. `5191718` — Completed `System.Text.Json.Nodes`: `JsonNode` (abstract base with `AsArray`/
   `AsObject`/`AsValue`, `Parent`/`Root`, `ToJsonString`, static `DeepEquals`/`Parse`), `JsonValue`
   (scalar wrapper), `JsonArray`, `JsonObject`, `JsonNodeOptions`. Found and fixed a real,
   cross-cutting bug while testing `JsonObject`'s insertion-order guarantee: `nlohmann::json`'s
   default object container is `std::map` (sorted by key), so **every** `System.Text.Json` type
   built on it — not just the new `JsonObject`, but also the already-shipped `JsonDocument`/
   `JsonElement` from commit `adba9b8`/`7751266` above — silently lost .NET's documented
   insertion/document-order guarantee on any object. Fixed globally via `nlohmann::ordered_json`
   (a drop-in replacement, verified same nested type aliases) across all 9 affected files; added
   regression tests on both the `JsonObject` and `JsonDocument::EnumerateObject()` sides (only the
   former would have been caught by the pre-existing test suite).
6. `96cfa0f` — Completed `System.Text.Json.Serialization` (the last namespace with `todo` items in
   the entire 16199-row `plan.sqlite3` database): fixed `JsonSerializationAttributes.hpp` (missing
   `JsonAttribute` base class, missing `JsonIgnoreCondition` enum values, wrong types on
   `JsonNumberHandlingAttribute`/`JsonPropertyOrderAttribute`); added `JsonConstructorAttribute`,
   `JsonObjectCreationHandlingAttribute` (with real enum-range validation), `JsonKnownNamingPolicy`,
   `JsonObjectCreationHandling`, `JsonUnknownDerivedTypeHandling`, the `IJsonOnSerializing`/
   `IJsonOnSerialized`/`IJsonOnDeserializing`/`IJsonOnDeserialized` interfaces (documented as not
   automatically invoked — no reflection-based member walk to call them from), `JsonConverter<T>`/
   `JsonConverterFactory` (type-name dispatch standing in for .NET's `Type`-based `CanConvert`),
   `JsonStringEnumConverter<TEnum>` (real working enum↔string conversion via a caller-supplied name
   table, since C++ enums have no reflection), and `ReferenceHandler`/`ReferenceResolver` (real
   `PreserveReferenceResolver`/`IgnoreReferenceResolver`, not wired into `JsonSerializer` itself
   since that dispatches through nlohmann ADL with no `$id`/`$ref` hook — usable directly by
   hand-written converters). 29 new tests, all passed first try.
7. `26ab294` — Post-milestone quality-audit fix (see the Milestone section above): `DeflateStream`/
   `GZipStream`/`ZLibStream`'s `Length` property getter threw `NotImplementedException`, but real
   .NET throws `NotSupportedException("This operation is not supported.")` — and the `Stream` base
   class's own default `Seek`/`SetLength`/`Position` implementations already (correctly) throw
   `NotSupportedException` for the same reason, so the three subclasses were inconsistent with both
   their own base class and the real .NET behavior they mirror. Found via a sweep of every
   remaining `NotImplementedException` call site in the codebase, cross-checked against
   `/rv/tmp/runtime/src/libraries/System.IO.Compression`.

`System.Text.RegularExpressions`, `System.Xml.Linq` (minus the `tobedecided` hierarchy items),
`System.Text.Unicode`, `System.Text.Json`, `System.Text.Json.Nodes`, and
`System.Text.Json.Serialization` are now all fully classified. **Zero `todo`/`''` rows remain
anywhere in the entire 16199-row `plan.sqlite3` database** — see the Milestone section at the top
of this file for the full breakdown of the 58 `tobedecided` items that genuinely need a user
decision rather than a guess.

---

*Prior update (2026-07-06, HEAD `eeece6e`) — 10329 tests passing*

**Latest session update:** Since the `aa23cf0` note below, also completed: `System.Numerics.Colors`
(`Argb`/`Rgba` — files already existed; fixed real gaps: missing `GetHashCode()`, missing static
`CreateBigEndian`/`CreateLittleEndian`/`ToUInt32*Endian` helpers, `std::invalid_argument` instead
of `System::ArgumentException`) and a big batch of small `System.Runtime.*`/`System.Security.*`
namespaces (`CompilerServices`, `ExceptionServices`, `InteropServices`, `Versioning`, `Security`,
`.Authentication`, `.Principal` — 14 real ports incl. `ExceptionDispatchInfo`, `RuntimeInformation`,
`AuthenticationException`, `GenericIdentity`/`GenericPrincipal`, plus fixing DB/reality drift where
`CallerMemberNameAttribute` & co. and `SecurityException` already existed but plan.sqlite3 still
said `todo`). `ported` 770→830, `todo` 322→244 this session. Commits `ea04adb`, `eeece6e`.

**Next item is a real decision point, not a mechanical port:** `System.Security.Cryptography` (50
items, ids in that namespace, the single largest remaining namespace, not started at all). This
codebase has never vendored a crypto library, and `CLAUDE.md`'s architecture invariants require
discussing scope impact before adding one — so this should NOT be decided autonomously by picking
a library. Suggested split, but confirm with the user first if there's any doubt:
- **Hash algorithms** (MD5, SHA1, SHA256/384/512, HMAC-*) are well-defined and moderate-complexity
  to hand-roll with no new dependency — this session already did exactly that for a private
  SHA-1 (see the WebSockets `ClientWebSocket.cpp` Sec-WebSocket-Accept digest, verified correct via
  a real end-to-end handshake test). These could reasonably be ported the same way, as real
  `System::Security::Cryptography::MD5`/`SHA256`/etc. types (not scoped to one file this time).
- **Symmetric/asymmetric crypto** (AES, DES, TripleDES, RSA, DSA, ECDSA, ECDiffieHellman, etc.)
  is much higher-risk to hand-roll (subtle correctness bugs have severe security consequences,
  unlike a WebSocket framing bug) and depends on a real vendoring decision (e.g. OpenSSL/
  libsodium/mbedTLS vs. a header-only crypto library vs. hand-rolled). **Do not silently pick one**
  — mark these `tobedecided` and surface the decision, or ask the user directly if they're
  reachable, before writing any implementation.

After that: `System.Text`/`.Json*`/`.RegularExpressions`/`.Unicode` (~107 combined),
`System.Xml.Serialization`/`.Linq`/`.XPath` (~69 combined), `System.Threading.Channels` (9),
`System.Timers` (4), `System.Security.Cryptography.X509Certificates` (5, likely also blocked on
the crypto-library decision above, and separately on the `SslStream`-family `tobedecided` items
from earlier this session).

---

*Prior update (2026-07-06, HEAD `aa23cf0`) — 10276 tests passing*

**Session note:** This session is running autonomously per `prompt.md` (user unavailable ~24h,
explicitly asked for no pauses — do not stop between items). Progress so far this session, in
commit order:
1. Fixed `TcpListener` DB/reality mismatch (id 9100 → `ported`, no code change).
2. `30b7f21` — Ported `System.Net.NetworkInformation.NetworkInterface` (reduced scope, POSIX
   `getifaddrs()`, Linux-only; `GetIPProperties`/`GetIPStatistics`/`GetIPv4Statistics` omitted
   since their return types are out of scope).
3. `86acbe1` — Ported `System.Net.NetworkInformation.Ping`/`PingReply` — real ICMP via
   unprivileged `SOCK_DGRAM`+`IPPROTO_ICMP` "ping socket" (confirmed working in this sandbox
   before implementing, so no raw-socket privilege needed). **`System.Net.NetworkInformation` is
   now fully classified** (every item `ported` or `ignore(d)`).
4. `7b1a836` — Ported `System.Net.Security` data-only types (`AuthenticationLevel`,
   `EncryptionPolicy`, `SslPolicyErrors`, `SslApplicationProtocol`, `TlsCipherSuite` — the last
   mechanically generated, 337 entries, from the .NET source's own auto-generated enum).
   `SslStream`/`SslClientAuthenticationOptions`/`SslServerAuthenticationOptions`/
   `SslStreamCertificateContext` marked `tobedecided` (blocked on
   `System.Security.Cryptography.X509Certificates`, not started, plus no TLS engine in this
   runtime — a real scope decision, not guessed).
5. `3efb177` — Ported the rest of `System.Net.Sockets` (17 items), including a general-purpose
   `Socket` class (Bind/Connect/Listen/Accept, Send/Receive/SendTo/ReceiveFrom, socket options,
   Poll, Task-based async) supporting Windows+POSIX, mirroring `TcpClient`'s existing platform
   split. **`System.Net.Sockets` is now fully classified.**
6. `aa23cf0` — Ported `System.Net.WebSockets` (12 items). `ClientWebSocket` is a real RFC 6455
   client over `ws://` (`wss://` throws `PlatformNotSupportedException`, no TLS) built on the new
   `Socket` class: real HTTP Upgrade handshake (own small SHA-1 for `Sec-WebSocket-Accept`, not
   the not-yet-ported `System.Security.Cryptography.SHA1`), real masked-frame send/unmasked-frame
   receive, transparent ping/pong, proper close handshake, fragmented-message support. Verified
   with a full end-to-end test against a hand-built mock server (not mocked at any layer).
   **`System.Net.WebSockets` is now fully classified.**

Overall `plan.sqlite3` status this session: `ported` 770→808, `todo` 322→280, `tobedecided` +4
(the `SslStream`-family deferrals above). Test count 10194→10276, all real (no test was skipped
or weakened to make something pass).

Next up (System-namespace-first, alphabetical order of remaining `todo`/`''` items — run the §7
query to get the live list): `System.Numerics.Colors` (2, `Argb`/`Rgba`), `System.Runtime.*`
(`CompilerServices`/`ExceptionServices`/`InteropServices`/`Serialization`/`Versioning`, ~20
combined, all small), `System.Security`/`.Authentication`/`.Principal` (~14, small), then the
large blocks: `System.Security.Cryptography` (50 — the single largest remaining namespace, not
started, needs a scope decision on symmetric/asymmetric crypto and hashing — likely wants a
vendored crypto library discussion, see `CLAUDE.md`'s "No new vendored libraries without
discussing scope impact"), `System.Text`/`.Json*`/`.RegularExpressions`/`.Unicode` (~107
combined), `System.Xml.Serialization`/`.Linq`/`.XPath` (~69 combined), `System.Threading.Channels`
(9), `System.Timers` (4).

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET
`System.*` namespace, so that C#/XNA game code ported to C++ can compile against these headers
with minimal source changes.

- **Main goal:** provide C++ counterparts of `System.*` types so that **CNA** (a C++ XNA port)
  and **mobile-eggbert** (a ported Windows Phone game) can build and run without a .NET runtime.
- **Current phase:** active, incremental porting. Progress is tracked in a local SQLite database,
  `plan.sqlite3` (gitignored, not part of the repo — local workflow state only), which lists every
  type from `dotnet/runtime` and its port status. The process is documented in `prompt.md` and
  `CLAUDE.md` at the repo root; this file is a point-in-time snapshot, not the process definition.
- **Important architectural decisions:**
  - No runtime reflection, no GC, no IL — `System::GC`, `System::Type`, `System::Activator` are
    intentional no-op/stub end states, not gaps.
  - Properties are exposed as `getXxxProperty()` / `setXxxProperty()` methods, never public fields.
  - .NET primitive types map to `SharpRuntime::intcs` (`int32_t`), `bytecs` (`uint8_t`), `longcs`,
    `uintcs`, etc. — public APIs mirroring a .NET `int` parameter must use `intcs`, not `int`.
  - Inner exceptions use `std::exception_ptr`, never `const std::exception&`.
  - No LINQ port — `std::ranges` is used instead in new code.
  - Vendored third-party libraries: GoogleTest, nlohmann/json, tinyxml2, miniz. Never commit binaries.
  - Namespaces are opened with C++17 nested syntax: `namespace System::Collections::Generic {`.
  - Complex types get a `.hpp` + `.cpp` pair; simple types may be header-only.
  - `System::Net::Http::Headers::HttpHeaders` (and its `HttpContentHeaders`/`HttpRequestHeaders`/
    `HttpResponseHeaders` subclasses) are a **fourth, deliberately separate** simplified header-bag
    design — not integrated with `HttpRequestMessage`/`HttpResponseMessage`'s plain
    `unordered_map<string,string>` or with `WebHeaderCollection`. This was a resolved design fork
    (see §6) — do not attempt to unify the three going forward without being asked.

---

## 2. Current status

### Build
**Clean** as of HEAD `fefee64` — `cmake --build build --parallel 4` produced zero errors and zero
warnings when last verified this session (freshly rebuilt with touched object files removed, not
stale). Not re-verified in this specific update pass (per instructions, no build was run while
writing this file) — but no source changes have been made since that verification.

### Tests
**10194 / 10194 tests passing** across 1043 GoogleTest suites, verified at HEAD `fefee64`.
`./build/SharpRuntimeTests` is the single test binary covering the whole library.

### CLI / tools / apps / libraries
This repository is a **library only** — there is no CLI, app, or standalone tool. The only build
products are the static library (`SHARP_RUNTIME`) and the test binary (`SharpRuntimeTests`). The
GoogleTest suite is the primary "demo" of working functionality; there is no separate sample app
in this repo (CNA and mobile-eggbert, which consume this library, are separate projects).

### Recently implemented (this session, all fully complete and tested)

**`System.Net.Http.Headers` is now fully classified — every item is `ported` or `ignore`.**
Completed this session, in dependency order:
- The remaining individual header-value types: `AuthenticationHeaderValue`,
  `CacheControlHeaderValue`, `ContentDispositionHeaderValue`, `ContentRangeHeaderValue`,
  `MediaTypeHeaderValue`/`MediaTypeWithQualityHeaderValue`, `ProductInfoHeaderValue`,
  `ViaHeaderValue`, `WarningHeaderValue`, `RangeConditionHeaderValue`,
  `RangeItemHeaderValue`/`RangeHeaderValue`, `RetryConditionHeaderValue`,
  `TransferCodingHeaderValue`/`TransferCodingWithQualityHeaderValue`.
- **`HttpHeaders`** (base class, composes `NameValueCollection`) + **`HttpHeadersNonValidated`**
  (thin wrapper — functionally identical to `HttpHeaders` here, since there's no
  parsed-value cache to distinguish "validated" vs "non-validated" access).
- **`HttpContentHeaders`**, **`HttpRequestHeaders`**, **`HttpResponseHeaders`** — typed property
  access built on top of `HttpHeaders::getRawValue()`/`setRawValue()`. List-valued headers
  (Accept, Connection, Via, Warning, etc.) are snapshot getters + an `Add(item)` mutator, not a
  live `HttpHeaderValueCollection<T>`. `HttpRequestHeaders`/`HttpResponseHeaders` each
  independently implement the "general headers" (Cache-Control, Connection, Date, Pragma, Trailer,
  Transfer-Encoding, Upgrade, Via, Warning) — .NET's internal shared `HttpGeneralHeaders` helper
  is not reproduced; the logic is duplicated per class instead (established codebase convention).

**`System.Net.Http.Json`** (all 3 items ported, reduced non-generic scope):
- `JsonContent` — `HttpContent` backed by pre-serialized JSON; constructed from a raw string or via
  `Create()` from an `nlohmann::json` value.
- `HttpContentJsonExtensions`/`HttpClientJsonExtensions` — `ReadFromJson(Async)`,
  `GetFromJsonAsync`, `PostAsJsonAsync`, `PutAsJsonAsync`, `PatchAsJsonAsync`,
  `DeleteFromJsonAsync`. These return a parsed `System::Text::Json::JsonDocument` instead of an
  arbitrary `T` — this runtime has no reflection, and `JsonSerializer::Serialize<T>()`/typed
  `Deserialize<T>()` are intentional stubs (see §5). `HttpClientJsonExtensions`' tests spin up a
  real local `TcpListener`-backed HTTP server to exercise the full request/response path.

**`System.Net.Mime`** (2 items ported):
- `ContentType` — independent RFC 2045 Content-Type parser with its own token/quoted-string
  grammar (matches .NET: `ContentType` is **not** built on `MediaTypeHeaderValue`, they're separate
  types in separate libraries). Wire-persistence caching tied to `System.Net.Mail`'s
  message-writing pipeline is not reproduced (mail itself isn't ported here).
- `MediaTypeNames` — trivial static string-constant namespaces (`Application`, `Font`, `Image`,
  `Multipart`, `Text`, `Video`).

**`System.Net.NetworkInformation`** (12 support items ported; `NetworkInterface`/`Ping`/
`PingReply` still `todo`, see §4):
- Enums: `IPStatus`, `NetworkInterfaceType`, `OperationalStatus`, `NetworkInterfaceComponent`.
- `PhysicalAddress` — full MAC-address parser (hyphen/colon/dot-delimited and unpunctuated hex),
  porting .NET's segment-length-inference algorithm faithfully.
- `NetworkInformationException` (uses `errno` in place of .NET's `Marshal.GetLastPInvokeError()`,
  since there's no P/Invoke layer here), `PingException`, `PingOptions`.
- `NetworkAvailabilityEventArgs`, `NetworkAddressChangedEventHandler`/
  `NetworkAvailabilityChangedEventHandler` delegate typedefs, and `NetworkChange` — the latter's
  event add/remove accessors are **stubs** (no real OS network-change notification), matching this
  codebase's pre-existing `AppDomain.UnhandledException` convention.

### What does not work yet
- `System.Net.NetworkInformation.NetworkInterface`/`Ping`/`PingReply` are not ported (see §4 — they
  depend on types already marked out of scope, and `Ping` needs raw ICMP sockets).
- `System::Net::Sockets::Socket` (the general BSD-socket-style class) has **no header at all**.
  `TcpClient`/`TcpListener`/`NetworkStream`/`UdpClient` exist and work, but `plan.sqlite3` still
  lists `TcpListener` (id 9100) as `todo` even though it's implemented as a nested class inside
  `TcpClient.hpp` — this is a **DB/reality mismatch that should be fixed first** in a future session
  (see §8 task 1), not a missing feature.
- Everything listed under §5/"remaining namespaces" in `plan.sqlite3` is simply not yet looked at:
  `System.Security.Cryptography` (50 items, the single largest remaining namespace), `System.Text`
  (36), `System.Text.Json.Serialization` (31), `System.Xml.Serialization` (30), `System.Xml.Linq`
  (24), `System.Net.Sockets` (18, minus `TcpListener`/`TcpClient`/`AddressFamily`/`SocketError`/
  etc. already done), `System.Text.Json` (17), `System.Xml.XPath` (15),
  `System.Text.RegularExpressions` (14), `System.Net.WebSockets` (12), `System.Net.Security` (9),
  `System.Threading.Channels` (9), and several smaller namespaces (full list: run the query in §7).

---

## 3. Recent changes

Most recent first (see `git log --oneline` for full history):

| Commit | Change |
|--------|--------|
| `fefee64` | `System.Net.NetworkInformation` support types (enums, `PhysicalAddress`, exceptions, `PingOptions`, `NetworkAvailabilityEventArgs`, delegates, `NetworkChange` stub). 23 new tests. |
| `d58d032` | `System.Net.Http.Json` (`JsonContent`, `HttpContentJsonExtensions`, `HttpClientJsonExtensions`) and `System.Net.Mime` (`ContentType`, `MediaTypeNames`). 44 new tests, including a real local-socket HTTP server integration test for `HttpClientJsonExtensions`. |
| `a1bd3ed` | `System.Net.Http.Headers.HttpResponseHeaders` — completes `System.Net.Http.Headers` classification. 24 new tests. |
| `73ff81b` | `System.Net.Http.Headers.HttpRequestHeaders`. 38 new tests. |
| `b7299f1` | `System.Net.Http.Headers.HttpContentHeaders`. 17 new tests. |
| `0238c72` | `System.Net.Http.Headers.HttpHeaders`, `HttpHeadersNonValidated` (the base collection design). |
| `6bcffcf`–`ef6bbdc` | The remaining individual `System.Net.Http.Headers` value types (`TransferCodingHeaderValue`+with-quality, `RetryConditionHeaderValue`, `RangeItemHeaderValue`/`RangeHeaderValue`, `RangeConditionHeaderValue`, `WarningHeaderValue`, `ViaHeaderValue`, `ProductInfoHeaderValue`, `MediaTypeHeaderValue`+with-quality, `ContentRangeHeaderValue`, `ContentDispositionHeaderValue`, `CacheControlHeaderValue`, `AuthenticationHeaderValue`) — each its own commit with tests. |
| `f586c73` and earlier | Prior session: `System.Net` core (IPAddress IPv6 rewrite, IPEndPoint, IPNetwork, WebHeaderCollection, Dns, CredentialCache, etc.) and `System.Net.Http` core (HttpClient, HttpContent family, Multipart*, HttpIOException/HttpProtocolException) fully classified — see `git log --oneline` for detail, or the previous revision of this file in git history. |

---

## 4. Current blocker / main problem

**No build- or test-breaking blocker exists right now.** The last verified state is clean and
stable (10194/10194 tests, zero warnings).

The main open item is a **scoping decision, not a bug**:

1. **`NetworkInterface`, `Ping`, `PingReply`** (`System.Net.NetworkInformation`, ids 8844/8850/8856)
   are the only `todo` items left in that namespace, and neither is a mechanical port:
   - `NetworkInterface.GetIPProperties()`/`GetIPStatistics()`/`GetIPv4Statistics()` return
     `IPInterfaceProperties`/`IPInterfaceStatistics`/`IPv4InterfaceStatistics` — all three are
     already marked `ignored` in `plan.sqlite3` (out of scope, part of the large PAL-internal
     interface-property subsystem). A full `NetworkInterface` port is therefore not possible as-is;
     only a reduced surface (`Name`, `Id`, `NetworkInterfaceType`, `OperationalStatus`,
     `GetPhysicalAddress()`, `Supports()`, `GetAllNetworkInterfaces()` via POSIX `getifaddrs`) is
     realistic, and that reduction needs to be a deliberate documented decision, not silently done.
   - `Ping`/`PingReply` need raw ICMP sockets (`SOCK_RAW`/`IPPROTO_ICMP`, or the unprivileged
     `SOCK_DGRAM`+`IPPROTO_ICMP` variant Linux supports via
     `net.ipv4.ping_group_range`) — this needs to be verified as workable in the actual sandboxed
     test environment before committing to an implementation approach, since ICMP sockets commonly
     require elevated privileges that a CI/sandbox runner may not have.
   - **Nothing has been implemented or attempted for these three yet** — this is a fresh decision
     point for the next session, not a partially-done piece of work.

2. Carried over, unchanged, still open:
   - **`Vector<T>`** (id `9228`, `System.Numerics`) is `tobedecided` — needs a human architecture
     decision (fixed-width fallback vs. real SIMD intrinsics vs. `std::experimental::simd`).
   - **`FileSystemInfo`** (id `6595`, `System.IO`) is also `tobedecided` — reason not re-investigated
     this session; check `plan.sqlite3` notes/git history before assuming why.
   - **Process risk, not a code bug:** always verify any delegated/background agent's "completed"
     report (actual commits, actual test run) before treating it as done.

---

## 5. Known bugs and limitations

New this session:

| Status | Issue |
|--------|-------|
| documented limitation | `System::Net::Http::Json::HttpClientJsonExtensions`/`HttpContentJsonExtensions` only provide non-generic, `JsonDocument`-returning overloads (`GetFromJsonAsync`, `ReadFromJsonAsync`, etc.) — .NET's generic `GetFromJsonAsync<T>`/`PostAsJsonAsync<T>` need reflection-based `JsonTypeInfo<T>` marshaling this runtime doesn't have. (Stale note fixed: `System::Text::Json::JsonSerializer::Serialize<T>()`/`Deserialize<T>()` are no longer stubs — they were given a real ADL-based (`nlohmann::ordered_json` `to_json`/`from_json`) ​implementation later in this same session; see the `JsonSerializerTests.Serialize_Int`/`Serialize_VectorOfInt`/`Deserialize_*` tests in `tests/Task41Tests.cpp`.) |
| documented limitation | `System::Net::Mime::ContentType` doesn't reproduce .NET's `_isChanged`/`_isPersisted` wire-caching (tied to `System.Net.Mail`'s message-writing pipeline, which isn't ported) — `ToString()` always recomputes fresh. Its RFC 2045 comment/CFWS grammar support is plain-whitespace-only (no nested `(...)` comments). |
| documented limitation | `System::Net::NetworkInformation::NetworkChange`'s event add/remove accessors are no-ops — there is no real OS network-change notification (Linux netlink, macOS `SCNetworkReachability`, Windows `NotifyAddrChange`), matching the pre-existing `AppDomain.UnhandledException` stub convention in this codebase. |
| documented limitation | `System::Net::NetworkInformation::NetworkInformationException`'s default constructor uses `errno` in place of .NET's `Marshal.GetLastPInvokeError()` (no P/Invoke layer here); its internal `(message, innerException)` constructor isn't reproduced (`Win32Exception`, the base class, has no inner-exception-carrying constructor to forward to). |
| fixed | `plan.sqlite3` `TcpListener` row (previously `todo`) has been corrected to `ported` — it was already fully implemented as a nested class in `include/System/Net/Sockets/TcpClient.hpp` (confirmed working — it backs `HttpClientJsonExtensionsTests` integration tests). |

Carried over from before (still accurate unless noted):

| Status | Issue |
|--------|-------|
| incomplete (needs decision) | `System::Numerics::Vector<T>` — no header exists; `tobedecided` pending an architecture choice (see §4). |
| tobedecided (needs re-investigation) | `System::IO::FileSystemInfo` — marked `tobedecided`; reason not re-verified this session. |
| missing | `System::Net::Sockets::Socket` — still no header at all. `TcpClient`/`TcpListener`/`NetworkStream`/`UdpClient`/`AddressFamily`/`SocketError`/`SocketException` all exist and are more tractable building blocks now than when this was first noted. |
| documented simplification | `System::Net::SocketAddress`'s buffer layout is this runtime's own simplified encoding, not guaranteed to match the platform sockaddr ABI. |
| documented limitation | `System::Net::Dns`'s `getaddrinfo` calls are still hardcoded to `AF_INET` even though `IPAddress` has full IPv6 support — never revisited after the `IPAddress` IPv6 rewrite. |
| documented limitation | `System::Net::WebHeaderCollection::GetValues()` returns raw stored values, not re-split through .NET's internal per-header multi-value parser table. |
| ignore (outofscope=0) | `HttpWebRequest`, `HttpWebResponse`, `WebRequest`, `WebResponse` — .NET's own source calls `WebRequest` "effectively obsolete"; superseded by `HttpClient`. |
| documented limitation | `XmlUrlResolver::GetEntity` only reads local files — no network stack for http(s) entity resolution. |
| documented limitation | `XmlReaderSettings`/`XmlWriterSettings` — most properties stored but not consulted by the concrete `XmlReader`/`XmlWriter`. |
| documented limitation | `XmlValidatingReader` performs no actual DTD/XSD validation. |
| documented limitation | `System::Threading::Tasks::TaskScheduler` doesn't route `Task` execution; `TaskFactory` omits APM `FromAsync`. |
| ignore (outofscope) | `ConcurrentExclusiveSchedulerPair`, `WaitHandleExtensions`. |
| POSIX-only (known, by design) | `System::Net::Sockets`, `System::IO::RandomAccess`. |
| POSIX/Linux-only (known, by design) | `System::AppDomain`/`AppContext`, `System::TimeZoneInfo`. |
| stub (by design, correct end state) | `System::GC`, `System::Type`, `System::Activator`. |
| legacy DB noise | `plan.sqlite3` has 15055 rows with `status='ignored'` (lowercase-d, note the distinct casing from the workflow's own `'ignore'` value) predating this workflow — inert, do not "fix" the casing, just be aware both exist. |
| needs verification | Emscripten/Windows builds have never been CI-tested; POSIX guards exist but are unverified there. |

---

## 6. Architecture notes

### Directory layout
- `include/System/...` — public headers, mirroring .NET namespace paths.
- `src/System/...` — `.cpp` bodies for complex types, same mirrored path.
- `tests/System/...` — GoogleTest files, same mirrored path; CMake's `GLOB_RECURSE` auto-discovers
  every `tests/**/*.cpp` and `src/**/*.cpp` — **but you must re-run `cmake .` (reconfigure) after
  adding a new file**, or the build silently won't pick it up.
- `vendor/` — GoogleTest, nlohmann/json, tinyxml2, miniz (vendored, never modify in place).
- `plan.sqlite3` — gitignored, local-only porting-progress database.

### Key invariants that must not be broken
- **`getXxxProperty()`/`setXxxProperty()`** naming on every property.
- **`SharpRuntime::intcs`/`bytecs`/`longcs`/`uintcs`**, not native C++ types, in public APIs.
- **C++17 nested namespace syntax** (`namespace System::Net {`).
- **No LINQ** in new ported code — use `std::ranges`.
- **POSIX-only includes** must stay inside `.cpp` files behind `#ifdef`.
- **SPDX header required** on every `.hpp`/`.cpp` file.
- **Doxygen `/** */` only** — never write a literal `*/` inside prose inside a `/** */` block.
- A derived class that declares **any** overload of a base-class method name hides *all* other
  base-class overloads of that name unless `using BaseClass::MethodName;` is added.
- **`strchr(allowedChars, c)` matches `c == 0`** (the haystack's own NUL terminator) — always guard
  with `c != 0 &&`, or prefer `std::string_view::find` instead (a real bug found and fixed twice
  in a prior session, across 5 files).

### Data flow / notable patterns
- **`System::Net::Http::Headers::HttpHeaders`** composes `NameValueCollection` (not inheritance) —
  same "composition over a non-virtual base" pattern as `WebHeaderCollection` and
  `XmlTextReader`/`XmlTextWriter`. Derived typed classes (`HttpContentHeaders` etc.) access the
  base's raw string storage through `protected getRawValue()/setRawValue()`, then parse/format
  through the individual `*HeaderValue` types on every access — there is **no lazily-parsed-and-
  cached value**, unlike real .NET's `HttpHeaders`. This is why `HttpHeadersNonValidated` is a
  functionally-identical thin wrapper here: there's no raw/parsed distinction to preserve.
- **General HTTP headers are duplicated, not shared**: `HttpRequestHeaders` and
  `HttpResponseHeaders` each independently implement Cache-Control/Connection/Date/Pragma/Trailer/
  Transfer-Encoding/Upgrade/Via/Warning, rather than sharing .NET's internal `HttpGeneralHeaders`
  helper — consistent with this codebase's broader preference for small duplicated per-file
  helpers (e.g. `tryParseRfc1123`, `splitTopLevel`, `isHttpTokenChar` are each copy-pasted across
  several `System.Net.Http.Headers` files) over introducing shared abstractions.
- **List-valued typed headers are snapshot + Add(), not a live collection**: every `getXxxProperty()`
  for a multi-value header (Accept, Via, Warning, Connection tokens, etc.) returns a `std::vector<T>`
  snapshot; there is a corresponding `AddXxx(item)` mutator instead of .NET's live
  `HttpHeaderValueCollection<T>` view.
- **`System::Net::Http::Json`**: `JsonContent`/`HttpClientJsonExtensions`/`HttpContentJsonExtensions`
  only support JSON via `nlohmann::json` values or raw strings, returning a parsed `JsonDocument`
  tree — not .NET's reflection-driven `T`. If `System::Text::Json::JsonSerializer` ever gains a
  real `Serialize<T>()`/`Deserialize<T>()` backend (e.g. via an ADL `to_json`/`from_json`
  convention), these JSON extension classes are the natural place to add generic overloads.
- **Event-accessor stubs**: for .NET static/instance events with no feasible native backing in this
  runtime (`AppDomain.UnhandledException`, `NetworkChange.NetworkAddressChanged`/
  `NetworkAvailabilityChanged`), the established pattern is literal no-op
  `add_XxxChanged(handler)`/`remove_XxxChanged(handler)` static methods — not a `std::vector` of
  registered handlers that's never invoked, and not silently omitting the API. Follow this same
  pattern for any future un-implementable event.
- **`System::Net::IPAddress`** stores IPv4 as a host-order `uint32_t` and IPv6 as 8×`uint16_t`
  groups plus a scope-ID `uint32_t`. `GetAddressBytes()` is the common currency other types use.
- `System::Net::Http`'s existing types (`HttpClient`, `HttpContent`, etc.) use a deliberately
  simplified **synchronous** content model (`ReadAsString()`/`ReadAsByteArray()`), not .NET's
  `Stream`/`Task`-based `SerializeToStreamAsync`. `System::Threading::Tasks::TaskT<T>` is a real,
  working `std::async`-backed future type (not a stub) — used this session to implement the
  `*Async` JSON extension methods by wrapping already-synchronous `HttpClient` calls in
  `TaskT<T>::Run([...]{ ... })`, matching the pattern already used internally by
  `HttpClient::GetAsync`/`PostAsync`/etc.
- `System::Xml`'s DOM classes wrap `tinyxml2::XMLNode*`/`XMLDocument` (unchanged).

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
For each `''`/`todo` item, classify without asking the user: port it (apply the full checklist in
`CLAUDE.md`), mark `ignore` (`outofscope=1` for permanent-deviation categories, `outofscope=0` for
merely-superseded/irrelevant-but-not-permanent-deviation items), or mark `tobedecided` only when
genuinely ambiguous. `in_progress` is not a valid status. **Before trusting a `plan.sqlite3` status,
spot-check the filesystem** — this session found one item (`TcpListener`) marked `todo` despite
already being fully implemented; the DB can drift from reality.

---

## 7. Useful commands

```bash
# Build (zero errors/warnings required) — reconfigure first if you added new files
cd build && cmake . && cd .. && cmake --build build --parallel 4

# Build, showing only errors/warnings
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run the full test suite
./build/SharpRuntimeTests

# Run a specific suite/test (glob pattern)
./build/SharpRuntimeTests --gtest_filter="PhysicalAddressTests.*"

# Check next unset/todo items in a namespace
sqlite3 plan.sqlite3 "SELECT id,name,type,status FROM task WHERE namespace='System.Net.Sockets' AND (status='' OR status='todo') ORDER BY id;"

# See remaining todo counts by namespace, largest first
sqlite3 plan.sqlite3 "SELECT namespace, COUNT(*) FROM task WHERE status='' OR status='todo' GROUP BY namespace ORDER BY COUNT(*) DESC;"

# Mark an item ported after review + tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"

# Find the .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit and push (routine pushes to origin/feature/work are pre-authorized)
git add <files>
git commit -m "message"
git push origin feature/work
```

There is no separate lint/format tool configured in this repository, and no standalone demo/sample
binary beyond the GoogleTest suite.

---

## 8. Next smallest tasks

1. **Fix the `TcpListener` DB/reality mismatch first** (id 9100) — it's already fully implemented
   in `include/System/Net/Sockets/TcpClient.hpp` (as a nested `TcpListener` class) and exercised by
   `tests/System/Net/Http/Json/HttpClientJsonExtensionsTests.cpp`. Just verify it against the full
   porting checklist in `CLAUDE.md` (doc-comments, SPDX, etc. — likely already fine) and mark it
   `ported`: `sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=9100;"`. No code change
   expected, just DB correctness — do this before starting new `System.Net.Sockets` work.
   - Files: `include/System/Net/Sockets/TcpClient.hpp` (read-only check).
   - Verification: none needed beyond re-reading the existing header against the checklist.

2. **Port `System::Net::Sockets::Socket`** (id 9072) — the general BSD-socket-style class, still
   completely missing. `AddressFamily`, `SocketError`, `SocketException`, `SocketAddress`,
   `EndPoint`/`IPEndPoint`, and `NetworkStream` all already exist as building blocks. Also port the
   small supporting enums in the same namespace while there (`ProtocolType`, `SocketType`,
   `SocketShutdown`, `SocketFlags`, `SelectMode`, `SocketOptionLevel`, `SocketOptionName`,
   `LingerOption`) — each is trivial once `Socket` itself exists.
   - Files: new `include/System/Net/Sockets/Socket.hpp` + `src/System/Net/Sockets/Socket.cpp`,
     `tests/System/Net/Sockets/SocketTests.cpp`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

3. **Decide the `NetworkInterface`/`Ping`/`PingReply` reduced-scope design** (ids 8844/8850/8856,
   see §4 for the full detail) — this is a genuine design fork, not mechanical:
   - For `NetworkInterface`: confirm the reduced surface (name/id/type/status/physical
     address/`Supports()`/`GetAllNetworkInterfaces()` via POSIX `getifaddrs`, skipping
     `GetIPProperties()`/`GetIPStatistics()`/`GetIPv4Statistics()` since their return types are
     `ignored`) is acceptable before implementing.
   - For `Ping`: **first** check whether raw/unprivileged ICMP sockets are actually usable in the
     sandboxed test environment (`cat /proc/sys/net/ipv4/ping_group_range`, or try opening a
     `SOCK_DGRAM`+`IPPROTO_ICMP` socket) — if not available, `Ping` tests can't verify real network
     behavior and the port would need to be scoped down further (e.g. testable packet
     construction/parsing only, with the actual send/receive loop behind a runtime capability
     check that throws a clear exception rather than silently failing).
   - Files: new `include/System/Net/NetworkInformation/NetworkInterface.hpp`/`.cpp`,
     `Ping.hpp`/`.cpp`, `PingReply.hpp`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

4. **`System.Security.Cryptography`** (50 items, the largest remaining namespace) — not started.
   Good next big block once the smaller `System.Net.*` remnants above are settled.

5. **`System.Net.Security`** (9 items) and **`System.Net.WebSockets`** (12 items) — smaller,
   adjacent to the `System.Net.*` work just completed; check for any dependency on `Socket`
   (task 2) before starting either.

6. **Decide `System::Numerics::Vector<T>` scope** (id `9228`) — unchanged, still needs a human
   architecture decision, not touched this or last session.

7. **Re-investigate `System::IO::FileSystemInfo`'s `tobedecided` status** (id `6595`) — check git
   history/prior session notes for why it was left ambiguous; it may just need a definitive port
   or ignore decision now.

---

## 9. Do not do yet

- **No broad header refactor** — `getXxxProperty()` naming and namespace style already touch
  hundreds of files across this project and CNA; do not attempt a sweeping rename/reformat pass.
- **No unifying the three (now four, with `HttpHeaders`) simplified header-bag designs** in
  `System.Net`/`System.Net.Http` unless explicitly asked — this was a deliberate, resolved decision
  this session (see §1/§6), not an oversight to "fix".
- **No work on `Vector<T>`** until the architecture decision is made by the user.
- **No attempt at real ICMP `Ping` implementation** before confirming raw/unprivileged ICMP sockets
  actually work in the sandboxed environment (see §8 task 3) — building it blind risks tests that
  can never pass in CI.
- **No Windows/Emscripten CI setup.**
- **No rewrite of `System.Net.Http`'s synchronous content model** to a `Stream`/`Task`-based one —
  that's an established design point from an earlier session, not a gap.
- **Push only to `feature/work`** — never push to `develop`/`master`, never create tags, without
  explicit per-action user approval in that turn. Routine pushes to `origin/feature/work` are
  pre-authorized.
- **No mass rewrite or reformatting** in a single commit — keep following the small, reviewable,
  per-namespace (or per-batch) commit pattern established across all sessions so far.
- **No blind trust in background/delegated agent "completed" reports** — always verify via
  `git log`/`git status`/an actual test run before treating delegated work as done.
- **No speculative API additions** — only port methods/types that actually exist in .NET's
  published surface.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation, don't stop between items). NEXT.md is a snapshot for context, not the
source of truth for process. This reflects the verified repository state as of HEAD aa23cf0
(10276/10276 tests passing, clean build, zero warnings) — do not assume anything beyond what it
documents; re-verify with a fresh build+test run after any context reset.

Query the live next-item list (System-namespace-first):
  sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo')
  ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 20;"

As of this update, that starts with System.Numerics.Colors (2 items), then the small
System.Runtime.* namespaces (~20 combined), then System.Security/.Authentication/.Principal
(~14), then the two large not-yet-started blocks: System.Security.Cryptography (50 items — the
single largest remaining namespace; will likely need a scope decision on whether to vendor a
crypto library, e.g. for AES/RSA — CLAUDE.md requires discussing that before adding one) and
System.Text/.Json*/.RegularExpressions/.Unicode (~107 combined) and System.Xml.Serialization/
.Linq/.XPath (~69 combined).

For each item: classify (port/ignore/tobedecided) per prompt.md Step 2 without asking the user,
then if porting: check the filesystem first (plan.sqlite3 can drift from reality — this session
already found and fixed one such case, TcpListener), implement per the full checklist in
CLAUDE.md (API surface, doc-comments, SPDX header, logic parity, getXxxProperty()/
setXxxProperty() naming, intcs/bytecs/etc. usage), reconfigure if you added files
(cd build && cmake . && cd ..), build clean (cmake --build build --parallel 4 — zero
errors/warnings), run the full suite (./build/SharpRuntimeTests — must show 10276+ passing, zero
failures), update plan.sqlite3's status, commit (and push to origin/feature/work — routine
pushes are pre-authorized), then move to the next item without stopping.

Do not expand scope beyond CLAUDE.md's "Known permanent deviations" and this session's own
documented reduced-scope decisions (see the per-commit notes above) — e.g. do not attempt TLS,
do not add SendFile/SendPacketsAsync to Socket, do not add permessage-deflate to WebSocket, unless
explicitly asked. Update NEXT.md's session note (prepend, don't rewrite the whole history) after
each meaningful batch of work, so this resumes cleanly after any context reset.
```
