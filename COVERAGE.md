# COVERAGE.md — sharp-runtime .NET API Coverage Analysis

*Generated: 2026-06-13 | Branch: develop | Tests: 3213 passing*

This document maps which .NET `System.*` namespaces, classes, and methods are present in
sharp-runtime, and whether each is fully implemented, partial, or a stub.

**Status icons:**
- ✅ implemented
- ⚠️ partial / stub
- ❌ missing from sharp-runtime

---

## Statistics

| Metric | Value |
|--------|-------|
| Total public headers | 449 |
| Headers with .cpp bodies | 79 |
| Test suites | 479 |
| Tests passing | 3171 |
| Tested headers | ~408 (~91%) |
| Pure interfaces (`IXxx`) — intentionally untested | ~42 |
| Classes DONE (fully implemented) | ~320 |
| Classes PARTIAL (80–99%) | ~60 |
| Classes STUB (<20%) | ~15 |

---

## System (core)

### Object (DONE)
| Method | Status |
|--------|--------|
| ToString | ✅ |
| GetType | ✅ |
| GetHashCode | ✅ |
| Equals | ✅ |

### String (DONE)
| Method | Status |
|--------|--------|
| IsNullOrEmpty | ✅ |
| IsEmpty | ✅ |
| IsNullOrWhiteSpace | ✅ |
| StartsWith | ✅ |
| EndsWith | ✅ |
| Contains | ✅ |
| Split(char) | ✅ |
| Replace(string, string, string) | ✅ |
| Replace(string, char, char) | ✅ |
| Substring(string, int) | ✅ |
| Substring(string, int, int) | ✅ |
| Trim / TrimStart / TrimEnd | ✅ |
| Concat(2/3/4 strings) | ✅ |
| Concat(vector\<string\>) | ✅ |
| Join(separator, vector\<string\>) | ✅ |
| Format(format, int) | ✅ |
| Format(format, string) | ✅ |
| ToString(int, width, fill) | ✅ |

### Exception hierarchy (DONE)
All standard exception types are present with `Message` / `what()` / `ToString()`:
`ArgumentException`, `ArgumentNullException`, `ArgumentOutOfRangeException`,
`ArithmeticException`, `DivideByZeroException`, `FormatException`,
`IndexOutOfRangeException`, `InvalidCastException`, `InvalidOperationException`,
`InvalidProgramException`, `InvalidTimeZoneException`, `MemberAccessException`,
`MissingFieldException`, `MissingMemberException`, `MissingMethodException`,
`NotImplementedException`, `NotSupportedException`, `NullReferenceException`,
`ObjectDisposedException`, `OperationCanceledException`, `OutOfMemoryException`,
`OverflowException`, `PlatformNotSupportedException`, `RankException`,
`StackOverflowException`, `SystemException`, `TimeoutException`,
`TimeZoneNotFoundException`, `TypeAccessException`, `TypeInitializationException`,
`TypeLoadException`, `TypeUnloadedException`, `UnauthorizedAccessException`,
`MulticastNotSupportedException`, `InvalidDataException` — all ✅

### Convert (DONE)
| Method | Status |
|--------|--------|
| ToInt16/32/64, ToUInt16/32/64 | ✅ |
| ToDouble (locale-safe `from_chars`) | ✅ |
| ToSingle (locale-safe `from_chars`) | ✅ |
| ToString(int / float / double) | ✅ locale-safe |
| ToBoolean, ToByte, ToChar, ToDecimal | ✅ |
| parseIntBase (strtoll, not strtol) | ✅ |

### Math (DONE)
| Method | Status |
|--------|--------|
| Sin, Cos, Tan, Sqrt, Abs, Min, Max | ✅ |
| Pow, Log, Log10, Exp | ✅ |
| Floor, Ceiling, Round, Truncate | ✅ |
| Atan2, Asin, Acos, Atan | ✅ |
| Sign, Clamp, BitDecrement, BitIncrement | ✅ |
| E, PI, Tau (constants) | ✅ |

