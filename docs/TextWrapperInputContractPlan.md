<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Text-wrapper input contract plan (tickets #1808 and #1809)

Ticket **#1823** (`REMED-IO-TEXT-WRAPPER-CONTRACT-PLAN`, P2, size M, design-only).
No production source changed under this ticket.

Tickets **#1808** (`StreamReader`/`StreamWriter` do not validate `CanRead`/`CanWrite`)
and **#1809** (a null `const char*` is undefined behaviour across the `TextWriter`
`Write` family) were both opened inactive by ticket #1806 and both name the same
headers. Neither carries an `SR-AUD-*` identifier; the audit numbering stays frozen
at 364 and nothing here reopens `SR-AUD-338`, which is remediated. This document
answers both together, from **measured** current behaviour and from the **current**
.NET sources, so that neither is patched one guard at a time.

Every "today" row below was produced by `build-probe/1823_prefix_defects.cpp`
(log `build-probe/1823_prefix_defects.log`), one case per process under
AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer, **before** any
production change.

---

## 1. Complete affected-surface inventory

### 1.1 The text-wrapper family as it exists in this repository

| Type | File | Base | Owns a `Stream*`? | `const char*` surface |
|---|---|---|---|---|
| `TextReader` | `modules/io/include/System/IO/TextReader.hpp` | — | no | none |
| `TextWriter` | `modules/io/include/System/IO/TextWriter.hpp` | — | no | `Write(const char*)`, `WriteLine(const char*)` (both `virtual`) |
| `StreamReader` | `modules/io/include/System/IO/StreamReader.hpp` + `.cpp` | `TextReader` | **yes** | none |
| `StreamWriter` | `modules/io/include/System/IO/StreamWriter.hpp` + `.cpp` | `TextWriter` | **yes** | `Write(const char*)` **override** |
| `StringReader` | `modules/io/include/System/IO/StringReader.hpp` | `TextReader` | no | none |
| `StringWriter` | `modules/io/include/System/IO/StringWriter.hpp` | `TextWriter` | no | inherits `TextWriter`'s two |
| `BinaryReader` | `modules/io/include/System/IO/BinaryReader.hpp` + `.cpp` | — | **yes** | none |
| `BinaryWriter` | `modules/io/include/System/IO/BinaryWriter.hpp` + `.cpp` | — | **yes** | none |
| `BufferedStream` | `modules/io/include/System/IO/BufferedStream.hpp` + `.cpp` | `Stream` | **yes** | none |
| `Console` | `modules/console/include/System/Console.hpp` | — (all `static`) | no | `Write(const char*)`, `WriteLine(const char*)` |

**Structures that do not exist in this port and are therefore out of scope, stated
rather than assumed:**

