# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-07-05 (branch: `feature/work`, HEAD `017b01b`) — 9508 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting, driven entirely by a `plan.sqlite3` namespace-review workflow (full rules in `prompt.md` — read that, not this file, for the process itself). The workflow is **fully autonomous**: no per-item user confirmation, classify and proceed. Fully complete: `System.Collections.Specialized`, `System.Diagnostics`(+`.CodeAnalysis`), `System.Globalization`, `System.IO`, `System.IO.Compression`, `System.IO.Hashing`, `System.IO.IsolatedStorage`, `System.Linq`, `System.Numerics` (except `Vector<T>`, left `tobedecided`), `System.Threading` (59/60; `WaitHandleExtensions` ignored — needs a native `SafeWaitHandle` this port doesn't expose), `System.Threading.Tasks` (16/17; `ConcurrentExclusiveSchedulerPair` ignored — needs a real pluggable-scheduler queuing engine). **`System.Xml`** (62 items — fresh DOM implementation on top of vendored tinyxml2) is currently being ported by a background agent. Largest remaining `System.*` namespaces by todo count: `System.Xml` (62, in flight), `System.Security.Cryptography` (50), `System.Text` (36), `System.Text.Json.Serialization` (31), `System.Xml.Serialization` (30), `System.Net` (26).
- **Header count:** ~610+ `.hpp` files under `include/System/` (+ `SharpRuntime/`).
- **Key architectural decisions:** no runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (`int32_t`), `bytecs` (`uint8_t`), etc. Inner exceptions use `std::exception_ptr`, never `const std::exception&`.

---

## 2. Current status

### Build
**Clean.** `cmake --build build --parallel 4` — zero errors, zero warnings.

### Tests
**9508 tests passing** across 954 test suites. Zero failures, stable across repeated runs. (A background agent is currently adding more for `System.Xml` — expect this number to grow before the next handoff.)

### Branch / remote state
- `feature/work` — local working branch, HEAD `017b01b`, pushed to `origin/feature/work`. Routine pushes here are pre-authorized per project convention.
- `develop` — not touched this session. Only merge/push to `develop` when the user explicitly asks in that turn.
- `master` — untouched. Do not touch without explicit instruction.
- `plan.sqlite3` is gitignored — local workflow state only, not part of what gets pushed.

### IMPORTANT: two local clones exist
This environment has **two separate git clones** of the same repo:
- `/rv/.../sharp-runtime_work` — primary working directory, **on `feature/work`**. All active porting must happen here.
- `/rv/.../sharp-runtime` — a second clone, was left on `develop` in a prior session. **Do not edit files here.** Always double-check `pwd`/absolute paths point at `sharp-runtime_work` before editing.

### What works (this session's commits, most recent first)

**`System.Diagnostics.StackFrameExtensions`** — genuine new port (no header existed). Free-function-style static helper class (`HasSource`, `HasILOffset`, `HasMethod`, `HasNativeImage`, `GetNativeIP`, `GetNativeImageBase`) mirroring .NET's extension methods, since C++ has no extension-method syntax. `GetNativeIP`/`GetNativeImageBase` always return `IntPtr::Zero`, matching .NET CoreLib's own base implementation (not a simplification — that's .NET's real behavior too).

