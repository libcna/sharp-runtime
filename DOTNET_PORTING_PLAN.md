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
| `UIntPtr` | ✅ DONE | `System/UIntPtr.hpp`. Mirrors `IntPtr` with `uintptr_t`. |
| `Nullable<T>` | ✅ DONE | `System/Nullable.hpp`. Map to `std::optional<T>`. |
| `Math` | ✅ DONE | `System/Math.hpp`. 📌 Essential, already used by CNA. |
| `Random` | ✅ DONE | `System/Random.hpp`. 📌 Used by mobile-eggbert `Decor`. |
| `DateTime` | ✅ DONE | `System/DateTime.hpp`. |
| `DateTimeOffset` | ✅ DONE | `System/DateTimeOffset.hpp`. 📌 Used by CNA sensor API. |
| `TimeSpan` | ✅ DONE | `System/TimeSpan.hpp`. 📌 Used by CNA and mobile-eggbert. |
| `TimeOnly` | ✅ DONE | `System/TimeOnly.hpp`. Hour/Minute/Second/Millisecond, ToString(), comparison. |
| `DateOnly` | ✅ DONE | `System/DateOnly.hpp`. Year/Month/Day, ToString(), comparison. |
| `Guid` | ✅ DONE | `System/Guid.hpp` + `.cpp`. RFC 4122 v4, `NewGuid()`, `ToString()`. |
| `Version` | ✅ DONE | `System/Version.hpp`. Major/Minor/Build/Revision, ToString(), comparison operators, string parser. |
| `Uri` | ❌ IGNORE | Network/web type. Not needed for game engine core. |
| `Convert` | ✅ DONE | `System/Convert.hpp` + `.cpp`. ToInt32/64/16, ToDouble/Single, ToString, base-N. |
| `BitConverter` | ✅ DONE | `System/BitConverter.hpp`. GetBytes/ToInt16/32/64/Single/Double/Boolean + ToString hex, little-endian. |
| `Buffer` | ✅ DONE | `System/Buffer.hpp`. BlockCopy, ByteLength<T>, GetByte/SetByte — header-only. |
| `Span<T>` / `ReadOnlySpan<T>` | ✅ DONE | `System/Span.hpp`. Non-owning view with Slice, range-for, bounds check. |
| `Memory<T>` | ❌ IGNORE | Too complex for the scope of sharp-runtime. Use `std::vector` slices. |
| `Array` (static methods) | ✅ DONE | `System/Array.hpp`. Sort/Copy/Resize/IndexOf/Reverse/Clear — template helpers over `std::vector`. |
| `Tuple<>` / `ValueTuple<>` | ✅ DONE | `System/Tuple.hpp`. Tuple2/3/4 structs with Item1/Item2/... fields. |
| `Type` | ✅ DONE | `System/Type.hpp`. Runtime type info is limited in C++; keep as partial stub. |
| `IComparable<T>` | ✅ DONE | Interface in `System/IComparable.hpp`. |
| `IEquatable<T>` | ✅ DONE | Interface in `System/IEquatable.hpp`. |
| `IDisposable` | ✅ DONE | `System/IDisposable.hpp`. 📌 Used by CNA audio/graphics. |
| `IServiceProvider` | ✅ DONE | `System/IServiceProvider.hpp`. |
| `IFormattable` | ✅ DONE | `System/IFormattable.hpp`. Abstract `ToString(format)` interface. |
| `ICloneable` | ✅ DONE | `System/ICloneable.hpp`. Abstract `Clone()` returning `shared_ptr<ICloneable>`. |
| `Action<>` | ✅ DONE | `System/Action.hpp`. Map to `std::function<void(...)>`. 📌 |
| `Func<>` | ✅ DONE | `System/Func.hpp`. `Func<R>`, `FuncT<T,R>`, `FuncT2`, `FuncT3` via `std::function`. |
| `Predicate<T>` | ✅ DONE | `System/Predicate.hpp`. Typedef `std::function<bool(T)>`. |
| `EventArgs` | ✅ DONE | `System/EventArgs.hpp`. 📌 Used everywhere in CNA. |
| `EventHandler<T>` | ✅ DONE | `System/EventHandler.hpp`. 📌 Used everywhere in CNA. |
| `Console` | ✅ DONE | `System/Console.hpp`. Write/WriteLine/ReadLine/Error, header-only. |
| `Environment` | ✅ DONE | `System/Environment.hpp`. NewLine, GetCurrentDirectory, GetEnvironmentVariable, ProcessorCount, Exit, Is64BitProcess — header-only. |
| `GC` | ❌ IGNORE | No GC in C++. RAII + smart pointers replace it. Don't port. |
| `AppDomain` | ✅ DONE | `System/AppDomain.hpp`. CurrentDomain() singleton; FriendlyName/BaseDirectory; UnhandledException/ProcessExit no-ops. |
| `AppContext` | ✅ DONE | `System/AppContext.hpp`. GetData/SetData + TryGetSwitch/SetSwitch with mutex-guarded maps. |
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
| `NullReferenceException` | ✅ DONE | `System/NullReferenceException.hpp`. |
| `IndexOutOfRangeException` | ✅ DONE | `System/IndexOutOfRangeException.hpp`. |
| `InvalidCastException` | ✅ DONE | `System/InvalidCastException.hpp`. |
| `FormatException` | ✅ DONE | `System/FormatException.hpp`. Thrown by `Convert` on bad input. |
| `OutOfMemoryException` | ✅ DONE | `System/OutOfMemoryException.hpp`. |
| `ObjectDisposedException` | ✅ DONE | `System/ObjectDisposedException.hpp`. 📌 Used by CNA. |
| `UnauthorizedAccessException` | ✅ DONE | `System/UnauthorizedAccessException.hpp`. |
| `IOException` | ✅ DONE | `System/IO/IOException.hpp`. Base for all I/O exceptions. |
| `FileNotFoundException` | ✅ DONE | `System/IO/FileNotFoundException.hpp`. Has `FileName` property. |
| `DirectoryNotFoundException` | ✅ DONE | `System/IO/DirectoryNotFoundException.hpp`. Inherits `IOException`. |
| `EndOfStreamException` | ✅ DONE | `System/IO/EndOfStreamException.hpp`. |
| `TimeoutException` | ✅ DONE | `System/TimeoutException.hpp`. |
| `StackOverflowException` | ❌ IGNORE | Platform crash, can't meaningfully catch in C++. |

