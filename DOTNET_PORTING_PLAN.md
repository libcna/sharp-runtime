# .NET Porting Plan for sharp-runtime

**sharp-runtime** is a C++ reimplementation of a .NET runtime subset, used primarily by:
- **CNA** — C++ reimplementation of XNA 4.0
- **mobile-eggbert** — C++ port of Windows Phone SpeedyBlupi game (uses sharp-runtime for `intcs`/`bytecs` types, `StringBuilder`, `TimeSpan`, `EventArgs`, `Random`, `Prop`)

.NET runtime source code used as reference: `dotnet/runtime` (MIT License).

---

## Legend

| Status | Meaning |
|--------|---------|
| ✅ DONE | Already implemented in sharp-runtime |
| 🔨 PORT | Should be ported — needed by CNA/mobile-eggbert or high value |
| 🧩 STUB | Create skeleton/placeholder — API compatibility but no real implementation |
| ❌ IGNORE | Not needed — too complex, platform-specific, or outside scope |

**Priority note:** CNA (XNA reimplementation) and mobile-eggbert drive priority.
Types already used by these projects are marked with 📌.

---

## License

sharp-runtime is partly based on the .NET runtime API design.
The .NET runtime is licensed under the **MIT License** (Copyright .NET Foundation and Contributors).

**Done:**
- [x] `LICENSE` file added (MIT, Copyright Robert Vokac and contributors, with .NET Foundation third-party notice)
- [x] Attribution section added to `README.md`
- [x] SPDX header + .NET attribution added to all `System/**/*.hpp` and `System/**/*.cpp` files
- [x] SPDX header added to all `SharpRuntime/**/*.hpp` and `SharpRuntime/**/*.cpp` files

---

## 1. System — Core Types

The foundation. These are what everything else depends on.
Already partially done; the gaps matter for CNA/mobile-eggbert compilation.

