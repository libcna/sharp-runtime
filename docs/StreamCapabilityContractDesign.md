<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# The `System::IO::Stream` capability contract

**Ticket #1839** (`DESIGN-IO-STREAM-CAPABILITY-CONTRACT`, P2, size M, **design-only**).
Written 2026-07-30. **No production source changed under this ticket.**

This document exists because three separate remediation tickets — **#1824**
(`StreamWriter`), **#1827** (`ZipArchive` capabilities) and **#1828** (the three zlib
wrappers) — are blocked by **one line**, and fixing them one at a time would apply four
guards to a base class whose contract nobody had written down. It issues **no `SR-AUD-*`
identifier**; audit numbering stays frozen at **364** and no finding changes status.

It **supersedes** `docs/TextWrapperInputContractPlan.md` §13.2 as the place the
Stream-capability approval question is stated. §13.2 is left exactly as written.

---

## 1. The one line, and the two lines nobody mentioned

`modules/io/include/System/IO/Stream.hpp`:

```cpp
/** Returns true if this stream supports writing. */
[[nodiscard]] virtual bool getCanWriteProperty() const { return false; }   // line 62

/** Returns true if this stream supports reading. */
[[nodiscard]] virtual bool getCanReadProperty()  const { return true; }    // line 65

/** Returns true if this stream supports seeking. */
[[nodiscard]] virtual bool getCanSeekProperty() const { return false; }    // line 79
```

.NET's `Stream.cs:29-31`:

```csharp
public abstract bool CanRead  { get; }
public abstract bool CanWrite { get; }
public abstract bool CanSeek  { get; }
```

The three blocked tickets all describe the problem as "`getCanWriteProperty()` defaults to
`false`". **That is only a third of the divergence, and stating it that way is what made the
problem look like a `CanWrite` problem.** The three capabilities have **three different
defaults**, none of them documented as a decision:

| Capability | This port | .NET | Consequence of the default being wrong |
|---|---|---|---|
| `getCanWriteProperty()` | **`false`** | abstract | a writable custom stream **lies that it cannot write** → any `CanWrite` guard rejects a working stream |
| `getCanReadProperty()` | **`true`** | abstract | an unreadable custom stream **lies that it can read** → a `CanRead` guard *passes* something it should reject |
| `getCanSeekProperty()` | **`false`** | abstract | a seekable custom stream lies that it cannot seek → a `CanSeek` guard rejects it, **and** `Seek`/`Position` still work because the default `Seek` is expressed in terms of `Position` |

The two failure directions are **opposite**, which matters for the choice of repair: the
`CanWrite` default makes guards **over-reject**, and the `CanRead` default makes guards
**under-reject**. A design that only fixes `CanWrite` leaves the base class asymmetrically
wrong in a way the next ticket will hit from the other side. That is why #1808's
`StreamReader` `CanRead` guard could ship without approval and #1824's `CanWrite` guard
cannot: they are not the same change with the direction swapped, they are guards against
defaults that fail differently.

`Stream.hpp`'s own class doc-comment says *"Derived classes must implement `Read()`,
`Close()`, and `getLengthProperty()`; `Write()` and `WriteByte()` may optionally be
overridden."* It says **nothing** about the three capability properties. A stream author has
no reason to suspect that omitting `getCanWriteProperty()` declares the stream unwritable.

---

## 2. Inventory — every `Stream` implementation

Measured 2026-07-30 by reading every class that derives from `System::IO::Stream`.
**"inherited"** means the class does not override and takes the base default.

### 2.1 Production implementations (nine, plus one derived)