---

## 3. System.Collections (non-generic)

Non-generic collections are legacy .NET 1.x. Limited use in XNA/game code.
C# ported code may use them but modern code prefers Generic collections.

| Type | Status | Opinion |
|------|--------|---------|
| `IEnumerable` | ✅ DONE | `System/Collections/IEnumerable.hpp`. |
| `IEnumerator` | ✅ DONE | `System/Collections/IEnumerator.hpp`. |
| `ICollection` | ✅ DONE | `System/Collections/ICollection.hpp`. Count + IsSynchronized. |
| `IList` | ✅ DONE | `System/Collections/IList.hpp`. Add/Clear/Contains/IndexOf/Insert/Remove/RemoveAt. |
| `IDictionary` | ✅ DONE | `System/Collections/IDictionary.hpp`. Add/Clear/Contains/Remove. |
| `ArrayList` | ❌ IGNORE | Use `List<T>`. |
| `Hashtable` | ❌ IGNORE | Use `Dictionary<K,V>`. |
| `Queue` | ✅ DONE | `System/Collections/Queue.hpp`. Non-generic void* queue. Prefer Generic::Queue<T>. |
| `Stack` | ✅ DONE | `System/Collections/Stack.hpp`. Non-generic void* stack. Prefer Generic::Stack<T>. |
| `BitArray` | ✅ DONE | `System/Collections/BitArray.hpp`. And/Or/Xor/Not, Get/Set/SetAll. |

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
| `IReadOnlyDictionary<K,V>` | ✅ DONE | `System/Collections/Generic/IReadOnlyDictionary.hpp`. |
| `HashSet<T>` | ✅ DONE | `System/Collections/Generic/HashSet.hpp`. Wraps `std::unordered_set`. Add/Remove/Contains/UnionWith/IntersectWith. |
| `SortedDictionary<K,V>` | ✅ DONE | `System/Collections/Generic/SortedDictionary.hpp`. Wraps `std::map`. |
| `SortedList<K,V>` | ✅ DONE | `System/Collections/Generic/SortedList.hpp`. Wraps `std::map`; has IndexOfKey/RemoveAt. |
| `Queue<T>` | ✅ DONE | `System/Collections/Generic/Queue.hpp`. Enqueue/Dequeue/Peek/Contains/Clear. |
| `Stack<T>` | ✅ DONE | `System/Collections/Generic/Stack.hpp`. Push/Pop/Peek/Contains/Clear. |
| `LinkedList<T>` | ✅ DONE | `System/Collections/Generic/LinkedList.hpp`. Wraps `std::list`; AddFirst/AddLast/Remove/Contains. |
| `KeyValuePair<K,V>` | ✅ DONE | `System/Collections/Generic/KeyValuePair.hpp`. Struct with Key+Value. |
| `Comparer<T>` | ✅ DONE | `System/Collections/Generic/Comparer.hpp`. Default() uses `operator<`. |
| `EqualityComparer<T>` | ✅ DONE | `System/Collections/Generic/Comparer.hpp`. Default() uses `operator==` + `std::hash`. |

