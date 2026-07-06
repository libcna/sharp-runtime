# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-06 (branch: `feature/work`, HEAD `7751266`) — 10506 tests passing*

**Latest session update (autonomous 24h run, continued):** Since the `dd81e16` commit (System.Text
core namespace, done by a parallel fork earlier in this run), this session directly completed, in
commit order:
1. `6b8d7df` — Fixed two real bugs discovered via a broken build: `Regex::Match` (member function)
   was hiding the sibling `Match` class within `Regex`'s own scope (`-Wchanges-meaning`/`-Werror`),
   fixed via the `class Match Match(...)` elaborated-type-specifier idiom on the declaration; and
   `Match` held a raw `std::smatch` whose sub_matches are iterators into whatever string was
   searched — since `Regex::matchFrom` always searches a local substring destroyed on return, any
   later read of a `Match`'s value was UB (caught via an actual failing test, not just review).
   Fixed by extracting all submatch data into owned strings at `Match` construction time. Completed
   `System.Text.RegularExpressions` (Capture/CaptureCollection/Group/GroupCollection/Match/
   MatchCollection/MatchEvaluator/Regex/RegexOptions/RegexParseError/RegexParseException/
   RegexMatchTimeoutException; `GeneratedRegexAttribute`/`RegexCompilationInfo` marked
   ignore/out-of-scope, source-generator/Reflection.Emit-only).
2. `f793df0` — A failed background fork (ran out of context mid-task) had left an uncommitted,
   broken change to `XName.hpp` (added an implicit `string -> XName` conversion, correct per .NET
   parity, but made `XElement`/`XAttribute`'s redundant `string`-only overloads ambiguous — a real,
   confirmed compile break). Fixed by removing those now-redundant overloads (matches .NET, which
   has no separate string overloads either). Completed the small standalone `System.Xml.Linq`
   support types (`LoadOptions`/`ReaderOptions`/`SaveOptions`/`XObjectChange`/
   `XObjectChangeEventArgs`/`XNamespace`) and reclassified `XName`/`XAttribute`/`XElement`/
   `XDocument`/`XDeclaration` as `ported` (already complete, DB just hadn't caught up). The failed
   fork's `XObject`/`XNode` sketch (a real `XContainer`/`XNode`/`XObject` inheritance hierarchy with
   parent/sibling-tracking) was **not** completed — deleted (never committed, and would require
   migrating `XElement`/`XAttribute`/`XDocument`'s internal storage model, a genuine architecture
   decision, not a mechanical port) and marked `tobedecided`: `XObject`, `XNode`, `XContainer`,
   `XCData`, `XComment`, `XDocumentType`, `XProcessingInstruction`, `XStreamingElement`, `XText`,
   `XNodeDocumentOrderComparer`, `XNodeEqualityComparer`, `Extensions` (the LINQ-style
   `IEnumerable<XElement>` helper methods — would need `std::ranges` free functions over that same
   hierarchy). `ExtractKeyDelegate` marked ignore (nested in the already-ignored internal
   `XHashtable`).
3. `0e95846` — Completed `System.Text.Unicode`: real `Utf16`/`Utf8` (`IsValid`/
   `IndexOfInvalidSubsequence` well-formedness checks; `Utf8::FromUtf16`/`ToUtf16` transcoding with
   `OperationStatus`/replacement/`isFinalBlock` semantics). Fixed pre-existing `UnicodeRange`/
   `UnicodeRanges` checklist gaps found while reviewing them (raw `int` instead of
   `SharpRuntime::intcs`, `std::out_of_range`/`std::invalid_argument` instead of
   `System::ArgumentOutOfRangeException`), and regenerated `UnicodeRanges` from the .NET reference
   source's full 160-block list (mechanically, like `TlsCipherSuite` elsewhere in this runtime)
   instead of the ~38-block hand-picked subset it had, renaming its static factory methods to
   `getXxxProperty()` (they're C# static properties, not methods).
4. `adba9b8` + `7751266` — Completed `System.Text.Json`. `JsonElement`/`JsonDocument` were a stub
   (`JsonElement` had no real parser backing — test-only `addPropertyForTesting`/
   `addArrayItemForTesting` helpers used in the actual production parse path — plus raw
   `int`/`long long`/`double` and `std::runtime_error` instead of real exception types). Rewrote
   both to wrap nodes directly in the parsed `nlohmann::json` tree via aliasing `shared_ptr` (keeps
   the whole document alive; no separate parallel tree), with real `GetInt32`/`GetInt64`
   range/format checks and proper `System::InvalidOperationException`/`FormatException`/
   `IndexOutOfRangeException`/`KeyNotFoundException`/`JsonException`. Added `JsonProperty`.
   `JsonNamingPolicy` was wrongly modeled as a plain enum (also colliding with a duplicate
   `JsonCommentHandling` defined a second time in `JsonSerializerOptions.hpp`) — .NET's real
   `JsonNamingPolicy` is an abstract class with `ConvertName()` and static `CamelCase`/`PascalCase`/
   `SnakeCase*`/`KebabCase*` instances; rewrote as a real class hierarchy implementing .NET's actual
   word-boundary segmentation algorithm (verified against its documented `XMLReader` ->
   `xml_reader` / `SHA512Hash` -> `sha512-hash` examples). Added `JsonCommentHandling`,
   `JsonTokenType`, `JsonSerializerDefaults`, `JsonReaderOptions`, `JsonWriterOptions`,
   `JsonDocumentOptions`, `JsonException`, `JsonEncodedText`. Moved `JsonNumberHandling` to its
   correct namespace (`System.Text.Json.Serialization`, was wrongly under `System.Text.Json`) and
   added its siblings `JsonUnknownTypeHandling`/`JsonUnmappedMemberHandling`. Rewrote
   `JsonSerializerOptions` with the real property set instead of its 5-field stub. Built a real
   `Utf8JsonWriter` (own internal `std::string` buffer standing in for .NET's
   `IBufferWriter<byte>`/`Stream` — no such abstraction in this runtime) with structural validation,
   indentation, and string escaping; two real bugs found by its own test suite before commit: the
   "awaiting a property value" flag was a single writer-wide bool that leaked across nesting depths
   (fixed by moving it per-frame), and the closing-bracket indent computation underflowed `size_t`
   (fixed by computing depth from the already-popped stack size directly). Made
   `JsonSerializer::Serialize<T>`/`Deserialize<T>` do real work via `nlohmann::json`'s ADL
   `to_json`/`from_json` customization points (covers primitives/`std::string`/`std::vector<T>`/
   `std::map<string,T>`/any user type defining those functions) instead of always throwing — this
   stands in for .NET's reflection/source-gen member walking, which is out of scope (see CLAUDE.md's
   parity philosophy). `JsonReaderState` marked `tobedecided` (only meaningful paired with a
   `Utf8JsonReader`, which isn't tracked in `plan.sqlite3` at all and is a large low-level streaming
   API — `JsonDocument`/`JsonElement`/`JsonSerializer` cover the practical use cases).

