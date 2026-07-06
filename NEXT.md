# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-06 (branch: `feature/work`, HEAD `e76ae05`)*

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
  - No LINQ port — `std::ranges` is used instead in new code. `System::Linq` exists as a small,
    intentionally partial header (~25 of .NET's ~200 `Enumerable` methods), not a full LINQ engine.
  - Vendored third-party libraries: GoogleTest, nlohmann/json, tinyxml2, miniz. Never commit binaries.
  - Namespaces are opened with C++17 nested syntax: `namespace System::Collections::Generic {`.
  - Complex types get a `.hpp` + `.cpp` pair; simple types may be header-only.

---

## 2. Current status

### Build
**Clean** as of HEAD `e76ae05` — `cmake --build build --parallel 4` produces zero errors and zero
warnings (verified this session, freshly rebuilt, not stale).

### Tests
**9748 / 9748 tests passing** across 997 GoogleTest suites, verified at HEAD `e76ae05`.
`./build/SharpRuntimeTests` is the single test binary covering the whole library.

### CLI / tools / apps / libraries
This repository is a **library only** — there is no CLI, app, or standalone tool. The only build
products are the static library (`SHARP_RUNTIME`) and the test binary (`SharpRuntimeTests`). The
GoogleTest suite is the primary "demo" of working functionality; there is no separate sample app
in this repo (CNA and mobile-eggbert, which consume this library, are separate projects).

### Recently implemented (this session, all fully complete and tested)
**`System.Net` is now fully classified — 22/22 relevant items ported, 4 legacy items marked
ignore.** In dependency order:
- `CredentialCache`, `ICredentials`, `ICredentialsByHost`, `NetworkCredential` — Uri-prefix and
  host/port credential caching with .NET's longest-prefix-match algorithm.
- `DecompressionMethods` (flags enum).
- `System.Net.Sockets.AddressFamily`, `SocketAddress` (own simplified buffer encoding — see §5),
  `EndPoint` (base class; `AddressFamily`/`Serialize`/`Create` throw `NotImplementedException` by
  design, matching .NET's own non-abstract base), `DnsEndPoint`.
- `Dns` — **real** `getaddrinfo`/`getnameinfo`/`gethostname`-backed resolution (Winsock2 on
  Windows, POSIX elsewhere, `PlatformNotSupportedException` on Emscripten), `IPHostEntry`,
  `System.Net.Sockets.SocketError`, `SocketException`. Scoped to the synchronous surface only
  (see §5).
- `HttpRequestHeader`, `HttpResponseHeader` (enums + `GetName()` free functions replacing .NET's
  extension methods); reviewed pre-existing `HttpStatusCode` (added a missing doc-comment, no
  logic gaps).
- `HttpVersion` (well-known `System::Version` constants).
- **`IPAddress` rewritten to add full IPv6 support** — was IPv4-only before this session. Now has
  real RFC 5952 parsing/formatting (`::`-compression, embedded IPv4 tail, numeric scope IDs),
  `AddressFamily`/`ScopeId` properties, `GetAddressBytes`, `MapToIPv4`/`MapToIPv6`, the
  `IsIPv6Multicast`/`LinkLocal`/`SiteLocal`/`Teredo`/`UniqueLocal`/`IsIPv4MappedToIPv6` predicates,
  `IsLoopback`, `HostToNetworkOrder`/`NetworkToHostOrder`, `IPv6Any`/`IPv6Loopback`/`IPv6None`.
  `getAddressProperty()`'s existing IPv4 host-order-uint32 semantics preserved unchanged for
  existing callers (`IPEndPoint`, `TcpClient`, `UdpClient`, `Dns`, `SocketAddress`).
- **`IPEndPoint` rewritten to derive from `EndPoint`** (was a standalone class). Adds IPv6 support
  (bracketed `ToString`, IPv6 in `Parse`/`TryParse`/constructors), port-range validation on
  construction and `setPortProperty` (previously unvalidated — a real gap fixed), `GetHashCode`.
  `SocketAddress` gained an `IPAddress`+port constructor and `GetIPEndPoint()` so
  `Serialize()`/`Create()` genuinely round-trip endpoint data (verified by IPv4 and IPv6
  round-trip tests), not just family+size.
- `IPNetwork` — CIDR network type, byte-mask based (not .NET's `UInt128`-shift, since this runtime
  has no `UInt128`); supports IPv4, IPv6, and cross-family `Contains()` for IPv4-mapped addresses.
- `ProtocolViolationException`, `WebException`, `WebExceptionStatus`.
- `WebHeaderCollection` — composes (doesn't inherit) `NameValueCollection`; real
  `CheckBadHeaderNameChars`/`CheckBadHeaderValueChars` validation ported from .NET's
  `HttpValidationHelpers` (including the CRLF-folding exception), and `IsRestricted()` reproduces
  the request/response-restricted flags from .NET's internal `HeaderInfoTable`.
- Reviewed pre-existing `WebUtility`: added `&#NNN;`/`&#xHH;` numeric HTML entity decoding (emitted
  as UTF-8) and a few more named entities (`nbsp`/`copy`/`reg`/`apos`/`trade`) — was previously
  only the 5 basic named entities.
- `HttpCompletionOption`, `HttpVersionPolicy`, `HttpRequestError` (simple `System.Net.Http` enums —
  first steps into that namespace, see §8).
- Classified `HttpWebRequest`, `HttpWebResponse`, `WebRequest`, `WebResponse` as **ignore**
  (`outofscope=0`): .NET's own source literally comments "effectively obsolete by virtue of
  `WebRequest.Create` being obsolete"; ~2500 lines of legacy pre-`HttpClient` protocol stack,
  superseded by the already-ported `System::Net::Http::HttpClient`.

### What does not work yet
`System.Net.Http` has ~40 remaining `todo` items (see §8) — several files already exist
(`HttpClient`, `HttpContent`, `HttpMethod`, `HttpRequestMessage`, `HttpResponseMessage`,
`ByteArrayContent`, `StringContent`, `FormUrlEncodedContent`) using an established simplified,
**synchronous, non-Stream-based** content model (`ReadAsString()`/`ReadAsByteArray()`, no
`SerializeToStreamAsync`/`Task`), not yet reviewed against the checklist this session. Beyond
that: `System.Net.Http.Headers` (25), `System.Security.Cryptography` (50), `System.Text` (36),
`System.Text.Json.Serialization` (31), `System.Xml.Serialization` (30), `System.Xml.Linq` (24,
separate from `System.Xml`), `System.Net.Sockets` (21, minus the `AddressFamily`/`SocketAddress`/
`SocketError`/`SocketException` pieces ported this session), and smaller `System.Text.*`/
`System.Xml.XPath` namespaces. `System::Net::Sockets::Socket` and `TcpListener` still have no
header at all (not POSIX-only — simply not started; `AddressFamily`/`SocketError`/
`SocketException` now exist as of this session, which unblocks starting them).

---

## 3. Recent changes

Most recent first (see `git log --oneline` for full history):

| Commit | Change |
|--------|--------|
| `e76ae05` | `HttpCompletionOption`, `HttpVersionPolicy`, `HttpRequestError` enums. |
| `b038c6b` | `WebUtility` review: numeric + more named HTML entity decoding. 6 new tests. |
| `849e4fe` | `WebHeaderCollection` (composes `NameValueCollection`; real header validation, `IsRestricted`). 20 new tests. |
| `8218b22` | `ProtocolViolationException`, `WebException`, `WebExceptionStatus`. 5 new tests. |
| `2a35c94` | `IPNetwork` (CIDR, byte-mask based). 12 new tests. |
| `2ff410b` | `IPEndPoint` rewritten to derive from `EndPoint`; `SocketAddress` gained `IPAddress`+port ctor and `GetIPEndPoint()` for real `Serialize()`/`Create()` round-tripping. Fixed unvalidated port range. 24 `IPEndPoint` tests total (14 new). |
| `8f9cd7f` | **`IPAddress` rewritten with full IPv6 support** (was IPv4-only). 35 new tests. Moved to `.hpp`+`.cpp`. |
| `4625d1c` | `HttpVersion`. |
| `558ae50` | `HttpRequestHeader`, `HttpResponseHeader`; reviewed `HttpStatusCode` (doc-comment added). |
| `aeaad47` | `Dns` (real `getaddrinfo`/`getnameinfo`/`gethostname`), `IPHostEntry`, `SocketError`, `SocketException`. 11 new tests exercising real resolution. |
| `80403d6` | `AddressFamily`, `SocketAddress`, `EndPoint`, `DnsEndPoint`. 19 new tests. |
| `2e24d40` | `DecompressionMethods`. |
| `22cbdbb` | `CredentialCache`, `ICredentials`, `ICredentialsByHost`, `NetworkCredential`. 17 new tests. |
| `ebf337d` and earlier | Prior session: `System.Numerics`, `System.Linq` classification, `System.IO.*`, `System.Threading`/`Threading.Tasks`, `System.Xml` (62/62) — see full `git log --oneline` for detail. |

---

## 4. Current blocker / main problem

**No build- or test-breaking blocker exists right now.** The last verified state is clean and
stable (9748/9748 tests, zero warnings, freshly rebuilt this session).

Two things carried over from before, still open:

1. **`Vector<T>` (id `9228` in `plan.sqlite3`, namespace `System.Numerics`) is marked
   `tobedecided`**, not ported. Needs a human architecture decision (fixed-width fallback vs. real
   SIMD intrinsics vs. `std::experimental::simd`) before implementing — not touched this session.
2. **Process risk, not a code bug (from a prior session):** a background porting agent once
   reported "completed" while actually incomplete. **Any future delegated/background work must be
   verified** (actual commits, actual test run) before being treated as done — this session's work
   was all done directly, not delegated, so this doesn't currently apply, but the lesson stands.

---

## 5. Known bugs and limitations

New this session:

| Status | Issue |
|--------|-------|
| documented simplification | `System::Net::SocketAddress`'s buffer layout is this runtime's own simplified encoding (family/port/address at fixed offsets), not guaranteed to match the platform sockaddr ABI — there's no real `Socket`/OS interop yet to need ABI compatibility with. |
| documented limitation | `System::Net::Dns` only ports the synchronous, non-obsolete surface (`GetHostName`, `GetHostAddresses`, `GetHostEntry`) — no async `Task`-returning overloads, no `Begin`/`End` `IAsyncResult` methods, no obsolete `GetHostByName`/`Resolve`/`GetHostByAddress`. `Dns`'s `getaddrinfo` calls are still hardcoded to `AF_INET` even though `IPAddress` gained IPv6 support this same session — a follow-up pass could request/return IPv6 results too, but `Dns` itself wasn't revisited after the `IPAddress` rewrite. |
| documented limitation | `System::Net::IPAddress`/`IPEndPoint`/`IPNetwork` don't port .NET's `Span<T>`/UTF8-based `Parse`/`TryParse`/`TryFormat` overloads or `IParsable`/`ISpanParsable` interface implementations — this runtime has no `Span<T>`/UTF8-string idiom. IPv6 scope IDs are numeric-only (no interface-name-to-index resolution). |
| documented limitation | `System::Net::WebHeaderCollection::GetValues()` returns raw stored values, not re-split through .NET's internal per-header multi-value parser table (e.g. `Set-Cookie`'s quote-aware comma splitting) — that table is large and request/response-plumbing-specific. |
| documented limitation | `System::Net::WebUtility::HtmlEncode` doesn't numeric-entity-encode the Latin-1 supplement range or non-BMP characters like .NET does (would need UTF-8-to-codepoint decoding this class doesn't otherwise need); `HtmlDecode`'s named-entity table is 9 entries, not .NET's ~250. |
| ignore (outofscope=0) | `HttpWebRequest`, `HttpWebResponse`, `WebRequest`, `WebResponse` — .NET's own source calls `WebRequest` "effectively obsolete"; ~2500 lines of legacy pre-`HttpClient` protocol stack, superseded by the already-ported `HttpClient`. |

Carried over from before (still accurate):

| Status | Issue |
|--------|-------|
| incomplete (needs decision) | `System::Numerics::Vector<T>` — no header exists; `tobedecided` pending an architecture choice (see §4). |
| missing | `System::Net::Sockets::Socket` / `TcpListener` — no header exists at all. `AddressFamily`/`SocketError`/`SocketException` now exist (this session), so starting these is more tractable than before. |
| documented limitation | `XmlUrlResolver::GetEntity` only reads local files — no network stack for http(s) entity resolution. |
| documented limitation | `XmlResolver`/`XmlUrlResolver`/`XmlSecureResolver`'s `ofObjectToReturn` parameter is accepted but ignored (no reflection). |
| documented limitation | `XmlReaderSettings`/`XmlWriterSettings` — most properties stored but not consulted by the concrete `XmlReader`/`XmlWriter`. |
| documented limitation | `XmlTextReader`/`XmlTextWriter`/`XmlValidatingReader` use composition, not inheritance. |
| documented limitation | `XmlValidatingReader` performs no actual DTD/XSD validation. |
| documented limitation | `XmlDocument`'s `NodeInserting`/etc. events exist as fields but aren't wired into mutation methods. |
| documented limitation | `System::Threading::Tasks::TaskScheduler` doesn't route `Task` execution; `TaskFactory` omits APM `FromAsync`. |
| ignore (outofscope) | `ConcurrentExclusiveSchedulerPair`, `WaitHandleExtensions`. |
| POSIX-only (known, by design) | `System::Net::Sockets`, `System::IO::RandomAccess`. |
| POSIX/Linux-only (known, by design) | `System::AppDomain`/`AppContext`, `System::TimeZoneInfo`. |
| stub (by design, correct end state) | `System::GC`, `System::Type`, `System::Activator`. |
| legacy DB noise | `plan.sqlite3` has 15055 rows with `status='ignored'` (lowercase-d) predating this workflow — inert. |
| needs verification | Emscripten/Windows builds have never been CI-tested; POSIX guards exist but are unverified there. |

---

## 6. Architecture notes

### Directory layout
- `include/System/...` — public headers, mirroring .NET namespace paths.
- `src/System/...` — `.cpp` bodies for complex types, same mirrored path.
- `tests/System/...` — GoogleTest files, same mirrored path; CMake's `GLOB_RECURSE` auto-discovers
  every `tests/**/*.cpp` and `src/**/*.cpp` — **but you must re-run `cmake .` (reconfigure) after
  adding a new file**, or the build silently won't pick it up. (`CONFIGURE_DEPENDS` handles this on
  the next `cmake --build` invocation in practice, but if in doubt, `cd build && cmake .` explicitly.)
