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
| `Guid` | ✅ DONE | `System/Guid.hpp` + `.cpp`. RFC 4122 v4, `NewGuid()`, `ToString()`. |
| `Version` | 🧩 STUB | Rarely needed in game code; stub with major.minor parsing is enough. |
| `Uri` | ❌ IGNORE | Network/web type. Not needed for game engine core. |
| `Convert` | ✅ DONE | `System/Convert.hpp` + `.cpp`. ToInt32/64/16, ToDouble/Single, ToString, base-N. |
| `BitConverter` | 🧩 STUB | Useful for binary data reading. CNA content pipeline may need it. |
| `Buffer` | 🧩 STUB | `Buffer.BlockCopy` — useful for raw memory operations in content loaders. |
| `Span<T>` / `ReadOnlySpan<T>` | 🧩 STUB | C++23 has `std::span`. Can typedef. Useful for slicing without copying. |
| `Memory<T>` | ❌ IGNORE | Too complex for the scope of sharp-runtime. Use `std::vector` slices. |
| `Array` (static methods) | ✅ DONE | `System/Array.hpp`. Sort/Copy/Resize/IndexOf/Reverse/Clear — template helpers over `std::vector`. |
| `Tuple<>` / `ValueTuple<>` | 🧩 STUB | Map to `std::tuple`. Rarely needed in game logic. |
| `Type` | ✅ DONE | `System/Type.hpp`. Runtime type info is limited in C++; keep as partial stub. |
| `IComparable<T>` | ✅ DONE | Interface in `System/IComparable.hpp`. |
| `IEquatable<T>` | ✅ DONE | Interface in `System/IEquatable.hpp`. |
| `IDisposable` | ✅ DONE | `System/IDisposable.hpp`. 📌 Used by CNA audio/graphics. |
| `IServiceProvider` | ✅ DONE | `System/IServiceProvider.hpp`. |
| `IFormattable` | 🧩 STUB | Interface only; `ToString(format)` not critical. |
| `ICloneable` | 🧩 STUB | Interface only. Rarely needed. |
| `Action<>` | ✅ DONE | `System/Action.hpp`. Map to `std::function<void(...)>`. 📌 |
| `Func<>` | ✅ DONE | `System/Func.hpp`. `Func<R>`, `FuncT<T,R>`, `FuncT2`, `FuncT3` via `std::function`. |
| `Predicate<T>` | ✅ DONE | `System/Predicate.hpp`. Typedef `std::function<bool(T)>`. |
| `EventArgs` | ✅ DONE | `System/EventArgs.hpp`. 📌 Used everywhere in CNA. |
| `EventHandler<T>` | ✅ DONE | `System/EventHandler.hpp`. 📌 Used everywhere in CNA. |
| `Console` | ✅ DONE | `System/Console.hpp`. Write/WriteLine/ReadLine/Error, header-only. |
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
| `ArgumentNullException` | ✅ DONE | `System/ArgumentNullException.hpp`. Inherits `ArgumentException`. |
| `ArgumentOutOfRangeException` | ✅ DONE | `System/ArgumentOutOfRangeException.hpp`. |
| `ArithmeticException` | ✅ DONE | `System/ArithmeticException.hpp`. |
| `DivideByZeroException` | ✅ DONE | `System/DivideByZeroException.hpp`. Inherits `ArithmeticException`. |
| `OverflowException` | ✅ DONE | `System/OverflowException.hpp`. |
| `InvalidOperationException` | ✅ DONE | `System/InvalidOperationException.hpp`. |
| `NotImplementedException` | ✅ DONE | `System/NotImplementedException.hpp`. 📌 Heavily used during porting. |
| `NotSupportedException` | ✅ DONE | `System/NotSupportedException.hpp`. Default thrown by `Stream::Write`. |
| `NullReferenceException` | 🧩 STUB | In C++ usually segfault; can stub for porting completeness. |
| `IndexOutOfRangeException` | ✅ DONE | `System/IndexOutOfRangeException.hpp`. |
| `InvalidCastException` | 🧩 STUB | Useful for type-safe casts. Low priority. |
| `FormatException` | ✅ DONE | `System/FormatException.hpp`. Thrown by `Convert` on bad input. |
| `OutOfMemoryException` | 🧩 STUB | In C++ this is `std::bad_alloc`. Can stub as alias. |
| `ObjectDisposedException` | ✅ DONE | `System/ObjectDisposedException.hpp`. 📌 Used by CNA. |
| `UnauthorizedAccessException` | ✅ DONE | `System/UnauthorizedAccessException.hpp`. |
| `IOException` | ✅ DONE | `System/IO/IOException.hpp`. Base for all I/O exceptions. |
| `FileNotFoundException` | ✅ DONE | `System/IO/FileNotFoundException.hpp`. Has `FileName` property. |
| `DirectoryNotFoundException` | 🧩 STUB | Useful for file system operations. |
| `EndOfStreamException` | ✅ DONE | `System/IO/EndOfStreamException.hpp`. |
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
| `IDictionary<K,V>` | ✅ DONE | `System/Collections/Generic/IDictionary.hpp`. Interface. |
| `IReadOnlyList<T>` | ✅ DONE | `System/Collections/Generic/IReadOnlyList.hpp`. |
| `IReadOnlyCollection<T>` | ✅ DONE | `System/Collections/Generic/IReadOnlyCollection.hpp`. |
| `IReadOnlyDictionary<K,V>` | 🧩 STUB | Low priority. |
| `HashSet<T>` | ✅ DONE | `System/Collections/Generic/HashSet.hpp`. Wraps `std::unordered_set`. Add/Remove/Contains/UnionWith/IntersectWith. |
| `SortedDictionary<K,V>` | 🧩 STUB | Wrap `std::map`. Low priority. |
| `SortedList<K,V>` | 🧩 STUB | Rarely used in game code. |
| `Queue<T>` | ✅ DONE | `System/Collections/Generic/Queue.hpp`. Enqueue/Dequeue/Peek/Contains/Clear. |
| `Stack<T>` | ✅ DONE | `System/Collections/Generic/Stack.hpp`. Push/Pop/Peek/Contains/Clear. |
| `LinkedList<T>` | 🧩 STUB | Rarely used directly. |
| `KeyValuePair<K,V>` | ✅ DONE | `System/Collections/Generic/KeyValuePair.hpp`. Struct with Key+Value. |
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
| `UTF8Encoding` | ✅ DONE | `System/Text/UTF8Encoding.hpp`. Inherits polymorphic `Encoding`. |
| `ASCIIEncoding` | ✅ DONE | `System/Text/ASCIIEncoding.hpp`. Non-ASCII chars replaced with `?`. |
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
| `Stream` | ✅ DONE | `System/IO/Stream.hpp`. 📌 Polymorphic base; now has virtual `Write`, `WriteByte`, `Flush`, `CanWrite`. |
| `StreamReader` | ✅ DONE | `System/IO/StreamReader.hpp`. |
| `StreamWriter` | ✅ DONE | `System/IO/StreamWriter.hpp`. Write/WriteLine over any `Stream` or file path. |
| `BinaryReader` | ✅ DONE | `System/IO/BinaryReader.hpp`. 📌 Critical for XNA content pipeline. |
| `BinaryWriter` | ✅ DONE | `System/IO/BinaryWriter.hpp`. Little-endian; 7-bit string encoding matches `BinaryReader`. |
| `MemoryStream` | ✅ DONE | `System/IO/MemoryStream.hpp`. Now has empty writable constructor + `Write`/`WriteByte`. |
| `FileStream` | ✅ DONE | `System/IO/FileStream.hpp`. Refactored to `std::fstream`; full read+write, all `FileMode` values. |
| `BufferedStream` | 🧩 STUB | Optimization wrapper. Low priority. |
| `TextReader` | 🧩 STUB | Abstract base for `StreamReader`. |
| `TextWriter` | 🧩 STUB | Abstract base for `StreamWriter`. |
| `StringReader` | 🧩 STUB | Read from string as stream. Occasionally useful. |
| `StringWriter` | 🧩 STUB | Write to string as stream. |
| `File` | ✅ DONE | `System/IO/File.hpp`. ReadAllText/WriteAllText/ReadAllBytes/WriteAllBytes/Exists/Delete/Copy/Move/AppendAllText. |
| `Directory` | ✅ DONE | `System/IO/Directory.hpp`. Exists/CreateDirectory/Delete/Move/GetFiles/GetDirectories. |
| `Path` | ✅ DONE | `System/IO/Path.hpp`. Combine/GetFileName/GetExtension/GetDirectoryName/GetTempPath/ChangeExtension etc. |
| `FileInfo` | 🧩 STUB | OOP wrapper over file metadata. Lower priority than static `File`. |
| `DirectoryInfo` | 🧩 STUB | OOP wrapper over directory. Lower priority than static `Directory`. |
| `FileMode` | ✅ DONE | `System/IO/FileMode.hpp`. Now includes all values: CreateNew/Create/Open/OpenOrCreate/Truncate/Append. |
| `FileAccess` | ✅ DONE | `System/IO/FileAccess.hpp`. Read/Write/ReadWrite. |
| `FileShare` | ✅ DONE | `System/IO/FileShare.hpp`. None/Read/Write/ReadWrite/Delete/Inheritable. |
| `SeekOrigin` | ✅ DONE | `System/IO/SeekOrigin.hpp`. Begin/Current/End. |
| `IOException` | ✅ DONE | `System/IO/IOException.hpp`. Base for all I/O exceptions. |
| `FileNotFoundException` | ✅ DONE | `System/IO/FileNotFoundException.hpp`. Has `FileName` property. |
| `DirectoryNotFoundException` | 🧩 STUB | Inherits `IOException`. |
| `EndOfStreamException` | ✅ DONE | `System/IO/EndOfStreamException.hpp`. |
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
| `Debug` | ✅ DONE | `System/Diagnostics/Debug.hpp`. Assert/Write/WriteLine/Fail — stripped in NDEBUG, header-only. |
| `Trace` | 🧩 STUB | Lower priority than `Debug`. |
| `Stopwatch` | ✅ DONE | `System/Diagnostics/Stopwatch.hpp`. Start/Stop/Reset/Restart/ElapsedMilliseconds/Elapsed(TimeSpan), header-only. |
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
| `Console` | ✅ DONE | `System/Console.hpp`. Write/WriteLine/ReadLine/Error_Write/Error_WriteLine, header-only. |

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