| Class | Header | `CanRead` | `CanWrite` | `CanSeek` |
|---|---|---|---|---|
| `FileStream` | `io/…/FileStream.hpp:97-104` | `canRead_` | `canWrite_` | `file_.is_open()` |
| `MemoryStream` | `io/…/MemoryStream.hpp:99-137` | `isOpen_` | `writable_` | `isOpen_` |
| `UnmanagedMemoryStream` | `io/…/UnmanagedMemoryStream.hpp:70-74` | `isOpen_ && (access_ & Read)` | `isOpen_ && (access_ & Write)` | `isOpen_` |
| `BufferedStream` | `io/…/BufferedStream.hpp:101-105` | `!closed_ && inner_->CanRead` | `!closed_ && inner_->CanWrite` | `!closed_ && inner_->CanSeek` |
| `NetworkStream` | `net-sockets/…/NetworkStream.hpp:43-44` | `fd_ >= 0` | `fd_ >= 0` | **inherited `false`** |
| `DeflateStream` | `io-compression/…/DeflateStream.cpp:83-84` | `mode_ == Decompress` | `mode_ == Compress` | **inherited `false`** |
| `GZipStream` | `io-compression/…/GZipStream.cpp:77-78` | `mode_ == Decompress` | `mode_ == Compress` | **inherited `false`** |
| `ZLibStream` | `io-compression/…/ZLibStream.cpp:77-78` | `mode_ == Decompress` | `mode_ == Compress` | **inherited `false`** |
| `ZipEntryWriteStream` (internal) | `io-compression-zip/…/ZipArchive.cpp:44-45` | `false` | `true` | **inherited `false`** |
| `IsolatedStorageFileStream` | `io-isolated-storage/…` | inherits `FileStream`'s | inherits | inherits |

**The single most important measured fact in this document:** **every production `Stream`
overrides both `getCanReadProperty()` and `getCanWriteProperty()`.** Not one relies on
either default. Only `getCanSeekProperty()` has production users of its default — five of
them, all of which genuinely cannot seek, so the default happens to be *right* for all five.

That reverses the intuition the three blocked tickets were written under. Making
`CanRead`/`CanWrite` **pure virtual** costs **zero** production edits. The obstacle is
entirely outside production code.

### 2.2 Test doubles (five)

| Class | File | `CanRead` | `CanWrite` | `CanSeek` |
|---|---|---|---|---|
| `UnreadableTestStream` | `io/tests/…/IOStreamTests.cpp:1352` | `false` | inherited | inherited |
| `UndeclaredReadableTestStream` | `io/tests/…/IOStreamTests.cpp:1364` | **inherited `true`** | inherited | inherited |
| `FlushTrackingStream` | `io/tests/…/IOStreamTests.cpp:627` | inherited | `true` | inherited |
| `NonSeekableReadStream` | `io/tests/…/IOStreamTests.cpp:639` | inherited | inherited | inherited |
| `NonSeekableTestStream` | `io/tests/…/StreamTests.cpp:661` | inherited | inherited | inherited |
| `ThrowingWriteStream` | `tests/integration/…/CompressionTests.cpp:74` | inherited | **inherited `false`** — overrides `Write()` | inherited |

`UndeclaredReadableTestStream` was written **on purpose** by #1808 as the compatibility
witness: a stream that reads and never mentions the property. `ThrowingWriteStream` is its
accidental write-direction twin, and it is the one that actually breaks (§4).

`NonSeekableTestStream` is a *deliberate* consumer of the `CanSeek == false` default —
`StreamTests.DefaultCanSeekIsFalse` asserts it. Making `CanSeek` pure virtual therefore
does not merely require an edit there; it **deletes the thing that test is testing**.

### 2.3 Non-`Stream` types with the same-named properties

`UnmanagedMemoryAccessor` (`io/…/UnmanagedMemoryAccessor.hpp:62-64`) has
`getCanReadProperty()`/`getCanWriteProperty()` that are **not virtual and not part of this
hierarchy**. It is out of scope and must not be swept in by a mechanical change.

`StringReader`/`StringWriter`/`TextReader`/`TextWriter` are not `Stream`s and have no
capability properties. There is **no `CryptoStream` in this repository** — symmetric
cryptography is an explicit permanent deviation (`CLAUDE.md`), so the "crypto streams" row
of the inventory brief is **empty by design**, not by omission.

---

## 3. Inventory — every consumer of the three capabilities

### 3.1 Wrappers that validate at construction

| Wrapper | Validates | Exception and message |
|---|---|---|
| `BinaryReader` (`BinaryReader.cpp:23-25`) | null, then `!CanRead` | `ArgumentException("Stream was not readable.")` |
| `BinaryWriter` (`BinaryWriter.cpp:17-19`) | null, then `!CanWrite` | `ArgumentException("Stream was not writable.")` |
| `StreamReader` (`StreamReader.cpp:57`) | null, then `!CanRead` — added by **#1808** | `ArgumentException("Stream was not readable.")` |
| `StreamWriter` (`StreamWriter.cpp:28`) | **null only** | — this is **#1824**'s gap |
| `ZipArchive` (`ZipArchive.cpp:398-401`) | null, then mode **range** — added by **#1813** | `ArgumentOutOfRangeException("mode")`; capabilities **not** checked — **#1827**'s gap |

