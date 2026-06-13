# API_AUDIT.md — sharp-runtime systematic .NET API audit
*Created: 2026-06-13 — session 67*

## Legend

| Status | Meaning |
|--------|---------|
| `PORTED` | All meaningful .NET methods implemented; no known gaps |
| `PARTIAL` | Some methods missing vs .NET; needs systematic completion |
| `STUB` | API surface only — bodies are no-ops or throw NotImplementedException |
| `IGNORE` | Pure interface / attribute marker / enum / thin wrapper — nothing to implement |
| `TO_CHECK` | Not yet audited against full .NET API |

---

## SharpRuntime/

| File | Status | Notes |
|------|--------|-------|
| `SharpRuntime/SharpRuntimeHelper.hpp` | `PORTED` | Primitive typedefs — complete |
| `SharpRuntime/Prop.hpp` | `PORTED` | Property macros |
| `SharpRuntime/Storage/StoragePaths.hpp` | `PORTED` | Platform storage root |
| `SharpRuntime/Experimental/Property.hpp` | `IGNORE` | Experimental |
| `SharpRuntime/Experimental/ReadonlyProperty.hpp` | `IGNORE` | Experimental |

---

## System/ (root)

| File | Status | Notes |
|------|--------|-------|
| `AccessViolationException.hpp` | `IGNORE` | Exception marker only |
| `Action.hpp` | `PORTED` | Typedef for std::function |
| `AggregateException.hpp` | `PARTIAL` | Missing InnerExceptions collection, Flatten() |
| `AppContext.hpp` | `PORTED` | BaseDirectory implemented |
| `AppDomain.hpp` | `PORTED` | BaseDirectory, FriendlyName |
| `AppDomainUnloadedException.hpp` | `IGNORE` | Exception marker |
| `ApplicationException.hpp` | `IGNORE` | Exception marker |
| `ApplicationId.hpp` | `IGNORE` | Metadata type |
| `ArgumentException.hpp` | `IGNORE` | Exception marker |
| `ArgumentNullException.hpp` | `IGNORE` | Exception marker |
| `ArgumentOutOfRangeException.hpp` | `IGNORE` | Exception marker |
| `ArithmeticException.hpp` | `IGNORE` | Exception marker |
| `Array.hpp` | `PARTIAL` | Missing: `CreateInstance`, `ConstrainedCopy`; ConvertAll/Exists/Find/FindLast/FindAll/FindIndex/FindLastIndex/ForEach/TrueForAll/LastIndexOf added session 68 |
| `ArraySegment.hpp` | `TO_CHECK` | Needs audit |
| `ArrayTypeMismatchException.hpp` | `IGNORE` | Exception marker |
| `AssemblyLoadEventArgs.hpp` | `IGNORE` | Event args |
| `AsyncCallback.hpp` | `IGNORE` | Delegate typedef |
| `Attribute.hpp` | `IGNORE` | Base attribute |
| `AttributeTargets.hpp` | `IGNORE` | Enum |
| `AttributeUsageAttribute.hpp` | `IGNORE` | Attribute marker |
| `BadImageFormatException.hpp` | `IGNORE` | Exception marker |
| `BitConverter.hpp` | `PORTED` | All bit/byte methods including DoubleToInt64Bits etc. |
| `Boolean.hpp` | `PARTIAL` | Missing: `TryParse(span)`, `ToString(IFormatProvider)` — minor |
| `Buffer.hpp` | `PORTED` | BlockCopy, ByteLength, GetByte, SetByte — already complete |
| `Byte.hpp` | `PARTIAL` | Has Parse/TryParse/MinValue/MaxValue — missing `ToString(format)` |
| `Char.hpp` | `PARTIAL` | ASCII helpers added session 68; still missing: `GetUnicodeCategory` |
| `CLSCompliantAttribute.hpp` | `IGNORE` | Attribute marker |
| `Console.hpp` | `PARTIAL` | ReadLine/Read/Color(ANSI)/ResetColor/Clear/Beep/SetCursorPosition/WindowWidth/WindowHeight added session 69; missing `ReadKey`, `CursorLeft/Top` (not queryable portably) |
| `Convert.hpp` | `PARTIAL` | All numeric conversions done; missing `ToSByte`, `ToDecimal`, `ChangeType` |
| `DataMisalignedException.hpp` | `IGNORE` | Exception marker |
| `DateOnly.hpp` | `PORTED` | AddDays/Months/Years, Parse/TryParse, ToString(format), FromDateTime |
| `DateTime.hpp` | `PORTED` | Full arithmetic, ISO-8601 Parse/ToString |
| `DateTimeKind.hpp` | `IGNORE` | Enum |
| `DateTimeOffset.hpp` | `PARTIAL` | Now/UtcNow, Add*/Subtract, Parse/TryParse, ToString(format), comparison — done session 69 |
| `DayOfWeek.hpp` | `IGNORE` | Enum |
| `DBNull.hpp` | `IGNORE` | Singleton marker |
| `Decimal.hpp` | `PORTED` | Full arithmetic, TryParse |
| `Double.hpp` | `PARTIAL` | Has Parse/TryParse/IsNaN etc. — missing `ToString(format)` |
| `DuplicateWaitObjectException.hpp` | `IGNORE` | Exception marker |
| `EntryPointNotFoundException.hpp` | `IGNORE` | Exception marker |
| `Environment.hpp` | `PARTIAL` | GetEnvironmentVariable/NewLine/MachineName/UserName/TickCount64 added session 68; still missing: SetEnvironmentVariable, GetEnvironmentVariables, OSVersion |
| `EnvironmentVariableTarget.hpp` | `IGNORE` | Enum |
| `EventArgs.hpp` | `IGNORE` | Base class |
| `EventHandler.hpp` | `IGNORE` | Delegate typedef |
| `Exception.hpp` | `PARTIAL` | InnerException/Data/StackTrace added session 69; still missing: HResult, HelpLink |
| `ExecutionEngineException.hpp` | `IGNORE` | Exception marker |
| `FieldAccessException.hpp` | `IGNORE` | Exception marker |
| `FlagsAttribute.hpp` | `IGNORE` | Attribute marker |
| `FormatException.hpp` | `IGNORE` | Exception marker |
| `FormattableString.hpp` | `STUB` | Not really applicable in C++ |
| `Func.hpp` | `PORTED` | Typedef for std::function |
| `GC.hpp` | `STUB` | No-op stubs — intentional |
| `Guid.hpp` | `PORTED` | Parse, TryParse, ToString(format), op==, constructor from string added session 68 |
| `Half.hpp` | `TO_CHECK` | 16-bit float — needs audit |
| `HashCode.hpp` | `PORTED` | Add, ToHashCode, Combine 1–8 args — done session 69 |
| `IAsyncDisposable.hpp` | `IGNORE` | Interface |
| `IAsyncResult.hpp` | `IGNORE` | Interface |
| `ICloneable.hpp` | `IGNORE` | Interface |
| `IComparable.hpp` | `IGNORE` | Interface |
| `IConvertible.hpp` | `IGNORE` | Interface |
| `ICustomFormatter.hpp` | `IGNORE` | Interface |
| `IDisposable.hpp` | `IGNORE` | Interface |
| `IEquatable.hpp` | `IGNORE` | Interface |
| `IFormatProvider.hpp` | `IGNORE` | Interface |
| `IFormattable.hpp` | `IGNORE` | Interface |
| `Index.hpp` | `TO_CHECK` | Range index type |
| `IndexOutOfRangeException.hpp` | `IGNORE` | Exception marker |
| `InsufficientExecutionStackException.hpp` | `IGNORE` | Exception marker |
| `InsufficientMemoryException.hpp` | `IGNORE` | Exception marker |
| `Int128.hpp` | `TO_CHECK` | 128-bit int — GCC __int128 |
| `Int16.hpp` | `PARTIAL` | Has Parse/TryParse — missing `ToString(format)` |
| `Int32.hpp` | `PARTIAL` | Has Parse/TryParse — missing `ToString(format)`, `TryParse(span)` |
| `Int64.hpp` | `PARTIAL` | Has Parse/TryParse — missing `ToString(format)` |
| `IntPtr.hpp` | `TO_CHECK` | Pointer wrapper |
| `InvalidCastException.hpp` | `IGNORE` | Exception marker |
| `InvalidOperationException.hpp` | `IGNORE` | Exception marker |
| `InvalidProgramException.hpp` | `IGNORE` | Exception marker |
| `InvalidTimeZoneException.hpp` | `IGNORE` | Exception marker |
| `IParsable.hpp` | `IGNORE` | Interface |
| `IProgress.hpp` | `IGNORE` | Interface |
| `IServiceProvider.hpp` | `IGNORE` | Interface |
| `ISpanFormattable.hpp` | `IGNORE` | Interface |
| `ISpanParsable.hpp` | `IGNORE` | Interface |
| `Lazy.hpp` | `PORTED` | Thread-safe via std::call_once — complete |
| `Linq.hpp` | `PORTED` | 19 operators; header-only |
| `MarshalByRefObject.hpp` | `IGNORE` | Base class marker |
| `Math.hpp` | `PARTIAL` | Missing: `DivRem(long,long)`, `MaxMagnitude`, `MinMagnitude`, `BigMul(long,long,out long)` |
| `MathF.hpp` | `PARTIAL` | Missing: `IsFinite`, `IsNormal`, `IsSubnormal`, `ScaleB`, `DivRem` |
| `MemberAccessException.hpp` | `IGNORE` | Exception marker |
| `MethodAccessException.hpp` | `IGNORE` | Exception marker |
| `MidpointRounding.hpp` | `IGNORE` | Enum |
| `MissingFieldException.hpp` | `IGNORE` | Exception marker |
| `MissingMemberException.hpp` | `IGNORE` | Exception marker |
| `MissingMethodException.hpp` | `IGNORE` | Exception marker |
| `MulticastNotSupportedException.hpp` | `IGNORE` | Exception marker |
| `NotFiniteNumberException.hpp` | `IGNORE` | Exception marker |
| `NotImplementedException.hpp` | `PORTED` | Exception type used internally |
| `NotSupportedException.hpp` | `PORTED` | Exception type |
| `Nullable.hpp` | `PORTED` | Backed by std::optional, HasValue/Value/GetValueOrDefault — complete |
| `NullReferenceException.hpp` | `IGNORE` | Exception marker |
| `Object.hpp` | `PARTIAL` | Base class — minimal |
| `ObsoleteAttribute.hpp` | `IGNORE` | Attribute marker |
| `OperatingSystem.hpp` | `PORTED` | IsWindows/IsLinux/IsMacOS/Platform/Version/VersionString — complete |
| `OperationCanceledException.hpp` | `IGNORE` | Exception marker |
| `OutOfMemoryException.hpp` | `IGNORE` | Exception marker |
| `OverflowException.hpp` | `IGNORE` | Exception marker |
| `ParamArrayAttribute.hpp` | `IGNORE` | Attribute marker |
| `PlatformID.hpp` | `IGNORE` | Enum |
| `PlatformNotSupportedException.hpp` | `PORTED` | Used for platform guards |
| `Predicate.hpp` | `PORTED` | Typedef for std::function |
| `Progress.hpp` | `TO_CHECK` | IProgress<T> implementation |
| `Random.hpp` | `PARTIAL` | Missing: `NextInt64()`, `NextInt64(max)`, `NextInt64(min,max)` |
| `Range.hpp` | `TO_CHECK` | Range type |
| `RankException.hpp` | `IGNORE` | Exception marker |
| `ResolveEventArgs.hpp` | `IGNORE` | Event args |
| `ResolveEventHandler.hpp` | `IGNORE` | Delegate typedef |
| `SByte.hpp` | `PARTIAL` | Has Parse/TryParse — minor gaps |
| `Single.hpp` | `PARTIAL` | Has Parse/TryParse/IsNaN etc. — missing `ToString(format)` |
| `Span.hpp` | `STUB` | No meaningful Span impl in C++ static lib context |
| `StackOverflowException.hpp` | `IGNORE` | Exception marker |
| `String.hpp` | `PARTIAL` | Good coverage; missing `Join(string,T[])` for more types, `GetEnumerator`, `Normalize`, `IsInterned`, `Intern` (N/A) |
| `StringComparer.hpp` | `PORTED` | Ordinal/OrdinalIgnoreCase/InvariantCulture/CurrentCulture — complete |
| `StringComparison.hpp` | `IGNORE` | Enum |
| `StringSplitOptions.hpp` | `PORTED` | Enum with None/RemoveEmptyEntries/TrimEntries |
| `SystemException.hpp` | `IGNORE` | Exception base |
| `ThreadStaticAttribute.hpp` | `IGNORE` | Attribute marker |
| `TimeOnly.hpp` | `PORTED` | Full implementation |
| `TimeoutException.hpp` | `IGNORE` | Exception marker |
| `TimeSpan.hpp` | `PORTED` | Full — arithmetic, Parse, ToString |
| `TimeZone.hpp` | `STUB` | Obsolete in .NET; stub |
| `TimeZoneInfo.hpp` | `PORTED` | Local, UTC, FindSystemTimeZoneById (IANA) |
| `TimeZoneNotFoundException.hpp` | `IGNORE` | Exception marker |
| `Tuple.hpp` | `TO_CHECK` | Tuple<T1,T2,...> |
| `TypeAccessException.hpp` | `IGNORE` | Exception marker |
| `TypeCode.hpp` | `IGNORE` | Enum |
| `Type.hpp` | `STUB` | Very minimal — reflection not applicable |
| `TypeInitializationException.hpp` | `IGNORE` | Exception marker |
| `TypeLoadException.hpp` | `IGNORE` | Exception marker |
| `TypeUnloadedException.hpp` | `IGNORE` | Exception marker |
| `UInt128.hpp` | `TO_CHECK` | 128-bit uint |
| `UInt16.hpp` | `PARTIAL` | Has Parse/TryParse — minor gaps |
| `UInt32.hpp` | `PARTIAL` | Has Parse/TryParse — minor gaps |
| `UInt64.hpp` | `PARTIAL` | Has Parse/TryParse — minor gaps |
| `UIntPtr.hpp` | `TO_CHECK` | Pointer wrapper |
| `UnauthorizedAccessException.hpp` | `IGNORE` | Exception marker |
| `UnhandledExceptionEventArgs.hpp` | `IGNORE` | Event args |
| `UnhandledExceptionEventHandler.hpp` | `IGNORE` | Delegate typedef |
| `Uri.hpp` | `PORTED` | Full URI parsing |
| `ValueTuple.hpp` | `TO_CHECK` | Value tuple |
| `Version.hpp` | `PORTED` | Parse/TryParse/CompareTo/Equals added session 69 |
| `WeakReference.hpp` | `PORTED` | WeakReference + WeakReferenceT<T>, IsAlive/Target/TryGetTarget — complete |