---

## 5. System.Collections.ObjectModel

| Type | Status | Opinion |
|------|--------|---------|
| `Collection<T>` | ✅ DONE | `System/Collections/ObjectModel/Collection.hpp`. |
| `ReadOnlyCollection<T>` | ✅ DONE | `System/Collections/ObjectModel/ReadOnlyCollection.hpp`. |
| `ObservableCollection<T>` | ✅ DONE | `System/Collections/ObjectModel/ObservableCollection.hpp`. CollectionChanged event + NotifyCollectionChangedEventArgs. |
| `KeyedCollection<K,T>` | ✅ DONE | `System/Collections/ObjectModel/KeyedCollection.hpp`. Abstract base; GetKeyForItem() + key-based lookup. |

---

## 6. System.Text

| Type | Status | Opinion |
|------|--------|---------|
| `StringBuilder` | ✅ DONE | `System/Text/StringBuilder.hpp`. 📌 Used by mobile-eggbert `Worlds`. |
| `Encoding` | ✅ DONE | `System/Text/Encoding.hpp`. Base class for encodings. |
| `UTF8Encoding` | ✅ DONE | `System/Text/UTF8Encoding.hpp`. Inherits polymorphic `Encoding`. |
| `ASCIIEncoding` | ✅ DONE | `System/Text/ASCIIEncoding.hpp`. Non-ASCII chars replaced with `?`. |
| `UnicodeEncoding` | ✅ DONE | `System/Text/UnicodeEncoding.hpp`. UTF-16 LE; `Encoding::Unicode()` factory added. |
| `Decoder` | ✅ DONE | `System/Text/Decoder.hpp`. Wraps Encoding::GetString; stateless. |
| `Encoder` | ✅ DONE | `System/Text/Encoder.hpp`. Wraps Encoding::GetBytes; stateless. |

---

## 7. System.Text.RegularExpressions

| Type | Status | Opinion |
|------|--------|---------|
| `Regex` | ✅ DONE | `System/Text/RegularExpressions/Regex.hpp`. Wraps `std::regex`; IsMatch/Match/Matches/Replace/Split + static overloads. |
| `Match` | ✅ DONE | `System/Text/RegularExpressions/Match.hpp`. Success/Value/Index/Length + Group(n). |
| `MatchCollection` | ✅ DONE | `System/Text/RegularExpressions/MatchCollection.hpp`. Count, operator[], range-for. |

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
| `BufferedStream` | ✅ DONE | `System/IO/BufferedStream.hpp`. Pass-through wrapper; no actual buffering. |
| `TextReader` | ✅ DONE | `System/IO/TextReader.hpp`. Abstract base; Peek/Read/ReadLine/ReadToEnd. |
| `TextWriter` | ✅ DONE | `System/IO/TextWriter.hpp`. Abstract base; Write/WriteLine for all primitive types. |
| `StringReader` | ✅ DONE | `System/IO/StringReader.hpp`. Reads from std::string; Peek/Read/ReadLine/ReadToEnd. |
| `StringWriter` | ✅ DONE | `System/IO/StringWriter.hpp`. Writes to std::ostringstream; ToString() returns buffer. |
| `File` | ✅ DONE | `System/IO/File.hpp`. ReadAllText/WriteAllText/ReadAllBytes/WriteAllBytes/Exists/Delete/Copy/Move/AppendAllText. |
| `Directory` | ✅ DONE | `System/IO/Directory.hpp`. Exists/CreateDirectory/Delete/Move/GetFiles/GetDirectories. |
| `Path` | ✅ DONE | `System/IO/Path.hpp`. Combine/GetFileName/GetExtension/GetDirectoryName/GetTempPath/ChangeExtension etc. |
| `FileInfo` | ✅ DONE | `System/IO/FileInfo.hpp`. Name/FullName/Extension/Length/Exists/IsReadOnly, Delete/CopyTo/MoveTo. |
| `DirectoryInfo` | ✅ DONE | `System/IO/DirectoryInfo.hpp`. Name/FullName/Exists/Parent, Create/Delete/MoveTo/GetFiles/GetDirectories. |
| `FileMode` | ✅ DONE | `System/IO/FileMode.hpp`. Now includes all values: CreateNew/Create/Open/OpenOrCreate/Truncate/Append. |
| `FileAccess` | ✅ DONE | `System/IO/FileAccess.hpp`. Read/Write/ReadWrite. |
| `FileShare` | ✅ DONE | `System/IO/FileShare.hpp`. None/Read/Write/ReadWrite/Delete/Inheritable. |
| `SeekOrigin` | ✅ DONE | `System/IO/SeekOrigin.hpp`. Begin/Current/End. |
| `IOException` | ✅ DONE | `System/IO/IOException.hpp`. Base for all I/O exceptions. |
| `FileNotFoundException` | ✅ DONE | `System/IO/FileNotFoundException.hpp`. Has `FileName` property. |
| `DirectoryNotFoundException` | ✅ DONE | `System/IO/DirectoryNotFoundException.hpp`. Inherits `IOException`. |
| `EndOfStreamException` | ✅ DONE | `System/IO/EndOfStreamException.hpp`. |
| `InvalidDataException` | ✅ DONE | `System/IO/InvalidDataException.hpp`. Useful for malformed content. |