- `vendor/` — GoogleTest, nlohmann/json, tinyxml2, miniz (vendored, never modify in place).
- `plan.sqlite3` — gitignored, local-only porting-progress database.

### Key invariants that must not be broken
- **`getXxxProperty()`/`setXxxProperty()`** naming on every property.
- **`SharpRuntime::intcs`/`bytecs`/`longcs`/`uintcs`**, not native C++ types, in public APIs.
- **C++17 nested namespace syntax** (`namespace System::Net {`).
- **No LINQ** in new ported code — use `std::ranges`.
- **POSIX-only includes** must stay inside `.cpp` files behind `#ifdef`.
- **SPDX header required** on every `.hpp`/`.cpp` file.
- **Doxygen `/** */` only** — and **never write a literal `*/` inside prose inside a `/** */`
  block** (e.g. `Begin*/End*`) — it silently closes the comment early and produces confusing
  "X has not been declared" errors far below the real mistake. Learned the hard way this session
  writing `Dns.hpp`'s doc-comment.
- A derived class that declares **any** overload of a base-class method name hides *all* other
  base-class overloads of that name unless `using BaseClass::MethodName;` is added.

### Data flow / notable patterns
- **`System::Net::EndPoint`/`IPEndPoint`/`DnsEndPoint`**: `EndPoint` is a concrete (not abstract)
  base whose `AddressFamily`/`Serialize`/`Create` throw `NotImplementedException` by design,
  matching .NET's actual base-class behavior (not a C++ abstraction gap). `IPEndPoint` and
  `DnsEndPoint` both derive from it. `SocketAddress` is the serialization target — it now has a
  real `IPAddress`+port constructor and `GetIPEndPoint()` decoder, so `Serialize()`→`Create()`
  round-trips actual endpoint data.
