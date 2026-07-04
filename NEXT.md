# NEXT.md — sharp-runtime handoff document
*Last updated: 2026-07-04 (branch: `feature/work`, HEAD `f1786d0`) — 9073 tests passing*

---

## 1. Project summary

**sharp-runtime** is a C++23 static library that reimplements a practical subset of the .NET `System.*` namespace so that ported C#/XNA game code compiles against C++ headers with minimal changes.

- **Main goal:** Provide C++ counterparts of `System.*` types so that **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game) can compile without a .NET runtime.
- **Phase:** Active porting, driven entirely by a `plan.sqlite3` namespace-review workflow (full rules in `prompt.md` — read that, not this file, for the process itself). The workflow is **fully autonomous**: no per-item user confirmation, classify and proceed. Currently working alphabetically through `System.Collections.ObjectModel` → `System.Collections.Specialized`.
- **Header count:** 600 `.hpp` files under `include/System/` (+ `SharpRuntime/`).
- **Test file count:** 301 GoogleTest `.cpp` files under `tests/`.
- **Key architectural decisions:** no runtime reflection, no GC, no IL. Properties map to `getXxxProperty()` / `setXxxProperty()`. Types alias to `SharpRuntime::intcs` (`int32_t`), `bytecs` (`uint8_t`), etc. Inner exceptions use `std::exception_ptr`, never `const std::exception&`.

---

## 2. Current status

### Build
**Clean.** `cmake --build build --parallel 4` — zero errors, zero warnings. Verified as of this HEAD, both on `feature/work` and after merging into `develop`.

### Tests
**9073 tests passing** across 897 test suites. Zero failures. Verified on the `develop` merge commit (`7056dcb`) before pushing.

### Branch / remote state
- `feature/work` — local working branch, HEAD `f1786d0`. This is where active porting happens.
- `develop` — just fast-forwarded to `origin/develop`, then merged with `feature/work` (clean merge, no conflicts) as commit `7056dcb`, and **pushed to `origin/develop`**.
- `master` — untouched, far behind (last real update was "Task 43" era, ~2691 tests). Do not touch without explicit instruction.
- `plan.sqlite3` is gitignored — it is *not* part of what gets pushed; it's local workflow state only.

### What works
See the type list accumulated over many sessions — too long to usefully repeat verbatim here without risking drift from actual code. High-confidence, recently-verified highlights:
- **Collections.ObjectModel: 6 of 7 items reviewed/fixed this session** — `Collection<T>`, `ReadOnlyCollection<T>`, `KeyedCollection<TKey,TItem>`, `ObservableCollection<T>`, `ReadOnlyDictionary<K,V>`, `ReadOnlyObservableCollection<T>`. Only `ReadOnlySet` remains unreviewed in this namespace (see §8, Task 1).
- Full checklist ports already done and tested: `System.Collections` (non-generic), `.Concurrent`, `.Frozen`, `.Generic` (37 types), `.Immutable` (13 types) — see git log for exact commits, all fixed real bugs (wrong exception types, missing bounds validation, silent UB), not rubber-stamped.
- Core value types (`Int16/32/64/128`, `UInt*`, `Half`, `Single`, `Double`, `Decimal`, `Guid`, `BitConverter`, `Math`/`MathF`, `Random`, `HashCode`), full exception hierarchy with `HResult`/`Source`/`HelpLink`, `DateTime`/`TimeSpan`/`TimeZoneInfo` family, `Span`/`Memory` family, `Buffers`/`Buffers.Text`, IO/IO.Compression/IO.Hashing, `Text`/`Text.Json`, `Threading`/`Threading.Tasks`, `Numerics`, `Globalization`, `Net`/`Net.Http` (no TLS), `Xml` (via tinyxml2).