So the repository is already **inconsistent between siblings**: `BinaryWriter` rejects an
undeclared-writable stream and `StreamWriter` accepts it, for the identical input. Whatever
this design selects, that inconsistency is the thing to remove — in one direction or the
other.

### 3.2 First-operation and mid-stream consumers

| Site | Uses | Purpose |
|---|---|---|
| `BinaryReader.cpp:210` | `!CanSeek` → throw | `PeekChar` needs to rewind |
| `BinaryReader.cpp:324, 357` | `CanSeek` → branch | bounded vs unbounded length handling |
| `BufferedStream.cpp:63, 159-164, 183, 216-218, 232` | inner's three | buffering, flush, seek and `SetLength` policy |
| `UnmanagedMemoryStream.cpp:62, 77, 116` | **own** `CanRead`/`CanWrite` → `NotSupportedException("Stream does not support reading."/"…writing.")` | per-operation access enforcement |
| `FileStream` `Read`/`Write`/`WriteByte` | own `canRead_`/`canWrite_` — added by **#1825** | per-operation access enforcement |
| `ZipArchive.cpp:520` | `stream->CanSeek` → branch | archive read strategy |

Note the pattern already present twice: `UnmanagedMemoryStream` and `FileStream` enforce
capability **per operation**, throwing `NotSupportedException`, which is what .NET's
`Stream` does for an unsupported operation. That is a real, already-shipped precedent for
option 5 in §5.

### 3.3 Async paths

There are **none to consider**. This port's `Stream` has no `ReadAsync`/`WriteAsync`/
`FlushAsync`/`CopyToAsync`/`DisposeAsync` members at all — the surface is
`Read`/`Write`/`WriteByte`/`ReadByte`/`Seek`/`SetLength`/`Flush`/`Close` plus the five
properties. So the sync/async split that would double the migration surface in .NET does
not exist here. Recorded explicitly so a future reader does not go looking.

### 3.4 Disposal and `leaveOpen`

| Type | Closed-state signal | `CanRead`/`CanWrite` reflect it? |
|---|---|---|
| `MemoryStream` | `isOpen_` | **yes**, since #1826 |
| `FileStream` | `file_.is_open()` | `CanSeek` yes; `CanRead`/`CanWrite` are `canRead_`/`canWrite_`, which do **not** fold in openness |
| `BufferedStream` | `closed_` | **yes**, all three |
| `UnmanagedMemoryStream` | `isOpen_` | **yes**, all three |
| `NetworkStream` | `fd_ >= 0` | **yes** |
| `DeflateStream`/`GZipStream`/`ZLibStream` | `state_->initialized` (set `true` at end of constructor, `false` in `Close()`) and `inner_ == nullptr` when `leaveOpen == false` | **no** — this is **#1828**'s first half |
| `ZipEntryWriteStream` | none | `CanWrite` is a literal `true` |

**A finding this inventory produced, and it unblocks half of #1828 for free:** the zlib
wrappers **already have a usable closed-state signal**. `state_->initialized` is set at the
end of the constructor and cleared in `Close()` before the flush loop that can throw
(`DeflateStream.cpp:200`), and `inner_` is nulled on a non-`leaveOpen` close
(`DeflateStream.cpp:190`). So `#1828`'s disposed-state half needs **no new member, no
object-layout change and therefore no layout approval** — contrary to the assumption that
made it look like `SR-AUD-337`'s disposed-flag problem. With `leaveOpen == true`, `inner_`
survives the close, which is exactly why `initialized` and not `inner_` is the signal to
test first.

`FileStream`'s `canRead_`/`canWrite_` **not** folding in `is_open()` is the same defect
#1826 fixed in `MemoryStream`, in a different type. It is **not** part of #1824/#1827/#1828
and is recorded here as newly discovered (§9).

---

## 4. The migration cost, measured rather than estimated

Estimating the migration surface is what kept these three tickets blocked. It was measured
instead, by **applying all three candidate guards at once, building, and running the whole
repository gate**. The patch is retained at
`build-probe/1839_capability_experiment.patch`; the run is
`build-probe/1839_experiment_full.log`. The tree was reverted afterwards and rebuilt clean.

The experiment applied:

