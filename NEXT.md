# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-06-08 (branch: develop) — session 24*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET/System namespace in C++. It serves as the foundation layer for **CNA** (C++ port of the XNA game framework) and **mobile-eggbert** (ported Windows Phone game).

**Main goal:** provide `System::*` API compatibility so that ported C#/XNA game code compiles against C++ headers with minimal changes.

**Current phase:** systematic test coverage expansion. Porting (waves 1–20) is largely complete. Focus is now writing GoogleTest suites to verify every ported type.

**Key architectural decisions:**
- All new types are **header-only** `.hpp`; `.cpp` only for older/complex types (Exception hierarchy, Guid, DateTime, Convert, BinaryReader/Writer, Encoding, etc.)
- CMake `GLOB_RECURSE` auto-discovers all `src/*.cpp` — no manual registration needed for new `.cpp` files
- Namespace: `System`, `System::IO`, `System::Collections::Generic`, etc. using C++17 nested syntax
- Property naming convention: `getXxxProperty()` / `setXxxProperty()` for all `.NET`-style properties
- Primitive typedefs live in `SharpRuntime::` (`intcs = int32_t`, `bytecs = uint8_t`, `shortcs = int16_t`, `charcs = char16_t`, etc.)
- Immutable collections use `shared_ptr<const std::container<T>>` — mutations return new instances

---

## 2. Current status

### Build
- **Clean build:** `cmake --build build --parallel 4` → `[100%] Built target SHARP_RUNTIME` ✅
- Output: `build/libSHARP_RUNTIME.a`
- One pre-existing warning: `Char.hpp:16` — null character in `u' '` literal (cosmetic, harmless)

### Tests
- **Tests ARE built:** `SHARP_RUNTIME_BUILD_TESTS=ON` in CMake cache ✅
- **All 1610 tests pass:** `./build/SharpRuntimeTests` → `1610 tests from 160 test suites` ✅
- GoogleTest is present at `vendor/googletest/`
- 47 test files in `tests/System/`

### What IS tested (1437 tests, 44 files)

| Suite file | Tests |
|------------|-------|
| `PrimitiveTypeTests.cpp` | Int32, Int64, UInt32 (18) |
| `PrimitiveTypeTests2.cpp` | Int16, UInt16, SByte, Boolean, Char, Single, Double (98) |
| `DecimalTests.cpp` | Decimal 128-bit (47) |
| `MathTests.cpp` | Math static methods (37) |
| `ConvertTests.cpp` | Convert static methods (43) |
| `GuidTests.cpp` | Guid (24) |
| `DateTimeTests.cpp` | DateTime + TimeSpan (23) |
| `TimeSpanTests.cpp` | TimeSpan (existing) |
| `RandomTests.cpp` | Random (existing) |
| `EventHandlerTests.cpp` | EventHandler (existing) |
| `ExceptionTests.cpp` | 11 exception types (33) |
| `Text/StringBuilderTests.cpp` | StringBuilder (27) |
| `Text/EncodingWebTests.cpp` | HtmlEncoder, UrlEncoder, JavaScriptEncoder (36) |
| `Text/JsonTests.cpp` | JsonDocument / JsonElement (28) |
| `Text/EncodingTests.cpp` | Encoding UTF8/ASCII/Unicode (14) |
| `IO/HashingTests.cpp` | CRC32, XxHash32, XxHash64 (27) |
| `IO/StreamTests.cpp` | MemoryStream, StringReader, StringWriter (29) |
| `Collections/ImmutableCollectionTests.cpp` | ImmutableArray, ImmutableList, ImmutableDictionary (33) |
| `Collections/PriorityQueueTests.cpp` | PriorityQueue (20) |
| `Collections/Generic/CollectionsTests.cpp` | List, Dictionary, HashSet (52) |
| `Collections/Generic/QueueStackTests.cpp` | Queue, Stack (34) |
| `Collections/Generic/LinkedListSortedSetTests.cpp` | LinkedList, SortedSet (38) |
| `Numerics/BigIntegerTests.cpp` | BigInteger (45) |
| `Numerics/ComplexTests.cpp` | Complex (38) |
| `Diagnostics/StopwatchTests.cpp` | Stopwatch (15) |
| `Diagnostics/DebugTraceTests.cpp` | Debug + Trace (22) |
| `TimeZoneInfoTests.cpp` | TimeZoneInfo (27) |
| `UriTests.cpp` | Uri (34) |
| `Threading/ThreadingTests.cpp` | Thread/Interlocked/Monitor/Mutex/Semaphore/SemaphoreSlim/MRE/ARE/CancellationToken/SpinLock/Volatile/Timeout (49) |
| `BitConverterTests.cpp` | BitConverter (23) |
| `ConsoleTests.cpp` | Console (17) |
| `EnvironmentTests.cpp` | Environment (8) |
| `VersionTests.cpp` | Version (19) |
| `ArrayTests.cpp` | Array (26) |
| `BufferTests.cpp` | Buffer (15) |
| `TupleTests.cpp` | Tuple2/Tuple3/Tuple4 (20) |
| `Globalization/GlobalizationTests.cpp` | CultureInfo/NumberFormatInfo/RegionInfo/StringInfo/UnicodeCategory (69) |
| `Net/NetTests.cpp` | IPAddress/IPEndPoint/HttpStatusCode/WebUtility (67) |
| `Buffers/BuffersTests.cpp` | ArrayPool/OperationStatus/StandardFormat (29) |
| `ComponentModel/ComponentModelTests.cpp` | 9 attribute types + INotifyPropertyChanged/Changing (39) |
| `Runtime/RuntimeTests.cpp` | CompilerServices/GCSettings/InteropServices/Versioning (60) |
| `Security/SecurityTests.cpp` | SecurityAttributes/SecurityException/CryptographicException (25) |
| `Xml/XmlTests.cpp` | ReadState/XmlNodeType enums + XmlReader/XmlWriter stubs + Linq types (55) |
| `IO/Compression/CompressionTests.cpp` | CompressionMode/ZipArchiveMode enums + GZipStream/DeflateStream + ZipArchive stubs (35) |
| `Net/Sockets/SocketsTests.cpp` | TcpClient/TcpListener/UdpClient stub-throws + constructors (20) |
| `IO/IOTests.cpp` | IO enums (FileMode/FileAccess/FileShare/FileAttributes/FileOptions/SeekOrigin/SearchOption/SearchTarget/MatchCasing/MatchType/HandleInheritability/UnixFileMode) + IO exceptions + IsolatedStorageScope + EnumerationOptions/FileStreamOptions/DriveType/DriveInfo (78) |
| `IO/IOStreamTests.cpp` | Path + File + FileInfo + Directory/DirectoryInfo + BinaryReader/Writer + StreamReader/Writer + BufferedStream + FileStream + IsolatedStorageFile (75) |