### What does NOT work
- `Regex` — `std::regex` backend; no named groups, no lookbehind.
- `HttpClient` — no TLS/HTTPS; plain HTTP only.
- `Net::Sockets` / `IO::RandomAccess` / `TimeZoneInfo` — POSIX-only (documented bugs per `CLAUDE.md`, not silent gaps).
- `AppDomain`/`AppContext` — Linux-only (`/proc/self/exe`).
- `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info.
- `ArrayList.GetEnumerator()` — returns `nullptr`; non-generic enumerator over `std::any` not implemented.
- `CopyTo(Array, int)` on `ICollection`/`BitArray`/`ArrayList` — `System.Array` doesn't exist in this codebase.
- Windows / Emscripten cross-compilation — untested; POSIX guards exist but are not CI-validated.

---

## 3. Recent changes

Most recent first. Full history: `git log`.

| Commit | Change |
|--------|--------|
| `7056dcb` (on `develop`, pushed) | Merged `feature/work` into `develop` (clean, no conflicts) after fast-forwarding `develop` to `origin/develop`. Rebuilt + reran full suite (9073 passing) before pushing. |
| `f1786d0` | Port `ReadOnlyObservableCollection<T>`: **the wrapper took its source `ObservableCollection` by const-ref (copy) or rvalue-ref (move), producing a disconnected private copy** — mutating the original afterwards was invisible through the wrapper and fired no events, defeating the purpose of an *observable* read-only view. Rewrote around shared `std::shared_ptr<ObservableCollection<T>>` (same pattern as `ReadOnlyDictionary`) so it's a true live view. Constructor now throws `ArgumentNullException` on null source. **Breaking API change**: callers now pass a `shared_ptr`, not a value/rvalue — all existing test call sites updated. |
| `0cbe249` | Port `ReadOnlyDictionary<K,V>`: indexer threw `std::out_of_range` instead of `KeyNotFoundException`; **constructor didn't validate the wrapped `shared_ptr`, so a null dictionary segfaulted on first access** instead of throwing `ArgumentNullException`. Added `Empty()` and protected `Dictionary` accessor. |
| `fe068f1` | Port `ObservableCollection<T>`: **overrode public `Add`/`Remove`/`Clear` instead of the protected `InsertItem`/`RemoveItem`/`ClearItems`/`SetItem` hooks that .NET's type actually overrides** — so `Insert()`/`RemoveAt()` silently bypassed `CollectionChanged` entirely, and there was no `Replace` notification path at all. Rewrote around the four hooks. Also: vector constructor fired N spurious `Add` events via a loop instead of populating directly (no notifications, matching .NET); `Move()` had zero bounds validation (silent UB); added missing `PropertyChanged`, `CheckReentrancy`/`BlockReentrancy`. |
| `4046539` | Port `KeyedCollection<TKey,TItem>`: **`InsertItem`/`SetItem` had zero duplicate-key validation** (silently overwrote the key index instead of `ArgumentException`); key-indexer threw `std::out_of_range` instead of `KeyNotFoundException`. Added missing `SetItem` override and `ChangeItemKey`. |
| `e8d30b4` and earlier | `System.Collections.ObjectModel` `Collection`/`ReadOnlyCollection` (bounds validation, exception types), `System.Collections.Immutable` (13 types), `System.Collections.Generic` (37 types), `.Concurrent`, `.Frozen`, non-generic `Collections`, `Buffers.Text` — see `git log --oneline` for the full, individually-detailed commit trail. Each commit fixed at least one real logic bug (wrong exception type, missing bounds check, or silent UB), not just cosmetic changes. |

### Recurring pattern worth knowing
Several exception types across the whole codebase were found to set no custom `HResult` because the `HResult` property was added to the `Exception` base class *after* those types were originally ported. Assume any exception type ported in an early session has this gap until checked against `/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs`.

---

## 4. Current blocker / main problem

**No active blocker.** Build is clean, 9073 tests pass, `develop` is pushed and in sync with `origin/develop`.

The very next queued item (per `plan.sqlite3`, `System`-namespace-first ordering) is:

```
id=3626  System.Collections.ObjectModel  ReadOnlySet  (status='todo')
```

This is the last unreviewed item in `System.Collections.ObjectModel` (6 of 7 already done this session). A header (`include/System/Collections/ObjectModel/ReadOnlySet.hpp`) and some tests already exist (see `Batch19Tests.cpp`) — per the workflow, review it against the full `CLAUDE.md` checklist as if new; don't rubber-stamp just because a file exists. Given the bug pattern found in every other `ObjectModel` type this session (wrong exception types, missing bounds/null validation, or a fundamental live-vs-copy semantics bug), expect to find something real here too.

After `ReadOnlySet`, the queue moves to `System.Collections.Specialized` (29 items, see §8).

---

## 5. Known bugs and limitations

| Status | Issue |
|--------|-------|
| POSIX-only | `System::Net::Sockets` — `<sys/socket.h>`, `pread`, `pwrite`; no Windows/Emscripten path |
| POSIX-only | `System::IO::RandomAccess` — `pread`, `pwrite`, `fsync` |
| Linux-only | `System::AppDomain` / `AppContext` — reads `/proc/self/exe`; not portable to macOS |
| POSIX-only | `System::TimeZoneInfo` — `localtime_r`, `/usr/share/zoneinfo` |
| incomplete | `System::Text::RegularExpressions::Regex` — no named groups, no lookbehind |
| incomplete | `System::Net::Http::HttpClient` — plain HTTP only; no TLS |
| incomplete | `ArrayList.Sort()` (no-arg) — cannot sort `std::any` without type info; throws |
| incomplete | `ArrayList.GetEnumerator()` — returns `nullptr`; not yet iterable via `IEnumerator*` |
| incomplete | `CopyTo(Array, int)` — `System.Array` type does not exist; skipped on all collections |
| audit needed | Exception types ported before the `HResult` property existed may set no per-type `HResult` — see §3 |
| stub | `System::SynchronizationContext` — `Progress<T>` calls handlers synchronously |
| stub (by design) | `System::GC` — all methods are no-ops; correct end state, not a gap |
| stub (by design) | `System::Type` / `System::Activator` — no runtime reflection, correct end state |
| stub | `System::Enum` — `GetNames`/`GetValues`/`Parse` not implemented; `HasFlag`/`ToUnderlying`/`ToInt32`/`ToString` work via templates |
| needs verification | Emscripten build — never CI-tested; POSIX guards exist but not validated |
| ⚠️ PARTIAL | `Utf8Formatter`/`Utf8Parser` — bool + all 8 integer types support G/D/N/X; `Guid`/`DateTime`/`DateTimeOffset`/`TimeSpan`/`Decimal`/`Single`/`Double` TryFormat/TryParse not yet implemented |
| design note | No concrete container in this codebase implements its corresponding `Generic::I*` interface (e.g. `Dictionary<K,V>` does not implement `IDictionary<K,V>`) except the `Collection<T>`/`ReadOnlyCollection<T>` family, which implements `IList<T>`. This is an established, deliberate inconsistency — don't "fix" it opportunistically without a real reason tied to the type you're reviewing. |
| workflow risk | Duplicate GoogleTest suite names cause linker errors — always check for collisions; use a `...Tests2` suffix when a name is already taken |
| legacy DB noise | `plan.sqlite3` has 37 rows with `status='in_progress'` and 15055 with `status='ignored'` (note: lowercase-d, different from the workflow's own `'ignore'` value) — these predate the current workflow (`in_progress` does not exist as a valid status per `CLAUDE.md`/`prompt.md`) and should be treated as inert legacy data, not active queue items |

---

## 6. Architecture notes

### Directory layout
```
include/
  SharpRuntime/SharpRuntimeHelper.hpp   ← intcs, bytecs, shortcs, longcs, charcs, ulongcs
  SharpRuntime/Prop.hpp                 ← property macros
  System/                               ← 600 .hpp files
    Collections/                        ← ArrayList, BitArray, Hashtable, IList, IComparer, …
    Collections/Generic/                ← List, Dictionary, Queue, SortedSet, …
    Collections/Concurrent/             ← ConcurrentDictionary, BlockingCollection
    Collections/Immutable/              ← ImmutableArray, ImmutableList, …
    Collections/ObjectModel/            ← Collection, ReadOnlyCollection, KeyedCollection,
                                           ObservableCollection, ReadOnlyDictionary,
                                           ReadOnlyObservableCollection, ReadOnlySet (next up)
    Buffers/, Buffers/Binary/, Buffers/Text/
    IO/, IO/Compression/, IO/Hashing/
    Text/, Text/Json/
    Threading/, Threading/Tasks/
    Numerics/, Diagnostics/, Globalization/, Net/, Xml/