1. `StreamWriter`: `if (!stream->getCanWriteProperty()) throw ArgumentException("Stream was not writable.");` after the null check — #1824's §13.2 proposal verbatim.
2. `ZipArchive`: `ZipArchive.cs:962-975`'s capability switch, after #1813's range check, on the `Stream*` constructor only (the path-taking constructor has no caller-supplied stream).
3. All three zlib wrappers: `DeflateStream.cs:171-195`'s shape — `false` when closed, else mode **and** the inner stream's matching capability.

**Result: exactly two tests fail, out of 14,139 across 37 executables.**

```
[  FAILED  ] ZipArchiveTests.Dispose_StreamWriteThrows_Propagates
[  FAILED  ] ZipArchiveTests.Destructor_StreamWriteThrows_DoesNotTerminateProcess
```

Both are in `tests/integration/System/IO/Compression/CompressionTests.cpp`, both construct
`ZipArchive(&inner, ZipArchiveMode::Create)` over `ThrowingWriteStream` — a double that
overrides `Write()` and **not** `getCanWriteProperty()`. They are the predicted
compatibility shape, caught in the wild, inside this repository.

What the measurement establishes, and none of it was previously known:

- **`StreamWriter`'s `CanWrite` guard breaks nothing at all.** Every in-repository
  `StreamWriter(Stream*)` site wraps a `MemoryStream` or a `FileStream`, both of which
  override the property. #1824's blocker is **not** an in-repository migration; it is purely
  a hypothetical-external-stream concern.
- **The zlib closed-state-plus-delegation change breaks nothing at all.** #1828's
  supposedly approval-needing half is compatible against the current tree.
- **`ZipArchive`'s capability guard breaks exactly two tests**, both fixable by adding one
  line to one test double.

So the total measured in-repository migration for **all three** blocked tickets is **one
line in one test double**:

```cpp
[[nodiscard]] bool getCanWriteProperty() const override { return true; }
```

That does not make the change compatible for *external* consumers — CNA and mobile-eggbert
were **not inspected**, per the batch boundary, and a downstream stream that omits the
override would still be rejected. But it removes the "large unknown migration" argument from
the decision entirely and replaces it with a known, one-line one inside this repository.

---

## 5. The six candidate contracts

Each is judged against: parity with .NET, whether it fixes **both** failure directions of
§1, source compatibility, vtable/ABI impact, and whether it needs approval.

### Option 1 — make all three capabilities pure virtual

```cpp
[[nodiscard]] virtual bool getCanReadProperty()  const = 0;
[[nodiscard]] virtual bool getCanWriteProperty() const = 0;
[[nodiscard]] virtual bool getCanSeekProperty()  const = 0;
```

- **Parity:** exact. This is `Stream.cs:29-31`.
- **Fixes both directions:** yes, permanently, by making the question unanswerable-by-accident.
- **Source compatibility:** **breaking.** Every `Stream` subclass anywhere must implement
  three members. In-repository: **zero** production edits for `CanRead`/`CanWrite` (§2.1),
  **five** production edits for `CanSeek`, **six** test-double edits, and it **destroys**
  `StreamTests.DefaultCanSeekIsFalse`, which exists to assert the default.
- **vtable/ABI:** the slots already exist and keep their order; making them pure does not
  change the vtable layout, but it **does** change whether `Stream` can be instantiated and
  changes the emitted `__cxa_pure_virtual` entries for any subclass that omitted one. Any
  downstream subclass fails to **compile**, which is the good failure mode.
- **Verdict:** the **principled end state** (as §13.2 option D already said), and the
  measurement now shows two-thirds of it is nearly free. But it is the **largest** approval
  and it deletes an existing test's subject.

### Option 2 — keep defaults, change them

`getCanWriteProperty()` → `true`, `getCanReadProperty()` → keep `true`,
`getCanSeekProperty()` → keep `false`.

- **Parity:** worse than today. No .NET stream claims a capability it may not have.
- **Fixes both directions:** **no.** It fixes over-rejection and makes under-rejection
  *worse*: every stream would then claim to be writable, so `BinaryWriter`'s
  already-correct guard stops catching anything.
- **Verdict:** **rejected**, for the reason §13.2 option C gave, which the inventory
  confirms: it weakens two guards that already work.

### Option 3 — validate the concrete stream type instead of the capability

