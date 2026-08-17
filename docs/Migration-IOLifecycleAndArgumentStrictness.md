<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::IO` lifecycle and argument strictness (ticket #2104)

*2026-08-12.* Plan §12 of `docs/SystemIONamespaceReviewPlan.md` requires one migration note
covering **#2098–#2103 together**, written when the first of them lands. They have all landed or
been dispositioned now, so this is that note — written last, as #2104's `LAND LAST` marker
requires, and covering the tickets the review's ticket list did not yet contain when §12 was
written: **#2108**, **#2344** and **#2345**, which are splits of #2098 and #2102.

Everything in **§1–§6** is **source-, ABI-, layout-, vtable- and `noexcept`-compatible**. Not one
public signature, member, base, virtual, object layout or exception specification changed in those
tickets. What did change is **behaviour, deliberately and in one direction**: calls that used to
succeed quietly on invalid input now throw, and a watcher now honours the filter it was given.

**§7 is different, and was added later.** Ticket #2098 landed on 2026-08-17 under Approval IO-1
and **does change one object layout** — `sizeof(StringWriter)` 384 → 392 — so it carries a
full-consumer-rebuild requirement that §1–§6 do not. It is still source-, ABI-, vtable- and
`noexcept`-compatible. Read §7 as well as §2 before upgrading.

**This note is not a claim that the namespace is finished.** Things are still open and each has a
named owner; §6 says exactly what they are, and §5 says what a caller can still rely on that a
future resolution would take away.

---

## 1. What changed, by ticket

| Ticket | Finding | Was | Is |
|---|---|---|---|
| **#2101** | SR-AUD-347 | `FileInfo("")` / `DirectoryInfo("")` let a raw `std::filesystem_error` out of a `System`-shaped API | `System::ArgumentException`, naming its parameter |
| **#2100** | SR-AUD-340 | `RandomAccess::Write(fd, buf, -1, 0)` **succeeded silently**; `GetLength(-1)` returned the sentinel `-1`; other rejections were a bare `IOException` naming neither the parameter nor the native reason | `ArgumentOutOfRangeException` naming the parameter; `GetLength` throws; every native failure carries its native reason |
| **#2103** | SR-AUD-345 | `FileInfo(directory).Delete()` **deleted the directory**, while its sibling `File::Delete` threw on the same input | both throw; the directory survives |
| **#2108** | SR-AUD-344 | `UnmanagedMemoryStream` recorded being closed and six members never consulted it | those six throw `ObjectDisposedException`; the closed check precedes the argument check |
| **#2099** | SR-AUD-342 | after `Close()`, `FileStream::Length` re-`stat`ed the path, `Position` returned the sentinel `-1`, and `Position=`, `Seek`, `SetLength` and `Flush` all succeeded | all six throw `ObjectDisposedException` |
| **#2344** | SR-AUD-339 | `FileSystemWatcher::setPathProperty` stored the new directory and did nothing else: the old `inotify` watch stayed armed, and its events were reported with a `FullPath` built from the **new** directory — a path naming no file that existed. Also a **data race** on `directory_` between the caller and the watcher thread (TSan: 3 races before, 0 after) | the watch is retired and re-armed on the new directory, with the watcher thread joined before the write |
| **#2345** | SR-AUD-346, class-crossing half | `NotifyFilter` was validated, stored, and **read by nothing**: all ten filter configurations produced the byte-identical seven events, including `NotifyFilters(0)` | a name-class filter no longer admits content-class events, and vice versa |

The reference tree (`/rv/tmp/runtime/src/libraries/`) was absent throughout. Every exception type
and `paramName` above is recorded in the plan as **this port's choice**, not as a verified match
to .NET.

---

## 2. What breaks, and how to fix it

Each row is a real call that changed answer. None of them was correct before.

**A closed stream or wrapper is now closed.**

```cpp
FileStream fs(path, FileMode::Open, FileAccess::Read);
fs.Close();
auto n = fs.getLengthProperty();   // was: a stale 5. now: ObjectDisposedException
```

Read the length before closing, or keep the stream open for as long as you query it. The same
applies to `Position` (get and set), `Seek`, `SetLength` and `Flush` on `FileStream`, and to the
six `UnmanagedMemoryStream` members #2108 fixed. Note the ordering rule that came with them: the
**closed check runs first**, so a closed stream given a bad argument now reports being closed
rather than reporting the argument.

**A negative count is no longer a silent no-op.**

```cpp
RandomAccess::Write(fd, buffer, /*count*/ -1, /*fileOffset*/ 0);
// was: returned normally, wrote nothing. now: ArgumentOutOfRangeException("count")
```

If a computed count can go negative, clamp it at the call site — the previous behaviour hid the
arithmetic bug rather than the write.

**`FileInfo::Delete` no longer deletes directories.**

```cpp
FileInfo(someDirectory).Delete();   // was: the directory was DELETED. now: throws
```

Use `DirectoryInfo::Delete` (or `Directory::Delete`) for a directory. This is the only change in
the namespace that **removes a data-loss path**: code that reached this by accident was destroying
user data, and code that reached it on purpose was relying on `FileInfo` and `File` disagreeing.

**An empty path throws a `System` exception, not a `std::` one.**

```cpp
try { FileInfo fi(""); }
catch (const std::filesystem::filesystem_error&) { /* was: reached */ }
catch (const System::Exception&)                 { /* now:  reached */ }
```

A caller catching `System::Exception` starts catching what it always should have.

**A configured `NotifyFilter` now excludes events.**

```cpp
FileSystemWatcher w(dir);
w.setNotifyFilterProperty(NotifyFilters::Size);
// was: Created, Deleted and Renamed arrived anyway. now: only Changed does.
```

Callers that set a filter and relied on receiving everything regardless must widen the filter —
`NotifyFilters::FileName | NotifyFilters::DirectoryName` restores the name-class events. The
default filter (`LastWrite | FileName | DirectoryName`) spans both classes and is **byte-identical
before and after**, so a caller that never touched `NotifyFilter` sees no change at all.
`NotifyFilters(0)` names no change and now admits nothing.

**Changing `Path` on a live watcher actually moves the watch.**

```cpp
w.setEnableRaisingEventsProperty(true);
w.setPathProperty(otherDirectory);
// was: events kept coming from the ORIGINAL directory, labelled with the new one.
// now: the original directory is no longer watched; the new one is.
```

Code that (knowingly or not) depended on the old directory staying watched must create a second
watcher. A rejected path — empty, or not an existing directory — still throws
`ArgumentException` and leaves the existing watch completely untouched.

---

## 3. What did NOT change

- **Every previously-valid call still returns exactly what it returned.** #2100's valid
  read/write round trip is byte-identical; #2099's open-stream control block is byte-identical;
  #2345's default-filter row is byte-identical.
- **No descriptor accounting changed.** `/proc/self/fd` — not LSan, per plan §14 — shows a delta
  of **0** across 100 throwing `FileStream` constructors, 100 closed-stream rejection cycles, 200
  rejected `RandomAccess` calls, and 50 watcher path re-arms.
- **`leaveOpen` still governs the underlying stream** in `StreamReader`/`StreamWriter`, exactly as
  before. Whether it *also* governs the wrapper's own disposal is answered by #2098 in §7 — and
  the answer is not the same for the reader and the writer.
- **No layout, vtable or `noexcept` change in any ticket of §1.** #2098, added in §7, changes one
  layout; nothing in this section is affected by it.

---

## 4. Two contracts that are new rather than changed

Both came out of #2344 and #2345 and are recorded here because they are observable and were not
specified before:

1. **A reconfiguration discards undelivered events.** Changing `Path` or `NotifyFilter` on a live
   watcher retires the old watch, and events already queued but not yet delivered go with it —
   exactly as they do when `EnableRaisingEvents` is set to false. Whether a watcher *should* drain
   first is the same question as #2105's and is not answered.
2. **Setting `Path` can arm a watcher that was enabled first.** `EnableRaisingEvents = true`
   before any `Path` was configured used to leave the watcher enabled and permanently inert. The
   re-arm is gated on the enabled flag rather than on a thread already running, so the path now
   arms it. If the new directory cannot be armed, the pre-existing contract applies unchanged:
   `EnableRaisingEvents` goes false and `Error` is raised.

---

## 5. Behaviour you can still rely on — but only until a named ticket resolves

These are **pinned by tests** in `modules/io/tests/System/IO/IONamespaceReviewTests.cpp`, so a
resolution cannot land silently. They are not guarantees; they are a record of the current answer.

| Behaviour | Owner |
|---|---|
| The six content-class `NotifyFilters` values are mutually indistinguishable, and `FileName` behaves as `DirectoryName` does | **#2346** (`needs_user`) |
| ~~A text wrapper keeps working after `Close()`~~ — **resolved by #2098**, see §7 | — |
| `BinaryData::ToString()` returns invalid UTF-8 bytes unchanged rather than substituting U+FFFD | **#2106** (deferred) |
| Every `BinaryData` construction path copies its source, including the `ReadOnlyMemory` ones | **#2106** (deferred) |
| No handler is invoked for activity occurring after `EnableRaisingEvents = false` returns | **#2105** (deferred) |

---

## 6. What is still open

- **#2098 — LANDED 2026-08-17**, see §7. Approval IO-1 was granted as `docs/StandingApprovals.md`
  SA-3. `UnmanagedMemoryStream`, the fifth member of the original ticket, needed no new storage and
  had already landed separately as **#2108**.
- **#2346 — `needs_user`.** The `NotifyFilters` → inotify mapping *within* each class. Five
  questions, each priced in plan §21.10: which value `IN_MODIFY` serves, which values `IN_ATTRIB`
  serves, what to do about `CreationTime` (inotify cannot report it at all), whether to add
  `IN_ACCESS` for `LastAccess`, and whether to separate `FileName` from `DirectoryName` using
  `IN_ISDIR`. Taking **no** decision is a supported outcome — it is simply not one this review may
  take on the user's behalf. **SR-AUD-346 therefore remains `confirmed`.**
- **#2105 — deferred.** Whether a handler already *executing* can still be running when
  `EnableRaisingEvents = false` returns. Answering it needs ThreadSanitizer and a blocking-handler
  harness. The observable half is pinned; the concurrency question is not answered.
- **#2106 — deferred.** `BinaryData`'s UTF-8 decoding (SR-AUD-185) and copy-vs-wrap semantics
  (SR-AUD-186). Both need the absent reference tree. SR-AUD-186's premise is **inverted**: .NET's
  behaviour is the aliasing one and this port's is the defensive one, so "fixing" it means making
  `BinaryData` alias caller memory it does not own.
- **#2347 — found while writing #2104's #2105 pin; REMEDIATED 2026-08-12.** Calling
  `EnableRaisingEvents`, `Path` or `NotifyFilter` **from inside a handler** self-joined the watcher
  thread: `std::system_error` ("Resource deadlock avoided") was raised on the watcher thread, and
  because handler invocation was not wrapped in a `try`/`catch` it reached `std::terminate`.
  Measured, `SIGABRT` (`build-probe/2104_probe1_modeA.log`). .NET permits the pattern. **What
  changed:**
  - `EnableRaisingEvents = false` **from a handler is now permitted**, matching .NET. The stop is
    signalled and the thread is not joined; the loop exits when the handler returns, and the
    thread is reaped by the next reconfiguration or by the destructor. The setter returns at once.
  - `Path` and `NotifyFilter` **from a handler now throw `System::InvalidOperationException`**
    while a watch is live, leaving the watcher's state exactly as it was. Rejection rather than
    deferral is deliberate: both re-arm by retiring the inotify watch the calling thread is
    dispatching from, and .NET's exact semantics for that case are not measurable here (`/rv`
    absent). Reconfiguring from any other thread is unchanged.
  - **An exception escaping any handler no longer reaches `std::terminate`**: it is delivered to
    the `Error` handlers, and an `Error` handler that itself throws is swallowed rather than
    allowed to recurse.

  If your code relied on the old behaviour it was crashing, so there is nothing to migrate except
  the two rejected setters — move those to another thread. No `SR-AUD-*` identifier is issued;
  audit numbering stays frozen at 364.

---

## 7. #2098 — the four text wrappers now enforce their closed state (2026-08-17)

**Approval IO-1 was granted** as `docs/StandingApprovals.md` SA-3. SR-AUD-337 and SR-AUD-343 are
remediated. This section is the migration record; the design is plan §21 and the corrections
below.

### 7.1 The layout change, and who must rebuild

| Type | `sizeof` before | `sizeof` after |
|---|---:|---:|
| `StringWriter` | 384 | **392** |
| `StringReader` | 48 | 48 |
| `StreamReader` | 24 | 24 |
| `StreamWriter` | 24 | 24 |
| `TextReader`, `TextWriter` (bases) | 8 | 8 |

One type grows, by one aligned slot; the other three absorb the flag into existing tail padding.
**Every consumer must be fully recompiled** — a `sizeof` change across a stale-header boundary is
an ODR violation with no diagnostic. sharp-runtime ships as a static library built from source, so
this is a rebuild requirement, not a broken distributed binary. **No vtable, mangled-symbol,
signature or `noexcept` change is involved**, and both base classes are untouched, so no type
derived from them outside this module changes shape.

Both layouts are pinned by `IoLayoutPinTests`, which now asserts the *after* state.

### 7.2 What starts throwing

After `Close()`, these throw `System::ObjectDisposedException`:

| Type | Members | Message | `getObjectNameProperty()` |
|---|---|---|---|
| `StringReader` | `Peek`, `Read`, `ReadLine`, `ReadToEnd` | `Cannot read from a closed TextReader.` | *(empty)* |
| `StringWriter` | every `Write`/`WriteLine` overload | `Cannot write to a closed TextWriter.` | *(empty)* |
| `StreamReader` | `Peek`, `Read`, `ReadLine`, `ReadToEnd` | same, plus `\nObject name: 'StreamReader'.` | `StreamReader` |
| `StreamWriter` | every `Write`/`WriteLine` overload, `Flush` | same, plus `\nObject name: 'StreamWriter'.` | `StreamWriter` |

The empty object name is not an oversight: `StringReader.cs:325-328` and `StringWriter.cs:73-75`
pass `null`, while `StreamReader.cs:1408` and `StreamWriter.cs:1015` pass `GetType().Name`. The
port reproduces the difference.

**To migrate:** stop using a wrapper after closing it, or drop the `Close()` call and let the
destructor do the work — which is what it was already doing.

### 7.3 What deliberately keeps working

- **`StringWriter::ToString()` and `GetStringBuilder()`** still return the text written before
  the close. `StringWriter.cs:309-312` and `:64-67` have no `_isOpen` check, so guarding them
  would be a divergence.
- **`StreamReader::getBaseStreamProperty()` and `StreamWriter::getBaseStreamProperty()`** still
  return the stream. `BaseStream` is a bare field read in both .NET types.
- **A `leaveOpen` `StreamWriter` keeps writing after `Close()`** — see §7.4.
- **`StreamWriter::Write(const char*)` with a null pointer** still writes nothing and throws
  nothing, even on a closed writer, because .NET's `Write(string?)` returns before its disposal
  check.
- Closing twice is safe everywhere, and the destructor no longer re-closes a stream that an
  explicit `Close()` already closed.

### 7.4 A premise correction: half of SR-AUD-337 was a false positive

The finding reported that a `leaveOpen` `StreamWriter` grows its underlying stream after
`Close()`. Measured against the reference tree, **.NET does that too**, and the two stream
wrappers disagree with each other by design:

```csharp
// StreamReader.cs:243-268     _disposed = true;  THEN  if (_closable) { _stream.Close(); }
// StreamWriter.cs:221-244     if (_closable && !_disposed) { ... finally { _disposed = true; } }
```

`_closable` is `!leaveOpen` in both. So a `leaveOpen` **reader** is disposed by `Close()` and a
`leaveOpen` **writer** is not. #2098 reproduces that asymmetry rather than tidying it away, and
pins it by test. What the writer half actually repairs is the *non*-`leaveOpen` case, where a
write after `Close()` used to reach a closed stream instead of throwing.

### 7.5 Downstream impact, measured

Per `docs/StandingApprovals.md` SA-2 condition 5, both consumer checkouts were searched:

- **`cna`** — **zero** uses of any of the four types. Its `StringReader` hits are CNA's own
  `CNA::Internal::Xnb::StringReader` content reader, an unrelated type.
- **`mobile-eggbert`** — **one** use, `Worlds.cpp:154`: a `StreamReader` constructed over a
  stream, read with `ReadToEnd()`, after which the *stream* is closed directly and the reader is
  destroyed. It never calls `StreamReader::Close()`, so no member of it starts throwing. It must
  still be rebuilt, like every consumer.

Neither repository was modified.