| Type | Status | Opinion |
|------|--------|---------|
| `object` (Object) | ✅ DONE | Base class for .NET hierarchy. Already in `System/Object.hpp`. |
| `string` (String) | ✅ DONE | Utility wrapper over `std::string`. Already in `System/String.hpp`. 📌 |
| Primitive typedefs (`intcs`, `longcs`, etc.) | ✅ DONE | In `SharpRuntime/SharpRuntimeHelper.hpp`. 📌 Essential for port accuracy. |
| `bool`, `char` native wrappers | ✅ DONE | Native C++ `bool`/`char` suffices. |
| `Int32` (boxed int) | ✅ DONE | `System/Int32.hpp` — mostly for `Int32::MaxValue` constants. |
| `Int64` (boxed long) | ✅ DONE | `System/Int64.hpp`. |
| `IntPtr` | ✅ DONE | `System/IntPtr.hpp`. |
| `UIntPtr` | 🧩 STUB | Mirror `IntPtr`; needed for completeness. Low priority. |
| `Nullable<T>` | ✅ DONE | `System/Nullable.hpp`. Map to `std::optional<T>`. |
| `Math` | ✅ DONE | `System/Math.hpp`. 📌 Essential, already used by CNA. |
| `Random` | ✅ DONE | `System/Random.hpp`. 📌 Used by mobile-eggbert `Decor`. |
| `DateTime` | ✅ DONE | `System/DateTime.hpp`. |
| `DateTimeOffset` | ✅ DONE | `System/DateTimeOffset.hpp`. 📌 Used by CNA sensor API. |
| `TimeSpan` | ✅ DONE | `System/TimeSpan.hpp`. 📌 Used by CNA and mobile-eggbert. |
| `TimeOnly` | 🧩 STUB | Needed only if XNA/mobile-eggbert uses it. Currently unused — low priority. |
| `DateOnly` | 🧩 STUB | Same as `TimeOnly`. |
| `Guid` | 🔨 PORT | XNA uses `Guid` (e.g. `GraphicsAdapter`). Simple to implement as 128-bit value + string formatting. |
| `Version` | 🧩 STUB | Rarely needed in game code; stub with major.minor parsing is enough. |
| `Uri` | ❌ IGNORE | Network/web type. Not needed for game engine core. |
| `Convert` | 🔨 PORT | `Convert.ToInt32()`, `Convert.ToString()` etc. used frequently in ported C# code. Simple to implement. |
| `BitConverter` | 🧩 STUB | Useful for binary data reading. CNA content pipeline may need it. |
| `Buffer` | 🧩 STUB | `Buffer.BlockCopy` — useful for raw memory operations in content loaders. |
| `Span<T>` / `ReadOnlySpan<T>` | 🧩 STUB | C++23 has `std::span`. Can typedef. Useful for slicing without copying. |
| `Memory<T>` | ❌ IGNORE | Too complex for the scope of sharp-runtime. Use `std::vector` slices. |
| `Array` (static methods) | 🔨 PORT | `Array::Sort`, `Array::Copy`, `Array::Resize` — frequently used in C# ports. Wrap `std::algorithm`. |
| `Tuple<>` / `ValueTuple<>` | 🧩 STUB | Map to `std::tuple`. Rarely needed in game logic. |
| `Type` | ✅ DONE | `System/Type.hpp`. Runtime type info is limited in C++; keep as partial stub. |
| `IComparable<T>` | ✅ DONE | Interface in `System/IComparable.hpp`. |
| `IEquatable<T>` | ✅ DONE | Interface in `System/IEquatable.hpp`. |
| `IDisposable` | ✅ DONE | `System/IDisposable.hpp`. 📌 Used by CNA audio/graphics. |
| `IServiceProvider` | ✅ DONE | `System/IServiceProvider.hpp`. |
| `IFormattable` | 🧩 STUB | Interface only; `ToString(format)` not critical. |
| `ICloneable` | 🧩 STUB | Interface only. Rarely needed. |
| `Action<>` | ✅ DONE | `System/Action.hpp`. Map to `std::function<void(...)>`. 📌 |
| `Func<>` | 🔨 PORT | Mirror `Action` — typedef `std::function<R(...)>`. Used in LINQ-style iteration in C# code. |
| `Predicate<T>` | 🔨 PORT | `std::function<bool(T)>` typedef. Used in `List::FindAll` etc. |
| `EventArgs` | ✅ DONE | `System/EventArgs.hpp`. 📌 Used everywhere in CNA. |
| `EventHandler<T>` | ✅ DONE | `System/EventHandler.hpp`. 📌 Used everywhere in CNA. |
| `Console` | 🔨 PORT | `Console::WriteLine`, `Console::Write` — basic debug output wrapper over `std::cout`. Simple and useful for ported code. |
| `Environment` | 🧩 STUB | `Environment::NewLine`, `Environment::OSVersion`. Simple constants; full process info is IGNORE. |
| `GC` | ❌ IGNORE | No GC in C++. RAII + smart pointers replace it. Don't port. |
| `AppDomain` | ❌ IGNORE | CLR concept. Not applicable. |
| `AppContext` | ❌ IGNORE | Not needed. |
| `Activator` | ❌ IGNORE | Runtime reflection-based factory. Not portable to C++. |
| `Delegate` | ❌ IGNORE | `std::function` covers all practical use cases. |

---

## 2. Exceptions

Already mostly done. A few important ones are missing.