e.g. `dynamic_cast<FileStream*>` / `MemoryStream*` and check their real access flags.

- **Parity:** none. .NET has no such notion.
- **Fixes both directions:** no; it just moves the guess.
- **Extensibility:** actively hostile — a correct third-party stream can never be
  recognised, which is worse than the default it replaces.
- **Verdict:** **rejected.** Recorded because the brief asked for it to be evaluated, and
  because a future reader may be tempted by it as the "compatible" option. It is compatible
  only by being unable to enforce anything.

### Option 4 — constructor-time capability probe

Detect writability by attempting a zero-length `Write` (or catching
`NotSupportedException` from a trial operation).

- **Parity:** none.
- **Correctness:** unsound. A zero-length `Write` on a genuinely unwritable stream is not
  required to throw, `Stream::Write`'s default *does* throw `NotSupportedException`
  regardless of the property, and probing a network or pipe stream has **side effects on
  the wire**. It also cannot probe read direction without consuming a byte.
- **Verdict:** **rejected as unsound**, not merely undesirable.

### Option 5 — first-operation validation instead of construction-time

Let the wrapper construct, and throw `NotSupportedException` from the first `Write`.

- **Parity:** partial. .NET validates at construction for `StreamWriter`/`BinaryWriter`, but
  .NET's own `Stream` **does** throw `NotSupportedException` per operation, and **this
  repository already does exactly that twice** — `UnmanagedMemoryStream.cpp:62/77/116` and
  `FileStream` since #1825 (§3.2).
- **Fixes both directions:** yes, and it is the only option that is *inherently* immune to
  the default being wrong, because it tests the stream's **behaviour** rather than its
  **declaration**.
- **Source compatibility:** **fully compatible.** An undeclared-writable stream keeps
  working, because nothing consults the property. A genuinely unwritable stream fails at
  first write instead of at construction — later, but with a correct exception rather than
  silent data loss.
- **Cost:** diverges from .NET on *when* and *which exception*. A caller that catches
  `ArgumentException` at construction would need to catch `NotSupportedException` at first
  use instead.
- **Verdict:** the **compatible fallback**, and the right answer for any wrapper where
  construction-time rejection cannot be approved.

### Option 6 — additive capability declaration / traits

Add a new non-virtual `StreamCapabilities` descriptor, or a `declaredCapabilities()` hook
defaulting to "unknown", and have guards reject only a **positively declared** absence.

- **Parity:** none, but it is *additive*: nothing existing changes meaning.
- **Fixes both directions:** yes — "unknown" is distinguishable from "no", which is exactly
  the information the current `bool` defaults destroy.
- **Source compatibility:** **fully compatible.** Old streams report "unknown" and are
  accepted; new streams can declare, and are enforced.
- **Cost:** a **new public member** on `Stream` (a vtable slot if virtual) and a second
  parallel notion of capability alongside the three `bool`s, which is a permanent
  API-surface cost for a transitional problem. It is also the option most likely to rot,
  because nothing forces a stream author to use it.
- **Verdict:** **rejected as over-engineering**, on the same reasoning
  `docs/DefinedArithmeticBoundaryPlan.md` §4 used to reject a `SafeArithmetic` helper: a new
  mechanism must earn its permanence, and this one is a workaround for three guards.

---

## 6. The selected contract

**Two layers, adopted together, splitting the problem exactly where the approval boundary
falls.**

### 6.1 Layer 1 — compatible now, no approval (tickets #1840, #1841)

**(a) Document the defaults as a decision, in `Stream.hpp` itself.** The class
doc-comment lists which members a subclass must implement and omits all three capability
properties. It must say, for each, what the default is and what omitting the override
declares. This is a doc-only change and it is the cheapest thing in this document with the
highest value: the entire family of bugs exists because a stream author could not know.

**(b) Fix the capabilities that are wrong about their own object's state.** This needs no
approval because it does not consult any *other* object's declaration — it corrects a stream
lying about **itself**:

- the three zlib wrappers return `false` for both capabilities once closed, using the
  **existing** `state_->initialized` signal, with **no new member and no layout change**
  (§3.4), matching `DeflateStream.cs:171-195`'s `_stream == null` arm — **#1828's first
  half, measured compatible in §4**;
- whether they also **delegate** to the inner stream's capability is Layer 2, because
  delegation *does* consult another object's possibly-defaulted declaration.