`System.Text.RegularExpressions`, `System.Xml.Linq` (minus the `tobedecided` hierarchy items),
`System.Text.Unicode`, and `System.Text.Json` are now all fully classified. Remaining `todo` items
in the **entire** `plan.sqlite3` database (16199 rows total): only `System.Text.Json.Nodes` (5) and
`System.Text.Json.Serialization` (31) — next up. Everything else across the whole dotnet/runtime
surface is already `ported`/`ignore(d)`/`tobedecided`.

---

*Prior update (2026-07-06, HEAD `eeece6e`) — 10329 tests passing*

**Latest session update:** Since the `aa23cf0` note below, also completed: `System.Numerics.Colors`
(`Argb`/`Rgba` — files already existed; fixed real gaps: missing `GetHashCode()`, missing static
`CreateBigEndian`/`CreateLittleEndian`/`ToUInt32*Endian` helpers, `std::invalid_argument` instead
of `System::ArgumentException`) and a big batch of small `System.Runtime.*`/`System.Security.*`
namespaces (`CompilerServices`, `ExceptionServices`, `InteropServices`, `Versioning`, `Security`,
`.Authentication`, `.Principal` — 14 real ports incl. `ExceptionDispatchInfo`, `RuntimeInformation`,
`AuthenticationException`, `GenericIdentity`/`GenericPrincipal`, plus fixing DB/reality drift where
`CallerMemberNameAttribute` & co. and `SecurityException` already existed but plan.sqlite3 still
said `todo`). `ported` 770→830, `todo` 322→244 this session. Commits `ea04adb`, `eeece6e`.

**Next item is a real decision point, not a mechanical port:** `System.Security.Cryptography` (50
items, ids in that namespace, the single largest remaining namespace, not started at all). This
codebase has never vendored a crypto library, and `CLAUDE.md`'s architecture invariants require
discussing scope impact before adding one — so this should NOT be decided autonomously by picking
a library. Suggested split, but confirm with the user first if there's any doubt:
- **Hash algorithms** (MD5, SHA1, SHA256/384/512, HMAC-*) are well-defined and moderate-complexity
  to hand-roll with no new dependency — this session already did exactly that for a private
  SHA-1 (see the WebSockets `ClientWebSocket.cpp` Sec-WebSocket-Accept digest, verified correct via
  a real end-to-end handshake test). These could reasonably be ported the same way, as real
  `System::Security::Cryptography::MD5`/`SHA256`/etc. types (not scoped to one file this time).
- **Symmetric/asymmetric crypto** (AES, DES, TripleDES, RSA, DSA, ECDSA, ECDiffieHellman, etc.)
  is much higher-risk to hand-roll (subtle correctness bugs have severe security consequences,
  unlike a WebSocket framing bug) and depends on a real vendoring decision (e.g. OpenSSL/
  libsodium/mbedTLS vs. a header-only crypto library vs. hand-rolled). **Do not silently pick one**
  — mark these `tobedecided` and surface the decision, or ask the user directly if they're
  reachable, before writing any implementation.

After that: `System.Text`/`.Json*`/`.RegularExpressions`/`.Unicode` (~107 combined),
`System.Xml.Serialization`/`.Linq`/`.XPath` (~69 combined), `System.Threading.Channels` (9),
`System.Timers` (4), `System.Security.Cryptography.X509Certificates` (5, likely also blocked on
the crypto-library decision above, and separately on the `SslStream`-family `tobedecided` items
from earlier this session).

---

*Prior update (2026-07-06, HEAD `aa23cf0`) — 10276 tests passing*

**Session note:** This session is running autonomously per `prompt.md` (user unavailable ~24h,
explicitly asked for no pauses — do not stop between items). Progress so far this session, in
commit order:
1. Fixed `TcpListener` DB/reality mismatch (id 9100 → `ported`, no code change).
2. `30b7f21` — Ported `System.Net.NetworkInformation.NetworkInterface` (reduced scope, POSIX
   `getifaddrs()`, Linux-only; `GetIPProperties`/`GetIPStatistics`/`GetIPv4Statistics` omitted
   since their return types are out of scope).
3. `86acbe1` — Ported `System.Net.NetworkInformation.Ping`/`PingReply` — real ICMP via
   unprivileged `SOCK_DGRAM`+`IPPROTO_ICMP` "ping socket" (confirmed working in this sandbox
   before implementing, so no raw-socket privilege needed). **`System.Net.NetworkInformation` is
   now fully classified** (every item `ported` or `ignore(d)`).
