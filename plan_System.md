# plan_System.md — namespace System .cs Files
All .cs files from dotnet/runtime (`/rv/tmp/runtime/src/libraries/`) that belong to namespace `System` (not sub-namespaces).
Status values: `ported`, `in_progress`, `todo`, `ignore`

Reference source: dotnet/runtime, MIT License

---

| # | File | Namespace | Status | Note |
|---|------|-----------|--------|------|
| 1 | Common/src/System/CharArrayHelpers.cs | `System` | | Internal char array helpers — not needed |
| 2 | Common/src/System/Console/ConsoleUtils.cs | `System` | | Console utility helpers — not needed (internal)
| 3 | Common/src/System/CSharpHelpers.cs | `System` | | CodeDom C# helpers — not needed |
| 4 | Common/src/System/ExceptionPolyfills.cs | `System` | | Polyfill for older TFMs — not needed |
| 5 | Common/src/System/Experimentals.cs | `System` | | Experimental API markers — not needed |
| 6 | Common/src/System/HashCodeRandomization.cs | `System` | | Hash code randomization — not needed |
| 7 | Common/src/System/HexConverter.cs | `System` | | Hex encoding helpers — port |
| 8 | Common/src/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| 9 | Common/src/System/LocalAppContextSwitches.Common.cs | `System` | | Runtime feature switches — not needed in C++ |
| 10 | Common/src/System/MemoryExtensionsPolyfills.cs | `System` | | Polyfill for older TFMs — not needed |
| 11 | Common/src/System/Memory/FixedBufferExtensions.cs | `System` | | Fixed buffer extensions — CLR-specific; not needed |
| 12 | Common/src/System/Net/LocalAppContextSwitches.Net.cs | `System` | | Runtime feature switches — not needed in C++ |
| 13 | Common/src/System/NotImplemented.cs | `System` | | Internal NotImplemented stubs — not needed |
| 14 | Common/src/System/NullableBool.cs | `System` | | Internal nullable bool — not needed |
| 15 | Common/src/System/Number.Formatting.Common.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 16 | Common/src/System/Number.NumberBuffer.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 17 | Common/src/System/Number.Parsing.Common.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 18 | Common/src/System/Obsoletions.cs | `System` | | Obsoletions catalog — not needed |
| 19 | Common/src/System/Sha1ForNonSecretPurposes.cs | `System` | | Internal SHA1 (non-crypto) — not needed |
| 20 | Common/src/System/SR.cs | `System` | | String resources / assembly info — not needed |
| 21 | Common/src/System/StringPolyfills.cs | `System` | | Polyfill for older TFMs — not needed |
| 22 | Common/src/System/StrongToWeakReference.cs | `System` | | Internal weak reference helper — not needed |
| 23 | Common/src/System/TimeProvider.cs | `System` | | TimeProvider — port (abstract time source) |
| 24 | Common/tests/System/DateTimeTestHelpers.cs | `System` | | Test infrastructure — skip |
| 25 | Common/tests/System/EnumTypes.cs | `System` | | Test infrastructure — skip |
| 26 | Common/tests/System/GenericMathHelpers.cs | `System` | | Test infrastructure — skip |
| 27 | Common/tests/System/MockType.cs | `System` | | Test infrastructure — skip |
| 28 | Common/tests/System/NonRuntimeType.cs | `System` | | Test infrastructure — skip |
| 29 | Common/tests/System/ObjectCloner.cs | `System` | | Test infrastructure — skip |
| 30 | Common/tests/System/RandomDataGenerator.cs | `System` | | Test infrastructure — skip |
| 31 | Common/tests/System/ShouldNotBeInvokedException.cs | `System` | | Test infrastructure — skip |
| 32 | Common/tests/TestUtilities/System/AdminHelpers.cs | `System` | | Test infrastructure — skip |
| 33 | Common/tests/TestUtilities/System/AssemblyPathHelper.cs | `System` | | Test infrastructure — skip |
| 34 | Common/tests/TestUtilities/System/AssertExtensions.cs | `System` | | Test infrastructure — skip |
| 35 | Common/tests/TestUtilities/System/DisableParallelization.cs | `System` | | Test infrastructure — skip |
| 36 | Common/tests/TestUtilities/System/LineEndingsHelper.cs | `System` | | Test infrastructure — skip |
| 37 | Common/tests/TestUtilities/System/PlatformDetection.cs | `System` | | Test infrastructure — skip |
| 38 | Common/tests/TestUtilities/System/PlatformDetection.Unix.cs | `System` | | Test infrastructure — skip |
| 39 | Common/tests/TestUtilities/System/PlatformDetection.Windows.cs | `System` | | Test infrastructure — skip |
| 40 | Common/tests/TestUtilities/System/RetryHelper.cs | `System` | | Test infrastructure — skip |
| 41 | Common/tests/TestUtilities/System/TestEnvironment.cs | `System` | | Test infrastructure — skip |
| 42 | Common/tests/TestUtilities/System/WindowsIdentityFixture.cs | `System` | | Test infrastructure — skip |
| 43 | Common/tests/TestUtilities/System/WindowsTestFileShare.cs | `System` | | Test infrastructure — skip |
| 44 | Microsoft.Bcl.Memory/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 45 | Microsoft.Bcl.Numerics/src/System/MathF.cs | `System` | | Single-precision math — port via cmath |
| 46 | System.ComponentModel.Composition/tests/System/LazyHelper.cs | `System` | | Test infrastructure — skip |
| 47 | System.ComponentModel.Composition/tests/System/ReferenceTracker.cs | `System` | | Test infrastructure — skip |
| 48 | System.ComponentModel.Composition/tests/System/TypeExtensions.cs | `System` | | Test infrastructure — skip |
| 49 | System.ComponentModel/ref/System.ComponentModel.cs | `System` | | Public API surface definition — use as porting reference |
| 50 | System.ComponentModel/src/System/IServiceProvider.cs | `System` | | IServiceProvider interface — port |
| 51 | System.ComponentModel.TypeConverter/ref/System.ComponentModel.TypeConverter.cs | `System` | | Public API surface definition — use as porting reference |
| 52 | System.ComponentModel.TypeConverter/src/System/ComponentModel/UriTypeConverter.cs | `System` | | UriTypeConverter — port with ComponentModel |
| 53 | System.ComponentModel.TypeConverter/src/System/InvariantComparer.cs | `System` | | Internal invariant string comparer — not needed |
| 54 | System.Configuration.ConfigurationManager/ref/System.Configuration.ConfigurationManager.cs | `System` | | Public API surface definition — use as porting reference |
| 55 | System.Configuration.ConfigurationManager/src/System/UriIdnScope.cs | `System` | | IDN scope enum — part of Uri port |
| 56 | System.Console/ref/System.Console.cs | `System` | | Public API surface definition — use as porting reference |
| 57 | System.Console/src/System/ConsoleCancelEventArgs.cs | `System` | | Ctrl+C event args — port |
| 58 | System.Console/src/System/ConsoleColor.cs | `System` | | ConsoleColor enum — ported in sharp-runtime |
| 59 | System.Console/src/System/Console.cs | `System` | | Console I/O — ported in sharp-runtime |
| 60 | System.Console/src/System/ConsoleKey.cs | `System` | | ConsoleKey enum — port |
| 61 | System.Console/src/System/ConsoleKeyInfo.cs | `System` | | ConsoleKeyInfo struct — port |
| 62 | System.Console/src/System/ConsoleModifiers.cs | `System` | | ConsoleModifiers enum — port |
| 63 | System.Console/src/System/ConsolePal.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| 64 | System.Console/src/System/ConsolePal.Browser.cs | `System` | | Platform-specific (Browser) — handle via #ifdef in C++ |
| 65 | System.Console/src/System/ConsolePal.iOS.cs | `System` | | Platform-specific (iOS) — handle via #ifdef in C++ |
| 66 | System.Console/src/System/ConsolePal.Unix.ConsoleStream.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 67 | System.Console/src/System/ConsolePal.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 68 | System.Console/src/System/ConsolePal.Wasi.cs | `System` | | Platform-specific (Wasi) — handle via #ifdef in C++ |
| 69 | System.Console/src/System/ConsolePal.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 70 | System.Console/src/System/ConsoleSpecialKey.cs | `System` | | ConsoleSpecialKey enum — port |
| 71 | System.Console/src/System/TermInfo.cs | `System` | | Unix terminal info — not needed |
| 72 | System.Console/tests/ManualTests/ManualTests.cs | `System` | | Test infrastructure — skip |
| 73 | System.Data.Common/src/System/Data/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 74 | System.Data.Common/src/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| 75 | System.Data.Odbc/src/Common/System/Data/Common/AdapterUtil.Odbc.cs | `System` | | ODBC adapter util — not needed |
| 76 | System.Data.Odbc/src/Common/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| 77 | System.Data.OleDb/src/System/Data/Common/SR.cs | `System` | | String resources / assembly info — not needed |
| 78 | System.Diagnostics.DiagnosticSource/src/System/Diagnostics/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 79 | System.Diagnostics.PerformanceCounter/src/misc/EnvironmentHelpers.cs | `System` | | Internal environment helpers — not needed |
| 80 | System.Diagnostics.Process/tests/LongPath/ClassDefinedInAssemblyWithAVeryLongPath.cs | `System` | | Test infrastructure — skip |
| 81 | System.DirectoryServices.Protocols/src/System/DirectoryServices/Protocols/ldap/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 82 | System.IO.FileSystem.Watcher/src/System/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| 83 | System.IO.Hashing/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 84 | System.Memory.Data/ref/System.Memory.Data.cs | `System` | | Public API surface definition — use as porting reference |
| 85 | System.Memory.Data/src/System/BinaryData.cs | `System` | | BinaryData — port |
| 86 | System.Memory/ref/System.Memory.cs | `System` | | Public API surface definition — use as porting reference |
| 87 | System.Memory/src/System/SequencePosition.cs | `System` | | SequencePosition (pipelines) — port with Buffers |
| 88 | System.Memory/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 89 | System.Memory/tests/TestHelpers.cs | `System` | | Test infrastructure — skip |
| 90 | System.Memory/tests/TestMemory.cs | `System` | | Test infrastructure — skip |
| 91 | System.Memory/tests/TInt.cs | `System` | | Test infrastructure — skip |
| 92 | System.Net.Http.Json/src/System/ArraySegmentExtensions.netstandard.cs | `System` | | ArraySegment extensions for netstandard compat — not needed |
| 93 | System.Net.HttpListener/src/System/Net/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 94 | System.Net.Http/src/System/Net/Http/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 95 | System.Net.Quic/src/System/Net/Quic/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 96 | System.Net.Security/src/System/Net/Security/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 97 | System.Numerics.Tensors/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 98 | System.Private.CoreLib/gen/ProductVersionInfoGenerator.cs | `System` | | CLR internal / not applicable in C++ |
| 99 | System.Private.CoreLib/ref/System.Private.CoreLib.ManualShimTypeForwards.cs | `System` | | Public API surface definition — use as porting reference |
| 100 | System.Private.CoreLib/src/System/AccessViolationException.cs | `System` | | CLR memory access violation — stub |
| 101 | System.Private.CoreLib/src/System/Action.cs | `System` | | Action<> delegates — port (maps to std::function<void(...)>) |
| 102 | System.Private.CoreLib/src/System/Activator.cs | `System` | | Activator.CreateInstance — CLR reflection; stub |
| 103 | System.Private.CoreLib/src/System/Activator.RuntimeType.cs | `System` | | Activator reflection impl — CLR internal; stub |
| 104 | System.Private.CoreLib/src/System/AggregateException.cs | `System` | | AggregateException — port (for Task errors) |
| 105 | System.Private.CoreLib/src/System/AppContext.AnyOS.cs | `System` | | AppContext cross-platform impl — part of AppContext port |
| 106 | System.Private.CoreLib/src/System/AppContext.Browser.cs | `System` | | Platform-specific (Browser) — handle via #ifdef in C++ |
| 107 | System.Private.CoreLib/src/System/AppContextConfigHelper.cs | `System` | | AppContext config helper — not needed |
| 108 | System.Private.CoreLib/src/System/AppContext.cs | `System` | | AppContext — stub (partially in sharp-runtime) |
| 109 | System.Private.CoreLib/src/System/AppDomain.cs | `System` | | AppDomain — stub (partially in sharp-runtime) |
| 110 | System.Private.CoreLib/src/System/AppDomainSetup.cs | `System` | | AppDomainSetup — stub |
| 111 | System.Private.CoreLib/src/System/AppDomain.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 112 | System.Private.CoreLib/src/System/AppDomainUnloadedException.cs | `System` | | CLR AppDomain exception — stub |
| 113 | System.Private.CoreLib/src/System/AppDomain.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 114 | System.Private.CoreLib/src/System/ApplicationException.cs | `System` | | ApplicationException — port |
| 115 | System.Private.CoreLib/src/System/ApplicationId.cs | `System` | | ClickOnce ApplicationId — not needed |
| 116 | System.Private.CoreLib/src/System/ArgumentException.cs | `System` | | ArgumentException — ported |
| 117 | System.Private.CoreLib/src/System/ArgumentNullException.cs | `System` | | ArgumentNullException — ported |
| 118 | System.Private.CoreLib/src/System/ArgumentOutOfRangeException.cs | `System` | | ArgumentOutOfRangeException — ported |
| 119 | System.Private.CoreLib/src/System/ArithmeticException.cs | `System` | | ArithmeticException — port |
| 120 | System.Private.CoreLib/src/System/Array.cs | `System` | | Array type — partial port (C++ uses std::vector/std::array) |
| 121 | System.Private.CoreLib/src/System/Array.Enumerators.cs | `System` | | Array enumerator — port |
| 122 | System.Private.CoreLib/src/System/ArraySegment.cs | `System` | | ArraySegment<T> — port (wraps array slice) |
| 123 | System.Private.CoreLib/src/System/ArrayTypeMismatchException.cs | `System` | | ArrayTypeMismatchException — stub |
| 124 | System.Private.CoreLib/src/System/AssemblyLoadEventArgs.cs | `System` | | Assembly load event — stub |
| 125 | System.Private.CoreLib/src/System/AssemblyLoadEventHandler.cs | `System` | | Assembly load handler — stub |
| 126 | System.Private.CoreLib/src/System/AsyncCallback.cs | `System` | | AsyncCallback delegate — port (legacy async) |
| 127 | System.Private.CoreLib/src/System/Attribute.cs | `System` | | System.Attribute — CLR metadata; stub |
| 128 | System.Private.CoreLib/src/System/AttributeTargets.cs | `System` | | AttributeTargets enum — stub |
| 129 | System.Private.CoreLib/src/System/AttributeUsageAttribute.cs | `System` | | AttributeUsageAttribute — stub |
| 130 | System.Private.CoreLib/src/System/BadImageFormatException.cs | `System` | | BadImageFormatException — stub |
| 131 | System.Private.CoreLib/src/System/BitConverter.cs | `System` | | BitConverter — port |
| 132 | System.Private.CoreLib/src/System/Boolean.cs | `System` | | Primitive bool — C++ has native bool; port ToString/Parse |
| 133 | System.Private.CoreLib/src/System/Buffer.cs | `System` | | Buffer.BlockCopy etc. — port (memcpy wrapper) |
| 134 | System.Private.CoreLib/src/System/ByReference.cs | `System` | | CLR internal / not applicable in C++ |
| 135 | System.Private.CoreLib/src/System/Byte.cs | `System` | | Primitive byte (uint8_t) — port ToString/Parse/formatting |
| 136 | System.Private.CoreLib/src/System/CannotUnloadAppDomainException.cs | `System` | | CLR-specific exception — stub |
| 137 | System.Private.CoreLib/src/System/Char.cs | `System` | | Unicode character — port; maps to char32_t |
| 138 | System.Private.CoreLib/src/System/CharEnumerator.cs | `System` | | String character enumerator — port |
| 139 | System.Private.CoreLib/src/System/CLSCompliantAttribute.cs | `System` | | CLSCompliant attribute — stub |
| 140 | System.Private.CoreLib/src/System/ComAwareWeakReference.cs | `System` | | CLR internal / not applicable in C++ |
| 141 | System.Private.CoreLib/src/System/Context.cs | `System` | | CLR internal / not applicable in C++ |
| 142 | System.Private.CoreLib/src/System/Convert.cs | `System` | | Convert static class — port |
| 143 | System.Private.CoreLib/src/System/CoreLib.cs | `System` | | String resources / assembly info — not needed |
| 144 | System.Private.CoreLib/src/System/CurrentSystemTimeZone.cs | `System` | | Legacy CurrentSystemTimeZone — stub |
| 145 | System.Private.CoreLib/src/System/DataMisalignedException.cs | `System` | | DataMisalignedException — stub |
| 146 | System.Private.CoreLib/src/System/DateOnly.cs | `System` | | Date-only struct — port |
| 147 | System.Private.CoreLib/src/System/DateTime.cs | `System` | | DateTime struct — ported in sharp-runtime |
| 148 | System.Private.CoreLib/src/System/DateTimeKind.cs | `System` | | DateTimeKind enum — ported |
| 149 | System.Private.CoreLib/src/System/DateTimeOffset.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| 150 | System.Private.CoreLib/src/System/DateTimeOffset.cs | `System` | | DateTimeOffset struct — ported in sharp-runtime |
| 151 | System.Private.CoreLib/src/System/DateTimeOffset.NonAndroid.cs | `System` | | Platform-specific (NonAndroid) — handle via #ifdef in C++ |
| 152 | System.Private.CoreLib/src/System/DateTime.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 153 | System.Private.CoreLib/src/System/DateTime.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 154 | System.Private.CoreLib/src/System/DayOfWeek.cs | `System` | | DayOfWeek enum — ported |
| 155 | System.Private.CoreLib/src/System/DBNull.cs | `System` | | DBNull — not needed (database type) |
| 156 | System.Private.CoreLib/src/System/Decimal.cs | `System` | | 128-bit decimal — port (already in sharp-runtime) |
| 157 | System.Private.CoreLib/src/System/Decimal.DecCalc.cs | `System` | | Decimal arithmetic internals — part of Decimal port |
| 158 | System.Private.CoreLib/src/System/DefaultBinder.cs | `System` | | CLR internal / not applicable in C++ |
| 159 | System.Private.CoreLib/src/System/Delegate.cs | `System` | | Delegate — CLR-specific; port event pattern only |
| 160 | System.Private.CoreLib/src/System/DivideByZeroException.cs | `System` | | DivideByZeroException — port |
| 161 | System.Private.CoreLib/src/System/DllNotFoundException.cs | `System` | | DllNotFoundException — port |
| 162 | System.Private.CoreLib/src/System/Double.cs | `System` | | double (64-bit) — port ToString/Parse/formatting |
| 163 | System.Private.CoreLib/src/System/DuplicateWaitObjectException.cs | `System` | | DuplicateWaitObjectException — stub |
| 164 | System.Private.CoreLib/src/System/Empty.cs | `System` | | CLR internal / not applicable in C++ |
| 165 | System.Private.CoreLib/src/System/EntryPointNotFoundException.cs | `System` | | EntryPointNotFoundException — stub |
| 166 | System.Private.CoreLib/src/System/Enum.cs | `System` | | Enum base class — port (ToString, Parse, GetValues) |
| 167 | System.Private.CoreLib/src/System/Enum.EnumInfo.cs | `System` | | Enum metadata cache — CLR internal; not needed |
| 168 | System.Private.CoreLib/src/System/Environment.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| 169 | System.Private.CoreLib/src/System/Environment.Browser.cs | `System` | | Platform-specific (Browser) — handle via #ifdef in C++ |
| 170 | System.Private.CoreLib/src/System/Environment.cs | `System` | | Environment class — partially ported |
| 171 | System.Private.CoreLib/src/System/Environment.FreeBSD.cs | `System` | | Platform-specific (FreeBSD) — handle via #ifdef in C++ |
| 172 | System.Private.CoreLib/src/System/Environment.GetFolderPathCore.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 173 | System.Private.CoreLib/src/System/Environment.Haiku.cs | `System` | | Platform-specific (Haiku) — handle via #ifdef in C++ |
| 174 | System.Private.CoreLib/src/System/Environment.iOS.cs | `System` | | Platform-specific (iOS) — handle via #ifdef in C++ |
| 175 | System.Private.CoreLib/src/System/Environment.Linux.cs | `System` | | Linux-specific environment impl — reference for C++ Linux build |
| 176 | System.Private.CoreLib/src/System/Environment.NoRegistry.cs | `System` | | Non-Windows registry stub — not needed |
| 177 | System.Private.CoreLib/src/System/Environment.OSVersion.MacCatalyst.cs | `System` | | Platform-specific (MacCatalyst) — handle via #ifdef in C++ |
| 178 | System.Private.CoreLib/src/System/Environment.OSVersion.OSX.cs | `System` | | macOS OSVersion impl — platform-specific; handle via #ifdef |
| 179 | System.Private.CoreLib/src/System/Environment.OSVersion.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 180 | System.Private.CoreLib/src/System/Environment.OSX.cs | `System` | | macOS environment impl — platform-specific; handle via #ifdef |
| 181 | System.Private.CoreLib/src/System/Environment.SpecialFolder.cs | `System` | | SpecialFolder enum — port |
| 182 | System.Private.CoreLib/src/System/Environment.SpecialFolderOption.cs | `System` | | SpecialFolderOption enum — port |
| 183 | System.Private.CoreLib/src/System/Environment.SunOS.cs | `System` | | Platform-specific (SunOS) — handle via #ifdef in C++ |
| 184 | System.Private.CoreLib/src/System/Environment.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 185 | System.Private.CoreLib/src/System/Environment.UnixOrBrowser.cs | `System` | | POSIX/Wasm environment impl — reference for C++ POSIX build |
| 186 | System.Private.CoreLib/src/System/Environment.Variables.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 187 | System.Private.CoreLib/src/System/Environment.Variables.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 188 | System.Private.CoreLib/src/System/EnvironmentVariableTarget.cs | `System` | | EnvironmentVariableTarget enum — port |
| 189 | System.Private.CoreLib/src/System/Environment.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 190 | System.Private.CoreLib/src/System/EventArgs.cs | `System` | | EventArgs base class — port |
| 191 | System.Private.CoreLib/src/System/EventHandler.cs | `System` | | EventHandler delegate — port (maps to std::function) |
| 192 | System.Private.CoreLib/src/System/Exception.cs | `System` | | Base Exception class — ported in sharp-runtime |
| 193 | System.Private.CoreLib/src/System/ExecutionEngineException.cs | `System` | | CLR internal exception — stub |
| 194 | System.Private.CoreLib/src/System/FieldAccessException.cs | `System` | | Reflection exception — stub |
| 195 | System.Private.CoreLib/src/System/FlagsAttribute.cs | `System` | | FlagsAttribute — port (useful for enum flags) |
| 196 | System.Private.CoreLib/src/System/FormatException.cs | `System` | | FormatException — ported |
| 197 | System.Private.CoreLib/src/System/FormattableString.cs | `System` | | FormattableString (interpolated strings) — low priority |
| 198 | System.Private.CoreLib/src/System/Function.cs | `System` | | Func<> delegates — port (maps to std::function<R(...)>) |
| 199 | System.Private.CoreLib/src/System/GC.cs | `System` | | GC class — no-op stubs in C++ (no GC needed) |
| 200 | System.Private.CoreLib/src/System/GCMemoryInfo.cs | `System` | | GC memory info — stub |
| 201 | System.Private.CoreLib/src/System/Gen2GcCallback.cs | `System` | | CLR internal / not applicable in C++ |
| 202 | System.Private.CoreLib/src/System/Globalization/DateTimeFormat.cs | `System` | | DateTime formatting internals — port with Globalization |
| 203 | System.Private.CoreLib/src/System/Globalization/DateTimeParse.cs | `System` | | DateTime parsing internals — port with Globalization |
| 204 | System.Private.CoreLib/src/System/Guid.cs | `System` | | Guid struct — port |
| 205 | System.Private.CoreLib/src/System/Guid.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 206 | System.Private.CoreLib/src/System/Guid.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 207 | System.Private.CoreLib/src/System/Half.cs | `System` | | Half-precision float — port using __fp16 or std::float16_t |
| 208 | System.Private.CoreLib/src/System/HashCode.cs | `System` | | HashCode combining — port |
| 209 | System.Private.CoreLib/src/System/IAsyncDisposable.cs | `System` | | IAsyncDisposable — port (async cleanup) |
| 210 | System.Private.CoreLib/src/System/IAsyncResult.cs | `System` | | IAsyncResult — port (legacy async pattern) |
| 211 | System.Private.CoreLib/src/System/ICloneable.cs | `System` | | ICloneable — port |
| 212 | System.Private.CoreLib/src/System/IComparable.cs | `System` | | IComparable — port (maps to operator<=>) |
| 213 | System.Private.CoreLib/src/System/IConvertible.cs | `System` | | IConvertible — port |
| 214 | System.Private.CoreLib/src/System/ICustomFormatter.cs | `System` | | ICustomFormatter — port |
| 215 | System.Private.CoreLib/src/System/IDisposable.cs | `System` | | IDisposable — port as destructor pattern |
| 216 | System.Private.CoreLib/src/System/IEquatable.cs | `System` | | IEquatable<T> — port (maps to operator==) |
| 217 | System.Private.CoreLib/src/System/IFormatProvider.cs | `System` | | IFormatProvider — port |
| 218 | System.Private.CoreLib/src/System/IFormattable.cs | `System` | | IFormattable — port |
| 219 | System.Private.CoreLib/src/System/Index.cs | `System` | | System.Index (^operator) — port |
| 220 | System.Private.CoreLib/src/System/IndexOutOfRangeException.cs | `System` | | IndexOutOfRangeException — ported |
| 221 | System.Private.CoreLib/src/System/InsufficientExecutionStackException.cs | `System` | | CLR stack exception — stub |
| 222 | System.Private.CoreLib/src/System/InsufficientMemoryException.cs | `System` | | InsufficientMemoryException — stub |
| 223 | System.Private.CoreLib/src/System/Int128.cs | `System` | | 128-bit integer — port using __int128 or custom impl |
| 224 | System.Private.CoreLib/src/System/Int16.cs | `System` | | Primitive short (int16_t) — port ToString/Parse |
| 225 | System.Private.CoreLib/src/System/Int32.cs | `System` | | Primitive int (int32_t) — port ToString/Parse/formatting |
| 226 | System.Private.CoreLib/src/System/Int64.cs | `System` | | Primitive long (int64_t) — port ToString/Parse |
| 227 | System.Private.CoreLib/src/System/IntPtr.cs | `System` | | Pointer-sized integer — maps to intptr_t |
| 228 | System.Private.CoreLib/src/System/InvalidCastException.cs | `System` | | InvalidCastException — ported |
| 229 | System.Private.CoreLib/src/System/InvalidOperationException.cs | `System` | | InvalidOperationException — ported |
| 230 | System.Private.CoreLib/src/System/InvalidProgramException.cs | `System` | | CLR IL exception — stub |
| 231 | System.Private.CoreLib/src/System/InvalidTimeZoneException.cs | `System` | | InvalidTimeZoneException — port |
| 232 | System.Private.CoreLib/src/System/IObservable.cs | `System` | | IObservable<T> — port (reactive pattern) |
| 233 | System.Private.CoreLib/src/System/IObserver.cs | `System` | | IObserver<T> — port (reactive pattern) |
| 234 | System.Private.CoreLib/src/System/IParsable.cs | `System` | | IParsable<T> — port (static Parse interface) |
| 235 | System.Private.CoreLib/src/System/IProgress.cs | `System` | | IProgress<T> interface — port |
| 236 | System.Private.CoreLib/src/System/ISpanFormattable.cs | `System` | | ISpanFormattable — port |
| 237 | System.Private.CoreLib/src/System/ISpanParsable.cs | `System` | | ISpanParsable<T> — port |
| 238 | System.Private.CoreLib/src/System/IUtf8SpanFormattable.cs | `System` | | IUtf8SpanFormattable — port |
| 239 | System.Private.CoreLib/src/System/IUtf8SpanParsable.cs | `System` | | IUtf8SpanParsable<T> — port |
| 240 | System.Private.CoreLib/src/System/IUtfChar.cs | `System` | | Internal UTF char interface — not needed |
| 241 | System.Private.CoreLib/src/System/Lazy.cs | `System` | | Lazy<T> — port (thread-safe lazy init) |
| 242 | System.Private.CoreLib/src/System/LazyOfTTMetadata.cs | `System` | | Lazy<T,TMetadata> impl — port if needed |
| 243 | System.Private.CoreLib/src/System/LoaderOptimizationAttribute.cs | `System` | | CLR internal / not applicable in C++ |
| 244 | System.Private.CoreLib/src/System/LoaderOptimization.cs | `System` | | CLR internal / not applicable in C++ |
| 245 | System.Private.CoreLib/src/System/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 246 | System.Private.CoreLib/src/System/LocalDataStoreSlot.cs | `System` | | CLR internal / not applicable in C++ |
| 247 | System.Private.CoreLib/src/System/MarshalByRefObject.cs | `System` | | CLR internal / not applicable in C++ |
| 248 | System.Private.CoreLib/src/System/Marvin.cs | `System` | | Marvin32 string hash (internal) — not needed |
| 249 | System.Private.CoreLib/src/System/Marvin.OrdinalIgnoreCase.cs | `System` | | Marvin32 string hash (internal) — not needed |
| 250 | System.Private.CoreLib/src/System/Math.cs | `System` | | Math functions — ported in sharp-runtime via cmath |
| 251 | System.Private.CoreLib/src/System/Math.DivModInt.cs | `System` | | Integer DivMod helpers — port |
| 252 | System.Private.CoreLib/src/System/MathF.cs | `System` | | Single-precision math — port via cmath |
| 253 | System.Private.CoreLib/src/System/MemberAccessException.cs | `System` | | Reflection exception — stub |
| 254 | System.Private.CoreLib/src/System/Memory.cs | `System` | | Memory<T> — port or map to std::vector slice |
| 255 | System.Private.CoreLib/src/System/MemoryDebugView.cs | `System` | | Debugger view helper — not needed |
| 256 | System.Private.CoreLib/src/System/MemoryExtensions.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| 257 | System.Private.CoreLib/src/System/MemoryExtensions.Globalization.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| 258 | System.Private.CoreLib/src/System/MemoryExtensions.Globalization.Utf8.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| 259 | System.Private.CoreLib/src/System/MemoryExtensions.Trim.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| 260 | System.Private.CoreLib/src/System/MemoryExtensions.Trim.Utf8.cs | `System` | | Memory/Span extension methods — use std::span in C++ |
| 261 | System.Private.CoreLib/src/System/MethodAccessException.cs | `System` | | Reflection exception — stub |
| 262 | System.Private.CoreLib/src/System/MidpointRounding.cs | `System` | | MidpointRounding enum — port |
| 263 | System.Private.CoreLib/src/System/MissingFieldException.cs | `System` | | Reflection exception — stub |
| 264 | System.Private.CoreLib/src/System/MissingMemberException.cs | `System` | | Reflection exception — stub |
| 265 | System.Private.CoreLib/src/System/MissingMethodException.cs | `System` | | Reflection exception — stub |
| 266 | System.Private.CoreLib/src/System/MulticastDelegate.cs | `System` | | MulticastDelegate — port (event multicasting) |
| 267 | System.Private.CoreLib/src/System/MulticastNotSupportedException.cs | `System` | | Delegate exception — stub |
| 268 | System.Private.CoreLib/src/System/NonSerializedAttribute.cs | `System` | | NonSerialized attribute — stub |
| 269 | System.Private.CoreLib/src/System/NotFiniteNumberException.cs | `System` | | NotFiniteNumberException — port |
| 270 | System.Private.CoreLib/src/System/NotImplementedException.cs | `System` | | NotImplementedException — ported |
| 271 | System.Private.CoreLib/src/System/NotSupportedException.cs | `System` | | NotSupportedException — ported |
| 272 | System.Private.CoreLib/src/System/Nullable.cs | `System` | | Nullable<T> — map to std::optional<T> in C++ |
| 273 | System.Private.CoreLib/src/System/NullReferenceException.cs | `System` | | NullReferenceException — ported |
| 274 | System.Private.CoreLib/src/System/Number.BigInteger.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 275 | System.Private.CoreLib/src/System/Number.DiyFp.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 276 | System.Private.CoreLib/src/System/Number.Dragon4.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 277 | System.Private.CoreLib/src/System/Number.Formatting.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 278 | System.Private.CoreLib/src/System/Number.Grisu3.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 279 | System.Private.CoreLib/src/System/Number.NumberToFloatingPointBits.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 280 | System.Private.CoreLib/src/System/Number.Parsing.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 281 | System.Private.CoreLib/src/System/Object.cs | `System` | | System.Object base — stub (C++ uses inheritance differently) |
| 282 | System.Private.CoreLib/src/System/ObjectDisposedException.cs | `System` | | ObjectDisposedException — ported |
| 283 | System.Private.CoreLib/src/System/ObsoleteAttribute.cs | `System` | | ObsoleteAttribute — stub (use [[deprecated]] in C++) |
| 284 | System.Private.CoreLib/src/System/OperatingSystem.cs | `System` | | OperatingSystem info — port |
| 285 | System.Private.CoreLib/src/System/OperationCanceledException.cs | `System` | | OperationCanceledException — port (for Task/CancellationToken) |
| 286 | System.Private.CoreLib/src/System/OutOfMemoryException.cs | `System` | | OutOfMemoryException — stub (C++ throws std::bad_alloc) |
| 287 | System.Private.CoreLib/src/System/OverflowException.cs | `System` | | OverflowException — port |
| 288 | System.Private.CoreLib/src/System/ParamArrayAttribute.cs | `System` | | params keyword attribute — stub |
| 289 | System.Private.CoreLib/src/System/ParseNumbers.cs | `System` | | CLR internal / not applicable in C++ |
| 290 | System.Private.CoreLib/src/System/PasteArguments.cs | `System` | | CLR internal / not applicable in C++ |
| 291 | System.Private.CoreLib/src/System/PasteArguments.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 292 | System.Private.CoreLib/src/System/PasteArguments.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 293 | System.Private.CoreLib/src/System/PlatformID.cs | `System` | | PlatformID enum — port |
| 294 | System.Private.CoreLib/src/System/PlatformNotSupportedException.cs | `System` | | PlatformNotSupportedException — ported |
| 295 | System.Private.CoreLib/src/System/Progress.cs | `System` | | IProgress<T>/Progress<T> — port |
| 296 | System.Private.CoreLib/src/System/Random.CompatImpl.cs | `System` | | Random compatibility impl — internal |
| 297 | System.Private.CoreLib/src/System/Random.cs | `System` | | Random class — ported in sharp-runtime |
| 298 | System.Private.CoreLib/src/System/Random.ImplBase.cs | `System` | | Random base impl — internal |
| 299 | System.Private.CoreLib/src/System/Random.Xoshiro128StarStarImpl.cs | `System` | | Xoshiro128** RNG — internal impl |
| 300 | System.Private.CoreLib/src/System/Random.Xoshiro256StarStarImpl.cs | `System` | | Xoshiro256** RNG — internal impl |
| 301 | System.Private.CoreLib/src/System/Range.cs | `System` | | System.Range — port |
| 302 | System.Private.CoreLib/src/System/RankException.cs | `System` | | RankException — stub |
| 303 | System.Private.CoreLib/src/System/ReadOnlyMemory.cs | `System` | | ReadOnlyMemory<T> — port or map to const slice |
| 304 | System.Private.CoreLib/src/System/ReadOnlySpan.cs | `System` | | ReadOnlySpan<T> — map to const std::span in C++ |
| 305 | System.Private.CoreLib/src/System/ResolveEventArgs.cs | `System` | | Assembly resolve event — stub |
| 306 | System.Private.CoreLib/src/System/ResolveEventHandler.cs | `System` | | Assembly resolve handler — stub |
| 307 | System.Private.CoreLib/src/System/Runtime/InteropServices/ComAwareWeakReference.ComWrappers.cs | `System` | | COM interop — not needed |
| 308 | System.Private.CoreLib/src/System/RuntimeType.cs | `System` | | CLR internal / not applicable in C++ |
| 309 | System.Private.CoreLib/src/System/SByte.cs | `System` | | Primitive sbyte (int8_t) — port ToString/Parse/formatting |
| 310 | System.Private.CoreLib/src/System/SerializableAttribute.cs | `System` | | Serializable attribute — stub |
| 311 | System.Private.CoreLib/src/System/Single.cs | `System` | | float (32-bit) — port ToString/Parse/formatting |
| 312 | System.Private.CoreLib/src/System/Span.cs | `System` | | Span<T> — map to std::span in C++ |
| 313 | System.Private.CoreLib/src/System/SpanDebugView.cs | `System` | | Debugger view helper — not needed |
| 314 | System.Private.CoreLib/src/System/SpanHelpers.BinarySearch.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 315 | System.Private.CoreLib/src/System/SpanHelpers.Byte.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 316 | System.Private.CoreLib/src/System/SpanHelpers.ByteMemOps.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 317 | System.Private.CoreLib/src/System/SpanHelpers.Char.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 318 | System.Private.CoreLib/src/System/SpanHelpers.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 319 | System.Private.CoreLib/src/System/SpanHelpers.Packed.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 320 | System.Private.CoreLib/src/System/SpanHelpers.T.cs | `System` | | Span internal algorithms — use std::* equivalents in C++ |
| 321 | System.Private.CoreLib/src/System/SR.cs | `System` | | String resources / assembly info — not needed |
| 322 | System.Private.CoreLib/src/System/StackOverflowException.cs | `System` | | CLR stack overflow — stub only |
| 323 | System.Private.CoreLib/src/System/StartupHookProvider.cs | `System` | | CLR internal / not applicable in C++ |
| 324 | System.Private.CoreLib/src/System/StringComparer.cs | `System` | | String comparison strategies — port |
| 325 | System.Private.CoreLib/src/System/String.Comparison.cs | `System` | | String comparison methods — port |
| 326 | System.Private.CoreLib/src/System/StringComparison.cs | `System` | | StringComparison enum — port |
| 327 | System.Private.CoreLib/src/System/String.cs | `System` | | Core string type — partially ported in sharp-runtime |
| 328 | System.Private.CoreLib/src/System/String.Manipulation.cs | `System` | | String manipulation (Replace, Split, Trim etc.) — port |
| 329 | System.Private.CoreLib/src/System/StringNormalizationExtensions.cs | `System` | | Unicode normalization — low priority |
| 330 | System.Private.CoreLib/src/System/String.Searching.cs | `System` | | String search (IndexOf, Contains etc.) — port |
| 331 | System.Private.CoreLib/src/System/StringSplitOptions.cs | `System` | | StringSplitOptions enum — port |
| 332 | System.Private.CoreLib/src/System/SystemException.cs | `System` | | SystemException — ported |
| 333 | System.Private.CoreLib/src/System/ThreadAttributes.cs | `System` | | Thread attributes — stub |
| 334 | System.Private.CoreLib/src/System/ThreadStaticAttribute.cs | `System` | | ThreadStatic attribute — stub |
| 335 | System.Private.CoreLib/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 336 | System.Private.CoreLib/src/System/TimeOnly.cs | `System` | | Time-only struct — port |
| 337 | System.Private.CoreLib/src/System/TimeoutException.cs | `System` | | TimeoutException — port |
| 338 | System.Private.CoreLib/src/System/TimeSpan.cs | `System` | | TimeSpan struct — ported in sharp-runtime |
| 339 | System.Private.CoreLib/src/System/TimeZone.cs | `System` | | Legacy TimeZone class — stub |
| 340 | System.Private.CoreLib/src/System/TimeZoneInfo.AdjustmentRule.cs | `System` | | TimeZoneInfo.AdjustmentRule — part of TimeZoneInfo port |
| 341 | System.Private.CoreLib/src/System/TimeZoneInfo.Cache.cs | `System` | | TimeZoneInfo cache — internal implementation |
| 342 | System.Private.CoreLib/src/System/TimeZoneInfo.cs | `System` | | TimeZoneInfo — partially ported (POSIX-only) |
| 343 | System.Private.CoreLib/src/System/TimeZoneInfo.FullGlobalizationData.cs | `System` | | TimeZoneInfo globalization — reference |
| 344 | System.Private.CoreLib/src/System/TimeZoneInfo.FullGlobalizationData.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 345 | System.Private.CoreLib/src/System/TimeZoneInfo.MinimalGlobalizationData.cs | `System` | | TimeZoneInfo minimal data — reference |
| 346 | System.Private.CoreLib/src/System/TimeZoneInfo.StringSerializer.cs | `System` | | TimeZoneInfo serialization — port |
| 347 | System.Private.CoreLib/src/System/TimeZoneInfo.TransitionTime.cs | `System` | | TimeZoneInfo.TransitionTime — port |
| 348 | System.Private.CoreLib/src/System/TimeZoneInfo.Unix.Android.cs | `System` | | Platform-specific (Android) — handle via #ifdef in C++ |
| 349 | System.Private.CoreLib/src/System/TimeZoneInfo.Unix.cs | `System` | | POSIX implementation — reference for Linux/macOS build |
| 350 | System.Private.CoreLib/src/System/TimeZoneInfo.Unix.NonAndroid.cs | `System` | | Platform-specific (NonAndroid) — handle via #ifdef in C++ |
| 351 | System.Private.CoreLib/src/System/TimeZoneInfo.Windows.cs | `System` | | Windows-specific implementation — not needed |
| 352 | System.Private.CoreLib/src/System/TimeZoneNotFoundException.cs | `System` | | TimeZoneNotFoundException — port |
| 353 | System.Private.CoreLib/src/System/Tuple.cs | `System` | | Tuple<T1..T8> — port (maps to std::tuple) |
| 354 | System.Private.CoreLib/src/System/TupleExtensions.cs | `System` | | Tuple deconstruct extensions — port |
| 355 | System.Private.CoreLib/src/System/TupleSlim.cs | `System` | | CLR internal / not applicable in C++ |
| 356 | System.Private.CoreLib/src/System/TypeAccessException.cs | `System` | | Reflection exception — stub |
| 357 | System.Private.CoreLib/src/System/TypeCode.cs | `System` | | TypeCode enum — port |
| 358 | System.Private.CoreLib/src/System/Type.cs | `System` | | System.Type — CLR reflection; stub |
| 359 | System.Private.CoreLib/src/System/TypedReference.cs | `System` | | TypedReference — CLR internal; stub |
| 360 | System.Private.CoreLib/src/System/Type.Enum.cs | `System` | | Type.GetEnumNames etc. — CLR reflection; stub |
| 361 | System.Private.CoreLib/src/System/Type.Helpers.cs | `System` | | Type helper methods — CLR reflection; stub |
| 362 | System.Private.CoreLib/src/System/TypeInitializationException.cs | `System` | | TypeInitializationException — stub |
| 363 | System.Private.CoreLib/src/System/TypeLoadException.cs | `System` | | CLR exception — stub |
| 364 | System.Private.CoreLib/src/System/TypeUnloadedException.cs | `System` | | CLR exception — stub |
| 365 | System.Private.CoreLib/src/System/UInt128.cs | `System` | | Unsigned 128-bit integer — port using unsigned __int128 |
| 366 | System.Private.CoreLib/src/System/UInt16.cs | `System` | | Primitive ushort (uint16_t) — port ToString/Parse |
| 367 | System.Private.CoreLib/src/System/UInt32.cs | `System` | | Primitive uint (uint32_t) — port ToString/Parse |
| 368 | System.Private.CoreLib/src/System/UInt64.cs | `System` | | Primitive ulong (uint64_t) — port ToString/Parse |
| 369 | System.Private.CoreLib/src/System/UIntPtr.cs | `System` | | Unsigned pointer-sized integer — maps to uintptr_t |
| 370 | System.Private.CoreLib/src/System/UnauthorizedAccessException.cs | `System` | | UnauthorizedAccessException — port (for file I/O) |
| 371 | System.Private.CoreLib/src/System/UnhandledExceptionEventArgs.cs | `System` | | UnhandledException event args — port |
| 372 | System.Private.CoreLib/src/System/UnhandledExceptionEventHandler.cs | `System` | | UnhandledException handler — port |
| 373 | System.Private.CoreLib/src/System/UnitySerializationHolder.cs | `System` | | CLR internal / not applicable in C++ |
| 374 | System.Private.CoreLib/src/System/ValueTuple.cs | `System` | | ValueTuple — port (maps to std::tuple) |
| 375 | System.Private.CoreLib/src/System/Version.cs | `System` | | Version (major.minor.build.revision) — port |
| 376 | System.Private.CoreLib/src/System/Void.cs | `System` | | System.Void — not needed |
| 377 | System.Private.CoreLib/src/System/WeakReference.cs | `System` | | WeakReference — port using std::weak_ptr |
| 378 | System.Private.CoreLib/src/System/WeakReference.T.cs | `System` | | WeakReference<T> — port using std::weak_ptr |
| 379 | System.Private.Uri/src/System/DomainNameHelper.cs | `System` | | DNS domain name helpers — port with Net |
| 380 | System.Private.Uri/src/System/GenericUriParser.cs | `System` | | Generic URI parser — port |
| 381 | System.Private.Uri/src/System/IriHelper.cs | `System` | | IRI helper — port with Uri |
| 382 | System.Private.Uri/src/System/PercentEncodingHelper.cs | `System` | | URL percent-encoding — port |
| 383 | System.Private.Uri/src/System/UncNameHelper.cs | `System` | | UNC path helper — Windows-specific; stub |
| 384 | System.Private.Uri/src/System/UriBuilder.cs | `System` | | UriBuilder — port |
| 385 | System.Private.Uri/src/System/UriCreationOptions.cs | `System` | | UriCreationOptions — port |
| 386 | System.Private.Uri/src/System/Uri.cs | `System` | | Uri class — port |
| 387 | System.Private.Uri/src/System/UriEnumTypes.cs | `System` | | Uri enum types (UriKind etc.) — port |
| 388 | System.Private.Uri/src/System/UriExt.cs | `System` | | Uri extension methods — port |
| 389 | System.Private.Uri/src/System/UriFormatException.cs | `System` | | UriFormatException — port |
| 390 | System.Private.Uri/src/System/UriHelper.cs | `System` | | Uri internal helpers — port with Uri |
| 391 | System.Private.Uri/src/System/UriHostNameType.cs | `System` | | UriHostNameType enum — port |
| 392 | System.Private.Uri/src/System/UriParserTemplates.cs | `System` | | URI parser templates — port |
| 393 | System.Private.Uri/src/System/UriPartial.cs | `System` | | UriPartial enum — port |
| 394 | System.Private.Uri/src/System/UriScheme.cs | `System` | | URI scheme constants — port |
| 395 | System.Private.Uri/src/System/UriSyntax.cs | `System` | | URI syntax — port |
| 396 | System.Private.Uri/tests/UnitTests/Fakes/FakeUri.cs | `System` | | Test infrastructure — skip |
| 397 | System.Private.Xml/src/Misc/HResults.cs | `System` | | Windows HRESULT error codes — not needed |
| 398 | System.Reflection.MetadataLoadContext/src/System/Reflection/DefaultBinder.CanConvert.cs | `System` | | CLR reflection binder — not needed |
| 399 | System.Reflection.MetadataLoadContext/src/System/Reflection/DefaultBinder.cs | `System` | | CLR internal / not applicable in C++ |
| 400 | System.Reflection.MetadataLoadContext/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 401 | System.Runtime.InteropServices/ref/System.Runtime.InteropServices.cs | `System` | | Public API surface definition — use as porting reference |
| 402 | System.Runtime.Numerics/src/System/Number.BigInteger.cs | `System` | | Number formatting/parsing internals — use std::format / printf in C++ |
| 403 | System.Runtime.Numerics/src/System/Number.Polyfill.cs | `System` | | Polyfill for older TFMs — not needed |
| 404 | System.Runtime.Numerics/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 405 | System.Runtime/ref/System.Runtime.cs | `System` | | Public API surface definition — use as porting reference |
| 406 | System.Runtime.Serialization.Formatters/src/System/Runtime/Serialization/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 407 | System.Runtime/tests/System.Runtime.Extensions.Tests/System/ApplicationIdTests.cs | `System` | | Test infrastructure — skip |
| 408 | System.Runtime/tests/System.Runtime.Extensions.Tests/TestHelpers.cs | `System` | | Test infrastructure — skip |
| 409 | System.Security.Cryptography/src/System/Security/Cryptography/X509Certificates/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 410 | System.Security.Cryptography.Xml/src/System/Security/Cryptography/Xml/LocalAppContextSwitches.cs | `System` | | Runtime feature switches — not needed in C++ |
| 411 | System.Security.Permissions/ref/System.Security.Permissions.cs | `System` | | Public API surface definition — use as porting reference |
| 412 | System.Security.Permissions/src/System/ApplicationIdentity.cs | `System` | | ClickOnce ApplicationIdentity — not needed |
| 413 | System.Text.Encodings.Web/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 414 | System.Text.Encodings.Web/tests/SR.cs | `System` | | Test infrastructure — skip |
| 415 | System.Text.Json/src/System/ThrowHelper.cs | `System` | | Internal throw helper — not needed in C++ |
| 416 | System.Text.RegularExpressions/gen/Stubs.cs | `System` | | Source generator stubs — not needed |
| 417 | System.Threading.Channels/src/System/VoidResult.cs | `System` | | Internal void result — not needed |
| 418 | System.Threading.Thread/ref/System.Threading.Thread.cs | `System` | | Public API surface definition — use as porting reference |