### 6.2 Layer 2 — needs one explicit approval, stated once (tickets #1824, #1827, #1828b)

**The single approval requested, in one sentence:**

> Adopt .NET's rule that a `Stream` subclass which does not override
> `getCanWriteProperty()` is **unwritable**, accepting that a custom stream which
> implements `Write()` without overriding the property will start being **rejected at
> construction** by `StreamWriter`, by `ZipArchive` in `Create`/`Update` mode, and by the
> zlib wrappers' delegated `CanWrite`; the fix for such a stream is one line.

If that is approved, all three blocked tickets unblock at once, with these exact
declarations:

```cpp
// modules/io/src/System/IO/StreamWriter.cpp — ticket #1824
StreamWriter::StreamWriter(Stream* stream, bool leaveOpen)
    : stream_(stream), leaveOpen_(leaveOpen), ownsStream_(false)
{
    if (stream == nullptr) throw System::ArgumentNullException("stream");
    if (!stream->getCanWriteProperty())
        throw System::ArgumentException("Stream was not writable.");
}
```

```cpp
// modules/io-compression-zip/src/System/IO/Compression/ZipArchive.cpp — ticket #1827
// ZipArchive.cs:962-975, verbatim messages from Strings.resx.
static void validateZipArchiveCapabilities(System::IO::Stream* stream, ZipArchiveMode mode)
{
    switch (mode) {
    case ZipArchiveMode::Create:
        if (!stream->getCanWriteProperty())
            throw System::ArgumentException("Cannot use create mode on a non-writable stream.");
        break;
    case ZipArchiveMode::Read:
        if (!stream->getCanReadProperty())
            throw System::ArgumentException("Cannot use read mode on a non-readable stream.");
        break;   // an unseekable Read-mode stream is NOT rejected — see below
    case ZipArchiveMode::Update:
        if (!stream->getCanReadProperty() || !stream->getCanWriteProperty()
            || !stream->getCanSeekProperty())
            throw System::ArgumentException(
                "Update mode requires a stream with read, write, and seek capabilities.");
        break;
    default: break;   // the range is #1813's guard, which runs first
    }
}
```

```cpp
// modules/io-compression/src/System/IO/Compression/{Deflate,GZip,ZLib}Stream.cpp — #1828
// DeflateStream.cs:171-195. Layer 1 is the first line; Layer 2 is the `inner_->…` conjunct.
bool DeflateStream::getCanReadProperty() const {
    if (!state_ || !state_->initialized || inner_ == nullptr) return false;
    return mode_ == CompressionMode::Decompress && inner_->getCanReadProperty();
}
bool DeflateStream::getCanWriteProperty() const {
    if (!state_ || !state_->initialized || inner_ == nullptr) return false;
    return mode_ == CompressionMode::Compress && inner_->getCanWriteProperty();
}
```

**Two decisions inside #1827 that the approval does not cover and that ticket must make:**

- **The `Update`-mode `CanSeek` requirement is the harshest clause in the whole design**,
  because `getCanSeekProperty()` defaults to `false` **and** five production streams rely on
  that default. Any custom stream must override `CanSeek` too, not only `CanWrite`.
