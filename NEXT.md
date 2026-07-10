# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-10 (branch: `feature/work`, HEAD `8c4073c`) — 11006 tests passing, full clean rebuild verified (0 errors/0 warnings)*

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