### DateTime (DONE)
| Method | Status |
|--------|--------|
| Constructor(ticks), Constructor(y, m, d, h, min, s) | ✅ |
| Year, Month, Day, Hour, Minute, Second, Millisecond | ✅ |
| DayOfWeek, DayOfYear, Ticks | ✅ |
| Today, Now, UtcNow (static) | ✅ |
| AddDays, AddHours, AddMinutes, AddSeconds, AddMonths, AddYears | ✅ |
| ToString (ISO-8601) | ✅ |
| gmtime_r / gmtime_s platform guards | ✅ |

### TimeSpan (DONE)
| Method | Status |
|--------|--------|
| Constructor(ticks), Constructor(h, m, s) | ✅ |
| Ticks, TotalMilliseconds, TotalSeconds, TotalMinutes, TotalHours | ✅ |
| Add, Subtract, Duration, Negate | ✅ |
| Zero, MaxValue, MinValue (static) | ✅ |

### Decimal (DONE)
| Method | Status |
|--------|--------|
| Constructors (int, long, double, string) | ✅ |
| Add, Subtract, Multiply, Divide, Remainder | ✅ |
| ToDouble, ToString, Parse, TryParse | ✅ |
| Negate, Zero, One, MinValue, MaxValue (static) | ✅ |

### Guid (DONE)
| Method | Status |
|--------|--------|
| Constructor(string), Constructor(bytes) | ✅ |
| NewGuid (static), Empty (static) | ✅ |
| ToString, ToByteArray | ✅ |
| Parse, TryParse | ✅ |

### Uri (DONE)
| Method | Status |
|--------|--------|
| Constructor(string) | ✅ |
| Scheme, Host, Port, Path, Query, Fragment | ✅ |
| AbsolutePath, AbsoluteUri, ToString | ✅ |

### Numeric primitive types (DONE)
`Int16`, `Int32`, `Int64`, `Int128`, `UInt16`, `UInt32`, `UInt64`, `UInt128`,
`Single`, `Double`, `Half`, `Byte`, `SByte`, `Char`, `Boolean` — each provides:
- `Parse` / `TryParse` (locale-safe via `std::from_chars`) ✅
- `ToString` (locale-safe via `std::to_chars`) ✅
- Type-specific helpers (`IsNaN`, `IsInfinity`, `IsPositive`, etc. for floats) ✅
- `MinValue` / `MaxValue` ✅

### Array (DONE)
| Method | Status |
|--------|--------|
| Sort, BinarySearch | ✅ |
| Copy, CopyTo | ✅ |
| Resize, Clear | ✅ |
| IndexOf, LastIndexOf | ✅ |
| Reverse | ✅ |

### Random (DONE)
| Method | Status |
|--------|--------|
| Next(), Next(max), Next(min, max) | ✅ |
| NextDouble, NextSingle | ✅ |
| NextBytes | ✅ |

### Environment (DONE)
| Method | Status |
|--------|--------|
| GetEnvironmentVariable, SetEnvironmentVariable | ✅ |
| GetCurrentDirectory (POSIX/Win32/Emscripten) | ✅ |
| ProcessorCount (POSIX/Win32/Emscripten) | ✅ |
| OSVersion, NewLine, Exit | ✅ |

### Console (DONE)
| Method | Status |
|--------|--------|
| WriteLine(string/int/double) | ✅ locale-safe |
| Write(string/int/float/double) | ✅ locale-safe |
| ReadLine | ✅ `std::getline(std::cin, ...)` |

### AppDomain / AppContext (DONE)
| Method | Status |
|--------|--------|
| BaseDirectory (Linux /proc/self/exe, macOS _NSGetExecutablePath, Win32, Emscripten ./) | ✅ |

### GC (STUB)
| Method | Status |
|--------|--------|
| Collect, GetTotalMemory, SuppressFinalize | ⚠️ no-op stubs |

### Lazy\<T\> (DONE)
`getValue()`, `IsValueCreated` — backed by `std::shared_ptr` ✅

### Tuple (DONE)
Constructors, `Item1`–`Item8` properties, `ToString` ✅

### Delegate types (DONE)
`Action`, `ActionT<T>`, `ActionT2<T1,T2>`, `ActionT3<T1,T2,T3>`,
`Func<R>`, `FuncT<T,R>`, `FuncT2<T1,T2,R>`, `FuncT3<T1,T2,T3,R>`,
`Predicate<T>`, `EventHandler`, `EventArgs` — all `std::function<>` aliases ✅