- **A `Read`-mode unseekable stream must not be rejected.** .NET sets
  `isReadModeAndUnseekable = true` and buffers (`ZipArchive.cs:968-971`, plus
  `DecideArchiveStream`'s `PositionPreservingWriteOnlyStreamWrapper` for `Create`). #1827
  must either implement that buffering or document the deviation explicitly. **Rejecting it
  is not an option** — it is neither what .NET does nor what this port does today.

### 6.3 Layer 3 — recorded, deliberately not proposed

Making the three capabilities **pure virtual** (option 1) remains the principled end state
and is **not** proposed here. It needs a strictly larger approval than Layer 2, it deletes
`StreamTests.DefaultCanSeekIsFalse`'s subject, and Layer 2 delivers the parity that the
three blocked tickets actually need. Revisit it only if a *fourth* capability-guard ticket
appears — at that point the per-ticket approvals cost more than the one-time break.

---

## 7. Behaviour for legacy custom streams

| Stream shape | Today | After Layer 1 | After Layer 2 |
|---|---|---|---|
| overrides `Write()`, not `CanWrite` | works everywhere except `BinaryWriter` | unchanged | **rejected** by `StreamWriter`, `ZipArchive` `Create`/`Update`; the one-line fix is named by the exception |
| overrides `Write()` **and** `CanWrite` | works | unchanged | works |
| overrides `Read()`, not `CanRead` | works (default `true`) | unchanged | works — `CanRead`'s default is permissive, so Layer 2 adds no read-direction rejection |
| positively declares `CanRead == false` | rejected by `StreamReader`/`BinaryReader` since #1808 | unchanged | unchanged |
| seekable but does not override `CanSeek` | `Seek`/`Position` work; `BinaryReader::PeekChar` throws | unchanged | additionally rejected by `ZipArchive` `Update` |
| zlib wrapper after `Close()` | still claims its mode's capability | **returns `false`** | also `false` |

The diagnostic for every Layer 2 rejection is an `ArgumentException` at the construction
site whose message names the missing capability, so the fix is discoverable from the failure
alone. That is the property that makes the migration acceptable at all.

---

## 8. Source, vtable, ABI, layout and performance

| Layer | Public signature | Virtual / vtable | Object layout | Mangled symbols | Consumer rebuild | Observable behaviour |
|---|---|---|---|---|---|---|
| 1(a) doc | none | none | none | none | no | none |
| 1(b) zlib closed-state | none | none | **none** — reuses `state_->initialized` | none | no | `CanRead`/`CanWrite` become `false` after `Close()` |
| 2 `StreamWriter` | none | none | none | none | no | new `ArgumentException` at construction |
| 2 `ZipArchive` | none | none | none | none | no | new `ArgumentException` at construction |
| 2 zlib delegation | none | none | none | none | no | capability now depends on the inner stream |
| 3 pure virtual | none | slots unchanged, **instantiability changes** | none | `__cxa_pure_virtual` entries | **yes** — every subclass must compile against it | none directly |

**No layer changes a vtable slot, a slot order, an object layout, or a return convention.**
Layer 2's cost is entirely *semantic*. Layer 3's cost is entirely *source*. Neither is an
ABI break, and that distinction is worth keeping straight: the previous tickets' phrase
"vtable-breaking" does not apply to any of Layers 1–2.

**Performance:** one virtual call per wrapper construction (Layer 2), and one extra virtual
call per zlib `CanRead`/`CanWrite` query (delegation). Neither is on a per-byte path.
Not measurable.

**Rollback:** each layer is a deletion of the added lines plus its tests. No persisted
state, no data migration, no layout change.

---

## 9. Newly discovered, outside all three tickets

**`FileStream::getCanReadProperty()`/`getCanWriteProperty()` do not fold in `is_open()`.**
`FileStream.hpp:97-104` returns the raw `canWrite_`/`canRead_` access flags while
`getCanSeekProperty()` correctly returns `file_.is_open()`. A **closed** `FileStream`
therefore still claims the capability its `FileAccess` granted — the same defect #1826 fixed
in `MemoryStream::getCanReadProperty()`, in a different type, and the exact case
`MemoryStream.hpp:107`'s own comment warns about. It became **inactive ticket #1842**. It is
**not** folded into #1824/#1827/#1828 (different type, different member, separable) and
**no `SR-AUD-*` identifier was issued** — numbering stays frozen at **364**.

This matters to Layer 2 specifically: with #1825's per-operation checks a closed
`FileStream` already refuses to read or write, so a `StreamWriter` constructed over a closed
`FileStream` would **pass** the new `CanWrite` guard and then fail at first write. Layer 2
is still correct, but it is not sufficient on its own, and #1842 is what makes the guard
answer truthfully.

---

## 10. Test matrix

| Dimension | Requirement | Layer |
|---|---|---|
| the undeclared-writable stream | accepted before, rejected after, with the exact message and **no** `(Parameter …)` suffix | 2 |
| validation order | null **before** capability; `ZipArchive` mode-range (#1813) **before** capability; pinned by passing a null stream with an invalid mode and with an incapable stream | 2 |
| every `ZipArchiveMode` | `Create` needs write; `Read` needs read and **tolerates** unseekable; `Update` needs all three | 2 |
| cross-type consistency | `StreamWriter` and `BinaryWriter` answer the **identical** input identically; likewise `StreamReader` and `BinaryReader` | 2 |
| each zlib wrapper × each mode × before/after `Close()` | `CanRead`/`CanWrite` correct in all twelve combinations | 1(b) |
| zlib over an incapable inner stream | delegation observable | 2 |
| `leaveOpen` both ways | closed-state answer identical whether or not the inner stream was closed | 1(b) |
| the valid path | at least one ordinary success per changed constructor | 1(b), 2 |
| the migrated test double | `ThrowingWriteStream` gains the override **and** its two original assertions still hold | 2 |
| the defaults themselves | `StreamTests.DefaultCanSeekIsFalse` and `UndeclaredReadableTestStream` keep passing — they are the compatibility witnesses | all |

No existing assertion may be weakened. The floor at the time of writing is **14,139 tests
across 37 executables**.

---

## 11. Implementation ticket split

| # | Ticket | Layer | Approval | Depends on |
|---|---|---|---|---|
| 1 | **#1840** — document the three capability defaults in `Stream.hpp` | 1(a) | **none** | — |
| 2 | **#1841** — zlib wrappers return `false` when closed (#1828's first half, split out) | 1(b) | **none** | — |
| 3 | **#1824** — `StreamWriter` `CanWrite` guard | 2 | **the §6.2 approval** | #1840 |
| 4 | **#1827** — `ZipArchive` capability validation, incl. the `Read`-mode unseekable decision | 2 | **the §6.2 approval** | #1840 |
| 5 | **#1828** — zlib inner-stream delegation (second half) | 2 | **the §6.2 approval** | #1841 |
| — | **#1842** — `FileStream` capabilities fold in `is_open()` | independent | **none expected**; that ticket decides | — |
| — | Layer 3, pure virtual | 3 | a strictly larger approval | not proposed |

**One approval unblocks items 3, 4 and 5 together.** That is the whole point of writing this
document instead of asking three times.

---

## 12. Status

| Ticket | Status at the close of #1839 |
|---|---|
| #1839 | done (this document) |
| #1840 | **inactive/ready** — Layer 1(a), doc-only, no approval |
| #1841 | **inactive/ready** — Layer 1(b), measured compatible, no approval |
| #1824 | **still blocked** — on the §6.2 approval; its own §13.2 record stands |
| #1827 | **still blocked** — on the §6.2 approval, plus its two in-ticket decisions |
| #1828 | **still blocked** — for its delegation half only; its closed-state half is now #1841 |
| #1842 | **inactive** — newly discovered, separable |

No `SR-AUD-*` identifier was issued. Audit numbering stays frozen at **364** and no finding
changed status under this ticket.

---

## 13. Implementation status (updated 2026-07-30, after the §6.2 approval)

The table in §12 is the frozen snapshot at the close of #1839. The whole plan is now
**implemented**; the user granted the single §6.2 approval and every ticket is done:

| Ticket | Layer | Final status |
|---|---|---|
| #1840 | 1(a) doc | **done** — `Stream.hpp` documents the three defaults |
| #1841 | 1(b) zlib closed-state | **done** — `state_->initialized` guard, no new member |
| #1842 | independent | **done** — `FileStream` `CanRead`/`CanWrite` fold in `is_open()` (prerequisite) |
| #1824 | 2 `StreamWriter` | **done** — `CanWrite` guard, "Stream was not writable." |
| #1828 | 2 zlib delegation | **done** — `CanRead`/`CanWrite` conjoin the inner stream, `inner_ == nullptr` guard |
| #1827 | 2 `ZipArchive` | **done** — per-mode capability guard; Update requires seek; Read-mode unseekable buffered |

The §6.2 approval was used exactly as scoped: the `CanWrite` rejection direction across
`StreamWriter`, `ZipArchive` `Create`/`Update`, and the zlib delegated `CanWrite`. The two
decisions §6.2 left to #1827 were taken in #1827: `Update` requires `CanSeek` (adopted, matching
.NET, measured compatible), and a `Read`-mode unseekable stream is **buffered, not rejected** (the
port already buffers the whole input at construction). The one measured in-repository migration --
`ThrowingWriteStream` gaining `getCanWriteProperty() -> true` -- was applied, declaring the double's
real behaviour rather than bypassing the guard.

**Layer 3 (pure virtual) remains deliberately not implemented** (§6.3): no fourth capability-guard
ticket has appeared, so the per-ticket approvals still cost less than the one-time interface break,
and `StreamTests.DefaultCanSeekIsFalse`'s subject is preserved. No `SR-AUD-*` identifier issued for
any of these; audit numbering stays frozen at **364**.
