# plan_System.md — namespace System .cs Files
All .cs files from dotnet/runtime (`/rv/tmp/runtime/src/libraries/`) that belong to namespace `System` (not sub-namespaces).
Status values: `ported`, `in_progress`, `todo`, `ignore`

Reference source: dotnet/runtime, MIT License

---

| # | File | Namespace | Status | Note |
|---|------|-----------|--------|------|
| 1 | Common/src/System/TimeProvider.cs | `System` | `ported` | TimeProvider — port (abstract time source) |
| 2 | Microsoft.Bcl.Numerics/src/System/MathF.cs | `System` | `ported` | Single-precision math — port via cmath |
| 3 | System.ComponentModel/ref/System.ComponentModel.cs | `System` | `ignore` | Public API surface definition — use as porting reference |
| 4 | System.ComponentModel/src/System/IServiceProvider.cs | `System` | `ignore` | IServiceProvider interface — port |
| 5 | System.ComponentModel.TypeConverter/ref/System.ComponentModel.TypeConverter.cs | `System` | `ignore` | Public API surface definition — use as porting reference |
| 6 | System.ComponentModel.TypeConverter/src/System/ComponentModel/UriTypeConverter.cs | `System` | `ignore` | UriTypeConverter — port with ComponentModel |
| 7 | System.Configuration.ConfigurationManager/ref/System.Configuration.ConfigurationManager.cs | `System` |   | Public API surface definition — use as porting reference |
| 8 | System.Configuration.ConfigurationManager/src/System/UriIdnScope.cs | `System` |   | IDN scope enum — part of Uri port |
| 9 | System.Console/ref/System.Console.cs | `System` |   | Public API surface definition — use as porting reference |
| 10 | System.Console/src/System/ConsoleCancelEventArgs.cs | `System` |   | Ctrl+C event args — port |
| 11 | System.Console/src/System/ConsoleColor.cs | `System` |   | ConsoleColor enum — ported in sharp-runtime |
| 12 | System.Console/src/System/Console.cs | `System` |   | Console I/O — ported in sharp-runtime |
| 13 | System.Console/src/System/ConsoleKey.cs | `System` |   | ConsoleKey enum — port |
| 14 | System.Console/src/System/ConsoleKeyInfo.cs | `System` |   | ConsoleKeyInfo struct — port |
| 15 | System.Console/src/System/ConsoleModifiers.cs | `System` |   | ConsoleModifiers enum — port |
| 16 | System.Console/src/System/ConsoleSpecialKey.cs | `System` |   | ConsoleSpecialKey enum — port |
| 17 | System.Memory.Data/ref/System.Memory.Data.cs | `System` |   | Public API surface definition — use as porting reference |
| 18 | System.Memory.Data/src/System/BinaryData.cs | `System` |   | BinaryData — port |
| 19 | System.Memory/ref/System.Memory.cs | `System` |   | Public API surface definition — use as porting reference |
| 20 | System.Memory/src/System/SequencePosition.cs | `System` |   | SequencePosition (pipelines) — port with Buffers |
| 21 | System.Private.CoreLib/ref/System.Private.CoreLib.ManualShimTypeForwards.cs | `System` |   | Public API surface definition — use as porting reference |
| 22 | System.Private.CoreLib/src/System/AccessViolationException.cs | `System` |   | CLR memory access violation — stub |
| 23 | System.Private.CoreLib/src/System/Action.cs | `System` |   | Action<> delegates — port (maps to std::function<void(...)>) |
| 24 | System.Private.CoreLib/src/System/Activator.cs | `System` |   | Activator.CreateInstance — CLR reflection; stub |
| 25 | System.Private.CoreLib/src/System/AggregateException.cs | `System` |   | AggregateException — port (for Task errors) |
| 26 | System.Private.CoreLib/src/System/AppContext.AnyOS.cs | `System` |   | AppContext cross-platform impl — part of AppContext port |
| 27 | System.Private.CoreLib/src/System/AppContext.cs | `System` |   | AppContext — stub (partially in sharp-runtime) |
| 28 | System.Private.CoreLib/src/System/AppDomain.cs | `System` |   | AppDomain — stub (partially in sharp-runtime) |
| 29 | System.Private.CoreLib/src/System/AppDomainSetup.cs | `System` |   | AppDomainSetup — stub |
| 30 | System.Private.CoreLib/src/System/AppDomainUnloadedException.cs | `System` |   | CLR AppDomain exception — stub |
| 31 | System.Private.CoreLib/src/System/ApplicationException.cs | `System` |   | ApplicationException — port |
| 32 | System.Private.CoreLib/src/System/ArgumentException.cs | `System` |   | ArgumentException — ported |
| 33 | System.Private.CoreLib/src/System/ArgumentNullException.cs | `System` |   | ArgumentNullException — ported |
| 34 | System.Private.CoreLib/src/System/ArgumentOutOfRangeException.cs | `System` |   | ArgumentOutOfRangeException — ported |
| 35 | System.Private.CoreLib/src/System/ArithmeticException.cs | `System` |   | ArithmeticException — port |
| 36 | System.Private.CoreLib/src/System/Array.cs | `System` |   | Array type — partial port (C++ uses std::vector/std::array) |
| 37 | System.Private.CoreLib/src/System/ArraySegment.cs | `System` |   | ArraySegment<T> — port (wraps array slice) |
| 38 | System.Private.CoreLib/src/System/ArrayTypeMismatchException.cs | `System` |   | ArrayTypeMismatchException — stub |
| 39 | System.Private.CoreLib/src/System/AssemblyLoadEventArgs.cs | `System` |   | Assembly load event — stub |
| 40 | System.Private.CoreLib/src/System/AssemblyLoadEventHandler.cs | `System` |   | Assembly load handler — stub |
| 41 | System.Private.CoreLib/src/System/AsyncCallback.cs | `System` |   | AsyncCallback delegate — port (legacy async) |
| 42 | System.Private.CoreLib/src/System/Attribute.cs | `System` |   | System.Attribute — CLR metadata; stub |
| 43 | System.Private.CoreLib/src/System/AttributeTargets.cs | `System` |   | AttributeTargets enum — stub |
| 44 | System.Private.CoreLib/src/System/AttributeUsageAttribute.cs | `System` |   | AttributeUsageAttribute — stub |
| 45 | System.Private.CoreLib/src/System/BadImageFormatException.cs | `System` |   | BadImageFormatException — stub |
| 46 | System.Private.CoreLib/src/System/BitConverter.cs | `System` |   | BitConverter — port |
| 47 | System.Private.CoreLib/src/System/Boolean.cs | `System` |   | Primitive bool — C++ has native bool; port ToString/Parse |
| 48 | System.Private.CoreLib/src/System/Buffer.cs | `System` |   | Buffer.BlockCopy etc. — port (memcpy wrapper) |
| 49 | System.Private.CoreLib/src/System/Byte.cs | `System` |   | Primitive byte (uint8_t) — port ToString/Parse/formatting |
| 50 | System.Private.CoreLib/src/System/Char.cs | `System` |   | Unicode character — port; maps to char32_t |
| 51 | System.Private.CoreLib/src/System/CharEnumerator.cs | `System` |   | String character enumerator — port |
| 52 | System.Private.CoreLib/src/System/CLSCompliantAttribute.cs | `System` |   | CLSCompliant attribute — stub |
| 53 | System.Private.CoreLib/src/System/Convert.cs | `System` |   | Convert static class — port |
| 54 | System.Private.CoreLib/src/System/DataMisalignedException.cs | `System` |   | DataMisalignedException — stub |
| 55 | System.Private.CoreLib/src/System/DateOnly.cs | `System` |   | Date-only struct — port |
| 56 | System.Private.CoreLib/src/System/DateTime.cs | `System` |   | DateTime struct — ported in sharp-runtime |
| 57 | System.Private.CoreLib/src/System/DateTimeKind.cs | `System` |   | DateTimeKind enum — ported |
| 58 | System.Private.CoreLib/src/System/DateTimeOffset.cs | `System` |   | DateTimeOffset struct — ported in sharp-runtime |
| 59 | System.Private.CoreLib/src/System/DayOfWeek.cs | `System` |   | DayOfWeek enum — ported |
| 60 | System.Private.CoreLib/src/System/Decimal.cs | `System` |   | 128-bit decimal — port (already in sharp-runtime) |
| 61 | System.Private.CoreLib/src/System/Decimal.DecCalc.cs | `System` |   | Decimal arithmetic internals — part of Decimal port |
| 62 | System.Private.CoreLib/src/System/DivideByZeroException.cs | `System` |   | DivideByZeroException — port |
| 63 | System.Private.CoreLib/src/System/DllNotFoundException.cs | `System` |   | DllNotFoundException — port |
| 64 | System.Private.CoreLib/src/System/Double.cs | `System` |   | double (64-bit) — port ToString/Parse/formatting |
| 65 | System.Private.CoreLib/src/System/DuplicateWaitObjectException.cs | `System` |   | DuplicateWaitObjectException — stub |
| 66 | System.Private.CoreLib/src/System/EntryPointNotFoundException.cs | `System` |   | EntryPointNotFoundException — stub |
| 67 | System.Private.CoreLib/src/System/Enum.cs | `System` |   | Enum base class — port (ToString, Parse, GetValues) |
| 68 | System.Private.CoreLib/src/System/Environment.cs | `System` |   | Environment class — partially ported |
| 69 | System.Private.CoreLib/src/System/Environment.Linux.cs | `System` |   | Linux-specific environment impl — reference for C++ Linux build |
| 70 | System.Private.CoreLib/src/System/Environment.OSVersion.OSX.cs | `System` |   | macOS OSVersion impl — platform-specific; handle via #ifdef |
| 71 | System.Private.CoreLib/src/System/Environment.OSX.cs | `System` |   | macOS environment impl — platform-specific; handle via #ifdef |
| 72 | System.Private.CoreLib/src/System/Environment.SpecialFolder.cs | `System` |   | SpecialFolder enum — port |
| 73 | System.Private.CoreLib/src/System/Environment.SpecialFolderOption.cs | `System` |   | SpecialFolderOption enum — port |
| 74 | System.Private.CoreLib/src/System/Environment.UnixOrBrowser.cs | `System` |   | POSIX/Wasm environment impl — reference for C++ POSIX build |
| 75 | System.Private.CoreLib/src/System/EnvironmentVariableTarget.cs | `System` |   | EnvironmentVariableTarget enum — port |
| 76 | System.Private.CoreLib/src/System/EventArgs.cs | `System` |   | EventArgs base class — port |
| 77 | System.Private.CoreLib/src/System/EventHandler.cs | `System` |   | EventHandler delegate — port (maps to std::function) |
| 78 | System.Private.CoreLib/src/System/Exception.cs | `System` |   | Base Exception class — ported in sharp-runtime |
| 79 | System.Private.CoreLib/src/System/FieldAccessException.cs | `System` |   | Reflection exception — stub |
| 80 | System.Private.CoreLib/src/System/FlagsAttribute.cs | `System` |   | FlagsAttribute — port (useful for enum flags) |
| 81 | System.Private.CoreLib/src/System/FormatException.cs | `System` |   | FormatException — ported |
| 82 | System.Private.CoreLib/src/System/FormattableString.cs | `System` |   | FormattableString (interpolated strings) — low priority |
| 83 | System.Private.CoreLib/src/System/Function.cs | `System` |   | Func<> delegates — port (maps to std::function<R(...)>) |
| 84 | System.Private.CoreLib/src/System/GC.cs | `System` |   | GC class — no-op stubs in C++ (no GC needed) |
| 85 | System.Private.CoreLib/src/System/GCMemoryInfo.cs | `System` |   | GC memory info — stub |
| 86 | System.Private.CoreLib/src/System/Guid.cs | `System` |   | Guid struct — port |
| 87 | System.Private.CoreLib/src/System/Half.cs | `System` |   | Half-precision float — port using __fp16 or std::float16_t |
| 88 | System.Private.CoreLib/src/System/HashCode.cs | `System` |   | HashCode combining — port |
| 89 | System.Private.CoreLib/src/System/IAsyncDisposable.cs | `System` |   | IAsyncDisposable — port (async cleanup) |
| 90 | System.Private.CoreLib/src/System/IAsyncResult.cs | `System` |   | IAsyncResult — port (legacy async pattern) |
| 91 | System.Private.CoreLib/src/System/ICloneable.cs | `System` |   | ICloneable — port |
| 92 | System.Private.CoreLib/src/System/IComparable.cs | `System` |   | IComparable — port (maps to operator<=>) |
| 93 | System.Private.CoreLib/src/System/IConvertible.cs | `System` |   | IConvertible — port |
| 94 | System.Private.CoreLib/src/System/ICustomFormatter.cs | `System` |   | ICustomFormatter — port |
| 95 | System.Private.CoreLib/src/System/IDisposable.cs | `System` |   | IDisposable — port as destructor pattern |
| 96 | System.Private.CoreLib/src/System/IEquatable.cs | `System` |   | IEquatable<T> — port (maps to operator==) |
| 97 | System.Private.CoreLib/src/System/IFormatProvider.cs | `System` |   | IFormatProvider — port |
| 98 | System.Private.CoreLib/src/System/IFormattable.cs | `System` |   | IFormattable — port |
| 99 | System.Private.CoreLib/src/System/Index.cs | `System` |   | System.Index (^operator) — port |
| 100 | System.Private.CoreLib/src/System/IndexOutOfRangeException.cs | `System` |   | IndexOutOfRangeException — ported |
| 101 | System.Private.CoreLib/src/System/InsufficientExecutionStackException.cs | `System` |   | CLR stack exception — stub |
| 102 | System.Private.CoreLib/src/System/InsufficientMemoryException.cs | `System` |   | InsufficientMemoryException — stub |
| 103 | System.Private.CoreLib/src/System/Int128.cs | `System` |   | 128-bit integer — port using __int128 or custom impl |
| 104 | System.Private.CoreLib/src/System/Int16.cs | `System` |   | Primitive short (int16_t) — port ToString/Parse |
| 105 | System.Private.CoreLib/src/System/Int32.cs | `System` |   | Primitive int (int32_t) — port ToString/Parse/formatting |
| 106 | System.Private.CoreLib/src/System/Int64.cs | `System` |   | Primitive long (int64_t) — port ToString/Parse |
| 107 | System.Private.CoreLib/src/System/IntPtr.cs | `System` |   | Pointer-sized integer — maps to intptr_t |
| 108 | System.Private.CoreLib/src/System/InvalidCastException.cs | `System` |   | InvalidCastException — ported |
| 109 | System.Private.CoreLib/src/System/InvalidOperationException.cs | `System` |   | InvalidOperationException — ported |
| 110 | System.Private.CoreLib/src/System/InvalidProgramException.cs | `System` |   | CLR IL exception — stub |
| 111 | System.Private.CoreLib/src/System/InvalidTimeZoneException.cs | `System` |   | InvalidTimeZoneException — port |
| 112 | System.Private.CoreLib/src/System/IObservable.cs | `System` |   | IObservable<T> — port (reactive pattern) |
| 113 | System.Private.CoreLib/src/System/IObserver.cs | `System` |   | IObserver<T> — port (reactive pattern) |
| 114 | System.Private.CoreLib/src/System/IParsable.cs | `System` |   | IParsable<T> — port (static Parse interface) |
| 115 | System.Private.CoreLib/src/System/IProgress.cs | `System` |   | IProgress<T> interface — port |
| 116 | System.Private.CoreLib/src/System/ISpanFormattable.cs | `System` |   | ISpanFormattable — port |
| 117 | System.Private.CoreLib/src/System/ISpanParsable.cs | `System` |   | ISpanParsable<T> — port |
| 118 | System.Private.CoreLib/src/System/IUtf8SpanFormattable.cs | `System` |   | IUtf8SpanFormattable — port |
| 119 | System.Private.CoreLib/src/System/IUtf8SpanParsable.cs | `System` |   | IUtf8SpanParsable<T> — port |
| 120 | System.Private.CoreLib/src/System/Lazy.cs | `System` |   | Lazy<T> — port (thread-safe lazy init) |
| 121 | System.Private.CoreLib/src/System/LazyOfTTMetadata.cs | `System` |   | Lazy<T,TMetadata> impl — port if needed |
| 122 | System.Private.CoreLib/src/System/Math.cs | `System` |   | Math functions — ported in sharp-runtime via cmath |
| 123 | System.Private.CoreLib/src/System/Math.DivModInt.cs | `System` |   | Integer DivMod helpers — port |
| 124 | System.Private.CoreLib/src/System/MathF.cs | `System` |   | Single-precision math — port via cmath |
| 125 | System.Private.CoreLib/src/System/MemberAccessException.cs | `System` |   | Reflection exception — stub |
| 126 | System.Private.CoreLib/src/System/Memory.cs | `System` |   | Memory<T> — port or map to std::vector slice |
| 127 | System.Private.CoreLib/src/System/MemoryExtensions.cs | `System` |   | Memory/Span extension methods — use std::span in C++ |
| 128 | System.Private.CoreLib/src/System/MemoryExtensions.Globalization.cs | `System` |   | Memory/Span extension methods — use std::span in C++ |
| 129 | System.Private.CoreLib/src/System/MemoryExtensions.Globalization.Utf8.cs | `System` |   | Memory/Span extension methods — use std::span in C++ |
| 130 | System.Private.CoreLib/src/System/MemoryExtensions.Trim.cs | `System` |   | Memory/Span extension methods — use std::span in C++ |
| 131 | System.Private.CoreLib/src/System/MemoryExtensions.Trim.Utf8.cs | `System` |   | Memory/Span extension methods — use std::span in C++ |
| 132 | System.Private.CoreLib/src/System/MethodAccessException.cs | `System` |   | Reflection exception — stub |
| 133 | System.Private.CoreLib/src/System/MidpointRounding.cs | `System` |   | MidpointRounding enum — port |
| 134 | System.Private.CoreLib/src/System/MissingFieldException.cs | `System` |   | Reflection exception — stub |
| 135 | System.Private.CoreLib/src/System/MissingMemberException.cs | `System` |   | Reflection exception — stub |
| 136 | System.Private.CoreLib/src/System/MissingMethodException.cs | `System` |   | Reflection exception — stub |
| 137 | System.Private.CoreLib/src/System/MulticastDelegate.cs | `System` |   | MulticastDelegate — port (event multicasting) |
| 138 | System.Private.CoreLib/src/System/MulticastNotSupportedException.cs | `System` |   | Delegate exception — stub |
| 139 | System.Private.CoreLib/src/System/NonSerializedAttribute.cs | `System` |   | NonSerialized attribute — stub |
| 140 | System.Private.CoreLib/src/System/NotFiniteNumberException.cs | `System` |   | NotFiniteNumberException — port |
| 141 | System.Private.CoreLib/src/System/NotImplementedException.cs | `System` |   | NotImplementedException — ported |
| 142 | System.Private.CoreLib/src/System/NotSupportedException.cs | `System` |   | NotSupportedException — ported |
| 143 | System.Private.CoreLib/src/System/Nullable.cs | `System` |   | Nullable<T> — map to std::optional<T> in C++ |
| 144 | System.Private.CoreLib/src/System/NullReferenceException.cs | `System` |   | NullReferenceException — ported |
| 145 | System.Private.CoreLib/src/System/Object.cs | `System` |   | System.Object base — stub (C++ uses inheritance differently) |
| 146 | System.Private.CoreLib/src/System/ObjectDisposedException.cs | `System` |   | ObjectDisposedException — ported |
| 147 | System.Private.CoreLib/src/System/ObsoleteAttribute.cs | `System` |   | ObsoleteAttribute — stub (use [[deprecated]] in C++) |
| 148 | System.Private.CoreLib/src/System/OperatingSystem.cs | `System` |   | OperatingSystem info — port |
| 149 | System.Private.CoreLib/src/System/OperationCanceledException.cs | `System` |   | OperationCanceledException — port (for Task/CancellationToken) |
| 150 | System.Private.CoreLib/src/System/OutOfMemoryException.cs | `System` |   | OutOfMemoryException — stub (C++ throws std::bad_alloc) |
| 151 | System.Private.CoreLib/src/System/OverflowException.cs | `System` |   | OverflowException — port |
| 152 | System.Private.CoreLib/src/System/ParamArrayAttribute.cs | `System` |   | params keyword attribute — stub |
| 153 | System.Private.CoreLib/src/System/PlatformID.cs | `System` |   | PlatformID enum — port |
| 154 | System.Private.CoreLib/src/System/PlatformNotSupportedException.cs | `System` |   | PlatformNotSupportedException — ported |
| 155 | System.Private.CoreLib/src/System/Progress.cs | `System` |   | IProgress<T>/Progress<T> — port |
| 156 | System.Private.CoreLib/src/System/Random.CompatImpl.cs | `System` |   | Random compatibility impl — internal |
| 157 | System.Private.CoreLib/src/System/Random.cs | `System` |   | Random class — ported in sharp-runtime |
| 158 | System.Private.CoreLib/src/System/Random.ImplBase.cs | `System` |   | Random base impl — internal |
| 159 | System.Private.CoreLib/src/System/Random.Xoshiro128StarStarImpl.cs | `System` |   | Xoshiro128** RNG — internal impl |
| 160 | System.Private.CoreLib/src/System/Random.Xoshiro256StarStarImpl.cs | `System` |   | Xoshiro256** RNG — internal impl |
| 161 | System.Private.CoreLib/src/System/Range.cs | `System` |   | System.Range — port |
| 162 | System.Private.CoreLib/src/System/RankException.cs | `System` |   | RankException — stub |
| 163 | System.Private.CoreLib/src/System/ReadOnlyMemory.cs | `System` |   | ReadOnlyMemory<T> — port or map to const slice |
| 164 | System.Private.CoreLib/src/System/ReadOnlySpan.cs | `System` |   | ReadOnlySpan<T> — map to const std::span in C++ |
| 165 | System.Private.CoreLib/src/System/ResolveEventArgs.cs | `System` |   | Assembly resolve event — stub |
| 166 | System.Private.CoreLib/src/System/ResolveEventHandler.cs | `System` |   | Assembly resolve handler — stub |
| 167 | System.Private.CoreLib/src/System/SByte.cs | `System` |   | Primitive sbyte (int8_t) — port ToString/Parse/formatting |
| 168 | System.Private.CoreLib/src/System/SerializableAttribute.cs | `System` |   | Serializable attribute — stub |
| 169 | System.Private.CoreLib/src/System/Single.cs | `System` |   | float (32-bit) — port ToString/Parse/formatting |
| 170 | System.Private.CoreLib/src/System/Span.cs | `System` |   | Span<T> — map to std::span in C++ |
| 171 | System.Private.CoreLib/src/System/StackOverflowException.cs | `System` |   | CLR stack overflow — stub only |
| 172 | System.Private.CoreLib/src/System/StringComparer.cs | `System` |   | String comparison strategies — port |
| 173 | System.Private.CoreLib/src/System/String.Comparison.cs | `System` |   | String comparison methods — port |
| 174 | System.Private.CoreLib/src/System/StringComparison.cs | `System` |   | StringComparison enum — port |
| 175 | System.Private.CoreLib/src/System/String.cs | `System` |   | Core string type — partially ported in sharp-runtime |
| 176 | System.Private.CoreLib/src/System/String.Manipulation.cs | `System` |   | String manipulation (Replace, Split, Trim etc.) — port |
| 177 | System.Private.CoreLib/src/System/StringNormalizationExtensions.cs | `System` |   | Unicode normalization — low priority |
| 178 | System.Private.CoreLib/src/System/String.Searching.cs | `System` |   | String search (IndexOf, Contains etc.) — port |
| 179 | System.Private.CoreLib/src/System/StringSplitOptions.cs | `System` |   | StringSplitOptions enum — port |
| 180 | System.Private.CoreLib/src/System/SystemException.cs | `System` |   | SystemException — ported |
| 181 | System.Private.CoreLib/src/System/ThreadAttributes.cs | `System` |   | Thread attributes — stub |
| 182 | System.Private.CoreLib/src/System/ThreadStaticAttribute.cs | `System` |   | ThreadStatic attribute — stub |
| 183 | System.Private.CoreLib/src/System/TimeOnly.cs | `System` |   | Time-only struct — port |
| 184 | System.Private.CoreLib/src/System/TimeoutException.cs | `System` |   | TimeoutException — port |
| 185 | System.Private.CoreLib/src/System/TimeSpan.cs | `System` |   | TimeSpan struct — ported in sharp-runtime |
| 186 | System.Private.CoreLib/src/System/TimeZone.cs | `System` |   | Legacy TimeZone class — stub |
| 187 | System.Private.CoreLib/src/System/TimeZoneInfo.AdjustmentRule.cs | `System` |   | TimeZoneInfo.AdjustmentRule — part of TimeZoneInfo port |
| 188 | System.Private.CoreLib/src/System/TimeZoneInfo.Cache.cs | `System` |   | TimeZoneInfo cache — internal implementation |
| 189 | System.Private.CoreLib/src/System/TimeZoneInfo.cs | `System` |   | TimeZoneInfo — partially ported (POSIX-only) |
| 190 | System.Private.CoreLib/src/System/TimeZoneInfo.FullGlobalizationData.cs | `System` |   | TimeZoneInfo globalization — reference |
| 191 | System.Private.CoreLib/src/System/TimeZoneInfo.MinimalGlobalizationData.cs | `System` |   | TimeZoneInfo minimal data — reference |
| 192 | System.Private.CoreLib/src/System/TimeZoneInfo.StringSerializer.cs | `System` |   | TimeZoneInfo serialization — port |
| 193 | System.Private.CoreLib/src/System/TimeZoneInfo.TransitionTime.cs | `System` |   | TimeZoneInfo.TransitionTime — port |
| 194 | System.Private.CoreLib/src/System/TimeZoneNotFoundException.cs | `System` |   | TimeZoneNotFoundException — port |
| 195 | System.Private.CoreLib/src/System/Tuple.cs | `System` |   | Tuple<T1..T8> — port (maps to std::tuple) |
| 196 | System.Private.CoreLib/src/System/TupleExtensions.cs | `System` |   | Tuple deconstruct extensions — port |
| 197 | System.Private.CoreLib/src/System/TypeAccessException.cs | `System` |   | Reflection exception — stub |
| 198 | System.Private.CoreLib/src/System/TypeCode.cs | `System` |   | TypeCode enum — port |
| 199 | System.Private.CoreLib/src/System/Type.cs | `System` |   | System.Type — CLR reflection; stub |
| 200 | System.Private.CoreLib/src/System/Type.Enum.cs | `System` |   | Type.GetEnumNames etc. — CLR reflection; stub |
| 201 | System.Private.CoreLib/src/System/Type.Helpers.cs | `System` |   | Type helper methods — CLR reflection; stub |
| 202 | System.Private.CoreLib/src/System/TypeInitializationException.cs | `System` |   | TypeInitializationException — stub |
| 203 | System.Private.CoreLib/src/System/TypeLoadException.cs | `System` |   | CLR exception — stub |
| 204 | System.Private.CoreLib/src/System/TypeUnloadedException.cs | `System` |   | CLR exception — stub |
| 205 | System.Private.CoreLib/src/System/UInt128.cs | `System` |   | Unsigned 128-bit integer — port using unsigned __int128 |
| 206 | System.Private.CoreLib/src/System/UInt16.cs | `System` |   | Primitive ushort (uint16_t) — port ToString/Parse |
| 207 | System.Private.CoreLib/src/System/UInt32.cs | `System` |   | Primitive uint (uint32_t) — port ToString/Parse |
| 208 | System.Private.CoreLib/src/System/UInt64.cs | `System` |   | Primitive ulong (uint64_t) — port ToString/Parse |
| 209 | System.Private.CoreLib/src/System/UIntPtr.cs | `System` |   | Unsigned pointer-sized integer — maps to uintptr_t |
| 210 | System.Private.CoreLib/src/System/UnauthorizedAccessException.cs | `System` |   | UnauthorizedAccessException — port (for file I/O) |
| 211 | System.Private.CoreLib/src/System/UnhandledExceptionEventArgs.cs | `System` |   | UnhandledException event args — port |
| 212 | System.Private.CoreLib/src/System/UnhandledExceptionEventHandler.cs | `System` |   | UnhandledException handler — port |
| 213 | System.Private.CoreLib/src/System/ValueTuple.cs | `System` |   | ValueTuple — port (maps to std::tuple) |
| 214 | System.Private.CoreLib/src/System/Version.cs | `System` |   | Version (major.minor.build.revision) — port |
| 215 | System.Private.CoreLib/src/System/WeakReference.cs | `System` |   | WeakReference — port using std::weak_ptr |
| 216 | System.Private.CoreLib/src/System/WeakReference.T.cs | `System` |   | WeakReference<T> — port using std::weak_ptr |
| 217 | System.Private.Uri/src/System/GenericUriParser.cs | `System` |   | Generic URI parser — port |
| 218 | System.Private.Uri/src/System/UriBuilder.cs | `System` |   | UriBuilder — port |
| 219 | System.Private.Uri/src/System/UriCreationOptions.cs | `System` |   | UriCreationOptions — port |
| 220 | System.Private.Uri/src/System/Uri.cs | `System` |   | Uri class — port |
| 221 | System.Private.Uri/src/System/UriEnumTypes.cs | `System` |   | Uri enum types (UriKind etc.) — port |
| 222 | System.Private.Uri/src/System/UriExt.cs | `System` |   | Uri extension methods — port |
| 223 | System.Private.Uri/src/System/UriFormatException.cs | `System` |   | UriFormatException — port |
| 224 | System.Private.Uri/src/System/UriHostNameType.cs | `System` |   | UriHostNameType enum — port |
| 225 | System.Private.Uri/src/System/UriParserTemplates.cs | `System` |   | URI parser templates — port |
| 226 | System.Private.Uri/src/System/UriPartial.cs | `System` |   | UriPartial enum — port |
| 227 | System.Private.Uri/src/System/UriScheme.cs | `System` |   | URI scheme constants — port |
| 228 | System.Private.Uri/src/System/UriSyntax.cs | `System` |   | URI syntax — port |
| 229 | System.Runtime.InteropServices/ref/System.Runtime.InteropServices.cs | `System` |   | Public API surface definition — use as porting reference |
| 230 | System.Runtime/ref/System.Runtime.cs | `System` |   | Public API surface definition — use as porting reference |
| 231 | System.Security.Permissions/ref/System.Security.Permissions.cs | `System` |   | Public API surface definition — use as porting reference |
| 232 | System.Threading.Thread/ref/System.Threading.Thread.cs | `System` |   | Public API surface definition — use as porting reference |