---

## System/Buffers/

| File | Status | Notes |
|------|--------|-------|
| `ArrayPool.hpp` | `STUB` | Rent/Return stubs |
| `IMemoryOwner.hpp` | `IGNORE` | Interface |
| `OperationStatus.hpp` | `IGNORE` | Enum |
| `StandardFormat.hpp` | `STUB` | Format specifier — minimal |

---

## System/Collections/

| File | Status | Notes |
|------|--------|-------|
| `ArrayList.hpp` | `PORTED` | std::vector<any> wrapper |
| `BitArray.hpp` | `PARTIAL` | HasAllSet/HasAnySet/CopyTo(bool[])/CopyTo(byte[]) added session 69; And/Or/Xor/Not already done |
| `Comparer.hpp` | `PORTED` | Default comparer |
| `DictionaryEntry.hpp` | `PORTED` | Key/value pair |
| `Hashtable.hpp` | `PORTED` | unordered_map<string,any> |
| `ICollection.hpp` | `IGNORE` | Interface |
| `IComparer.hpp` | `IGNORE` | Interface |
| `IDictionary.hpp` | `IGNORE` | Interface |
| `IDictionaryEnumerator.hpp` | `IGNORE` | Interface |
| `IEnumerable.hpp` | `IGNORE` | Interface |
| `IEnumerator.hpp` | `IGNORE` | Interface |
| `IEqualityComparer.hpp` | `IGNORE` | Interface |
| `IList.hpp` | `IGNORE` | Interface |
| `IStructuralComparable.hpp` | `IGNORE` | Interface |
| `IStructuralEquatable.hpp` | `IGNORE` | Interface |
| `Queue.hpp` | `PORTED` | Non-generic Queue |
| `Stack.hpp` | `PORTED` | Non-generic Stack |