### ✅ Already ported (first wave — complete)
- MIT License + SPDX headers
- `ArgumentNullException`, `NotSupportedException`, `DivideByZeroException`
- `IndexOutOfRangeException`, `FormatException`
- `IOException`, `FileNotFoundException`, `EndOfStreamException`
- `SeekOrigin`, `FileAccess`, `FileShare`, `FileMode` (extended)
- `StreamWriter`, `BinaryWriter`
- `Stream` (Write/WriteByte/Flush/CanWrite), `MemoryStream` (writable), `FileStream` (read+write)
- `File`, `Directory`, `Path`
- `IDictionary<K,V>`, `IReadOnlyList<T>`, `IReadOnlyCollection<T>`, `KeyValuePair<K,V>`
- `Queue<T>`, `Stack<T>`, `HashSet<T>`
- `Convert`, `Guid`, `Array` (static helpers)
- `Func<>` / `FuncT` / `FuncT2` / `FuncT3`, `Predicate<T>`
- `Console`, `Debug`, `Stopwatch`
- `UTF8Encoding`, `ASCIIEncoding`

### 🔨 Next to port (nice to have)
- `TimeOnly`, `DateOnly`
- `Version`
- `ObservableCollection<T>`
- `CultureInfo` (`InvariantCulture` constant)
- `GZipStream` / `DeflateStream` (XNB uses DEFLATE)
- `XmlReader` / `XmlWriter` (XNA content descriptors)
- `Regex` + `Match` (string processing in content)
- `INotifyPropertyChanged`
- `UIntPtr`
- `Thread`, `Monitor`, `CancellationToken`
- `BitConverter`
- `DirectoryNotFoundException`, `InvalidDataException`