---

## System::Collections

### Non-Generic (DONE)
| Class | Status | Backing |
|-------|--------|---------|
| ArrayList | ✅ | `std::vector<std::any>`, full IList |
| Hashtable | ✅ | `std::unordered_map<string,any>`, IDictionary |
| Queue | ✅ | `std::queue` |
| Stack | ✅ | `std::stack` |
| BitArray | ✅ | bitset wrapper |

### Generic (DONE)
| Class | Status | Backing |
|-------|--------|---------|
| List\<T\> | ✅ | `std::vector<T>` |
| Dictionary\<K,V\> | ✅ | `std::map<K,V>` |
| HashSet\<T\> | ✅ | `std::unordered_set<T>` |
| Queue\<T\>, Stack\<T\> | ✅ | `std::queue`, `std::stack` |
| LinkedList\<T\> | ✅ | `std::list<T>` |
| SortedDictionary\<K,V\> | ✅ | `std::map<K,V>` |
| SortedList\<K,V\> | ✅ | `std::vector` + sorted insert |
| SortedSet\<T\> | ✅ | `std::set<T>` |
| PriorityQueue\<T\> | ✅ | `std::priority_queue<T>` |
| KeyedCollection\<K,T\> | ✅ | |

### Immutable (DONE)
All backed by `shared_ptr<const std::container<T>>`:
`ImmutableArray<T>`, `ImmutableList<T>`, `ImmutableDictionary<K,V>`,
`ImmutableHashSet<T>`, `ImmutableSortedDictionary<K,V>`,
`ImmutableSortedSet<T>`, `ImmutableQueue<T>`, `ImmutableStack<T>` ✅

### Concurrent (DONE)
`ConcurrentDictionary<K,V>`, `ConcurrentQueue<T>`, `ConcurrentStack<T>`,
`ConcurrentBag<T>`, `BlockingCollection<T>` ✅

### Specialized (DONE)
| Class | Status |
|-------|--------|
| StringCollection, StringDictionary | ✅ |
| OrderedDictionary | ✅ |
| BitVector32 | ✅ C++20 `std::popcount` |
| HybridDictionary, ListDictionary | ✅ |
| NameValueCollection | ✅ Get comma-joins all values; Get/GetValues by index; Add(collection) |

### Collection Interfaces (DONE)
`IEnumerable<T>`, `IEnumerator<T>`, `ICollection<T>`, `IList<T>`,
`IDictionary<K,V>`, `IComparer<T>`, `IEqualityComparer<T>`,
`IReadOnlyCollection<T>`, `IReadOnlyList<T>`, `IReadOnlyDictionary<K,V>`,
`ISet<T>`, `IReadOnlySet<T>` — all ✅

---

## System::IO

### File system (DONE)
| Class | Key methods |
|-------|-------------|
| File | ReadAllText, WriteAllText, ReadAllBytes, WriteAllBytes, ReadAllLines, WriteAllLines, Delete, Exists, Copy, Move |
| FileInfo | Name, FullName, Length, Exists, Delete, MoveTo, CopyTo |
| Directory | CreateDirectory, Delete, Exists, GetFiles, GetDirectories |
| DirectoryInfo | Name, FullName, Exists, GetFiles, GetDirectories, Parent, Delete |
| Path | Combine, GetFileName, GetExtension, GetDirectoryName, GetTempPath, GetTempFileName |
| DriveInfo | Name, TotalSize, AvailableFreeSpace, DriveType, IsReady, GetDrives |

### Streams (DONE)
| Class | Status |
|-------|--------|
| Stream (abstract) | ✅ Read, Write, Seek, Position, Length, Flush, Close |
| FileStream | ✅ FileMode / FileAccess / FileShare |
| MemoryStream | ✅ GetBuffer, ToArray, WriteTo |
| BufferedStream | ✅ wrapper |
| StreamReader | ✅ ReadLine, ReadToEnd, Peek |
| StreamWriter | ✅ WriteLine, Write, Flush, AutoFlush |
| StringReader / StringWriter | ✅ |
| BinaryReader / BinaryWriter | ✅ typed read/write for all primitives |