- **`System::Net::IPAddress`** stores IPv4 as a host-order `uint32_t` (unchanged from before this
  session — many call sites depend on this exact representation) and IPv6 as 8×`uint16_t` groups
  plus a scope-ID `uint32_t`. `GetAddressBytes()` is the common currency other types
  (`SocketAddress`, `IPNetwork`) use to work generically across both families via byte-level
  masking/copying rather than family-specific branches everywhere.
- **`System::Net::WebHeaderCollection`** composes `System::Collections::Specialized::
  NameValueCollection` rather than inheriting — that type has no virtual override points, matching
  the established "composition over a non-virtual base" pattern already used for
  `XmlTextReader`/`XmlTextWriter`/`XmlValidatingReader`.
- `System::Net::Http`'s existing types (`HttpClient`, `HttpContent`, etc.) use a deliberately
  simplified **synchronous** content model (`ReadAsString()`/`ReadAsByteArray()`), not .NET's
  `Stream`/`Task`-based `SerializeToStreamAsync`. This predates this session and should be treated
  as an established design point to work within, not rewrite, when reviewing/extending that
  namespace (see §8 task 1).
- `System::Xml`'s DOM classes wrap `tinyxml2::XMLNode*`/`XMLDocument` (unchanged this session).
- `System::Threading::Tasks::Task` is `std::async`-backed, not a lazy continuation model
  (unchanged this session).

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
For each `''`/`todo` item, classify without asking the user: port it (apply the full checklist in
`CLAUDE.md`), mark `ignore` (`outofscope=1` for permanent-deviation categories, `outofscope=0` for
merely-superseded/irrelevant-but-not-permanent-deviation items like the legacy `WebRequest`
family), or mark `tobedecided` only when genuinely ambiguous. `in_progress` is not a valid status.

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
./build/SharpRuntimeTests --gtest_filter="IPAddressIPv6Tests.*"