### What is NOT yet tested (priority order)

1. **`System::Collections::Concurrent`** — ConcurrentDictionary, ConcurrentQueue, ConcurrentStack → **Task 34**
2. **`System::Collections::ObjectModel`** — ObservableCollection, ReadOnlyCollection, ReadOnlyDictionary → **Task 34**
3. **`System::Collections::Specialized`** — NameValueCollection, StringCollection, BitVector32, … → **Task 34**
4. **`System::Diagnostics` remaining** — DebuggerDisplayAttribute, DebuggerBrowsableAttribute, StackTrace/StackFrame, UnreachableException → **Task 35**
5. **`System::Text` remaining** — Rune, NormalizationForm, CompositeFormat, UTF7/UTF32/Latin1Encoding, Encoder/Decoder, RegularExpressions stubs → **Task 35**

### What does NOT work yet (implementation gaps)

- **GZipStream / DeflateStream / ZipArchive:** throw `NotImplementedException` — awaiting zlib/miniz
- **XmlReader / XmlWriter:** throw `NotImplementedException` — awaiting tinyxml2/pugixml
- **TcpClient / UdpClient:** throw `NotImplementedException` — awaiting POSIX/Winsock
- **Task/TaskT:** use `std::async(std::launch::async)`, not a full threadpool scheduler
- **Thread::CurrentThread():** returns a proxy struct, not a real `Thread` — no `Join()` / `IsAlive`
- **BigInteger::TryParse:** not yet implemented
- **AppDomain / AppContext / GC:** stubs only

---

## 3. Recent changes (last 3 sessions)

**Session 24 — Task 33 (Net::Sockets + IO remaining):**

| File | Change |
|------|--------|
| `tests/System/Net/Sockets/SocketsTests.cpp` | New — 20 tests: TcpClient/TcpListener/UdpClient constructors + stub-throws |
| `tests/System/IO/IOTests.cpp` | New — 78 tests: all IO enums (FileMode/FileAccess/FileShare/FileAttributes/FileOptions/SeekOrigin/SearchOption/SearchTarget/MatchCasing/MatchType/HandleInheritability/UnixFileMode) + IO exceptions (IOException/FileNotFoundException/DirectoryNotFoundException/EndOfStreamException/PathTooLongException/FileLoadException/InvalidDataException) + IsolatedStorageScope + EnumerationOptions/FileStreamOptions/DriveType/DriveInfo |
| `tests/System/IO/IOStreamTests.cpp` | New — 75 tests: Path (string ops), File (write/read/copy/move/delete), FileInfo, Directory/DirectoryInfo, BinaryReader/Writer roundtrip (int8–64/float/double/string/bool), StreamWriter/Reader, BufferedStream delegation, FileStream write+read, IsolatedStorageFile |