| Type | Status | Opinion |
|------|--------|---------|
| `Exception` | ✅ DONE | `System/Exception.hpp`. 📌 |
| `SystemException` | ✅ DONE | `System/SystemException.hpp`. |
| `ArgumentException` | ✅ DONE | `System/ArgumentException.hpp`. |
| `ArgumentNullException` | 🔨 PORT | Missing! Common in C# code. Simple — inherits ArgumentException. |
| `ArgumentOutOfRangeException` | ✅ DONE | `System/ArgumentOutOfRangeException.hpp`. |
| `ArithmeticException` | ✅ DONE | `System/ArithmeticException.hpp`. |
| `DivideByZeroException` | 🔨 PORT | Common exception. Inherits ArithmeticException. |
| `OverflowException` | ✅ DONE | `System/OverflowException.hpp`. |
| `InvalidOperationException` | ✅ DONE | `System/InvalidOperationException.hpp`. |
| `NotImplementedException` | ✅ DONE | `System/NotImplementedException.hpp`. 📌 Heavily used during porting. |
| `NotSupportedException` | 🔨 PORT | Commonly thrown in partial implementations. Easy to add. |
| `NullReferenceException` | 🧩 STUB | In C++ usually segfault; can stub for porting completeness. |
| `IndexOutOfRangeException` | 🔨 PORT | Needed for `List`/`Array` bounds checking. |
| `InvalidCastException` | 🧩 STUB | Useful for type-safe casts. Low priority. |
| `FormatException` | 🔨 PORT | Needed for string parsing (`Int32::Parse` etc.). |
| `OutOfMemoryException` | 🧩 STUB | In C++ this is `std::bad_alloc`. Can stub as alias. |
| `ObjectDisposedException` | ✅ DONE | `System/ObjectDisposedException.hpp`. 📌 Used by CNA. |
| `UnauthorizedAccessException` | ✅ DONE | `System/UnauthorizedAccessException.hpp`. |
| `IOException` | 🔨 PORT | Needed as base for file-system exceptions. |
| `FileNotFoundException` | 🔨 PORT | Content pipeline needs this. |
| `DirectoryNotFoundException` | 🧩 STUB | Useful for file system operations. |
| `EndOfStreamException` | 🔨 PORT | Used by `BinaryReader` when reading past end. |
| `TimeoutException` | 🧩 STUB | Low priority for game engine. |
| `StackOverflowException` | ❌ IGNORE | Platform crash, can't meaningfully catch in C++. |

---

## 3. System.Collections (non-generic)

Non-generic collections are legacy .NET 1.x. Limited use in XNA/game code.
C# ported code may use them but modern code prefers Generic collections.

| Type | Status | Opinion |
|------|--------|---------|
| `IEnumerable` | ✅ DONE | `System/Collections/IEnumerable.hpp`. |
| `IEnumerator` | ✅ DONE | `System/Collections/IEnumerator.hpp`. |
| `ICollection` | 🧩 STUB | Interface only. |
| `IList` | 🧩 STUB | Interface only. |
| `IDictionary` | 🧩 STUB | Interface only. |
| `ArrayList` | ❌ IGNORE | Use `List<T>`. |
| `Hashtable` | ❌ IGNORE | Use `Dictionary<K,V>`. |
| `Queue` | 🧩 STUB | Wrap `std::queue`. Only if non-generic usage found. |
| `Stack` | 🧩 STUB | Wrap `std::stack`. Only if non-generic usage found. |
| `BitArray` | 🧩 STUB | Wrap `std::vector<bool>`. Rarely used in game code. |

---

## 4. System.Collections.Generic

Core collection types. Already partially done.

| Type | Status | Opinion |
|------|--------|---------|
| `List<T>` | ✅ DONE | `System/Collections/Generic/List.hpp`. 📌 Wraps `std::vector<T>`. |
| `Dictionary<K,V>` | ✅ DONE | `System/Collections/Generic/Dictionary.hpp`. 📌 Wraps `std::unordered_map`. |
| `IEnumerable<T>` | ✅ DONE | `System/Collections/Generic/IEnumerable.hpp`. |
| `IEnumerator<T>` | ✅ DONE | `System/Collections/Generic/IEnumerator.hpp`. |
| `ICollection<T>` | ✅ DONE | `System/Collections/Generic/ICollection.hpp`. |
| `IList<T>` | ✅ DONE | `System/Collections/Generic/IList.hpp`. |
| `IDictionary<K,V>` | 🔨 PORT | Interface. Needed for proper type hierarchy. |
| `IReadOnlyList<T>` | 🔨 PORT | Used in XNA public API (e.g. `EffectPassCollection`). |
| `IReadOnlyCollection<T>` | 🔨 PORT | Interface for read-only collections. |
| `IReadOnlyDictionary<K,V>` | 🧩 STUB | Low priority. |
| `HashSet<T>` | 🔨 PORT | Used in some XNA/game logic. Wrap `std::unordered_set`. Useful. |
| `SortedDictionary<K,V>` | 🧩 STUB | Wrap `std::map`. Low priority. |
| `SortedList<K,V>` | 🧩 STUB | Rarely used in game code. |
| `Queue<T>` | 🔨 PORT | Useful for event queues in game engines. Wrap `std::queue`. |
| `Stack<T>` | 🔨 PORT | Useful for undo/parse stacks. Wrap `std::stack`. |
| `LinkedList<T>` | 🧩 STUB | Rarely used directly. |
| `KeyValuePair<K,V>` | 🔨 PORT | Used in Dictionary iteration (`foreach (var kvp in dict)`). |
| `Comparer<T>` | 🧩 STUB | Used for sort customization. Low priority. |
| `EqualityComparer<T>` | 🧩 STUB | Used for dictionary customization. |