---

## System/Collections/Concurrent/

| File | Status | Notes |
|------|--------|-------|
| `ConcurrentDictionary.hpp` | `PARTIAL` | GetOrAdd, AddOrUpdate — TO_CHECK |
| `ConcurrentQueue.hpp` | `PORTED` | TryDequeue, TryPeek, Enqueue |
| `ConcurrentStack.hpp` | `PORTED` | TryPop, TryPopRange, Push |
| `IProducerConsumerCollection.hpp` | `IGNORE` | Interface |

---

## System/Collections/Generic/

| File | Status | Notes |
|------|--------|-------|
| `CollectionExtensions.hpp` | `PARTIAL` | GetValueOrDefault, TryAdd — TO_CHECK for more |
| `Comparer.hpp` | `PORTED` | Default comparer |
| `Dictionary.hpp` | `PARTIAL` | Missing: `EnsureCapacity`, `TrimExcess`, `Remove(key,out value)` |
| `EqualityComparer.hpp` | `PORTED` | Default() and Create() |
| `HashSet.hpp` | `PARTIAL` | Missing: `EnsureCapacity`, `TrimExcess`, `TryGetValue` (.NET 9) |
| `IAsyncEnumerable.hpp` | `IGNORE` | Interface |
| `ICollection.hpp` | `IGNORE` | Interface |
| `IComparer.hpp` | `IGNORE` | Interface |
| `IDictionary.hpp` | `IGNORE` | Interface |
| `IEnumerable.hpp` | `IGNORE` | Interface |
| `IEnumerator.hpp` | `IGNORE` | Interface |
| `IEqualityComparer.hpp` | `IGNORE` | Interface |
| `IList.hpp` | `IGNORE` | Interface |
| `IReadOnlyCollection.hpp` | `IGNORE` | Interface |
| `IReadOnlyDictionary.hpp` | `IGNORE` | Interface |
| `IReadOnlyList.hpp` | `IGNORE` | Interface |
| `IReadOnlySet.hpp` | `IGNORE` | Interface |
| `ISet.hpp` | `IGNORE` | Interface |
| `KeyNotFoundException.hpp` | `IGNORE` | Exception marker |
| `KeyValuePair.hpp` | `PORTED` | Simple struct |
| `LinkedList.hpp` | `PARTIAL` | LinkedListNode<T> added session 69; AddBefore/AddAfter/Find/FindLast/Remove(node) done |
| `List.hpp` | `PARTIAL` | Missing: `EnsureCapacity`, `TrimExcess`, `getCapacityProperty`, `ConvertAll`, `AsReadOnly`, `CopyTo` |
| `PriorityQueue.hpp` | `PORTED` | Enqueue, TryDequeue, TryPeek, Count |
| `Queue.hpp` | `PARTIAL` | Has TryDequeue/TryPeek; missing `EnsureCapacity` |
| `ReferenceEqualityComparer.hpp` | `PORTED` | Pointer-equality comparer |
| `SortedDictionary.hpp` | `PORTED` | Full parity with Dictionary |
| `SortedList.hpp` | `PORTED` | Full parity |
| `SortedSet.hpp` | `PORTED` | Full set algebra |
| `Stack.hpp` | `PARTIAL` | Has TryPop/TryPeek; missing `EnsureCapacity` |