### Compression (DONE)
| Class | Status |
|-------|--------|
| GZipStream | ✅ zlib, PIMPL, 64 KB buffers |
| DeflateStream | ✅ raw DEFLATE (-MAX_WBITS), XNB-compatible |
| ZipArchive | ✅ miniz, Read+Create mode |

### Hashing (DONE)
`XxHash32`, `XxHash64` — cpp bodies ✅

### RandomAccess (DONE — POSIX-only documented)
`Read`, `Write` — POSIX pread/pwrite; Win32 OVERLAPPED ReadFile/WriteFile; Emscripten throws ✅

### IsolatedStorage (PARTIAL)
`IsolatedStorageFile`, `IsolatedStorageFileStream`, `IsolatedStorage` — basic ⚠️

### Enumerations (DONE)
`FileMode`, `FileAccess`, `FileShare`, `FileAttributes`, `SearchOption`,
`SeekOrigin`, `EnumerationOptions` ✅

---

## System::Text

### Encoding (DONE)
`Encoding`, `UTF8Encoding`, `ASCIIEncoding`, `UnicodeEncoding`,
`Latin1Encoding`, `UTF7Encoding` — `GetBytes` / `GetString` ✅

### Text utilities (DONE)
| Class | Status |
|-------|--------|
| StringBuilder | ✅ Append (int/long/double/string), Insert, Remove, Replace, ToString |
| Rune | ✅ GetUnicodeCategory, IsWhiteSpace, IsLetter |
| Ascii | ✅ IsValid, ToUpper, ToLower, Trim, EqualsIgnoreCase |

### Regular Expressions (PARTIAL)
| Class / Feature | Status |
|-----------------|--------|
| Regex — IsMatch, Match, Matches, Replace | ✅ backed by `std::regex` |
| Match — Value, Index, Length | ✅ |
| Groups (indexed) | ✅ |
| Named groups | ❌ not supported by `std::regex` |

### JSON (DONE)
| Class | Status |
|-------|--------|
| JsonDocument.Parse | ✅ nlohmann/json |
| JsonElement — ValueKind, GetString, GetInt32, GetDouble, EnumerateArray/Object | ✅ |
| JsonSerializer.Serialize / Deserialize | ✅ |
| JsonSerializerOptions | ✅ |

### Web encoding (DONE)
`HtmlEncoder`, `UrlEncoder`, `JavaScriptEncoder` ✅

---

## System::Threading

### Core threading (DONE)
| Class | Status |
|-------|--------|
| Thread | ✅ Start (deferred, once; 2nd throws), Join, IsAlive, ManagedThreadId |
| ThreadPool | ✅ QueueUserWorkItem detaches `std::thread`; GetMin/MaxThreads; Emscripten guard |

### Synchronization primitives (DONE)
`Monitor`, `Mutex`, `Semaphore`, `SemaphoreSlim`,
`AutoResetEvent`, `ManualResetEvent`, `ManualResetEventSlim`,
`EventWaitHandle`, `ReaderWriterLockSlim`,
`Barrier`, `CountdownEvent`, `SpinLock`, `SpinWait`, `Lock`, `WaitHandle` ✅

### Task-based asynchrony (PARTIAL)
| Class | Status |
|-------|--------|
| Task, Task\<T\> | ⚠️ `std::async(launch::async)`, no real threadpool; Emscripten throws |
| TaskCompletionSource\<T\> | ✅ |
| Parallel.For | ⚠️ stubs; Emscripten guard |

### Timers (DONE)
| Class | Status |
|-------|--------|
| Timer | ✅ `shared_ptr<State>` (no dangling-this) |
| PeriodicTimer | ✅ `WaitForNextTick` + `Dispose` implemented |

### Cancellation & async locals (DONE)
`CancellationToken`, `CancellationTokenSource`, `CancellationTokenRegistration`,
`AsyncLocal<T>` ✅

---

## System::Numerics

### Vectors & Matrices (DONE)
| Class | Key methods |
|-------|-------------|
| Vector2, Vector3, Vector4 | X/Y/Z/W, Length, Normalize, Dot, Cross, Lerp, Clamp, operators |
| Matrix3x2, Matrix4x4 | Multiply, Invert, Determinant, CreateRotation, LookAt, PerspectiveFov |
| Quaternion | Slerp, CreateFromAxisAngle, CreateFromYawPitchRoll, Conjugate, Inverse |
| Plane | CreateFromVertices, Dot, Normalize, Transform |

