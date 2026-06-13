# plan.md — sharp-runtime .NET Namespace Plan
*Last updated: 2026-06-13 (session 69) — 3939 tests passing*

sharp-runtime is a C++23 static library reimplementing a practical subset of .NET `System.*` for **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game).

Reference source: `/rv/tmp/runtime/src/libraries/` (dotnet/runtime, MIT License)

---

## Legend

| Status | Meaning |
|--------|---------|
| `ported` | Implemented in sharp-runtime — good coverage |
| `in_progress` | Partially implemented, work ongoing |
| `todo` | Needs to be ported/implemented |
| `ignore` | Out of scope for sharp-runtime |

Status column is filled interactively: for each empty row, Claude describes what the namespace contains, then asks: **todo / ignore / ported / in_progress**.

---

## .NET Namespace Table (311 namespaces)

| # | Namespace | Status | Notes |
|---|-----------|--------|-------|
| 1 | `Microsoft.CSharp` | `ignore` | DLR / dynamic keyword runtime — C++ has no dynamic keyword |
| 2 | `Microsoft.CSharp.RuntimeBinder` | `ignore` | DLR runtime binder |
| 3 | `Microsoft.CSharp.RuntimeBinder.ComInterop` | `ignore` | DLR COM interop |
| 4 | `Microsoft.CSharp.RuntimeBinder.Errors` | `ignore` | DLR error handling |
| 5 | `Microsoft.CSharp.RuntimeBinder.Semantics` | `ignore` | DLR semantics |
| 6 | `Microsoft.CSharp.RuntimeBinder.Syntax` | `ignore` | DLR syntax tree |
| 7 | `Microsoft.DotNet.PlatformAbstractions` | `ignore` | Obsolete HashCodeCombiner — use System.HashCode instead |
| 8 | `Microsoft.Extensions.Caching.Distributed` | `ignore` | ASP.NET Core — IDistributedCache |
| 9 | `Microsoft.Extensions.Caching.Hybrid` | `ignore` | ASP.NET Core — hybrid cache |
| 10 | `Microsoft.Extensions.Caching.Memory` | `ignore` | ASP.NET Core — IMemoryCache |
| 11 | `Microsoft.Extensions.Configuration` | `ignore` | ASP.NET Core — IConfiguration |
| 12 | `Microsoft.Extensions.Configuration.CommandLine` | `ignore` | ASP.NET Core — CLI args configuration |
| 13 | `Microsoft.Extensions.Configuration.EnvironmentVariables` | `ignore` | ASP.NET Core — env configuration |
| 14 | `Microsoft.Extensions.Configuration.Ini` | `ignore` | ASP.NET Core — INI configuration |
| 15 | `Microsoft.Extensions.Configuration.Json` | `ignore` | ASP.NET Core — JSON configuration |
| 16 | `Microsoft.Extensions.Configuration.Memory` | `ignore` | ASP.NET Core — in-memory configuration |
| 17 | `Microsoft.Extensions.Configuration.UserSecrets` | `ignore` | ASP.NET Core — user secrets |
| 18 | `Microsoft.Extensions.Configuration.Xml` | `ignore` | ASP.NET Core — XML configuration |
| 19 | `Microsoft.Extensions.DependencyInjection` | `ignore` | ASP.NET Core — IoC container |
| 20 | `Microsoft.Extensions.DependencyInjection.Extensions` | `ignore` | ASP.NET Core — DI extensions |
| 21 | `Microsoft.Extensions.DependencyInjection.ServiceLookup` | `ignore` | ASP.NET Core — DI internal |
| 22 | `Microsoft.Extensions.DependencyInjection.Specification` | `ignore` | ASP.NET Core — DI spec tests |
| 23 | `Microsoft.Extensions.DependencyInjection.Specification.Fakes` | `ignore` | ASP.NET Core — DI test fakes |
| 24 | `Microsoft.Extensions.DependencyModel` | `ignore` | ASP.NET Core — dependency model |
| 25 | `Microsoft.Extensions.DependencyModel.Resolution` | `ignore` | ASP.NET Core — assembly resolution |
| 26 | `Microsoft.Extensions.Diagnostics.Metrics` | `ignore` | ASP.NET Core — OpenTelemetry metrics |
| 27 | `Microsoft.Extensions.Diagnostics.Metrics.Configuration` | `ignore` | ASP.NET Core — metrics configuration |
| 28 | `Microsoft.Extensions.FileProviders` | `ignore` | ASP.NET Core — IFileProvider |
| 29 | `Microsoft.Extensions.FileProviders.Composite` | `ignore` | ASP.NET Core — composite file provider |
| 30 | `Microsoft.Extensions.FileProviders.Internal` | `ignore` | ASP.NET Core — internal |
| 31 | `Microsoft.Extensions.FileProviders.Physical` | `ignore` | ASP.NET Core — physical file provider |
| 32 | `Microsoft.Extensions.FileProviders.Physical.Internal` | `ignore` | ASP.NET Core — internal |
| 33 | `Microsoft.Extensions.FileSystemGlobbing` | `ignore` | ASP.NET Core — glob pattern matching |
| 34 | `Microsoft.Extensions.FileSystemGlobbing.Abstractions` | `ignore` | ASP.NET Core — globbing abstractions |
| 35 | `Microsoft.Extensions.FileSystemGlobbing.Internal` | `ignore` | ASP.NET Core — internal |
| 36 | `Microsoft.Extensions.FileSystemGlobbing.Internal.PathSegments` | `ignore` | ASP.NET Core — internal |
| 37 | `Microsoft.Extensions.FileSystemGlobbing.Internal.PatternContexts` | `ignore` | ASP.NET Core — internal |
| 38 | `Microsoft.Extensions.FileSystemGlobbing.Internal.Patterns` | `ignore` | ASP.NET Core — internal |
| 39 | `Microsoft.Extensions.FileSystemGlobbing.Util` | `ignore` | ASP.NET Core — internal |
| 40 | `Microsoft.Extensions.Hosting` | `ignore` | ASP.NET Core — IHost / IHostedService |
| 41 | `Microsoft.Extensions.Hosting.Internal` | `ignore` | ASP.NET Core — internal |
| 42 | `Microsoft.Extensions.Hosting.Systemd` | `ignore` | ASP.NET Core — systemd integration |
| 43 | `Microsoft.Extensions.Hosting.WindowsServices` | `ignore` | ASP.NET Core — Windows Service integration |
| 44 | `Microsoft.Extensions.Hosting.WindowsServices.Internal` | `ignore` | ASP.NET Core — internal |
| 45 | `Microsoft.Extensions.Http` | `ignore` | ASP.NET Core — IHttpClientFactory |
| 46 | `Microsoft.Extensions.Http.Logging` | `ignore` | ASP.NET Core — HTTP logging |
| 47 | `Microsoft.Extensions.Internal` | `ignore` | ASP.NET Core — internal utilities |
| 48 | `Microsoft.Extensions.Logging` | `ignore` | ASP.NET Core — ILogger / ILoggerFactory |
| 49 | `Microsoft.Extensions.Logging.Abstractions` | `ignore` | ASP.NET Core — logging abstractions |
| 50 | `Microsoft.Extensions.Logging.Abstractions.Internal` | `ignore` | ASP.NET Core — internal |
| 51 | `Microsoft.Extensions.Logging.Configuration` | `ignore` | ASP.NET Core — logging configuration |
| 52 | `Microsoft.Extensions.Logging.Console` | `ignore` | ASP.NET Core — console logger |
| 53 | `Microsoft.Extensions.Logging.Debug` | `ignore` | ASP.NET Core — debug logger |
| 54 | `Microsoft.Extensions.Logging.EventLog` | `ignore` | ASP.NET Core — Windows Event Log logger |
| 55 | `Microsoft.Extensions.Logging.EventSource` | `ignore` | ASP.NET Core — ETW logger |
| 56 | `Microsoft.Extensions.Logging.Internal` | `ignore` | ASP.NET Core — internal |
| 57 | `Microsoft.Extensions.Logging.TraceSource` | `ignore` | ASP.NET Core — TraceSource logger |
| 58 | `Microsoft.Extensions.Options` | `ignore` | ASP.NET Core — IOptions<T> pattern |
| 59 | `Microsoft.Extensions.Primitives` | `ignore` | ASP.NET Core — StringValues, ChangeToken |
| 60 | `Microsoft.Internal` | `ignore` | MEF internal implementation — not public API |
| 61 | `Microsoft.Internal.Collections` | `ignore` | MEF internal collections — not public API |
| 62 | `Microsoft.Interop` | `ignore` | Roslyn source generator for JS interop — compile-time only, Blazor/Wasm specific |
| 63 | `Microsoft.NETCore.Platforms` | `ignore` | MSBuild task for RID graph management — build infrastructure only |
| 64 | `Microsoft.Quic` | `ignore` | MsQuic P/Invoke interop — low-level QUIC native bindings, not needed |
| 65 | `Microsoft.VisualBasic` | `ignore` | VB.NET runtime support — not applicable to C++ port |
| 66 | `Microsoft.VisualBasic.CompilerServices` | `ignore` | VB.NET compiler services — not applicable |
| 67 | `Microsoft.VisualBasic.FileIO` | `ignore` | VB.NET FileIO helpers — not applicable |
| 68 | `Microsoft.Win32` | `ignore` | Windows Registry and SystemEvents — Windows-only, irrelevant for game dev |
| 69 | `Microsoft.Win32.SafeHandles` | `ignore` | Windows Safe Handle wrappers (SafeFileHandle etc.) — Windows-only |
| 70 | `Microsoft.XmlSerializer.Generator` | `ignore` | MSBuild/CLI tool for pre-generating XmlSerializer code — build tooling only |
| 71 | `System` | `todo` | Object, String, Int32, DateTime, Math, Console, Exception, Array, Enum, Convert, Random, GC, Environment |
| 72 | `System.Buffers` | `todo` | ArrayPool<T>, MemoryPool<T>, ReadOnlySequence<T>, SearchValues<T> |
| 73 | `System.Buffers.Binary` | `todo` | BinaryPrimitives — endian-aware reading/writing of primitives |
| 74 | `System.Buffers.Text` | `todo` | Utf8Formatter, Utf8Parser — text formatting/parsing on byte spans |
| 75 | `System.CodeDom` | `ignore` | Code DOM — programmatic code generation AST, superseded by Roslyn |
| 76 | `System.CodeDom.Compiler` | `ignore` | Code DOM compiler/provider — superseded by Roslyn |
| 77 | `System.Collections` | `todo` | IList, ICollection, ArrayList, Hashtable, Stack, Queue |
| 78 | `System.Collections.Concurrent` | `todo` | ConcurrentDictionary, ConcurrentQueue, ConcurrentBag, BlockingCollection |
| 79 | `System.Collections.Frozen` | `todo` | FrozenDictionary, FrozenSet — immutable read-optimized collections |
| 80 | `System.Collections.Generic` | `todo` | List<T>, Dictionary<K,V>, HashSet<T>, Queue<T>, Stack<T>, LinkedList<T> |
| 81 | `System.Collections.Immutable` | `todo` | ImmutableList, ImmutableDictionary, ImmutableArray etc. |
| 82 | `System.Collections.ObjectModel` | `todo` | Collection<T>, ReadOnlyCollection<T>, ObservableCollection<T> |
| 83 | `System.Collections.Specialized` | `todo` | StringCollection, NameValueCollection, BitVector32, OrderedDictionary |
| 84 | `System.ComponentModel` | `todo` | INotifyPropertyChanged, TypeConverter, IComponent, CancelEventArgs, BackgroundWorker |
| 85 | `System.ComponentModel.Composition` | `todo` | MEF — CompositionContainer, ImportAttribute, ExportAttribute |
| 86 | `System.ComponentModel.Composition.AttributedModel` | `todo` | MEF attributed programming model |
| 87 | `System.ComponentModel.Composition.Hosting` | `todo` | MEF hosting — AssemblyCatalog, DirectoryCatalog |
| 88 | `System.ComponentModel.Composition.Primitives` | `todo` | MEF primitives — ComposablePart, ExportDefinition |
| 89 | `System.ComponentModel.Composition.ReflectionModel` | `todo` | MEF reflection model |
| 90 | `System.ComponentModel.Composition.Registration` | `todo` | MEF registration builder API |
| 91 | `System.ComponentModel.DataAnnotations` | `todo` | Validation attributes — Required, Range, StringLength, ValidationResult |
| 92 | `System.ComponentModel.DataAnnotations.Schema` | `todo` | Data schema attributes — Table, Column, ForeignKey |
| 93 | `System.ComponentModel.Design` | `todo` | Designer infrastructure — IDesigner, IDesignerHost |
| 94 | `System.ComponentModel.Design.Serialization` | `todo` | Designer serialization — CodeDomSerializer |
| 95 | `System.Composition` | `ignore` | MEF 2 (Lightweight MEF) — IoC/plugin framework, irrelevant for game dev |
| 96 | `System.Composition.Convention` | `ignore` | MEF 2 — convention-based registration |
| 97 | `System.Composition.Debugging` | `ignore` | MEF 2 — debug utilities |
| 98 | `System.Composition.Diagnostics` | `ignore` | MEF 2 — diagnostics |
| 99 | `System.Composition.Hosting` | `ignore` | MEF 2 — CompositionHost container |
| 100 | `System.Composition.Hosting.Core` | `ignore` | MEF 2 — hosting core |
| 101 | `System.Composition.Hosting.Providers` | `ignore` | MEF 2 — internal providers |
| 102 | `System.Composition.Hosting.Providers.CurrentScope` | `ignore` | MEF 2 — internal |
| 103 | `System.Composition.Hosting.Providers.ExportFactory` | `ignore` | MEF 2 — internal |
| 104 | `System.Composition.Hosting.Providers.ImportMany` | `ignore` | MEF 2 — internal |
| 105 | `System.Composition.Hosting.Providers.Lazy` | `ignore` | MEF 2 — internal |
| 106 | `System.Composition.Hosting.Providers.Metadata` | `ignore` | MEF 2 — internal |
| 107 | `System.Composition.Hosting.Util` | `ignore` | MEF 2 — internal |
| 108 | `System.Composition.Runtime.Util` | `ignore` | MEF 2 — internal |
| 109 | `System.Composition.TypedParts` | `ignore` | MEF 2 — typed parts discovery |
| 110 | `System.Composition.TypedParts.ActivationFeatures` | `ignore` | MEF 2 — internal |
| 111 | `System.Composition.TypedParts.Discovery` | `ignore` | MEF 2 — internal |
| 112 | `System.Composition.TypedParts.Util` | `ignore` | MEF 2 — internal |
| 113 | `System.Configuration` | `ignore` | Legacy XML app.config system — replaced by Microsoft.Extensions.Configuration |
| 114 | `System.Configuration.Assemblies` | `ignore` | Assembly hash/algorithm metadata attributes |
| 115 | `System.Configuration.Internal` | `ignore` | Internal configuration interfaces |
| 116 | `System.Configuration.Provider` | `ignore` | ProviderBase/ProviderCollection — legacy provider pattern |
| 117 | `System.Data` | `ignore` | ADO.NET — DataSet, DataTable, DataRow — irrelevant for game dev |
| 118 | `System.Data.Common` | `ignore` | ADO.NET — DbConnection, DbCommand, DbDataReader abstractions |
| 119 | `System.Data.Odbc` | `ignore` | ADO.NET — ODBC driver |
| 120 | `System.Data.OleDb` | `ignore` | ADO.NET — OLE DB driver, Windows-only |
| 121 | `System.Data.OracleClient` | `ignore` | ADO.NET — Oracle driver, obsolete |
| 122 | `System.Data.ProviderBase` | `ignore` | ADO.NET — internal base classes for DB providers |
| 123 | `System.Data.SqlClient` | `ignore` | ADO.NET — SQL Server driver |
| 124 | `System.Data.SqlTypes` | `ignore` | ADO.NET — SQL Server native types (SqlInt32, SqlString etc.) |
| 125 | `System.Diagnostics` | `todo` | Debug, Trace, Stopwatch, Process |
| 126 | `System.Diagnostics.CodeAnalysis` | `todo` | Static analysis attributes — NotNull, MaybeNull, DoesNotReturn |
| 127 | `System.Diagnostics.Contracts` | `ignore` | Code contracts — obsolete |
| 128 | `System.Diagnostics.Eventing.Reader` | `ignore` | Windows Event Log reader — Windows-only |
| 129 | `System.Diagnostics.Metrics` | `ignore` | OpenTelemetry metrics — Meter, Counter<T>, Histogram<T> |
| 130 | `System.Diagnostics.PerformanceData` | `ignore` | Windows Performance Counters V2 — Windows-only |
| 131 | `System.Diagnostics.SymbolStore` | `ignore` | PDB symbol store API |
| 132 | `System.Diagnostics.Tracing` | `ignore` | ETW/structured logging — EventSource, EventListener, EventCounter |
| 133 | `System.DirectoryServices` | `ignore` | LDAP/Active Directory — DirectoryEntry, DirectorySearcher |
| 134 | `System.DirectoryServices.AccountManagement` | `ignore` | AD user/group management — Windows enterprise only |
| 135 | `System.DirectoryServices.ActiveDirectory` | `ignore` | AD topology — Domain, Forest, DomainController |
| 136 | `System.DirectoryServices.Design` | `ignore` | AD designer attributes |
| 137 | `System.DirectoryServices.Protocols` | `ignore` | Low-level LDAP protocol |
| 138 | `System.Drawing` | `ignore` | GDI+ wrapper — Color, Point, Rectangle, Bitmap, Graphics, Font |
| 139 | `System.Drawing.Configuration` | `ignore` | GDI+ configuration — Windows/libgdiplus only |
| 140 | `System.Drawing.Printing` | `ignore` | Printing — PrintDocument, PrinterSettings — irrelevant for game dev |
| 141 | `System.Dynamic` | `ignore` | DLR — DynamicObject, ExpandoObject — requires C# dynamic keyword |
| 142 | `System.Dynamic.Utils` | `ignore` | DLR internal utilities |
| 143 | `System.Formats.Asn1` | `ignore` | ASN.1 DER/BER parser — used internally by cryptography only |
| 144 | `System.Formats.Cbor` | `ignore` | CBOR reader/writer — WebAuthn/FIDO2 specific |
| 145 | `System.Formats.Nrbf` | `ignore` | .NET Remoting Binary Format — legacy deserialization |
| 146 | `System.Formats.Nrbf.Utils` | `ignore` | .NET Remoting Binary Format — internal utilities |
| 147 | `System.Formats.Tar` | `todo` | TAR archive reader/writer — GnuTar, PAX, USTAR formats |
| 148 | `System.Globalization` | `todo` | CultureInfo, NumberFormatInfo, DateTimeFormatInfo, Calendar types, TextInfo, RegionInfo, IdnMapping |
| 149 | `System.IO` | `in_progress` | File, Directory, Stream, FileStream, MemoryStream, Path, BinaryReader/Writer |
| 150 | `System.IO.Compression` | `in_progress` | ZipArchive (miniz), GZipStream, DeflateStream, BrotliStream |
| 151 | `System.IO.Enumeration` | `todo` | FileSystemEnumerable<T> — low-level file system enumeration |
| 152 | `System.IO.Hashing` | `todo` | XxHash32/64/128, Crc32, Crc64 — non-cryptographic hashes |
| 153 | `System.IO.IsolatedStorage` | `ignore` | Sandboxed storage — WinForms/Silverlight legacy |
| 154 | `System.IO.MemoryMappedFiles` | `todo` | MemoryMappedFile, MemoryMappedViewAccessor — mmap for large assets |
| 155 | `System.IO.Packaging` | `ignore` | OPC packaging — .docx/.xlsx format, irrelevant for game dev |
| 156 | `System.IO.Pipelines` | `todo` | Pipe, PipeReader, PipeWriter — high-performance I/O pipeline |
| 157 | `System.IO.Pipes` | `todo` | Named/anonymous pipes — IPC |
| 158 | `System.IO.Ports` | `ignore` | SerialPort — RS-232 serial communication |
| 159 | `System.IO.Strategies` | `ignore` | Internal async/sync file access strategies |
| 160 | `System.Linq` | `ignore` | LINQ extension methods — use std::ranges in C++ instead |
| 161 | `System.Linq.Expressions` | `ignore` | Expression trees — ORM/EF infrastructure, not applicable in C++ |
| 162 | `System.Linq.Expressions.Compiler` | `ignore` | IL compilation of expression trees |
| 163 | `System.Linq.Expressions.Interpreter` | `ignore` | Interpreted evaluation of expression trees |
| 164 | `System.Linq.Parallel` | `ignore` | PLINQ — parallel LINQ |
| 165 | `System.Management` | `ignore` | WMI — Windows-only COM infrastructure, irrelevant for game dev |
| 166 | `System.Media` | `ignore` | SoundPlayer, SystemSounds — WAV playback via Win32, Windows-only |
| 167 | `System.Net` | `todo` | IPAddress, IPEndPoint, HttpStatusCode, Cookie, Dns |
| 168 | `System.Net.Cache` | `ignore` | HTTP cache policies |
| 169 | `System.Net.Http` | `todo` | HttpClient, HttpRequestMessage, HttpResponseMessage, HttpContent |
| 170 | `System.Net.Http.Headers` | `todo` | HTTP headers — HttpHeaders, MediaTypeHeaderValue |
| 171 | `System.Net.Http.Json` | `ignore` | JsonContent, GetFromJsonAsync — ASP.NET Core extension |
| 172 | `System.Net.Http.Metrics` | `ignore` | HTTP client OpenTelemetry metrics |
| 173 | `System.Net.Mail` | `ignore` | SmtpClient, MailMessage — email sending |
| 174 | `System.Net.Mime` | `ignore` | MIME types and content disposition |
| 175 | `System.Net.NetworkInformation` | `todo` | NetworkInterface, Ping, IPGlobalProperties |
| 176 | `System.Net.PeerToPeer` | `ignore` | PNRP P2P networking — Windows Vista legacy |
| 177 | `System.Net.PeerToPeer.Collaboration` | `ignore` | P2P collaboration — Windows Vista legacy |
| 178 | `System.Net.Quic` | `ignore` | QUIC protocol — QuicConnection, QuicStream |
| 179 | `System.Net.Security` | `ignore` | TLS/SSL — SslStream, NegotiateAuthentication |
| 180 | `System.Net.ServerSentEvents` | `ignore` | Server-sent events parser/formatter |
| 181 | `System.Net.Sockets` | `in_progress` | Socket, TcpClient, TcpListener, UdpClient — partially implemented |
| 182 | `System.Net.WebSockets` | `ignore` | WebSocket client/server |
| 183 | `System.Net.WebSockets.Compression` | `ignore` | WebSocket per-message deflate compression |
| 184 | `System.Numerics` | `in_progress` | BigInteger, Complex, INumber<T> interfaces, Vector2/3/4, Matrix4x4, Quaternion |
| 185 | `System.Numerics.Colors` | `in_progress` | Matrix3x2/4x4, Plane, Quaternion, Vector2/3/4 — XNA math types |
| 186 | `System.Numerics.Hashing` | `ignore` | Internal — empty public surface |
| 187 | `System.Numerics.Tensors` | `ignore` | TensorPrimitives, TensorSpan<T> — ML/AI tensor ops, irrelevant for game dev |
| 188 | `System.Reflection` | `ignore` | Runtime reflection — Type, MethodInfo, Assembly — CLR-dependent, no C++ equivalent |
| 189 | `System.Reflection.Context` | `ignore` | Custom reflection contexts — CLR-dependent |
| 190 | `System.Reflection.Context.Custom` | `ignore` | Custom reflection context internals |
| 191 | `System.Reflection.Context.Delegation` | `ignore` | Delegation reflection context internals |
| 192 | `System.Reflection.Context.Projection` | `ignore` | Projection reflection context internals |
| 193 | `System.Reflection.Context.Virtual` | `ignore` | Virtual reflection context internals |
| 194 | `System.Reflection.Emit` | `ignore` | Dynamic IL code generation — not applicable in C++ |
| 195 | `System.Reflection.Internal` | `ignore` | Internal reflection utilities |
| 196 | `System.Reflection.Metadata` | `ignore` | PE/ECMA-335 metadata reader — Roslyn/analyzer tooling |
| 197 | `System.Reflection.Metadata.Ecma335` | `ignore` | ECMA-335 metadata API — Roslyn/analyzer tooling |
| 198 | `System.Reflection.PortableExecutable` | `ignore` | PE file reader — .dll/.exe parsing |
| 199 | `System.Reflection.Runtime.BindingFlagSupport` | `ignore` | Internal runtime reflection implementation |
| 200 | `System.Reflection.Runtime.General` | `ignore` | Internal runtime reflection implementation |
| 201 | `System.Reflection.Runtime.TypeInfos` | `ignore` | Internal runtime reflection implementation |
| 202 | `System.Reflection.TypeLoading` | `ignore` | MetadataLoadContext — load types without executing code |
| 203 | `System.Reflection.TypeLoading.Ecma` | `ignore` | MetadataLoadContext ECMA internals |
| 204 | `System.Resources` | `ignore` | ResourceManager, ResXResourceReader — .resx localization system |
| 205 | `System.Resources.Extensions` | `ignore` | .resx extensions for custom types |
| 206 | `System.Resources.Extensions.BinaryFormat` | `ignore` | Binary resources format |
| 207 | `System.Resources.Extensions.BinaryFormat.Deserializer` | `ignore` | Binary resources deserializer |
| 208 | `System.Runtime` | `todo` | GC, WeakReference, Lazy<T>, AppDomain, runtime attributes |
| 209 | `System.Runtime.Caching` | `ignore` | MemoryCache, ObjectCache — in-memory object cache |
| 210 | `System.Runtime.Caching.Configuration` | `ignore` | Cache configuration |
| 211 | `System.Runtime.Caching.Hosting` | `ignore` | Cache hosting interface |
| 212 | `System.Runtime.Caching.Resources` | `ignore` | Internal — no public surface |
| 213 | `System.Runtime.CompilerServices` | `todo` | MethodImpl, CallerMemberName, Unsafe, AsyncStateMachine — compiler attributes |
| 214 | `System.Runtime.ConstrainedExecution` | `ignore` | ReliabilityContract, CriticalFinalizerObject — legacy safety |
| 215 | `System.Runtime.ExceptionServices` | `todo` | ExceptionDispatchInfo, FirstChanceExceptionEventArgs |
| 216 | `System.Runtime.InteropServices` | `todo` | Marshal, DllImport, GCHandle, SafeHandle, ComVisible — P/Invoke |
| 217 | `System.Runtime.InteropServices.ComTypes` | `ignore` | COM type definitions — Windows-only |
| 218 | `System.Runtime.InteropServices.Java` | `ignore` | Java interop — Android only |
| 219 | `System.Runtime.InteropServices.JavaScript` | `ignore` | JS interop — Blazor/Wasm only |
| 220 | `System.Runtime.InteropServices.Marshalling` | `ignore` | Source-gen marshalling — LibraryImport |
| 221 | `System.Runtime.InteropServices.ObjectiveC` | `ignore` | Objective-C interop — macOS/iOS only |
| 222 | `System.Runtime.InteropServices.Swift` | `ignore` | Swift interop — macOS/iOS only |
| 223 | `System.Runtime.Intrinsics` | `ignore` | Vector64/128/256/512<T> — SIMD intrinsics |
| 224 | `System.Runtime.Intrinsics.Arm` | `ignore` | ARM NEON/SVE intrinsics |
| 225 | `System.Runtime.Intrinsics.Wasm` | `ignore` | Wasm SIMD intrinsics |
| 226 | `System.Runtime.Intrinsics.X86` | `ignore` | SSE/AVX x86 intrinsics |
| 227 | `System.Runtime.Loader` | `ignore` | AssemblyLoadContext — dynamic assembly loading |
| 228 | `System.Runtime.Remoting` | `ignore` | .NET Remoting — obsolete, replaced by WCF/gRPC |
| 229 | `System.Runtime.Serialization` | `ignore` | DataContract, ISerializable, XmlObjectSerializer |
| 230 | `System.Runtime.Serialization.DataContracts` | `ignore` | DataContract serialization |
| 231 | `System.Runtime.Serialization.Formatters` | `ignore` | IFormatter interface |
| 232 | `System.Runtime.Serialization.Formatters.Binary` | `ignore` | BinaryFormatter — obsolete, security risk |
| 233 | `System.Runtime.Serialization.Json` | `ignore` | DataContract JSON serializer — legacy |
| 234 | `System.Runtime.Versioning` | `todo` | SupportedOSPlatform, ObsoletedOSPlatform attributes |
| 235 | `System.Security` | `ignore` | SecurityException, PermissionSet, SecureString — CAS infrastructure |
| 236 | `System.Security.AccessControl` | `ignore` | ACL/DACL — FileSecurity, RegistrySecurity — Windows-only |
| 237 | `System.Security.Authentication` | `ignore` | SslProtocols, CipherAlgorithmType enums — TLS |
| 238 | `System.Security.Authentication.ExtendedProtection` | `ignore` | Channel binding tokens — TLS extended protection |
| 239 | `System.Security.Claims` | `ignore` | ClaimsIdentity, ClaimsPrincipal, Claim — identity/auth |
| 240 | `System.Security.Cryptography` | `todo` | SHA256, AES, RSA, HMAC, MD5, RandomNumberGenerator — useful for checksums/encryption |
| 241 | `System.Security.Cryptography.Cose` | `ignore` | COSE (CBOR Object Signing) — IoT/WebAuthn specific |
| 242 | `System.Security.Cryptography.Pkcs` | `ignore` | PKCS#7/CMS — digital signatures, certificates |
| 243 | `System.Security.Cryptography.Pkcs.Asn1` | `ignore` | PKCS ASN.1 internals |
| 244 | `System.Security.Cryptography.X509Certificates` | `ignore` | X.509 certificates — TLS |
| 245 | `System.Security.Cryptography.X509Certificates.Asn1` | `ignore` | X.509 ASN.1 internals |
| 246 | `System.Security.Cryptography.Xml` | `ignore` | XML Digital Signatures |
| 247 | `System.Security.Permissions` | `ignore` | Code Access Security — obsolete |
| 248 | `System.Security.Policy` | `ignore` | CAS policy — obsolete |
| 249 | `System.Security.Principal` | `ignore` | WindowsIdentity, IPrincipal — Windows auth |
| 250 | `System.ServiceModel` | `ignore` | WCF — SOAP/web services |
| 251 | `System.ServiceModel.Channels` | `ignore` | WCF transport channels |
| 252 | `System.ServiceModel.Syndication` | `ignore` | RSS/Atom feed syndication |
| 253 | `System.ServiceProcess` | `ignore` | Windows Services — ServiceBase, ServiceController |
| 254 | `System.Speech` | `ignore` | Windows Speech API — SAPI.dll, Windows-only |
| 255 | `System.Speech.AudioFormat` | `ignore` | Speech audio format definitions |
| 256 | `System.Speech.Internal` | `ignore` | Speech API internals |
| 257 | `System.Speech.Internal.GrammarBuilding` | `ignore` | Speech grammar builder internals |
| 258 | `System.Speech.Internal.ObjectTokens` | `ignore` | SAPI object token internals |
| 259 | `System.Speech.Internal.SapiInterop` | `ignore` | SAPI COM interop internals |
| 260 | `System.Speech.Internal.SrgsCompiler` | `ignore` | SRGS grammar compiler internals |
| 261 | `System.Speech.Internal.SrgsParser` | `ignore` | SRGS grammar parser internals |
| 262 | `System.Speech.Internal.Synthesis` | `ignore` | Speech synthesis internals |
| 263 | `System.Speech.Recognition` | `ignore` | SpeechRecognizer, Grammar — Windows-only |
| 264 | `System.Speech.Recognition.SrgsGrammar` | `ignore` | SRGS grammar definitions |
| 265 | `System.Speech.Synthesis` | `ignore` | SpeechSynthesizer, TTS — Windows-only |
| 266 | `System.Speech.Synthesis.TtsEngine` | `ignore` | TTS engine API |
| 267 | `System.Text` | `in_progress` | StringBuilder, Encoding, UTF8Encoding, ASCIIEncoding, Rune |
| 268 | `System.Text.Encodings.Web` | `ignore` | HtmlEncoder, JavaScriptEncoder, UrlEncoder — web-specific |
| 269 | `System.Text.Json` | `in_progress` | JsonSerializer, JsonDocument, JsonElement, Utf8JsonReader/Writer — via nlohmann/json |
| 270 | `System.Text.Json.Nodes` | `in_progress` | JsonNode, JsonObject, JsonArray, JsonValue — DOM API |
| 271 | `System.Text.Json.Reflection` | `ignore` | Internal — no public surface |
| 272 | `System.Text.Json.Schema` | `ignore` | JSON Schema generation from types |
| 273 | `System.Text.Json.Serialization` | `in_progress` | JsonConverter<T>, JsonSerializerOptions, serialization attributes |
| 274 | `System.Text.Json.Serialization.Converters` | `ignore` | Internal converters — no public surface |
| 275 | `System.Text.Json.Serialization.Metadata` | `ignore` | Source-gen metadata for JSON |
| 276 | `System.Text.RegularExpressions` | `in_progress` | Regex, Match, Group, MatchCollection — via std::regex |
| 277 | `System.Text.RegularExpressions.Symbolic` | `ignore` | Internal symbolic regex engine — no public surface |
| 278 | `System.Text.Unicode` | `todo` | Utf8, Utf16, UnicodeRange — low-level Unicode utilities |
| 279 | `System.Threading` | `in_progress` | Thread, Monitor, Mutex, Semaphore, ManualResetEvent, Interlocked, CancellationToken, Timer |
| 280 | `System.Threading.Channels` | `todo` | Channel<T>, ChannelReader/Writer<T> — async producer/consumer |
| 281 | `System.Threading.RateLimiting` | `ignore` | TokenBucketRateLimiter, SlidingWindowRateLimiter — rate limiting |
| 282 | `System.Threading.Tasks` | `in_progress` | Task, Task<T>, TaskFactory, Parallel, ValueTask, async/await infrastructure |
| 283 | `System.Threading.Tasks.Dataflow` | `ignore` | ActionBlock<T>, TransformBlock<T> — TPL Dataflow pipeline |
| 284 | `System.Threading.Tasks.Dataflow.Internal` | `ignore` | TPL Dataflow internals — no public surface |
| 285 | `System.Threading.Tasks.Sources` | `ignore` | IValueTaskSource<T> — low-level ValueTask infrastructure |
| 286 | `System.Timers` | `todo` | Timer — event-based thread-safe timer |
| 287 | `System.Transactions` | `ignore` | TransactionScope, CommittableTransaction — database transactions |
| 288 | `System.Transactions.Configuration` | `ignore` | Transaction configuration |
| 289 | `System.Transactions.DtcProxyShim` | `ignore` | DTC proxy shim — distributed transactions |
| 290 | `System.Transactions.DtcProxyShim.DtcInterfaces` | `ignore` | DTC COM interfaces |
| 291 | `System.Transactions.Oletx` | `ignore` | OLE TX distributed transactions |
| 292 | `System.Web` | `ignore` | ASP.NET — HttpContext, HttpRequest, HttpResponse |
| 293 | `System.Web.Util` | `ignore` | ASP.NET internal utilities |
| 294 | `System.Windows.Input` | `ignore` | WPF — ICommand, RoutedCommand, KeyGesture |
| 295 | `System.Windows.Markup` | `ignore` | WPF XAML — XamlReader, MarkupExtension |
| 296 | `System.Xaml.Permissions` | `ignore` | XAML security permissions |
| 297 | `System.Xml` | `in_progress` | XmlReader, XmlWriter, XmlDocument, XmlNode — via tinyxml2 |
| 298 | `System.Xml.Linq` | `todo` | XDocument, XElement, XAttribute — LINQ to XML |
| 299 | `System.Xml.Resolvers` | `ignore` | XmlPreloadedResolver — DTD/XSD cache |
| 300 | `System.Xml.Schema` | `ignore` | XmlSchema, XmlSchemaValidator — XSD validation |
| 301 | `System.Xml.Serialization` | `todo` | XmlSerializer, XmlElement/Attribute attributes |
| 302 | `System.Xml.Serialization.Configuration` | `ignore` | XmlSerializer configuration — internal |
| 303 | `System.Xml.XPath` | `ignore` | XPathNavigator, XPathExpression — XPath queries |
| 304 | `System.Xml.Xsl` | `ignore` | XslCompiledTransform — XSLT transformations |
| 305 | `System.Xml.Xsl.IlGen` | `ignore` | XSLT IL code generation internals |
| 306 | `System.Xml.Xsl.Qil` | `ignore` | XSLT QIL (Query Intermediate Language) internals |
| 307 | `System.Xml.Xsl.Runtime` | `ignore` | XSLT runtime internals |
| 308 | `System.Xml.Xsl.XPath` | `ignore` | XSLT XPath internals |
| 309 | `System.Xml.Xsl.Xslt` | `ignore` | XSLT compiler internals |
| 310 | `System.Xml.Xsl.XsltOld` | `ignore` | Legacy XSLT implementation internals |
| 311 | `System.Xml.Xsl.XsltOld.Debugger` | `ignore` | Legacy XSLT debugger internals |