---

## 9. System.IO.IsolatedStorage

Used by mobile-eggbert for save game storage on Windows Phone.

| Type | Status | Opinion |
|------|--------|---------|
| `IsolatedStorageFile` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageFile.hpp`. 📌 |
| `IsolatedStorageFileStream` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp`. 📌 |
| `IsolatedStorageException` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageException.hpp`. 📌 |
| `IsolatedStorage` (abstract) | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorage.hpp`. Abstract base with Scope/AvailableFreeSpace/Quota/UsedSize. |
| `IsolatedStorageScope` | ✅ DONE | `System/IO/IsolatedStorage/IsolatedStorageScope.hpp`. User/Domain/Assembly/Roaming/Machine/Application flags. |

---

## 10. System.IO.Compression

| Type | Status | Opinion |
|------|--------|---------|
| `GZipStream` | ✅ DONE | `System/IO/Compression/GZipStream.hpp`. Stub; NotImplementedException + zlib/miniz integration notes. |
| `DeflateStream` | ✅ DONE | `System/IO/Compression/DeflateStream.hpp`. Stub; NotImplementedException + zlib integration notes; XNB note. |
| `ZipArchive` | ✅ DONE | `System/IO/Compression/ZipArchive.hpp`. Stub; NotImplementedException + miniz/libzip notes. |
| `ZipFile` | ❌ IGNORE | Too complex for game engine core. |

---

## 11. System.Threading

Threading support. C++ has excellent STL threading; these are shims for ported code.