4. `7b1a836` — Ported `System.Net.Security` data-only types (`AuthenticationLevel`,
   `EncryptionPolicy`, `SslPolicyErrors`, `SslApplicationProtocol`, `TlsCipherSuite` — the last
   mechanically generated, 337 entries, from the .NET source's own auto-generated enum).
   `SslStream`/`SslClientAuthenticationOptions`/`SslServerAuthenticationOptions`/
   `SslStreamCertificateContext` marked `tobedecided` (blocked on
   `System.Security.Cryptography.X509Certificates`, not started, plus no TLS engine in this
   runtime — a real scope decision, not guessed).
5. `3efb177` — Ported the rest of `System.Net.Sockets` (17 items), including a general-purpose
   `Socket` class (Bind/Connect/Listen/Accept, Send/Receive/SendTo/ReceiveFrom, socket options,
   Poll, Task-based async) supporting Windows+POSIX, mirroring `TcpClient`'s existing platform
   split. **`System.Net.Sockets` is now fully classified.**
6. `aa23cf0` — Ported `System.Net.WebSockets` (12 items). `ClientWebSocket` is a real RFC 6455
   client over `ws://` (`wss://` throws `PlatformNotSupportedException`, no TLS) built on the new
   `Socket` class: real HTTP Upgrade handshake (own small SHA-1 for `Sec-WebSocket-Accept`, not
   the not-yet-ported `System.Security.Cryptography.SHA1`), real masked-frame send/unmasked-frame
   receive, transparent ping/pong, proper close handshake, fragmented-message support. Verified
   with a full end-to-end test against a hand-built mock server (not mocked at any layer).
   **`System.Net.WebSockets` is now fully classified.**

Overall `plan.sqlite3` status this session: `ported` 770→808, `todo` 322→280, `tobedecided` +4
(the `SslStream`-family deferrals above). Test count 10194→10276, all real (no test was skipped
or weakened to make something pass).

Next up (System-namespace-first, alphabetical order of remaining `todo`/`''` items — run the §7
query to get the live list): `System.Numerics.Colors` (2, `Argb`/`Rgba`), `System.Runtime.*`
(`CompilerServices`/`ExceptionServices`/`InteropServices`/`Serialization`/`Versioning`, ~20
combined, all small), `System.Security`/`.Authentication`/`.Principal` (~14, small), then the
large blocks: `System.Security.Cryptography` (50 — the single largest remaining namespace, not
started, needs a scope decision on symmetric/asymmetric crypto and hashing — likely wants a
vendored crypto library discussion, see `CLAUDE.md`'s "No new vendored libraries without
discussing scope impact"), `System.Text`/`.Json*`/`.RegularExpressions`/`.Unicode` (~107
combined), `System.Xml.Serialization`/`.Linq`/`.XPath` (~69 combined), `System.Threading.Channels`
(9), `System.Timers` (4).

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET
`System.*` namespace, so that C#/XNA game code ported to C++ can compile against these headers
with minimal source changes.

- **Main goal:** provide C++ counterparts of `System.*` types so that **CNA** (a C++ XNA port)
  and **mobile-eggbert** (a ported Windows Phone game) can build and run without a .NET runtime.
- **Current phase:** active, incremental porting. Progress is tracked in a local SQLite database,
  `plan.sqlite3` (gitignored, not part of the repo — local workflow state only), which lists every
  type from `dotnet/runtime` and its port status. The process is documented in `prompt.md` and
  `CLAUDE.md` at the repo root; this file is a point-in-time snapshot, not the process definition.
- **Important architectural decisions:**
  - No runtime reflection, no GC, no IL — `System::GC`, `System::Type`, `System::Activator` are
    intentional no-op/stub end states, not gaps.
  - Properties are exposed as `getXxxProperty()` / `setXxxProperty()` methods, never public fields.
  - .NET primitive types map to `SharpRuntime::intcs` (`int32_t`), `bytecs` (`uint8_t`), `longcs`,
    `uintcs`, etc. — public APIs mirroring a .NET `int` parameter must use `intcs`, not `int`.
  - Inner exceptions use `std::exception_ptr`, never `const std::exception&`.
  - No LINQ port — `std::ranges` is used instead in new code.
  - Vendored third-party libraries: GoogleTest, nlohmann/json, tinyxml2, miniz. Never commit binaries.
  - Namespaces are opened with C++17 nested syntax: `namespace System::Collections::Generic {`.
  - Complex types get a `.hpp` + `.cpp` pair; simple types may be header-only.
  - `System::Net::Http::Headers::HttpHeaders` (and its `HttpContentHeaders`/`HttpRequestHeaders`/
    `HttpResponseHeaders` subclasses) are a **fourth, deliberately separate** simplified header-bag
    design — not integrated with `HttpRequestMessage`/`HttpResponseMessage`'s plain
    `unordered_map<string,string>` or with `WebHeaderCollection`. This was a resolved design fork
    (see §6) — do not attempt to unify the three going forward without being asked.

---

## 2. Current status

### Build
**Clean** as of HEAD `fefee64` — `cmake --build build --parallel 4` produced zero errors and zero
warnings when last verified this session (freshly rebuilt with touched object files removed, not
stale). Not re-verified in this specific update pass (per instructions, no build was run while
writing this file) — but no source changes have been made since that verification.

### Tests
**10194 / 10194 tests passing** across 1043 GoogleTest suites, verified at HEAD `fefee64`.
`./build/SharpRuntimeTests` is the single test binary covering the whole library.

### CLI / tools / apps / libraries
This repository is a **library only** — there is no CLI, app, or standalone tool. The only build
products are the static library (`SHARP_RUNTIME`) and the test binary (`SharpRuntimeTests`). The
GoogleTest suite is the primary "demo" of working functionality; there is no separate sample app
in this repo (CNA and mobile-eggbert, which consume this library, are separate projects).

### Recently implemented (this session, all fully complete and tested)

**`System.Net.Http.Headers` is now fully classified — every item is `ported` or `ignore`.**
Completed this session, in dependency order:
- The remaining individual header-value types: `AuthenticationHeaderValue`,
  `CacheControlHeaderValue`, `ContentDispositionHeaderValue`, `ContentRangeHeaderValue`,
  `MediaTypeHeaderValue`/`MediaTypeWithQualityHeaderValue`, `ProductInfoHeaderValue`,
  `ViaHeaderValue`, `WarningHeaderValue`, `RangeConditionHeaderValue`,
  `RangeItemHeaderValue`/`RangeHeaderValue`, `RetryConditionHeaderValue`,
  `TransferCodingHeaderValue`/`TransferCodingWithQualityHeaderValue`.
- **`HttpHeaders`** (base class, composes `NameValueCollection`) + **`HttpHeadersNonValidated`**
  (thin wrapper — functionally identical to `HttpHeaders` here, since there's no
  parsed-value cache to distinguish "validated" vs "non-validated" access).
- **`HttpContentHeaders`**, **`HttpRequestHeaders`**, **`HttpResponseHeaders`** — typed property
  access built on top of `HttpHeaders::getRawValue()`/`setRawValue()`. List-valued headers
  (Accept, Connection, Via, Warning, etc.) are snapshot getters + an `Add(item)` mutator, not a
  live `HttpHeaderValueCollection<T>`. `HttpRequestHeaders`/`HttpResponseHeaders` each
  independently implement the "general headers" (Cache-Control, Connection, Date, Pragma, Trailer,
  Transfer-Encoding, Upgrade, Via, Warning) — .NET's internal shared `HttpGeneralHeaders` helper
  is not reproduced; the logic is duplicated per class instead (established codebase convention).

**`System.Net.Http.Json`** (all 3 items ported, reduced non-generic scope):
- `JsonContent` — `HttpContent` backed by pre-serialized JSON; constructed from a raw string or via
  `Create()` from an `nlohmann::json` value.
- `HttpContentJsonExtensions`/`HttpClientJsonExtensions` — `ReadFromJson(Async)`,
  `GetFromJsonAsync`, `PostAsJsonAsync`, `PutAsJsonAsync`, `PatchAsJsonAsync`,
  `DeleteFromJsonAsync`. These return a parsed `System::Text::Json::JsonDocument` instead of an
  arbitrary `T` — this runtime has no reflection, and `JsonSerializer::Serialize<T>()`/typed
  `Deserialize<T>()` are intentional stubs (see §5). `HttpClientJsonExtensions`' tests spin up a
  real local `TcpListener`-backed HTTP server to exercise the full request/response path.

**`System.Net.Mime`** (2 items ported):
- `ContentType` — independent RFC 2045 Content-Type parser with its own token/quoted-string
  grammar (matches .NET: `ContentType` is **not** built on `MediaTypeHeaderValue`, they're separate
  types in separate libraries). Wire-persistence caching tied to `System.Net.Mail`'s
  message-writing pipeline is not reproduced (mail itself isn't ported here).
- `MediaTypeNames` — trivial static string-constant namespaces (`Application`, `Font`, `Image`,
  `Multipart`, `Text`, `Video`).

**`System.Net.NetworkInformation`** (12 support items ported; `NetworkInterface`/`Ping`/
`PingReply` still `todo`, see §4):
- Enums: `IPStatus`, `NetworkInterfaceType`, `OperationalStatus`, `NetworkInterfaceComponent`.
- `PhysicalAddress` — full MAC-address parser (hyphen/colon/dot-delimited and unpunctuated hex),
  porting .NET's segment-length-inference algorithm faithfully.
- `NetworkInformationException` (uses `errno` in place of .NET's `Marshal.GetLastPInvokeError()`,
  since there's no P/Invoke layer here), `PingException`, `PingOptions`.
- `NetworkAvailabilityEventArgs`, `NetworkAddressChangedEventHandler`/
  `NetworkAvailabilityChangedEventHandler` delegate typedefs, and `NetworkChange` — the latter's
  event add/remove accessors are **stubs** (no real OS network-change notification), matching this
  codebase's pre-existing `AppDomain.UnhandledException` convention.

### What does not work yet
- `System.Net.NetworkInformation.NetworkInterface`/`Ping`/`PingReply` are not ported (see §4 — they
  depend on types already marked out of scope, and `Ping` needs raw ICMP sockets).
- `System::Net::Sockets::Socket` (the general BSD-socket-style class) has **no header at all**.
  `TcpClient`/`TcpListener`/`NetworkStream`/`UdpClient` exist and work, but `plan.sqlite3` still
  lists `TcpListener` (id 9100) as `todo` even though it's implemented as a nested class inside
  `TcpClient.hpp` — this is a **DB/reality mismatch that should be fixed first** in a future session
  (see §8 task 1), not a missing feature.
- Everything listed under §5/"remaining namespaces" in `plan.sqlite3` is simply not yet looked at:
  `System.Security.Cryptography` (50 items, the single largest remaining namespace), `System.Text`
  (36), `System.Text.Json.Serialization` (31), `System.Xml.Serialization` (30), `System.Xml.Linq`
  (24), `System.Net.Sockets` (18, minus `TcpListener`/`TcpClient`/`AddressFamily`/`SocketError`/
  etc. already done), `System.Text.Json` (17), `System.Xml.XPath` (15),
  `System.Text.RegularExpressions` (14), `System.Net.WebSockets` (12), `System.Net.Security` (9),
  `System.Threading.Channels` (9), and several smaller namespaces (full list: run the query in §7).

---

## 3. Recent changes

Most recent first (see `git log --oneline` for full history):

| Commit | Change |
|--------|--------|
| `fefee64` | `System.Net.NetworkInformation` support types (enums, `PhysicalAddress`, exceptions, `PingOptions`, `NetworkAvailabilityEventArgs`, delegates, `NetworkChange` stub). 23 new tests. |
| `d58d032` | `System.Net.Http.Json` (`JsonContent`, `HttpContentJsonExtensions`, `HttpClientJsonExtensions`) and `System.Net.Mime` (`ContentType`, `MediaTypeNames`). 44 new tests, including a real local-socket HTTP server integration test for `HttpClientJsonExtensions`. |
| `a1bd3ed` | `System.Net.Http.Headers.HttpResponseHeaders` — completes `System.Net.Http.Headers` classification. 24 new tests. |
| `73ff81b` | `System.Net.Http.Headers.HttpRequestHeaders`. 38 new tests. |
| `b7299f1` | `System.Net.Http.Headers.HttpContentHeaders`. 17 new tests. |
| `0238c72` | `System.Net.Http.Headers.HttpHeaders`, `HttpHeadersNonValidated` (the base collection design). |
| `6bcffcf`–`ef6bbdc` | The remaining individual `System.Net.Http.Headers` value types (`TransferCodingHeaderValue`+with-quality, `RetryConditionHeaderValue`, `RangeItemHeaderValue`/`RangeHeaderValue`, `RangeConditionHeaderValue`, `WarningHeaderValue`, `ViaHeaderValue`, `ProductInfoHeaderValue`, `MediaTypeHeaderValue`+with-quality, `ContentRangeHeaderValue`, `ContentDispositionHeaderValue`, `CacheControlHeaderValue`, `AuthenticationHeaderValue`) — each its own commit with tests. |
| `f586c73` and earlier | Prior session: `System.Net` core (IPAddress IPv6 rewrite, IPEndPoint, IPNetwork, WebHeaderCollection, Dns, CredentialCache, etc.) and `System.Net.Http` core (HttpClient, HttpContent family, Multipart*, HttpIOException/HttpProtocolException) fully classified — see `git log --oneline` for detail, or the previous revision of this file in git history. |

---

## 4. Current blocker / main problem

**No build- or test-breaking blocker exists right now.** The last verified state is clean and
stable (10194/10194 tests, zero warnings).

The main open item is a **scoping decision, not a bug**:

1. **`NetworkInterface`, `Ping`, `PingReply`** (`System.Net.NetworkInformation`, ids 8844/8850/8856)
   are the only `todo` items left in that namespace, and neither is a mechanical port:
   - `NetworkInterface.GetIPProperties()`/`GetIPStatistics()`/`GetIPv4Statistics()` return
     `IPInterfaceProperties`/`IPInterfaceStatistics`/`IPv4InterfaceStatistics` — all three are
     already marked `ignored` in `plan.sqlite3` (out of scope, part of the large PAL-internal
     interface-property subsystem). A full `NetworkInterface` port is therefore not possible as-is;
     only a reduced surface (`Name`, `Id`, `NetworkInterfaceType`, `OperationalStatus`,
     `GetPhysicalAddress()`, `Supports()`, `GetAllNetworkInterfaces()` via POSIX `getifaddrs`) is
     realistic, and that reduction needs to be a deliberate documented decision, not silently done.
   - `Ping`/`PingReply` need raw ICMP sockets (`SOCK_RAW`/`IPPROTO_ICMP`, or the unprivileged
     `SOCK_DGRAM`+`IPPROTO_ICMP` variant Linux supports via
     `net.ipv4.ping_group_range`) — this needs to be verified as workable in the actual sandboxed
     test environment before committing to an implementation approach, since ICMP sockets commonly
     require elevated privileges that a CI/sandbox runner may not have.
   - **Nothing has been implemented or attempted for these three yet** — this is a fresh decision
     point for the next session, not a partially-done piece of work.

2. Carried over, unchanged, still open:
   - **`Vector<T>`** (id `9228`, `System.Numerics`) is `tobedecided` — needs a human architecture
     decision (fixed-width fallback vs. real SIMD intrinsics vs. `std::experimental::simd`).
   - **`FileSystemInfo`** (id `6595`, `System.IO`) is also `tobedecided` — reason not re-investigated
     this session; check `plan.sqlite3` notes/git history before assuming why.
   - **Process risk, not a code bug:** always verify any delegated/background agent's "completed"
     report (actual commits, actual test run) before treating it as done.

---

## 5. Known bugs and limitations

New this session:

| Status | Issue |
|--------|-------|
| documented limitation | `System::Net::Http::Json::HttpClientJsonExtensions`/`HttpContentJsonExtensions` only provide non-generic, `JsonDocument`-returning overloads (`GetFromJsonAsync`, `ReadFromJsonAsync`, etc.) — .NET's generic `GetFromJsonAsync<T>`/`PostAsJsonAsync<T>` need reflection-based `JsonTypeInfo<T>` marshaling this runtime doesn't have. `System::Text::Json::JsonSerializer::Serialize<T>()` remains an intentional stub (pre-existing, not touched this session) — see `JsonSerializerTests.Serialize_ThrowsNotImplemented`. |
| documented limitation | `System::Net::Mime::ContentType` doesn't reproduce .NET's `_isChanged`/`_isPersisted` wire-caching (tied to `System.Net.Mail`'s message-writing pipeline, which isn't ported) — `ToString()` always recomputes fresh. Its RFC 2045 comment/CFWS grammar support is plain-whitespace-only (no nested `(...)` comments). |
| documented limitation | `System::Net::NetworkInformation::NetworkChange`'s event add/remove accessors are no-ops — there is no real OS network-change notification (Linux netlink, macOS `SCNetworkReachability`, Windows `NotifyAddrChange`), matching the pre-existing `AppDomain.UnhandledException` stub convention in this codebase. |
| documented limitation | `System::Net::NetworkInformation::NetworkInformationException`'s default constructor uses `errno` in place of .NET's `Marshal.GetLastPInvokeError()` (no P/Invoke layer here); its internal `(message, innerException)` constructor isn't reproduced (`Win32Exception`, the base class, has no inner-exception-carrying constructor to forward to). |
| DB/reality mismatch (should be fixed) | `plan.sqlite3` id 9100 (`TcpListener`) is still marked `todo`, but `TcpListener` is already fully implemented as a nested class in `include/System/Net/Sockets/TcpClient.hpp` (confirmed working — it backs this session's `HttpClientJsonExtensionsTests` integration tests). Fix the DB status before starting new `System.Net.Sockets` work so the next session doesn't re-investigate something already done. |

Carried over from before (still accurate unless noted):

| Status | Issue |
|--------|-------|
| incomplete (needs decision) | `System::Numerics::Vector<T>` — no header exists; `tobedecided` pending an architecture choice (see §4). |
| tobedecided (needs re-investigation) | `System::IO::FileSystemInfo` — marked `tobedecided`; reason not re-verified this session. |
| missing | `System::Net::Sockets::Socket` — still no header at all. `TcpClient`/`TcpListener`/`NetworkStream`/`UdpClient`/`AddressFamily`/`SocketError`/`SocketException` all exist and are more tractable building blocks now than when this was first noted. |
| documented simplification | `System::Net::SocketAddress`'s buffer layout is this runtime's own simplified encoding, not guaranteed to match the platform sockaddr ABI. |
| documented limitation | `System::Net::Dns`'s `getaddrinfo` calls are still hardcoded to `AF_INET` even though `IPAddress` has full IPv6 support — never revisited after the `IPAddress` IPv6 rewrite. |
| documented limitation | `System::Net::WebHeaderCollection::GetValues()` returns raw stored values, not re-split through .NET's internal per-header multi-value parser table. |
| ignore (outofscope=0) | `HttpWebRequest`, `HttpWebResponse`, `WebRequest`, `WebResponse` — .NET's own source calls `WebRequest` "effectively obsolete"; superseded by `HttpClient`. |
| documented limitation | `XmlUrlResolver::GetEntity` only reads local files — no network stack for http(s) entity resolution. |
| documented limitation | `XmlReaderSettings`/`XmlWriterSettings` — most properties stored but not consulted by the concrete `XmlReader`/`XmlWriter`. |
| documented limitation | `XmlValidatingReader` performs no actual DTD/XSD validation. |
| documented limitation | `System::Threading::Tasks::TaskScheduler` doesn't route `Task` execution; `TaskFactory` omits APM `FromAsync`. |
| ignore (outofscope) | `ConcurrentExclusiveSchedulerPair`, `WaitHandleExtensions`. |
| POSIX-only (known, by design) | `System::Net::Sockets`, `System::IO::RandomAccess`. |
| POSIX/Linux-only (known, by design) | `System::AppDomain`/`AppContext`, `System::TimeZoneInfo`. |
| stub (by design, correct end state) | `System::GC`, `System::Type`, `System::Activator`. |
| legacy DB noise | `plan.sqlite3` has 15055 rows with `status='ignored'` (lowercase-d, note the distinct casing from the workflow's own `'ignore'` value) predating this workflow — inert, do not "fix" the casing, just be aware both exist. |
| needs verification | Emscripten/Windows builds have never been CI-tested; POSIX guards exist but are unverified there. |

---

## 6. Architecture notes

### Directory layout
- `include/System/...` — public headers, mirroring .NET namespace paths.
- `src/System/...` — `.cpp` bodies for complex types, same mirrored path.
- `tests/System/...` — GoogleTest files, same mirrored path; CMake's `GLOB_RECURSE` auto-discovers
  every `tests/**/*.cpp` and `src/**/*.cpp` — **but you must re-run `cmake .` (reconfigure) after
  adding a new file**, or the build silently won't pick it up.
- `vendor/` — GoogleTest, nlohmann/json, tinyxml2, miniz (vendored, never modify in place).
- `plan.sqlite3` — gitignored, local-only porting-progress database.

### Key invariants that must not be broken
- **`getXxxProperty()`/`setXxxProperty()`** naming on every property.
- **`SharpRuntime::intcs`/`bytecs`/`longcs`/`uintcs`**, not native C++ types, in public APIs.
- **C++17 nested namespace syntax** (`namespace System::Net {`).
- **No LINQ** in new ported code — use `std::ranges`.
- **POSIX-only includes** must stay inside `.cpp` files behind `#ifdef`.
- **SPDX header required** on every `.hpp`/`.cpp` file.
- **Doxygen `/** */` only** — never write a literal `*/` inside prose inside a `/** */` block.
- A derived class that declares **any** overload of a base-class method name hides *all* other
  base-class overloads of that name unless `using BaseClass::MethodName;` is added.
- **`strchr(allowedChars, c)` matches `c == 0`** (the haystack's own NUL terminator) — always guard
  with `c != 0 &&`, or prefer `std::string_view::find` instead (a real bug found and fixed twice
  in a prior session, across 5 files).

### Data flow / notable patterns
- **`System::Net::Http::Headers::HttpHeaders`** composes `NameValueCollection` (not inheritance) —
  same "composition over a non-virtual base" pattern as `WebHeaderCollection` and
  `XmlTextReader`/`XmlTextWriter`. Derived typed classes (`HttpContentHeaders` etc.) access the
  base's raw string storage through `protected getRawValue()/setRawValue()`, then parse/format
  through the individual `*HeaderValue` types on every access — there is **no lazily-parsed-and-
  cached value**, unlike real .NET's `HttpHeaders`. This is why `HttpHeadersNonValidated` is a
  functionally-identical thin wrapper here: there's no raw/parsed distinction to preserve.
- **General HTTP headers are duplicated, not shared**: `HttpRequestHeaders` and
  `HttpResponseHeaders` each independently implement Cache-Control/Connection/Date/Pragma/Trailer/
  Transfer-Encoding/Upgrade/Via/Warning, rather than sharing .NET's internal `HttpGeneralHeaders`
  helper — consistent with this codebase's broader preference for small duplicated per-file
  helpers (e.g. `tryParseRfc1123`, `splitTopLevel`, `isHttpTokenChar` are each copy-pasted across
  several `System.Net.Http.Headers` files) over introducing shared abstractions.
- **List-valued typed headers are snapshot + Add(), not a live collection**: every `getXxxProperty()`
  for a multi-value header (Accept, Via, Warning, Connection tokens, etc.) returns a `std::vector<T>`
  snapshot; there is a corresponding `AddXxx(item)` mutator instead of .NET's live
  `HttpHeaderValueCollection<T>` view.
- **`System::Net::Http::Json`**: `JsonContent`/`HttpClientJsonExtensions`/`HttpContentJsonExtensions`
  only support JSON via `nlohmann::json` values or raw strings, returning a parsed `JsonDocument`
  tree — not .NET's reflection-driven `T`. If `System::Text::Json::JsonSerializer` ever gains a
  real `Serialize<T>()`/`Deserialize<T>()` backend (e.g. via an ADL `to_json`/`from_json`
  convention), these JSON extension classes are the natural place to add generic overloads.
- **Event-accessor stubs**: for .NET static/instance events with no feasible native backing in this
  runtime (`AppDomain.UnhandledException`, `NetworkChange.NetworkAddressChanged`/
  `NetworkAvailabilityChanged`), the established pattern is literal no-op
  `add_XxxChanged(handler)`/`remove_XxxChanged(handler)` static methods — not a `std::vector` of
  registered handlers that's never invoked, and not silently omitting the API. Follow this same
  pattern for any future un-implementable event.
- **`System::Net::IPAddress`** stores IPv4 as a host-order `uint32_t` and IPv6 as 8×`uint16_t`
  groups plus a scope-ID `uint32_t`. `GetAddressBytes()` is the common currency other types use.
- `System::Net::Http`'s existing types (`HttpClient`, `HttpContent`, etc.) use a deliberately
  simplified **synchronous** content model (`ReadAsString()`/`ReadAsByteArray()`), not .NET's
  `Stream`/`Task`-based `SerializeToStreamAsync`. `System::Threading::Tasks::TaskT<T>` is a real,
  working `std::async`-backed future type (not a stub) — used this session to implement the
  `*Async` JSON extension methods by wrapping already-synchronous `HttpClient` calls in
  `TaskT<T>::Run([...]{ ... })`, matching the pattern already used internally by
  `HttpClient::GetAsync`/`PostAsync`/etc.
- `System::Xml`'s DOM classes wrap `tinyxml2::XMLNode*`/`XMLDocument` (unchanged).

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
For each `''`/`todo` item, classify without asking the user: port it (apply the full checklist in
`CLAUDE.md`), mark `ignore` (`outofscope=1` for permanent-deviation categories, `outofscope=0` for
merely-superseded/irrelevant-but-not-permanent-deviation items), or mark `tobedecided` only when
genuinely ambiguous. `in_progress` is not a valid status. **Before trusting a `plan.sqlite3` status,
spot-check the filesystem** — this session found one item (`TcpListener`) marked `todo` despite
already being fully implemented; the DB can drift from reality.

---

## 7. Useful commands

```bash
# Build (zero errors/warnings required) — reconfigure first if you added new files
cd build && cmake . && cd .. && cmake --build build --parallel 4

# Build, showing only errors/warnings
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run the full test suite
./build/SharpRuntimeTests

# Run a specific suite/test (glob pattern)
./build/SharpRuntimeTests --gtest_filter="PhysicalAddressTests.*"

# Check next unset/todo items in a namespace
sqlite3 plan.sqlite3 "SELECT id,name,type,status FROM task WHERE namespace='System.Net.Sockets' AND (status='' OR status='todo') ORDER BY id;"

# See remaining todo counts by namespace, largest first
sqlite3 plan.sqlite3 "SELECT namespace, COUNT(*) FROM task WHERE status='' OR status='todo' GROUP BY namespace ORDER BY COUNT(*) DESC;"

# Mark an item ported after review + tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"

# Find the .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit and push (routine pushes to origin/feature/work are pre-authorized)
git add <files>
git commit -m "message"
git push origin feature/work
```

There is no separate lint/format tool configured in this repository, and no standalone demo/sample
binary beyond the GoogleTest suite.

---

## 8. Next smallest tasks

1. **Fix the `TcpListener` DB/reality mismatch first** (id 9100) — it's already fully implemented
   in `include/System/Net/Sockets/TcpClient.hpp` (as a nested `TcpListener` class) and exercised by
   `tests/System/Net/Http/Json/HttpClientJsonExtensionsTests.cpp`. Just verify it against the full
   porting checklist in `CLAUDE.md` (doc-comments, SPDX, etc. — likely already fine) and mark it
   `ported`: `sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=9100;"`. No code change
   expected, just DB correctness — do this before starting new `System.Net.Sockets` work.
   - Files: `include/System/Net/Sockets/TcpClient.hpp` (read-only check).
   - Verification: none needed beyond re-reading the existing header against the checklist.

2. **Port `System::Net::Sockets::Socket`** (id 9072) — the general BSD-socket-style class, still
   completely missing. `AddressFamily`, `SocketError`, `SocketException`, `SocketAddress`,
   `EndPoint`/`IPEndPoint`, and `NetworkStream` all already exist as building blocks. Also port the
   small supporting enums in the same namespace while there (`ProtocolType`, `SocketType`,
   `SocketShutdown`, `SocketFlags`, `SelectMode`, `SocketOptionLevel`, `SocketOptionName`,
   `LingerOption`) — each is trivial once `Socket` itself exists.
   - Files: new `include/System/Net/Sockets/Socket.hpp` + `src/System/Net/Sockets/Socket.cpp`,
     `tests/System/Net/Sockets/SocketTests.cpp`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

3. **Decide the `NetworkInterface`/`Ping`/`PingReply` reduced-scope design** (ids 8844/8850/8856,
   see §4 for the full detail) — this is a genuine design fork, not mechanical:
   - For `NetworkInterface`: confirm the reduced surface (name/id/type/status/physical
     address/`Supports()`/`GetAllNetworkInterfaces()` via POSIX `getifaddrs`, skipping
     `GetIPProperties()`/`GetIPStatistics()`/`GetIPv4Statistics()` since their return types are
     `ignored`) is acceptable before implementing.
   - For `Ping`: **first** check whether raw/unprivileged ICMP sockets are actually usable in the
     sandboxed test environment (`cat /proc/sys/net/ipv4/ping_group_range`, or try opening a
     `SOCK_DGRAM`+`IPPROTO_ICMP` socket) — if not available, `Ping` tests can't verify real network
     behavior and the port would need to be scoped down further (e.g. testable packet
     construction/parsing only, with the actual send/receive loop behind a runtime capability
     check that throws a clear exception rather than silently failing).
   - Files: new `include/System/Net/NetworkInformation/NetworkInterface.hpp`/`.cpp`,
     `Ping.hpp`/`.cpp`, `PingReply.hpp`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

4. **`System.Security.Cryptography`** (50 items, the largest remaining namespace) — not started.
   Good next big block once the smaller `System.Net.*` remnants above are settled.

5. **`System.Net.Security`** (9 items) and **`System.Net.WebSockets`** (12 items) — smaller,
   adjacent to the `System.Net.*` work just completed; check for any dependency on `Socket`
   (task 2) before starting either.

6. **Decide `System::Numerics::Vector<T>` scope** (id `9228`) — unchanged, still needs a human
   architecture decision, not touched this or last session.

7. **Re-investigate `System::IO::FileSystemInfo`'s `tobedecided` status** (id `6595`) — check git
   history/prior session notes for why it was left ambiguous; it may just need a definitive port
   or ignore decision now.

---

## 9. Do not do yet

- **No broad header refactor** — `getXxxProperty()` naming and namespace style already touch
  hundreds of files across this project and CNA; do not attempt a sweeping rename/reformat pass.
- **No unifying the three (now four, with `HttpHeaders`) simplified header-bag designs** in
  `System.Net`/`System.Net.Http` unless explicitly asked — this was a deliberate, resolved decision
  this session (see §1/§6), not an oversight to "fix".
- **No work on `Vector<T>`** until the architecture decision is made by the user.
- **No attempt at real ICMP `Ping` implementation** before confirming raw/unprivileged ICMP sockets
  actually work in the sandboxed environment (see §8 task 3) — building it blind risks tests that
  can never pass in CI.
- **No Windows/Emscripten CI setup.**
- **No rewrite of `System.Net.Http`'s synchronous content model** to a `Stream`/`Task`-based one —
  that's an established design point from an earlier session, not a gap.
- **Push only to `feature/work`** — never push to `develop`/`master`, never create tags, without
  explicit per-action user approval in that turn. Routine pushes to `origin/feature/work` are
  pre-authorized.
- **No mass rewrite or reformatting** in a single commit — keep following the small, reviewable,
  per-namespace (or per-batch) commit pattern established across all sessions so far.
- **No blind trust in background/delegated agent "completed" reports** — always verify via
  `git log`/`git status`/an actual test run before treating delegated work as done.
- **No speculative API additions** — only port methods/types that actually exist in .NET's
  published surface.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation, don't stop between items). NEXT.md is a snapshot for context, not the
source of truth for process. This reflects the verified repository state as of HEAD aa23cf0
(10276/10276 tests passing, clean build, zero warnings) — do not assume anything beyond what it
documents; re-verify with a fresh build+test run after any context reset.

Query the live next-item list (System-namespace-first):
  sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo')
  ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 20;"

As of this update, that starts with System.Numerics.Colors (2 items), then the small
System.Runtime.* namespaces (~20 combined), then System.Security/.Authentication/.Principal
(~14), then the two large not-yet-started blocks: System.Security.Cryptography (50 items — the
single largest remaining namespace; will likely need a scope decision on whether to vendor a
crypto library, e.g. for AES/RSA — CLAUDE.md requires discussing that before adding one) and
System.Text/.Json*/.RegularExpressions/.Unicode (~107 combined) and System.Xml.Serialization/
.Linq/.XPath (~69 combined).

For each item: classify (port/ignore/tobedecided) per prompt.md Step 2 without asking the user,
then if porting: check the filesystem first (plan.sqlite3 can drift from reality — this session
already found and fixed one such case, TcpListener), implement per the full checklist in
CLAUDE.md (API surface, doc-comments, SPDX header, logic parity, getXxxProperty()/
setXxxProperty() naming, intcs/bytecs/etc. usage), reconfigure if you added files
(cd build && cmake . && cd ..), build clean (cmake --build build --parallel 4 — zero
errors/warnings), run the full suite (./build/SharpRuntimeTests — must show 10276+ passing, zero
failures), update plan.sqlite3's status, commit (and push to origin/feature/work — routine
pushes are pre-authorized), then move to the next item without stopping.

Do not expand scope beyond CLAUDE.md's "Known permanent deviations" and this session's own
documented reduced-scope decisions (see the per-commit notes above) — e.g. do not attempt TLS,
do not add SendFile/SendPacketsAsync to Socket, do not add permessage-deflate to WebSocket, unless
explicitly asked. Update NEXT.md's session note (prepend, don't rewrite the whole history) after
each meaningful batch of work, so this resumes cleanly after any context reset.
```