### Big numbers & complex (DONE)
| Class | Status |
|-------|--------|
| BigInteger | ✅ +/−/×/÷/%, TryParse, Knuth D division |
| Complex | ✅ Real, Imaginary, Add, Subtract, Multiply, Divide, Magnitude |

### Floating-point types (DONE)
`Half` (16-bit), `BFloat16` — ToString locale-safe ✅

### Color types (DONE)
`Argb`, `Rgba` ✅

### Utilities (DONE)
| Class | Status |
|-------|--------|
| BitOperations | ✅ C++20 `std::countl_zero`, `std::popcount` |
| MathF | ✅ single-precision math |
| GenericMathInterfaces | ✅ INumberBase, INumber, IBinaryInteger, etc. |

---

## System::Net

### Core types (DONE)
`IPAddress` (Parse, Loopback, Any), `IPEndPoint` (Address, Port),
`WebUtility` (HtmlDecode/Encode, UrlDecode/Encode) ✅

### HTTP (DONE)
| Class | Status |
|-------|--------|
| HttpClient | ✅ HTTP/1.1, POSIX+Winsock2, chunked+Content-Length, Emscripten throws |
| HttpMethod | ✅ Get, Post, Put, Delete, Patch |
| StringContent, ByteArrayContent | ✅ |
| FormUrlEncodedContent | ✅ |
| HttpRequestMessage, HttpResponseMessage | ✅ |
| HttpStatusCode | ✅ all standard codes |
| HTTPS / TLS | ❌ not implemented |

### Sockets (DONE — POSIX-only documented)
`TcpClient`, `TcpListener`, `UdpClient`, `NetworkStream`
— POSIX + Winsock2; Emscripten throws `PlatformNotSupportedException` ✅

---

## System::Globalization

### Culture & locale (DONE)
`CultureInfo`, `RegionInfo`, `CompareInfo`, `CharUnicodeInfo`,
`TextInfo`, `SortKey`, `SortVersion`, `StringInfo`, `TextElementEnumerator` ✅

### Formatting info (DONE)
`DateTimeFormatInfo` (MonthNames, DayNames, format patterns),
`NumberFormatInfo` (CurrencySymbol, DecimalSeparator, etc.) ✅

### Calendars (DONE — all 10 types)
`GregorianCalendar`, `JulianCalendar`, `HebrewCalendar`, `HijriCalendar`,
`JapaneseCalendar`, `KoreanCalendar`, `ThaiBuddhistCalendar`, `TaiwanCalendar`,
`PersianCalendar`, `UmAlQuraCalendar` ✅

### Time zones (DONE)
`TimeZone`, `TimeZoneInfo` — IANA mapping (~85 zones), Win32 `GetTimeZoneInformation`,
Emscripten returns UTC ✅

### Localization utilities (DONE)
`IdnMapping` — Punycode/IDNA RFC 3492, GetAscii/GetUnicode ✅
`ISOWeek` — GetWeekOfYear, GetYear ✅

### Enumerations (DONE)
`CalendarAlgorithmType`, `CalendarWeekRule`, `CompareOptions`, `CultureTypes`,
`DateTimeStyles`, `DigitShapes`, `GregorianCalendarTypes`, `NumberStyles`,
`TimeSpanStyles`, `UnicodeCategory` ✅

---

## System::Xml

### XML I/O (DONE)
| Class | Status |
|-------|--------|
| XmlReader | ✅ tinyxml2 DOM cursor; Read, GetAttribute, NodeType, Name, Value |
| XmlWriter | ✅ tinyxml2 DOM builder + XMLPrinter; WriteElement, WriteAttribute, Flush |

### LINQ to XML (DONE)
`XName`, `XAttribute`, `XElement`, `XDocument` ✅

---

## System::Diagnostics

### Debugging & tracing (DONE)
`Debugger` (Break, IsAttached), `Debug`, `Trace`,
`StackTrace`, `StackFrame`, `Stopwatch` ✅