| Type | Status | Opinion |
|------|--------|---------|
| `Thread` | ✅ DONE | `System/Threading/Thread.hpp`. Wraps `std::thread`; Join/Sleep/IsAlive/Name/IsBackground. |
| `Monitor` | ✅ DONE | `System/Threading/Monitor.hpp`. Stub — Enter/Exit/TryEnter (full impl needs per-object map). |
| `Mutex` | ✅ DONE | `System/Threading/Mutex.hpp`. Wraps `std::mutex`; WaitOne/ReleaseMutex. |
| `Semaphore` / `SemaphoreSlim` | ✅ DONE | `System/Threading/SemaphoreSlim.hpp`. Wraps `std::condition_variable`; Wait/Release/CurrentCount. |
| `ManualResetEvent` | ✅ DONE | `System/Threading/ManualResetEvent.hpp`. Set/Reset/WaitOne with optional timeout. |
| `AutoResetEvent` | ✅ DONE | `System/Threading/AutoResetEvent.hpp`. Set/Reset/WaitOne — auto-resets after releasing one thread. |
| `Interlocked` | ✅ DONE | `System/Threading/Interlocked.hpp`. Increment/Decrement/Add/Exchange/CompareExchange via GCC builtins. |
| `Timer` | ✅ DONE | `System/Threading/Timer.hpp`. std::thread-based; dueTime + period, Change/Dispose. |
| `ThreadPool` | ❌ IGNORE | Complex to port correctly. Use `std::async` directly. |
| `CancellationToken` | ✅ DONE | `System/Threading/CancellationToken.hpp`. IsCancellationRequested, ThrowIfCancellationRequested. |
| `CancellationTokenSource` | ✅ DONE | `System/Threading/CancellationTokenSource.hpp`. Cancel, Token property. |
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
| `Complex` | ✅ DONE | `System/Numerics/Complex.hpp`. Wraps std::complex<double>; arithmetic, Sqrt/Exp/Log/Sin/Cos/Pow. |
| `BigInteger` | ✅ DONE | `System/Numerics/BigInteger.hpp`. Self-contained sign+base-10⁹ magnitude; +/-/*/compare/ToString/Parse. |
| `Half` (float16) | ✅ DONE | `System/Half.hpp`. IEEE 754 ToSingle/FromSingle, Zero/NaN/Infinity/MaxValue/Epsilon constants. |

---

## 15. System.Diagnostics

| Type | Status | Opinion |
|------|--------|---------|
| `Debug` | ✅ DONE | `System/Diagnostics/Debug.hpp`. Assert/Write/WriteLine/Fail — stripped in NDEBUG, header-only. |
| `Trace` | ✅ DONE | `System/Diagnostics/Trace.hpp`. Write/WriteLine/TraceInfo/TraceWarning/TraceError/Assert/Fail — always enabled, writes to std::cerr. |
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
| `CultureInfo` | ✅ DONE | `System/Globalization/CultureInfo.hpp`. InvariantCulture() + CurrentCulture() stub, Name/IsNeutralCulture/IsReadOnly — header-only. |
| `NumberFormatInfo` | ✅ DONE | `System/Globalization/NumberFormatInfo.hpp`. InvariantInfo/CurrentInfo; decimal/group separators, currency, NaN symbols. |
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
| `INotifyPropertyChanged` | ✅ DONE | `System/ComponentModel/INotifyPropertyChanged.hpp`. PropertyChanged event + PropertyChangedEventArgs. |
| `TypeConverter` | ❌ IGNORE | Reflection-based type conversion. Not needed. |
| `DescriptionAttribute` | ✅ DONE | `System/ComponentModel/DescriptionAttribute.hpp`. Stores description string. |
| `DefaultValueAttribute` | ✅ DONE | `System/ComponentModel/DescriptionAttribute.hpp`. Stores default value (int/double/bool/string). |

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
| `Sockets (TcpClient, UdpClient)` | ✅ DONE | `System/Net/Sockets/TcpClient.hpp` + `UdpClient.hpp`. Stubs; NotImplementedException + POSIX/Winsock integration notes. |
| `IPAddress`, `IPEndPoint` | ✅ DONE | `System/Net/IPAddress.hpp` + `IPEndPoint.hpp`. IPv4 parse/toString, Any/Loopback constants. |

---

## 24. System.Xml / System.Xml.Linq

| Type | Status | Opinion |
|------|--------|---------|
| `XmlReader` / `XmlWriter` | ✅ DONE | `System/Xml/XmlReader.hpp` + `XmlWriter.hpp`. Stubs; NotImplementedException + tinyxml2/pugixml integration notes. |
| `XDocument` / `XElement` (LINQ to XML) | ✅ DONE | `System/Xml/Linq/XDocument.hpp` + `XElement.hpp` + `XAttribute.hpp` + `XName.hpp`. Stub parse/load; full tree manipulation, ToString(). |
| `XmlSerializer` | ❌ IGNORE | Reflection-based. Too complex. |

---

## 25. System.Text.Json

| Type | Status | Opinion |
|------|--------|---------|
| `JsonSerializer` | ❌ IGNORE | Use nlohmann/json or rapidjson directly in C++. |
| `JsonDocument` / `JsonElement` | ✅ DONE | `System/Text/Json/JsonDocument.hpp` + `JsonElement.hpp` + `JsonValueKind.hpp`. Parse stub; GetString/Int32/Double/Boolean/TryGetProperty/EnumerateArray. |
| `JsonSerializerOptions` | ✅ DONE | `System/Text/Json/JsonSerializerOptions.hpp`. WriteIndented/AllowTrailingCommas/MaxDepth/ReadCommentHandling; Default() singleton. |
| `JsonSerializer` | ✅ DONE | `System/Text/Json/JsonSerializer.hpp`. Stub; Serialize throws NotImplementedException; Deserialize calls JsonDocument::Parse(). |

---

## 26. System.Console

| Type | Status | Opinion |
|------|--------|---------|
| `Console` | ✅ DONE | `System/Console.hpp`. Write/WriteLine/ReadLine/Error_Write/Error_WriteLine, header-only. |

---

## 27. System.Environment

| Type | Status | Opinion |
|------|--------|---------|
| `Environment` | ✅ DONE | `System/Environment.hpp`. NewLine, GetCurrentDirectory, GetEnvironmentVariable, ProcessorCount, Exit, Is64BitProcess — header-only. |

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

### ✅ Already ported (wave 1 — core types)
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

### ✅ Already ported (wave 2+3 — exceptions, IO/Text/Threading, Regex, collections)
- **Exceptions**: `DirectoryNotFoundException`, `InvalidDataException`, `NullReferenceException`,
  `InvalidCastException`, `OutOfMemoryException`, `TimeoutException`, `OperationCanceledException`
- **IO**: `TextReader`, `TextWriter`, `StringReader`, `StringWriter`
- **Text**: `UnicodeEncoding` + `Encoding::Unicode()` factory
- **Text.RegularExpressions**: `Regex`, `Match`, `MatchCollection` (wraps `std::regex`)
- **Threading**: `Thread`, `Monitor` (stub), `Mutex`, `Interlocked`, `CancellationToken`, `CancellationTokenSource`
- **Collections**: `IReadOnlyDictionary<K,V>`
- **Core**: `Span<T>`, `ReadOnlySpan<T>`, `Tuple2/3/4`, `IFormattable`, `ICloneable`,
  `BitConverter`, `Buffer`, `Environment`, `Version`, `CultureInfo`

### ✅ Already ported (wave 4 — collections, threading events, Globalization)
- **Core**: `UIntPtr`, `TimeOnly`, `DateOnly`
- **Collections non-generic**: `ICollection`, `IList`, `IDictionary` interfaces
- **Collections.Generic**: `SortedDictionary<K,V>`, `SortedList<K,V>`, `LinkedList<T>`, `Comparer<T>`, `EqualityComparer<T>`
- **Collections.ObjectModel**: `ObservableCollection<T>` with `CollectionChanged` event + `NotifyCollectionChangedEventArgs`
- **ComponentModel**: `INotifyPropertyChanged` + `PropertyChangedEventArgs`
- **Threading**: `ManualResetEvent`, `AutoResetEvent`, `SemaphoreSlim`
- **IO.IsolatedStorage**: `IsolatedStorage` (abstract base), `IsolatedStorageScope` enum
- **Globalization**: `NumberFormatInfo`

### ✅ Already ported (wave 5 — IO wrappers, collections, numerics, net, XML stubs)
- **IO**: `BufferedStream`, `FileInfo`, `DirectoryInfo`
- **IO.Compression**: `ZipArchive` stub (NotImplementedException + miniz/libzip notes)
- **Collections**: non-generic `Queue`, `Stack`, `BitArray`
- **Numerics**: `Complex` (wraps std::complex<double>)
- **Half**: IEEE 754 float16 with ToSingle/FromSingle
- **Threading**: `Timer`
- **Diagnostics**: `Trace` (always-on, std::cerr)
- **ComponentModel**: `DescriptionAttribute`, `DefaultValueAttribute`
- **Net**: `IPAddress`, `IPEndPoint`
- **Xml**: `XmlReader`, `XmlWriter` stubs (NotImplementedException + tinyxml2/pugixml notes)

### ✅ Already ported (wave 6 — concurrent, specialized, BigInteger, sockets stubs)
- **Collections.Concurrent**: `ConcurrentDictionary<K,V>`, `ConcurrentQueue<T>`, `ConcurrentStack<T>`
- **Collections.Specialized**: `OrderedDictionary`, `NameValueCollection`
- **Collections.ObjectModel**: `KeyedCollection<K,T>` abstract base
- **Text**: `Decoder`, `Encoder` (thin wrappers over `Encoding`)
- **Numerics**: `BigInteger` (self-contained, no external lib)
- **Net.Sockets**: `TcpClient`, `TcpListener`, `UdpClient` stubs (NotImplementedException + POSIX/Winsock notes)

### ✅ Already ported (waves 10–15 — attributes, immutable, hashing, unicode, JSON/XML stubs)
- **System**: `Attribute`, `AttributeTargets`, `AttributeUsageAttribute`, `CLSCompliantAttribute`, `FlagsAttribute`, `ObsoleteAttribute`, `NonSerializedAttribute`, `ParamArrayAttribute`, `SerializableAttribute`, `ThreadStaticAttribute`, `AppContext`, `AppDomain`, `GC`, `MarshalByRefObject`, `AsyncCallback`, `ResolveEventArgs`, `ResolveEventHandler`, `UnhandledExceptionEventArgs`, `UnhandledExceptionEventHandler`, `ApplicationId`, `IParsable<T>`, `Int128`, `UInt128`, `TimeZone`, `TimeZoneInfo`, `AssemblyLoadEventArgs`
- **System.Diagnostics**: `ConditionalAttribute`, `DebuggableAttribute`, `DebuggerBrowsableAttribute`, `DebuggerDisplayAttribute`, `DebuggerHiddenAttribute`, `DebuggerNonUserCodeAttribute`, `DebuggerStepperBoundaryAttribute`, `DebuggerStepThroughAttribute`, `DebuggerTypeProxyAttribute`, `DebuggerVisualizerAttribute`, `StackTraceHiddenAttribute`, `StackFrame`, `StackTrace`
- **System.ComponentModel**: `DefaultValueAttribute`, `Win32Exception`
- **System.Globalization**: `SortVersion`, `StringInfo` (stub), `Calendar` (abstract), `GregorianCalendar`
- **System.Buffers**: `OperationStatus`, `IMemoryOwner<T>`, `StandardFormat`, `ArrayPool<T>`
- **System.Security**: `CryptographicException`, `SecurityException`, `VerificationException`, security attributes (SecurityRules/AllowPartiallyTrustedCallers/SecurityCritical/etc.)
- **System.Runtime**: `AmbiguousImplementationException`, `GCSettings`
- **System.Net**: `HttpStatusCode` (100–511)
- **System.IO**: `DriveInfo` + `DriveType` enum
- **System.Text**: `Rune` (UTF-32 codepoint), `UTF7Encoding`
- **System.Text.Unicode**: `UnicodeRange`, `UnicodeRanges` (30+ named blocks)
- **System.IO.Hashing**: `NonCryptographicHashAlgorithm`, `Crc32`, `XxHash32`, `XxHash64`
- **System.Xml.Linq**: `XName`, `XAttribute`, `XElement`, `XDocument` (stub parse/load)
- **System.Text.Json**: `JsonValueKind`, `JsonElement`, `JsonDocument`, `JsonSerializerOptions`, `JsonSerializer` (stub)
- **System.Runtime.CompilerServices**: `MethodImplOptions`, `MethodImplAttribute`, `CallerMemberNameAttribute`, `CallerFilePathAttribute`, `CallerLineNumberAttribute`, `CallerArgumentExpressionAttribute`
- **System.Collections.Generic**: `EqualityComparer<T>`
- **System.Numerics**: `BFloat16`, `DivisionRounding`
- **System.Collections.Immutable**: `ImmutableArray<T>`, `ImmutableList<T>`, `ImmutableDictionary<K,V>`, `ImmutableHashSet<T>`, `ImmutableSortedDictionary<K,V>`, `ImmutableSortedSet<T>`, `ImmutableQueue<T>`, `ImmutableStack<T>`

### 🔨 Zbývá portovat (pouze s externími závislostmi)
- `GZipStream` / `DeflateStream` — implementace čeká na zlib/miniz
- `ZipArchive` — implementace čeká na miniz/libzip
- `XmlReader` / `XmlWriter` — implementace čeká na tinyxml2/pugixml
- `TcpClient` / `UdpClient` — implementace čeká na POSIX sockets / Winsock2

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
| `System.Collections` | ✅ DONE | IEnumerable/IEnumerator/ICollection/IList/IDictionary interfaces; Queue/Stack/BitArray non-generic; DictionaryEntry; Comparer. |
| `System.Collections.Generic` | ✅ DONE | List, Dictionary, Queue, Stack, HashSet, SortedDictionary, SortedList, LinkedList, KeyValuePair, Comparer, EqualityComparer, CollectionExtensions, IAsyncEnumerable, all IXxx interfaces. |
| `System.Collections.Concurrent` | ✅ DONE | `ConcurrentDictionary<K,V>`, `ConcurrentQueue<T>`, `ConcurrentStack<T>`, `IProducerConsumerCollection<T>`. |
| `System.Collections.Immutable` | ✅ DONE | `ImmutableArray<T>`, `ImmutableList<T>`, `ImmutableDictionary<K,V>`, `ImmutableHashSet<T>`, `ImmutableSortedDictionary<K,V>`, `ImmutableSortedSet<T>`, `ImmutableQueue<T>`, `ImmutableStack<T>` — all use `shared_ptr<const container<T>>` pattern. |
| `System.Collections.NonGeneric` | ❌ IGNORE | Legacy .NET 1.x (ArrayList, Hashtable). Používej Generic varianty. |
| `System.Collections.Specialized` | ✅ DONE | `OrderedDictionary`, `NameValueCollection`, `StringCollection`, `StringDictionary`, `HybridDictionary`, `BitVector32`. |
| `System.Collections.ObjectModel` | ✅ PORT | Collection<T>, ReadOnlyCollection<T> hotové. ObservableCollection pro WinPhone. |
| `System.ComponentModel` | ✅ DONE | `Attribute` base, `INotifyPropertyChanged`, `PropertyDescriptorCollection`, `DescriptionAttribute`, `DefaultValueAttribute`, `EditorBrowsableAttribute`, `Win32Exception`. TypeConverter IGNORE. |
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
| `System.IO` | ✅ DONE | Vše hotovo: Stream, File, Directory, Path, BinaryReader/Writer, StreamReader/Writer, TextReader/Writer, StringReader/Writer, FileInfo, DirectoryInfo, MemoryStream, FileStream, BufferedStream, DriveInfo. |
| `System.IO.Compression` | 🧩 STUB | GZipStream/DeflateStream — XNB formát používá DEFLATE. Obalit zlib/miniz. |
| `System.IO.IsolatedStorage` | ✅ PORT | Hotovo. Klíčové pro mobile-eggbert save hry. |
| `System.IO.FileSystem.Watcher` | ❌ IGNORE | FileSystemWatcher. Nepotřebné pro game. |
| `System.IO.FileSystem.DriveInfo` | ❌ IGNORE | Informace o discích. Mimo scope. |
| `System.IO.Hashing` | ✅ DONE | `NonCryptographicHashAlgorithm` (abstract base), `Crc32` (lookup table, 0xEDB88320), `XxHash32` (streaming, accumulators), `XxHash64` (streaming, 32-byte blocks). |
| `System.IO.MemoryMappedFiles` | ❌ IGNORE | Memory-mapped soubory. Příliš OS-specifické. |
| `System.IO.Pipelines` | ❌ IGNORE | Async I/O pipeline. Závisí na async/await. |
| `System.IO.Pipes` | ❌ IGNORE | Named pipes. Mimo scope. |
| `System.IO.Ports` | ❌ IGNORE | Sériové porty. Mimo scope. |
| `System.Linq` | ❌ IGNORE | C++ ranges/algorithms jsou idiomatičtější. Neplést port. |
| `System.Linq.Expressions` | ❌ IGNORE | Expression trees. Vyžaduje runtime kompilaci. |
| `System.Memory` | ✅ DONE | `Span<T>` + `ReadOnlySpan<T>` in `System/Span.hpp`. `Memory<T>` — IGNORE (use `std::vector` slices). |
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
| `System.Text` | ✅ DONE | `StringBuilder`, `Encoding`, `UTF8Encoding`, `ASCIIEncoding`, `UnicodeEncoding`, `UTF7Encoding`, `Decoder`, `Encoder`, `Rune`. |
| `System.Text.Encoding.CodePages` | ❌ IGNORE | Windows code pages (CP1250 atd.). Zbytečné. |
| `System.Text.Encodings.Web` | ❌ IGNORE | HTML/URL/JSON escaping. Webový koncept. |
| `System.Text.Json` | ❌ IGNORE | JSON serializace. Použij nlohmann/json nebo rapidjson přímo v C++. |
| `System.Text.RegularExpressions` | ✅ DONE | `Regex`, `Match`, `MatchCollection` — wraps `std::regex`; IsMatch/Match/Matches/Replace/Split. |
| `System.Threading` | ✅ DONE | Thread, Monitor, Mutex, Semaphore/Slim, ManualResetEvent, AutoResetEvent, Interlocked, Timer, CancellationToken/Source, SpinLock, SpinWait, ReaderWriterLockSlim, Volatile, ThreadLocal, AsyncLocal, PeriodicTimer. |
| `System.Threading.Channels` | ❌ IGNORE | Producer-consumer channels. Příliš async. |
| `System.Threading.Tasks` | ❌ IGNORE | async/await runtime. Nemoho portovat do C++ smysluplně. |
| `System.Threading.Tasks.Dataflow` | ❌ IGNORE | Dataflow pipelines. Mimo scope. |
| `System.Transactions` | ❌ IGNORE | Databázové transakce. |
| `System.Web` | ❌ IGNORE | ASP.NET/web framework. Mimo scope. |
| `System.Xml` | 🧩 STUB | XmlReader/XmlWriter — XNA content pipeline. Obalit tinyxml2 nebo pugixml. |
| `System.Xml.Linq` | ✅ DONE | `XName`, `XAttribute`, `XElement`, `XDocument` — full tree manipulation + stub parse/load. |
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