src/System/                             ← .cpp bodies (auto-discovered by CMake GLOB_RECURSE)
tests/                                  ← 301 GoogleTest .cpp files
vendor/                                 ← googletest, nlohmann/json, tinyxml2, miniz
plan.sqlite3                            ← porting-status tracker (gitignored, local-only)
prompt.md                               ← canonical plan.sqlite3 workflow instructions
```

### Invariants that must not be broken
1. **Zero errors, zero warnings** before every commit.
2. **9073+ tests passing** — never go below this watermark.
3. **Property naming:** `getXxxProperty()` / `setXxxProperty()` — used by CNA (449+ files).
4. **Namespace syntax:** `namespace System::Collections::Generic {` (C++17 nested form).
5. **`SharpRuntime::intcs`** (`int32_t`) in public APIs mirroring .NET `int` parameters.
6. **SPDX header on every file.**
7. **Doxygen `/** */`** on all public declarations — never `///`.
8. **No POSIX includes in public `.hpp`** — platform code belongs in `.cpp`, guarded by `#ifdef`.
9. **Inner exceptions use `std::exception_ptr`**, never `const std::exception&`.
10. **No broad header refactor** — property naming touches 449+ files in CNA.
11. **Push only to `develop`** — never push to `master` or create tags without explicit per-action user approval.
12. **GPG signing times out** — always commit with `git -c commit.gpgsign=false commit`.
13. **plan.sqlite3 processing is fully autonomous** — classify and proceed without asking per item, per `prompt.md`.

### Established local conventions worth following (found/confirmed this session)
- **Exception message text**, when a container's key lookup fails, is the plain, non-formatted `KeyNotFoundException()` default message (`"The given key was not present in the dictionary."`) — used consistently by `Dictionary`, `SortedList`, `SortedDictionary`, `KeyedCollection`, `ReadOnlyDictionary`, even though .NET's own message text varies slightly by type. Match this existing convention rather than inventing a new message per type.
- **Duplicate-key `ArgumentException`** message is the plain `"An item with the same key has already been added."` (no key value interpolated) — same rationale.
- **Shared live-view wrappers** (e.g. `ReadOnlyDictionary<K,V>`, `ReadOnlyObservableCollection<T>`) use `std::shared_ptr<Underlying>` to share ownership with the original mutable object, matching .NET's reference semantics. Plain value-copying wrappers (e.g. `ReadOnlyCollection<T>` over a `std::vector<T>`) are a deliberate, different, already-established choice — copy-based, not live — presumably because raw references to a `std::vector` are more dangerous to dangle than a `shared_ptr`. Don't conflate the two patterns.

### Type alias summary
| Alias | Underlying | .NET equivalent |
|-------|-----------|-----------------|
| `SharpRuntime::intcs` | `int32_t` | `int` |
| `SharpRuntime::shortcs` | `int16_t` | `short` |
| `SharpRuntime::longcs` | `int64_t` | `long` |
| `SharpRuntime::bytecs` | `uint8_t` | `byte` |
| `SharpRuntime::sbytecs` | `int8_t` | `sbyte` |
| `SharpRuntime::uintcs` | `uint32_t` | `uint` |
| `SharpRuntime::ulongcs` | `uint64_t` | `ulong` |
| `SharpRuntime::ushortcs` | `uint16_t` | `ushort` |
| `SharpRuntime::charcs` | `char16_t` | `char` |

### plan.sqlite3 workflow (see `prompt.md` for the full canonical version)
- Table `task`, columns: `id, namespace, name, type, internal, outofscope, status`.
- Valid status values: `''` (unset), `todo`, `ported`, `ignore`, `tobedecided`. **`in_progress` is not a valid value** — legacy rows with it are inert (see §5).
- Current counts (this session): `ported`=314, `todo`=723, `''`=62, `ignore`=8, plus legacy `ignored`=15055 and `in_progress`=37 (not part of the active workflow).
- Query next unset/todo item, `System`-namespace-first:
  ```sql
  SELECT id,namespace,name,type FROM task
  WHERE (status='' OR status='todo')
  ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name
  LIMIT 1;
  ```

### Inner exception ctor pattern (correct)
```cpp
// Header:
FooException(const std::string& message, std::exception_ptr inner);
// Body:
FooException::FooException(const std::string& message, std::exception_ptr inner)
    : BaseException(message, std::move(inner)) {}
```

### HResult pattern for exception types
```cpp
FooException::FooException()
    : SystemException(DefaultMsg) {
    setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131XXX)); // COR_E_FOO — from HResults.cs
}
```
Look up the exact constant in `/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs`.

---

## 7. Useful commands

```bash
# Configure (first time only)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel 4

# Build — errors/warnings only
cmake --build build --parallel 4 2>&1 | grep -E "error:|warning:" | grep -v "^#"

# Run all tests
./build/SharpRuntimeTests

# Run a specific suite
./build/SharpRuntimeTests --gtest_filter="ReadOnlySet*"

# Check next unset/todo type (System namespace prioritized)
sqlite3 plan.sqlite3 "SELECT id,namespace,name,type FROM task WHERE (status='' OR status='todo') ORDER BY (CASE WHEN namespace LIKE 'System%' THEN 0 ELSE 1 END), namespace, name LIMIT 8;"

# Mark an item ported after review+tests pass
sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=<id>;"

# Check .NET reference source for a type
find /rv/tmp/runtime/src/libraries -iname "<TypeName>.cs" | grep -v tests

# Commit (GPG disabled — required in this environment)
git -c commit.gpgsign=false commit -m "message"

# Merge feature/work into develop and push (only when explicitly asked)
git checkout develop && git merge --ff-only origin/develop && git merge feature/work
cmake --build build --parallel 4 && ./build/SharpRuntimeTests   # must be clean before pushing
git push origin develop
git checkout feature/work   # return to the working branch afterward
```

---

## 8. Next smallest tasks

Ordered by `plan.sqlite3` processing order (`System`-namespace-first). Each is sized for one focused session.

### Task 1 — System.Collections.ObjectModel.ReadOnlySet
- **Goal:** Full checklist review of `ReadOnlySet<T>` against .NET's `System.Collections.ObjectModel.ReadOnlySet<T>` (wraps `ISet<T>`; provides `IsSubsetOf`/`IsSupersetOf`/`IsProperSubsetOf`/`IsProperSupersetOf`/`Overlaps`/`SetEquals`). A header and some tests already exist (`include/System/Collections/ObjectModel/ReadOnlySet.hpp`, `tests/System/Collections/Batch19Tests.cpp`) — review for the same bug classes found in every other `ObjectModel` type this session: wrong exception type on missing-element access, unvalidated null constructor argument, and whether it's a live view or an accidental disconnected copy (check whether it wraps a `shared_ptr<unordered_set<T>>` like `ReadOnlyDictionary` now does, or copies).
- **Files:** `include/System/Collections/ObjectModel/ReadOnlySet.hpp`, `tests/System/Collections/Batch19Tests.cpp`
- **Verify:** `cmake --build build --parallel 4 && ./build/SharpRuntimeTests --gtest_filter="*ReadOnlySet*"`
- Then mark `id=3626` ported in `plan.sqlite3` and commit.

### Task 2 — System.Collections.Specialized.BitVector32
- **Goal:** `plan.sqlite3` still lists this as `todo` even though `include/System/Collections/Specialized/BitVector32.hpp` exists with tests in `Batch19Tests.cpp` — confirm whether it's actually complete (in which case just mark `ported`) or has a real gap against .NET's `BitVector32`/`BitVector32.Section` API.
- **Files:** `include/System/Collections/Specialized/BitVector32.hpp`
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="BitVector32*"`

### Task 3 — System.Collections.Specialized.NotifyCollectionChangedAction/EventArgs/EventHandler
- **Goal:** These three are marked `todo` in `plan.sqlite3` but are *already implemented* — inline inside `include/System/Collections/ObjectModel/ObservableCollection.hpp` rather than as their own files under `Collections/Specialized/`. Decide: mark `ported` as-is (documenting the location deviation), or extract into their own header(s) under `Collections/Specialized/` for namespace fidelity (note `ReadOnlyObservableCollection.hpp` also depends on these types — don't break it if extracting).
- **Files:** `include/System/Collections/ObjectModel/ObservableCollection.hpp` (current location), `include/System/Collections/Specialized/` (if extracting)
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="*ObservableCollection*"`

### Task 4 — System.Collections.Specialized.OrderedDictionary / Section / remaining `todo` items
- **Goal:** After BitVector32/NotifyCollectionChanged*, continue down the `System.Collections.Specialized` list: `HybridDictionary`, `ListDictionary`, `NameValueCollection`, `OrderedDictionary`, `Section`, `StringCollection`, `StringDictionary` (7 more `todo` items, ids 3634–3656). Note: `System.Collections.Generic.OrderedDictionary<TKey,TValue>` was already ported this session (different type, different namespace) — don't conflate with `System.Collections.Specialized.OrderedDictionary` (the older, non-generic, string-keyed one).
- **Files:** check `include/System/Collections/Specialized/` for existing headers first
- **Verify:** `./build/SharpRuntimeTests --gtest_filter="*Specialized*"` (or per-type filters)

---

## 9. Do not do yet

- **No broad header refactor** — property naming (`getXxxProperty`) and namespace style touch 449+ files in CNA.
- **No LINQ port** — use `std::ranges` in all new ported code.
- **No Windows / Emscripten CI** — POSIX-only subsystems are documented bugs, not open work items.
- **No merge to `master`** — always push to `develop` only; `master` requires explicit per-action approval.
- **No new vendored libraries** without discussing scope impact.
- **No speculative API additions** — only add methods present in .NET's published API surface.
- **No work on `System::Type` / `System::Activator`** — stubs are the correct end state.
- **No `SynchronizationContext` full implementation** — synchronous stub is correct for game use.
- **No duplicate GoogleTest suite names** — check for collisions; use a `...Tests2` suffix.
- **No reintroduction of `///` Doxygen** — all headers use `/** */`.
- **No `ArrayList.Sort()` without comparer** — `std::any` cannot be compared without type info.
- **No mass rewrite or reformatting** in a single commit — incremental changes only.
- **No retrofitting `Generic::I*` interface inheritance onto existing containers** "for consistency" — the codebase's current inconsistency (only the `Collection<T>` family implements its interface) is an established, deliberate choice; changing it is a scope decision for a human, not an autonomous cleanup.
- **No per-item user confirmation in the plan.sqlite3 workflow** — fully autonomous; classify and proceed per `prompt.md`.

---

## 10. Resume prompt

```
Read prompt.md first — it is the canonical, up-to-date plan.sqlite3 workflow (fully autonomous,
no per-item confirmation). NEXT.md is a snapshot for context, not the source of truth for process.

Then inspect only the files needed for Task 1 in NEXT.md §8 (currently: ReadOnlySet in
System.Collections.ObjectModel, plan.sqlite3 id=3626). Do not refactor unrelated code.

For that task:
  1. Look up the .NET reference in /rv/tmp/runtime/src/libraries/ and read the existing C++ header/tests.
  2. Review against the full checklist in CLAUDE.md (API surface, doc-comments, SPDX, logic parity
     incl. HResult/exception-type correctness, bounds/null validation, live-vs-copy semantics) —
     fix any real gap found, don't rubber-stamp just because a file already exists.
  3. Make one small, verified improvement — don't scope-creep into adjacent types.
  4. Run: cmake --build build --parallel 4   (zero errors, zero warnings)
  5. Run: ./build/SharpRuntimeTests           (9073+ tests must still pass)
  6. Mark it ported: sqlite3 plan.sqlite3 "UPDATE task SET status='ported', updated_at=datetime('now') WHERE id=3626;"
  7. Commit only the files for that change: git -c commit.gpgsign=false commit -m "..."
  8. Update NEXT.md with what changed before ending the session.
  9. Never push without the user explicitly asking in that turn, and only ever to develop.
```