---

## 5. System.Collections.ObjectModel

| Type | Status | Opinion |
|------|--------|---------|
| `Collection<T>` | ✅ DONE | `System/Collections/ObjectModel/Collection.hpp`. |
| `ReadOnlyCollection<T>` | ✅ DONE | `System/Collections/ObjectModel/ReadOnlyCollection.hpp`. |
| `ObservableCollection<T>` | 🧩 STUB | Used in WPF/WinPhone MVVM pattern. Mobile-eggbert might need it. Stub with change notification. |
| `KeyedCollection<K,T>` | 🧩 STUB | Rarely needed in game code. |

---

## 6. System.Text

| Type | Status | Opinion |
|------|--------|---------|
| `StringBuilder` | ✅ DONE | `System/Text/StringBuilder.hpp`. 📌 Used by mobile-eggbert `Worlds`. |
| `Encoding` | ✅ DONE | `System/Text/Encoding.hpp`. Base class for encodings. |
| `UTF8Encoding` | 🔨 PORT | Most important encoding. Needed for file/network I/O in game content. |
| `ASCIIEncoding` | 🔨 PORT | Simple encoding, useful for legacy content formats. |
| `UnicodeEncoding` | 🧩 STUB | UTF-16. Less common in modern game content. |
| `Decoder` | 🧩 STUB | Advanced encoding; low priority. |
| `Encoder` | 🧩 STUB | Advanced encoding; low priority. |

---

## 7. System.Text.RegularExpressions

| Type | Status | Opinion |
|------|--------|---------|
| `Regex` | 🧩 STUB | Wrap `std::regex`. Not common in XNA/game engine code. Low priority. |
| `Match` | 🧩 STUB | Goes with `Regex`. |
| `MatchCollection` | 🧩 STUB | Goes with `Regex`. |

---

## 8. System.IO

File I/O — already partially done. Important for content loading and save games.

