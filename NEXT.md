# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-07-04 (branch: feature/work) — 9051 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting — driven by a `plan.sqlite3` namespace review workflow. **As of 2026-07-02 this workflow is fully autonomous** (see §3a) — no more per-item user confirmation. Currently working through the `System` namespace alphabetically.
- **Header count:** ~600 `.hpp` files across `System`, `System.Collections`, `System.IO`, `System.Text`, `System.Threading`, `System.Net`, `System.Numerics`, `System.Diagnostics`, `System.Globalization`, `System.Xml`, `System.Buffers`, etc.
- **Test file count:** ~299 GoogleTest `.cpp` files.
- **Key architectural decisions:** No runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (int32_t), `bytecs` (uint8_t), etc. Inner exceptions use `std::exception_ptr`.

---

## 2. Current status

### Build
- **Clean.** `cmake --build build --parallel 4` produces zero errors, zero warnings.

### Tests
- **9051 tests passing** across 897 test suites. Zero failures.

### What works
- Core types: `String`, `Object`, `Boolean`, `Byte`, `Char`, `Int16`, `Int32`, `Int64`, `Int128`, `IntPtr`, `UInt16`, `UInt64`, `UInt128`, `Half` (full checklist port — correct round-to-nearest-even `FromSingle`/subnormal `ToSingle`, `NaN`/`E`/`Pi`/`Tau`/`One`/`NegativeOne`/`NegativeZero` constants, `IsNormal`/`IsSubnormal`, full arithmetic operators, `Parse`/`TryParse`, `ToString(format)`, `TryFormat`), `Single`, `Double`, `Decimal` (+ OACurrency), `Guid` (full checklist port — fixed `ToByteArray()`/byte-array-ctor endianness bug, added `X` format, `ParseExact`/`TryParseExact`, `Variant`/`Version`, `CreateVersion7`, span-based Parse/TryParse/TryFormat/TryWriteBytes), `BitConverter` (full API including Half/BFloat16/Int128/UInt128), `Math` (full overloads + BigMul/DivRem/ILogB), `MathF`, `Random`, `HashCode` (full checklist port — per-process random seed like .NET, `AddBytes(ReadOnlySpan<byte>)`), `Void`, `Index`, `Lazy<T>`
- Delegates/Events: `Func<T>` (full `FuncT..FuncT16` arities), `Action`/`ActionT..ActionT16`, `Converter<T,R>`, `EventHandler<T>`, `EventArgs`, `Delegate`
- Attributes: `Attribute`, `FlagsAttribute`, `ObsoleteAttribute`, `SerializableAttribute`, `CLSCompliantAttribute`
- Enums: `Casing`, `CrashReason`, `GCCollectionMode`, `GCKind`, `GCLatencyMode`, `GCNotificationStatus`, `EnvironmentVariableTarget`, `MidpointRounding`
- Formatting: `FormattableString`, `FormattableStringFactory`, `IFormatProvider`, `IFormattable` (now includes the `ToString(format, provider)` overload), `ISpanFormattable` (now includes the `TryFormat(..., provider)` overload), `IUtf8SpanFormattable` (fixed: was wrongly generic over `<TSelf>`, now matches .NET's non-generic interface; added the `provider` overload), `ICustomFormatter`
- Interfaces: `IAsyncDisposable` (`DisposeAsync()` returns `ValueTask`), `IAsyncResult` (full 4-member surface incl. `AsyncState`/`AsyncWaitHandle`), `ICloneable`, `IComparable<T>`, `IConvertible` (full surface incl. `ToDecimal`/`ToDateTime`), `IDisposable`, `IEquatable<T>`, `IObservable<T>`, `IObserver<T>` (both now document the C++-template variance gap), `IParsable<T>`, `IProgress<T>` (documents variance gap), `IServiceProvider`, `ISpanParsable<T>`, `IUtf8SpanParsable<T>` (fixed: was taking a mutable `Span<byte>` instead of `ReadOnlySpan<byte>`; added the `provider` overload)
- Time: `DateTime`, `DateTimeOffset`, `DateOnly`, `TimeOnly`, `TimeSpan`, `TimeZoneInfo`, `TimeProvider`, `Stopwatch`
- Exceptions: full hierarchy — all `std::exception_ptr` inner-exception ctors, `HResult`/`Source`/`HelpLink` on the `Exception` base, `HResult` now correctly set per-type on `ExecutionEngineException`/`FieldAccessException`/`FormatException` (was previously inheriting the base `COR_E_EXCEPTION` default — audit other exception types for the same gap, see §5), all `/** */` Doxygen on all types
- Collections (non-generic): full namespace done — `ArrayList`, `BitArray` (+Length setter, And/Or/Xor length validation), `Hashtable`, `Queue`/`Stack` (real GetEnumerator, InvalidOperationException on empty), `Comparer`, `DictionaryEntry`, `ListDictionaryInternal` (real Keys/Values/GetEnumerator), `IList`, `ICollection`, `IComparer`, `IDictionary`, `IEnumerable`, `IEnumerator`, `IDictionaryEnumerator`, `IEqualityComparer`, `IStructuralComparable`, `IStructuralEquatable`, `StructuralComparisons`
- Collections.Concurrent: full namespace done — `ConcurrentDictionary<K,V>`, `ConcurrentQueue<T>`/`ConcurrentStack<T>` (now implement `IProducerConsumerCollection<T>`), `IProducerConsumerCollection<T>`, `EnumerablePartitionerOptions`
- Collections.Frozen: `FrozenDictionary<K,V>`, `FrozenSet<T>` (public API surface; internal SIMD-optimized implementation types out of scope)
- Collections (generic): full namespace done (37 types) — `List<T>` (real bounds validation, was silent UB), `Dictionary<K,V>`, `Queue<T>`, `Stack<T>`, `LinkedList<T>`/`LinkedListNode<T>`, `SortedList<K,V>`, `SortedDictionary<K,V>`, `HashSet<T>` (+TryGetValue/RemoveWhere/Overlaps), `SortedSet<T>`, `OrderedDictionary<K,V>`, `PriorityQueue<T,P>`, `ReadOnlyCollection<T>`, `ArraySegment<T>`, `CollectionExtensions`, `Comparer<T>`/`EqualityComparer<T>` (now implements `IEqualityComparer<T>`)/`NullableComparer<T>`/`NullableEqualityComparer<T>`/`ObjectComparer<T>`/`ObjectEqualityComparer<T>`/`ReferenceEqualityComparer<T>`/`NonRandomizedStringEqualityComparer`, `KeyValuePair<K,V>`, `KeyNotFoundException`, all `I*` interfaces (`ICollection<T>`, `IComparer<T>`, `IDictionary<K,V>`, `IEnumerable<T>`, `IEnumerator<T>`, `IEqualityComparer<T>`, `IList<T>`, `IReadOnlyCollection<T>`, `IReadOnlyDictionary<K,V>`, `IReadOnlyList<T>`, `IReadOnlySet<T>`, `ISet<T>`, `IAsyncEnumerable<T>`, `IAsyncEnumerator<T>`)
- Collections.Immutable: full namespace done (13 types) — `ImmutableArray/List<T>` (real bounds validation, was silent UB), `ImmutableDictionary/SortedDictionary<K,V>`, `ImmutableHashSet/SortedSet<T>` (Min/Max no longer UB on empty), `ImmutableQueue/Stack<T>` (InvalidOperationException on empty, was std::out_of_range), `IImmutableDictionary/List/Queue/Set/Stack<T>`
- Collections.ObjectModel: 2 of 7 items reviewed/fixed — `Collection<T>` (real bounds validation, was silent UB), `ReadOnlyCollection<T>` (NotSupportedException on mutation, was std::runtime_error). Remaining: `KeyedCollection`, `ObservableCollection`, `ReadOnlyDictionary`, `ReadOnlyObservableCollection`, `ReadOnlySet`
- Span/Memory: `Span<T>`, `ReadOnlySpan<T>`, `Memory<T>`, `ReadOnlyMemory<T>`, `MemoryExtensions` (full), `SpanSplitEnumerator`
- Buffers: `ArrayPool<T>`, `MemoryPool<T>`, `MemoryHandle`, `IPinnable`, `MemoryManager<T>`, `IBufferWriter<T>`, `ArrayBufferWriter<T>`, `SearchValues<T>`, `SequencePosition`, `ReadOnlySequence<T>`, `ReadOnlySequenceSegment<T>`, `SequenceReader<T>`, `SequenceReaderExtensions`, `BinaryPrimitives` (full, incl. Single/Double + TryRead*/TryWrite* family), `BuffersExtensions`, `StandardFormat`
- Buffers.Text: `Base64` (full modern API incl. InPlace/Chars/whitespace-aware decode), `Base64Url` (same), `Utf8Formatter`/`Utf8Parser` (bool + integers only — see §5 for the documented Guid/DateTime/TimeSpan/Decimal/float gap)
- IO: `Stream`, `FileStream`, `MemoryStream`, `BinaryReader`, `BinaryWriter`, `StreamReader`, `StreamWriter`, `TextReader`, `TextWriter`, `File`, `Directory`, `Path`, `FileInfo`, `DirectoryInfo`, `RandomAccess`
- IO.Compression: `ZipArchive`, `ZipArchiveEntry`, `ZipFile`, `DeflateStream`, `GZipStream`
- IO.Hashing: `Crc32`, `Crc64`, `XxHash32`, `XxHash64`, `XxHash3`, `XxHash128`
- Text: `StringBuilder`, `Encoding` (UTF-8/16/32/ASCII), `Rune`, `Unicode.*`, `FormattableString`
- Text.Json: `JsonSerializer`, `JsonElement`, `JsonDocument`, `Utf8JsonReader`, `Utf8JsonWriter`
- Threading: `Thread`, `ThreadPool`, `Monitor`, `Mutex`, `SemaphoreSlim`, `AutoResetEvent`, `ManualResetEvent`, `ManualResetEventSlim`, `Interlocked`, `CancellationToken/Source`, `Barrier`, `CountdownEvent`, `Lock`, `AsyncLocal<T>`, `LazyInitializer`, `WaitHandle`, `EventWaitHandle`
- Threading.Tasks: `Task`, `Task<T>`, `ValueTask`, `ValueTask<T>`, `TaskCompletionSource<T>`
- Numerics: `BigInteger`, `Complex`, `BFloat16`, `Vector2/3/4`, `Matrix3x2`, `Matrix4x4`, `Quaternion`, `Plane`
- Diagnostics: `Debug`, `Trace`, `Stopwatch`, `DiagnosticListener`, `Activity`
- Globalization: `CultureInfo`, `DateTimeFormatInfo`, `NumberFormatInfo`, `TextInfo`, `IdnMapping`, `Calendar` types, `CompareInfo`, `RegionInfo`
- Net: `IPAddress`, `IPEndPoint`, `HttpStatusCode`, `HttpMethod`, `Uri`, sockets (POSIX-only)
- Net.Http: `HttpClient`, `HttpRequestMessage`, `HttpResponseMessage`, `HttpContent` (no TLS)
- Xml: `XmlReader`, `XmlWriter`, `XmlDocument`, `XElement`, `XDocument` (via tinyxml2)
- Runtime handles: `RuntimeTypeHandle`, `RuntimeMethodHandle`, `RuntimeFieldHandle`, `ModuleHandle`, `ValueType`
- Other: `Environment` (full), `AppDomain`, `AppContext`, `GC` (stubs, now with full overload surface — see recent changes), `DBNull` (full `IConvertible`), `Delegate`, `Nullable<T>`, `WeakReference`, `BinaryData`, `String.Intern/IsInterned`, `Convert`, `Enum` (stub)

### What does NOT work
- `Regex` — `std::regex` back-end; no named groups, no lookbehind.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` — POSIX-only; will not compile on Windows without Winsock2 path.
- `SynchronizationContext` — stub; `Progress<T>` calls handlers synchronously.
- `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; not implemented.
- `CopyTo(Array, int)` on `ICollection`/`BitArray`/`ArrayList` — `System.Array` type does not exist.
- `ArrayList.GetEnumerator()` — returns `nullptr`; non-generic enumerator over `std::any` not yet implemented.
- Windows / Emscripten cross-compilation — untested; POSIX guards exist but not CI-validated.

---

## 3. Recent changes

All on branch `feature/work` (not yet pushed), most recent first:

| Commit | Change |
|--------|--------|
| `9128b2a` | Port Collections.ObjectModel Collection/ReadOnlyCollection (2 of 7 items): **Collection\<T\> Insert/RemoveAt/SetItem/indexer had zero bounds validation (silent UB)** — added requireIndexInRange throwing ArgumentOutOfRangeException; both types' CopyTo threw std::out_of_range instead of the correct ArgumentOutOfRangeException/ArgumentException split; ReadOnlyCollection's mutating methods threw std::runtime_error instead of System::NotSupportedException("Collection is read-only.") per .NET's NotSupported_ReadOnlyCollection resource string; int→intcs throughout. Remaining ObjectModel items (KeyedCollection, ObservableCollection, ReadOnlyDictionary, ReadOnlyObservableCollection, ReadOnlySet) not yet reviewed |
| `f1e7559` | Port System.Collections.Immutable (13 items): same bug classes as Generic — Immutable Queue/Stack threw std::out_of_range instead of InvalidOperationException; Dictionary/SortedDictionary const indexer/Add wrong exception types; **ImmutableList/ImmutableArray Insert/InsertRange/SetItem/RemoveAt/RemoveRange had zero bounds validation (silent UB)**; ImmutableSortedSet.Min/Max UB on empty. Completes System.Collections.Immutable |
| `6c8e4c6` | Port SortedDictionary/SortedList/SortedSet (Generic): wrong exception types (KeyNotFoundException/ArgumentException/ArgumentOutOfRangeException), SortedList missing const indexer, **SortedSet.Min/Max dereferenced begin()/rbegin() unconditionally — UB on an empty set** — now returns default(T) matching .NET; int→intcs. Completes System.Collections.Generic (37 items) |
| `e6e8e89` | Port PriorityQueue (Generic): wrong exception type on empty Dequeue/Peek (std::invalid_argument → InvalidOperationException) |
| `4c071e3` | Port OrderedDictionary (Generic): wrong exception types throughout (KeyNotFoundException/ArgumentException/ArgumentOutOfRangeException), int→intcs |
| `c1bf8c6` | Port LinkedList/LinkedListNode (Generic): CopyTo had zero bounds validation — out-of-range index/too-small destination was silent UB |
| `9a50a38` | Port List\<T\> (Generic): **Insert/RemoveAt/RemoveRange/Reverse/InsertRange had zero bounds validation — silent UB on bad indices**; operator[] used std::out_of_range instead of ArgumentOutOfRangeException; int→intcs across the whole (large) file |
| `59a7c7d`, `33a7284` | Port Queue\<T\>/Stack\<T\>/Dictionary\<K,V\>/HashSet\<T\> (Generic): wrong exception types (std::runtime_error/std::invalid_argument/std::out_of_range → real .NET types); added HashSet.TryGetValue/RemoveWhere/Overlaps (missing entirely) |
| `bce3f43` | Port Collections.Generic interfaces/comparers: **EqualityComparer\<T\> didn't implement IEqualityComparer\<T\> at all** — fixed inheritance; int→intcs and size_t→intcs (GetHashCode) across ICollection/IComparer/IDictionary/IEqualityComparer/IList/IReadOnlyCollection/IReadOnlyList/Comparer/EqualityComparer/NullableComparer/NullableEqualityComparer/ObjectComparer/ObjectEqualityComparer/ReferenceEqualityComparer/NonRandomizedStringEqualityComparer |
| `d98b7e1` | Port Frozen.FrozenDictionary/FrozenSet: CopyTo silently grew the destination instead of throwing when too small |
| `8e551e3` | Port Collections.Concurrent: ConcurrentQueue\<T\>/ConcurrentStack\<T\> **didn't implement IProducerConsumerCollection\<T\> at all** — added TryAdd/TryTake/ToArray/CopyTo/GetEnumerator; ConcurrentStack.CopyTo silently grew the destination instead of throwing |
| `56aa48c` | Port Collections (non-generic) interfaces + BitArray/Queue/Stack/ListDictionaryInternal: **Queue/Stack.GetEnumerator() returned nullptr unconditionally** (crash on first use); wrong exception types; BitArray And/Or/Xor had no length validation (OOB read); DictionaryEntry.ToString() printed type names instead of values |
| `d84869d` | Port Utf8Formatter/Utf8Parser: **format specifier was silently ignored for all integer types** on both format and parse sides (e.g. requesting hex 'X' produced plain decimal instead of throwing or formatting hex) — added real G/D/N/X dispatch with correct precision/zero-padding (D/X) and thousands-grouping (N, 2 decimals by default matching .NET's NumberFormatInfo default) semantics; bool now validates its format symbol and throws FormatException like .NET; all TryParse overloads now correctly set bytesConsumed=0 on failure (previously left stale); Guid/DateTime/DateTimeOffset/TimeSpan/Decimal/float TryFormat/TryParse intentionally deferred — documented gap, see §5 |
| `f3bce47` | Port Base64Url: mirrors the Base64 whitespace-decode fix; **GetMaxDecodedLength used a loose `(len*3+3)/4` upper-bound approximation** instead of .NET's exact `whole*3+(remainder-1)` formula (fixed a pre-existing test that had locked in the wrong value); added the full modern API surface (InPlace, Chars, byte[]/vector-returning, Try* variants) |
| `9e0bd35` | Port Base64: **DecodeFromUtf8/IsValid rejected any embedded whitespace** and **an incomplete trailing group with isFinalBlock=true silently returned Done instead of InvalidData** (silently dropping the unconsumed tail) — rewrote decode/validate as a whitespace-skipping state machine; added the full modern API surface (InPlace, Chars, byte[]/vector-returning, Try* variants) |
| `4a46b24` | Port BinaryPrimitives: added the entire missing TryRead*/TryWrite* bool-returning family (previously only throwing Read*/Write* existed) plus Single/Double support; int→intcs on checkSize |
| `becdf7d` | Port Tuple + TupleExtensions: **added missing CompareTo/operator<** — Tuple1..Tuple7 had operator==/!= but no ordering at all, unlike .NET's IStructuralComparable-based sequential compare; added intcs-returning CompareTo + operator< using the same first-non-zero-wins algorithm; fixed int→intcs on GetHashCode and hash helpers |
| `32f8bbb` | Port TypeLoadException/TypeAccessException/TypeInitializationException: fixed missing HResults (COR_E_TYPELOAD/TYPEACCESS/TYPEINITIALIZATION); TypeAccessException's default message corrected to match .NET; **TypeInitializationException took a raw `const std::exception*` instead of this codebase's std::exception_ptr convention** and manually embedded inner->what() text into the message instead of exposing it via InnerException like .NET — fixed both |
| `0af23c9` | Port TimeoutException: fixed missing HResult (COR_E_TIMEOUT) |
| `4f9d08c` | Port TimeProvider: fixed exception message text to match .NET's actual resource string |
| `00d55a0` | Port TimeZoneInfo + TransitionTime: **fixed wrong exception type** — FindSystemTimeZoneById threw std::invalid_argument/PlatformNotSupportedException for an unknown zone instead of TimeZoneNotFoundException (which already existed unused); **fixed SupportsDaylightSavingTime** — it read tm_isdst at the current instant ("is DST active right now") instead of .NET's "does this zone's rules include DST at all" (e.g. America/New_York reported false in January); added a Jan/Jul offset-sampling check; int→intcs across TransitionTime/AdjustmentRule |
| `d351849` | Port TimeSpan: **fixed wrong exception type** — FromDays/FromHours/etc. and operator*//  threw ArgumentOutOfRangeException for NaN instead of .NET's plain ArgumentException; replaced ~7 placeholder exception messages (raw resource-key strings) with real .NET text; added entirely-missing GetHashCode(); int→intcs |
| `a79999b` | Port TimeOnly: **fixed IsBetween** — treated the end of the range as inclusive and returned true when start==end, but .NET's IsBetween is end-exclusive and always false when start==end; rewrote using .NET's own unsigned-wraparound tick arithmetic; added missing ctor bounds validation (hour/minute/second/millisecond/ticks); AddHours/AddMinutes now take double (was int, so fractional adds were unsupported) |
| `5629d4b` | Port StringComparer: fixed int→intcs and GetHashCode return type |
| `a3044d6` | Port String: **added missing bounds validation** to Substring/Remove/IndexOf/LastIndexOf — these silently clamped out-of-range length/count via std::string::substr/erase/find instead of throwing like .NET (e.g. `Substring("abcde", 2, 100)` returned `"cde"` instead of throwing); fixed int→intcs and GetHashCode return type |
| `56b68a4` | Port SystemException: fixed missing HResult; also fixed ArithmeticException's missing HResult (discovered as a side effect) |
| `ee28e91` | Port StackOverflowException: fixed missing HResult, marked `final` (sealed in .NET), corrected default message to match .NET's actual resource string |
| `c389cbd` | Port Double: same bug class as Single (below) — Max/Min NaN propagation, MaxMagnitude/MinMagnitude tie-break, Sign exception type, RootN negative-base bug |
| `b8f4398` | Port Single: **fixed real bugs** — Max/Min used std::fmax/fmin (returns the non-NaN operand) instead of .NET's NaN-propagating +0>-0 semantics; MaxMagnitude/MinMagnitude picked the tie-break without checking sign; Sign returned 0 for NaN instead of throwing ArithmeticException; Equals used raw == (NaN != NaN) instead of .NET's NaN-equals-NaN contract; GetHashCode didn't collapse NaN/zero bit patterns; RootN was a naive pow(x, 1/n) returning NaN for any negative base instead of the real root for odd n |
| `710e315` | Port Random: **fixed a real bug** — `GetItems`/`GetString` had no validation for empty `choices`, so `GetItems(emptyChoices, 0)` indexed an empty collection at `Next(0)` (undefined behavior) instead of throwing `ArgumentException` like .NET does (even when `length == 0`); restructured to match .NET's actual delegation chain so validation applies uniformly |
| `21d42e8` | Port Progress: `Progress<T>(Action<T>)` now throws `ArgumentNullException` for a null/empty handler, matching .NET |
| `e6a6d4e` | Port PlatformNotSupportedException: fixed missing `HResult` |
| `f768e92` | Port OperationCanceledException: **added the entire missing `CancellationToken` surface** — 3 of 6 constructors and the `CancellationToken` property were absent — plus `HResult` |
| `f1756a3` | Port OperatingSystem: verified complete/correct; fixed `int` → `SharpRuntime::intcs` param typing |
| `0dd726d` | Port OverflowException: fixed missing `HResult` |
| — | Object, ObsoleteAttribute, OrdinalComparer, ParamArrayAttribute, PlatformID, Predicate, ProcessCpuUsage: all verified complete and correct against .NET, no changes needed |
| `b53d34f` | Port ObjectDisposedException: fixed missing `HResult` |
| `abbdbbf` | Port Nullable: added missing `explicit operator T()` conversion (throws `InvalidOperationException` if no value, matching .NET); fixed `GetHashCode()` return type; fixed `ToString()` to prefer a `T::ToString()` member over `operator<<` (same fix pattern as `Lazy<T>` earlier) |
| `9340c98` | Port NullReferenceException: fixed missing `HResult` (`E_POINTER` — a standard COM HResult, not `COR_E_*`) |
| `521f0a6` | Port NotImplementedException (`E_NOTIMPL`) + NotSupportedException (`COR_E_NOTSUPPORTED`): fixed missing `HResult` on both |
| `200efb3` | Port MemoryExtensions: **fixed a real bug** — `Contains`/`IndexOf`/`LastIndexOf`/`Count`/`ContainsAny`/`IndexOfAny`/`Replace`/`StartsWith`/`EndsWith`/`Trim*`/`SequenceEqual`/`CommonPrefixLength` all used plain `==`, but .NET's real implementations treat NaN as equal to itself for `float`/`double` (matching `float.Equals`); a NaN in a `float`/`double` span would silently fail to be found/counted/replaced. Added a NaN-aware equality helper and routed all the equality-driven methods through it |
| `0504c05` | Port NotFiniteNumberException: fixed missing `HResult`; fixed a genuine .NET *source* quirk — the `(double offendingNumber)` ctor uses the base `ArithmeticException`'s default message instead of its own, verified against the actual .NET source rather than "corrected" |
| `faa3e40` | Port MulticastNotSupportedException: fixed missing `HResult` |
| — | Port MulticastDelegate: verified complete and correct, no changes needed |
| `0bfa818` | Port MissingMethodException: fixed missing `HResult` (`COR_E_MISSINGMETHOD`) and dynamic-message format |
| `17e3e2d` | Port MissingFieldException: fixed missing `HResult` (`COR_E_MISSINGFIELD`) and dynamic-message format |
| `b9cdc44` | Port MissingMemberException (base of the two above, reviewed first): fixed missing `HResult` (`COR_E_MISSINGMEMBER`) and message format |
| — | Port MidpointRounding: verified complete and correct, no changes needed |
| `741e631` | Port MethodAccessException: fixed missing `HResult` (`COR_E_METHODACCESS`) |
| `f326ac7` / `d5e48f9` | Port MathF / Math: **fixed real bugs in both** — `Max`/`Min` used `std::fmax`/`fmin` (returns the non-NaN side) instead of .NET's actual NaN-*propagating* semantics; `Sign` silently returned 0 for NaN instead of throwing `ArithmeticException`; `Round()` defaulted to away-from-zero via `std::round` instead of .NET's actual round-to-even default; `Math.Abs(int/long/short/sbyte)` didn't throw on `MinValue`; `Math.Clamp` never validated `min > max` across any of its 10 overloads; `MathF.Log(x,y)` didn't match .NET's IEEE-754 edge-case special-casing |
| `ce2c9e9` | Port Memory: added missing `Pin()` (returns `MemoryHandle`, no-op pin since no GC) |
| `7096215` | Port MemberAccessException: fixed missing `HResult` (`COR_E_MEMBERACCESS`) |
| — | `System.MDArray`: classified `ignore`/out-of-scope — NativeAOT/CoreCLR internal type-system implementation detail, never a public API |
| `79a93ac` | Port Lazy: **fixed three real bugs** — `ToString()` always returned a fixed placeholder instead of `Value().ToString()` once created; exception-caching semantics were backwards (None/ExecutionAndPublication should cache+rethrow the same exception, PublicationOnly should retry — the old code did the opposite via `std::call_once`'s built-in retry-on-exception behavior); a factory recursively accessing `Value()` deadlocked via recursive `std::call_once` instead of throwing `InvalidOperationException` like .NET — added a same-thread reentrancy check that runs before dispatching into the lock |
| `d98ee9e` | Port InvalidTimeZoneException: verified against .NET (correctly derives from `Exception` not `SystemException`, correctly sets no custom `HResult`) — no code change, added tests locking in both facts |
| `8f607d6` | Port InvalidProgramException: added missing `(message, innerException)` ctor and `HResult` (`COR_E_INVALIDPROGRAM`) |
| `9c75be8` | Port InvalidOperationException: fixed missing `HResult` (`COR_E_INVALIDOPERATION`) |
| `2cb0513` | Port InvalidCastException: added missing `(message, errorCode)` ctor and `HResult` (`COR_E_INVALIDCAST`) |
| `a4a5ef0` | Port IntPtr: added `ToInt32`/`ToInt64`/`ToPointer`/`CompareTo`/`Equals`/`GetHashCode`/`ToString`/`Add`/`Subtract`/`MaxValue`/`MinValue`/`Size` — was down to just ctor/Zero/equality |
| `dbe61df` | Port Int64: **fixed a real bug** — `Log2(0)` threw `std::domain_error`, but .NET's `BitOperations.Log2(0)` returns 0, not an error; added `BigMul` (now returns `Int128`), `CopySign`, `MaxMagnitude`, `MinMagnitude`, `IsNegative`, `IsPositive` |
| `3cdba03` | Port Int32: **fixed a real bug** — `MaxMagnitude`/`MinMagnitude` computed `MinValue`'s magnitude as `MinValue` itself instead of treating it as unrepresentable/largest like .NET does, inverting results whenever `MinValue` was an operand; added missing `CompareTo`/`Equals`/`GetHashCode` |
| `f46af96` | Port Int16: added the full math/comparison surface it was missing entirely (`CompareTo`, `Equals`, `GetHashCode`, `Abs`, `Clamp`, `Max`, `Min`, `Sign`, `DivRem`, `IsEvenInteger`, `IsOddInteger`, `IsPow2`, `LeadingZeroCount`, `PopCount`, `TrailingZeroCount`, `RotateLeft`, `RotateRight`, `Log2`) |
| `a58e32f` | Port Int128: **fixed two real bugs** — `operator<<`/`operator>>` passed the shift amount straight to the native `__int128` shift, which is UB outside [0,127] (.NET masks it mod 128); `Abs(MinValue)` silently wrapped instead of throwing. Added `Clamp`, `Max`, `Min`, `Sign`, `IsEvenInteger`, `IsOddInteger`, `IsPow2`, `Log2`, `ToString(format)`, `NegativeOne` |
| `af536ec` | Port InsufficientMemoryException: fixed missing `HResult` on it and its base `OutOfMemoryException` (both predate the `HResult` property, see §3a) |
| `5edf94a` | Port InsufficientExecutionStackException: added missing `(message, innerException)` ctor (.NET has 3 public ctors, this had 2) and `HResult` |
| `e566f22` | Port IndexOutOfRangeException: fixed missing `HResult` (`COR_E_INDEXOUTOFRANGE`) |
| `14cea70` | Port Index: **fixed a real bug** — `GetOffset()` threw on out-of-bounds offsets, but .NET's implementation intentionally performs no validation there (bounds checking is the caller's job, e.g. `Range.GetOffsetAndLength`); fixed `Range.GetOffsetAndLength` to do that validation itself (previously missed the `end > length` case) |
| `361ce17` | Port IUtf8SpanParsable: fixed mutable `Span<byte>` → `ReadOnlySpan<byte>`, added `IFormatProvider` overloads, removed misleading "Stub" status, updated 3 test implementers |
| `99dc25d` | Port IUtf8SpanFormattable: fixed wrong `<TSelf>` generic arity (.NET's interface isn't generic), added `IFormatProvider` overload, updated 2 test implementers |
| `4f91f45` | Port ISpanFormattable: added missing `TryFormat(..., IFormatProvider)` overload (known gap from previous NEXT.md) |
| `01939a2` | Port IProgress: documented C++-template variance gap (no code change needed otherwise) |
| `4bdc703` | Port IParsable: documented static-abstract → instance-virtual mapping (no code change needed otherwise) |
| `73af3f9` | Port IObservable/IObserver: documented C++-template variance gap (no code change needed otherwise) |
| `2edaed4` | Port HashCode: added per-process random seed (matches .NET's documented non-determinism contract), `AddBytes(ReadOnlySpan<byte>)` overload |
| `0ff26c8` | Port Half: **fixed two real bugs** — `FromSingle()` truncated instead of rounding (now correct IEEE 754 round-to-nearest-even); `ToSingle()` mis-scaled subnormals by ~2^85 (was treating the half mantissa as a float subnormal's mantissa). Also fixed `NaN` bits (0x7E00 → correct 0xFE00) and `operator==`/`!=` (was bit-exact, now correct IEEE semantics for NaN/±0). Added `NegativeZero`/`One`/`NegativeOne`/`E`/`Pi`/`Tau`, `IsNormal`/`IsSubnormal`, full arithmetic operators, `ToDouble`/`FromDouble`, `Parse`/`TryParse`, `ToString(format)`, `TryFormat` |
| `b1625fc` | Port Guid: **fixed a real bug** — `ToByteArray()`/byte-array ctor used the same byte order as `ToString()`, but .NET's actual default `ToByteArray()` is little-endian for the first three fields (verified against real dotnet/runtime test vectors); added `X` format, `ParseExact`/`TryParseExact`, `Variant`/`Version`, `CreateVersion7`, component ctors, span-based Parse/TryParse/TryFormat/TryWriteBytes, `GetHashCode` |
| `1c549f5` | Port Stream.Position: get/set `Position` property matching .NET's full `Stream` API |
| `d01d1b4` | Port IFormattable: add `ToString(format, IFormatProvider)` overload (default forwards to 1-arg version); stale doc-comment corrected |
| `00e651b` | Port IConvertible: add missing `ToDecimal()`/`ToDateTime()`; implement in `DBNull` |
| `d61ffb8` | Port IAsyncResult: add missing `AsyncState`/`AsyncWaitHandle` properties |
| `47b2f90` | Port IAsyncDisposable: fix `DisposeAsync()` to return `ValueTask` instead of `void` |
| `da58de8` | Port GCMemoryInfo: fix `PinnedObjectsCount` type mismatch (was `bool`, should be `long`); add 5 missing properties |
| `1dfb24a` | Port GC: add missing `TryStartNoGCRegion`/`WaitForFullGCApproach`/`Complete` overloads |
| `74ac3ff` | Port Func: extend `FuncT4..FuncT16` for full arity parity with `Action` |
| `9473526` | Port FormatException: fix missing `HResult` (`COR_E_FORMAT`) |
| `80a1089` | **plan.sqlite3 workflow: switch to autonomous batch processing, add `tobedecided` status** (see §3a) |
| `51487b3` | Port FieldAccessException: fix missing `HResult` (`COR_E_FIELDACCESS`) |
| `195bd31` | Port ExecutionEngineException: fix missing `HResult` (`COR_E_EXECUTIONENGINE`) |
| `bab45c2` | Port Exception: add `HResult`/`Source`/`HelpLink` properties |
| `4cf6c68`–`d78e6c8` | Ports of EventHandler, EventArgs, EnvironmentVariableTarget, Environment, Enum, DuplicateWaitObjectException, Double, DllNotFoundException, Decimal, DateTimeOffset, DateTime, DateOnly, DBNull, Convert, ContextMarshalException, ConsoleSpecialKey, ConsoleKeyInfo, CharEnumerator, Char, Casing, CLSCompliantAttribute, Byte, System.Buffer — see `git log` for full messages |

### §3a — Workflow change: autonomous plan.sqlite3 processing (2026-07-02)

The `plan.sqlite3` review workflow **no longer asks the user before each type**. Full rules live in
`prompt.md` (the canonical source — read it, not this summary). Short version:

- Classify each `todo`/empty item yourself: port it, `ignore` it (with `outofscope` set appropriately),
  or — if genuinely ambiguous — set `status = 'tobedecided'` and move on without guessing.
- Never stop between items to ask for confirmation.
- State lives in `plan.sqlite3` + git history, not conversation memory, so this resumes cleanly from
  a fresh context: just re-open `prompt.md` and continue from its Step 1.
- Still build clean + all tests passing before every commit; still one port per commit; still never push
  without being explicitly asked in that turn.

A recurring pattern found while re-reviewing already-"ported" types under this workflow: several
exception types set no custom `HResult` because the `HResult` property itself was added to the
`Exception` base class after those types were originally ported. `ExecutionEngineException`,
`FieldAccessException`, and `FormatException` have been fixed so far — **assume other exception
types ported before `bab45c2` have the same gap** and check `HResult` against `HResults.cs` /
`/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs` when reviewing them.

---

## 4. Current blocker / main problem

**No active technical blocker.** Build is clean, all 8751 tests pass.

The workflow is now autonomous (§3a) — do **not** revert to asking the user before each type; that was
the old behavior and has been intentionally replaced.

Next unprocessed types in `System` namespace (from `plan.sqlite3`, in processing order):
`RankException`, `ReadOnlyMemory`, `RuntimeType`, `RuntimeTypeHandle`, `SByte`,
`SerializableAttribute`, `Single`, `SpecialFolder`, …
884 `todo` + 62 empty items remain across all namespaces (154 `ported` so far). Note:
`OutOfMemoryException` and `Range` were fixed earlier in the session as side effects of reviewing
other types (`InsufficientMemoryException`, `Index`) but their own `plan.sqlite3` rows weren't marked
`ported` until later checkpoints — worth double-checking `plan.sqlite3` status against actual code
state occasionally, since side-effect fixes on related types can leave the tracking row stale.

**Process note:** when delegating a review to a background fork (`Agent` tool with `subagent_type:
"fork"`) while continuing other work yourself in parallel, be careful with `git add`/`git commit` — a
bare `git commit` (no pathspec) commits *everything currently staged*, including anything the fork
staged concurrently in the same working tree. This happened once this session (Math + MathF work landed
in one commit under the MathF message) and had to be split after the fact with `git reset HEAD~1` +
two accurate commits. Always run `git status --short` immediately before committing when a fork might
be running concurrently, and only `git add` the exact files you intend for that commit.

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| POSIX-only | `System::Net::Sockets` — `<sys/socket.h>`, `pread`, `pwrite`; no Windows/Emscripten |
| POSIX-only | `System::IO::RandomAccess` — `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — reads `/proc/self/exe`; not portable to macOS |
| POSIX-only | `System::TimeZoneInfo` — `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| incomplete | `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; throws |
| incomplete | `ArrayList.GetEnumerator()` — returns `nullptr`; not yet iterable via `IEnumerator*` |
| incomplete | `CopyTo(Array, int)` — `System.Array` type does not exist; skipped on all collections |
| audit needed | Exception types ported before `bab45c2` may set no per-type `HResult` — see §3a |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub | `System::GC` — all methods are no-ops (by design — this is the correct end state, not a gap) |
| stub | `System::Type` — no runtime reflection |
| stub | `System::Activator` — `CreateInstance` not implementable without reflection |
| stub | `System::Enum` — `GetNames`/`GetValues`/`Parse`/reflection methods not implemented; `HasFlag`/`ToUnderlying`/`ToInt32`/`ToString` work via templates |
| suspected bug | `extern char** environ` must remain at file scope in `Environment.cpp` — placing it inside `namespace System` causes a PIE relocation error |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| design note | `ArrayList` compares `std::any` elements by `type_info` only; meaningful value comparison requires a typed `IComparer` |
| workflow risk | Duplicate test suite names cause linker errors — always check `--gtest_filter` output and use `Tests2` suffix when collisions exist |
| ⚠️ PARTIAL | `System::Buffers::Text::Utf8Formatter`/`Utf8Parser` — bool + all 8 integer types fully support G/D/N/X format specifiers (fixed: format was previously silently ignored); `Guid`/`DateTime`/`DateTimeOffset`/`TimeSpan`/`Decimal`/`Single`/`Double` TryFormat/TryParse overloads are not yet implemented |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs, ulongcs
  SharpRuntime/Prop.hpp                 ← property macros
  SharpRuntime/Storage/StoragePaths.hpp ← platform storage root
  System/                               ← ~600 .hpp files
    Collections/                        ← ArrayList, BitArray, Hashtable, IList, IComparer, …
    Collections/Generic/                ← List, Dictionary, Queue, SortedSet, …
    Collections/Concurrent/             ← ConcurrentDictionary, BlockingCollection
    Collections/Immutable/              ← ImmutableArray, ImmutableList, …
    Buffers/                            ← ArrayPool, MemoryPool, SearchValues, SequenceReader, …
    Buffers/Binary/                     ← BinaryPrimitives
    Buffers/Text/                       ← Base64, Base64Url
    IO/                                 ← Stream, File, Path, Compression/, Hashing/
    Text/                               ← StringBuilder, Encoding, Json/, Encodings/
    Threading/                          ← Thread, Monitor, Tasks/, LazyThreadSafetyMode
    Numerics/                           ← BigInteger, Vector*, Matrix*, Quaternion, BFloat16
    Diagnostics/                        ← Debug, Stopwatch, Activity
    Globalization/                      ← CultureInfo, Calendar, DateTimeFormatInfo
    Net/                                ← IPAddress, Http/, Sockets/
    Xml/                                ← XmlReader, XmlWriter, Linq/
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← ~299 GoogleTest .cpp files
vendor/                                 ← googletest, nlohmann/json, tinyxml2, miniz
plan.sqlite3                            ← tracks porting status per type (gitignored, local-only)
prompt.md                               ← canonical plan.sqlite3 workflow instructions (read this, not just NEXT.md)
```

### Invariants that must not be broken
1. **Zero errors, zero warnings** (`-Wall -Wextra -Werror`) before every commit.
2. **8751+ tests passing** — never go below the watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (= `int32_t`) in public APIs mirroring .NET `int` parameters.
6. **SPDX header on every file** — `// SPDX-License-Identifier: MIT` + copyright + .NET attribution.
7. **Doxygen `/** */`** on all public declarations — `///` has been fully eliminated; never reintroduce it.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef`.
9. **Inner exceptions use `std::exception_ptr`** — never `const std::exception&`. Pattern: `FooException(const std::string& msg, std::exception_ptr inner) : Base(msg, std::move(inner)) {}`
10. **No broad header refactor** — property naming touches 449+ files in CNA.
11. **Push only to `develop`** — never push to `master` or create tags without explicit per-action user approval.
12. **GPG signing times out** — always commit with `git -c commit.gpgsign=false commit`.
13. **plan.sqlite3 processing is autonomous** (since 2026-07-02, see §3a) — do NOT ask the user per type; classify and proceed. This reverses the previous "always ask" rule.

### Type alias summary
| Alias | Underlying | .NET equivalent |
|-------|-----------|-----------------|
| `SharpRuntime::intcs` | `int32_t` | `int` |
| `SharpRuntime::shortcs` | `int16_t` | `short` |
| `SharpRuntime::longcs` | `int64_t` | `long` |
| `SharpRuntime::bytecs` | `uint8_t` | `byte` |
| `SharpRuntime::sbytecs` | `int8_t` | `sbyte` |
| `SharpRuntime::uintcs` | `uint32_t` | `uint` |
| `SharpRuntime::ulongcs` | `uint64_t` | `ulong` |
| `SharpRuntime::ushortcs` | `uint16_t` | `ushort` |
| `SharpRuntime::charcs` | `char16_t` | `char` |

### plan.sqlite3 workflow (see prompt.md for the full canonical version)
- Table `task`, columns: `id, namespace, name, type, internal, outofscope, status`
- Status values: `ported`, `ignore`, `todo`, `tobedecided`, `''` (empty = unset). **`in_progress` does not exist.**
- Current counts: 107 ported, 6 ignore, 932 todo, 62 empty (unset), plus large `ignored`/`in_progress` legacy buckets not part of the active workflow
- Query next unset: `sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 1;"`
- **Fully autonomous — no per-type user approval required** (changed 2026-07-02)

### Inner exception ctor pattern (correct)
```cpp
// Header (.hpp):
FooException(const std::string& message, std::exception_ptr inner);

// Body (.cpp):
FooException::FooException(const std::string& message, std::exception_ptr inner)
    : BaseException(message, std::move(inner)) {}

// Test caller:
auto inner = std::make_exception_ptr(std::runtime_error("cause"));
FooException e("msg", inner);
EXPECT_NE(std::string(e.what()).find("msg"), std::string::npos);
// Do NOT check for inner.what() content — it is not concatenated into what()
```

### HResult pattern for exception types (added 2026-07-02, see §3a)
```cpp
// In every constructor, after the base-class init list:
FooException::FooException()
    : SystemException(DefaultMsg) {
    setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131XXX)); // COR_E_FOO — from HResults.cs
}
```
Look up the exact HResult constant in `/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs`.

### Duplicate test suite names
`EXCEPT_SIMPLE(ExType)` macro in `ExceptionRemainingTests.cpp` already defines `DefaultCtor_WhatNotEmpty`, `MessageCtor_WhatContainsMessage`, `IsA_Exception`. New test files for those same types must use a `Tests2` suffix (e.g. `ExecutionEngineExceptionTests2`) to avoid linker duplicate symbol errors.

---

## 7. Useful commands

```bash
# Configure (first time only)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel 4

# Build — errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run all tests
./build/SharpRuntimeTests

# Run a specific suite
./build/SharpRuntimeTests --gtest_filter="BitConverter*"

# Check next unset types (System namespace prioritized)
sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 8;"

# Verify no /// Doxygen remains
grep -rl "^\s*///" include/System/ | wc -l

# Verify no old inner-exception pattern remains
grep -rl "const std::exception& inner" include/System/ | wc -l

# Check .NET source for a type
find /rv/tmp/runtime/src/libraries -name "Buffer.cs" | head -3

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"

# Push (develop only, only when explicitly asked)
git push origin develop
```

---

## 8. Next smallest tasks

Ordered by `plan.sqlite3` processing order. Workflow is now autonomous (§3a) — no approval needed,
just classify and proceed per `prompt.md`. Previous batches (Index through Lazy, MDArray through
Random — 44 types total) are done, see §3 for what changed.

### Task 1 — System.RankException
- **Goal:** Checklist review; per §3a, check `HResult` against `HResults.cs` since this type may predate the `HResult` property being added to `Exception`.
- **Files:** `include/System/RankException.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="RankException*"`

### Task 2 — System.ReadOnlyMemory
- **Goal:** Full checklist review of the existing `ReadOnlyMemory<T>` struct against `.NET`'s ref surface. `Memory<T>` was reviewed this session (added `Pin()`, fixed `GetHashCode` typing) — check whether `ReadOnlyMemory<T>` has the same `Pin()` gap or other parallel issues, since they're usually implemented in lockstep in .NET.
- **Files:** `include/System/ReadOnlyMemory.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="ReadOnlyMemory*"`

### Task 3 — System.RuntimeType / RuntimeTypeHandle
- **Goal:** Both are reflection-adjacent (`RuntimeTypeHandle` backs `typeof(T)`/`Type.TypeHandle`). Check what's actually in `/rv/tmp/runtime/src/libraries/` before assuming out-of-scope — `RuntimeTypeHandle` is already listed as ported (full struct, not reflection metadata) per NEXT.md §2 "Runtime handles", so a real (if minimal) C++ mapping already exists; verify it rather than reclassifying. `RuntimeType` (the enum, not the reflection class `System.RuntimeType`) may be a distinct, simpler thing — check namespace/type column in plan.sqlite3 before conflating them.
- **Files:** `include/System/RuntimeTypeHandle.hpp`; check `RuntimeType` existence separately
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="RuntimeType*"`

### Task 4 — System.SByte
- **Goal:** Full checklist review against `.NET`'s ref surface, same bar as this session's Int16/Int32/Int64 reviews (CompareTo/Equals/GetHashCode/Abs/Clamp/Max/Min/Sign/DivRem/IsEvenInteger/IsOddInteger/IsPow2/bit ops/Log2). Also worth checking: does `SByte.hpp` have the same `MaxMagnitude`/`MinMagnitude` `MinValue`-handling bug found in `Int32`/`Int64` this session? `SByteTests.cpp` already has `MaxMagnitude_Larger`/`MinMagnitude_Smaller` tests — check if they exercise the `MinValue` edge case or not.
- **Files:** `include/System/SByte.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="SByte*"`

### Task 5 — System.SerializableAttribute
- **Goal:** Full checklist review. Like `ObsoleteAttribute`/`ParamArrayAttribute` (verified this session), likely a trivial marker-class port with no runtime behavior needed - match that established pattern.
- **Files:** check `include/System/SerializableAttribute.hpp` existence first
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="SerializableAttribute*"`

---

## 9. Do not do yet

- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA.
- **No LINQ port** — use `std::ranges` in all new ported code.
- **No Windows / Emscripten CI** — POSIX-only subsystems are documented bugs.
- **No merge to `master`** — always push to `develop` only; master merge requires explicit per-action approval.
- **No new vendored libraries** without discussing scope impact.
- **No speculative API additions** — only add methods present in .NET's published API surface.
- **No work on `System::Type` / `System::Activator`** — stubs are the correct end state.
- **No `SynchronizationContext` full implementation** — synchronous stub is correct for game use.
- **No duplicate test suite names** — always check for collisions; use `Tests2` suffix.
- **No reintroduction of `///` Doxygen** — all headers now use `/** */`; never write `///` in new code.
- **No `ArrayList.Sort()` without comparer** — `std::any` cannot be compared without type info.
- **No mass rewrite or reformatting** in a single commit — incremental changes only.
- **No `System.Buffer.BlockCopy`/`ByteLength`/`GetByte`/`SetByte`** — these require `System::Array` which does not exist and is out of scope.
- **No per-item user confirmation in the plan.sqlite3 workflow** — this was the *old* rule and has been intentionally reversed as of 2026-07-02 (§3a). Do not reintroduce it; classify and proceed autonomously per `prompt.md`.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation). NEXT.md is a snapshot for context, not the source of truth for process.

Then open plan.sqlite3 and find the next unprocessed type (System namespace first):
  sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 1;"

For each item, per prompt.md:
  1. Look up what it does in /rv/tmp/runtime/src/libraries/ and classify it yourself — port / ignore
     (+ outofscope) / tobedecided (if genuinely ambiguous). Do not ask the user.
  2. If porting: check the existing header/tests, review against the FULL checklist in CLAUDE.md
     (API surface, doc-comments, SPDX, logic parity incl. HResult where applicable, build, tests)
     as if it were new — fix gaps, don't rubber-stamp.
  3. Run: cmake --build build --parallel 4  (zero errors, zero warnings)
  4. Run: ./build/SharpRuntimeTests  (all 8751+ tests must pass)
  5. Mark ported: sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"
  6. Commit only the files for that port: git -c commit.gpgsign=false commit -m "..."
  7. Loop back to step 1 — keep going, do not stop to ask between items.
  8. Never push without the user explicitly asking in that turn.

After a batch of types, update NEXT.md.
```