### Attributes (DONE)
`ConditionalAttribute`, `DebuggerBrowsableAttribute`, `DebuggerHiddenAttribute`,
`DebuggerNonUserCodeAttribute`, `DebuggerStepperBoundaryAttribute`,
`DebuggerStepThroughAttribute`, `StackTraceHiddenAttribute`,
`DebuggerDisableUserUnhandledExceptionsAttribute` ✅

### Exceptions (DONE)
`UnreachableException` ✅

---

## System::ComponentModel

### Attributes (DONE)
`CategoryAttribute`, `DefaultValueAttribute`, `DescriptionAttribute`,
`EditorBrowsableAttribute`, `Attribute`, `ParamArrayAttribute`,
`NonSerializedAttribute`, `SerializableAttribute`, `ThreadStaticAttribute` ✅

### Observable (DONE)
`INotifyPropertyChanged`, `INotifyPropertyChanging`, `PropertyDescriptorCollection` ✅

### Data Annotations (DONE)
`Required`, `Range`, `StringLength`, `RegularExpression`, `EmailAddress`,
`MaxLength`, `MinLength` and others ✅

---

## System::Runtime

### Compiler services (DONE)
`CallerFilePathAttribute`, `CallerMemberNameAttribute`, `CallerLineNumberAttribute`,
`MethodImplAttribute`, `MethodImplOptions`, `AmbiguousImplementationException`,
`CompilerFeatureRequiredAttribute`, `RequiredMemberAttribute` ✅

### Interop (DONE)
`ExternalException`, `DllImportAttribute`, `StructLayoutAttribute` ✅

### Versioning (DONE)
Version attributes ✅

---

## System::Security

### Exceptions (DONE)
`SecurityException`, `CryptographicException`, `VerificationException` ✅

### Attributes (DONE)
`AllowPartiallyTrustedCallersAttribute` and others ✅

---

## System::Buffers

### Memory pools (DONE)
`ArrayPool<T>` — `Shared`, `Rent`, `Return` ✅

### Types (DONE)
`IMemoryOwner<T>`, `StandardFormat`, `OperationStatus` ✅

---

## Key gaps (summary)

| Gap | Impact | Notes |
|-----|--------|-------|
| `String` — Concat, Join, Replace, Trim, Substring, EndsWith, IsNullOrWhiteSpace | ~~Medium~~ | **Fixed session 50** ✅ |
| `NameValueCollection` — Get comma-join, Get/GetValues by index, Add(collection) | ~~Medium~~ | **Fixed session 51** ✅ |
| `Console.ReadLine` — was marked stub | ~~Low~~ | Already implemented ✅ (COVERAGE.md was wrong) |
| `PeriodicTimer` — was marked stub | ~~Low~~ | Already implemented ✅ (COVERAGE.md was wrong) |
| `ThreadPool` — was marked stub | ~~Low~~ | Already implemented ✅ (COVERAGE.md was wrong) |
| `Task` / `Parallel` — no real threadpool | Low | `std::async` sufficient for current use cases |
| `GC` — no-op | Low | not meaningful in C++ |
| `Regex` — no named groups | Low | `std::regex` limitation |
| `HttpClient` — no HTTPS/TLS | Medium | raw sockets only |

---

## Platform portability

| Platform | Status |
|----------|--------|
| Linux (POSIX) | ✅ Full support |
| macOS (POSIX) | ✅ Full support |
| Windows (Winsock2 / Win32 API) | ✅ mingw-w64 14 tested |
| Emscripten (WebAssembly) | ✅ Threading/networking throw `PlatformNotSupportedException` |

---

## Implementation quality notes

1. **Locale safety** — All float I/O uses `std::from_chars` / `std::to_chars`; never `strtod` / `to_string(float)`.
2. **Platform isolation** — All `#ifdef _WIN32` / `#elif __EMSCRIPTEN__` / `#else` guards confined to `.cpp` files; public headers are platform-agnostic.
3. **Memory safety** — Immutable collections use `shared_ptr<const std::container<T>>`; `Task` / `Timer` use `shared_ptr<State>` to prevent dangling-`this` UB.
4. **Doxygen** — All 449 public `.hpp` headers have `///` or `/** */` documentation.
5. **Build** — Zero errors, zero warnings on Linux, Windows (mingw-w64), and Emscripten.
