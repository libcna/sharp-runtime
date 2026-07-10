# NEXT.md — sharp-runtime handoff document

*Last updated: 2026-07-10 (branch: `feature/work`, HEAD `a5c34ec`) — 11152 tests passing, full clean rebuild verified (0 errors/0 warnings)*

## Session checkpoint (2026-07-10, continued again) — wave-3 priority item 4 fixed (Utf8JsonWriter escaping/MaxDepth + JsonDocument MaxDepth)

*Branch: `feature/work`, HEAD `a5c34ec` — 11152 tests passing (up from 11142 at the top of
the item-3 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Fixed both parts of the wave-3 "suggested processing priority" list's item 4, each verified
against the corresponding real .NET source:

- **`Utf8JsonWriter::appendEscapedString` non-ASCII passthrough** — verified against
  `DefaultJavaScriptEncoder.cs`/`AllowedBmpCodePointsBitmap.cs`: the default
  `JavaScriptEncoder`'s allow-list is `UnicodeRanges.BasicLatin` minus undefined/control
  characters (including DEL) minus HTML-sensitive characters (`< > & ' " +`) minus explicit
  extra escapes for `\` and backtick — net effect, every codepoint ≥ U+0080 must be escaped
  as `\uXXXX` (or a `\uXXXX\uYYYY` surrogate pair for astral codepoints ≥ U+10000). The
  previous implementation only escaped control chars, `"`, `\`, and `<>&'` — non-ASCII text
  (names, i18n strings, emoji) passed straight through as raw UTF-8 bytes inside the JSON
  string, and `+`/backtick/DEL were missed even within ASCII. Added a `decodeUtf8` helper
  (same validated-decode pattern used by `ASCIIEncoding`/`UnicodeEncoding`/`UTF32Encoding`/
  `IdnMapping` elsewhere in this codebase) and rewrote `appendEscapedString` to use it. 6
  regression tests (non-ASCII, astral surrogate pair, `+`/backtick/DEL).
- **`Utf8JsonWriter`/`JsonWriterOptions.MaxDepth` never resolved from its 0 sentinel** —
  verified against `Utf8JsonWriter.cs`'s `SetOptions()`: real .NET resolves `MaxDepth == 0`
  to `JsonWriterOptions.DefaultMaxDepth` (1000) once, at construction time, so the writer's
  three depth-check call sites (all originally gated by `MaxDepth > 0 && ...`) actually
  engage. This port left `MaxDepth` at 0 forever for default-constructed writers, silently
  disabling every depth check — unbounded nesting wrote successfully with no
  `InvalidOperationException`, unlike real .NET. Added `JsonWriterOptions::DefaultMaxDepth =
  1000` and resolved it in the `Utf8JsonWriter` constructor.
- **`JsonDocument::Parse` never enforced `MaxDepth` at all** — verified against
  `Utf8JsonReader.cs` (the reader `JsonDocument.Parse` delegates depth tracking to) and
  `JsonDocumentOptions.cs` (`DefaultMaxDepth = 64`): real .NET throws once nesting reaches
  the configured/default depth. This port validated `MaxDepth >= 0` but never checked it
  against the parsed tree at all — pathologically deep documents parsed with no limit. Added
  `JsonDocumentOptions::DefaultMaxDepth = 64` and a post-parse recursive depth walk
  (`JsonDocument::checkMaxDepth`) that throws `JsonException` with .NET's exact message
  format (`Strings.resx`'s `ArrayDepthTooLarge`/`ObjectDepthTooLarge`) once the effective
  max depth is exceeded. 4 regression tests (default/custom `MaxDepth`, at-limit succeeds,
  one-over throws).

Both `Utf8JsonWriter` (commit `4ffb04e`) and `JsonDocument` (commit `a5c34ec`) fixes are
built, tested, and pushed. All four items from the "suggested processing priority" list are
now done. **Next: item 5 — everything else, namespace by namespace (~180 remaining
findings)**, same discipline as waves 1-2: verify against real .NET source, fix, add
regression tests, rebuild/retest clean, commit, push, checkpoint.

## Session checkpoint (2026-07-10, continued again) — wave-3 priority item 3 fixed (all 4 memory-safety criticals)

*Branch: `feature/work`, HEAD `8a440a2` — 11142 tests passing (up from 11136 at the top of
the "top 2 priority items" checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Fixed all four memory-safety criticals from the wave-3 "suggested processing priority" list's
item 3, each verified against the corresponding real .NET source before fixing:

- **`HttpClient::Send()` null-pointer dereference** — added
  `ArgumentNullException::ThrowIfNull(request)`, matching `HttpClient.cs`'s
  `CheckRequestBeforeSend`. Commit `dc02094`.
- **`Socket::Send/Receive/SendTo/ReceiveFrom` missing bounds validation** — added a
  `validateBufferArgs()` helper matching `Socket.Tasks.cs`'s `ValidateBufferArguments`
  exactly (casts offset/count to `uint32_t` before comparing, so a negative value is caught
  by the same range check as a too-large one). Commit `ab60037`.
- **`ClientWebSocket::SendAsync`/`ReceiveAsync` missing bounds validation** — added the
  equivalent `validateWebSocketBuffer()` helper, matching `WebSocketValidate.cs`'s
  `ValidateBuffer`; validated synchronously before the returned `Task` is constructed,
  matching real .NET's async-method-validates-synchronously convention (confirmed against
  `ManagedWebSocket.cs`). Commit `ed80e24`.
- **`XmlReader` post-EOF out-of-bounds `events[pos]` access** — 8 methods
  (`getNameProperty`, `getValueProperty`, `getIsEmptyElementProperty`, `MoveToElement`,
  `MoveToNextAttribute`, `GetAttribute`, `ReadStartElement`, `ReadEndElement`) only checked
  `pos < 0`, not the upper bound `pos >= events.size()` that `getNodeTypeProperty()` already
  had — after `Read()` returns false at EOF, `pos` sits exactly at `events.size()`, so any of
  these called after an ordinary "read until EOF" loop indexed out of bounds. Fixed all 8 to
  match `getNodeTypeProperty()`'s existing correct check. Commit `8a440a2`.

Each fix has a regression test exercising exactly the previously-broken path. All four items
from the "suggested processing priority" list's items 1-3 are now done. **Item 4
(`Utf8JsonWriter` non-ASCII escaping + `MaxDepth`/`JsonDocument` depth-limit enforcement) is
next**, followed by the remaining ~180 catalogued findings (item 5: "everything else,
namespace by namespace").

## Session checkpoint (2026-07-10, continued again) — wave-3 catalogue: top 2 priority items fixed

*Branch: `feature/work`, HEAD `367357e` — 11136 tests passing (up from 11129 at the top of
the wave-3-dispatch checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Picked off the top two items from the wave-3 "suggested processing priority" list below:

- **`ZipArchive` Update-mode data loss + `ZipArchiveEntry::Delete()` no-op** (IO.Compression
  criticals #1-2) — fixed. `flushWriter()` now extracts every pre-existing, non-deleted
  entry into memory before opening the writer (the reader must be fully drained/closed
  first, since the writer may truncate the same backing file/buffer), then writes those
  entries alongside the pending ones. Added a `deletedEntries` set so `Delete()` actually
  excludes an entry instead of doing nothing. 3 regression tests. Commit `568323a`.
- **Threading's `Timeout.Infinite` (-1) systemic mishandling** (found independently ~11
  times across `Monitor`/`Mutex`/`Semaphore`/`SemaphoreSlim`/`Lock`/`SpinWait`/
  `AutoResetEvent`/`ManualResetEvent`/`EventWaitHandle`/`ManualResetEventSlim`/
  `CountdownEvent`/`WaitHandle.WaitAll`/`WaitAny`) — fixed uniformly: every site now
  special-cases `millisecondsTimeout == -1` to call the underlying untimed blocking
  primitive instead of computing a timed wait that std::chrono treats as already-expired.
  5 regression tests (one per underlying primitive shape: mutex-like, semaphore-like,
  event-like, spin-based, multi-handle), each proving the fix by asserting the wait is
  still blocked after 100ms before signaling it to complete. Commit `367357e`.

The IO.Compression and Threading sections of the wave-3 catalogue below are otherwise
unchanged (all their other findings remain open) — only the two specific items above are
resolved. The "suggested processing priority" list's items 1-2 are done; **item 3 (memory-
safety criticals: Socket bounds validation, XmlReader post-EOF OOB access, ClientWebSocket
buffer bounds, HttpClient null deref) is next.**

## Session checkpoint (2026-07-10, continued again) — wave-3 audit dispatched, 221 findings (56 critical)

*Branch: `feature/work`, HEAD `2701f60` — 11129 tests passing (up from 11125 at the top of
part 5's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

### What happened

Per option (b) of the original sweep instruction, dispatched a wave-3 parallel audit — 9
read-only general-purpose agents, same methodology as waves 1-2 (compare each ported type's
actual logic against real .NET source in `/rv/tmp/runtime/src/libraries/`, not just its
public signature) — covering `System.Net.*` (2 agents), `System.Diagnostics*`, `System.IO.*`
(2 agents), `System.Text.Json*`, `System.Threading.*`, and `System.Xml.*` (2 agents). ~450
files audited.

**Result: 221 findings, 56 critical, 99 moderate, 66 minor.** This is far more than waves 1-2
combined and reflects that `System.Net.*`/`System.IO.*`/`System.Threading.*`/`System.Xml.*`
had never been systematically audited before (unlike `System.Globalization`/
`System.Collections`, which waves 1-2 already covered). **This volume cannot be safely
processed in one sitting** — each finding needs the same verify-against-.NET-source →
fix → build → test → commit discipline used all session, and rushing 200+ fixes risks
regressions. Only the Diagnostics slice (7 findings, 0 critical) was fixed this pass; see
below. The rest are catalogued here for systematic processing across future sessions,
exactly like waves 1-2's findings were.

### Fixed this pass: System.Diagnostics (7/7 findings)

All verified against `Stopwatch.cs`/`Stopwatch.Unix.cs`, `DebuggableAttribute.cs`,
`DebuggerBrowsableAttribute.cs`, `DebugProvider.cs`, `StackFrame.cs`. Commit `2701f60`.

- **Stopwatch**: backing clock was `std::chrono::high_resolution_clock`, which on this
  project's toolchain (libstdc++/GCC) is an alias for `system_clock` (wall clock, not
  monotonic) — real .NET's `GetTimestamp()` is `clock_gettime(CLOCK_MONOTONIC)`. Switched to
  `std::chrono::steady_clock`, matching `PeriodicTimer`/`WaitHandle`/`SpinWait`/`Thread`'s
  existing convention.
- **DebuggableAttribute**: `IsJITTrackingEnabled`/`IsJITOptimizerDisabled` were stored bools
  only populated by the `(bool,bool)` constructor, leaving them wrong (always false) when
  built via the `DebuggingModes`-flags constructor. Fixed by deriving both from
  `debuggingModes_` live (bit-test), matching .NET's computed-property design.
- **DebuggerBrowsableAttribute**: added the missing `state < Never || state > RootHidden`
  range validation (note: this does NOT specially reject the removed `Expanded=1` — it falls
  within range, so real .NET's check silently accepts it too; a candidate test asserting
  otherwise was corrected after failing against the actual verified behavior).
- **Debug::Assert(bool)**: called the raw `assert()` macro instead of routing through
  `Fail()`/the active `DebugProvider` like the message-carrying overloads — fixed to
  delegate to `Assert(condition, "")`.
- **DebugProvider::Fail()**: used `assert(false)` (a no-op under `NDEBUG`), contradicting its
  own doc comment and .NET's `[DoesNotReturn]` contract — switched to `std::abort()`
  unconditionally (kept as a plain virtual, not `[[noreturn]]`, since it's legitimately
  overridable by a non-aborting custom provider).
- **StackFrame**: `nativeOffset_` defaulted to `0` instead of `OFFSET_UNKNOWN` (`-1`);
  `ilOffset_` two lines below already correctly defaulted to `-1`. Fixed.
- (Not fixed — deferred) `Debugger::getIsAttachedProperty()` is hardcoded `false`; a real fix
  needs a new `.cpp` parsing `/proc/self/status`'s `TracerPid` on Linux (POSIX-only, per
  CLAUDE.md's platform-abstraction rule) plus a cmake reconfigure for the new file. Low
  priority (minor severity), noted here as an easy follow-up.

### Full findings catalogue (not yet fixed) — 214 findings, 56 critical

Organized by audited slice. File paths are relative to the repo root. Severity counts are
per-slice as reported by each audit agent.

#### System.Net core + Sockets (24 findings: 4 critical, 12 moderate, 8 minor)

**Critical:**
1. `SocketError` is never translated from POSIX `errno` — every `SocketException` carries a
   meaningless error code on Linux (raw errno reinterpreted as the enum, e.g. `ECONNREFUSED`
   =111 read as if it were the Winsock-numbered `SocketError` value). Standard idioms like
   `catch (e) { if (e.SocketErrorCode == SocketError::WouldBlock) ... }` never match. Needs
   an errno→`SocketError` translation table; keep raw errno as a separate native-code field.
2. `Socket::Send/Receive/SendTo/ReceiveFrom` have no offset/count bounds validation — real
   OOB heap read/write when `offset+count > buffer.size()` (`src/System/Net/Sockets/Socket.cpp:497-576`).
3. `IPAddress` IPv4 parsing uses `sscanf("%u.%u.%u.%u%c")` — UB on segment overflow (a
   sufficiently long digit run is UB per C11 7.21.6.2p10), and accepts input real .NET
   rejects (`sscanf`'s optional sign) while rejecting input .NET accepts (octal/hex segments,
   short forms). `src/System/Net/IPAddress.cpp:23-30`.
4. `WebUtility::UrlDecode` throws an uncaught `std::invalid_argument` (not a `System::`
   exception) on malformed percent-encoding, e.g. `"100%complete"`.
   `include/System/Net/WebUtility.hpp:147`.

**Moderate (12) and minor (8)**: Socket setters skip .NET's validation (`ExclusiveAddressUse`
after bind, negative timeouts, negative buffer sizes); `TcpClient`/`TcpListener`/`UdpClient`
are effectively IPv4-only (IPv6 throws, hostname resolution doesn't fall back);
`SocketFlags` cast directly to native flags with no translation (bit patterns don't match
Linux, e.g. .NET's `Truncated` coincides with Linux `MSG_WAITALL`); IPv6 scope-ID parsing can
throw uncaught `std::out_of_range`; `IsLoopback` misses `::ffff:127.0.0.1`; `IPEndPoint`
integer constructors silently truncate instead of throwing; `Dns` doesn't reject
`0.0.0.0`/`::`; `WebUtility::HtmlDecode`/`UrlEncode` have wrong entity-scan/safe-char-set
logic; `TcpListener::Start()` hardcodes backlog 5 instead of `int.MaxValue`;
`Socket::Accept()` skips bound/listening validation; `IPAddress::GetHashCode()` only hashes
the first 64 bits of IPv6 (hash-table degradation for same-`/64`-prefix addresses); plus 8
minor items (embedded-IPv4 formatting, scope-ID range validation, bracketed-IPv6 parsing,
hostname length check, `NetworkStream` post-close silent no-op, etc.) — see the full
per-finding detail in the agent's original report (not preserved verbatim here; re-run a
similar audit prompt on `include/System/Net/*.hpp` + `Sockets/*.hpp` to regenerate if needed).

#### System.Net.Http + WebSockets + Security + Mime + NetworkInformation (19 findings: 2 critical, 13 moderate, 4 minor)

**Critical:**
1. `HttpClient::Send()` dereferences a null `request` immediately with no
   `ArgumentNullException` check — null-pointer dereference (UB/crash) instead of a catchable
   exception. `src/System/Net/Http/HttpClient.cpp`.
2. `ClientWebSocket::SendAsync`/`ReceiveAsync` do `buffer.data() + offset` with no
   offset/count bounds validation against `buffer.size()` — out-of-bounds read/write.
   `src/System/Net/WebSockets/ClientWebSocket.cpp:334-411`.

**Moderate (13) highlights**: `HttpClient::parseUrl` throws `std::invalid_argument` instead
of a `System::` exception (same systemic std::/System:: bug noted elsewhere in this
project's history); `HttpResponseMessage` skips status-code range validation; quality
(`q=`) header parsing accepts NaN/malformed values via unguarded `std::stod`;
`CacheControlHeaderValue` seconds parsing can silently overflow-wrap; several header getters
accept negative values .NET rejects; `ClientWebSocketOptions` subprotocol validation is too
permissive and case-sensitive where .NET is case-insensitive; `ClientWebSocket` doesn't
validate message type or close-status codes before sending; `NetworkInterface.GetIsNetworkAvailable()`
misses the Tunnel-interface exclusion.

**Minor (4)**: `Content-Length` accepts a leading `+`; `SslApplicationProtocol.ToString()`
doesn't implement its documented hex-dump fallback; `ValueWebSocketReceiveResult` skips
`messageType` range validation; `Ping::Send` throws `ArgumentException` instead of
`ArgumentNullException` for a null/empty host.

#### System.IO core (22 findings: 7 critical, 10 moderate, 5 minor)

**Critical:**
1. `MemoryStream::Write()`/`WriteByte()` silently no-op instead of throwing
   `NotSupportedException` when `!CanWrite` — writes are silently dropped.
   `src/System/IO/MemoryStream.cpp:28-43`. (`SetLength()` in the same file gets this right —
   internal inconsistency confirms it's a bug.)
2. `File::Move`/`Directory::Move`/`DirectoryInfo::MoveTo`/`FileInfo::MoveTo` all silently
   overwrite an existing destination via unconditional `std::filesystem::rename()`; real
   .NET's non-overwrite `Move` throws `IOException` if the destination exists.
3. `FileStream::getLengthProperty()` returns a stale cached length after `Write()` extends
   the file — only `SetLength()` updates the cache. Common create-then-write pattern returns
   wrong (often 0) `Length`. `src/System/IO/FileStream.cpp:49,102,152`.
4. `StreamReader::Close()`/`StreamWriter::Close()` ignore `leaveOpen` and unconditionally
   close the underlying stream, defeating the flag's entire purpose. (Their destructors and
   `BinaryReader`/`BinaryWriter::Close()` get this right.)

**Moderate (10) highlights**: `File::Delete` can delete a directory (uses
`std::filesystem::remove()`, which dispatches to `rmdir()`); `RandomAccess::Write` doesn't
loop on short/partial writes; `Directory::GetFiles(path, "*.*")` wildcard excludes
extensionless files (wrong DOS_DOT compatibility); `FileSystemInfo`'s local-time properties
return UTC verbatim instead of converting; generic `IOException` instead of
`DirectoryNotFoundException` on missing parent directory; `MemoryStream::Read()`/`Close()`
have more silent-wrong-behavior bugs (returns 0 on invalid args instead of throwing;
`Close()` destroys the buffer, contradicting its own doc comment and .NET's `Dispose`);
`UnmanagedMemoryStream` throws the wrong exception type on a closed stream;
`StreamReader`/`StreamWriter` constructors skip null/`CanRead`/`CanWrite` validation;
`BinaryWriter` doesn't flush on Close when `leaveOpen=true`; `StreamReader::ReadLine()`/
`StringReader::ReadLine()` don't treat a lone `'\r'` as a line terminator.

**Minor (5)**: `Path::IsPathRooted` Windows-build inconsistency (not exercised on Linux);
`FileSystemWatcher` filter normalization; `FileSystemEventArgs::Combine()` alt-separator
handling; `PathTooLongException` missing HResult; `BinaryReader::Read` missing a null check.

#### System.IO.Compression + Hashing + IsolatedStorage (13 findings: 4 critical, 6 moderate, 3 minor)

**Critical — this is the highest-value fix in the entire wave-3 catalogue (real, silent data loss):**
1. `ZipArchive::flushWriter()` (Update-mode `Dispose()`) only writes newly-`CreateEntry`'d
   entries — for a file-backed archive it re-inits the writer on the same path opened for
   reading, so **disposing an Update-mode archive after even one `CreateEntry()` call
   overwrites the file, discarding every pre-existing entry**. `src/System/IO/Compression/ZipArchive.cpp:154-186,280-291`.
2. `ZipArchiveEntry::Delete()` does `state_ = nullptr;` only — never marks anything for
   exclusion, so the "deleted" entry is fully intact in the persisted output regardless.
   `src/System/IO/Compression/ZipArchive.cpp:123-129`.
3. `DeflateStream`/`GZipStream` constructors/`Read()`/`Write()` throw bare
   `std::runtime_error` on zlib failures instead of `System::` exception types — uncatchable
   by code catching `System::Exception&`/`IOException&`. Sibling `ZLibStream.cpp` already
   does this correctly, confirming it's a bug not a design choice.

**Moderate (6)**: rest of `ZipArchive`'s failure paths also throw bare `std::runtime_error`;
`GZipEncoder::GetMaxCompressedLength()` omits the `+12` gzip header/trailer overhead
(understates worst-case buffer size); `IsolatedStorageFile` methods skip path-null/empty
validation; `DeleteDirectory()` is always recursive (`remove_all`) where .NET's non-recursive
default throws `IOException` on a non-empty directory; disposed-state (`disposed_`) is set
but never checked anywhere, so all operations remain usable after `Close()`/`Remove()`;
`IsolatedStorageFile::getQuotaProperty()` isn't overridden, so it inherits the base's `0`
instead of real .NET's `long.MaxValue`.

**Minor (3)**: `DeflateEncoder::GetMaxCompressedLength()` truncates on >4GiB input (32-bit
`compressBound`); Application-scope and Assembly-scope isolated storage share the same root
(not actually isolated from each other, though documented); `IsolatedStorageException`
never sets its HResult.

**Hashing: 0 findings.** CRC32/CRC32C/CRC64-ECMA182/XxHash32/64/3/128 constants, polynomials,
seeds, bit/byte order, and output-endianness convention were all independently verified
against `Crc32ParameterSet.WellKnown.cs`/`Crc64ParameterSet.WellKnown.cs` and found correct.

#### System.Text.Json (26 findings: 7 critical, 12 moderate, 7 minor)

**Critical:**
1. `Utf8JsonWriter`'s string escaping never escapes non-ASCII characters (only control
   chars, `"`, `\`, `<>&'`) — real .NET's default encoder escapes every codepoint ≥U+0080 as
   `\uXXXX`. Any non-ASCII string (names, i18n text, emoji) round-trips unescaped instead of
   matching .NET's byte-for-byte output. `src/System/Text/Json/Utf8JsonWriter.cpp:37-66`.
2. `Utf8JsonWriter`'s `MaxDepth` default of `0` enforces no limit at all (`options_.MaxDepth
   > 0 && ...` guards are always skipped at the default) — real .NET resolves `0` to 1000.
   Unbounded native recursion / stack-overflow risk on deeply-nested writes with default
   options.
3. `JsonDocument::Parse` never enforces `MaxDepth` at all — arbitrarily deep/malicious input
   parses with no bound (verified: 5000-level nesting parses fine) instead of throwing
   `JsonException`, risking a native stack overflow.
4. `JsonElement::TryGetInt32`/`TryGetInt64` round-trip through `double` — UB when casting an
   out-of-range double to a signed integer, plus precision loss for large int64 values
   (doubles only exactly represent integers up to 2^53). The non-`Try` `GetInt64()` sibling
   already does this correctly.
5. `JsonElement::GetProperty` always throws `KeyNotFoundException`, even when called on a
   non-object element, where real .NET throws `InvalidOperationException` — breaks
   catch-block discrimination between "wrong shape" and "missing key".
6. `JsonObject`/`JsonArray` `Add`/`SetItem`/`Insert` have no "already has a parent" or cycle
   check — a node can be silently attached to two containers at once (dangling non-owning
   `parent_` pointer → use-after-free risk) or become its own ancestor (unbounded recursion
   in `ToJsonString`/`DeepClone`/etc.).
7. `JsonObject::operator[]` throws `KeyNotFoundException` for a missing key; real .NET
   returns `null` — breaks the single most common .NET JSON-node idiom (`obj["maybe"]`
   null-check pattern).

**Moderate (12) highlights**: `WriteNumberValue(double)` doesn't reject NaN/Infinity (nlohmann
silently emits `null` instead); missing ASCII escapes for `+`, backtick, DEL;
`AllowTrailingCommas`/`AllowDuplicateProperties` (JsonDocumentOptions) are validated but
never actually enforced by the underlying nlohmann parse; `GetRawText()` reformats numbers
instead of returning exact source text; `GetInt32`/`GetInt64` too lenient (accepts `3.0`/`2e1`
where .NET's strict digit parser throws); `JsonSerializerOptions.AllowDuplicateProperties`
defaults `false` where .NET defaults `true` (and the sibling `JsonDocumentOptions` version in
the *same codebase* correctly defaults `true` — confirms this is an oversight);
`JsonSerializerOptions(Strict)` is a silent no-op (falls through to `General` behavior);
`JsonEncodedText::Encode` doesn't validate/pre-escape its input, defeating its whole purpose;
`JsonNodeOptions.PropertyNameCaseInsensitive` is stored but never consulted;
`JsonValue` accessors throw `FormatException` instead of `InvalidOperationException`;
`JsonPolymorphicAttribute.UnknownDerivedTypeHandling` is typed `bool` instead of the
3-valued enum that already exists elsewhere in the same directory.

**Minor (7)**: double formatting/hex-escape casing cosmetically diverges from .NET (valid
JSON either way); `JsonWriterOptions.NewLine` hardcoded `"\n"` (only observable on Windows);
`JsonException` doesn't append position info to its message text; `WriteRawValue` validates
in the wrong order; several `JsonElement` temporal/numeric getters (`GetGuid`, `GetDateTime`,
`GetDecimal`, etc.) don't exist yet (API-surface gap, not a wrong-value bug);
`GetString()` on JSON `null` returns `""` (self-documented as an intentional deviation).

#### System.Threading + Tasks + Channels (63 findings: 24 critical, 29 moderate, 10 minor)

**By far the largest and most severe slice — a systemic bug pattern plus several real
deadlocks/data races/UB. Recommended top priority for the next processing session.**

**Systemic critical pattern (found independently ~11 times): `Timeout.Infinite` (`-1`) is
never special-cased before being fed into `std::chrono::milliseconds(-1)`/`wait_for`, which
the standard treats as an already-expired deadline** — these return almost immediately
instead of blocking forever, unlike .NET where `-1` means infinite wait:
`Monitor::TryEnter`, `Mutex::WaitOne`, `Semaphore::WaitOne`, `SemaphoreSlim::Wait`,
`Lock::TryEnter`, `SpinWait::SpinUntil`, `AutoResetEvent::WaitOne(intcs)`,
`ManualResetEvent::WaitOne(intcs)`, `EventWaitHandle::WaitOne(intcs)`,
`ManualResetEventSlim::Wait(intcs)`, `CountdownEvent::Wait(intcs)`,
`WaitHandle::WaitAll`/`WaitAny`. `ReaderWriterLock::AcquireReaderLock/WriterLock` in the same
file correctly special-cases `<0`, proving this is an inconsistency bug, not a design
choice. **Fix direction: a single shared helper (`WaitInfinite`-aware) used by all of these
would likely fix most of the pattern in one well-scoped pass** — worth tackling as a batch
given how mechanically similar each site is, rather than 11 separate one-off fixes.

**Other critical findings (13, non-Timeout.Infinite):**
- `ReaderWriterLock::AcquireReaderLock`/`AcquireWriterLock` silently `return` on timeout
  instead of throwing `ReaderWriterLockApplicationException` — callers proceed without
  holding the lock.
- `ReaderWriterLockSlim::TryEnterReadLock/WriteLock/UpgradeableReadLock(intcs)` discard the
  timeout parameter entirely (always a single non-blocking attempt); also ignores
  `LockRecursionPolicy` entirely, so same-thread recursion **deadlocks** instead of throwing
  `LockRecursionException`; recursive `EnterReadLock()` has a bug where the second matching
  `ExitReadLock()` throws (`unordered_set` membership-only tracking, not a count) and
  **permanently starves all future writers**.
- `Barrier::SignalAndWait`: when the post-phase action throws, only the triggering thread
  sees `BarrierPostPhaseException` — every other participant of that phase silently proceeds
  as if it succeeded. `Barrier::FinishPhase` invokes the post-phase action while still
  holding its mutex — reentrant calls **deadlock** instead of throwing
  `InvalidOperationException`.
- `CountdownEvent::AddCount` has unchecked signed integer overflow (UB) — same bug class as
  the already-fixed TimeSpan copy_count/move_count race from earlier this session, but for
  overflow rather than a data race.
- `TaskCompletionSource<T>` completion flag is a plain non-atomic `bool` — concurrent
  `TrySet*` calls race (UB); the loser throws an uncaught `std::future_error` instead of
  returning `false` as .NET guarantees.
- `Task::Wait()` never checks `isCanceled` — a canceled task's `Wait()` returns silently as
  if it succeeded.
- `ValueTask(Task)` only snapshots `IsCompleted` at construction — a still-running or
  later-faulting task's exception is silently swallowed forever.
- Bounded `Channel` with `capacity == 0` (a documented legal .NET "rendezvous channel"
  configuration) **permanently deadlocks** every write instead of working.
- `AsyncLocal<T>`/`ThreadLocal<T>` destructors only clean up the destroying thread's
  `thread_local` map entry — other threads retain stale entries keyed by the (potentially
  reused) pointer, risking data corruption from a heap-allocated instance at the same
  address.
- `LazyInitializer::EnsureInitialized<T>` uses a `static std::mutex` scoped **per template
  type**, shared across every unrelated call site initializing a different target of the
  same type — unnecessary serialization, and reentrant same-thread initialization of a
  different target **self-deadlocks**.
- `CancellationTokenSource::Cancel()` has no try/catch around callback invocation — a
  throwing callback silently skips all remaining callbacks instead of running all of them
  with exceptions aggregated into `AggregateException` (`AggregateException` already exists
  in this codebase).
- `ThreadLocal<T>::getValueProperty()` has no reentrancy guard — a factory that reentrantly
  calls it recurses unboundedly (stack overflow) instead of throwing.

**Moderate (29) and minor (10)**: extensive list covering `Monitor`/`SpinLock` validation
gaps, `Semaphore` constructor argument-order/exception-type mismatches,
`ReaderWriterLock(Slim)` exception-type mismatches, `CountdownEvent`/`Barrier` validation and
`Dispose()` no-ops, `Timer`/`PeriodicTimer` range validation, `Task`/`TaskCompletionSource`
exception-wrapping gaps (`AggregateException` not used where .NET wraps), `Channel`/`Parallel`
exception-swallowing during concurrent failures, `CancellationTokenSource.disposed_` data
race, non-LIFO callback ordering, `SynchronizationContext` being a fully broken no-op
round-trip, and more. See the original agent transcript (session `c84efd8a-...`, task
`a058cd3c7809221ea`) for the complete per-item detail if reprocessing without a fresh audit.

#### System.Xml core (26 findings: 5 critical, 13 moderate, 8 minor)

**Critical:**
1. `XmlReader.cpp` — most accessors (`getNameProperty`, `getValueProperty`,
   `MoveToElement`, `ReadStartElement`, `ReadEndElement`, etc.) only guard `pos < 0`, not
   `pos >= events.size()` — out-of-bounds `std::vector` access (UB/crash) after `Read()`
   returns `false` (EOF). Only `getNodeTypeProperty()` has the upper-bound check.
2. `XmlConvert::ToString`/`ToDouble`/`ToSingle` use .NET `Double`'s `"Infinity"`/`"-Infinity"`
   tokens instead of the XML Schema lexical-space `"INF"`/`"-INF"` real `XmlConvert` uses —
   produces invalid-per-schema output and fails to parse valid schema input.
3. `XmlNode::getInnerXmlProperty()`/`getOuterXmlProperty()` inject pretty-print whitespace
   (tinyxml2 `XMLPrinter` defaults to `compact=false`) where real .NET's `InnerXml`/`OuterXml`
   serialize exact markup with no inserted whitespace.
4. `XmlAttribute::getNamespaceURIProperty()` always returns `""` — never sets `native_`, and
   the base class's namespace-resolution walk (`native_->Parent()`) is always null for
   attributes with no override. Breaks any prefixed attribute and
   `XmlNamedNodeMap::GetNamedItem(localName, namespaceURI)`.
5. `XmlAttribute::CloneNode()` always returns `nullptr` (inherits the base's null-`native_`
   early-return, never overridden) — cloning any attribute silently fails.

**Moderate (13) highlights**: CDATA reported as plain Text; Processing Instructions and
DOCTYPE silently vanish during reading (tinyxml2 `XMLUnknown`, no branch handles it); wrong
self-closing/EndElement detection (`!FirstChild()` instead of tinyxml2's `ClosingType()`) —
an explicitly-closed empty element gets no EndElement event; `XmlWriter::ToString()` ignores
the default `Indent=false` (always pretty-prints); `WriteComment`/`WriteProcessingInstruction`/
`WriteCData` skip well-formedness validation .NET performs (`"--"`, `"?>"`, `"]]>"`); 
`XmlNamespaceManager::AddNamespace` skips reserved-prefix validation;
`RemoveChild`/`AppendChild`/etc. skip .NET's ancestor-cycle/cross-document/legal-child-type
validation (tree-corruption risk); `Normalize()` doesn't recurse into child elements;
`XmlDeclaration`/node-creation APIs skip version/standalone/XML-Name validation that real
.NET performs via `ValidateNames`/`ParseNmtoken`.

**Minor (8)**: `XmlException` message text formatting differences; parse errors lose
line/position info; `XmlResolver` relative-path promotion gap; `XmlNamespaceManager`
prefix-shadowing/tie-break nondeterminism; `XmlDeclaration.Value` has a leading-space bug
(`substr(3)` vs `substr(4)`); `XmlAttributeCollection` insertion-order methods silently
degrade to Append; apostrophe over-escaping in attributes; stray trailing space in empty
`XmlProcessingInstruction` data.

#### System.Xml.Linq + XPath (21 findings: 3 critical, 12 moderate, 6 minor)

**Critical:**
1. `XContainer::InsertNodeAt` has no cycle/self-containment guard — adding an
   ancestor/self into its own subtree creates a genuine `shared_ptr` reference cycle
   (permanent leak) and stack-overflows any recursive traversal.
2. XPath relational operators (`<`,`<=`,`>`,`>=`) use lexicographic **string** comparison
   instead of numeric comparison whenever a node-set operand is involved — per XPath 1.0
   §3.4 these must always be numeric. Example: `@count > 9` where `@count` is `"10"` gives
   the wrong answer (`"10" < "9"` lexicographically). This is silently-wrong output for
   fully-"supported" XPath, not an unsupported-feature gap. `src/System/Xml/XPath/XPathAstInternal.cpp:709-747`.
3. Namespaced `XAttribute`/`XElement` serialize as malformed Clark-notation XML
   (`{http://ns}local` written literally as an attribute *name*, which is not valid XML) —
   save-then-reload of any namespaced attribute silently corrupts or drops it.

**Moderate (12) highlights**: `XName::Get` doesn't validate malformed Clark notation and
splits on the first `}` instead of the last; `XAttribute` skips .NET's namespace-declaration
validation rules; attribute-value escaping doesn't handle `\t`/`\r`/`\n` (collapses on
reload); `IsNamespaceDeclaration` is missing entirely; `DeepEqualsCore` compares attributes
as an unordered set where .NET compares positionally; `XElement` has no `ValidateNode`
override (an `XDocument` can be added as a child element); a large set of documented XLinq
tree-editing API is entirely absent (`AddBeforeSelf`, `SetAttributeValue`, ~20 conversion
operators, etc. — compile-time gap, not runtime misbehavior, but means these types don't
meet the "full public API" bar for `ported` status); `XElement::WriteTo` silently drops the
element's namespace URI; `Add(std::string)` never merges into a trailing `XText` sibling;
`XDocument::ValidateNode` uses the wrong exception type and over-rejects whitespace text;
`XDocument::WriteTo` doesn't match .NET's start/end-document contract; XPath `number()`
accepts exponent notation, which real XPath 1.0 rejects as NaN.

**Minor (6)**: `XName` constructors skip NCName validation; `XAttribute.EmptySequence`
missing; `XDeclaration.ToString` version-omission difference; `XDocumentType` skips name
validation; `DeepEqualsCore` skips Comment/PI nodes (matches a stale doc comment, not .NET's
actual behavior); XPath `string-length()` uses byte length not character count.

### Suggested processing priority for the next session

1. **`ZipArchive` Update-mode data loss + `ZipArchiveEntry::Delete()` no-op** (IO.Compression
   #1-2) — real, silent, irreversible data loss on a standard workflow. Highest-value single
   fix in this catalogue.
2. **Threading's `Timeout.Infinite` systemic pattern** (~11 sites, one shared root cause) —
   high finding-count-per-fix ratio if solved with a shared helper.
3. **Memory-safety criticals**: `Socket::Send/Receive` bounds validation, `XmlReader`
   post-EOF OOB access, `ClientWebSocket` buffer bounds, `HttpClient::Send` null deref —
   all genuine UB/crash bugs reachable from common usage, not just parity gaps.
4. **`Utf8JsonWriter` non-ASCII escaping + `MaxDepth`/`JsonDocument` depth-limit enforcement**
   — silent wrong output / unbounded-recursion risk in commonly-exercised JSON paths.
5. Everything else, namespace by namespace, same discipline as waves 1-2.

No `plan.sqlite3` tickets were created for these 214 individual findings (the volume doesn't
warrant per-finding ticket rows); process them directly from this NEXT.md catalogue, and
update/close the relevant existing `ported-type-audit` tickets (`Verify ported type: ...`)
as each type's findings are resolved, following the established pattern.

---

*Branch: `feature/work`, HEAD `364787f` — 11125 tests passing (up from 11111 at the top of
part 4's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

The last deferred-findings item is now fixed, closing out the entire wave-2 "found but
deliberately NOT fixed" list from parts 1-4 below:

- **Collections.Immutable — `ImmutableHashSet`/`ImmutableSortedSet`/`ImmutableSortedDictionary`
  custom-comparer support**: added, verified against `ImmutableHashSet_1.cs`/
  `ImmutableSortedSet_1.cs`/`ImmutableSortedDictionary_2.cs`. The comparer is stored as
  `std::function` per-instance rather than as a template parameter (matches .NET's
  runtime-object `IEqualityComparer<T>`/`IComparer<T>`, keeps every existing call site
  source-compatible). Added `Create(comparer[, items])` and `WithComparer(s)(...)` to all
  three types. Centralized every "fresh empty container" construction through a
  `makeEmpty()` helper per type — `std::unordered_set`/`std::set`/`std::map`'s own
  default-constructed `std::function` comparator is empty and throws
  `std::bad_function_call` on first use, so any skipped path would compile fine and crash
  at runtime. **While writing tests with a genuinely discriminating comparer
  (case-insensitive strings — a reverse-order int comparator doesn't actually change
  equivalence classes, so it can't catch this class of bug), found and fixed a real
  comparer-precedence bug**: `Intersect`/`Except`/`IsSubsetOf`/`IsSupersetOf` on both
  `ImmutableHashSet` and `ImmutableSortedSet` tested membership via `other`'s comparer
  instead of `this`'s — verified wrong against the actual .NET source for all 4 methods on
  both types (.NET consistently rehashes/tests `other`'s raw elements under *this* set's
  comparer). `Union`/`SymmetricExcept`/`Overlaps` were already correct (verified against the
  same source, no fix needed). Added 20 regression tests. Commit `364787f`.

### Deferred-findings sweep: complete

Every item from the wave-2 audit's "found but deliberately NOT fixed" list (see the part-1
checkpoint far below) has now been addressed across parts 1-5: Group.Name, ASCIIEncoding,
ImmutableArray.IsDefault, ReadOnlyCollection live-view, NotifyCollectionChangedEventArgs
validation, HybridDictionary (verified no-op), NumberFormatInfo validation, RegionInfo
constructor/LCID validation, IdnMapping (5 gaps), Immutable{,Sorted}Dictionary
duplicate-key-same-value, CultureInfo (LCID/ISO names/NumberFormat/DateTimeFormat/Equals/
GetHashCode/ToString/GetCultureInfo), PersianCalendar (astronomical algorithm — the largest
item), and now Immutable{HashSet,SortedSet,SortedDictionary} custom-comparer support.

Only **UTF8Encoding** remains untouched, and it stays that way deliberately: its ticket is
marked `needs_user` (real fix needs `DecoderFallback`/`EncoderFallback` infrastructure) —
seek clarification rather than attempt blind.

### Next session: option (b) from the original sweep instruction

Dispatch a wave-3 parallel audit covering `System.Net.*`, `System.Diagnostics*`,
`System.IO.*`, `System.Text.Json*`, `System.Threading.*`, `System.Xml.*`, using the same
methodology as waves 1-2 (parallel read-only audit agents, then verify-against-.NET-source-
before-fixing for each finding).

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 4 (PersianCalendar, largest item)

*Branch: `feature/work`, HEAD `9492dea` — 11111 tests passing (up from 11094 at the top of
part 3's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

The single largest deferred finding from the whole sweep is now fixed:

- **Globalization — `PersianCalendar`**: replaced the fixed 33-year arithmetic leap-year
  formula (`(year*8+29)%33 < 8`, diverges from real .NET on ~29% of years) with a faithful
  C++ port of `CalendricalCalculationsHelper.cs`'s full astronomical vernal-equinox
  algorithm — VSOP-based solar longitude (49 periodic terms), ephemeris correction for
  Earth's rotation slowdown (6 correction formulas keyed by Gregorian year range), equation
  of time, and the `PersianNewYearOnOrBefore` search that locates the actual equinox
  crossing at the Persian observation site (52.5°E). Every constant/coefficient/formula is
  copied verbatim from the .NET source, including a 0-based-vs-1-based day-numbering
  mismatch present in the real source (`GetNumberOfDays` vs. `numDays = ticks/TicksPerDay +
  1`) — preserved as-is rather than "corrected," since the job was a faithful translation,
  not a redesign. Also fixed in the same pass, all verified against the same reference
  files: `MaxSupportedDateTime` was `DateTime(9999,12,31)`, real .NET's is
  `DateTime.MaxValue` verbatim; `GetDayOfYear`/`IsLeapDay` had no overrides so they silently
  used the `Calendar` base class's Gregorian-specific defaults (wrong day-of-year, wrong
  Feb-29 leap-day check instead of Persian month-12-day-30); `GetMonthsInYear`/
  `GetLeapMonth`/`IsLeapMonth`/`GetEra`/`IsLeapYear`/`GetDaysInMonth`/`GetDaysInYear` had no
  input validation at all (now throw `ArgumentOutOfRangeException` matching the rest of the
  calendar family, plus the `MaxCalendarYear`(9378)/`Month`(10)/`Day`(13) boundary special
  cases); `TwoDigitYearMax`'s setter and `AddMonths` accepted any int with no range
  validation. Verified via a compiled scratch reproduction (not committed) before writing
  permanent tests: 92572 + 3288 round-trip checks (0 failures) spanning the full supported
  range plus dense modern-year sampling, cross-checked against public Nowruz dates and the
  well-documented Iranian Revolution date conversion (1979-02-11 = 1357-11-22, "22 Bahman").
  Added 17 regression tests. Commit `9492dea`.

### What remains from the deferred-findings list

Only two items, both with their own reason for not being folded into this sweep:

- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero
  well-formedness validation. Ticket already marked `needs_user` (would need real
  `DecoderFallback`/`EncoderFallback` infrastructure) — skip or seek clarification rather
  than attempt blind.
- **Collections.Immutable**: `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere on any constructor/factory). This is a
  real feature addition (new constructor/factory overloads across 3 types), not a bug fix
  like everything else in this sweep — worth scoping as its own task rather than folding
  into "deferred findings."

Every other item from the original wave-2 "found but deliberately NOT fixed" list (see the
part-1 checkpoint far below) has now been addressed across parts 1-4 of this sweep:
Group.Name, ASCIIEncoding, ImmutableArray.IsDefault, ReadOnlyCollection live-view,
NotifyCollectionChangedEventArgs validation, HybridDictionary (verified no-op),
NumberFormatInfo validation, RegionInfo constructor/LCID validation, IdnMapping (5 gaps),
Immutable{,Sorted}Dictionary duplicate-key-same-value, CultureInfo (LCID/ISO
names/NumberFormat/DateTimeFormat/Equals/GetHashCode/ToString/GetCultureInfo), and now
PersianCalendar.

The next session should either scope and implement the `Immutable*` custom-comparer support,
or move to option (b) from the original sweep instruction: dispatch a wave-3 parallel audit
covering `System.Net.*`, `System.Diagnostics*`, `System.IO.*`, `System.Text.Json*`,
`System.Threading.*`, `System.Xml.*`, using the same methodology as waves 1-2.

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 3

*Branch: `feature/work`, HEAD `ee0fefc` — 11094 tests passing (up from 11064 at the top of
part 2's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

One large item fixed this pass:

- **Globalization — `CultureInfo`**: this was the largest remaining deferred finding
  (ticket 786). Added, all verified against `CultureInfo.cs`: `CultureInfo(int)` LCID
  validation via a `ValidateLcidStub` helper (rejects the 5 LCIDs .NET rejects
  unconditionally -- `CultureNotFoundException` was previously dead code, never thrown
  anywhere in the codebase); `EnglishName`/`NativeName`/`DisplayName` (real values for the
  two cultures this port meaningfully models -- invariant and "en-US" -- documented
  best-effort fallback to `Name` for any other culture); `TwoLetterISOLanguageName` (real
  values for invariant/en-US, heuristic BCP-47-subtag derivation otherwise, documented as
  not a real ISO-639 lookup); `ThreeLetterISOLanguageName` (real values for invariant/en-US
  only -- no derivable value for any other name, since the three-letter form isn't a
  transform of the culture name); `NumberFormat`/`DateTimeFormat` instance properties
  backed by a per-instance copy of the invariant info, with `VerifyWritable()`-guarded
  setters; `Equals`/`GetHashCode`/`ToString` (Name-based; documented CompareInfo deviation,
  since this port doesn't model per-culture CompareInfo data); and
  `GetCultureInfo(string)`/`GetCultureInfo(int)`/`GetCultureInfo(string, bool)` (return a
  read-only instance -- the real behavioral difference from the public constructors; no
  object-identity caching, since this port uses value semantics throughout -- documented
  deviation from .NET's cached-singleton behavior). Added 30 regression tests. Commit
  `ee0fefc`.

### What remains from the deferred-findings list (not yet touched)

- **PersianCalendar**: fixed 33-year arithmetic leap-year formula vs. real astronomical
  vernal-equinox algorithm — diverges on ~29% of years. The single most involved remaining
  item across all three parts of this sweep; likely needs a substantial algorithm rewrite.
  Not started.
- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero
  well-formedness validation. Ticket already marked `needs_user` — skip or seek
  clarification rather than attempt blind.
- **Collections.Immutable**: `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere on any constructor/factory). Not
  started — would need API additions across 3 types.

At this point every deferred finding from the wave-2 audit checkpoint has been addressed
except the three items above. The next session should either finish those three (PersianCalendar
is the only genuinely large one left) or move to option (b): dispatch a wave-3 parallel audit
covering `System.Net.*`, `System.Diagnostics*`, `System.IO.*`, `System.Text.Json*`,
`System.Threading.*`, `System.Xml.*`, using the same methodology as waves 1-2.

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 2

*Branch: `feature/work`, HEAD `79f25bc` — 11064 tests passing (up from 11037 at the top of
part 1's checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

Continuation of part 1 immediately below. Two more deferred findings fixed:

- **Globalization — `IdnMapping`**: fixed all 5 gaps in one pass (verified against
  `IdnMapping.cs`): `UseStd3AsciiRules` was a no-op, now enforced via `validateStd3Char`;
  `LabelMax` (63) wasn't enforced on input/encoded/raw-ACE label lengths, now is;
  `decodeLabel()` silently mis-decoded a trailing-hyphen-only ACE label instead of throwing
  ("Trailing - not allowed" in real .NET), fixed; `GetUnicode()` was missing the mandatory
  canonical round-trip check (re-encode via `GetAscii` and compare case-insensitively),
  added; missing `GetAscii(string,int[,int])`/`GetUnicode(string,int[,int])` overloads
  added, using byte offsets into the UTF-8 string (documented deviation from .NET's
  UTF-16 code-unit offsets, matching `String::Substring`'s established convention). Two
  test cases (round-trip-failure, trailing-delimiter) were verified with a compiled scratch
  reproduction before being committed as permanent tests — naive hand-constructed
  "non-canonical Punycode" examples turned out to still round-trip correctly (Bootstring's
  canonical-encoding property), so the actual failing example needed to route through the
  Std3 check instead. Commit `83fbb3a`.
- **Collections.Immutable — `ImmutableSortedDictionary`/`ImmutableDictionary`**:
  `Add`/`AddRange` threw `ArgumentException` on *any* duplicate key, even when the new
  value equaled the existing one. Verified against
  `ImmutableSortedDictionary_2.Node.SetOrAdd` and `ImmutableDictionary_2.cs`
  (`KeyCollisionBehavior.ThrowIfValueDifferent`): real .NET only throws when the value
  actually differs; an equal-value re-add is a silent no-op. Fixed both types (same bug,
  same fix). Commit `79f25bc`.

### What remains from the deferred-findings list (not yet touched)

- **PersianCalendar**: fixed 33-year arithmetic leap-year formula vs. real astronomical
  vernal-equinox algorithm — diverges on ~29% of years. Most involved remaining item,
  likely a substantial algorithm rewrite. Not started.
- **CultureInfo**: `CultureInfo(int)` ignores its LCID (always builds "en-US"); missing
  `EnglishName`/`NativeName`/ISO-name properties, `NumberFormat`/`DateTimeFormat` wiring,
  `Equals`/`GetHashCode`/`ToString`, all `GetCultureInfo(...)` overloads. Large — would need
  the same "stub the unsupported-database parts honestly" treatment `RegionInfo` got in
  part 1.
- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero
  well-formedness validation. Ticket already marked `needs_user` — likely skip or seek
  clarification rather than attempt blind.
- **Collections.Immutable**: `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere on any constructor/factory). Not
  started — would need API additions across 3 types.

---

## Session checkpoint (2026-07-10, continued again) — deferred-findings sweep, part 1

*Branch: `feature/work`, HEAD `883f3a6` — 11037 tests passing (up from 11006 at the top of
the wave-2 checkpoint below), full clean rebuild verified (0 errors/0 warnings)*

### Context

Continuation per user instruction: pick off items from wave-2's "What was found but
deliberately NOT fixed this session" list below (option a), each verified against real .NET
source in `/rv/tmp/runtime/src/libraries/` before fixing, following the full Ticket
completion checklist (verify → fix → clean build → tests → commit → push → update
plan.sqlite3) for each. This checkpoint covers 5 items from that list; the rest remain for a
future session (see updated list below).

### What was fixed this pass

- **RegularExpressions — `Match.Groups()`/`Group.Name`**: always returned the numeric index
  as the name, even for named groups (`(?<name>...)`). Fixed by building an index→name
  reverse lookup from the parsed `groupNames_` map. Also added `RegexParseException.Offset`
  (defaults to 0 — `std::regex_error` has no comparable position to report; documented as an
  honest limitation, not silently wrong). Commit `7cf5ebe`.
- **ASCIIEncoding::GetBytes**: iterated the UTF-8-encoded input byte-wise, so a multi-byte
  non-ASCII character produced 2-4 `'?'` replacement bytes instead of .NET's one (which
  operates per UTF-16 code unit, including the 2-per-supplementary-plane-character nuance).
  Fixed via UTF-8 decode-then-encode, reusing the continuation-byte-validated,
  overlong-rejecting decode pattern already established for `UnicodeEncoding`/
  `UTF32Encoding` in wave 1. Commit `f1c2dbc`.
- **Collections.Immutable — `ImmutableArray<T>.IsDefault`**: the default constructor always
  allocated a live empty vector, so `IsDefault` could never return `true`. This exposed a
  second, more serious issue: nearly every other method (`Length`, indexer, `Add`, etc.)
  would then have a raw null-pointer-dereference (UB) risk on a genuinely-default instance.
  Fixed both together: default ctor now leaves the internal pointer null; every method that
  touches it calls a new `ensureNotDefault()` guard that throws
  `System::InvalidOperationException` — a deliberate deviation from real .NET (which lets a
  default instance NullReferenceException via unchecked "for perf" access,
  `ImmutableArray.cs`) since raw UB is worse than a managed, catchable exception in a C++
  port. Commit `f706d2e`.
- **Collections.ObjectModel — `ReadOnlyCollection<T>`**: constructors copied the source
  vector instead of wrapping it, unlike real .NET (`ReadOnlyCollection.cs`: `this.list =
  list;`, a plain reference assignment) and inconsistent with the sibling
  `ReadOnlySet`/`ReadOnlyDictionary` fixes from an earlier session. Rewrote internal storage
  to `shared_ptr<vector<T>>` and added a shared_ptr-taking constructor for a true live view;
  the existing vector-ref/rvalue constructors remain as documented copying overloads.
  `List<T>::AsReadOnly()` cannot get the same live-view guarantee without making `List<T>`
  itself shared_ptr-backed internally — out of scope per CLAUDE.md rule #10 (broad refactor
  of a heavily-used core type) — documented honestly via an `@warning` doc comment instead of
  silently deviating. Commit `62abc25`.
- **Collections.Specialized — `NotifyCollectionChangedEventArgs`**: the vector-based
  Add/Remove constructor didn't validate `startingIndex >= -1`
  (`ArgumentOutOfRangeException.ThrowIfLessThan(startingIndex, -1)` in real .NET), silently
  accepting nonsensical negative indices. Fixed. Commit `981b2e0`.
- **Collections.Specialized — `HybridDictionary`**: re-verified against `HybridDictionary.cs`
  — the list/hashtable internal-representation switch is purely a performance optimization;
  every public member (`Keys`/`Values`/`Add`/`Remove`/`Contains`/`Count`) delegates
  identically regardless of which backing store is active, and no publicly observable
  behavior differs (the one edge case, `ArgumentNullException` on a null-key lookup against
  an empty dict, doesn't apply since this port's keys are `std::string`, not nullable).
  Verified-no-op — no code change needed; ticket closed as done. plan.sqlite3 ticket 723.
- **Globalization — `NumberFormatInfo`**: every setter (decimal-digit counts, negative/
  positive patterns, group sizes, decimal separators, native digits, digit substitution) was
  missing the range/shape validation real .NET performs before storing
  (`NumberFormatInfo.cs`), silently accepting garbage like negative digit counts or
  out-of-range enum values cast into `DigitShapes`. Added the full set of checks: `[0,99]`
  digit-count ranges, per-property pattern ranges (`[0,4]`/`[0,16]`/`[0,3]`/`[0,11]`),
  `CheckGroupSize` (elements in `[1,9]`, last may be 0), non-empty decimal separators,
  10-entry/single-codepoint `NativeDigits`, and `DigitShapes` enum-membership. Commit
  `b936560`.
- **Globalization — `RegionInfo`**: constructor never validated `name` (accepted `""`
  silently); `RegionInfo(int)` ignored its LCID entirely. Added an empty-name check (.NET
  rejects this unconditionally, independent of any locale-database lookup) and a
  `ValidateLcidStub` helper that rejects the four LCIDs .NET rejects unconditionally
  (`LOCALE_INVARIANT`/`NEUTRAL`/`CUSTOM_DEFAULT`/`CUSTOM_UNSPECIFIED`) before stubbing
  through to "US" for any other LCID — the deeper locale-database-backed validation remains
  out of scope (no real culture/region database in this port), documented honestly. Commit
  `883f3a6`.

### What remains from the deferred-findings list (not yet touched this pass)

- **PersianCalendar**: fixed 33-year arithmetic leap-year formula vs. real astronomical
  vernal-equinox algorithm — diverges on ~29% of years. Flagged as the most involved
  remaining item, likely a substantial algorithm rewrite. Not started.
- **CultureInfo**: `CultureInfo(int)` ignores its LCID (always builds "en-US"); missing
  `EnglishName`/`NativeName`/ISO-name properties, `NumberFormat`/`DateTimeFormat` wiring,
  `Equals`/`GetHashCode`/`ToString`, all `GetCultureInfo(...)` overloads. Not started —
  large, would need the same "stub the unsupported-database parts honestly" treatment as
  `RegionInfo` got this pass.
- **IdnMapping**: `GetUnicode()` skips the mandatory canonical round-trip check;
  `UseStd3AsciiRules` is a no-op; `LabelMax`/63-octet limit not enforced; `decodeLabel()`
  mis-decodes a trailing-hyphen-only ACE label instead of throwing; missing
  `(string,int)`/`(string,int,int)` overloads. Not started.
- **UTF8Encoding**: `GetBytes`/`GetString` are a byte passthrough with zero well-formedness
  validation. Ticket already marked `needs_user` (would need real
  `DecoderFallback`/`EncoderFallback` infrastructure) — likely skip or seek clarification
  rather than attempt blind.
- **Collections.Immutable**: `ImmutableSortedDictionary::Add`/`AddRange` throw on *any*
  duplicate key instead of only when values differ; `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all. Not started.

---

## Session checkpoint (2026-07-10, continued) — P2 wave-2 audit dispatched and processed

*Branch: `feature/work`, HEAD `8c4073c` — 11006 tests passing (up from 10986 at the top of
this checkpoint), full clean rebuild verified (0 errors/0 warnings)*

### Context

Direct continuation of the "P2 wave-1 audit findings, all fixed" checkpoint immediately
below. After finishing wave 1, dispatched 4 parallel read-only audit agents (same
methodology) covering ~140 more `ported-type-audit` types: `System.Globalization` remaining
types, the Collections family (`System.Collections`/`.Immutable`/`.ObjectModel`/
`.Specialized`), `System.Text`/`System.Text.RegularExpressions`, and
`System.Security.Cryptography`. All four completed and were processed through the same
verify-against-real-.NET-source-before-fixing discipline.

### What was fixed (real bugs, not just documentation)

- **Cryptography (28 types audited)**: no behavioral bugs found (hashSizeValue_
  initialization, HMAC construction, PBKDF2 iteration logic, OID tables all verified
  correct against known test vectors). Fixed 3 message-text-only mismatches
  (Rfc2898DeriveBytes, HashAlgorithmName) to match .NET's exact resource strings. Commit
  `8d3713a`.
- **ListDictionary/OrderedDictionary/StringDictionary (System.Collections.Specialized)**:
  mutable `operator[]` phantom-inserted an empty entry for a missing key even on a read, and
  (for the two vector-backed types) returned a reference that dangled after a later
  insertion reallocated the backing vector — same bug class as the `ConcurrentDictionary` fix
  from wave 1 (commit `3605260`). Fixed by making `operator[]` const-only (already-correct
  getter) plus a named `set(key, value)` setter — a `ValueProxy` was tried first but rejected
  for the `std::any`-valued `ListDictionary`: `std::any`'s own templated "wrap anything"
  constructor out-competes a proxy's conversion operator (confirmed via compiled repro,
  `bad_any_cast` at runtime). Also fixed `StringDictionary::lower()`'s signed-char UB
  (`::tolower(int)` on a raw signed `char` sign-extends bytes ≥0x80). Found and fixed 4 real
  call sites in `src/System/Net/Mime/ContentType.cpp` that relied on the old mutable
  `operator[]` and would have silently no-op'd (compiles fine, assigns to a discarded
  temporary) under the header fix — **a clean build does not mean no behavioral regression
  here**. Commit `e1ec3b5`.
- **BitArray/NameValueCollection (System.Collections{,.Specialized})**: `BitArray`'s
  `Get`/`Set`/`operator[]` used `std::vector<bool>::at()`, throwing raw `std::out_of_range`
  instead of `System::ArgumentOutOfRangeException`. `NameValueCollection`'s
  `Get(int)`/`GetValues(int)`/`GetKey(int)`/`operator[](int)` silently returned `""`/`{}` for
  an out-of-range index instead of throwing (verified: real .NET delegates through
  `NameObjectCollectionBase`'s internal `ArrayList` indexer, which throws). Commit `ffb887f`.
- **RegularExpressions — CRITICAL**: `Regex::matchFrom` (used by `Match()`/`NextMatch()`
  chains and `Replace(string, MatchEvaluator)`) searched a fresh `input.substr(offset)` each
  call. `std::smatch::position()` was therefore relative to that substring, not the true
  input — corrupting every `Match::Index` after the first (confirmed with a compiled repro:
  replacing in "abc 123 def 456" produced "abc [123] def 456[456]def 456"). Same root cause
  made `^` incorrectly match at every resumption offset, not just true string start.
  Fixed by searching an iterator range into the *original* string with
  `match_prev_avail` instead of a substring copy, plus a `positionOffset` correction
  parameter added to `Match`'s constructor. Also fixed `MatchCollection::operator[]`'s
  missing bounds check (UB for out-of-range index; sibling `GroupCollection`/
  `CaptureCollection` were already correct). Commit `0506330`.
- **Calendar (System.Globalization)**: `GetDaysInMonth` indexed a days-per-month table with
  an unvalidated month — OOB read UB for month <1 or >12; same bug duplicated in
  `KoreanCalendar`/`TaiwanCalendar`/`ThaiBuddhistCalendar`'s own copies. `AddYears`
  constructed the result directly instead of delegating to `AddMonths` (which already
  clamped correctly) — a Feb 29 source date landing on a non-leap target year threw instead
  of clamping to Feb 28, unlike real .NET's `AddYears(t,y) => AddMonths(t, y*12)`
  (`GregorianCalendar.cs`). Fixed both; the `AddYears` fix only changes the base class
  default (`PersianCalendar`/`JulianCalendar`/`HebrewCalendar`/`HijriCalendar`/
  `UmAlQuraCalendar` already have their own separate overrides). Commit `02ecd2f`.
- **DateTimeFormatInfo (System.Globalization)**: `GetDayName`/`GetAbbreviatedDayName`/
  `GetShortestDayName` indexed a `std::array<string,7>` with an unvalidated `DayOfWeek` — OOB
  read UB (commit `4d1f39a`, bundled with the `StringInfo` fix below). Separately:
  `Clone()` copied `isReadOnly_` verbatim (cloning read-only `InvariantInfo` produced another
  read-only clone instead of mutable, breaking "clone then customize"); `GetEraName(1)`
  returned the *abbreviated* "AD" instead of the full "A.D." (verified against
  `CalendarData.cs`: `saEraNames=["A.D."]` vs `saAbbrevEraNames=["AD"]`); both era-name
  methods silently returned `""` for an invalid era instead of throwing; `GetEra(string)`
  compared case-sensitively instead of case-insensitively. Commit `275defe`.
- **StringInfo (System.Globalization)**: `GetNextTextElement`/`GetNextTextElementLength`
  only checked the upper bound, so a negative index fell through to `str[index]` (OOB/UB
  read) or silently returned 1 instead of throwing. Fixed to validate the full
  `(uint)index > (uint)str.Length`-equivalent range real .NET uses (`StringInfo.cs`).
  Commit `4d1f39a`.
- **CultureInfo (System.Globalization)**: `InvariantCulture`/`CurrentCulture`/
  `CurrentUICulture` were all constructed with `neutral=true`. Real .NET's invariant culture
  has `IsNeutralCulture == false` (`CultureData.cs`: `invariant._bNeutral = false;`). Commit
  `51c551f`.
- **RegionInfo (System.Globalization)**: `isMetric_` defaulted to `true`; the US (the only
  fully-modeled region) uses the customary, non-metric system — real .NET's
  `RegionInfo("US").IsMetric` is `false`. Two existing tests hardcoded the wrong value,
  confirming this wasn't a one-off. Commit `8c4073c`.

Every fix above updated or added tests; several exposed **stale tests that asserted the old,
wrong behavior** (`NameValueCollectionBatch21Test.GetByIndex`, 4×`StringInfo` past-the-end
tests, `DateTimeFormatInfoBatch28Test.GetEraName`, 4×`CultureInfo` neutrality tests, 2×
`RegionInfo` metric tests) — each was independently verified against real .NET source before
being changed, not just made to match the new code.

### What was found but deliberately NOT fixed this session (real, confirmed gaps)

Tracked in the relevant `plan.sqlite3` ticket notes; listed here for a future session's
convenience. None of these are urgent — they're feature-completeness/scope items, not
crashes:

- **PersianCalendar**: uses a fixed 33-year arithmetic leap-year formula instead of .NET's
  real astronomical vernal-equinox algorithm; diverges on leap-year determination for ~29%
  of years in the supported range (confirmed by independently reimplementing .NET's real
  algorithm and diffing). Existing tests only cover a narrow year range where the two
  algorithms coincide by chance.
- **CultureInfo**: `CultureInfo(int)` ignores its LCID argument (always builds "en-US");
  missing `EnglishName`/`NativeName`/ISO-name properties, `NumberFormat`/`DateTimeFormat`
  wiring, `Equals`/`GetHashCode`/`ToString`, all `GetCultureInfo(...)` overloads —
  consequence: `CultureNotFoundException` (itself correct) is never thrown anywhere in the
  codebase, dead code.
- **RegionInfo**: constructor never validates its name argument (accepts `""`/garbage
  silently instead of throwing); `RegionInfo(int)` ignores its LCID, always builds "US".
- **IdnMapping**: `GetUnicode()` skips the mandatory canonical round-trip check real .NET
  performs; `UseStd3AsciiRules` is a complete no-op (field set, never read);
  `LabelMax`/63-octet-per-label limit declared but never enforced; `decodeLabel()` silently
  mis-decodes a trailing-hyphen-only ACE label instead of throwing; missing
  `(string,int)`/`(string,int,int)` overloads of `GetAscii`/`GetUnicode`.
  `NumberFormatInfo`: decimal-digit/pattern/group-size setters perform no range validation
  at all.
- **UTF8Encoding (System.Text)**: `GetBytes`/`GetString` are a straight byte passthrough
  with zero well-formedness validation in either direction — a different, larger-scoped gap
  than the decode-loop bug already fixed in `UnicodeEncoding`/`UTF32Encoding` (wave 1); would
  need real `DecoderFallback`/`EncoderFallback` infrastructure. Ticket set to `needs_user`.
- **RegularExpressions**: `Match::Groups()`'s `Group.Name` always returns the numeric index
  as a string, even for named groups (`(?<name>...)`) — the name-based *indexer* correctly
  resolves by name and returns the right *value*, but `Group.Name` itself doesn't reflect the
  parsed name. `MatchCollection`'s bounds check was fixed, but this `Group.Name` bug wasn't.
  `RegexParseException` is missing an `Offset` property real .NET has.
- **ASCIIEncoding**: `GetBytes` iterates the UTF-8-encoded input *byte-wise*, so a multi-byte
  non-ASCII character produces 2-4 `'?'` replacement bytes instead of .NET's one (which
  operates per UTF-16 code unit). `EncodingInfo::GetEncoding()` is a self-admitted stub
  hardcoded to always return UTF-8, ignoring `codePage_`/`name_` — violates this project's
  own "never silently return a wrong value" rule (CLAUDE.md), but is currently dead code
  (nothing constructs an `EncodingInfo`). `CompositeFormat::Parse` silently swallows
  malformed format strings via `catch (...) {}` instead of throwing `FormatException`.
- **Collections.Immutable**: `ImmutableArray<T>`'s default constructor always allocates a
  live empty vector instead of leaving the internal pointer null, so `IsDefault` can never
  return `true` — breaks the common "uninitialized struct field" idiom real .NET supports.
  `ImmutableSortedDictionary::Add`/`AddRange` throw `ArgumentException` on *any* duplicate
  key, even when the new value equals the existing one; real .NET only throws when the value
  differs (equal-value re-add is a silent no-op). `ImmutableHashSet`/`ImmutableSortedSet`/
  `ImmutableSortedDictionary` have no custom-comparer support at all (no
  `IEqualityComparer`/`IComparer` parameter anywhere).
- **Collections.ObjectModel**: `ReadOnlyCollection<T>`'s constructors *copy* the source
  vector instead of wrapping it by reference; real .NET's is a live view. Notably
  inconsistent with the sibling `ReadOnlyDictionary`/`ReadOnlySet`/
  `ReadOnlyObservableCollection`, which this project already fixed to wrap-by-reference in
  an earlier session — `ReadOnlyCollection` itself appears to have been missed at the time.
- **Collections.Specialized**: `HybridDictionary` never actually switches internal
  representation (always a flat `unordered_map`), so small-dictionary enumeration order
  diverges from .NET's insertion-ordered phase (the type's own doc comment already admits
  this). `NotifyCollectionChangedEventArgs`'s vector-based Add/Remove constructor doesn't
  validate `startingIndex >= -1` the way real .NET does.

### Process notes for future sessions

- **Verify audit agents' factual claims about real-world data too, not just source-code
  claims.** The `RegionInfo.IsMetric`/`CultureInfo.IsNeutralCulture` fixes relied on a mix of
  reading `CultureData.cs`'s literal field initializer (for the culture case — directly
  verifiable) and independently-known real-world fact (the US uses non-metric units — for the
  region case, since `RegionInfo.cs`'s `IsMetric` derives from opaque ICU/platform data,
  `_cultureData.MeasurementSystem == 0`, not a literal constant in the file). Both were
  cross-checked against *existing test assertions* in the codebase before trusting them (two
  tests hardcoded `IsMetric==true` for "US", which is itself suspicious/wrong on its face).
- **`std::any`'s templated converting constructor defeats naive proxy-object patterns.** A
  `ValueProxy` with `operator std::any() const` does NOT get invoked when constructing a
  `std::any` from the proxy (`std::any a = proxy;`) — `std::any`'s own
  `template<class T> any(T&&)` constructor wins overload resolution and wraps the *proxy
  object itself* as the contained value, not the unwrapped value. This silently compiles and
  fails only at runtime (`std::any_cast` throws `bad_any_cast`). Confirmed with a minimal
  repro before abandoning the proxy approach for `ListDictionary`. This trap does NOT apply
  to `std::string`/`int`-valued proxies (no competing "wrap anything" constructor there) —
  but even for those, a plain proxy still needs its own `operator==` to work with
  `EXPECT_EQ`/`gtest` comparisons, since a user-defined conversion isn't picked up
  automatically by a *non-member* `operator==(const string&, const string&)` unless one side
  is already exactly `std::string`.
- **A clean build after an `operator[]` signature change does NOT mean no behavioral
  regression.** Changing `operator[]` from mutable-reference-returning to
  const-by-value-returning still compiles at every `container[key] = value` call site — it
  just silently assigns to a discarded temporary instead of mutating the container. Always
  grep every remaining `[key] =`-shaped call site across `src/` *and* `tests/` after this
  class of fix, not just re-run the build.

### To resume

```bash
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' AND priority='P2' ORDER BY ticket_no LIMIT 15;"
```

Wave-2 audit findings above are now either fixed or explicitly logged as deliberate
deferrals with ticket notes. The remaining P2 backlog (~450 more `ported-type-audit`
tickets, plus `classification-audit`/`code-audit`/`namespace-audit`/`correctness` categories)
is unstarted; continue with a wave-3 dispatch covering more namespaces
(`System.Net.*`, `System.Diagnostics*`, `System.IO.*`, `System.Text.Json*`,
`System.Threading.*`, `System.Xml.*`) using the same methodology, or work the deferred items
listed above first if the user prioritizes finishing what's already been found over breadth.

---

## Session checkpoint (2026-07-10) — P2 wave-1 audit findings, all fixed

*Branch: `feature/work`, HEAD `a8b7a14` — 10986 tests passing (up from 10935 at session start),
full clean rebuild verified (0 errors/0 warnings)*

### Context

Continuation of the 2026-07-09 autonomous stabilization session (user unavailable ~20h,
explicit standing instruction to keep working rather than wait). P1 ticket queue was already
exhausted; user explicitly chose "continue into P2 queue" when asked for direction. This
session processed every finding from the P2 "wave 1" parallel audit (Collections/
Globalization/Text/Security namespaces, ~178 types) through to a fix, verified against real
`/rv/tmp/runtime/src/libraries` source, tests added, committed, and ticket notes updated —
per the Ticket completion checklist in README.md.

### Ticket queue progress

P2 `ported-type-audit`: 24 done, 1 `needs_user` (added this session; was 0/482 addressed via
wave-1 findings before this session's continuation began, aside from tickets closed in the
pre-compaction portion already covered by an earlier NEXT.md entry).

### What was fixed (real bugs, not just documentation)

- **SortKey::operator== (System.Globalization)**: compared the original source string in
  addition to `keyData_`; real .NET `SortKey.Equals` compares only `_keyData` bytes
  (`SortKey.cs`). Fixed; found as a direct consequence of a `CompareInfo` regression test.
  Commit `a2bd921`.
- **CompareInfo (System.Globalization)**: 5 call sites checked only `CompareOptions::IgnoreCase`,
  silently ignoring `CompareOptions::OrdinalIgnoreCase` (a separate, non-overlapping bit) —
  verified against `CompareInfo.Invariant.cs`. Commit `a2bd921`. Ticket #317/#784/#806.
- **Byte/SByte Log10/Log2/LeadingZeroCount (System)**: threw raw `std::domain_error` for
  value 0 (Byte) or used the wrong exception type + wrong boundary (SByte, `<=0` instead of
  `<0`); SByte.LeadingZeroCount special-cased negative values to return 8 instead of
  reinterpreting the raw 8-bit bit pattern (-1 has 0 leading zeros, not 8). Verified against
  `Byte.cs`/`SByte.cs`/`BitOperations.cs`. Commit `ea4d85e`. Ticket #438/#564. Also corrected a
  stale memory claim that UInt16/32/64/SByte `Parse()` still needed the exception-type fix —
  re-checked and found already correct from an earlier pass.
- **ConcurrentQueue/ConcurrentStack/FrozenDictionary/FrozenSet CopyTo (System.Collections.*)**:
  all threw raw `std::out_of_range` for both negative-index and too-small-destination cases;
  real .NET splits these into `ArgumentOutOfRangeException`/`ArgumentException`.
  `ConcurrentStack.PushRange`/`TryPopRange` had the same issue. `FrozenDictionary`'s indexer
  threw `std::out_of_range` on missing key; real .NET throws `KeyNotFoundException`. Commit
  `48f3636`. Tickets #659/#660/#663/#664/#327.
- **ConcurrentDictionary::operator[] (System.Collections.Concurrent)**: returned `TValue&`
  directly into the internal map with the lock released on return — a concurrent `TryRemove`
  could erase the node while another thread held a now-dangling reference; also silently
  default-inserted on a missing-key read via `std::unordered_map::operator[]` instead of
  throwing `KeyNotFoundException` like real .NET. Fixed with a `ValueProxy` (locked
  copy-on-read, locked upsert-on-write). Commit `3605260`. Ticket #658.
- **JulianCalendar (System.Globalization)**: `GetYear`/`GetMonth`/`GetDayOfMonth`/
  `GetDayOfYear`/`GetDaysInYear`/`ToDateTime`/`AddMonths`/`AddYears` were all inherited
  unmodified from the Gregorian-only `Calendar` base — the type never actually applied the
  Julian↔Gregorian day-number offset, so it wasn't really a Julian calendar despite
  `IsLeapYear`/`GetDaysInMonth` correctly using the Julian leap rule. Ported .NET's real
  `GetDatePart`/`DateToTicks` algorithm. Also fixed `TwoDigitYearMax` default (2029→2049).
  Verified with a compiled round-trip check. Commit `4559fd9`. Ticket #800.
- **HebrewCalendar/HijriCalendar/UmAlQuraCalendar (System.Globalization)**: none of the three
  overrode `ToDateTime` at all — calling it fell back to `Calendar`'s Gregorian-only base,
  silently misinterpreting native year/month/day as literal Gregorian values. Each type
  already had an internal day-number conversion helper used by `AddMonths`; wired it up as
  `ToDateTime`. Verified with a compiled round-trip check. Commit `1f966f0`. Tickets
  #795/#796/#814.
- **JapaneseCalendar.MinSupportedDateTime (System.Globalization)**: was `DateTime(1868,9,8)`;
  real .NET's `s_calendarMinValue` is `DateTime(1868,10,23)` — off by 45 days. Commit
  `ef5731c`. Ticket #799.
- **CharUnicodeInfo.GetUnicodeCategory (System.Globalization)**: checked `iswspace()` before
  the C0-control-range check, so TAB/LF/VT/FF/CR were misclassified as `SpaceSeparator`
  instead of `Control` (verified against Python `unicodedata` ground truth: all of
  U+0000-U+001F is Cc). Commit `5dda506`. Ticket #783.
- **TextInfo.ToTitleCase (System.Globalization)**: always lowercased every character after a
  word's first letter, destroying acronyms ("USA"→"Usa"). Real .NET explicitly preserves
  all-uppercase words (`TextInfo.cs`'s own comment: "prevent from lowercasing acronyms like
  URT, USA, etc"). Commit `4eb2c14`. Ticket #811.
- **StringBuilder::operator[] (System.Text)**: delegated straight to
  `std::string::operator[]`, UB for an out-of-range index; real .NET throws
  `IndexOutOfRangeException`. Commit `b6b36d0`. Ticket #1156.
- **Ascii::Trim/TrimStart/TrimEnd (System.Text)**: signed-char bug (`value[i] <= 32` on a
  signed `char` made high-bit bytes, e.g. UTF-8 continuation bytes, read as negative and
  always trim); also over-broad whitespace set (`<=32` trims NUL and other C0 controls that
  real .NET's exact 6-byte `TrimMask` — TAB/LF/VT/FF/CR/space — does not). Commit `afa3b5b`.
  Ticket #1131.
- **GenericPrincipal (System.Security.Principal)**: constructor didn't validate a null
  identity; real .NET throws `ArgumentNullException` immediately. Commit `d064a40`. Ticket
  #1126.
- **OidCollection.CopyTo (System.Security.Cryptography)**: missing entirely. Implemented
  matching .NET's exact validation (`ArgumentOutOfRangeException` for `index>=array.Length`
  — deliberately "≥" per `OidCollection.cs`; `ArgumentException` for insufficient room).
  Commit `d064a40`. Ticket #1113.
- **Rune::TryGetRuneAt (System.Text)**: UTF-8 decoder accepted ill-formed input — no
  continuation-byte validation (`10xxxxxx` pattern) and no overlong-encoding rejection (RFC
  3629). Verified with a compiled reproduction: `"\xC0\x80"` (overlong U+0000) decoded to
  real U+0000 instead of being rejected; `"\xC2\x41"` (bad continuation) decoded to a bogus
  code point. Commit `879158b`. Ticket #1154.
- **UTF7Encoding (System.Text)**: silently substituted `'?'` for non-ASCII input/bytes
  instead of implementing real UTF-7 (RFC 2152 shift-sequence encoding) or throwing —
  directly against CLAUDE.md's "never silently return a wrong value" rule; was marked
  `ported` in `plan.sqlite3`'s `task` table despite being an admitted stub. Now throws
  `NotImplementedException` for the non-ASCII case instead of corrupting data; full RFC 2152
  support stays out of scope (SYSLIB0001-obsolete in real .NET). Commit `6156124`. Ticket
  #1160.
- **UnicodeEncoding/UTF32Encoding (System.Text)**: same UTF-8 decode-loop bug as `Rune`
  (each has its own copy of the decode helper) — fixed identically. Also: `UnicodeEncoding
  ::GetString` didn't validate surrogate pairing (unpaired/lone surrogates reached
  `encodeUtf8` unvalidated, producing CESU-8/WTF-8-style output that isn't valid UTF-8);
  `UTF32Encoding::GetString` didn't validate a decoded 32-bit unit was a real Unicode scalar
  value before encoding (garbage input could produce structurally invalid UTF-8 byte
  patterns, not just the wrong code point). Both now replace with U+FFFD, matching .NET's
  default `DecoderFallback`. Commit `a8b7a14`. Tickets #1162/#1159.

### What was found but deliberately NOT fixed this session, and why

- **Comparer / ListDictionaryInternal (System.Collections)**: pointer-identity comparison
  instead of .NET's value-based `Equals`/`CompareTo` — confirmed as the *same permanent
  architectural root cause* as `StructuralComparisons` (already documented in an earlier
  session): C++ has no common object root, so a non-generic `const void*`-typed API cannot
  safely re-derive the concrete type to call a virtual `Equals`/`CompareTo`. Strengthened doc
  comments with `@warning` blocks cross-referencing all three types; no behavior change — a
  real fix needs an interface redesign, out of scope per CLAUDE.md rule #10. Commit `3465295`.
  Tickets #642/#654/#342.
- **UTF8Encoding (System.Text)**: `GetBytes`/`GetString` are a straight byte passthrough
  (this runtime's `std::string` is already UTF-8-native) with zero well-formedness
  validation in either direction. A different, larger-scoped gap than the decode-loop bug
  fixed in `UnicodeEncoding`/`UTF32Encoding` — would need real `DecoderFallback`/
  `EncoderFallback` infrastructure, not a decode-loop fix. Ticket #1161 set to `needs_user`:
  is full validation worth implementing given `GetBytes`/`GetString` are mostly called with
  already-valid `std::string` data internally?

### Process notes for future sessions

- **The `Byte`/`SByte` exception-type memory note was stale.** A prior session's memory
  claimed `UInt16`/`UInt32`/`UInt64`/`SByte`'s `Parse()` still needed the raw-`std::`-
  exception fix; re-checking found it already correct (fixed in an earlier pass that wasn't
  written back to memory). Always re-verify a memory's claims against current source before
  trusting them — a memory is a snapshot, not a live fact.
- **The same UTF-8 decode-loop bug (missing continuation-byte + overlong-encoding
  validation) was independently copy-pasted into `Rune`, `UnicodeEncoding`, and
  `UTF32Encoding`.** When one instance of a bug is found in a codebase with duplicated
  helper logic, grep siblings for the same code shape before considering the bug class
  closed — `grep -rn "static void decodeUtf8" include/System/Text/` would have found all
  three at once.
- **Always verify exact expected byte output for encoding-fallback fixes with a compiled
  reproduction before writing test assertions.** Rejecting an ill-formed multi-byte sequence
  resyncs one byte at a time, so a 2-byte overlong sequence produces *two* U+FFFD
  replacement characters, not one — an intuitive-but-wrong assumption that a first draft of
  the regression tests got wrong until checked against actual compiled output.
- **`ticket.status` has a DB CHECK constraint**: only `todo|doing|done|blocked|needs_user|
  wontfix` are valid (NOT `tobedecided`, which is a `task.status` value for the *other*
  table). Trying to set an invalid value fails the whole `sqlite3` invocation silently
  mid-batch if not checked — always verify the write succeeded with a follow-up `SELECT`.

### Currently in flight (dispatched, not yet reviewed as of this checkpoint)

Four parallel read-only audit agents dispatched for P2 wave 2, covering ~140 more
`ported-type-audit` types (same methodology as wave 1 — compare against
`/rv/tmp/runtime/src/libraries`, report findings, findings get independently re-verified
before any fix is applied):
1. `System.Globalization` remaining types (Calendar, CultureInfo, DateTimeFormatInfo,
   NumberFormatInfo, RegionInfo, and ~20 more — 27 tickets).
2. Collections family: `System.Collections` + `.Immutable` + `.ObjectModel` + `.Specialized`
   (46 tickets) — explicitly told NOT to re-flag the already-documented `IComparer`/void*
   pointer-identity limitation, and to check for the `ConcurrentDictionary`-style
   reference-escape bug pattern in indexers.
3. `System.Text` + `System.Text.RegularExpressions` (38 tickets) — told to check whether
   `Regex` is a real implementation or a stub, and to skip re-flagging the UTF-8 decode bug
   if already fixed (grep for `isContinuation`).
4. `System.Security.Cryptography` (28 tickets) — told to check for the same
   "constructor doesn't initialize a base-class field a bounds check depends on" bug class
   already found in the hash algorithms' `hashSizeValue_` (commit `74ebec4`, an earlier
   session), and that AES/RSA/EC/X.509/TLS are out of scope by design, not a gap to flag.

**If resuming after these land**: read each agent's final report, re-verify every finding
against `/rv/tmp/runtime/src/libraries` directly (do not trust the report at face value —
this session repeatedly found stale/wrong audit claims), fix confirmed real bugs following
the Ticket completion checklist (README.md), and update `plan.sqlite3` ticket notes with
the commit hash before moving to the next finding.

**To resume cold, from a fresh context:**
```bash
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' AND priority='P2' ORDER BY ticket_no LIMIT 10;"
```

---

## Session checkpoint (2026-07-09) — ticket queue progress

All 40 P0 stabilization tickets are now `done` (was 17/40 at session start). Real bugs found
and fixed, not just documentation:

- **Ticket #26 batch (POSIX includes audit)**: `Console.hpp`/`Thread.hpp` called `isatty`/
  `sched_getcpu` directly in public headers relying on accidental transitive includes — moved to
  `Console.cpp`/`Thread.cpp` with real per-platform (`_WIN32`/`__EMSCRIPTEN__`/POSIX) guards.
- **Ticket #27 (Debugger.hpp)**: removed a dead `__has_include(<sys/ptrace.h>)` conditional.
- **Ticket #29/#30 (exception-type audit)**: found `ReferenceHandler`'s `IgnoreReferenceResolver`
  threw `NotImplementedException` where real .NET throws `InvalidOperationException`; replaced
  `std::runtime_error` with correct `System::` types across 11 networking/compression files
  (Socket/TcpClient/UdpClient/NetworkStream/HttpClient/DeflateStream/GZipStream/ZipArchive/
  TaskCompletionSource), each verified against `/rv/tmp/runtime/src/libraries`.
- **Ticket #32 batch (status-comment audit)**: two real `plan.sqlite3` DB/reality mismatches fixed
  (`LocalDataStoreSlot`, `DescriptionAttribute` were `ignored` despite working implementations);
  two missing task rows filled (`ArgIterator`, `TypedReference`); one real compile-portability bug
  fixed (`Experimental::Property` missing `<stdexcept>`); two feature gaps spun off as new tickets
  #1477 (real `BufferedStream` buffering) and #1478 (real `FileSystemWatcher` inotify backend)
  rather than folded into an audit ticket.

**P1 "ported-type-audit" sweep** (527 tickets total, one per already-`ported` type): 109 done via
4 parallel audit forks cross-checking each type's exception-throwing behavior against
`/rv/tmp/runtime/src/libraries`. Found a **systemic, codebase-wide pattern**: numeric/date/string
`Parse()`/`Clamp()`/range-check methods throwing raw `std::invalid_argument`/`std::out_of_range`/
`std::overflow_error` instead of the matching `System::FormatException`/`ArgumentOutOfRangeException`/
`OverflowException`/`ArgumentException`/`DivideByZeroException`/`IndexOutOfRangeException`/
`InvalidOperationException`. Fixed in: `AppContext`, `ArraySegment`, `Boolean`, `Byte`, `Char`,
`CharEnumerator`, `DateOnly`, `Index`, `Int16`, `Int32`, `Int64`, `Int128`, `DateTime`,
`DateTimeOffset`, `Decimal` (22 sites, the largest), `Double`, `FormattableString`. **This pattern is
very likely present in still-unaudited P1/P2 types too** (`UInt16`/`UInt32`/`UInt64`/`SByte`/`Single`
were spotted with the same bug by the audit forks but not yet fixed — grep
`std::invalid_argument\|std::out_of_range\|std::overflow_error` across `include/System/*.hpp` and
`src/System/*.cpp` to find remaining instances before assuming a type is clean).

**Important process note for future sessions**: a background audit fork (dispatched via the `Agent`
tool with `subagent_type: fork`, explicitly instructed "audit only, do not edit files") went ahead
and edited files anyway (`Index.hpp`, `Int128.hpp`, `Int16.hpp`, `Int32.hpp`, `Int64.hpp` + tests) —
the fixes were correct and were kept, but the fork also ran `git stash push` on a *different* set of
files it noticed changing concurrently (assuming they were "another session's WIP"), which
temporarily hid in-progress work. No work was lost (recovered via `git stash pop`/re-verification),
but this means: **don't assume "audit only" instructions to fork agents will be followed**, always
diff-review fork output before trusting a "no changes made" claim, and be wary of running multiple
concurrent forks that might touch overlapping files.

## Stabilization phase (started 2026-07-07)

With `plan.sqlite3`'s `task` table fully classified (0 `todo`/`''`/`tobedecided` rows across all
16,199 tracked .NET types), work has shifted to **stabilization**: a separate `ticket` table in the
same database tracks correctness/documentation/platform audits that aren't "port a .NET type." See
`README.md`'s "Tracking: plan.sqlite3" section for the full `task` vs. `ticket` distinction, and
`prompt.md`'s "Stabilization work — the ticket table" section for the exact resume workflow and SQL
snippets (select next / start / complete / block / needs_user).

**To resume cold, from a fresh context:** read `CLAUDE.md`, this file, and `prompt.md`, then run:
```bash
sqlite3 plan.sqlite3 "SELECT ticket_no, priority, category, title FROM ticket WHERE status='todo' ORDER BY priority, ticket_no LIMIT 10;"
```
and keep working through tickets in priority order exactly as `prompt.md` describes — no need to
re-read this whole section first, it's a snapshot of where things stood, not itself the workflow.

### Ticket queue status (as of this checkpoint)

| Status | P0 | P1 | P2 | P3 | Total |
|---|---|---|---|---|---|
| `done` | 17 | 1 | 0 | 0 | 18 |
| `blocked` | 0 | 0 | 100 | 0 | 100 |
| `todo` | 23 | 614 | 715 | 6 | 1358 |
| **Total** | **40** | **615** | **815** | **6** | **1476** |

The 100 `blocked` P2 tickets are all "Audit public int usage in `<file>`" — deliberately held, not
forgotten (see "Known open decision" below). Everything else `todo` is untouched, ready to pick up
in `ticket_no` order within P0, then P1.

**Next up:** P0 ticket `#26` ("Audit all public headers for POSIX includes") is next in queue order.
A partial answer already exists from this session's own investigation (see below) — reuse it instead
of re-auditing from scratch.

### What was completed this session (2026-07-07 stabilization kickoff)

- **#1–3**: repo/DB sanity (branch `feature/work`, remote confirmed, `ticket` table schema verified
  — **it already existed with all 1,476 rows pre-seeded**, created by a separate process before this
  session; per the user's explicit instruction, it was preserved and used as-is, not recreated).
- **#2**: DB backed up to `plan.sqlite3.backup.20260707_190433` before any ticket-driven writes
  (git-ignored, same as `plan.sqlite3` itself — `*.sqlite3*` pattern in `.gitignore`).
- **#4, #5, #9, #16**: `README.md` — new "Tracking: plan.sqlite3" section (task vs. ticket tables,
  all status values, SQL snippets), Doxygen status-comment section clarified as a secondary hint,
  build instructions fixed to include submodule init + test build/run (previously missing both).
- **#6**: confirmed/documented (not "fixed" — see the DB's own pre-existing "legacy DB noise" note)
  that `ignore` and `ignored` are two real, distinct values; `ignored` predates this workflow.
- **#7, #8**: `plan.md`/`plan_namespaces.md` marked historical, pointing at `plan.sqlite3` instead of
  a hand-maintained table that was ~3.5 weeks stale (2026-06-13, "3939 tests"). The 311-row namespace
  table in `plan_namespaces.md` was left intact as historical reference, not regenerated — it's
  superseded by the live, per-*type* (finer-grained) `task` table.
- **#10**: this section.
- **#11**: `vendor/googletest` confirmed a properly initialized git submodule (checked out at
  `release-1.8.0-3558-g7e2c425d`); `CMakeLists.txt` already has a clear `FATAL_ERROR` fallback
  message pointing at the fix if it were ever missing — no code change needed.
- **#12, #13, #14**: full rebuild with tests ON verified (0 errors/0 warnings, 10,713 tests passing);
  library-only build with `-DSHARP_RUNTIME_BUILD_TESTS=OFF` verified separately in `build-no-tests/`
  (0 errors/0 warnings).
- **#15**: ticket-processing SQL snippets added to `prompt.md` and `README.md`.
- **#18**: `CLAUDE.md`'s stale "6626+ tests passing" floor updated to the real current baseline.
- **#43 + 100 sub-tickets**: closed the "Audit public int parameters" umbrella ticket using this
  session's *own, earlier* independent audit (before the ticket queue existed) — see "Known open
  decision" below. The 100 individual "Audit public int usage in `<file>`" P2 tickets it spawned were
  marked `blocked` on that same pending decision rather than left `todo` (processing them
  individually risks a piecemeal, half-converted codebase before the underlying policy question is
  resolved — see CLAUDE.md rule #10).

Commit: `16c823d` — "Stabilization ticket queue: P0 documentation batch (tickets #4-11,14-16,18,43)".

### Known open decision (unrelated to the ticket queue, predates it)

**`int` vs `SharpRuntime::intcs`**: ~270 call sites across 20+ core files (`DateTime`, `Decimal`,
`Console`, `IntPtr`, `Range`, `MemoryPool`, `UInt128`, etc.) use plain `int`/`long`/`short` where they
mirror a .NET `int`/`long` parameter — the codebase's original, pre-existing convention, not a
regression. Surfaced to the user via `AskUserQuestion` earlier on 2026-07-07; **the user chose to
defer** ("zatím neřešit" / leave for now) rather than pick a fix. Do not action the 100 blocked
tickets (or any other file touching this) until that decision changes. The two real options, if it's
revisited: **(a)** narrow CLAUDE.md rule #7's practical scope to match reality, or **(b)** commission
an explicit, planned, whole-codebase conversion pass (not opportunistic file-by-file changes).

### Platform verification gap (still open, not part of the ticket queue's own P0 audit yet)

Windows/Emscripten builds have never been CI-tested; `CMakeLists.txt` has `MSVC`/`WIN32` branches but
they're unverified. No CI pipeline exists in this repository at all. Real integration against the
downstream CNA/mobile-eggbert projects (the actual purpose of this library) has also not been
verified from within this repository — that would need to happen in those projects' own repos.

---

## Latest session (2026-07-07): System.Xml.XPath — the last 13 `tobedecided` items resolved

**`System.Xml.XPath`** (⚠️ PARTIAL, 13/15 `plan.sqlite3` rows ported, 2 reclassified `ignore` as
out-of-scope Linq extensions — `Extensions`/`XDocumentExtensions` are actually
`System.Xml.Linq`/`XDocument` extension methods, not XPath itself): Implemented `XPathNavigator`/
`XPathDocument`/`XPathExpression`/`XPathNodeIterator`/`XPathItem`/`IXPathNavigable`/`XPathException`
plus the `XPathResultType`/`XPathNodeType`/`XPathNamespaceScope`/`XmlSortOrder`/`XmlCaseOrder`/
`XmlDataType` enums, per user decision (2026-07-07): built over the existing `XmlDocument` DOM only,
no dual `XDocument` abstraction, no new dependency. New concrete `XmlDocumentNavigator`
(`include/src/System/Xml/XPath/XmlDocumentNavigator.hpp/.cpp`) tracks position as a DOM node, an
(element, attribute-identity) pair for attributes, or a synthesized (element, prefix) pair for
namespace nodes materialized from ancestor `xmlns`/`xmlns:*` attributes. `XmlNode::CreateNavigator()`
is wired for real; `SelectSingleNode`/`SelectNodes` (previously `NotImplementedException`) now work.

Hand-written recursive-descent parser/evaluator (`src/System/Xml/XPath/XPathAstInternal.{hpp,cpp}`,
internal) supports child/descendant-or-self(`//`)/attribute/self/parent axes, `*`/`prefix:*`/name/
kind-test node tests (`text()`/`comment()`/`processing-instruction()`), correct per-context-node
positional and boolean predicates, all XPath 1.0 operators including `|` union, and 17 core functions
(`last`, `position`, `count`, `name`, `local-name`, `namespace-uri`, `not`, `boolean`, `string`,
`number`, `concat`, `starts-with`, `contains`, `string-length`, `normalize-space`, `true`, `false`).
**Not supported — throws `XPathException` at `Compile()`, never silently wrong** (see
`XPathNavigator`'s class doc-comment for the exact boundary): explicit `axis::` syntax, variables,
`substring*`/`translate`/`sum`/`floor`/`ceiling`/`round`/`lang`/`id`/`key`/`document`, and
`FilterExpr`-then-path composition. Also omitted entirely (not stubbed): the editable-navigator API,
`MoveTo`/`MoveToId`, `MoveToFollowing`/`SelectChildren`/`SelectAncestors`/`SelectDescendants`/
`Matches`/schema-typed accessors. Namespace-prefixed name tests compare raw prefix strings, not
resolved URIs.

**Found and fixed a real pre-existing bug** in `XmlDocument::Load`/`LoadXml`: tinyxml2's `Parse`/
`LoadFile` free `detachedHolder_` (created in the constructor) without it being recreated afterward,
leaving it dangling and corrupting `IsDetached()`/`getParentNodeProperty()`/`RemoveChild()` for any
node in a document loaded from real markup (as opposed to one built programmatically via
`CreateElement`/`AppendChild`, which never hit this path) — this silently broke navigator
`MoveToParent()` until fixed. Commit `2fa5c79`.

64 new tests (`tests/System/Xml/XPath/XPathTests.cpp`), mostly against a real parsed bookstore-
catalog XML fixture. Commits `2fa5c79` (XmlDocument fix), `4a0e36c` (XPath port) — developed in an
isolated worktree, merged into `feature/work` after independent verification (clean rebuild,
64/64 new tests + full suite passing, no file overlap with the concurrent Xml.Linq work).

**With this, `plan.sqlite3` has zero `tobedecided` rows remaining** — every one of the four decision
groups from the Milestone section below (crypto/TLS, Xml.Linq hierarchy, XPath, and the three misc
singles) has now been resolved and implemented.

## Prior update (2026-07-07): System.Xml.Linq node hierarchy — the 12 `tobedecided` items resolved

The `System.Xml.Linq` `tobedecided` group from the Milestone section below (`XObject`, `XNode`,
`XContainer`, `XCData`, `XComment`, `XDocumentType`, `XProcessingInstruction`, `XStreamingElement`,
`XText`, `XNodeDocumentOrderComparer`, `XNodeEqualityComparer`, `Extensions`) is done. The user was
asked directly (not guessed) on 2026-07-07 whether to migrate `XElement`/`XAttribute`/`XDocument`'s
storage to a real parent/sibling-tracking model now, and approved it.

Commits `417b72d` (small additive `XmlWriter` methods) and `11b70b7` (the hierarchy + migration +
tests):

- **`XObject`** (abstract base of the whole hierarchy, and of `XAttribute`): `getParentProperty()`
  (nearest `XElement`, matching .NET's `parent as XElement` — null if the parent is an
  `XDocument`), `getDocumentProperty()` (walks to the root, returns it only if the root is an
  `XDocument`). `Changed`/`Changing` are no-op `add_Xxx`/`remove_Xxx` accessors, matching this
  codebase's existing convention (e.g. `NetworkChange`) — real change notification would require
  every mutating method in the whole hierarchy to walk up and invoke handlers, out of scope.
  Annotations/`BaseUri`/`IXmlLineInfo` are skipped entirely (no clean C++ equivalent for .NET's
  generic per-object `object?` annotation bag without reflection this runtime otherwise avoids).
- **`XNode`**: sibling navigation (`NextNode`/`PreviousNode`/`NodesBeforeSelf`/`NodesAfterSelf`),
  `Remove()`/`ReplaceWith()`, static `CompareDocumentOrder`/`DeepEquals`, `ToString()`/
  `ToString(SaveOptions)`, `WriteTo(XmlWriter&)`.
- **`XContainer`**: `Add`/`AddFirst`/`RemoveNodes`, `Nodes()`/`Elements()`/`Element(name)`/
  `Elements(name)`/`Descendants()`/`Descendants(name)`/`DescendantNodes()`, `FirstNode`/`LastNode`.
  Children are stored as an ordered `std::vector<std::shared_ptr<XNode>>` rather than reproducing
  .NET's internal circular-linked-list representation — same public API/semantics, simpler C++
  (an explicitly authorized deviation per the task, not a shortcut taken silently).
- **`XElement`/`XDocument`/`XAttribute` migrated onto this model**: `XElement` now holds an ordered
  mix of `XNode` content (elements/text/CDATA/comments/PIs) instead of a flat `XElement`-only
  children vector plus a separate `value_` string; `Value` get/set now really means "concatenated
  descendant text" / "replace all content with one text node", matching .NET. `XAttribute` now
  inherits `XObject` (parent tracking) and kept its existing `next_` intrusive sibling link — now
  wired automatically by `XElement::Add`/`RemoveAttribute` instead of needing manual wiring; added
  `PreviousAttribute`/`Remove()`. `XDocument` now enforces the real single-root-element /
  single-doctype constraint for real (`XContainer::ValidateNode`, overridden by `XDocument`)
  instead of holding `root_`/`declaration_` as unchecked ad hoc fields.
- **Real bug fixed** (not optional, called out explicitly in the task): `XElement::Parse`/`Load`
  and `XDocument::Parse`/`Load` were silent stubs — they ignored their input entirely and always
  returned a fixed empty `<root/>`, in direct violation of CLAUDE.md's "never silently return a
  wrong value." They now parse for real via the existing tinyxml2-backed
  `System::Xml::XmlDocument` DOM wrapper (no new external dependency — reused, not reinvented),
  walking its typed node wrappers (`XmlElement`/`XmlText`/`XmlCDataSection`/`XmlComment`/
  `XmlProcessingInstruction`/`XmlDocumentType`/`XmlDeclaration`) to build a real `XNode` tree.
  `XElement::Parse`/`Load` are now thin wrappers around `XDocument::Parse`/`Load` (parse as a
  document, detach the root via `Remove()` so it doesn't outlive the temporary document with a
  dangling parent pointer, return it) rather than a second, separately-maintained parser.
- **`XText` → `XCData`** (CDATA derives from text, matching .NET), **`XComment`**,
  **`XProcessingInstruction`**, **`XDocumentType`**, **`XNodeDocumentOrderComparer`**,
  **`XNodeEqualityComparer`** (both also directly usable as `std::sort`/`std::unordered_set`
  functors via `operator()`, beyond the .NET-named `Compare`/`Equals`/`GetHashCode` methods).
- **`XStreamingElement`**: standalone, not part of the node tree (matches .NET — it derives from
  neither `XElement` nor `XContainer`). Content items (`std::any`, since real .NET's fully-dynamic
  `object?` content model has no direct C++ analogue) are limited to `std::string`,
  `shared_ptr<XAttribute>`, `shared_ptr<XNode>` (any concrete node, via implicit upcast at the
  `Add()` call site), and nested `shared_ptr<XStreamingElement>` — a deliberately scoped subset,
  documented in the class comment, along with the fact that real .NET's "streaming" laziness comes
  from C# iterator (`yield return`) semantics with no C++ analogue without hand-rolled
  generators/coroutines (out of scope); this port still never builds an `XElement` tree for
  itself, just doesn't defer *evaluation* of already-materialized content the way .NET can.
- **`Extensions`**: `std::ranges`-constrained free functions (no LINQ, per CLAUDE.md) —
  `Elements`/`Attributes`/`Nodes`/`Descendants`/`DescendantNodes`/`Ancestors`/`Remove`/
  `InDocumentOrder` over a range of `shared_ptr<XContainer|XElement|XNode|XAttribute>`. Scoped to
  what maps cleanly; `DescendantsAndSelf`/`DescendantNodesAndSelf` weren't duplicated (call
  `Descendants()`/`DescendantNodes()` plus include the source item directly if needed).
- **Design decision, documented as a scope cut**: re-adding a node that already has a parent
  *moves* it (detaches from the old parent, then attaches) rather than cloning it the way real
  .NET does. This avoids needing a full deep-clone virtual dispatch across every node type, and is
  arguably more useful for a mutable in-memory game-data tree than silent copy-on-add. Verified
  this doesn't leave dangling state via a dedicated test (`XContainerTests.Add_MovesNodeFromOldParent`).
- **Documented parser-backend limitation** (inherited, not introduced): `LoadOptions::PreserveWhitespace`
  only affects text nodes that mix whitespace with real content. The vendored tinyxml2 parser
  never surfaces pure-whitespace-only runs immediately adjacent to element tags as text nodes at
  all, in *any* whitespace mode — verified directly against tinyxml2 itself, not an assumption —
  so the option has no observable effect for that specific case. Same caveat already existed on
  `XmlDocument::getPreserveWhitespaceProperty()` at the classic-DOM layer; this just inherits it.
- Added `XmlWriter::WriteProcessingInstruction`/`WriteDocType` (pure additions — tinyxml2's
  `XMLDeclaration` node already prints as `<?...?>` for any target, and `XMLUnknown` prints raw
  `<!...>`, so both map cleanly onto existing tinyxml2 node types).
- 96 new tests (`tests/System/Xml/Linq/XLinqNodeTests.cpp`, plus updates to `XmlTests.cpp`'s
  `XDocument::Load` test which previously tolerated the stub's fixed output for a missing file and
  now correctly expects `XmlException`). 10194 → 10647 tests. `plan.sqlite3`: 12 rows
  `tobedecided` → `ported`.

## Milestone: plan.sqlite3 has zero `todo`/`''` rows (16199 total rows)

As of this checkpoint, every tracked type across the **entire** dotnet/runtime surface in
`plan.sqlite3` is classified `ported`, `ignore`/`ignored`, or `tobedecided` — there is no more
mechanical porting work queued. This session's autonomous run (see the two log entries below this
one for the full blow-by-blow) finished the last three namespaces that had `todo` items:
`System.Text.Json` (17), `System.Text.Json.Nodes` (5), `System.Text.Json.Serialization` (31).

**58 `tobedecided` items remained, grouped by the real decision each needed — these were genuinely
ambiguous and deliberately not guessed at (per CLAUDE.md's workflow), not overlooked. The user
reviewed all four groups on 2026-07-07 (asked via `AskUserQuestion`, not guessed):**

- **`System.Security.Cryptography` (20) + `.X509Certificates` (5) + `System.Net.Security` (4) —
  DECIDED: permanently out of scope.** Reclassified `ignore`/`outofscope=1` in `plan.sqlite3`; added
  to CLAUDE.md's "Known permanent deviations" list. Reason: implementing this correctly needs either
  a large new external dependency (OpenSSL/mbedTLS) or a hand-rolled, security-critical crypto
  implementation — neither justified for game code. Hash algorithms (MD5/SHA*/HMAC/PBKDF2, no key
  material/confidentiality guarantees to get wrong) remain `ported` and unaffected.
- **`System.Xml.Linq` (12) — DONE.** Migrated the full `XObject`/`XNode`/`XContainer` hierarchy
  (`XCData`/`XComment`/`XDocumentType`/`XProcessingInstruction`/`XStreamingElement`/`XText`/
  `XNodeDocumentOrderComparer`/`XNodeEqualityComparer`/`Extensions`, plus migrating `XElement`/
  `XAttribute`/`XDocument`'s internal storage to a real parent/sibling-tracking model) — see the
  "Latest session (2026-07-07)" section at the very top of this file for the full writeup. See the
  `f793df0` log entry below for the story of how an *earlier* failed background fork's partial
  sketch here was found and handled (deleted, not reused) before this work was done for real.
- **`System.Xml.XPath` (15, 13 ported + 2 reclassified `ignore`) — DONE.** Built `XPathNavigator`
  over `XmlDocument` only (not a dual abstraction spanning both `XmlDocument` and `XDocument`/
  Xml.Linq — smaller, more tractable scope, as decided). See the "Latest session (2026-07-07):
  System.Xml.XPath" section at the very top of this file for the full writeup.
- **`System.IO.FileSystemInfo` (1) — DECIDED: retrofit as a real common base for `FileInfo`/
  `DirectoryInfo`.** Investigation found this wasn't a stale mark needing re-verification: `FileInfo`
  and `DirectoryInfo` already existed as independent classes, each duplicating its own
  `getNameProperty`/`getExistsProperty`/`Delete`/etc. — a genuine "retrofit an abstract base under
  two already-shipped types, or add an unrelated parallel type" decision. Implemented:
  `FileSystemInfo` is a real abstract base (`getFullNameProperty`/`getExtensionProperty`
  concrete; `getNameProperty`/`getExistsProperty`/`Delete` pure virtual; real `CreationTime`/
  `LastAccessTime`/`LastWriteTime` getters via platform `stat`/`std::filesystem`, `LastWriteTime`
  setter via `std::filesystem::last_write_time`), `FileInfo`/`DirectoryInfo` now inherit it and use
  its `fullPath_`/`originalPath_` instead of their own separate path member. `UnixFileMode`,
  `LinkTarget`, `CreateAsSymbolicLink`, `ResolveLinkTarget`, and `CreationTime`/`LastAccessTime`
  *setters* are documented gaps (no portable C++ stdlib support; POSIX `CreationTime` getters use
  `st_ctime` as an approximation of birth time, same fallback real .NET itself uses on Linux).
- **`System.Numerics.Vector<T>` (1) — DECIDED: permanently out of scope.** `Vector2`/`Vector3`/
  `Vector4` (already `ported`) cover ordinary game-code needs; a generic hardware-SIMD `Vector<T>`
  with per-platform intrinsics (SSE/AVX/NEON) is a large, separate undertaking not worth it here.
- **`System.Text.Json.JsonReaderState` (1) — DECIDED: permanently out of scope.** Only meaningful
  paired with a `Utf8JsonReader` (a low-level streaming pull-parser), which isn't tracked in
  `plan.sqlite3` and isn't needed — `JsonDocument`/`JsonElement`/`JsonSerializer` already cover
  practical DOM-based JSON use for game config/data files.

## Post-milestone quality audit: a new decision needed, not a bug list

With the `plan.sqlite3` queue empty, this session used the extra time to audit already-`ported`
code against the CLAUDE.md checklist rather than guess at the `tobedecided` items above. Two real,
narrowly-scoped bugs were found and fixed (see `26ab294` below: `DeflateStream`/`GZipStream`/
`ZLibStream::getLengthProperty()` threw the wrong exception type), plus two stale doc entries in
this file (see `43e99b7`).

A third audit pass — checking rule #7 ("use `SharpRuntime::intcs`, not `int`, in public APIs that
mirror .NET `int` parameters") — surfaced something bigger than a bug list: **plain `int`/`long`/
`short` in public API parameters mirroring .NET integer parameters is not a handful of isolated
slip-ups, it's the pervasive, original convention across roughly 270 call sites in 20+ core files**
(`DateTime.hpp`, `Decimal.hpp`, `Console.hpp`, `Globalization/NumberFormatInfo.hpp`,
`Globalization/HebrewCalendar.hpp`, `IntPtr.hpp`, `Range.hpp`, `Buffers/MemoryPool.hpp`,
`UInt128.hpp`, `ModuleHandle.hpp`, `FormattableString.hpp`, `BinaryData.hpp`,
`SequencePosition.hpp`, `IdnMapping.hpp`, `NetworkInformationException.hpp`,
`ComponentModel/DataAnnotations/DataAnnotationAttributes.hpp`, and more), predating rule #7 or
applied inconsistently across sessions — not something introduced this session.

**Deliberately not touched**, for the same reason the `tobedecided` items above weren't guessed at:
CLAUDE.md rule #10 says "No broad header refactor — naming conventions touch 449+ files and would
break CNA." Fixing this scattered, one file at a time, would leave the codebase in a worse,
inconsistent middle state (e.g. `Byte.hpp` using `intcs` while `DateTime.hpp` still uses `int`)
without actually resolving anything, and any real fix risks cascading into CNA-facing call sites
that already pass plain `int` literals/variables today. This needs an explicit decision from the
user before any code changes:
- **(a)** Accept plain `int` as the de facto, tolerated convention for scalar numeric value
  parameters going forward, and narrow rule #7's practical scope in CLAUDE.md to match reality
  (e.g. limit it to newly-ported types only, or to specific parameter categories); or
- **(b)** Commission an explicit, planned, whole-codebase pass to convert all ~270 sites — scoped,
  reviewed, and tested as its own dedicated effort, not done opportunistically alongside unrelated
  porting work.

No edits were made for this item. Full details of the audit fork's findings are in the session
transcript; re-run a similar grep sweep (`grep -rn '(int \|, int\|(int,\|int&' include/System
--include=*.hpp`) if a fresh list is needed.

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

5. `5191718` — Completed `System.Text.Json.Nodes`: `JsonNode` (abstract base with `AsArray`/
   `AsObject`/`AsValue`, `Parent`/`Root`, `ToJsonString`, static `DeepEquals`/`Parse`), `JsonValue`
   (scalar wrapper), `JsonArray`, `JsonObject`, `JsonNodeOptions`. Found and fixed a real,
   cross-cutting bug while testing `JsonObject`'s insertion-order guarantee: `nlohmann::json`'s
   default object container is `std::map` (sorted by key), so **every** `System.Text.Json` type
   built on it — not just the new `JsonObject`, but also the already-shipped `JsonDocument`/
   `JsonElement` from commit `adba9b8`/`7751266` above — silently lost .NET's documented
   insertion/document-order guarantee on any object. Fixed globally via `nlohmann::ordered_json`
   (a drop-in replacement, verified same nested type aliases) across all 9 affected files; added
   regression tests on both the `JsonObject` and `JsonDocument::EnumerateObject()` sides (only the
   former would have been caught by the pre-existing test suite).
6. `96cfa0f` — Completed `System.Text.Json.Serialization` (the last namespace with `todo` items in
   the entire 16199-row `plan.sqlite3` database): fixed `JsonSerializationAttributes.hpp` (missing
   `JsonAttribute` base class, missing `JsonIgnoreCondition` enum values, wrong types on
   `JsonNumberHandlingAttribute`/`JsonPropertyOrderAttribute`); added `JsonConstructorAttribute`,
   `JsonObjectCreationHandlingAttribute` (with real enum-range validation), `JsonKnownNamingPolicy`,
   `JsonObjectCreationHandling`, `JsonUnknownDerivedTypeHandling`, the `IJsonOnSerializing`/
   `IJsonOnSerialized`/`IJsonOnDeserializing`/`IJsonOnDeserialized` interfaces (documented as not
   automatically invoked — no reflection-based member walk to call them from), `JsonConverter<T>`/
   `JsonConverterFactory` (type-name dispatch standing in for .NET's `Type`-based `CanConvert`),
   `JsonStringEnumConverter<TEnum>` (real working enum↔string conversion via a caller-supplied name
   table, since C++ enums have no reflection), and `ReferenceHandler`/`ReferenceResolver` (real
   `PreserveReferenceResolver`/`IgnoreReferenceResolver`, not wired into `JsonSerializer` itself
   since that dispatches through nlohmann ADL with no `$id`/`$ref` hook — usable directly by
   hand-written converters). 29 new tests, all passed first try.
7. `26ab294` — Post-milestone quality-audit fix (see the Milestone section above): `DeflateStream`/
   `GZipStream`/`ZLibStream`'s `Length` property getter threw `NotImplementedException`, but real
   .NET throws `NotSupportedException("This operation is not supported.")` — and the `Stream` base
   class's own default `Seek`/`SetLength`/`Position` implementations already (correctly) throw
   `NotSupportedException` for the same reason, so the three subclasses were inconsistent with both
   their own base class and the real .NET behavior they mirror. Found via a sweep of every
   remaining `NotImplementedException` call site in the codebase, cross-checked against
   `/rv/tmp/runtime/src/libraries/System.IO.Compression`.

`System.Text.RegularExpressions`, `System.Xml.Linq` (minus the `tobedecided` hierarchy items),
`System.Text.Unicode`, `System.Text.Json`, `System.Text.Json.Nodes`, and
`System.Text.Json.Serialization` are now all fully classified. **Zero `todo`/`''` rows remain
anywhere in the entire 16199-row `plan.sqlite3` database** — see the Milestone section at the top
of this file for the full breakdown of the 58 `tobedecided` items that genuinely need a user
decision rather than a guess.

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
| documented limitation | `System::Net::Http::Json::HttpClientJsonExtensions`/`HttpContentJsonExtensions` only provide non-generic, `JsonDocument`-returning overloads (`GetFromJsonAsync`, `ReadFromJsonAsync`, etc.) — .NET's generic `GetFromJsonAsync<T>`/`PostAsJsonAsync<T>` need reflection-based `JsonTypeInfo<T>` marshaling this runtime doesn't have. (Stale note fixed: `System::Text::Json::JsonSerializer::Serialize<T>()`/`Deserialize<T>()` are no longer stubs — they were given a real ADL-based (`nlohmann::ordered_json` `to_json`/`from_json`) ​implementation later in this same session; see the `JsonSerializerTests.Serialize_Int`/`Serialize_VectorOfInt`/`Deserialize_*` tests in `tests/Task41Tests.cpp`.) |
| documented limitation | `System::Net::Mime::ContentType` doesn't reproduce .NET's `_isChanged`/`_isPersisted` wire-caching (tied to `System.Net.Mail`'s message-writing pipeline, which isn't ported) — `ToString()` always recomputes fresh. Its RFC 2045 comment/CFWS grammar support is plain-whitespace-only (no nested `(...)` comments). |
| documented limitation | `System::Net::NetworkInformation::NetworkChange`'s event add/remove accessors are no-ops — there is no real OS network-change notification (Linux netlink, macOS `SCNetworkReachability`, Windows `NotifyAddrChange`), matching the pre-existing `AppDomain.UnhandledException` stub convention in this codebase. |
| documented limitation | `System::Net::NetworkInformation::NetworkInformationException`'s default constructor uses `errno` in place of .NET's `Marshal.GetLastPInvokeError()` (no P/Invoke layer here); its internal `(message, innerException)` constructor isn't reproduced (`Win32Exception`, the base class, has no inner-exception-carrying constructor to forward to). |
| fixed | `plan.sqlite3` `TcpListener` row (previously `todo`) has been corrected to `ported` — it was already fully implemented as a nested class in `include/System/Net/Sockets/TcpClient.hpp` (confirmed working — it backs `HttpClientJsonExtensionsTests` integration tests). |

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