- no synchronized wrapper (.NET's `TextWriter.Synchronized` / `SyncTextWriter`);
- no null implementation (.NET's `TextWriter.Null` / `TextReader.Null`);
- no adapters or decorators over `TextReader`/`TextWriter` — the four subclasses
  above are the complete set, confirmed by
  `grep -rn ": public TextWriter\|: public TextReader" modules/`;
- **no asynchronous path at all**: `grep -rn "Async" ` over all six text-wrapper
  headers returns nothing. There is no `WriteAsync`, `FlushAsync`,
  `ReadLineAsync`, `DisposeAsync` or task-returning member anywhere in the family,
  so the prompt's "whether synchronous and asynchronous paths differ" question has
  the answer *there is only a synchronous path*;
- no span or `ReadOnlyMemory<char>` overload on `TextWriter`; the only buffer-shaped
  overloads in the family are `Console::Write(const std::vector<char>&)` and
  `Console::Write(const std::vector<char>&, intcs, intcs)`, which take a container
  by reference and cannot be null;
- no `Dispose`; `Close()` is the disposal entry point and neither wrapper has a
  disposed flag — that is `SR-AUD-337`, still `confirmed`, and deliberately **not**
  in scope here (see §13.3).

### 1.2 Every method in the family, and whether each ticket touches it

`TextWriter` — 18 virtuals. `Write(const std::string&)` is pure virtual; the other
17 forward to it.

| Method | #1808 | #1809 |
|---|---|---|
| `Write(const std::string&)` = 0 | — | — |
| `Write(const char*)` | — | **yes** |
| `Write(char)`, `Write(intcs)`, `Write(longcs)`, `Write(double)`, `Write(Single)`, `Write(bool)` | — | — (no pointer parameter) |
| `WriteLine(const std::string&)` | — | — |
| `WriteLine(const char*)` | — | **yes** |
| `WriteLine()`, `WriteLine(char/intcs/longcs/double/Single/bool)` | — | — |
| `Flush()`, `Close()` | — | — |
| `~TextWriter()` | — | — |

`StreamWriter` — `Write(const std::string&)`, `Write(const char*)`, `Flush()`,
`Close()`, two constructors, destructor.

| Method | #1808 | #1809 |
|---|---|---|
| `StreamWriter(Stream*, bool leaveOpen)` | **yes** | — |
| `StreamWriter(const std::string& path)` | **yes** (indirectly — it builds its own `FileStream`) | — |
| `Write(const std::string&)` | — | — |
| `Write(const char*)` **override** | — | **yes** |
| `Flush()`, `Close()`, `~StreamWriter()` | — | — |

`StreamReader` — `Peek()`, `Read()`, `ReadLine()`, `ReadToEnd()`, `Close()`, two
constructors, destructor. Only the two constructors are touched, by #1808.

`StringWriter` — `Write(const std::string&)` override, `ToString()`,
`GetStringBuilder()`. #1809 reaches it only through the inherited
`TextWriter::Write(const char*)`/`WriteLine(const char*)`; it declares no
`const char*` member of its own.

`StringReader`, `TextReader` — untouched by both tickets.

`Console` — `Write(const char*)` and `WriteLine(const char*)` are touched by #1809.
Its 10 other `Write` overloads and 12 other `WriteLine` overloads take values or
`const&` containers and cannot receive a null pointer.

### 1.3 Explicitly excluded from #1809

Any parameter declared `const std::string&` can also be handed a null `const char*`
by a caller, because the implicit conversion runs at the call site. That is true of
several hundred parameters across this repository and is not a property of the
text-wrapper family; #1809 is scoped to surfaces that **declare** `const char*`.
`StringReader(const std::string&)` is the nearest example and is excluded on that
basis.

---

## 2. Current sharp-runtime behaviour, measured

### 2.1 #1808 — stream direction

`build-probe/1823_prefix_defects.log`, cases 1–16:

| # | Input | Measured behaviour today |
|---|---|---|
| 1 | `StreamWriter` over `MemoryStream(buf, n, writable=false)` — `CanWrite=0` | **constructs successfully** |
| 2 | …then `Write("x")` | `NotSupportedException: Stream does not support writing.` |
| 3 | …then `Flush()` only | **succeeds silently** — no diagnostic ever |
| 4 | …then destruction with `leaveOpen=false` | **succeeds silently** |
| 5 | `StreamWriter` over `FileStream(path, Open, FileAccess::Read)` — `CanWrite=0`, then `Write("x")` | **constructs and writes "successfully"** — no exception at all |
| 15 | the same, then read the file back | file still contains `"seed"` — **the write was silently discarded** |
| 16 | `FileStream::Write` directly on the same read-only `FileStream` | accepted; file still `"seed"` — **silent loss one level down** |
| 6 | `StreamReader` over `FileStream(path, Append)` — `CanRead=0`, then `Read()` | constructs; `Read()` returns **`-1`** |
| 7 | the same, `ReadToEnd()` | returns **`""`** |
| 8 | `StreamWriter` over a custom `Stream` that implements `Write()` but does **not** override `getCanWriteProperty()` — reports `CanWrite=0` | **works perfectly**: `written="hello"` |
| 9 | `StreamReader` over a custom `Stream` that does not override `getCanReadProperty()` — reports `CanRead=1` | works: `readToEnd="hi"` |
| 10 | `BinaryWriter` over the read-only `MemoryStream` | `ArgumentException: Stream was not writable.` — **already guarded** |
| 11 | `BinaryReader` over the write-only `FileStream` | `ArgumentException: Stream was not readable.` — **already guarded** |
| 12 | `StreamWriter` over a **closed** `MemoryStream` — reports `CanWrite=1` | constructs |
| 13 | `StreamReader` over a **closed** `MemoryStream` — reports `CanRead=1` | constructs |
| 14 | `StreamWriter` over a `MemoryStream` closed **after** construction, then `Write` | `ObjectDisposedException` |

Three facts here are stronger than #1808's own description, which said the error
"surfaces later as a `NotSupportedException` from the first `Write`, or not at all
if nothing is ever written":

1. **For `FileStream` the error surfaces never, even when data *is* written**
   (cases 5/15). `std::fstream::write` on a handle opened without `std::ios::out`
   sets `badbit` and returns; `FileStream::Write` inspects neither `canWrite_` nor
   the stream state, so the bytes are dropped in silence. This is data loss, not a
   late diagnostic.
2. **`Flush()` and destruction are silent too** (cases 3/4), so the "or not at all"
   half is reachable on `MemoryStream` as well.
3. **The reader half is the `SR-AUD-338` laundering defect again** (cases 6/7):
   `-1` and `""` are exactly what an empty document returns, so "this stream cannot
   be read at all" is indistinguishable from "the document was empty" — the same
   argument that justified removing `StreamReader`'s null guards under #1806.

### 2.2 #1809 — null `const char*`

`build-probe/1823_prefix_defects.log`, cases 20–29. Three **structurally different**
current behaviours across one family:

| # | Call | Measured behaviour today |
|---|---|---|
| 20 | `TextWriter&` → `StringWriter::Write(nullptr)` | `std::logic_error: basic_string: construction from null is not valid` |
| 21 | `TextWriter&` → `StringWriter::WriteLine(nullptr)` | same `std::logic_error` |
| 24 | `StringWriter::Write(nullptr)` (static dispatch) | same `std::logic_error` |
| 25 | `StringWriter::WriteLine(nullptr)` (static dispatch) | same `std::logic_error` |
| 22 | `StreamWriter::Write(nullptr)` | **ASan `SEGV` on `0x0` in `strlen`**, called from `StreamWriter::Write(char const*)` |
| 23 | `StreamWriter::WriteLine(nullptr)` | same **`SEGV`**, via `TextWriter.hpp:63` → `StreamWriter::Write(const char*)` |
| 26 | `Console::Write(nullptr)` | accepted, **`std::cout.bad() == 1`** |
| 27 | `Console::WriteLine(nullptr)` | accepted, **`std::cout.bad() == 1`** |
| 28 | `StringWriter::Write("")` control | length 0, no diagnostic |
| 29 | `StreamWriter::Write("")` control | length 0, no diagnostic |

The `std::logic_error` in cases 20/21/24/25 is libstdc++ being generous — the
standard says constructing `std::basic_string` from a null pointer is undefined, and
the exception is a quality-of-implementation courtesy, not a contract. It is also
the wrong hierarchy: it is a `std::` exception, not a `System::` one, so a caller
writing `catch (const System::Exception&)` does not catch it.

Cases 26/27 are the worst of the three and were not called out in #1809's
description. `std::cout << (const char*)nullptr` sets `badbit` on `std::cout`
permanently, so **every subsequent `Console::Write`/`WriteLine` in the process
silently produces nothing.** One null argument disables the program's entire
standard output, with no exception, no crash and no message.

---

## 3. Actual .NET behaviour

Read from `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/IO/`
on 2026-07-29, not from memory.

### 3.1 Stream direction

| .NET member | File:line | Behaviour |
|---|---|---|
| `StreamReader(Stream, Encoding?, bool, int, bool)` | `StreamReader.cs:145-148` | `if (!stream.CanRead) throw new ArgumentException(SR.Argument_StreamNotReadable);` — **after** the null check, **before** the `bufferSize` check |
| `StreamWriter(Stream, Encoding?, int, bool)` | `StreamWriter.cs:101-104` | `if (!stream.CanWrite) throw new ArgumentException(SR.Argument_StreamNotWritable);` — same position |
| `BinaryReader(Stream, Encoding, bool)` | `BinaryReader.cs:44-46` | same rule, `Argument_StreamNotReadable` |
| `BinaryWriter(Stream, Encoding, bool)` | `BinaryWriter.cs:50-51` | same rule, `Argument_StreamNotWritable` |
| `BufferedStream(Stream, int)` | `BufferedStream.cs:80` | rejects only when **both** are false, and with `ObjectDisposedException`, not `ArgumentException` |
| `FileStream.Write` (sync) | `Strategies/OSFileStreamStrategy.cs:232-241` | closed check first, then `if ((_access & FileAccess.Write) == 0) ThrowNotSupportedException_UnwritableStream()` |
| `FileStream.Read` (sync) | `Strategies/OSFileStreamStrategy.cs:208-217` | same shape, `ThrowNotSupportedException_UnreadableStream()` |
| `MemoryStream.CanRead` / `CanWrite` | `MemoryStream.cs:99,103` | `CanRead => _isOpen`, `CanWrite => _writable` |

Exact strings, from `Resources/Strings.resx`:

| Key | Value |
|---|---|
| `Argument_StreamNotReadable` | `Stream was not readable.` |
| `Argument_StreamNotWritable` | `Stream was not writable.` |
| `NotSupported_UnreadableStream` | `Stream does not support reading.` |
| `NotSupported_UnwritableStream` | `Stream does not support writing.` |

Both `ArgumentException`s are constructed with **the message only** — no
`paramName`, so no `(Parameter 'stream')` suffix. `BinaryReader`/`BinaryWriter` in
this repository already reproduce that exactly (`BinaryReader.cpp:24-25`,
`BinaryWriter.cpp:18-19`), so the spelling for #1808 is already fixed by
in-repository precedent and needs no invention.

**A capability cannot usefully change after construction in .NET's model**: the
constructor snapshot is not re-checked, and every later operation relies on the
underlying stream's own guard. `.NET` validates once at construction *and* again in
the stream itself; this port must do the same or accept silent loss.

### 3.2 Null text

| .NET member | File:line | Behaviour for `null` |
|---|---|---|
| `TextWriter.Write(string? value)` | `TextWriter.cs:277-283` | `if (value != null) Write(value.ToCharArray());` — **no-op** |
| `TextWriter.Write(char[]? buffer)` | `TextWriter.cs:162-168` | `if (buffer != null) …` — **no-op** |
| `TextWriter.WriteLine(string? value)` | `TextWriter.cs:502-509` | `if (value != null) Write(value); Write(CoreNewLineStr);` — **writes the line terminator only** |
| `StreamWriter.Write(string? value)` | `StreamWriter.cs:476-479` | `WriteSpan(value, appendNewLine: false)`; a null string converts to an empty span — no-op |
| `StreamWriter.WriteLine(string? value)` | `StreamWriter.cs:482-486` | `WriteSpan(value, appendNewLine: true)` — line terminator only |
| `StringWriter.Write(string? value)` | `StringWriter.cs:125-136` | disposed check, then `if (value != null) _sb.Append(value);` — no-op |
| `Console.Write(string?)` / `WriteLine(string?)` | delegates to `Out`, a `TextWriter` | same no-op / terminator-only rule |

So .NET's answer to "does null mean no-op, newline-only, empty string, or
exception" is **no-op for `Write`, newline-only for `WriteLine`, never an
exception**, and it is uniform across the whole family including the null and
synchronized writers (`StreamWriter.cs:1045,1070`: `public override void
Write(string? value) { }`). A guard that threw `ArgumentNullException` would be a
**divergence**, exactly as #1809's description warned.

`.NET` has no `const char*` overload — the pointer overload is this port's own
addition, introduced so that a string literal binds to the string overload instead
of `Write(bool)` (`TextWriter.hpp:30-37`). Since the port added the overload as a
spelling of "a string", its null behaviour should be the null-string behaviour.

---

## 4. Defect reproductions

All in `build-probe/1823_prefix_defects.cpp`; log
`build-probe/1823_prefix_defects.log`. Compiled as a single translation unit
against the built archives (one process, so one job):

```
g++ -std=c++23 -fsanitize=address,undefined -g -O1 \
    -Imodules/core/include -Imodules/io/include -Imodules/console/include \
    -Imodules/text/include \
    build-probe/1823_prefix_defects.cpp -o build-probe/1823_prefix_defects \
    -Lbuild -lsharp_runtime_io -lsharp_runtime_core -lsharp_runtime_text
```

Distinct defects proven, with the ticket each belongs to:

| Defect | Cases | Ticket |
|---|---|---|
| `StreamReader` accepts a stream that declares itself unreadable and then launders it into an empty document | 6, 7 | **#1808** |
| `StreamWriter` accepts a stream that declares itself unwritable | 1, 3, 4, 5 | #1824 (blocked) |
| A `CanWrite` guard would reject a custom stream that works today | 8 | the reason #1824 is blocked |
| `FileStream::Write` silently discards data on a read-only handle | 5, 15, 16 | **#1825** |
| `StreamWriter::Write(const char*)` / `WriteLine(const char*)` crash on null | 22, 23 | **#1809** |
| `TextWriter::Write(const char*)` / `WriteLine(const char*)` throw `std::logic_error` on null | 20, 21, 24, 25 | **#1809** |
| `Console::Write(const char*)` / `WriteLine(const char*)` poison `std::cout` on null | 26, 27 | **#1809** |
| `MemoryStream::getCanReadProperty()` ignores `isOpen_` (.NET: `CanRead => _isOpen`) | 13 | #1826 (inactive) |

---

## 5. Compatibility matrix

"Narrowing" means an input accepted today is rejected afterwards. The decisive
question for this repository is **whether the narrowed input worked**: #1817 and
#1818 narrowed the accepted set and were classified compatible because the rejected
inputs were already producing wrong answers. The same test is applied here.

| Change | Rejects what works today? | Classification | Approval |
|---|---|---|---|
| `StreamReader` rejects `!getCanReadProperty()` | **No.** `Stream::getCanReadProperty()` defaults to `true`, so only a stream that *positively declares itself unreadable* is rejected, and such a stream can only ever produce `-1` / `""`. Cases 6/7. | compatible narrowing | **none** |
| `StreamWriter` rejects `!getCanWriteProperty()` | **Yes.** `Stream::getCanWriteProperty()` defaults to **`false`**, so every custom stream that implements `Write()` without overriding the property is rejected although it works — proven by case 8, `written="hello"`. | breaking; mandatory downstream migration | **required** |
| `TextWriter`/`StreamWriter`/`Console` treat a null `const char*` as .NET treats a null string | **No.** Every current behaviour for null is undefined behaviour: `SEGV`, `std::logic_error`, or permanent `badbit`. Non-null input is untouched. | compatible widening (defines the undefined) | **none** |
| `FileStream::Read`/`Write` validate the access flags | **No.** A write on a read-only handle never reached the file (case 15/16) and a read on a write-only handle always returned 0. | compatible narrowing | **none** |

The asymmetry between the two halves of #1808 is entirely caused by one line:

```cpp
// modules/io/include/System/IO/Stream.hpp:62,65
[[nodiscard]] virtual bool getCanWriteProperty() const { return false; }
[[nodiscard]] virtual bool getCanReadProperty()  const { return true; }
```

.NET's `Stream.CanRead`/`CanWrite` are **abstract** — a stream author must answer.
This port gave them defaults, and the default answer for writing is "no". There is
no way through this virtual to distinguish "declared unwritable" from "never
answered", so a `CanWrite` guard cannot be made compatible by being cleverer; it can
only be made compatible by changing the base default, which is a broader semantic
change than the guard itself. Both options are recorded in §13.2 for the approval
request.

---

## 6. Selected contracts

### 6.1 #1808 — `StreamReader` direction (selected, compatible)

`StreamReader::StreamReader(Stream* stream, bool leaveOpen)` gains, immediately
after the existing null check:

```cpp
if (!stream->getCanReadProperty())
    throw System::ArgumentException("Stream was not readable.");
```

Message and exception type copied from `StreamReader.cs:147` /
`Argument_StreamNotReadable`, and byte-identical to `BinaryReader.cpp:25`, which
already does this for the same input. No `paramName`, matching .NET.

`StreamReader(const std::string& path)` is unaffected in behaviour: it constructs
`FileStream(path, FileMode::Open, FileAccess::Read)`, whose `canRead_` is `true` by
construction. It is nevertheless routed through the same check so that a future
change to `FileStream` cannot silently reintroduce the gap.

### 6.2 #1809 — null `const char*` (selected, compatible)

.NET's rule, applied to the port's own pointer overloads:

| Surface | Null contract |
|---|---|
| `TextWriter::Write(const char*)` | write nothing, return normally |
| `TextWriter::WriteLine(const char*)` | write **only** the line terminator |
| `StreamWriter::Write(const char*)` | write nothing, return normally |
| `Console::Write(const char*)` | write nothing, return normally |
| `Console::WriteLine(const char*)` | write **only** `Console::NewLine` |

No exception is thrown anywhere, because .NET throws none. `StringWriter` and any
future subclass inherit the contract from `TextWriter` without writing their own
guard, because the null test happens **before** the virtual dispatch to
`Write(const std::string&)`. `StreamWriter::Write(const char*)`, which overrides,
needs its own test — that is the only override in the family.

The decision and its .NET citation go into the header doc-comment of each of the
five surfaces, as #1809's acceptance criteria requires.

### 6.3 #1824 — `StreamWriter` direction (designed, blocked)

Recorded here in full so the approval request is concrete; **not implemented**.

```cpp
// modules/io/src/System/IO/StreamWriter.cpp, in StreamWriter(Stream*, bool)
if (stream == nullptr) throw System::ArgumentNullException("stream");
if (!stream->getCanWriteProperty())
    throw System::ArgumentException("Stream was not writable.");
```

See §13.2.

### 6.4 #1825 — `FileStream` access flags (designed, compatible)

```cpp
// FileStream::Read, after the existing is_open() check
if (!canRead_)  throw System::NotSupportedException("Stream does not support reading.");
// FileStream::Write and FileStream::WriteByte, after the existing is_open() check
if (!canWrite_) throw System::NotSupportedException("Stream does not support writing.");
```

Order — closed first, then access — is `OSFileStreamStrategy.cs:208-217` and
`232-241` exactly. `MemoryStream` and `UnmanagedMemoryStream` in this repository
already throw this message for the same condition, so this makes `FileStream` the
last stream that does not.

---

## 7. Exact validation order

### 7.1 `StreamReader(Stream*, bool)` and, if approved, `StreamWriter(Stream*, bool)`

1. `stream == nullptr` → `ArgumentNullException("stream")` — existing, unchanged.
2. `!stream->getCanReadProperty()` / `!stream->getCanWriteProperty()` →
   `ArgumentException("Stream was not readable." / "Stream was not writable.")`.
3. member initialisation.

This is .NET's order (`StreamReader.cs:140-148`). It matters: a **null** stream must
report `ArgumentNullException`, not `ArgumentException`, and evaluating
`stream->getCanReadProperty()` first would dereference the null pointer that #1806
exists to reject. Because both checks throw from the constructor **body**, the
destructor never runs, so a rejected construction cannot close or delete anything —
the property #1806 pinned with a permanent test and which stays true here.

### 7.2 `FileStream::Read` / `Write` / `WriteByte` (#1825)

1. `!file_.is_open()` → `ObjectDisposedException` — existing, unchanged.
2. `!canRead_` / `!canWrite_` → `NotSupportedException`.
3. `buffer == nullptr` → `ArgumentNullException("buffer")` — existing.
4. `offset < 0`, `count < 0` → `ArgumentOutOfRangeException` — existing.
5. `count == 0` → return.
6. the I/O.

Steps 1 and 2 are .NET's order. Steps 3–5 are this port's existing order and are not
disturbed; note that a **closed** stream reports `ObjectDisposedException` even when
the buffer is also null, which is what both this port and .NET already do.

### 7.3 The null-text surfaces (#1809)

`value == nullptr` is tested **first**, before any string is formed, any `strlen` is
called and any virtual dispatch happens. There is nothing else to order against —
these methods have exactly one parameter.

---

## 8. Exact exception / no-op behaviour

| Surface | Condition | Result |
|---|---|---|
| `StreamReader(Stream*, bool)` | `stream == nullptr` | `System::ArgumentNullException`, `paramName = "stream"` |
| `StreamReader(Stream*, bool)` | `!CanRead` | `System::ArgumentException`, message `Stream was not readable.`, **no** `paramName` |
| `StreamWriter(Stream*, bool)` *(blocked)* | `!CanWrite` | `System::ArgumentException`, message `Stream was not writable.`, **no** `paramName` |
| `FileStream::Read` *(#1825)* | `!canRead_` | `System::NotSupportedException`, message `Stream does not support reading.` |
| `FileStream::Write`/`WriteByte` *(#1825)* | `!canWrite_` | `System::NotSupportedException`, message `Stream does not support writing.` |
| `TextWriter::Write(const char*)` | `value == nullptr` | **no-op**, no exception, no output |
| `TextWriter::WriteLine(const char*)` | `value == nullptr` | line terminator only |
| `StreamWriter::Write(const char*)` | `value == nullptr` | **no-op** |
| `Console::Write(const char*)` | `value == nullptr` | **no-op**; `std::cout` must stay `good()` |
| `Console::WriteLine(const char*)` | `value == nullptr` | `Console::NewLine` only; `std::cout` must stay `good()` |

`HResult` is not modelled by this port's exception hierarchy (there is no
`getHResultProperty()` on `System::Exception`), so no `HResult` can be pinned; the
exception **type**, the **message** and, where .NET sets one, the **parameter name**
are the full observable contract and are all pinned by the test matrix in §10.

---

## 9. Implementation ticket split and dependency order

| Ticket | Scope | Class | Depends on |
|---|---|---|---|
| **#1823** *(this document)* | design only | design | — |
| **#1809** | null `const char*` across `TextWriter`, `StreamWriter`, `Console` | compatible implementation | #1823 |
| **#1808** | `StreamReader` direction guard; `StreamWriter` half inventoried and split out | compatible implementation | #1823 |
| **#1824** | `StreamWriter` direction guard | **approval-blocked implementation** | #1823, explicit user approval |
| **#1825** | `FileStream::Read`/`Write`/`WriteByte` access-flag validation | compatible implementation | #1823; ordered **after** #1808 so the two diagnostics are measured separately |
| **#1826** | `MemoryStream::getCanReadProperty()` ignores `isOpen_` | inactive, newly discovered | — |

**#1809 goes first** because it is confined to five method bodies with no
constructor interaction, so its regressions cannot be perturbed by the constructor
work. **#1808 goes second**; **#1825 third**, because #1825 changes what a
write-only `FileStream` does to a `StreamReader` that #1808 now rejects earlier, and
running them in the other order would make it ambiguous which guard produced a given
verdict.

### 9.1 Amendment to #1808's own acceptance criteria, and why

#1808 was written as one ticket covering both directions, and its acceptance
criteria ask that "the `CanRead`/`CanWrite` state of every stream type this
repository can pass is inventoried first, **so the check cannot reject a stream that
is in fact usable**". That inventory has now been done, and it shows the writer
check **would** reject a stream that is in fact usable (case 8). The ticket's own
criterion therefore forbids implementing the writer half as specified. #1808 is
consequently rescoped to the reader half plus the completed inventory, and the
writer half is carried by the new blocked ticket #1824 rather than being quietly
dropped or quietly landed. The rescoping is recorded in the ticket's `notes`, in
`NEXT.md`, and here.

---

## 10. Test matrix

All permanent, all in `modules/io/tests/System/IO/IOStreamTests.cpp` except the
`Console` rows, which go in `modules/console/tests/System/Batch10ConsoleTests.cpp`.

### 10.1 #1809

| Case | Surface | Expectation |
|---|---|---|
| null via `TextWriter&` | `Write(const char*)` on `StringWriter` | no throw, output `""` |
| null via `TextWriter&` | `WriteLine(const char*)` on `StringWriter` | no throw, output is exactly the line terminator |
| null, static dispatch | `StringWriter::Write` / `WriteLine` | same two results |
| null | `StreamWriter::Write(const char*)` | no throw, stream length 0 |
| null | `StreamWriter::WriteLine(const char*)` | no throw, stream contains exactly the line terminator |
| null | `Console::Write` / `Console::WriteLine` | no throw **and** `std::cout.good()` afterwards |
| empty `""` | each of the five surfaces | unchanged from today: nothing, or terminator only |
| ordinary `"abc"` | each of the five surfaces | unchanged from today |
| cross-type | `StringWriter` and `StreamWriter` answer a null identically | same observable output |
| ordering | `Write(nullptr)` between two ordinary writes | the two ordinary writes are contiguous in the output |

### 10.2 #1808

| Case | Expectation |
|---|---|
| `StreamReader` over `FileStream(path, Append)` (`CanRead=0`) | `ArgumentException` |
| …its message | exactly `Stream was not readable.` |
| …no `(Parameter '…')` suffix | message contains no `Parameter '` |
| `StreamReader` over a `MemoryStream` | still constructs and reads (valid path) |
| `StreamReader` over a custom stream that never overrides `getCanReadProperty()` | still constructs and reads — the compatibility case |
| `StreamReader(nullptr)` | still `ArgumentNullException`, **not** `ArgumentException` — order pinned |
| `StreamReader(path)` | unchanged |
| rejected construction leaves a neighbouring live stream untouched | no close, no delete |
| `leaveOpen` true and false on the valid path | unchanged |
| `StreamReader` and `BinaryReader` answer the same unreadable stream identically | same type, same message |

### 10.3 #1825

| Case | Expectation |
|---|---|
| `FileStream(path, Open, FileAccess::Read)::Write` | `NotSupportedException`, `Stream does not support writing.` |
| `FileStream(path, Open, FileAccess::Read)::WriteByte` | same |
| `FileStream(path, Append)::Read` | `NotSupportedException`, `Stream does not support reading.` |
| closed **and** unwritable | `ObjectDisposedException` wins — order pinned |
| the valid read and write paths | unchanged |
| the file contents after a rejected write | unchanged, and now the caller is told |

### 10.4 Not weakened

No existing assertion is relaxed. The 552 `SharpRuntimeTests_IO` cases and 123
`SharpRuntimeTests_Console` cases measured on 2026-07-29 before this work are the
floor; each ticket states its own new total.

---

## 11. Sanitizer strategy

| Ticket | ASan | UBSan | LSan | TSan |
|---|---|---|---|---|
| #1809 | **yes** — the defect is a null read in `strlen` (case 22/23); ASan is what proves it is gone | **yes** — null pointer use and the `std::string` construction path | **yes** — a no-op must not leak the buffer it declined to write | no |
| #1808 | yes — a rejected construction must not touch the stream it refused | yes | **yes** — throwing from a constructor body must not leak partially initialised members | no |
| #1825 | yes | yes | yes | no |

TSan is **not** run: nothing in this family touches shared mutable state. `Console`'s
statics are written only by colour/cursor setters that these tickets do not touch,
and no member added by any of these tickets is mutable at all.

Activation is proven the way #1818 proved it — the runtime's own `ASAN_OPTIONS=help=1`
banner plus a symbol count of `__asan`/`__ubsan` interceptors in the binary — rather
than assumed from the compiler flags.

---

## 12. Public and ABI impact

| Ticket | Public signature | Virtual / vtable | Object layout | Mangled symbols | Consumer rebuild |
|---|---|---|---|---|---|
| #1809 | unchanged | unchanged — no virtual added, removed or reordered; the five bodies change only | unchanged — no member added | unchanged; `TextWriter::Write(const char*)` and `WriteLine(const char*)` stay `inline virtual` in the header | not required on its account |
| #1808 | unchanged | unchanged | unchanged | unchanged | not required |
| #1825 | unchanged | unchanged | unchanged | unchanged | not required |
| #1824 *(blocked)* | unchanged | unchanged | unchanged | unchanged | **not** an ABI break — but a **source-compatible, behaviour-breaking** change: code that compiles today stops working at run time |

#1824 is the case the approval boundary exists for. It is not a layout or signature
change; it is a *semantic* one with mandatory downstream migration, which the batch
rules list explicitly.

Note for #1809: `TextWriter::Write(const char*)` and `WriteLine(const char*)` are
`inline` in a public header, so a consumer that does not rebuild keeps the old
inlined body. That is a *stale-inline* consideration, not an ABI break, and it is
benign in this direction — the old body only ever crashed on the input the new one
handles.

---

## 13. Explicit approval requirements

### 13.1 Needing no approval

#1809, #1808 (reader half) and #1825. None changes a public signature, a virtual,
a return convention, an object or iterator layout, or forces a downstream migration.
Each either defines behaviour that was undefined, or rejects an input that was
already producing a wrong answer.

### 13.2 Needing approval — #1824

**The change requested:**

```cpp
// modules/io/src/System/IO/StreamWriter.cpp
StreamWriter::StreamWriter(Stream* stream, bool leaveOpen)
    : stream_(stream), leaveOpen_(leaveOpen), ownsStream_(false)
{
    if (stream == nullptr) throw System::ArgumentNullException("stream");
    if (!stream->getCanWriteProperty())
        throw System::ArgumentException("Stream was not writable.");   // NEW
}
```

**What breaks.** Any `Stream` subclass that overrides `Write(const bytecs*, intcs,
intcs)` but not `getCanWriteProperty()` is silently unwritable by declaration and
writable in fact. Such a stream is accepted today and writes correctly (case 8) and
would be rejected afterwards, at construction, with no way for the caller to
proceed. The in-repository test double `FlushTrackingStream` avoided this only
because it happens to override the property; a stream author has no reason to
suspect the default is `false`.

**Alternatives considered:**

| Option | Effect | Verdict |
|---|---|---|
| **A. Add the guard as .NET has it** | exact parity; breaks every undeclared-writable custom stream | needs approval; **recommended** if the migration is acceptable |
| **B. Do nothing** | silent data loss on read-only `FileStream` persists | rejected — but note #1825 removes the *silent* half without approval |
| **C. Change `Stream::getCanWriteProperty()`'s default to `true`** | makes A compatible, but makes every stream claim to be writable, weakens the already-correct `BinaryWriter` guard, and diverges further from .NET (where the property is abstract) | rejected |
| **D. Make `getCanRead/WriteProperty()` pure virtual, as .NET has them** | true parity and removes the ambiguity permanently; but every `Stream` subclass everywhere must implement both | needs a **larger** approval than A; recorded as the principled end state |

**Migration if A is approved:** a rejected stream is fixed by one line —
`[[nodiscard]] bool getCanWriteProperty() const override { return true; }`. The
diagnostic is `ArgumentException: Stream was not writable.` at the construction
site, which names the fix unambiguously.

**Rollback:** delete the two added lines and the tests that assert them. No data
migration, no persisted state, no layout change.

**Performance:** one virtual call per `StreamWriter` construction. Not measurable.

### 13.3 Deliberately not in scope

`SR-AUD-337` (`Close()` with `leaveOpen=true` leaves the wrapper usable; neither
wrapper has a disposed flag) shares these two files and stays `confirmed`,
untouched, exactly as #1806 left it. Adding a disposed flag changes object layout
and is a separate ticket with its own approval question.

---

## 14. Dependency order between #1808 and #1809

They touch overlapping headers but **disjoint members**: #1809 changes only
`Write(const char*)` and `WriteLine(const char*)` bodies; #1808 changes only
`StreamReader`'s `Stream*` constructor. There is no textual conflict and no semantic
coupling, so either could be first on correctness grounds.

**#1809 is sequenced first** for evidence hygiene: its reproduction is a crash, and a
crash is easiest to attribute when no constructor in the same family has just
changed which objects can exist. #1808 then lands against a family whose null-text
behaviour is already pinned, so any new failure in its run belongs to it.

`#1825` follows `#1808` for the same reason, stated in §9.

---

## 15. Status

| Ticket | Status at the close of #1823 | Status now |
|---|---|---|
| #1823 | done (this document) | done |
| #1809 | ready — compatible, no approval | **done** — §6.2 landed unchanged |
| #1808 | ready — compatible, no approval, rescoped per §9.1 | **done** — §6.1 landed unchanged |
| #1824 | **blocked** on the approval in §13.2 | **still blocked** — §13.2 approval not given |
| #1825 | ready — compatible, no approval | **done** — §6.4/§7.2 landed unchanged |
| #1826 | inactive — newly discovered, no `SR-AUD-*` identifier | ready — see §15.1 |

No `SR-AUD-*` identifier was issued by this ticket. The audit numbering stays frozen
at 364, and `SR-AUD-337` and `SR-AUD-338` keep the statuses they had before it.

### 15.1 One correction found while implementing #1825

§6.4 and §7.2 described `WriteByte` as sharing `Write`'s `is_open()` check, following
#1825's own description. It does not: before #1825 `WriteByte` had **no validation at
all**, so a byte written to a *closed* `FileStream` was accepted in silence
(`build-probe/1825_prefix_defects.log` case 4 — a case this document did not predict).
The selected contract is unchanged, because §7.2 step 1 already required the
`is_open()` check to precede the access check; the correction is that for `WriteByte`
step 1 was **added** rather than merely reordered against. .NET has no equivalent gap:
`OSFileStreamStrategy.cs:226-227` defines `WriteByte` in terms of
`Write(ReadOnlySpan<byte>)`, so it inherits both checks.

This is recorded by appending, not by rewriting §6.4, so that what the design believed
and what the implementation measured stay separately readable.
