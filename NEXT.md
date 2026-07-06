# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-06 (branch: `feature/work`, HEAD `fefee64`)*

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
Read NEXT.md first. It reflects the actual, verified repository state as of HEAD fefee64
(10194/10194 tests passing, clean build, zero warnings) — do not assume anything beyond what it
documents.

Start with NEXT.md §8 task 1 (fix the TcpListener plan.sqlite3 status — it's a one-line DB
correction, not a code change, and clears up a DB/reality mismatch before anything else touches
System.Net.Sockets). Then move to task 2 (port System::Net::Sockets::Socket plus its small
supporting enums) as the next substantive piece of work, since AddressFamily/SocketError/
SocketException/SocketAddress/EndPoint/IPEndPoint/NetworkStream all already exist as building
blocks for it.

Do not start on NetworkInterface/Ping/PingReply (§8 task 3) without first reading §4's detail on
why they're a design fork, not a mechanical port — in particular, verify whether ICMP sockets are
even usable in this sandboxed environment before writing any Ping implementation.

Inspect only the files needed for whichever task you pick. Do not refactor unrelated code, and do
not expand scope beyond that one task (see NEXT.md §9 for things explicitly out of bounds right
now). In particular, do not attempt to unify HttpHeaders/WebHeaderCollection/the plain
unordered_map header storage — that's a resolved decision, not an open question.

Make one small, verified improvement:
  1. Read the relevant .NET reference source under /rv/tmp/runtime/src/libraries/ and any existing
     C++ header/source for the type(s) involved (check the filesystem first — plan.sqlite3 can
     drift from reality, as this session found with TcpListener).
  2. Implement/fix per the full checklist in CLAUDE.md (API surface, doc-comments, SPDX header,
     logic parity, getXxxProperty()/setXxxProperty() naming, intcs/bytecs/etc. usage).
  3. If you add new files, reconfigure first: cd build && cmake . && cd ..
  4. Run: cmake --build build --parallel 4   (must be zero errors, zero warnings)
  5. Run: ./build/SharpRuntimeTests           (must show 10194+ passing, zero failures)
  6. If it's a plan.sqlite3-tracked item, update its status:
     sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=<id>;"
  7. Commit only the files for that one change: git commit -m "..."
  8. Update NEXT.md to reflect the new state (test count, HEAD commit, what changed) before ending
     the session.
```