# Check next unset/todo items in a namespace
sqlite3 plan.sqlite3 "SELECT id,name,type,status FROM task WHERE namespace='System.Net.Http' AND (status='' OR status='todo') ORDER BY id;"

# Mark an item ported after review + tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=<id>;"

# Find the .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit (GPG signing times out in this sandboxed environment — always disable it explicitly)
git -c commit.gpgsign=false commit -m "message"
```

There is no separate lint/format tool configured in this repository, and no standalone demo/sample
binary beyond the GoogleTest suite.

---

## 8. Next smallest tasks

1. **Review and continue `System.Net.Http`** (~40 `todo` items remain, id range starts at 8394).
   - Goal: `HttpClient`, `HttpContent`, `HttpMethod`, `HttpRequestMessage`, `HttpResponseMessage`,
     `ByteArrayContent`, `StringContent`, `FormUrlEncodedContent` already have headers (and
     `HttpClientTests.cpp` already exercises them) but are unmarked in `plan.sqlite3` — review each
     against the checklist (do NOT rewrite the established synchronous content model — see §6).
     `MultipartContent`, `MultipartFormDataContent`, `ReadOnlyMemoryContent`,
     `HttpRequestException`, `HttpIOException`, `HttpProtocolException` have no header yet.
   - Files: `include/System/Net/Http/`, `src/System/Net/Http/`, `tests/System/Net/HttpClientTests.cpp`.
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

2. **`System.Net.Sockets.Socket`/`TcpListener`** — still no header at all (not POSIX-only, simply
   not started). More tractable now: `AddressFamily`, `SocketError`, `SocketException`,
   `SocketAddress`, `EndPoint`/`IPEndPoint` all exist from this session.
   - Files: new `include/System/Net/Sockets/Socket.hpp` + `.cpp`, similarly for `TcpListener`
     (note: `TcpListener` already exists in `TcpClient.hpp`/`.cpp` as a nested class — check there
     first).
   - Verification: `cmake --build build --parallel 4 && ./build/SharpRuntimeTests`.

3. **Decide `System::Numerics::Vector<T>` scope** (id `9228`) — unchanged from before, still needs
   a human architecture decision, not touched this session.

4. **`System.Net.Http.Headers`** (25 items) — natural follow-on once `System.Net.Http` proper is
   reviewed/completed; likely depends on `WebHeaderCollection`/`HttpRequestHeader`/
   `HttpResponseHeader` patterns already established this session.

5. **`System.Security.Cryptography`** (50 items, largest remaining namespace) — not started.

---

## 9. Do not do yet

- **No broad header refactor** — `getXxxProperty()` naming and namespace style already touch
  449+ files across this project and CNA; do not attempt a sweeping rename/reformat pass.
- **No LINQ implementation expansion.**
- **No work on `Vector<T>`** until the architecture decision is made by the user.
- **No Windows/Emscripten CI setup.**
- **No rewrite of `System.Net.Http`'s synchronous content model** to a `Stream`/`Task`-based one —
  that's an established design point from a prior session, not a gap (see §6). If it needs to
  change, that's a decision for the user, not something to do opportunistically while reviewing.
- **Push only to `feature/work`** — never push to `develop`/`master`, never create tags, without
  explicit per-action user approval in that turn. Routine pushes to `origin/feature/work` are
  pre-authorized.
- **No mass rewrite or reformatting** in a single commit — keep following the small,
  reviewable, per-namespace (or per-batch) commit pattern established across both sessions.
- **No blind trust in background/delegated agent "completed" reports** — always verify via
  `git log`/`git status`/an actual test run before treating delegated work as done.
- **No speculative API additions** — only port methods/types that actually exist in .NET's
  published surface.

---

## 10. Resume prompt

```
Read NEXT.md first. It reflects the actual, verified repository state as of HEAD e76ae05
(9748/9748 tests passing, clean build, zero warnings) — do not assume anything beyond what it
documents.