**Session 23 — Task 32 (Xml + IO::Compression):**

| File | Change |
|------|--------|
| `tests/System/Xml/XmlTests.cpp` | New — 55 tests: ReadState/XmlNodeType enums, XmlReader/XmlWriter stub-throws, XName/XAttribute/XElement/XDocument/XDeclaration |
| `tests/System/IO/Compression/CompressionTests.cpp` | New — 35 tests: CompressionMode/ZipArchiveMode enums, GZipStream/DeflateStream CanRead/CanWrite/Flush/Close + stub-throws, ZipArchive/ZipArchiveEntry stub-throws |

Notes: `XDocument::Save` declared but not defined — excluded. Vexing-parse `ZipArchive(ptr)` fixed with `{ ZipArchive z(ptr); }`. `[[nodiscard]]` stub callers wrapped with `(void)`. `XElement(XName("name"), "val")` required — two-arg `const char*` constructor missing.

**Session 22 — Task 31 (Runtime + Security):**

| File | Change |
|------|--------|
| `include/System/Runtime/InteropServices/InteropAttributes.hpp` | Fix: fully-qualify `CharSet` and `CallingConvention` member type references (`-Wchanges-meaning`) |
| `tests/System/Runtime/RuntimeTests.cpp` | New — 60 tests: CompilerServices/GCSettings/InteropServices enums+attributes/Versioning/ExternalException |
| `tests/System/Security/SecurityTests.cpp` | New — 25 tests: SecurityAttributes/SecurityException/CryptographicException |

**Session 21 — Task 30 (Buffers + ComponentModel):**

| File | Change |
|------|--------|
| `include/System/Buffers/ArrayPool.hpp` | Fix: move `SharedArrayPool<T>` outside `ArrayPool<T>` — eliminates incomplete-type inheritance warning |
| `tests/System/Buffers/BuffersTests.cpp` | New — 29 tests: ArrayPool/OperationStatus/StandardFormat |
| `tests/System/ComponentModel/ComponentModelTests.cpp` | New — 39 tests: 9 attribute types + INotifyPropertyChanged/Changing |

Note: `DefaultValueAttribute.hpp` conflicts with `DescriptionAttribute.hpp` — tests include only the latter. `DefaultValueAttribute(std::string("…"))` needed to avoid `const char*` → `bool` over `std::string` overload resolution.

*For older history see `git log --oneline`.*

---

## 4. Known bugs and limitations

| Status | Issue |
|--------|-------|
| **confirmed** | `Task` / `TaskT` use `std::async(std::launch::async)` — raw OS thread per task, no threadpool |
| **confirmed** | `XmlReader` / `XmlWriter` throw `NotImplementedException` always |
| **confirmed** | `GZipStream`, `DeflateStream`, `ZipArchive` throw `NotImplementedException` always |
| **confirmed** | `TcpClient`, `UdpClient` throw `NotImplementedException` always |
| **incomplete** | `Thread::CurrentThread()` returns a proxy struct — cannot `Join()` or check `IsAlive` |
| **incomplete** | `Char::Parse(string)` only handles 1-byte ASCII — no multi-byte UTF-8 |
| **incomplete** | `TimeZoneInfo::FindSystemTimeZoneById()` only knows UTC, Local, and a handful of hardcoded zones |
| **incomplete** | `BigInteger::TryParse` not implemented |
| **incomplete** | `AppDomain`, `AppContext`, `GC` are stubs |
| **risky** | `SharpRuntime::charcs = char16_t` — some headers cast to `wint_t`; not identity on all platforms |
| **known warning** | `Char.hpp:16` emits "null character in literal" — cosmetic, does not affect behaviour |
| **excluded** | `Calendar.hpp` + `ISOWeek.hpp` reference `DateTime` properties not yet in `DateTime.hpp` |
| **excluded** | `DefaultValueAttribute.hpp` conflicts with `DescriptionAttribute.hpp` — duplicate class def |

---

## 5. Architecture notes