| Type | Status | Opinion |
|------|--------|---------|
| `Stream` | ✅ DONE | `System/IO/Stream.hpp`. 📌 Base stream class. |
| `StreamReader` | ✅ DONE | `System/IO/StreamReader.hpp`. |
| `StreamWriter` | 🔨 PORT | Counterpart to `StreamReader`. Needed for save file writing. |
| `BinaryReader` | ✅ DONE | `System/IO/BinaryReader.hpp`. 📌 Critical for XNA content pipeline. |
| `BinaryWriter` | 🔨 PORT | Counterpart to `BinaryReader`. Needed for binary save files. |
| `MemoryStream` | ✅ DONE | `System/IO/MemoryStream.hpp`. |
| `FileStream` | ✅ DONE | `System/IO/FileStream.hpp`. |
| `BufferedStream` | 🧩 STUB | Optimization wrapper. Low priority. |
| `TextReader` | 🧩 STUB | Abstract base for `StreamReader`. |
| `TextWriter` | 🧩 STUB | Abstract base for `StreamWriter`. |
| `StringReader` | 🧩 STUB | Read from string as stream. Occasionally useful. |
| `StringWriter` | 🧩 STUB | Write to string as stream. |
| `File` | 🔨 PORT | `File::ReadAllText`, `File::WriteAllText`, `File::Exists`, `File::Delete`. Very useful for save/config. Wrap C++ file APIs. |
| `Directory` | 🔨 PORT | `Directory::Exists`, `Directory::CreateDirectory`. Needed for save game paths. |
| `Path` | 🔨 PORT | `Path::Combine`, `Path::GetFileName`, `Path::GetExtension`. Very commonly used. |
| `FileInfo` | 🧩 STUB | OOP wrapper over file metadata. Lower priority than static `File`. |
| `DirectoryInfo` | 🧩 STUB | OOP wrapper over directory. Lower priority than static `Directory`. |
| `FileMode` | ✅ DONE | `System/IO/FileMode.hpp`. Enum. |
| `FileAccess` | 🔨 PORT | Enum used with `FileStream`. |
| `FileShare` | 🔨 PORT | Enum used with `FileStream`. |
| `SeekOrigin` | 🔨 PORT | Enum for `Stream::Seek`. |
| `IOException` | 🔨 PORT | Base exception for I/O errors. |
| `FileNotFoundException` | 🔨 PORT | Inherits `IOException`. |
| `DirectoryNotFoundException` | 🧩 STUB | Inherits `IOException`. |
| `EndOfStreamException` | 🔨 PORT | Used by `BinaryReader`. |
| `InvalidDataException` | 🧩 STUB | Useful for malformed content. |

---

## 9. System.IO.IsolatedStorage

Used by mobile-eggbert for save game storage on Windows Phone.