Inspect only the files needed for the first task in NEXT.md §8. Do not refactor unrelated code,
and do not expand scope beyond that one task (see NEXT.md §9 for things explicitly out of bounds
right now). In particular, System.Net.Http's existing types use a deliberately simplified
synchronous content model — review against it, don't rewrite it to be Stream/Task-based.

Make one small, verified improvement:
  1. Read the relevant .NET reference source under /rv/tmp/runtime/src/libraries/ and any existing
     C++ header/source for the type(s) involved (check the filesystem first — several "todo" items
     across both prior sessions turned out to already have a file, just unmarked in plan.sqlite3).
  2. Implement/fix per the full checklist in CLAUDE.md (API surface, doc-comments, SPDX header,
     logic parity, getXxxProperty()/setXxxProperty() naming, intcs/bytecs/etc. usage).
  3. If you add new files, reconfigure first: cd build && cmake . && cd ..
  4. Run: cmake --build build --parallel 4   (must be zero errors, zero warnings)
  5. Run: ./build/SharpRuntimeTests           (must show 9748+ passing, zero failures)
  6. If it's a plan.sqlite3-tracked item, update its status:
     sqlite3 plan.sqlite3 "UPDATE task SET status='ported' WHERE id=<id>;"
  7. Commit only the files for that one change: git -c commit.gpgsign=false commit -m "..."
  8. Update NEXT.md to reflect the new state (test count, HEAD commit, what changed) before ending
     the session.
```