### Module layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← primitive typedefs (intcs, bytecs, shortcs, charcs, etc.)
  System/                               ← root namespace: exceptions, Math, Convert, ...
  System/Collections/                   ← Generic/, Concurrent/, Immutable/, ObjectModel/, Specialized/
  System/IO/                            ← Stream, File, BinaryReader/Writer, Compression/, Hashing/
  System/Text/                          ← StringBuilder, Encoding, Rune, Json/, Encodings/Web/
  System/Threading/                     ← Thread, Monitor, Mutex, ..., Tasks/
  System/Numerics/                      ← BigInteger, Complex, GenericMathInterfaces
  System/Diagnostics/                   ← Debug, Trace, Stopwatch, CodeAnalysis/
  System/Globalization/                 ← CultureInfo, NumberFormatInfo, Calendar, ...
  System/Runtime/                       ← CompilerServices/, InteropServices/, Versioning/
  System/Net/                           ← IPAddress, IPEndPoint, HttpStatusCode, Sockets/
  System/Xml/                           ← XmlReader, XmlWriter, Linq/
  System/ComponentModel/                ← attributes, INotifyPropertyChanged, DataAnnotations/
  System/Security/                      ← exceptions, security attributes
  System/Buffers/                       ← ArrayPool, IMemoryOwner, OperationStatus
src/                                    ← .cpp for types needing it (exceptions, Guid, DateTime, Encoding, etc.)
tests/System/                           ← GoogleTest suites (44 files, 1437 tests)
vendor/googletest/                      ← bundled test framework
vendor/nlohmann/json.hpp                ← nlohmann/json 3.10.4 (MIT)
```

### Invariants that must not be broken
1. **All new types are header-only** — only add `.cpp` if there is a genuine ODR or compile-time reason
2. **Property naming:** always `getXxxProperty()` / `setXxxProperty()` — never bare public fields
3. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET Foundation attribution
4. **Namespace syntax:** `namespace System::Collections::Generic {` — C++17 nested form
5. **`SharpRuntime::intcs` not `int`** in public APIs that mirror .NET `int` parameters
6. **Build must stay clean** — zero errors, zero warnings before any commit
7. **`inline` statics** in headers for ODR-safe static members

### API compatibility rules
- Method names mirror .NET exactly (PascalCase)
- Template parameter names: `TKey`, `TValue`, `TElement`, `TPriority`, etc.
- Static factory methods preferred where .NET uses them (`Empty()`, `Create()`, `Default()`)

---

## 6. Useful commands

```bash
# Build
cmake --build build --parallel 4

# Run all tests
./build/SharpRuntimeTests

# Run a single suite
./build/SharpRuntimeTests --gtest_filter="StopwatchTests.*"

# Run multiple suites
./build/SharpRuntimeTests --gtest_filter="StopwatchTests.*:EncodingTests.*"

# Check for warnings/errors
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Git log
git log --oneline -15

# Count test files
find tests -name "*.cpp" | wc -l

# Count header files
find include -name "*.hpp" | wc -l
```

---

## 7. Next tasks

### Task 33 — Net::Sockets + IO remaining ✅ DONE (session 24, 1610 tests)

---

### Task 34 — Collections remaining ← NEXT

- `Collections/Concurrent/`: ConcurrentDictionary, ConcurrentQueue, ConcurrentStack
- `Collections/ObjectModel/`: ObservableCollection, ReadOnlyCollection, ReadOnlyDictionary, KeyedCollection
- `Collections/Specialized/`: NameValueCollection, StringCollection, BitVector32, HybridDictionary, ListDictionary, StringDictionary, OrderedDictionary

---

### Task 35 — Diagnostics + Text remaining

- `Diagnostics/` remaining: DebuggerDisplayAttribute, DebuggerBrowsableAttribute, StackTrace/StackFrame stubs, UnreachableException
- `Text/` remaining: Rune, NormalizationForm, CompositeFormat, UTF7/UTF32/Latin1Encoding, Encoder/Decoder, RegularExpressions stubs

---

## 8. Do not do yet

- **No broad header refactor** — changing naming conventions across 444 files would break CNA and all dependents
- **No LINQ (System.Linq/Enumerable)** — use `std::ranges` algorithms in ported code instead
- **No zlib/tinyxml2/pugixml integration** until the test suite has stable broad coverage
- **No changes to `SharpRuntime::` primitive typedefs** — API foundations used by hundreds of headers
- **No split of header-only types into .cpp** unless there is a demonstrated linker ODR failure
- **No merge to master** until test coverage is substantially broader (currently 1437 tests / 444 headers)

---

## 9. Resume prompt

> Working directory: `/rv/data/development/github.com/openeggbert/sharp-runtime`. Branch: `develop`.
>
> Read NEXT.md section 7 — **Task 34** is next: Collections remaining.
>
> Scan `include/System/Collections/Concurrent/`, `include/System/Collections/ObjectModel/`, `include/System/Collections/Specialized/`. Write test files covering enums + stub-throws + functional methods.
>
> Build: `cmake --build build --parallel 4` (zero errors, zero warnings)
> Run full suite: `./build/SharpRuntimeTests` — must show 1610+ passing, 0 failing.
> Commit, then update NEXT.md: bump count, mark Task 34 done.