| Type | Status | Opinion |
|------|--------|---------|
| `IsolatedStorageFile` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageFile.hpp`. 📌 |
| `IsolatedStorageFileStream` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp`. 📌 |
| `IsolatedStorageException` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageException.hpp`. 📌 |
| `IsolatedStorage` (abstract) | 🧩 STUB | Abstract base. Interface only. |
| `IsolatedStorageScope` | 🧩 STUB | Enum for scope (User/Assembly). |

---

## 10. System.IO.Compression

| Type | Status | Opinion |
|------|--------|---------|
| `GZipStream` | 🧩 STUB | Useful for compressed XNB content. Wrap zlib or miniz. |
| `DeflateStream` | 🧩 STUB | Same as above — XNB format uses DEFLATE. Consider porting if XNB loading is needed. |
| `ZipArchive` | 🧩 STUB | Lower priority. Wrap libzip or miniz. |
| `ZipFile` | ❌ IGNORE | Too complex for game engine core. |

---

## 11. System.Threading

Threading support. C++ has excellent STL threading; these are shims for ported code.

| Type | Status | Opinion |
|------|--------|---------|
| `Thread` | 🧩 STUB | Wrap `std::thread`. Only basic Start/Join needed for ported code. |
| `Monitor` | 🧩 STUB | Wrap `std::mutex`. `Monitor::Enter`/`Exit` pattern used in C#. |
| `Mutex` | 🧩 STUB | Wrap `std::mutex`. |
| `Semaphore` / `SemaphoreSlim` | 🧩 STUB | Wrap `std::counting_semaphore` (C++20). Low priority. |
| `ManualResetEvent` | 🧩 STUB | Wrap `std::condition_variable`. |
| `AutoResetEvent` | 🧩 STUB | Wrap `std::condition_variable`. |
| `Interlocked` | 🧩 STUB | Wrap `std::atomic` operations. |
| `Timer` | 🧩 STUB | Periodic callback. Game engines usually use their own loop. Low priority. |
| `ThreadPool` | ❌ IGNORE | Complex to port correctly. Use `std::async` directly. |
| `CancellationToken` | 🧩 STUB | Useful for async-style cancellation. Can use `std::atomic<bool>`. |
| `CancellationTokenSource` | 🧩 STUB | Goes with `CancellationToken`. |
| `volatile` keyword | ❌ IGNORE | Language feature; C++ has `std::atomic`. |

---

## 12. System.Threading.Tasks

| Type | Status | Opinion |
|------|--------|---------|
| `Task` / `Task<T>` | ❌ IGNORE | Requires full async runtime. C++ `std::future` covers practical needs directly. Too complex to port faithfully. |
| `TaskCompletionSource<T>` | ❌ IGNORE | Same reasoning. |
| `ValueTask` | ❌ IGNORE | Same reasoning. |
| `async`/`await` | ❌ IGNORE | Language feature, cannot be ported to C++. Use callbacks or coroutines. |

---

## 13. System.Linq

| Type | Status | Opinion |
|------|--------|---------|
| `Enumerable` (extension methods) | ❌ IGNORE | LINQ is a language + runtime feature. C++23 ranges (`std::ranges`) cover the same functionality idiomatically. Do NOT try to replicate the full LINQ API — it would create confusion. |
| `IQueryable<T>` | ❌ IGNORE | ORM/database query abstraction. Irrelevant here. |

---

## 14. System.Numerics

**Note:** XNA/CNA already defines `Vector2`, `Vector3`, `Matrix`, `Quaternion` etc. Do NOT duplicate them in sharp-runtime.

| Type | Status | Opinion |
|------|--------|---------|
| `Vector2/3/4` | ❌ IGNORE | Defined in CNA (`Microsoft::Xna::Framework`). |
| `Matrix4x4` | ❌ IGNORE | Defined in CNA as `Matrix`. |
| `Quaternion` | ❌ IGNORE | Defined in CNA. |
| `Complex` | 🧩 STUB | Not needed for game engine. Could be useful for DSP/audio. Low priority. |
| `BigInteger` | 🧩 STUB | Not needed for game engine. |
| `Half` (float16) | 🧩 STUB | Could be useful for GPU texture data. |

---

## 15. System.Diagnostics

| Type | Status | Opinion |
|------|--------|---------|
| `Debug` | 🔨 PORT | `Debug::Assert`, `Debug::WriteLine` — essential during development. Simple wrapper. |
| `Trace` | 🧩 STUB | Lower priority than `Debug`. |
| `Stopwatch` | 🔨 PORT | Used for performance measurement in games. Wrap `std::chrono::high_resolution_clock`. Very useful. |
| `Process` | ❌ IGNORE | Platform-specific, not needed for game engine. |
| `PerformanceCounter` | ❌ IGNORE | Windows-specific performance monitoring. Not portable. |
| `StackTrace` | ❌ IGNORE | Too platform-specific. |

---

## 16. System.Reflection

| Type | Status | Opinion |
|------|--------|---------|
| All Reflection types | ❌ IGNORE | C++ has no runtime reflection. Compile-time templates cover what XNA/game code needs. Do not port. |

---

## 17. System.Globalization

| Type | Status | Opinion |
|------|--------|---------|
| `CultureInfo` | 🧩 STUB | `CultureInfo::InvariantCulture` constant is the most important part. Used for locale-independent number formatting. |
| `NumberFormatInfo` | 🧩 STUB | Number formatting. Low priority. |
| `StringInfo` | ❌ IGNORE | Unicode text segmentation. Not needed for game code. |
| `Calendar` | ❌ IGNORE | Not needed. |

---

## 18. System.Resources

| Type | Status | Opinion |
|------|--------|---------|
| `ResourceManager` | ❌ IGNORE | .NET assembly-based resources. XNA uses its own Content system. |

---

## 19. System.ComponentModel

Already partially done.

| Type | Status | Opinion |
|------|--------|---------|
| `Attribute` (base) | ✅ DONE | `System/ComponentModel/Attribute.hpp`. |
| `PropertyDescriptorCollection` | ✅ DONE | `System/ComponentModel/PropertyDescriptorCollection.hpp`. |
| `INotifyPropertyChanged` | 🧩 STUB | Used in MVVM pattern on Windows Phone. Mobile-eggbert may need it. Stub with event. |
| `TypeConverter` | ❌ IGNORE | Reflection-based type conversion. Not needed. |
| `DescriptionAttribute` | 🧩 STUB | Metadata only. Low priority. |
| `DefaultValueAttribute` | 🧩 STUB | Metadata only. Low priority. |

---

## 20. System.Runtime.CompilerServices

| Type | Status | Opinion |
|------|--------|---------|
| `CallerMemberName` / `CallerFilePath` | ❌ IGNORE | Language-level attributes. C++ has `__FUNCTION__` / `__FILE__`. |
| `MethodImpl` / `MethodImplOptions` | ❌ IGNORE | JIT hints. Not applicable in C++. |
| `RuntimeHelpers` | ❌ IGNORE | CLR internals. |

---

## 21. System.Runtime.InteropServices

| Type | Status | Opinion |
|------|--------|---------|
| `Marshal` | ❌ IGNORE | COM interop / P/Invoke. Not applicable. |
| `DllImport` | ❌ IGNORE | P/Invoke attribute. C++ has native DLL imports. |
| `GCHandle` | ❌ IGNORE | No GC in C++. |

---

## 22. System.Security.Cryptography

| Type | Status | Opinion |
|------|--------|---------|
| All crypto types | ❌ IGNORE | Not needed for game engine. Use OpenSSL or platform crypto if needed in a higher-level library. |

---

## 23. System.Net

| Type | Status | Opinion |
|------|--------|---------|
| `HttpClient` | ❌ IGNORE | Not needed for game engine core. |
| `WebClient` | ❌ IGNORE | Deprecated even in .NET. |
| `Sockets (TcpClient, UdpClient)` | 🧩 STUB | Listed in `TODO.md`. Possibly needed for multiplayer. Low priority now but on roadmap. |
| `IPAddress`, `IPEndPoint` | 🧩 STUB | Supporting types for sockets. |

---

## 24. System.Xml / System.Xml.Linq

| Type | Status | Opinion |
|------|--------|---------|
| `XmlReader` / `XmlWriter` | 🧩 STUB | XNA content pipeline uses XML for XNB descriptors. Wrap tinyxml2 or pugixml. |
| `XDocument` / `XElement` (LINQ to XML) | 🧩 STUB | Easier API over raw XML. Lower priority than `XmlReader`. |
| `XmlSerializer` | ❌ IGNORE | Reflection-based. Too complex. |

---

## 25. System.Text.Json

| Type | Status | Opinion |
|------|--------|---------|
| `JsonSerializer` | ❌ IGNORE | Use nlohmann/json or rapidjson directly in C++. |
| `JsonDocument` / `JsonElement` | 🧩 STUB | If save file format uses JSON. Low priority. |

---

## 26. System.Console

| Type | Status | Opinion |
|------|--------|---------|
| `Console` | 🔨 PORT | `Console::WriteLine`, `Console::Write`, `Console::ReadLine`. Simple wrapper over stdout/stdin. Useful for ported code. |

---

## 27. System.Environment

| Type | Status | Opinion |
|------|--------|---------|
| `Environment` | 🧩 STUB | `Environment::NewLine` (platform-correct `\n`/`\r\n`), `Environment::CurrentDirectory`. Constants only. |

---

## 28. Microsoft.Extensions.*

| Namespace | Status | Opinion |
|-----------|--------|---------|
| `Microsoft.Extensions.DependencyInjection` | ❌ IGNORE | ASP.NET infrastructure. Not needed. |
| `Microsoft.Extensions.Logging` | ❌ IGNORE | Use CNA Logger. |
| `Microsoft.Extensions.Configuration` | ❌ IGNORE | Not needed for game engine. |

---

## 29. SharpRuntime-specific (Non-.NET types)

Types unique to sharp-runtime with no .NET equivalent.

| Type | Status | Opinion |
|------|--------|---------|
| `SharpRuntimeHelper` (type aliases) | ✅ DONE | `intcs`, `longcs`, `bytecs` etc. — keep and extend as needed. 📌 |
| `Prop<T>` / `Property<T>` | ✅ DONE | C# property emulation. 📌 Used by mobile-eggbert `Slider`, `Decor`. |
| `ReadonlyProperty<T>` | ✅ DONE | Read-only property. |
| `StoragePaths` | ✅ DONE | Platform-specific save paths. Used by `IsolatedStorageFile`. 📌 |

---

## Priority Summary

### Must port (blocking CNA/mobile-eggbert)
- `ArgumentNullException` (very commonly thrown in C# code)
- `FormatException` (needed for parsing methods)
- `IndexOutOfRangeException` (bounds checking in List/Array)
- `NotSupportedException` (partial implementations)
- `IOException` + `FileNotFoundException` + `EndOfStreamException`
- `File`, `Directory`, `Path` (static helpers)
- `SeekOrigin`, `FileAccess`, `FileShare` (Stream API completeness)
- `StreamWriter`, `BinaryWriter` (write-side of streams)
- `IDictionary<K,V>`, `IReadOnlyList<T>`, `IReadOnlyCollection<T>`
- `KeyValuePair<K,V>`
- `Queue<T>`, `Stack<T>`, `HashSet<T>`
- `Convert` (type conversion)
- `Guid` (XNA uses it in public API)
- `Debug::Assert` + `Stopwatch`
- `Console::WriteLine`
- `UTF8Encoding`, `ASCIIEncoding`
- `Func<>`, `Predicate<T>`
- `Array` static methods
- MIT License file + attribution headers

### Nice to have (adds completeness)
- `TimeOnly`, `DateOnly`
- `Version`
- `ObservableCollection<T>`
- `CultureInfo::InvariantCulture`
- `GZipStream` / `DeflateStream` (XNB uses DEFLATE)
- `XmlReader` (XNA content descriptors)
- `Regex` (string processing in content)
- `INotifyPropertyChanged`
- `UIntPtr`
- `Thread`, `Monitor`, `CancellationToken`

### Explicitly out of scope
- Full CLR / GC / JIT
- Reflection
- async/await / Task
- LINQ (use C++ ranges/algorithms instead)
- WPF / WinForms / ASP.NET
- Entity Framework
- Cryptography
- Network HTTP (HttpClient, WebClient)
- Microsoft.Extensions.*
- System.Numerics (vectors/matrix — CNA owns these)

---

## Notes on XNA/CNA Public API Types Needed from sharp-runtime

Based on analysis of CNA includes:
- `System::Exception` — base for `CNA::CNAException`
- `System::IDisposable` — implemented by most XNA resource types (Texture2D, SoundEffect, etc.)
- `System::EventArgs` + `EventHandler<T>` — game events (Activated, Deactivated, Exiting)
- `System::TimeSpan` — used in audio (Cue duration, Microphone buffer)
- `System::DateTimeOffset` — sensor readings timestamp
- `System::Object` — base class for some XNA types
- `System::ObjectDisposedException` — thrown when using disposed resources
- `System::Collections::Generic::List<T>` — collections throughout XNA
- `System::Collections::ObjectModel::ReadOnlyCollection<T>` — `EffectPassCollection` etc.

## Notes on mobile-eggbert Usage

mobile-eggbert (C++ port of Windows Phone SpeedyBlupi) uses:
- `SharpRuntime::intcs`, `bytecs`, `ubytecs` — type aliases everywhere
- `SharpRuntime::Prop<T>` — property accessors in `Slider`, `Decor`
- `System::Random` — in `Decor.hpp`
- `System::TimeSpan` — in `Game1.hpp` (game timing)
- `System::EventArgs` — in `Game1.hpp` / `IGame1.hpp`
- `System::Text::StringBuilder` — in `Worlds.hpp` (world name building)
- `System::IO::IsolatedStorage::*` — save game storage (Windows Phone compat)

The C# legacy code (`mobile-eggbert-legacy`) additionally uses:
- `System.Collections.Generic` (List, Dictionary)
- `System.IO` (File, Stream, IsolatedStorage)
- `System.Text` (StringBuilder)
- `System.Linq` (→ IGNORE in C++ port, use STL algorithms)
- `System.Resources` (→ IGNORE, handled differently in C++)
- `System.Globalization` (→ STUB CultureInfo.InvariantCulture only)
- `System.Diagnostics` (→ PORT Debug.Assert / Stopwatch)