---

## System/Collections/Immutable/

| File | Status | Notes |
|------|--------|-------|
| `ImmutableArray.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableDictionary.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableHashSet.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableList.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableQueue.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableSortedDictionary.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableSortedSet.hpp` | `PARTIAL` | TO_CHECK |
| `ImmutableStack.hpp` | `PARTIAL` | TO_CHECK |

---

## System/Collections/ObjectModel/

| File | Status | Notes |
|------|--------|-------|
| `Collection.hpp` | `PARTIAL` | TO_CHECK |
| `KeyedCollection.hpp` | `PARTIAL` | TO_CHECK |
| `ObservableCollection.hpp` | `PARTIAL` | TO_CHECK |
| `ReadOnlyCollection.hpp` | `PARTIAL` | TO_CHECK |
| `ReadOnlyDictionary.hpp` | `PARTIAL` | TO_CHECK |
| `ReadOnlyObservableCollection.hpp` | `PARTIAL` | TO_CHECK |
| `ReadOnlySet.hpp` | `PARTIAL` | TO_CHECK |

---

## System/Collections/Specialized/

| File | Status | Notes |
|------|--------|-------|
| `BitVector32.hpp` | `PORTED` | Bit operations |
| `HybridDictionary.hpp` | `PARTIAL` | TO_CHECK |
| `ListDictionary.hpp` | `PARTIAL` | TO_CHECK |
| `NameValueCollection.hpp` | `PORTED` | Get/GetValues/Add(collection) |
| `OrderedDictionary.hpp` | `PARTIAL` | TO_CHECK |
| `StringCollection.hpp` | `PARTIAL` | TO_CHECK |
| `StringDictionary.hpp` | `PARTIAL` | TO_CHECK |

---

## System/ComponentModel/

