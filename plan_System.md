# plan_System.md — namespace System .cs Files
All .cs files from dotnet/runtime (`/rv/tmp/runtime/src/libraries/`) that belong to namespace `System` (not sub-namespaces).
Status values: `ported`, `in_progress`, `todo`, `ignore`

Reference source: dotnet/runtime, MIT License

---

| File | Namespace | Status | Note |
|------|-----------|--------|------|
| Common/src/System/CharArrayHelpers.cs | `System` | | Internal char array helpers — not needed |
| Common/src/System/Console/ConsoleUtils.cs | `System` | | Console utility helpers — not needed (internal)
| Common/src/System/CSharpHelpers.cs | `System` | | CodeDom C# helpers — not needed |
| Common/src/System/ExceptionPolyfills.cs | `System` | | Polyfill for older TFMs — not needed |
| Common/src/System/Experimentals.cs | `System` | | Experimental API markers — not needed |
| Common/src/System/HashCodeRandomization.cs | `System` | | Hash code randomization — not needed |
| Common/src/System/HexConverter.cs | `System` | | Hex encoding helpers — port |
| Common/src/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| Common/src/System/LocalAppContextSwitches.Common.cs | `System` | | Runtime feature switches — not needed in C++ |
| Common/src/System/MemoryExtensionsPolyfills.cs | `System` | | Polyfill for older TFMs — not needed |
| Common/src/System/Memory/FixedBufferExtensions.cs | `System` | | Fixed buffer extensions — CLR-specific; not needed |
| Common/src/System/Net/LocalAppContextSwitches.Net.cs | `System` | | Runtime feature switches — not needed in C++ |
| Common/src/System/NotImplemented.cs | `System` | | Internal NotImplemented stubs — not needed |
| Common/src/System/NullableBool.cs | `System` | | Internal nullable bool — not needed |
| Common/src/System/Number.Formatting.Common.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| Common/src/System/Number.NumberBuffer.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| Common/src/System/Number.Parsing.Common.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| Common/src/System/Obsoletions.cs | `System` | | Obsoletions catalog — not needed |
| Common/src/System/Sha1ForNonSecretPurposes.cs | `System` | | Internal SHA1 (non-crypto) — not needed |
| Common/src/System/SR.cs | `System` | | String resources / assembly info — not needed |
| Common/src/System/StringPolyfills.cs | `System` | | Polyfill for older TFMs — not needed |
| Common/src/System/StrongToWeakReference.cs | `System` | | Internal weak reference helper — not needed |
| Common/src/System/TimeProvider.cs | `System` | | TimeProvider — port (abstract time source) |
| Common/tests/System/DateTimeTestHelpers.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/EnumTypes.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/GenericMathHelpers.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/MockType.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/NonRuntimeType.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/ObjectCloner.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/RandomDataGenerator.cs | `System` | | Test infrastructure — skip |
| Common/tests/System/ShouldNotBeInvokedException.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/AdminHelpers.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/AssemblyPathHelper.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/AssertExtensions.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/DisableParallelization.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/LineEndingsHelper.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/PlatformDetection.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/PlatformDetection.Unix.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/PlatformDetection.Windows.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/RetryHelper.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/TestEnvironment.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/WindowsIdentityFixture.cs | `System` | | Test infrastructure — skip |
| Common/tests/TestUtilities/System/WindowsTestFileShare.cs | `System` | | Test infrastructure — skip |
| Microsoft.Bcl.Memory/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| Microsoft.Bcl.Numerics/src/System/MathF.cs | `System` | | Single-precision math — port via cmath |
| System.ComponentModel.Composition/tests/System/LazyHelper.cs | `System` | | Test infrastructure — skip |
| System.ComponentModel.Composition/tests/System/ReferenceTracker.cs | `System` | | Test infrastructure — skip |
| System.ComponentModel.Composition/tests/System/TypeExtensions.cs | `System` | | Test infrastructure — skip |
| System.ComponentModel/ref/System.ComponentModel.cs | `System` | | Public API surface definition — use as porting reference |
| System.ComponentModel/src/System/IServiceProvider.cs | `System` | | IServiceProvider interface — port |
| System.ComponentModel.TypeConverter/ref/System.ComponentModel.TypeConverter.cs | `System` | | Public API surface definition — use as porting reference |
| System.ComponentModel.TypeConverter/src/System/ComponentModel/UriTypeConverter.cs | `System` | | UriTypeConverter — port with ComponentModel |
| System.ComponentModel.TypeConverter/src/System/InvariantComparer.cs | `System` | | Internal invariant string comparer — not needed |
| System.Configuration.ConfigurationManager/ref/System.Configuration.ConfigurationManager.cs | `System` | | Public API surface definition — use as porting reference |
| System.Configuration.ConfigurationManager/src/System/UriIdnScope.cs | `System` | | IDN scope enum — part of Uri port |
| System.Console/ref/System.Console.cs | `System` | | Public API surface definition — use as porting reference |
| System.Console/src/System/ConsoleCancelEventArgs.cs | `System` | | Ctrl+C event args — port |
| System.Console/src/System/ConsoleColor.cs | `System` | | ConsoleColor enum — ported in sharp-runtime |
| System.Console/src/System/Console.cs | `System` | | Console I/O — ported in sharp-runtime |
| System.Console/src/System/ConsoleKey.cs | `System` | | ConsoleKey enum — port |
| System.Console/src/System/ConsoleKeyInfo.cs | `System` | | ConsoleKeyInfo struct — port |
| System.Console/src/System/ConsoleModifiers.cs | `System` | | ConsoleModifiers enum — port |
| System.Console/src/System/ConsolePal.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| System.Console/src/System/ConsolePal.Browser.cs | `System` | | Platform-specific (Browser) — handle via #ifdef in C++ |
| System.Console/src/System/ConsolePal.iOS.cs | `System` | | Platform-specific (iOS) — handle via #ifdef in C++ |
| System.Console/src/System/ConsolePal.Unix.ConsoleStream.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Console/src/System/ConsolePal.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Console/src/System/ConsolePal.Wasi.cs | `System` | | Platform-specific (Wasi) — handle via #ifdef in C++ |
| System.Console/src/System/ConsolePal.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Console/src/System/ConsoleSpecialKey.cs | `System` | | ConsoleSpecialKey enum — port |
| System.Console/src/System/TermInfo.cs | `System` | | Unix terminal info — not needed |
| System.Console/tests/ManualTests/ManualTests.cs | `System` | | Test infrastructure — skip |
| System.Data.Common/src/System/Data/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Data.Common/src/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| System.Data.Odbc/src/Common/System/Data/Common/AdapterUtil.Odbc.cs | `System` | | ODBC adapter util — not needed |
| System.Data.Odbc/src/Common/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| System.Data.OleDb/src/System/Data/Common/SR.cs | `System` | | String resources / assembly info — not needed |
| System.Diagnostics.DiagnosticSource/src/System/Diagnostics/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Diagnostics.PerformanceCounter/src/misc/EnvironmentHelpers.cs | `System` | | Internal environment helpers — not needed |
| System.Diagnostics.Process/tests/LongPath/ClassDefinedInAssemblyWithAVeryLongPath.cs | `System` | | Test infrastructure — skip |
| System.DirectoryServices.Protocols/src/System/DirectoryServices/Protocols/ldap/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.IO.FileSystem.Watcher/src/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| System.IO.Hashing/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Memory.Data/ref/System.Memory.Data.cs | `System` | | Public API surface definition — use as porting reference |
| System.Memory.Data/src/System/BinaryData.cs | `System` | | BinaryData — port |
| System.Memory/ref/System.Memory.cs | `System` | | Public API surface definition — use as porting reference |
| System.Memory/src/System/SequencePosition.cs | `System` | | SequencePosition (pipelines) — port with Buffers |
| System.Memory/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Memory/tests/TestHelpers.cs | `System` | | Test infrastructure — skip |
| System.Memory/tests/TestMemory.cs | `System` | | Test infrastructure — skip |
| System.Memory/tests/TInt.cs | `System` | | Test infrastructure — skip |
| System.Net.Http.Json/src/System/ArraySegmentExtensions.netstandard.cs | `System` | | ArraySegment extensions for netstandard compat — not needed |
| System.Net.HttpListener/src/System/Net/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Net.Http/src/System/Net/Http/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Net.Quic/src/System/Net/Quic/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Net.Security/src/System/Net/Security/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Numerics.Tensors/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Private.CoreLib/gen/ProductVersionInfoGenerator.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/ref/System.Private.CoreLib.ManualShimTypeForwards.cs | `System` | | Public API surface definition — use as porting reference |
| System.Private.CoreLib/src/System/AccessViolationException.cs | `System` | | CLR memory access violation — stub |
| System.Private.CoreLib/src/System/Action.cs | `System` | | Action<> delegates — port (maps to std::function<void(...)>) |
| System.Private.CoreLib/src/System/Activator.cs | `System` | | Activator.CreateInstance — CLR reflection; stub |
| System.Private.CoreLib/src/System/Activator.RuntimeType.cs | `System` | | Activator reflection impl — CLR internal; stub |
| System.Private.CoreLib/src/System/AggregateException.cs | `System` | | AggregateException — port (for Task errors) |
| System.Private.CoreLib/src/System/AppContext.AnyOS.cs | `System` | | AppContext cross-platform impl — part of AppContext port |
| System.Private.CoreLib/src/System/AppContext.Browser.cs | `System` | | Platform-specific (Browser) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/AppContextConfigHelper.cs | `System` | | AppContext config helper — not needed |
| System.Private.CoreLib/src/System/AppContext.cs | `System` | | AppContext — stub (partially in sharp-runtime) |
| System.Private.CoreLib/src/System/AppDomain.cs | `System` | | AppDomain — stub (partially in sharp-runtime) |
| System.Private.CoreLib/src/System/AppDomainSetup.cs | `System` | | AppDomainSetup — stub |
| System.Private.CoreLib/src/System/AppDomain.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/AppDomainUnloadedException.cs | `System` | | CLR AppDomain exception — stub |
| System.Private.CoreLib/src/System/AppDomain.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/ApplicationException.cs | `System` | | ApplicationException — port |
| System.Private.CoreLib/src/System/ApplicationId.cs | `System` | | ClickOnce ApplicationId — not needed |
| System.Private.CoreLib/src/System/ArgumentException.cs | `System` | | ArgumentException — ported |
| System.Private.CoreLib/src/System/ArgumentNullException.cs | `System` | | ArgumentNullException — ported |
| System.Private.CoreLib/src/System/ArgumentOutOfRangeException.cs | `System` | | ArgumentOutOfRangeException — ported |
| System.Private.CoreLib/src/System/ArithmeticException.cs | `System` | | ArithmeticException — port |
| System.Private.CoreLib/src/System/Array.cs | `System` | | Array type — partial port (C++ uses std::vector/std::array) |
| System.Private.CoreLib/src/System/Array.Enumerators.cs | `System` | | Array enumerator — port |
| System.Private.CoreLib/src/System/ArraySegment.cs | `System` | | ArraySegment<T> — port (wraps array slice) |
| System.Private.CoreLib/src/System/ArrayTypeMismatchException.cs | `System` | | ArrayTypeMismatchException — stub |
| System.Private.CoreLib/src/System/AssemblyLoadEventArgs.cs | `System` | | Assembly load event — stub |
| System.Private.CoreLib/src/System/AssemblyLoadEventHandler.cs | `System` | | Assembly load handler — stub |
| System.Private.CoreLib/src/System/AsyncCallback.cs | `System` | | AsyncCallback delegate — port (legacy async) |
| System.Private.CoreLib/src/System/Attribute.cs | `System` | | System.Attribute — CLR metadata; stub |
| System.Private.CoreLib/src/System/AttributeTargets.cs | `System` | | AttributeTargets enum — stub |
| System.Private.CoreLib/src/System/AttributeUsageAttribute.cs | `System` | | AttributeUsageAttribute — stub |
| System.Private.CoreLib/src/System/BadImageFormatException.cs | `System` | | BadImageFormatException — stub |
| System.Private.CoreLib/src/System/BitConverter.cs | `System` | | BitConverter — port |
| System.Private.CoreLib/src/System/Boolean.cs | `System` | | Primitive bool — C++ has native bool; port ToString/Parse |
| System.Private.CoreLib/src/System/Buffer.cs | `System` | | Buffer.BlockCopy etc. — port (memcpy wrapper) |
| System.Private.CoreLib/src/System/ByReference.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/Byte.cs | `System` | | Primitive byte (uint8_t) — port ToString/Parse/formatting |
| System.Private.CoreLib/src/System/CannotUnloadAppDomainException.cs | `System` | | CLR-specific exception — stub |
| System.Private.CoreLib/src/System/Char.cs | `System` | | Unicode character — port; maps to char32_t |
| System.Private.CoreLib/src/System/CharEnumerator.cs | `System` | | String character enumerator — port |
| System.Private.CoreLib/src/System/CLSCompliantAttribute.cs | `System` | | CLSCompliant attribute — stub |
| System.Private.CoreLib/src/System/ComAwareWeakReference.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/Context.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/Convert.cs | `System` | | Convert static class — port |
| System.Private.CoreLib/src/System/CoreLib.cs | `System` | | String resources / assembly info — not needed |
| System.Private.CoreLib/src/System/CurrentSystemTimeZone.cs | `System` | | Legacy CurrentSystemTimeZone — stub |
| System.Private.CoreLib/src/System/DataMisalignedException.cs | `System` | | DataMisalignedException — stub |
| System.Private.CoreLib/src/System/DateOnly.cs | `System` | | Date-only struct — port |
| System.Private.CoreLib/src/System/DateTime.cs | `System` | | DateTime struct — ported in sharp-runtime |
| System.Private.CoreLib/src/System/DateTimeKind.cs | `System` | | DateTimeKind enum — ported |
| System.Private.CoreLib/src/System/DateTimeOffset.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/DateTimeOffset.cs | `System` | | DateTimeOffset struct — ported in sharp-runtime |
| System.Private.CoreLib/src/System/DateTimeOffset.NonAndroid.cs | `System` | | Platform-specific (NonAndroid) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/DateTime.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/DateTime.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/DayOfWeek.cs | `System` | | DayOfWeek enum — ported |
| System.Private.CoreLib/src/System/DBNull.cs | `System` | | DBNull — not needed (database type) |
| System.Private.CoreLib/src/System/Decimal.cs | `System` | | 128-bit decimal — port (already in sharp-runtime) |
| System.Private.CoreLib/src/System/Decimal.DecCalc.cs | `System` | | Decimal arithmetic internals — part of Decimal port |
| System.Private.CoreLib/src/System/DefaultBinder.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/Delegate.cs | `System` | | Delegate — CLR-specific; port event pattern only |
| System.Private.CoreLib/src/System/DivideByZeroException.cs | `System` | | DivideByZeroException — port |
| System.Private.CoreLib/src/System/DllNotFoundException.cs | `System` | | DllNotFoundException — port |
| System.Private.CoreLib/src/System/Double.cs | `System` | | double (64-bit) — port ToString/Parse/formatting |
| System.Private.CoreLib/src/System/DuplicateWaitObjectException.cs | `System` | | DuplicateWaitObjectException — stub |
| System.Private.CoreLib/src/System/Empty.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/EntryPointNotFoundException.cs | `System` | | EntryPointNotFoundException — stub |
| System.Private.CoreLib/src/System/Enum.cs | `System` | | Enum base class — port (ToString, Parse, GetValues) |
| System.Private.CoreLib/src/System/Enum.EnumInfo.cs | `System` | | Enum metadata cache — CLR internal; not needed |
| System.Private.CoreLib/src/System/Environment.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.Browser.cs | `System` | | Platform-specific (Browser) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.cs | `System` | | Environment class — partially ported |
| System.Private.CoreLib/src/System/Environment.FreeBSD.cs | `System` | | Platform-specific (FreeBSD) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.GetFolderPathCore.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/Environment.Haiku.cs | `System` | | Platform-specific (Haiku) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.iOS.cs | `System` | | Platform-specific (iOS) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.Linux.cs | `System` | | Linux-specific environment impl — reference for C++ Linux build |
| System.Private.CoreLib/src/System/Environment.NoRegistry.cs | `System` | | Non-Windows registry stub — not needed |
| System.Private.CoreLib/src/System/Environment.OSVersion.MacCatalyst.cs | `System` | | Platform-specific (MacCatalyst) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.OSVersion.OSX.cs | `System` | | macOS OSVersion impl — platform-specific; handle via #ifdef |
| System.Private.CoreLib/src/System/Environment.OSVersion.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/Environment.OSX.cs | `System` | | macOS environment impl — platform-specific; handle via #ifdef |
| System.Private.CoreLib/src/System/Environment.SpecialFolder.cs | `System` | | SpecialFolder enum — port |
| System.Private.CoreLib/src/System/Environment.SpecialFolderOption.cs | `System` | | SpecialFolderOption enum — port |
| System.Private.CoreLib/src/System/Environment.SunOS.cs | `System` | | Platform-specific (SunOS) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/Environment.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/Environment.UnixOrBrowser.cs | `System` | | POSIX/Wasm environment impl — reference for C++ POSIX build |
| System.Private.CoreLib/src/System/Environment.Variables.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/Environment.Variables.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/EnvironmentVariableTarget.cs | `System` | | EnvironmentVariableTarget enum — port |
| System.Private.CoreLib/src/System/Environment.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/EventArgs.cs | `System` | | EventArgs base class — port |
| System.Private.CoreLib/src/System/EventHandler.cs | `System` | | EventHandler delegate — port (maps to std::function) |
| System.Private.CoreLib/src/System/Exception.cs | `System` | | Base Exception class — ported in sharp-runtime |
| System.Private.CoreLib/src/System/ExecutionEngineException.cs | `System` | | CLR internal exception — stub |
| System.Private.CoreLib/src/System/FieldAccessException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/FlagsAttribute.cs | `System` | | FlagsAttribute — port (useful for enum flags) |
| System.Private.CoreLib/src/System/FormatException.cs | `System` | | FormatException — ported |
| System.Private.CoreLib/src/System/FormattableString.cs | `System` | | FormattableString (interpolated strings) — low priority |
| System.Private.CoreLib/src/System/Function.cs | `System` | | Func<> delegates — port (maps to std::function<R(...)>) |
| System.Private.CoreLib/src/System/GC.cs | `System` | | GC class — no-op stubs in C++ (no GC needed) |
| System.Private.CoreLib/src/System/GCMemoryInfo.cs | `System` | | GC memory info — stub |
| System.Private.CoreLib/src/System/Gen2GcCallback.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/Globalization/DateTimeFormat.cs | `System` | | DateTime formatting internals — port with Globalization |
| System.Private.CoreLib/src/System/Globalization/DateTimeParse.cs | `System` | | DateTime parsing internals — port with Globalization |
| System.Private.CoreLib/src/System/Guid.cs | `System` | | Guid struct — port |
| System.Private.CoreLib/src/System/Guid.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/Guid.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/Half.cs | `System` | | Half-precision float — port using __fp16 or std::float16_t |
| System.Private.CoreLib/src/System/HashCode.cs | `System` | | HashCode combining — port |
| System.Private.CoreLib/src/System/IAsyncDisposable.cs | `System` | | IAsyncDisposable — port (async cleanup) |
| System.Private.CoreLib/src/System/IAsyncResult.cs | `System` | | IAsyncResult — port (legacy async pattern) |
| System.Private.CoreLib/src/System/ICloneable.cs | `System` | | ICloneable — port |
| System.Private.CoreLib/src/System/IComparable.cs | `System` | | IComparable — port (maps to operator<=>) |
| System.Private.CoreLib/src/System/IConvertible.cs | `System` | | IConvertible — port |
| System.Private.CoreLib/src/System/ICustomFormatter.cs | `System` | | ICustomFormatter — port |
| System.Private.CoreLib/src/System/IDisposable.cs | `System` | | IDisposable — port as destructor pattern |
| System.Private.CoreLib/src/System/IEquatable.cs | `System` | | IEquatable<T> — port (maps to operator==) |
| System.Private.CoreLib/src/System/IFormatProvider.cs | `System` | | IFormatProvider — port |
| System.Private.CoreLib/src/System/IFormattable.cs | `System` | | IFormattable — port |
| System.Private.CoreLib/src/System/Index.cs | `System` | | System.Index (^operator) — port |
| System.Private.CoreLib/src/System/IndexOutOfRangeException.cs | `System` | | IndexOutOfRangeException — ported |
| System.Private.CoreLib/src/System/InsufficientExecutionStackException.cs | `System` | | CLR stack exception — stub |
| System.Private.CoreLib/src/System/InsufficientMemoryException.cs | `System` | | InsufficientMemoryException — stub |
| System.Private.CoreLib/src/System/Int128.cs | `System` | | 128-bit integer — port using __int128 or custom impl |
| System.Private.CoreLib/src/System/Int16.cs | `System` | | Primitive short (int16_t) — port ToString/Parse |
| System.Private.CoreLib/src/System/Int32.cs | `System` | | Primitive int (int32_t) — port ToString/Parse/formatting |
| System.Private.CoreLib/src/System/Int64.cs | `System` | | Primitive long (int64_t) — port ToString/Parse |
| System.Private.CoreLib/src/System/IntPtr.cs | `System` | | Pointer-sized integer — maps to intptr_t |
| System.Private.CoreLib/src/System/InvalidCastException.cs | `System` | | InvalidCastException — ported |
| System.Private.CoreLib/src/System/InvalidOperationException.cs | `System` | | InvalidOperationException — ported |
| System.Private.CoreLib/src/System/InvalidProgramException.cs | `System` | | CLR IL exception — stub |
| System.Private.CoreLib/src/System/InvalidTimeZoneException.cs | `System` | | InvalidTimeZoneException — port |
| System.Private.CoreLib/src/System/IObservable.cs | `System` | | IObservable<T> — port (reactive pattern) |
| System.Private.CoreLib/src/System/IObserver.cs | `System` | | IObserver<T> — port (reactive pattern) |
| System.Private.CoreLib/src/System/IParsable.cs | `System` | | IParsable<T> — port (static Parse interface) |
| System.Private.CoreLib/src/System/IProgress.cs | `System` | | IProgress<T> interface — port |
| System.Private.CoreLib/src/System/ISpanFormattable.cs | `System` | | ISpanFormattable — port |
| System.Private.CoreLib/src/System/ISpanParsable.cs | `System` | | ISpanParsable<T> — port |
| System.Private.CoreLib/src/System/IUtf8SpanFormattable.cs | `System` | | IUtf8SpanFormattable — port |
| System.Private.CoreLib/src/System/IUtf8SpanParsable.cs | `System` | | IUtf8SpanParsable<T> — port |
| System.Private.CoreLib/src/System/IUtfChar.cs | `System` | | Internal UTF char interface — not needed |
| System.Private.CoreLib/src/System/Lazy.cs | `System` | | Lazy<T> — port (thread-safe lazy init) |
| System.Private.CoreLib/src/System/LazyOfTTMetadata.cs | `System` | | Lazy<T,TMetadata> impl — port if needed |
| System.Private.CoreLib/src/System/LoaderOptimizationAttribute.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/LoaderOptimization.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Private.CoreLib/src/System/LocalDataStoreSlot.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/MarshalByRefObject.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/Marvin.cs | `System` | | Marvin32 string hash (internal) — not needed |
| System.Private.CoreLib/src/System/Marvin.OrdinalIgnoreCase.cs | `System` | | Marvin32 string hash (internal) — not needed |
| System.Private.CoreLib/src/System/Math.cs | `System` | | Math functions — ported in sharp-runtime via cmath |
| System.Private.CoreLib/src/System/Math.DivModInt.cs | `System` | | Integer DivMod helpers — port |
| System.Private.CoreLib/src/System/MathF.cs | `System` | | Single-precision math — port via cmath |
| System.Private.CoreLib/src/System/MemberAccessException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/Memory.cs | `System` | | Memory<T> — port or map to std::vector slice |
| System.Private.CoreLib/src/System/MemoryDebugView.cs | `System` | | Debugger view helper — not needed |
| System.Private.CoreLib/src/System/MemoryExtensions.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| System.Private.CoreLib/src/System/MemoryExtensions.Globalization.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| System.Private.CoreLib/src/System/MemoryExtensions.Globalization.Utf8.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| System.Private.CoreLib/src/System/MemoryExtensions.Trim.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| System.Private.CoreLib/src/System/MemoryExtensions.Trim.Utf8.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| System.Private.CoreLib/src/System/MethodAccessException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/MidpointRounding.cs | `System` | | MidpointRounding enum — port |
| System.Private.CoreLib/src/System/MissingFieldException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/MissingMemberException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/MissingMethodException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/MulticastDelegate.cs | `System` | | MulticastDelegate — port (event multicasting) |
| System.Private.CoreLib/src/System/MulticastNotSupportedException.cs | `System` | | Delegate exception — stub |
| System.Private.CoreLib/src/System/NonSerializedAttribute.cs | `System` | | NonSerialized attribute — stub |
| System.Private.CoreLib/src/System/NotFiniteNumberException.cs | `System` | | NotFiniteNumberException — port |
| System.Private.CoreLib/src/System/NotImplementedException.cs | `System` | | NotImplementedException — ported |
| System.Private.CoreLib/src/System/NotSupportedException.cs | `System` | | NotSupportedException — ported |
| System.Private.CoreLib/src/System/Nullable.cs | `System` | | Nullable<T> — map to std::optional<T> in C++ |
| System.Private.CoreLib/src/System/NullReferenceException.cs | `System` | | NullReferenceException — ported |
| System.Private.CoreLib/src/System/Number.BigInteger.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Number.DiyFp.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Number.Dragon4.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Number.Formatting.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Number.Grisu3.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Number.NumberToFloatingPointBits.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Number.Parsing.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Private.CoreLib/src/System/Object.cs | `System` | | System.Object base — stub (C++ uses inheritance differently) |
| System.Private.CoreLib/src/System/ObjectDisposedException.cs | `System` | | ObjectDisposedException — ported |
| System.Private.CoreLib/src/System/ObsoleteAttribute.cs | `System` | | ObsoleteAttribute — stub (use [[deprecated]] in C++) |
| System.Private.CoreLib/src/System/OperatingSystem.cs | `System` | | OperatingSystem info — port |
| System.Private.CoreLib/src/System/OperationCanceledException.cs | `System` | | OperationCanceledException — port (for Task/CancellationToken) |
| System.Private.CoreLib/src/System/OutOfMemoryException.cs | `System` | | OutOfMemoryException — stub (C++ throws std::bad_alloc) |
| System.Private.CoreLib/src/System/OverflowException.cs | `System` | | OverflowException — port |
| System.Private.CoreLib/src/System/ParamArrayAttribute.cs | `System` | | params keyword attribute — stub |
| System.Private.CoreLib/src/System/ParseNumbers.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/PasteArguments.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/PasteArguments.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/PasteArguments.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/PlatformID.cs | `System` | | PlatformID enum — port |
| System.Private.CoreLib/src/System/PlatformNotSupportedException.cs | `System` | | PlatformNotSupportedException — ported |
| System.Private.CoreLib/src/System/Progress.cs | `System` | | IProgress<T>/Progress<T> — port |
| System.Private.CoreLib/src/System/Random.CompatImpl.cs | `System` | | Random compatibility impl — internal |
| System.Private.CoreLib/src/System/Random.cs | `System` | | Random class — ported in sharp-runtime |
| System.Private.CoreLib/src/System/Random.ImplBase.cs | `System` | | Random base impl — internal |
| System.Private.CoreLib/src/System/Random.Xoshiro128StarStarImpl.cs | `System` | | Xoshiro128** RNG — internal impl |
| System.Private.CoreLib/src/System/Random.Xoshiro256StarStarImpl.cs | `System` | | Xoshiro256** RNG — internal impl |
| System.Private.CoreLib/src/System/Range.cs | `System` | | System.Range — port |
| System.Private.CoreLib/src/System/RankException.cs | `System` | | RankException — stub |
| System.Private.CoreLib/src/System/ReadOnlyMemory.cs | `System` | | ReadOnlyMemory<T> — port or map to const slice |
| System.Private.CoreLib/src/System/ReadOnlySpan.cs | `System` | | ReadOnlySpan<T> — map to const std::span in C++ |
| System.Private.CoreLib/src/System/ResolveEventArgs.cs | `System` | | Assembly resolve event — stub |
| System.Private.CoreLib/src/System/ResolveEventHandler.cs | `System` | | Assembly resolve handler — stub |
| System.Private.CoreLib/src/System/Runtime/InteropServices/ComAwareWeakReference.ComWrappers.cs | `System` | | COM interop — not needed |
| System.Private.CoreLib/src/System/RuntimeType.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/SByte.cs | `System` | | Primitive sbyte (int8_t) — port ToString/Parse/formatting |
| System.Private.CoreLib/src/System/SerializableAttribute.cs | `System` | | Serializable attribute — stub |
| System.Private.CoreLib/src/System/Single.cs | `System` | | float (32-bit) — port ToString/Parse/formatting |
| System.Private.CoreLib/src/System/Span.cs | `System` | | Span<T> — map to std::span in C++ |
| System.Private.CoreLib/src/System/SpanDebugView.cs | `System` | | Debugger view helper — not needed |
| System.Private.CoreLib/src/System/SpanHelpers.BinarySearch.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SpanHelpers.Byte.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SpanHelpers.ByteMemOps.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SpanHelpers.Char.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SpanHelpers.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SpanHelpers.Packed.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SpanHelpers.T.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| System.Private.CoreLib/src/System/SR.cs | `System` | | String resources / assembly info — not needed |
| System.Private.CoreLib/src/System/StackOverflowException.cs | `System` | | CLR stack overflow — stub only |
| System.Private.CoreLib/src/System/StartupHookProvider.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/StringComparer.cs | `System` | | String comparison strategies — port |
| System.Private.CoreLib/src/System/String.Comparison.cs | `System` | | String comparison methods — port |
| System.Private.CoreLib/src/System/StringComparison.cs | `System` | | StringComparison enum — port |
| System.Private.CoreLib/src/System/String.cs | `System` | | Core string type — partially ported in sharp-runtime |
| System.Private.CoreLib/src/System/String.Manipulation.cs | `System` | | String manipulation (Replace, Split, Trim etc.) — port |
| System.Private.CoreLib/src/System/StringNormalizationExtensions.cs | `System` | | Unicode normalization — low priority |
| System.Private.CoreLib/src/System/String.Searching.cs | `System` | | String search (IndexOf, Contains etc.) — port |
| System.Private.CoreLib/src/System/StringSplitOptions.cs | `System` | | StringSplitOptions enum — port |
| System.Private.CoreLib/src/System/SystemException.cs | `System` | | SystemException — ported |
| System.Private.CoreLib/src/System/ThreadAttributes.cs | `System` | | Thread attributes — stub |
| System.Private.CoreLib/src/System/ThreadStaticAttribute.cs | `System` | | ThreadStatic attribute — stub |
| System.Private.CoreLib/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Private.CoreLib/src/System/TimeOnly.cs | `System` | | Time-only struct — port |
| System.Private.CoreLib/src/System/TimeoutException.cs | `System` | | TimeoutException — port |
| System.Private.CoreLib/src/System/TimeSpan.cs | `System` | | TimeSpan struct — ported in sharp-runtime |
| System.Private.CoreLib/src/System/TimeZone.cs | `System` | | Legacy TimeZone class — stub |
| System.Private.CoreLib/src/System/TimeZoneInfo.AdjustmentRule.cs | `System` | | TimeZoneInfo.AdjustmentRule — part of TimeZoneInfo port |
| System.Private.CoreLib/src/System/TimeZoneInfo.Cache.cs | `System` | | TimeZoneInfo cache — internal implementation |
| System.Private.CoreLib/src/System/TimeZoneInfo.cs | `System` | | TimeZoneInfo — partially ported (POSIX-only) |
| System.Private.CoreLib/src/System/TimeZoneInfo.FullGlobalizationData.cs | `System` | | TimeZoneInfo globalization — reference |
| System.Private.CoreLib/src/System/TimeZoneInfo.FullGlobalizationData.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/TimeZoneInfo.MinimalGlobalizationData.cs | `System` | | TimeZoneInfo minimal data — reference |
| System.Private.CoreLib/src/System/TimeZoneInfo.StringSerializer.cs | `System` | | TimeZoneInfo serialization — port |
| System.Private.CoreLib/src/System/TimeZoneInfo.TransitionTime.cs | `System` | | TimeZoneInfo.TransitionTime — port |
| System.Private.CoreLib/src/System/TimeZoneInfo.Unix.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/TimeZoneInfo.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| System.Private.CoreLib/src/System/TimeZoneInfo.Unix.NonAndroid.cs | `System` | | Platform-specific (NonAndroid) — handle via #ifdef in C++ |
| System.Private.CoreLib/src/System/TimeZoneInfo.Windows.cs | `System` | | Windows-specific implementation — not needed |
| System.Private.CoreLib/src/System/TimeZoneNotFoundException.cs | `System` | | TimeZoneNotFoundException — port |
| System.Private.CoreLib/src/System/Tuple.cs | `System` | | Tuple<T1..T8> — port (maps to std::tuple) |
| System.Private.CoreLib/src/System/TupleExtensions.cs | `System` | | Tuple deconstruct extensions — port |
| System.Private.CoreLib/src/System/TupleSlim.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/TypeAccessException.cs | `System` | | Reflection exception — stub |
| System.Private.CoreLib/src/System/TypeCode.cs | `System` | | TypeCode enum — port |
| System.Private.CoreLib/src/System/Type.cs | `System` | | System.Type — CLR reflection; stub |
| System.Private.CoreLib/src/System/TypedReference.cs | `System` | | TypedReference — CLR internal; stub |
| System.Private.CoreLib/src/System/Type.Enum.cs | `System` | | Type.GetEnumNames etc. — CLR reflection; stub |
| System.Private.CoreLib/src/System/Type.Helpers.cs | `System` | | Type helper methods — CLR reflection; stub |
| System.Private.CoreLib/src/System/TypeInitializationException.cs | `System` | | TypeInitializationException — stub |
| System.Private.CoreLib/src/System/TypeLoadException.cs | `System` | | CLR exception — stub |
| System.Private.CoreLib/src/System/TypeUnloadedException.cs | `System` | | CLR exception — stub |
| System.Private.CoreLib/src/System/UInt128.cs | `System` | | Unsigned 128-bit integer — port using unsigned __int128 |
| System.Private.CoreLib/src/System/UInt16.cs | `System` | | Primitive ushort (uint16_t) — port ToString/Parse |
| System.Private.CoreLib/src/System/UInt32.cs | `System` | | Primitive uint (uint32_t) — port ToString/Parse |
| System.Private.CoreLib/src/System/UInt64.cs | `System` | | Primitive ulong (uint64_t) — port ToString/Parse |
| System.Private.CoreLib/src/System/UIntPtr.cs | `System` | | Unsigned pointer-sized integer — maps to uintptr_t |
| System.Private.CoreLib/src/System/UnauthorizedAccessException.cs | `System` | | UnauthorizedAccessException — port (for file I/O) |
| System.Private.CoreLib/src/System/UnhandledExceptionEventArgs.cs | `System` | | UnhandledException event args — port |
| System.Private.CoreLib/src/System/UnhandledExceptionEventHandler.cs | `System` | | UnhandledException handler — port |
| System.Private.CoreLib/src/System/UnitySerializationHolder.cs | `System` | | CLR internal / not applicable in C++ |
| System.Private.CoreLib/src/System/ValueTuple.cs | `System` | | ValueTuple — port (maps to std::tuple) |
| System.Private.CoreLib/src/System/Version.cs | `System` | | Version (major.minor.build.revision) — port |
| System.Private.CoreLib/src/System/Void.cs | `System` | | System.Void — not needed |
| System.Private.CoreLib/src/System/WeakReference.cs | `System` | | WeakReference — port using std::weak_ptr |
| System.Private.CoreLib/src/System/WeakReference.T.cs | `System` | | WeakReference<T> — port using std::weak_ptr |
| System.Private.Uri/src/System/DomainNameHelper.cs | `System` | | DNS domain name helpers — port with Net |
| System.Private.Uri/src/System/GenericUriParser.cs | `System` | | Generic URI parser — port |
| System.Private.Uri/src/System/IriHelper.cs | `System` | | IRI helper — port with Uri |
| System.Private.Uri/src/System/PercentEncodingHelper.cs | `System` | | URL percent-encoding — port |
| System.Private.Uri/src/System/UncNameHelper.cs | `System` | | UNC path helper — Windows-specific; stub |
| System.Private.Uri/src/System/UriBuilder.cs | `System` | | UriBuilder — port |
| System.Private.Uri/src/System/UriCreationOptions.cs | `System` | | UriCreationOptions — port |
| System.Private.Uri/src/System/Uri.cs | `System` | | Uri class — port |
| System.Private.Uri/src/System/UriEnumTypes.cs | `System` | | Uri enum types (UriKind etc.) — port |
| System.Private.Uri/src/System/UriExt.cs | `System` | | Uri extension methods — port |
| System.Private.Uri/src/System/UriFormatException.cs | `System` | | UriFormatException — port |
| System.Private.Uri/src/System/UriHelper.cs | `System` | | Uri internal helpers — port with Uri |
| System.Private.Uri/src/System/UriHostNameType.cs | `System` | | UriHostNameType enum — port |
| System.Private.Uri/src/System/UriParserTemplates.cs | `System` | | URI parser templates — port |
| System.Private.Uri/src/System/UriPartial.cs | `System` | | UriPartial enum — port |
| System.Private.Uri/src/System/UriScheme.cs | `System` | | URI scheme constants — port |
| System.Private.Uri/src/System/UriSyntax.cs | `System` | | URI syntax — port |
| System.Private.Uri/tests/UnitTests/Fakes/FakeUri.cs | `System` | | Test infrastructure — skip |
| System.Private.Xml/src/Misc/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| System.Reflection.MetadataLoadContext/src/System/Reflection/DefaultBinder.CanConvert.cs | `System` | | CLR reflection binder — not needed |
| System.Reflection.MetadataLoadContext/src/System/Reflection/DefaultBinder.cs | `System` | | CLR internal / not applicable in C++ |
| System.Reflection.MetadataLoadContext/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Runtime.InteropServices/ref/System.Runtime.InteropServices.cs | `System` | | Public API surface definition — use as porting reference |
| System.Runtime.Numerics/src/System/Number.BigInteger.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| System.Runtime.Numerics/src/System/Number.Polyfill.cs | `System` | | Polyfill for older TFMs — not needed |
| System.Runtime.Numerics/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Runtime/ref/System.Runtime.cs | `System` | | Public API surface definition — use as porting reference |
| System.Runtime.Serialization.Formatters/src/System/Runtime/Serialization/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Runtime/tests/System.Runtime.Extensions.Tests/System/ApplicationIdTests.cs | `System` | | Test infrastructure — skip |
| System.Runtime/tests/System.Runtime.Extensions.Tests/TestHelpers.cs | `System` | | Test infrastructure — skip |
| System.Security.Cryptography/src/System/Security/Cryptography/X509Certificates/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Security.Cryptography.Xml/src/System/Security/Cryptography/Xml/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| System.Security.Permissions/ref/System.Security.Permissions.cs | `System` | | Public API surface definition — use as porting reference |
| System.Security.Permissions/src/System/ApplicationIdentity.cs | `System` | | ClickOnce ApplicationIdentity — not needed |
| System.Text.Encodings.Web/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Text.Encodings.Web/tests/SR.cs | `System` | | Test infrastructure — skip |
| System.Text.Json/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| System.Text.RegularExpressions/gen/Stubs.cs | `System` | | Source generator stubs — not needed |
| System.Threading.Channels/src/System/VoidResult.cs | `System` | | Internal void result — not needed |
| System.Threading.Thread/ref/System.Threading.Thread.cs | `System` | | Public API surface definition — use as porting reference |