**`System.Globalization` — full pass, every single item had at least one real bug**, not just missing status (same pattern as last session's `Collections.Specialized` sweep):

- **`Calendar`** (base class) — `GetWeekOfYear` completely ignored the `rule`/`firstDayOfWeek` parameters (always returned `dayOfYear/7+1`); ported .NET's real `GetFirstDayWeekOfYear`/`GetWeekOfYearFullDays`/`GetWeekOfYearOfMinSupportedDateTime` algorithm. `AlgorithmType` hardcoded `SolarCalendar` instead of .NET's real default `Unknown`. `ToFourDigitYear` used the wrong century-derivation formula (`MaxSupportedDateTime` instead of a real `TwoDigitYearMax` property, which didn't exist) — added `TwoDigitYearMax` get/set and fixed the formula to match .NET exactly.
- **Every concrete calendar subclass** (`Gregorian`, `Hebrew`, `Hijri`, `Japanese`, `Julian`, `Korean`, `Persian`, `Taiwan`, `ThaiBuddhist`, `UmAlQura`) — `Eras` was inherited unoverridden from the base (`{0}`/`CurrentEra`), inconsistent with `GetEra()` which correctly returns the calendar's real era id; added the override to all ten. `AlgorithmType` was hardcoded `SolarCalendar` on **all** of them (including `Hebrew`, which is Lunisolar, and `Hijri`/`UmAlQura`, which are Lunar) — fixed each to the correct value. `TwoDigitYearMax` getter/setter were plain non-virtual methods that didn't call `VerifyWritable()` — a "read-only" calendar's `TwoDigitYearMax` could still be mutated; added `override` + `VerifyWritable()` to five of them. `GregorianCalendar`'s constructor/`CalendarType` setter had zero range validation. Several calendars threw `std::out_of_range` instead of `ArgumentOutOfRangeException` for era/year/month range checks.
- **`CharUnicodeInfo`** — `GetDigitValue` was just an alias for `GetDecimalDigitValue`, losing .NET's real distinction (superscript digits ¹²³ have a Digit value but aren't Decimal digits). All `(string, index)` overloads had zero bounds validation.
- **`CompareInfo`** — `GetHashCode`/`GetSortKey` completely ignored `CompareOptions::IgnoreCase`, so `"Hello"` and `"hello"` hashed differently and produced non-equal sort keys despite comparing equal — a real contract violation. Substring `Compare` overload used unvalidated `substr`, throwing `std::out_of_range` instead of `ArgumentOutOfRangeException`.
- **`CultureInfo`** — `InvariantCulture()`/`CurrentCulture()`/`CurrentUICulture()` violated this project's own `getXxxProperty()` convention (every other property in the same file already followed it) — renamed. Added real `setCurrentCultureProperty()`/`setCurrentUICultureProperty()` (.NET's are genuinely settable; ported C# startup code commonly does `CultureInfo.CurrentCulture = ...`).
- **`CultureNotFoundException`** — the 2-arg `(string, string)` constructor claimed to be .NET's `(paramName, message)` overload but actually stored the second arg as `InvalidCultureName`, matching neither real .NET overload. Fixed the mapping and added the two real 3-arg overloads that do carry `InvalidCultureName`.
- **`DateTimeFormatInfo`** / **`NumberFormatInfo`** — nearly every field (`FullDateTimePattern`, `AMDesignator`, `NumberDecimalSeparator`, `CurrencySymbol`, `NativeDigits`, ~25 more) was a **plain public mutable field** — violating the `getXxxProperty()`/`setXxxProperty()` convention, and worse, making `ReadOnly()`-produced instances not actually read-only (any caller could mutate a "read-only" instance's fields directly). Converted every field to private with accessors enforcing `VerifyWritable()`. Array-typed getters (day/month names, group sizes) now return copies, matching .NET's `Clone()`-on-read behavior.
- **`GregorianCalendar`** — see calendar bullet above.
- **`IdnMapping`** — all internal Punycode/UTF-8 validation threw `std::invalid_argument` instead of `ArgumentException`. Missing `GetHashCode()` (had `operator==` but no hash).
- **`ISOWeek`** — `GetWeeksInYear` used a wrong formula (`P(y)==3 || P(y)==4` instead of .NET's real `P(y)==4 || P(y-1)==3`) — these agree for many years but diverge for others (e.g. **2032**, a real 53-ISO-week year, was computed as 52). Also, `ISOWeek.ToDateTime(year, week, dayOfWeek)` — the primary API for building a date from an ISO week — was **entirely missing**; added it, and reimplemented `GetYearStart`/`GetYearEnd`/`GetWeekOfYear`/`GetYear` in terms of it to match .NET's actual structure.
- **`RegionInfo`** — `CurrentRegion()` naming violation (same as `CultureInfo`) — renamed to `getCurrentRegionProperty()`.
- **`StringInfo`** — `SubstringByTextElements(int[, int])` had **zero** bounds validation; negative/out-of-range inputs either silently clamped via `substr`'s forgiving semantics or threw the wrong exception type. Added validation matching .NET's exact unsigned-comparison logic.
- **`TextElementEnumerator`** — `GetTextElement()`/`ElementIndex` only checked "not started," not "enumeration exhausted" — after `MoveNext()` returned `false`, `GetTextElement()` kept returning the **last element's stale value** instead of throwing. Rewrote the offset/length state machine to mirror .NET's actual signed-int wraparound trick in `Reset()`, so both boundary conditions now throw `InvalidOperationException` correctly (was `std::runtime_error`).
- **`TextInfo`** — `ListSeparator` setter didn't call `VerifyWritable()` — same read-only-not-enforced bug as `DateTimeFormatInfo`/`NumberFormatInfo`.
- **`TimeSpanStyles`** — correct values, but missing Doxygen doc-comments and the `operator|`/`operator&` helpers every other `[Flags]`-equivalent enum in this directory defines. Added both.
- **Clean, no changes needed:** `CalendarAlgorithmType`, `CalendarWeekRule`, `CompareOptions`, `CultureTypes`, `DateTimeStyles`, `DaylightTime`, `DigitShapes`, `GregorianCalendarTypes` (values only — constructor validation was the actual bug), `NumberStyles`, `SortKey`, `SortVersion`, `UnicodeCategory` (values correct, only doc-comments were missing).

Also this session (before the Globalization sweep): `System.Diagnostics.DebugProvider`, `DebuggerGuidedStepThroughAttribute`, `System.Diagnostics.CodeAnalysis` (28/28 complete) — see prior session notes in git log if needed, superseded by this file.

**`System.IO` — in progress, 13/59 items done, same "every reviewed item had a real bug" pattern continuing:**

- **`BinaryReader`** — constructor didn't null-check the stream (`std::invalid_argument` instead of `ArgumentNullException`, and no `CanRead` check); fixed-read helper threw `std::runtime_error` on premature EOF instead of `EndOfStreamException`; **no disposed-state tracking at all** (`Read*` after `Close()` just touched a closed/null stream instead of throwing `ObjectDisposedException`); missing the public `ReadBytes(int) -> vector<byte>` method entirely (trim-on-EOF semantics, distinct from the exact-fill `Read(buffer,offset,count)`); `Read7BitEncodedInt()` was inlined into `ReadString()` without .NET's real 5-byte overflow check (a malformed length prefix wouldn't have been rejected).
- **`BinaryWriter`** — same null-stream gap (crash risk, not just wrong exception — no check at all); `Close()` unconditionally closed the stream **ignoring `leaveOpen_`**, inconsistent with its own destructor which checked it correctly; no disposed tracking, same as `BinaryReader`. Extracted `Write7BitEncodedInt()` as a public method to mirror the `BinaryReader` fix.
- **`BufferedStream`** — null-stream crash risk (no check); `Position` getter/setter weren't delegated to the inner stream even though every other operation was — fell through to the `NotSupportedException` base default.
- **`Directory`** (static class) — `Exists()` called throwing `std::filesystem` overloads, violating .NET's never-throws contract; `Delete()`/`Move()`/`GetFiles()`/`GetDirectories()` on a nonexistent path silently no-op'd or threw the wrong exception type instead of `DirectoryNotFoundException` (`std::filesystem::remove`/`rename` don't set `error_code` when the source simply doesn't exist — that's not treated as an error by the standard library, but .NET wants it treated as one here).
- **`DirectoryInfo`** — identical bug class to `Directory` (same throwing-overload / missing-existence-check issues), fixed the same way.
- **`DirectoryNotFoundException`** — missing the `(message, Exception)` and `(message, directoryPath)` constructors (the latter also exposing a `DirectoryPath` property) — added both.
- **`DriveInfo`** — `AvailableFreeSpace`/`TotalFreeSpace`/`TotalSize` were hardcoded to `0` and documented as a "stub," but unlike genuine OS-data gaps elsewhere, real values are trivially available cross-platform via `std::filesystem::space()` — implemented properly. `getVolumeLabel()` violated the `getXxxProperty()` convention — renamed.
- **`DriveNotFoundException`** — genuine new port (no header existed).
- **`DriveType`** — already correct, just embedded in `DriveInfo.hpp` rather than its own file; left as-is (functionally fine, not a bug).
- **`EndOfStreamException`** — wrong default message text (real logic-parity mismatch, not just cosmetic) and missing `(message, Exception)` constructor.
- **`EnumerationOptions`** — default `AttributesToSkip` was `0x022`, not `Hidden|System` (`0x06`) as its own adjacent comment claimed and as .NET's real default is — doesn't correspond to any meaningful attribute combination, a clear typo-class bug. Missing the real `BufferSize` property.
- **`ErrorEventArgs`**, **`ErrorEventHandler`** — genuine new ports (no headers existed); `FileSystemWatcher.Error` event plumbing.

### Recurring bug patterns worth knowing (confirmed again and again this session)
1. **`Eras`/similar "list" properties inherited unoverridden from a base class default** — when a subclass overrides `GetEra()` to return a real value but doesn't override the corresponding "list all eras" property, the two go out of sync. Check every override for a sibling property that also needs overriding.
2. **Static factory methods (`InvariantX()`, `CurrentX()`) not following `getXxxProperty()`** — found on `CultureInfo`, `RegionInfo`, later also on `DateTimeFormatInfo`/`NumberFormatInfo` (already correct there, established the pattern others should have followed). Always grep for `static const X&` methods without `get`/`Property` in the name.
3. **Public mutable fields instead of `getXxxProperty()`/`setXxxProperty()`, with `ReadOnly()` that doesn't actually protect anything** — the single biggest bug class this session (`DateTimeFormatInfo`, `NumberFormatInfo`). Any type with a `ReadOnly(T)` static factory needs its setters (not just a bare `isReadOnly_` flag) to actually call `VerifyWritable()`.
4. **Wrong exception types** — `std::out_of_range`/`std::invalid_argument`/`std::runtime_error` instead of `ArgumentOutOfRangeException`/`ArgumentException`/`InvalidOperationException`. Endemic across `Globalization`, same as `Specialized` last session. Always check `#include <stdexcept>` + a `throw std::...` pair as a smell.
5. **Enumerator "exhausted" state not distinguished from "not started"** — `TextElementEnumerator` checked only one boundary condition, allowing stale reads. When porting a `MoveNext()`/`Current` pair, verify **both** boundary conditions throw, not just the pre-start one.
6. **`AlgorithmType`/similar "characterization" properties hardcoded to one value across an entire type hierarchy** — every `Calendar` subclass hardcoded `SolarCalendar`, silently wrong for the Lunar/Lunisolar ones. When a base class provides a default virtual implementation, check whether *every* subclass actually needs its own override, not just some.

---

## 3. Recent changes

Full history: `git log --oneline`. Most recent first, this session's commits:

| Commit | Change |
|--------|--------|
| `017b01b` | Fixed a flaky `IsolatedStorageFileTests` test (compared live disk free-space across two calls for exact equality — legitimately racy under concurrent disk activity). |
| `170142b` | Ported `System.Threading.Tasks` (16/17 items — `Task`, `TaskCompletionSource`, `ValueTask`, `Parallel`+`ParallelOptions`/`ParallelLoopResult`/`ParallelLoopState` (added real `Stop()`/`Break()` support), `TaskStatus`, `TaskCreationOptions`, `TaskContinuationOptions`, `ConfigureAwaitOptions`, `TaskCanceledException`, `TaskSchedulerException`, `TaskScheduler`, `TaskFactory`, `UnobservedTaskExceptionEventArgs`; `ConcurrentExclusiveSchedulerPair` ignored). Fixed `Task::FromCanceled(CancellationToken)` silently discarding its argument and `Parallel.hpp`'s `int`-instead-of-`intcs` convention violation. 80 new tests. |
| `62bfef5` | Ported `System.Threading` (59/60 items). Fixed `Monitor` being a complete no-op stub (Enter/Exit/Wait/Pulse did nothing) — replaced with a real pointer-identity-keyed registry of `recursive_timed_mutex`+`condition_variable_any`. Fixed `Mutex` (didn't derive from `WaitHandle`, `WaitOne(ms)` didn't actually block up to timeout), `Semaphore`/`SemaphoreSlim` (no validation), `ReaderWriterLockSlim` (`IsReadLockHeld` etc. always returned `false`), `Barrier` (post-phase exceptions silently swallowed instead of wrapped in `BarrierPostPhaseException`). Added `AsyncLocalValueChangedArgs`, `ExecutionContext`, `IThreadPoolWorkItem`, `LockCookie`, `ReaderWriterLock` (legacy), `RegisteredWaitHandle`, `WaitCallback`, `WaitOrTimerCallback`, `WaitHandle::WaitAll`/`WaitAny`. 33 new tests. |
| `6562275` | Updated `NEXT.md` handoff doc. |
| `df32df9` | Ported remaining `System.Numerics` generic-math stubs (`ITrigonometricFunctions`, `IHyperbolicFunctions`, `ILogarithmicFunctions`, `IExponentialFunctions`, `IPowerFunctions`, `IRootFunctions`, `IEqualityOperators`, `IFloatingPointConstants`, `IBinaryFloatingPointIeee754`); fixed `DivisionRounding` (was missing `AwayFromZero`/`Euclidean`); added `TotalOrderIeee754Comparer<T>` (float/double/Half specializations). Classified `System.Linq` (LINQ/`IQueryable`/PLINQ/async-LINQ out of scope; `Enumerable` marked ported via existing practical-subset `System::Linq.hpp`) and closed out `System.Numerics` (`Vector2/3/4`, `Matrix3x2/4x4`, `Plane`, `Quaternion`, `BFloat16`, `BitOperations`, `Complex`, `BigInteger` already existed/tested, just unmarked; `Vector<T>` left `tobedecided`). |
| `6d03a2a` | Ported `System.IO.Hashing.XxHash3`/`XxHash128` (streaming + one-shot, official .NET test vectors, portable 64×64→128 multiply avoiding `__uint128_t`/`UInt128` dependency). Completes `System.IO.Hashing`. |
| `587b173` | Ported `System.IO.IsolatedStorage`: fixed `IsolatedStorageFile` to actually inherit `IsolatedStorage`; added missing `IsolatedStorageException` constructors; rewrote `IsolatedStorageFileStream` as a thin `FileStream` subclass (fixed a real bug — `FileMode::Open` was incorrectly read-only). Completes `System.IO.IsolatedStorage`. |
| `f59a86b` | Ported `System.IO.Hashing`: `NonCryptographicHashAlgorithm` base, `Adler32`, `Crc32`/`Crc32ParameterSet`, `Crc64`/`Crc64ParameterSet`; fixed `XxHash32`/`XxHash64` gaps (wrong types, big-endian bug, missing `Clone()`). |
| `f82110e` | Ported `System.IO.Compression.ZipFile` and `ZipFileExtensions`. |
| `306d5a2` | Ported `System.IO.Compression` streamless codec API (`DeflateDecoder`/`Encoder`, `GZipDecoder`/`Encoder`, `ZLibDecoder`/`Encoder`) and `ZLibStream`. Completes `System.IO.Compression`. |
| `2c302f2` | Ported `System.IO.UnmanagedMemoryStream` and `UnmanagedMemoryAccessor`. |
| `672c617` | Ported `System.IO.RandomAccess` and `System.IO.Path`. Completes `System.IO` (56 items). |
| `8e85f91` | Fixed `TextElementEnumerator` exhausted-state bug (stale reads after `MoveNext()==false`); `TextInfo.ListSeparator` read-only not enforced; added doc-comments to `TimeSpanStyles`/`UnicodeCategory`. |
| `4af2e31` | Fixed `RegionInfo::CurrentRegion()` naming; `StringInfo.SubstringByTextElements` missing bounds validation. |
| `dd47f56` | Fixed `NumberFormatInfo` public-fields-violate-convention + read-only-not-enforced (same class of bug as `DateTimeFormatInfo`). |
| `15ff337` | Fixed `IdnMapping` wrong exception type; added missing `GetHashCode()`. |
| `a3a207a` | Fixed `ISOWeek.GetWeeksInYear` wrong formula (2032 bug); added missing `ToDateTime` API. |
| `98456e5` | Fixed Julian/Korean/Persian/Taiwan/ThaiBuddhist/UmAlQura calendars: wrong `Eras`, unenforced `TwoDigitYearMax`, wrong exception types. |
| `65e60fa` | Fixed Hijri/Japanese calendars: wrong `Eras`, wrong exception types, unenforced `TwoDigitYearMax`. |
| `2e66329` | Fixed `HebrewCalendar`: wrong `Eras`, wrong exception types. |
| `60d1332` | Fixed `GregorianCalendar`: wrong `Eras`, unvalidated constructor/setter. |
| `6a076c9` | Fixed `DateTimeFormatInfo` public-fields-violate-convention + read-only-not-enforced. |
| `53b4aea` | Fixed `CultureInfo` static property naming; fixed `CultureNotFoundException` ctor overloads. |
| `c4726fe` | Fixed `CompareInfo`: `IgnoreCase` ignored by `GetHashCode`/`GetSortKey`; wrong exception type. |
| `cd0a731` | Fixed `CharUnicodeInfo`: `GetDigitValue`/`GetDecimalDigitValue` conflation; missing bounds checks. |
| `2cba6da` | Fixed `Calendar.AlgorithmType` wrong default; added correct overrides per calendar. |
| `afa0f8a` | Fixed `Calendar.GetWeekOfYear` ignoring `rule`/`firstDayOfWeek`; added `TwoDigitYearMax`. |
| `4df9ddc` | Ported `System.Diagnostics.StackFrameExtensions` (new — no header existed). |

Earlier history (prior sessions): `System.Collections.Specialized` full pass, `System.Diagnostics`/`CodeAnalysis`, `System.Collections.ObjectModel` full pass, `System.Collections` (non-generic/.Concurrent/.Frozen/.Generic/.Immutable), core value types, exception hierarchy, `DateTime`/`TimeSpan`/`TimeZoneInfo`, `Span`/`Memory`, `Buffers`, `IO`/`IO.Compression`/`IO.Hashing`, `Text`/`Text.Json`, `Threading`/`Threading.Tasks`, `Numerics`, `Net`/`Net.Http`, `Xml`.

---

## 4. Current blocker / main problem

**No active blocker.** Build is clean, 9508 tests pass (stable across 3 consecutive runs), pushed to `origin/feature/work` (HEAD `017b01b`). Not merged into `develop` — only do that when the user explicitly asks.

A background agent is **currently porting `System.Xml`** (62 items — the classic `XmlDocument` DOM API, `XmlReader`/`XmlWriter`, ~18 enums, `XmlConvert`/`XmlNamespaceManager`/`XmlQualifiedName`/resolvers). Only `XmlReader.hpp`/`XmlWriter.hpp` existed before (unmarked in the DB); everything else is a fresh port built on the vendored `tinyxml2` library. **Do not start System.Xml work again until that lands** — check `git log --oneline -5` for a new commit before touching `include/System/Xml/` (not `include/System/Xml/Linq/`, which is untouched/separate).

Fully complete this session: `System.IO` (56), `System.IO.Compression`, `System.IO.Hashing`, `System.IO.IsolatedStorage`, `System.Linq` (LINQ operator machinery correctly out-of-scope per the "No LINQ" rule — only `Enumerable`'s existing practical-subset `System::Linq.hpp` counts as ported), `System.Numerics` (except `Vector<T>`, `tobedecided`), `System.Threading` (59/60), `System.Threading.Tasks` (16/17).

Next largest unclaimed namespaces (`System.*`, todo count): `System.Xml` (62, in flight), `System.Security.Cryptography` (50), `System.Text` (36), `System.Text.Json.Serialization` (31), `System.Xml.Serialization` (30), `System.Net` (26), `System.Net.Http.Headers` (25), `System.Xml.Linq` (24).

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| missing | `System::Net::Sockets::Socket` / `TcpListener` — no header exists at all (not POSIX-only, just not ported) |
| POSIX-only | `System::IO::RandomAccess` — `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — reads `/proc/self/exe`; not portable to macOS |
| POSIX-only | `System::TimeZoneInfo` — `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| incomplete | `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; throws |
| incomplete | `ArrayList.GetEnumerator()` — returns `nullptr`; not yet iterable via `IEnumerator*` |
| incomplete | `CopyTo(Array, int)` — `System.Array` type does not exist; skipped on all collections |
| deliberate simplification | `Globalization` types (`CultureInfo`, `CompareInfo`, `TextInfo`, `CharUnicodeInfo`, etc.) only meaningfully support the invariant/en-US locale; no real ICU/OS culture data. Documented in each type's doc-comment, not a silent gap. |
| deliberate simplification | `Debug`'s output does not auto-indent after newlines |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub (by design) | `System::GC` — all methods are no-ops; correct end state, not a gap |
| stub (by design) | `System::Type` / `System::Activator` — no runtime reflection, correct end state |
| stub | `System::Enum` — `GetNames`/`GetValues`/`Parse` not implemented |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| workflow risk | Duplicate GoogleTest suite names cause linker errors — always check for collisions |
| legacy DB noise | `plan.sqlite3` has 15055 rows with `status='ignored'` (lowercase-d, different from the workflow's `'ignore'`) — predates the current workflow, inert legacy data |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs, ulongcs
  System/
    Collections/Specialized/            ← complete
    Diagnostics/                        ← complete (including StackFrameExtensions, new this session)
    Diagnostics/CodeAnalysis/           ← complete (28/28)
    Globalization/                     ← complete this session (~40 items) — every item had ≥1 real bug fixed
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← GoogleTest .cpp files, "Batch##Tests.cpp" per session
plan.sqlite3                            ← porting-status tracker (gitignored, local-only)
prompt.md                               ← canonical plan.sqlite3 workflow instructions
```

### Invariants that must not be broken
1. **Zero errors, zero warnings** before every commit.
2. **9179+ tests passing** — never go below this watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files). This includes **static factory methods** like `getInvariantCultureProperty()` — a bare `InvariantCulture()`/`CurrentRegion()` name is a bug, not a stylistic choice (found and fixed repeatedly this session).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (`int32_t`) in public APIs mirroring .NET `int` parameters; **`SharpRuntime::shortcs`** (`int16_t`) for .NET `short`.
6. **SPDX header on every file.**
7. **Doxygen `/** */`** on all public declarations — never `///`.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef`.
9. **Inner exceptions use `std::exception_ptr`**, never `const std::exception&`.
10. **No broad header refactor** — property naming touches 449+ files in CNA. (Renames done this session — e.g. `CultureInfo::InvariantCulture()` → `getInvariantCultureProperty()` — were verified via grep to have zero production/CNA-facing callers before renaming; only test files referenced the old names.)
11. **Push only to `feature/work`** — never push to `develop` or `master`, and never create tags, without explicit per-action user approval.
12. **GPG signing times out** — always commit with `git -c commit.gpgsign=false commit`.
13. **plan.sqlite3 processing is fully autonomous** — classify and proceed without asking per item.

### Established local conventions worth following
- **Exception message text** for key-not-found is the plain `KeyNotFoundException()` default message; duplicate-key `ArgumentException` message is `"An item with the same key has already been added."` — used consistently across containers.
- **Read-only enforcement pattern**: any type with a `static T ReadOnly(const T&)` factory must have a `VerifyWritable()` (or equivalently-named) private helper that every setter calls, throwing `InvalidOperationException("Instance is read-only.")`. A `ReadOnly()` that only flips a bool with no setter enforcement is a bug, not a partial implementation — found on `DateTimeFormatInfo`, `NumberFormatInfo`, `TextInfo` this session, already correctly done on `Calendar`/`DateTimeFormatInfo` (after fixing) as the reference pattern.
- **Shared live-view wrappers** (`ReadOnlyDictionary<K,V>`, `ReadOnlyObservableCollection<T>`, `OrderedDictionary::AsReadOnly()`) use `std::shared_ptr<Underlying>`. Plain value-copying wrappers (`ReadOnlyCollection<T>`) are a deliberate, different, already-established choice. Don't conflate the two.
- **Static members of a class's own type require declare-in-class, define-outside-class** — a self-referential `inline static T member{...};` initializer inside the class body fails with "incomplete type" (confirmed this session on `CultureInfo::currentCulture_`). Pattern: declare `static T member;` in the class, then `inline T ClassName::member{...};` immediately after the closing brace, still in the same header.
- **Case-insensitive key comparison for `std::unordered_map`-backed Specialized types**: implement via custom hash+equal functor *instances* holding a runtime `bool caseInsensitive` flag. See `HybridDictionary`/`NameValueCollection`.

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
- Table `task`, columns: `id, namespace, name, type, internal, outofscope, status`.
- Valid status values: `''` (unset), `todo`, `ported`, `ignore`, `tobedecided`. **`in_progress` is not valid.**
- Query next unset/todo item, `System`-namespace-first:
  ```sql
  SELECT id,namespace,name,type FROM task
  WHERE (status='' OR status='todo')
  ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name
  LIMIT 1;
  ```

---

## 7. Useful commands

```bash
# Build
cmake --build build --parallel 4

# Build — errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run all tests
./build/SharpRuntimeTests

# Run a specific suite
./build/SharpRuntimeTests --gtest_filter="BinaryReader*"

# Check next unset/todo type (System namespace prioritized)
sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 8;"

# Mark an item ported after review+tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"

# Check .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"
```

---

## 8. Next smallest tasks

### Task 1 — Check on the System.Threading background agent
- **Goal:** Verify it finished, pulled/merged cleanly, build is clean, tests pass. Check `git log --oneline -5` for its commit before touching anything in `include/System/Threading/`.
- **If it's done:** move to `System.Threading.Tasks` (17 items, separate namespace it did not touch) or the next largest namespace (`System.Xml`, 62 items).
- **If it's still running:** avoid concurrent `cmake --build`/test runs against the same `build/` directory; do DB classification or research on a different namespace instead (as this session did with `System.Numerics`/`System.Linq` while it ran).

### Task 2 — System.Xml (62 items) or System.Security.Cryptography (50 items)
- Neither has been scoped yet this session. Query `plan.sqlite3` for the live per-item list before starting; check `include/System/Xml/` and `include/System/Security/Cryptography/` for what already exists (this session found that `System.Numerics` and much of `System.Threading` were already implemented but simply unmarked in the DB — always check the filesystem before assuming a fresh port).
- `System.Security.Cryptography` in particular touches real crypto primitives (hashing, symmetric/asymmetric encryption) — verify against test vectors, not just API shape, same discipline used for `System.IO.Hashing`'s CRC/xxHash vectors this session.

---

## 9. Do not do yet

- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA. (Static-method renames done this session were individually verified zero-production-usage first — that's the bar for "safe," not "small.")
- **No LINQ port** — use `std::ranges` in all new ported code.
- **No Windows / Emscripten CI** — POSIX-only subsystems are documented bugs, not open work items.
- **Push only to `feature/work`** — pushing to `develop` or `master`, or merging `feature/work` → `develop`, requires the user explicitly asking in that turn. Routine pushes to `origin/feature/work` are pre-authorized.
- **No new vendored libraries** without discussing scope impact.
- **No speculative API additions** — only add methods present in .NET's published API surface. (The `TimeSpanStyles` operator|/operator& added this session are not speculative — every sibling `[Flags]` enum in the same directory already has them, and .NET's `[Flags]` enums get `|`/`&` for free from the language; C++ needs the explicit operators to be usable at all.)
- **No work on `System::Type` / `System::Activator`** — stubs are the correct end state.
- **No duplicate GoogleTest suite names** — check for collisions.
- **No reintroduction of `///` Doxygen** — all headers use `/** */`.
- **No mass rewrite or reformatting** in a single commit — incremental changes only.
- **No editing files in the second clone (`/rv/.../sharp-runtime`, currently on `develop`)** — always verify the absolute path targets `sharp-runtime_work` before writing.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation). NEXT.md is a snapshot for context, not the source of truth for process.

IMPORTANT: work only in /rv/.../sharp-runtime_work (branch feature/work). A second clone at
/rv/.../sharp-runtime exists on develop — do not edit files there.

System.Globalization, System.IO, System.IO.Compression, System.IO.Hashing, System.IO.IsolatedStorage,
System.Linq, and System.Numerics (except Vector<T>, tobedecided) are now 100% complete (verified via
plan.sqlite3 queries — see §4). A background agent is (or was) porting System.Threading (60 items) —
check git log first (see Task 1 in §8) before touching include/System/Threading/. After confirming
that's landed cleanly, move to Task 2 in §8: System.Xml (62 items) or System.Security.Cryptography
(50 items), whichever the user has no other preference on.

For that task and every task after it:
  1. Look up the .NET reference in /rv/tmp/runtime/src/libraries/ and read the existing C++ header
     for context (check whether it already exists — repeatedly this session, "todo" items turned out
     to already have a file, either with a real bug or just unmarked in the DB, not missing entirely).
  2. Review/implement per the full checklist in CLAUDE.md (API surface, doc-comments, SPDX, logic
     parity, bounds/null validation, getXxxProperty()/setXxxProperty() naming including on static
     factory methods, VerifyWritable() enforcement on any ReadOnly()-capable type).
  3. Run: cmake --build build --parallel 4   (zero errors, zero warnings)
  4. Run: ./build/SharpRuntimeTests           (9439+ tests must still pass)
  5. Mark it ported: sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"
  6. Commit only the files for that change: git -c commit.gpgsign=false commit -m "..."
  7. Continue to the next todo item per prompt.md's Step 1 — don't stop to ask before each item.
  8. Update NEXT.md with what changed before ending the session.
  9. Do NOT push to origin without the user asking in that turn — this session's commits are not
     yet pushed. Never push to `develop` or `master`, and never merge feature/work → develop,
     without the user explicitly asking in that turn.
```