| File | Status | Notes |
|------|--------|-------|
| `Attribute.hpp` | `IGNORE` | Attribute base |
| `CategoryAttribute.hpp` | `IGNORE` | Attribute marker |
| `DataAnnotations/*` | `IGNORE` | Attribute markers |
| `DefaultValueAttribute.hpp` | `IGNORE` | Attribute marker |
| `DescriptionAttribute.hpp` | `IGNORE` | Attribute marker |
| `EditorBrowsableAttribute.hpp` | `IGNORE` | Attribute marker |
| `INotifyPropertyChanged.hpp` | `IGNORE` | Interface |
| `INotifyPropertyChanging.hpp` | `IGNORE` | Interface |
| `PropertyDescriptorCollection.hpp` | `STUB` | Minimal |
| `Win32Exception.hpp` | `IGNORE` | Exception marker |

---

## System/Diagnostics/

| File | Status | Notes |
|------|--------|-------|
| `CodeAnalysis/*` | `IGNORE` | Attribute markers |
| `Conditional/Debugger/Debuggable/etc. attributes` | `IGNORE` | Attribute markers |
| `Debug.hpp` | `PORTED` | Assert, Write, WriteLine |
| `Debugger.hpp` | `PARTIAL` | TO_CHECK |
| `StackFrame.hpp` | `PORTED` | File, line, col |
| `StackTrace.hpp` | `PORTED` | GetFrame, GetFrames |
| `Stopwatch.hpp` | `PORTED` | Start/Stop/Elapsed/Frequency/IsHighResolution |
| `Trace.hpp` | `PORTED` | Write/WriteLine/Assert |
| `UnreachableException.hpp` | `IGNORE` | Exception marker |

---

## System/Globalization/

| File | Status | Notes |
|------|--------|-------|
| `Calendar.hpp` | `PORTED` | Base calendar |
| `CalendarAlgorithmType.hpp` | `IGNORE` | Enum |
| `CalendarWeekRule.hpp` | `IGNORE` | Enum |
| `CharUnicodeInfo.hpp` | `PORTED` | GetDecimalDigitValue, GetNumericValue, GetUnicodeCategory |
| `CompareInfo.hpp` | `PORTED` | Compare, IsPrefix/IsSuffix, IndexOf, GetSortKey |
| `CompareOptions.hpp` | `IGNORE` | Enum |
| `CultureInfo.hpp` | `PARTIAL` | TO_CHECK |
| `CultureNotFoundException.hpp` | `IGNORE` | Exception marker |
| `CultureTypes.hpp` | `IGNORE` | Enum |
| `DateTimeFormatInfo.hpp` | `PORTED` | Invariant defaults, MonthNames, DayNames |
| `DateTimeStyles.hpp` | `IGNORE` | Enum |
| `DaylightTime.hpp` | `PARTIAL` | TO_CHECK |
| `DigitShapes.hpp` | `IGNORE` | Enum |
| `GregorianCalendar.hpp` | `PORTED` | Standard Gregorian |
| `GregorianCalendarTypes.hpp` | `IGNORE` | Enum |
| `HebrewCalendar.hpp` | `PORTED` | Lookup table 1583-2239 |
| `HijriCalendar.hpp` | `PORTED` | 30-year cycle algorithm |
| `IdnMapping.hpp` | `PORTED` | Punycode/IDNA RFC 3492 |
| `ISOWeek.hpp` | `PORTED` | ISO week number |
| `JapaneseCalendar.hpp` | `PORTED` | 5 eras |
| `JulianCalendar.hpp` | `PORTED` | Leap: year%4==0 |
| `KoreanCalendar.hpp` | `PORTED` | +2333 |
| `NumberFormatInfo.hpp` | `PARTIAL` | TO_CHECK |
| `NumberStyles.hpp` | `IGNORE` | Enum |
| `PersianCalendar.hpp` | `PORTED` | Solar Hijri, 12053-day cycle |
| `RegionInfo.hpp` | `STUB` | Minimal |
| `SortKey.hpp` | `PORTED` | GetSortKey |
| `SortVersion.hpp` | `STUB` | TO_CHECK |
| `StringInfo.hpp` | `PARTIAL` | TO_CHECK |
| `TaiwanCalendar.hpp` | `PORTED` | -1911 |
| `TextElementEnumerator.hpp` | `PORTED` | Unicode grapheme iteration |
| `TextInfo.hpp` | `PARTIAL` | TO_CHECK |
| `ThaiBuddhistCalendar.hpp` | `PORTED` | +543 |
| `TimeSpanStyles.hpp` | `IGNORE` | Enum |
| `UmAlQuraCalendar.hpp` | `PORTED` | Pre-computed 1318-1500 AH |
| `UnicodeCategory.hpp` | `IGNORE` | Enum |

---

## System/IO/