### ❌ Explicitly out of scope
- Full CLR / GC / JIT
- Reflection
- async/await / Task
- LINQ (use C++ ranges/algorithms instead)
- WPF / WinForms / ASP.NET
- Entity Framework
- Cryptography
- Network HTTP (HttpClient, WebClient)
- Microsoft.Extensions.*
- System.Numerics vectors/matrix (CNA owns these)

---

---

## 30. .NET Namespace Overview

Complete view of .NET namespaces from `dotnet/runtime`.
**Rozhodnutí je na tobě** — navrhnu, ty rozhodneš. Použij `✅ PORT` / `🧩 STUB` / `❌ IGNORE`.

| Namespace | Můj návrh | Zdůvodnění |
|-----------|-----------|------------|
| `System` (core) | ✅ PORT | Základ všeho — typy, výjimky, Math, Convert, Guid, Console. Velká část hotova. |
| `System.Collections` | 🧩 STUB | Non-generic kolekce (ArrayList, Hashtable) jsou zastaralé. Jen rozhraní IEnumerable/IEnumerator. |
| `System.Collections.Generic` | ✅ PORT | Klíčové — List, Dictionary, Queue, Stack, HashSet, interfaces. Velká část hotova. |
| `System.Collections.Concurrent` | 🧩 STUB | Thread-safe kolekce (ConcurrentDictionary, ConcurrentQueue). Pro game engine nízká priorita. |
| `System.Collections.Immutable` | ❌ IGNORE | Immutable kolekce. Příliš komplexní, v C++ máme `const`. |
| `System.Collections.NonGeneric` | ❌ IGNORE | Legacy .NET 1.x (ArrayList, Hashtable). Používej Generic varianty. |
| `System.Collections.Specialized` | 🧩 STUB | OrderedDictionary, NameValueCollection — občas užitečné v ported kódu. |
| `System.Collections.ObjectModel` | ✅ PORT | Collection<T>, ReadOnlyCollection<T> hotové. ObservableCollection pro WinPhone. |
| `System.ComponentModel` | 🧩 STUB | Attribute, INotifyPropertyChanged hotové/stub. TypeConverter IGNORE. |
| `System.ComponentModel.Annotations` | ❌ IGNORE | Validační atributy. Nepotřebné pro game engine. |
| `System.ComponentModel.Composition` | ❌ IGNORE | MEF (Managed Extensibility Framework). Příliš komplexní. |
| `System.ComponentModel.TypeConverter` | ❌ IGNORE | Reflection-based konverze. |
| `System.Configuration` | ❌ IGNORE | app.config/web.config. Pro C++ game engine nepotřebné. |
| `System.Console` | ✅ PORT | Hotovo. |
| `System.Data` | ❌ IGNORE | ADO.NET, databáze. Mimo scope. |
| `System.Data.Common` | ❌ IGNORE | Stejné jako System.Data. |
| `System.Diagnostics` | ✅ PORT | Debug, Stopwatch hotové. Trace stub. Process/PerformanceCounter IGNORE. |
| `System.Diagnostics.Contracts` | ❌ IGNORE | Code Contracts. Nahrazeno asserty. |
| `System.Diagnostics.Process` | ❌ IGNORE | Spouštění procesů. Mimo scope. |
| `System.Diagnostics.StackTrace` | ❌ IGNORE | Platform-specific. |
| `System.Drawing.Primitives` | ❌ IGNORE | Point, Size, Rectangle — CNA je má jako `Microsoft::Xna::Framework::Point` apod. Neduplikovat. |
| `System.Formats.Tar` | ❌ IGNORE | TAR archiv. Nepotřebné. |
| `System.Formats.Asn1` | ❌ IGNORE | ASN.1 kódování (kryptografie). |
| `System.IO` | ✅ PORT | Velká část hotova. Zbývá: TextReader/TextWriter, StringReader/StringWriter, FileInfo/DirectoryInfo. |
| `System.IO.Compression` | 🧩 STUB | GZipStream/DeflateStream — XNB formát používá DEFLATE. Obalit zlib/miniz. |
| `System.IO.IsolatedStorage` | ✅ PORT | Hotovo. Klíčové pro mobile-eggbert save hry. |
| `System.IO.FileSystem.Watcher` | ❌ IGNORE | FileSystemWatcher. Nepotřebné pro game. |
| `System.IO.FileSystem.DriveInfo` | ❌ IGNORE | Informace o discích. Mimo scope. |
| `System.IO.Hashing` | ❌ IGNORE | Hashovací funkce (xxHash, CRC). Použij přímou C++ implementaci. |
| `System.IO.MemoryMappedFiles` | ❌ IGNORE | Memory-mapped soubory. Příliš OS-specifické. |
| `System.IO.Pipelines` | ❌ IGNORE | Async I/O pipeline. Závisí na async/await. |
| `System.IO.Pipes` | ❌ IGNORE | Named pipes. Mimo scope. |
| `System.IO.Ports` | ❌ IGNORE | Sériové porty. Mimo scope. |
| `System.Linq` | ❌ IGNORE | C++ ranges/algorithms jsou idiomatičtější. Neplést port. |
| `System.Linq.Expressions` | ❌ IGNORE | Expression trees. Vyžaduje runtime kompilaci. |
| `System.Memory` | 🧩 STUB | Span<T>, Memory<T> — C++23 má `std::span`. Typedef by stačil. |
| `System.Net` | ❌ IGNORE | HTTP client. Mimo scope game engine core. |
| `System.Net.Sockets` | 🧩 STUB | TcpClient, UdpClient — v TODO.md pro multiplayer. Nízká priorita. |
| `System.Net.Http` | ❌ IGNORE | HttpClient. Použij libcurl nebo platform API přímo. |
| `System.Net.Security` | ❌ IGNORE | TLS/SSL. Mimo scope. |
| `System.Net.WebSockets` | ❌ IGNORE | WebSocket. Mimo scope. |
| `System.Numerics` | ❌ IGNORE | Vector2/3/4, Matrix4x4, Quaternion — CNA je definuje v `Microsoft::Xna::Framework`. Neduplikovat! |
| `System.Numerics.Vectors` | ❌ IGNORE | Stejné. |
| `System.ObjectModel` | ✅ PORT | Viz System.Collections.ObjectModel — hotovo. |
| `System.Reflection` | ❌ IGNORE | C++ nemá runtime reflection. Templates pokryjí potřeby. |
| `System.Reflection.Emit` | ❌ IGNORE | IL generování za běhu. Naprosto mimo scope. |
| `System.Reflection.Metadata` | ❌ IGNORE | Low-level metadata reader. Mimo scope. |
| `System.Resources` | ❌ IGNORE | Assembly resources. XNA používá vlastní Content systém. |
| `System.Runtime` | ❌ IGNORE | CLR internals (GCHandle, RuntimeHelpers). |
| `System.Runtime.CompilerServices` | ❌ IGNORE | JIT hints, CallerMemberName. C++ má `__FUNCTION__`/`__FILE__`. |
| `System.Runtime.InteropServices` | ❌ IGNORE | P/Invoke, COM, Marshal. Nativní C++ nepotřebuje. |
| `System.Runtime.Intrinsics` | ❌ IGNORE | SIMD intrinsics. Použij C++ `<immintrin.h>` přímo. |
| `System.Runtime.Serialization` | ❌ IGNORE | XML/JSON serializace. Příliš komplexní, reflection-based. |
| `System.Security` | ❌ IGNORE | Permissions, access control. Mimo scope. |
| `System.Security.Cryptography` | ❌ IGNORE | Kryptografie. Použij OpenSSL v případě potřeby. |
| `System.Security.Claims` | ❌ IGNORE | Identity claims. Webový koncept. |
| `System.Speech` | ❌ IGNORE | Text-to-speech. Mimo scope. |
| `System.Text` | ✅ PORT | StringBuilder, Encoding, UTF8Encoding, ASCIIEncoding hotové. |
| `System.Text.Encoding.CodePages` | ❌ IGNORE | Windows code pages (CP1250 atd.). Zbytečné. |
| `System.Text.Encodings.Web` | ❌ IGNORE | HTML/URL/JSON escaping. Webový koncept. |
| `System.Text.Json` | ❌ IGNORE | JSON serializace. Použij nlohmann/json nebo rapidjson přímo v C++. |
| `System.Text.RegularExpressions` | 🧩 STUB | Regex/Match — obalit `std::regex`. Občas užitečné pro parsing. |
| `System.Threading` | 🧩 STUB | Thread, Monitor, Mutex, Interlocked — obalit STL threading. Nízká priorita. |
| `System.Threading.Channels` | ❌ IGNORE | Producer-consumer channels. Příliš async. |
| `System.Threading.Tasks` | ❌ IGNORE | async/await runtime. Nemoho portovat do C++ smysluplně. |
| `System.Threading.Tasks.Dataflow` | ❌ IGNORE | Dataflow pipelines. Mimo scope. |
| `System.Transactions` | ❌ IGNORE | Databázové transakce. |
| `System.Web` | ❌ IGNORE | ASP.NET/web framework. Mimo scope. |
| `System.Xml` | 🧩 STUB | XmlReader/XmlWriter — XNA content pipeline. Obalit tinyxml2 nebo pugixml. |
| `System.Xml.Linq` | 🧩 STUB | XDocument/XElement — pohodlnější API nad XML. Nízká priorita. |
| `System.Xml.XPath` | ❌ IGNORE | XPath queries. Mimo scope pro game engine. |
| `System.Xml.XmlSerializer` | ❌ IGNORE | Reflection-based serializace. |
| `Microsoft.CSharp` | ❌ IGNORE | Dynamic runtime. Mimo scope. |
| `Microsoft.Extensions.*` | ❌ IGNORE | ASP.NET Core infrastruktura. Mimo scope. |
| `Microsoft.VisualBasic` | ❌ IGNORE | VB.NET runtime. Mimo scope. |

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