| File | Status | Notes |
|------|--------|-------|
| `BinaryReader.hpp` | `PARTIAL` | TO_CHECK |
| `BinaryWriter.hpp` | `PARTIAL` | TO_CHECK |
| `BufferedStream.hpp` | `STUB` | TO_CHECK |
| `Compression/CompressionMode.hpp` | `IGNORE` | Enum |
| `Compression/DeflateStream.hpp` | `PORTED` | Raw DEFLATE |
| `Compression/GZipStream.hpp` | `PORTED` | zlib |
| `Compression/ZipArchive.hpp` | `PORTED` | miniz |
| `Directory.hpp` | `PARTIAL` | TO_CHECK |
| `DirectoryInfo.hpp` | `PARTIAL` | TO_CHECK |
| `DirectoryNotFoundException.hpp` | `IGNORE` | Exception marker |
| `DriveInfo.hpp` | `PARTIAL` | GetDrives, DriveType |
| `EndOfStreamException.hpp` | `IGNORE` | Exception marker |
| `EnumerationOptions.hpp` | `IGNORE` | Options struct |
| `FileAccess.hpp` | `IGNORE` | Enum |
| `FileAttributes.hpp` | `IGNORE` | Enum |
| `File.hpp` | `PARTIAL` | TO_CHECK |
| `FileInfo.hpp` | `PARTIAL` | TO_CHECK |
| `FileLoadException.hpp` | `IGNORE` | Exception marker |
| `FileMode.hpp` | `IGNORE` | Enum |
| `FileNotFoundException.hpp` | `IGNORE` | Exception marker |
| `FileOptions.hpp` | `IGNORE` | Enum |
| `FileShare.hpp` | `IGNORE` | Enum |
| `FileStream.hpp` | `PARTIAL` | TO_CHECK |
| `FileStreamOptions.hpp` | `IGNORE` | Options struct |
| `HandleInheritability.hpp` | `IGNORE` | Enum |
| `Hashing/Crc32.hpp` | `TO_CHECK` | CRC32 |
| `Hashing/NonCryptographicHashAlgorithm.hpp` | `IGNORE` | Base class |
| `Hashing/XxHash32.hpp` | `PORTED` | Fast hash |
| `Hashing/XxHash64.hpp` | `PORTED` | Fast hash |
| `InvalidDataException.hpp` | `IGNORE` | Exception marker |
| `IOException.hpp` | `IGNORE` | Exception marker |
| `IsolatedStorage/IsolatedStorageException.hpp` | `IGNORE` | Exception marker |
| `IsolatedStorage/IsolatedStorageFile.hpp` | `PORTED` | Full file ops |
| `IsolatedStorage/IsolatedStorageFileStream.hpp` | `STUB` | |
| `IsolatedStorage/IsolatedStorage.hpp` | `IGNORE` | Base class |
| `IsolatedStorage/IsolatedStorageScope.hpp` | `IGNORE` | Enum |
| `MatchCasing.hpp` | `IGNORE` | Enum |
| `MatchType.hpp` | `IGNORE` | Enum |
| `MemoryStream.hpp` | `PARTIAL` | TO_CHECK |
| `Path.hpp` | `PARTIAL` | Combine, GetExtension, etc. — TO_CHECK |
| `PathTooLongException.hpp` | `IGNORE` | Exception marker |
| `RandomAccess.hpp` | `PORTED` | POSIX/Win32/Emscripten |
| `SearchOption.hpp` | `IGNORE` | Enum |
| `SearchTarget.hpp` | `IGNORE` | Enum |
| `SeekOrigin.hpp` | `IGNORE` | Enum |
| `Stream.hpp` | `PARTIAL` | TO_CHECK |
| `StreamReader.hpp` | `PARTIAL` | TO_CHECK |
| `StreamWriter.hpp` | `PARTIAL` | TO_CHECK |
| `StringReader.hpp` | `PARTIAL` | TO_CHECK |
| `StringWriter.hpp` | `PARTIAL` | TO_CHECK |
| `TextReader.hpp` | `PARTIAL` | TO_CHECK |
| `TextWriter.hpp` | `PARTIAL` | TO_CHECK |
| `UnixFileMode.hpp` | `IGNORE` | Enum |

---

## System/Net/

| File | Status | Notes |
|------|--------|-------|
| `Http/ByteArrayContent.hpp` | `PORTED` | |
| `Http/FormUrlEncodedContent.hpp` | `PORTED` | |
| `Http/HttpClient.hpp` | `PORTED` | HTTP/1.1; no TLS |
| `Http/HttpContent.hpp` | `PORTED` | Base class |
| `Http/HttpMethod.hpp` | `PORTED` | GET/POST/PUT/DELETE/etc. |
| `Http/HttpRequestMessage.hpp` | `PORTED` | |
| `Http/HttpResponseMessage.hpp` | `PORTED` | |
| `Http/StringContent.hpp` | `PORTED` | |
| `HttpStatusCode.hpp` | `PORTED` | Standard HTTP codes |
| `IPAddress.hpp` | `PORTED` | Parse, ToString |
| `IPEndPoint.hpp` | `PORTED` | Address + Port |
| `Sockets/NetworkStream.hpp` | `PORTED` | POSIX+Winsock2 |
| `Sockets/TcpClient.hpp` | `PORTED` | POSIX+Winsock2 |
| `Sockets/UdpClient.hpp` | `PORTED` | POSIX+Winsock2 |
| `WebUtility.hpp` | `PARTIAL` | HtmlEncode/Decode, UrlEncode/Decode — TO_CHECK |

---

## System/Numerics/

| File | Status | Notes |
|------|--------|-------|
| `BFloat16.hpp` | `PORTED` | 16-bit brain float |
| `BigInteger.hpp` | `PORTED` | Full arithmetic, Knuth D |
| `BitOperations.hpp` | `PORTED` | C++20 countl_zero, popcount, etc. |
| `Colors/Argb.hpp` | `PORTED` | Constructor, CopyTo, Equals, ToRgba |
| `Colors/Colors.hpp` | `PORTED` | |
| `Colors/Rgba.hpp` | `PORTED` | Constructor, CopyTo, Equals, ToArgb |
| `Complex.hpp` | `TO_CHECK` | Complex number |
| `DivisionRounding.hpp` | `IGNORE` | Enum |
| `GenericMathInterfaces.hpp` | `IGNORE` | Interfaces |
| `Matrix3x2.hpp` | `PORTED` | Full transforms, Invert |
| `Matrix4x4.hpp` | `PORTED` | Full transforms, Invert |
| `Plane.hpp` | `PORTED` | CreateFromVertices, Dot, Normalize, Transform |
| `Quaternion.hpp` | `PORTED` | Slerp, CreateFrom*, Conjugate, Inverse |
| `Vector2.hpp` | `PORTED` | Constants, Dot, Normalize, Lerp, Clamp |
| `Vector3.hpp` | `PORTED` | + Cross |
| `Vector4.hpp` | `PORTED` | Full |

---

## System/Runtime/

| File | Status | Notes |
|------|--------|-------|
| `AmbiguousImplementationException.hpp` | `IGNORE` | Exception marker |
| `CompilerServices/*` | `IGNORE` | Attribute markers |
| `GCSettings.hpp` | `STUB` | |
| `InteropServices/*` | `IGNORE` | Attribute markers |
| `Versioning/*` | `IGNORE` | Attribute markers |

---

## System/Text/

| File | Status | Notes |
|------|--------|-------|
| `ASCIIEncoding.hpp` | `PARTIAL` | TO_CHECK |
| `Ascii.hpp` | `PORTED` | IsValid, ToUpper/Lower, Trim, EqualsIgnoreCase |
| `CompositeFormat.hpp` | `STUB` | |
| `Decoder.hpp` | `PARTIAL` | TO_CHECK |
| `DecoderFallback.hpp` | `IGNORE` | Abstract base |
| `Encoder.hpp` | `PARTIAL` | TO_CHECK |
| `EncoderFallback.hpp` | `IGNORE` | Abstract base |
| `Encoding.hpp` | `PARTIAL` | TO_CHECK |
| `EncodingInfo.hpp` | `PARTIAL` | TO_CHECK |
| `EncodingProvider.hpp` | `IGNORE` | Abstract base |
| `Encodings/Web/HtmlEncoder.hpp` | `PARTIAL` | TO_CHECK |
| `Encodings/Web/JavaScriptEncoder.hpp` | `PARTIAL` | TO_CHECK |
| `Encodings/Web/UrlEncoder.hpp` | `PARTIAL` | TO_CHECK |
| `Json/JsonDocument.hpp` | `PORTED` | Backed by nlohmann |
| `Json/JsonElement.hpp` | `PORTED` | Full access |
| `Json/JsonSerializer.hpp` | `PORTED` | Serialize/Deserialize |
| `Json/JsonSerializerOptions.hpp` | `PARTIAL` | Basic options |
| `Json/JsonValueKind.hpp` | `IGNORE` | Enum |
| `Json/Serialization/*` | `IGNORE` | Attribute markers |
| `Latin1Encoding.hpp` | `PARTIAL` | TO_CHECK |
| `NormalizationForm.hpp` | `IGNORE` | Enum |
| `RegularExpressions/Match.hpp` | `PARTIAL` | TO_CHECK |
| `RegularExpressions/MatchCollection.hpp` | `PARTIAL` | TO_CHECK |
| `RegularExpressions/Regex.hpp` | `PARTIAL` | Basic — no named groups |
| `Rune.hpp` | `PARTIAL` | TO_CHECK |
| `StringBuilder.hpp` | `PARTIAL` | Missing: `Append(char,count)` ✅ done; `getCapacityProperty`, `EnsureCapacity`, `Append(uint/ushort/sbyte)`, `AppendFormat(3-arg)`, char indexer |
| `UnicodeEncoding.hpp` | `PARTIAL` | TO_CHECK |
| `Unicode/UnicodeRange.hpp` | `PARTIAL` | TO_CHECK |
| `Unicode/UnicodeRanges.hpp` | `PARTIAL` | TO_CHECK |
| `UTF32Encoding.hpp` | `PARTIAL` | TO_CHECK |
| `UTF7Encoding.hpp` | `STUB` | Obsolete in .NET |
| `UTF8Encoding.hpp` | `PARTIAL` | TO_CHECK |

---

## System/Threading/

| File | Status | Notes |
|------|--------|-------|
| `AbandonedMutexException.hpp` | `IGNORE` | Exception marker |
| `ApartmentState.hpp` | `IGNORE` | Enum |
| `AsyncLocal.hpp` | `TO_CHECK` | |
| `AutoResetEvent.hpp` | `PARTIAL` | TO_CHECK |
| `Barrier.hpp` | `PARTIAL` | TO_CHECK |
| `CancellationToken.hpp` | `PARTIAL` | TO_CHECK |
| `CancellationTokenSource.hpp` | `PARTIAL` | TO_CHECK |
| `CountdownEvent.hpp` | `PARTIAL` | TO_CHECK |
| `EventResetMode.hpp` | `IGNORE` | Enum |
| `EventWaitHandle.hpp` | `PARTIAL` | TO_CHECK |
| `Interlocked.hpp` | `PARTIAL` | TO_CHECK — missing CompareExchange, Exchange |
| `LazyInitializer.hpp` | `PARTIAL` | TO_CHECK |
| `LazyThreadSafetyMode.hpp` | `IGNORE` | Enum |
| `Lock.hpp` | `PARTIAL` | TO_CHECK |
| `LockRecursionException.hpp` | `IGNORE` | Exception marker |
| `LockRecursionPolicy.hpp` | `IGNORE` | Enum |
| `ManualResetEvent.hpp` | `PARTIAL` | TO_CHECK |
| `ManualResetEventSlim.hpp` | `PARTIAL` | TO_CHECK |
| `Monitor.hpp` | `PARTIAL` | Enter/Exit — TO_CHECK |
| `Mutex.hpp` | `PARTIAL` | TO_CHECK |
| `PeriodicTimer.hpp` | `PORTED` | WaitForNextTickAsync |
| `ReaderWriterLockSlim.hpp` | `PARTIAL` | TO_CHECK |
| `Semaphore.hpp` | `PARTIAL` | TO_CHECK |
| `SemaphoreFullException.hpp` | `IGNORE` | Exception marker |
| `SemaphoreSlim.hpp` | `PARTIAL` | TO_CHECK |
| `SpinLock.hpp` | `PARTIAL` | TO_CHECK |
| `SpinWait.hpp` | `PARTIAL` | TO_CHECK |
| `SynchronizationContext.hpp` | `PARTIAL` | TO_CHECK |
| `SynchronizationLockException.hpp` | `IGNORE` | Exception marker |
| `Tasks/Parallel.hpp` | `PORTED` | For/ForEach/MaxDegreeOfParallelism |
| `Tasks/Task.hpp` | `PORTED` | async/await pattern |
| `Tasks/TaskCompletionSource.hpp` | `PARTIAL` | TO_CHECK |
| `Tasks/ValueTask.hpp` | `STUB` | |
| `Thread.hpp` | `PORTED` | Start/Join/IsAlive/ManagedThreadId |
| `ThreadAbortException.hpp` | `IGNORE` | Exception marker |
| `ThreadExceptionEventArgs.hpp` | `IGNORE` | Event args |
| `ThreadInterruptedException.hpp` | `IGNORE` | Exception marker |
| `ThreadLocal.hpp` | `PARTIAL` | TO_CHECK |
| `ThreadPool.hpp` | `PORTED` | QueueUserWorkItem |
| `ThreadPriority.hpp` | `IGNORE` | Enum |
| `ThreadStart.hpp` | `IGNORE` | Delegate typedef |
| `ThreadStateException.hpp` | `IGNORE` | Exception marker |
| `ThreadState.hpp` | `IGNORE` | Enum |
| `Timeout.hpp` | `PARTIAL` | TO_CHECK |
| `Timer.hpp` | `PORTED` | Callback, Change |
| `Volatile.hpp` | `PARTIAL` | Read/Write — TO_CHECK |
| `WaitHandle.hpp` | `PARTIAL` | TO_CHECK |
| `WaitHandleCannotBeOpenedException.hpp` | `IGNORE` | Exception marker |

---

## System/Xml/

| File | Status | Notes |
|------|--------|-------|
| `Linq/XAttribute.hpp` | `PORTED` | |
| `Linq/XDocument.hpp` | `PORTED` | |
| `Linq/XElement.hpp` | `PORTED` | |
| `Linq/XName.hpp` | `PORTED` | |
| `XmlReader.hpp` | `PORTED` | tinyxml2 DOM cursor |
| `XmlWriter.hpp` | `PORTED` | tinyxml2 DOM builder |

---

## Priority queue for TO_CHECK / PARTIAL files (game dev relevance)

### HIGH priority — ✅ ALL DONE (session 68)
1. `Random::NextInt64` — ✅ done session 68
2. `Math::DivRem(long)/MaxMagnitude/MinMagnitude` — ✅ done session 68
3. `MathF::IsFinite/IsNormal/IsSubnormal/ScaleB` — ✅ done session 68 (+IsNegative/MaxMagnitude/MinMagnitude)
4. `List<T>::EnsureCapacity/TrimExcess/ConvertAll/AsReadOnly` — ✅ done session 68
5. `Dictionary<K,V>::Remove(key,val)/EnsureCapacity/TrimExcess` — ✅ done session 68
6. `HashSet<T>::EnsureCapacity/TrimExcess` — ✅ done session 68
7. `Char` ASCII helpers — ✅ done session 68
8. `Environment::GetEnvironmentVariable/MachineName/UserName` — ✅ done session 68
9. `Array` functional methods — ✅ done session 68
10. `Guid::Parse/TryParse/ToString(format)` — ✅ done session 68

### MEDIUM priority — ✅ ALL DONE (session 69)
- `StringBuilder::AppendFormat(3-arg)/EnsureCapacity` — ✅ done session 68
- `Int32/Int64/Double/Single::ToString(format)` — ✅ done session 68
- `String::Empty/ToUpperInvariant/CompareOrdinal/Format(float)` — ✅ done session 68
- `LinkedList` node-based API (`LinkedListNode<T>`, AddBefore/AddAfter/Find/FindLast/Remove(node)) — ✅ done session 69 (Task 107)
- `Version::Parse/TryParse/CompareTo/Equals` — ✅ done session 69 (Task 107)
- `HashCode::Combine` 5–8 args — ✅ done session 69 (Task 107)
- `Exception::InnerException/Data/StackTrace`, `AggregateException::Flatten()` — ✅ done session 69 (Task 108)
- `Byte/SByte/Int16/UInt16::ToString(format)` — ✅ done session 69 (Task 109)
- `DateTimeOffset` full arithmetic/Parse/ToString/comparison — ✅ done session 69 (Task 110)
- `Buffer::BlockCopy/ByteLength/GetByte/SetByte` — ✅ already implemented (marked TO_CHECK, now PORTED)

### LOW priority (not relevant for game dev)
- `ConcurrentDictionary` detailed audit
- `Immutable*` collections detailed audit  
- Threading primitives detailed audit
- `System.IO.*` detailed stream audit
